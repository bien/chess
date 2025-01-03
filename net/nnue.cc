#include "evaluate.hh"
#include "fenboard.hh"
#include "nnue.hh"
#include "move.hh"
#include "matrix.hh"
#include <cstdint>
#include <stdio.h>
#include <string.h>

#define HALFKASINGLE 1

extern "C" {
extern const int8_t dense_bias[256];
#ifdef HALFKP
extern const int8_t dense_weights[40960][256];
#endif
#ifdef HALFKASINGLE
extern const int8_t dense_weights[768][256];
#endif

extern const int16_t dense_1_bias[32];
extern const int8_t dense_1_weights[512][32];
extern const int16_t dense_2_bias[32];
extern const int8_t dense_2_weights[32][32];
extern const int16_t dense_3_bias[3];
extern const int8_t dense_3_weights[32][3];
}

template <int m, int n, typename atype>
struct WeightsFlip {
    WeightsFlip(const atype dense_1_weights[m][n]) {
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
                data[i][j] = dense_1_weights[i][jflip];
            }
        }
    }
    atype data[m][n];
};

template <int m, int n, typename atype>
struct MatrixTranspose {
    MatrixTranspose(const atype matrix[n][m]) {
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                data[i][j] = matrix[j][i];
            }
        }
    }
    atype data[m][n];
};



const mvector<32, int16_t> m_dense_1_bias(dense_1_bias);
const mvector<32, int16_t> m_dense_2_bias(dense_2_bias);
const mvector<3, int16_t> m_dense_3_bias(dense_3_bias);

const MatrixTranspose<32, 512, int8_t> dense_1_weightsT(dense_1_weights);
const matrix<32, 512, int8_t, UNITY> m_dense_1_weights_forward(dense_1_weightsT.data);
const WeightsFlip<32, 512, int8_t> dense_1_weights_flip(dense_1_weightsT.data);
const matrix<32, 512, int8_t, UNITY> m_dense_1_weights_flipped(dense_1_weights_flip.data);

const MatrixTranspose<32, 32, int8_t> dense_2_weightsT(dense_2_weights);
const matrix<32, 32, int8_t, UNITY> m_dense_2_weights(dense_2_weightsT.data);
const MatrixTranspose<3, 32, int8_t> dense_3_weightsT(dense_3_weights);
const matrix<3, 32, int8_t, UNITY> m_dense_3_weights(dense_3_weightsT.data);


static int mirror_square(int sq) {
    return (sq % 8) + 8 * (7 - (sq / 8));
}

template<int m, int n, typename atype, int unity>
void dump_matrix(const matrix<m, n, atype, unity> &x)
{
    for (int i = 0; i < m; i++) {
        if (m > 2) {
            std::cout << "row " << i << std::endl;
        }
        for (int k = 0; k < n; k++) {
            if (n > 16 && k % 16 == 0) {
                std::cout << std::endl << k << ": ";
            }
            std::cout << static_cast<int>(x.data[i][k]) << ", ";
        }
        std::cout << std::endl;
    }
}

// feature set dependent variables
static piece_t get_max_piece()
{
#ifdef HALFKP
    return bb_queen;
#endif
#ifdef HALFKASINGLE
    return bb_king;
#endif
}

static int get_dense_index(int king_square, int piece_type, int piece_pos, int piece_color, int king_color)
{
    int rel_pos;
    int dense_index = 0;
    if (king_color == Black) {
        rel_pos = mirror_square(piece_pos);
    } else {
        rel_pos = piece_pos;
    }

     // nnue piece_type is 0 based instead of 1 based
#ifdef HALFKP
    dense_index = king_square * (64 * 10) + (piece_type - 1 + (piece_color == king_color ? 0 : 1) * 5) * 64 + rel_pos;
#endif
#ifdef HALFKASINGLE
    dense_index = (piece_type - 1 + (piece_color == king_color ? 0 : 1) * 6) * 64 + rel_pos;
#endif
    return dense_index;
}


int NNUEEvaluation::delta_evaluate(Fenboard &b, move_t move, int previous_score) {
    int score;
    unsigned char src_rank, src_file, dest_rank, dest_file;
    piece_t captured_piece;
    get_source(move, src_rank, src_file);
    get_dest(move, dest_rank, dest_file);
    captured_piece = b.get_piece(dest_rank, dest_file);
    piece_t moved_piece = b.get_piece(src_rank, src_file);
    int enpassant_capture_square = 0;
    piece_t enpassant_captured_piece = b.get_piece(src_rank, dest_file);
    // board side to play wasn't updated, so manually switch
    Color side_to_play = b.get_side_to_play() == White ? Black : White;

    // enpassant weirdness
    if ((moved_piece & PIECE_MASK) == bb_pawn && captured_piece == 0 && src_file != dest_file) {
        enpassant_capture_square = src_rank * 8 + dest_file;
    }

    // this works for HALFKP and HALFKA-single but maybe not other feature sets
    if ((moved_piece & PIECE_MASK) <= get_max_piece()) {
        add_remove_piece(b, moved_piece, true, src_rank * 8 + src_file, dense_1_layer);
        if (captured_piece > 0) {
            add_remove_piece(b, captured_piece, true, dest_rank * 8 + dest_file, dense_1_layer);
        } else if (enpassant_capture_square > 0 && enpassant_captured_piece > 0) {
            add_remove_piece(b, enpassant_captured_piece, true, enpassant_capture_square, dense_1_layer);
        }
        add_remove_piece(b, moved_piece, false, dest_rank * 8 + dest_file, dense_1_layer);
        score = calculate_score(dense_1_layer, side_to_play);
        // undo changes
        add_remove_piece(b, moved_piece, true, dest_rank * 8 + dest_file, dense_1_layer);
        if (captured_piece > 0) {
            add_remove_piece(b, captured_piece, false, dest_rank * 8 + dest_file, dense_1_layer);
        } else if (enpassant_capture_square > 0 && enpassant_captured_piece > 0) {
            add_remove_piece(b, enpassant_captured_piece, false, enpassant_capture_square, dense_1_layer);
        }
        add_remove_piece(b, moved_piece, false, src_rank * 8 + src_file, dense_1_layer);
    } else {
        // need to redo the whole board so just start over
        matrix<1, 512, int16_t, UNITY> layer;
        b.apply_move(move);
        recalculate_dense1_layer(b, layer);
        score = calculate_score(layer, b.get_side_to_play());
        b.undo_move(move);
    }

    return score;
}

void NNUEEvaluation::add_remove_piece(const Fenboard &b, int colored_piece_type, bool remove, int piece_pos, matrix<1, 512, int16_t, UNITY> &layer)
{
    piece_t piece_type = colored_piece_type & PIECE_MASK;
    Color piece_color = (colored_piece_type > bb_king ? Black : White);
    // always put white on the first half of the nnue, black on second half
    for (int king_color = 0; king_color<= 1; king_color++) {
        int king_square, half_adj;

        if (king_color == Black) {
            half_adj = 256;
        } else {
            half_adj = 0;
        }
        if (king_color == White) {
            king_square = get_low_bit(b.piece_bitmasks[bb_king], 0);
        } else {
            king_square = mirror_square(get_low_bit(b.piece_bitmasks[(bb_king + 1) + bb_king], 0));
        }

        int dense_index = get_dense_index(king_square, piece_type, piece_pos, piece_color, king_color);

        if (remove) {
            for (int k = 0; k < 256; k++) {
                layer.data[0][k + half_adj] -= dense_weights[dense_index][k];
            }

        } else {
            for (int k = 0; k < 256; k++) {
                layer.data[0][k + half_adj] += dense_weights[dense_index][k];
            }
        }
    }

}


int NNUEEvaluation::calculate_score(const matrix<1, 512, int16_t, UNITY> &input_layer, Color side_to_play) const
{
    matrix<1, 512, uint8_t, UNITY> dense_layer;
    matrix<1, 32, uint8_t, UNITY> dense_2_layer;
    matrix<1, 32, uint8_t, UNITY> dense_3_layer;
    matrix<1, 3, int16_t, UNITY> output_layer;
#ifdef DEBUG
    std::cout << "concat layer" << std::endl;
    dump_matrix(input_layer);
#endif
    input_layer.matrix_relu(dense_layer);
#ifdef DEBUG
    std::cout << "relu concat layer" << std::endl;
    dump_matrix(dense_layer);
#endif
    if (side_to_play == White) {
        dense_layer.matrix_multiply_add_div_relu(m_dense_1_weights_forward, m_dense_1_bias, dense_2_layer);
    } else {
        dense_layer.matrix_multiply_add_div_relu(m_dense_1_weights_flipped, m_dense_1_bias, dense_2_layer);
    }

    dense_2_layer.matrix_multiply_add_div_relu(m_dense_2_weights, m_dense_2_bias, dense_3_layer);
#ifdef DEBUG
    for (int k = 0; k < 32; k++) {
        std::cout << "output[" << k << "] = " << (int)m_dense_1_bias.data[k];
        bool first = true;
        int check = m_dense_1_bias.data[k];
        for (int j = 0; j < 512; j++) {
            if (j % 16 == 0) {
                std::cout << std::endl << j << ": ";
            }
            if (dense_layer.data[0][j] > 0) {
                std::cout << "+";
                if (first) {
                    std::cout << "(";
                    first = false;
                }
                check += ((int)(side_to_play == White ? m_dense_1_weights_forward : m_dense_1_weights_flipped).data[k][j]) * dense_layer.data[0][j];
                std::cout << (int)(side_to_play == White ? m_dense_1_weights_forward : m_dense_1_weights_flipped).data[k][j] << "*" << (int)dense_layer.data[0][j];
            }
        }
        std::cout << ")/64 # " << (int)dense_2_layer.data[0][k] << " == " << check << std::endl;
    }

    std::cout << "layer 1-2 relu" << std::endl;
    dump_matrix(dense_2_layer);
    for (int k = 0; k < 32; k++) {
        std::cout << "output[" << k << "] = " << (int)m_dense_2_bias.data[k];
        bool first = true;
        for (int j = 0; j < 32; j++) {
            if (dense_2_layer.data[0][j] > 0) {
                std::cout << "+";
                if (first) {
                    std::cout << "(";
                    first = false;
                }
                std::cout << (int)m_dense_2_weights.data[k][j] << "*" << (int)dense_2_layer.data[0][j];
            }
        }
        std::cout << ")/64 # " << (int)dense_3_layer.data[0][k] << std::endl;
    }
    std::cout << "layer 2-3 relu" << std::endl;
    dump_matrix(dense_3_layer);
    for (int k = 0; k < 3; k++) {
        std::cout << "output[" << k << "] = " << (int)m_dense_3_bias.data[k];
        for (int j = 0; j < 32; j++) {
            if (dense_3_layer.data[0][j] > 0) {
                std::cout << "+";
                std::cout << (int)m_dense_3_weights.data[k][j] << "*" << (int)dense_3_layer.data[0][j];
            }
        }
        std::cout << std::endl;
    }
#endif
    dense_3_layer.matrix_multiply_add_div(m_dense_3_weights, m_dense_3_bias, output_layer);

#ifdef DEBUG
    std::cout << "output layer" << std::endl;
    dump_matrix(output_layer);
#endif
    output_layer.matrix_softmax();
#ifdef DEBUG
    std::cout << "softmax" << std::endl;
    dump_matrix(output_layer);
#endif

    // https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md#loss-functions-and-how-to-apply-them
    const int CP_WDL_SCALING_FACTOR = 410;
    float winprob = (output_layer.data[0][2] + 0.5 * output_layer.data[0][1]) / 10000;
    int score;
    if (winprob == 0) {
        score = VERY_BAD;
    } else {
        score = -CP_WDL_SCALING_FACTOR * log(1.0 / winprob - 1);
    }
    // don't output scores that are more extreme than forced mate
    if (score > VERY_GOOD) {
        score = VERY_GOOD;
    } else if (score < VERY_BAD) {
        score = -VERY_BAD;
    }
#ifdef DEBUG
    std::cout << "wp = " << winprob << " score = " << score << std::endl;
#endif
    /*
    // nnue calculated with respect to side-to-play but engine wants respect to white
    if (side_to_play == Black) {
        score = -score;
    }
    */
    return score;
}

void NNUEEvaluation::recalculate_dense1_layer(const Fenboard &b, matrix<1, 512, int16_t, UNITY> &layer)
{
    int i;
    for (i = 0; i < 256; i++) {
        layer.data[0][i] = dense_bias[i];
        layer.data[0][i + 256] = dense_bias[i];
    }

    // always put white on the first half of the nnue, black on second half
    for (int king_color = 0; king_color <= 1; king_color++) {
        int king_square, half_adj;

        if (king_color == Black) {
            half_adj = 256;
        } else {
            half_adj = 0;
        }
        if (king_color == White) {
            king_square = get_low_bit(b.piece_bitmasks[bb_king], 0);
        } else {
            king_square = mirror_square(get_low_bit(b.piece_bitmasks[(bb_king + 1) + bb_king], 0));
        }
        for (int piece_color = 0; piece_color <= 1; piece_color++) {
            for (int piece_type = bb_pawn; piece_type <= get_max_piece(); piece_type++) {
                int start = -1;
                while ((start = get_low_bit(b.piece_bitmasks[piece_color * (bb_king + 1) + piece_type], start + 1)) >= 0) {

                     int dense_index = get_dense_index(king_square, piece_type, start, piece_color, king_color);
//                     std::cout << "place adj=" << half_adj << " piece=" << (int) piece_type << " color=" << piece_color << " kc=" << king_color << " at " << (int) start << " idx=" << dense_index << std::endl;
//                     matrix<1, 512, int16_t, UNITY>::vector_add(&layer.data[0][half_adj], &dense_weights[dense_index][0], 256);

                     for (int k = 0; k < 256; k++) {
                         layer.data[0][k + half_adj] += dense_weights[dense_index][k];
                     }
                 }
             }
         }
    }
}


int NNUEEvaluation::evaluate(const Fenboard &b)
{
    recalculate_dense1_layer(b, dense_1_layer);

    return calculate_score(dense_1_layer, b.get_side_to_play());
}