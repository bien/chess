#include "evaluate.hh"
#include "matrix.hh"

const int UNITY = 64;
const int MODEL_DENSE_LAYERS = 2; // excluding cp output layer
const int MODEL_HIDDEN_LAYER_WIDTH = 32;
const int MODEL_FIRST_LAYER_WIDTH = 512;
#define HALFKASINGLE

#ifdef HALFKP
#define INPUT_WIDTH 40960
#else
#ifdef HALFKASINGLE
#define INPUT_WIDTH 768
#endif
#endif


struct nnue_model {
    const int8_t model_input_bias[MODEL_FIRST_LAYER_WIDTH/2];
    const int16_t model_psqt[INPUT_WIDTH];
    const int16_t model_psqt_bias;
    const int8_t model_input_weights[INPUT_WIDTH][MODEL_FIRST_LAYER_WIDTH/2];


    const int16_t model_dense_bias[MODEL_DENSE_LAYERS][MODEL_HIDDEN_LAYER_WIDTH];
    const int8_t model_dense_1_weights[MODEL_FIRST_LAYER_WIDTH][MODEL_HIDDEN_LAYER_WIDTH];
    // NB: omits first layer!
    const int8_t model_dense_weights[MODEL_DENSE_LAYERS-1][MODEL_HIDDEN_LAYER_WIDTH][MODEL_HIDDEN_LAYER_WIDTH];

    const int16_t model_cp_bias;
    const int8_t model_cp_weights[MODEL_HIDDEN_LAYER_WIDTH];
};

extern nnue_model *load_nnue_model();

template <int m, int n, typename atype>
struct WeightsFlip : public mmatrix<m, n, atype> {
    WeightsFlip(const atype dense_1_weights[n][m]) {
        static_assert(m % 2 == 0, "size must be multiple of 2");
        int half = n / 2;
        for (int i = 0; i < m; i++) {
            int jflip;
            for (int j = 0; j < n; j++) {
                if (j >= half) {
                    jflip = j - half;
                } else {
                    jflip = j + half;
                }
                this->mdata[i][j] = dense_1_weights[jflip][i];
            }
        }
    }
//    atype data[m][n];
};

template <int m, int n, typename atype>
struct MatrixTranspose : public mmatrix<m, n, atype> {
    MatrixTranspose(const atype matrix[n][m]) {
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                this->mdata[i][j] = matrix[j][i];
            }
        }
    }
//    atype data[m][n];
};

template <int m, int n, typename t>
struct bias_promotion {
    bias_promotion(const t bias[m][n]) {
        for (int i = 0; i < m; i++) {
            data[i] = mvector<n,t>(bias[i]);
        }
    }
    mvector<n, t> data[m];
};

template <int k, int m, int n, typename t>
struct weight_promotion {
    weight_promotion(const t weights[k][m][n]) {
        for (int i = 0; i < k; i++) {
            data[i] = MatrixTranspose<m, n, t>(weights[i]);
        }
    }
    mmatrix<m, n, t> data[k];
};

class NNUEEvaluation : public SimpleEvaluation {
public:
    NNUEEvaluation();
    int evaluate(const Fenboard &b);
    int delta_evaluate(Fenboard &b, move_t move, int previous_score);

private:
    void add_remove_piece(const Fenboard &b, int colored_piece_type, bool remove, int piece_pos, mvector<MODEL_FIRST_LAYER_WIDTH, int16_t> &layer, int &psqt);
    void recalculate_dense1_layer(const Fenboard &b, mvector<MODEL_FIRST_LAYER_WIDTH, int16_t> &layer, int &psqt);
    int calculate_score(const mvector<MODEL_FIRST_LAYER_WIDTH, int16_t> &input_layer, int psqt, Color side_to_play) const;
    mvector<MODEL_FIRST_LAYER_WIDTH, int16_t> dense_1_layer;
    int psqt_cached;
    nnue_model *model;

    const bias_promotion<MODEL_DENSE_LAYERS, MODEL_HIDDEN_LAYER_WIDTH, int16_t> model_dense_bias_promoted;
    const weight_promotion<MODEL_DENSE_LAYERS-1, MODEL_HIDDEN_LAYER_WIDTH, MODEL_HIDDEN_LAYER_WIDTH, int8_t> model_dense_weights_promoted;
    const mvector<MODEL_HIDDEN_LAYER_WIDTH, int8_t> model_cp_weights_promoted;

    const MatrixTranspose<MODEL_HIDDEN_LAYER_WIDTH, MODEL_FIRST_LAYER_WIDTH, int8_t> m_dense_1_weights_forward;
    const WeightsFlip<MODEL_HIDDEN_LAYER_WIDTH, MODEL_FIRST_LAYER_WIDTH, int8_t> m_dense_1_weights_flipped;
};
