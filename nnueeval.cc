#include "evaluate.hh"
#include "fenboard.hh"
#include "nnueeval.hh"
#include "move.hh"
#include "matrix.hh"
#include <cstdint>
#include <stdio.h>
#include <string.h>


static int mirror_square(int sq) {
    return (sq % 8) + 8 * (7 - (sq / 8));
}

template<int m, int n, typename atype>
void dump_matrix(const mmatrix<m, n, atype> &x)
{
    for (int i = 0; i < m; i++) {
        if (m > 2) {
            std::cout << "row " << i << std::endl;
        }
        for (int k = 0; k < n; k++) {
            if (n > 16 && k % 16 == 0) {
                std::cout << std::endl << k << ": ";
            }
            std::cout << static_cast<int>(x.mdata[i][k]) << ", ";
        }
        std::cout << std::endl;
    }
}

template<int n, typename atype>
void dump_matrix(const mvector<n, atype> &x)
{
    for (int k = 0; k < n; k++) {
        if (n > 16 && k % 16 == 0) {
            std::cout << std::endl << k << ": ";
        }
        std::cout << static_cast<int>(x.mdata[k]) << ", ";
    }
    std::cout << std::endl;
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

NNUEEvaluation::NNUEEvaluation()
    : model(load_nnue_model()),
      model_dense_bias_promoted(model->model_dense_bias),
      model_dense_weights_promoted(model->model_dense_weights),
      model_cp_weights_promoted(model->model_cp_weights),
      m_dense_1_weights_forward(model->model_dense_1_weights),
      m_dense_1_weights_flipped(model->model_dense_1_weights)
{}

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
        add_remove_piece(b, moved_piece, true, src_rank * 8 + src_file, dense_1_layer, psqt_cached);
        if (captured_piece > 0) {
            add_remove_piece(b, captured_piece, true, dest_rank * 8 + dest_file, dense_1_layer, psqt_cached);
        } else if (enpassant_capture_square > 0 && enpassant_captured_piece > 0) {
            add_remove_piece(b, enpassant_captured_piece, true, enpassant_capture_square, dense_1_layer, psqt_cached);
        }
        add_remove_piece(b, moved_piece, false, dest_rank * 8 + dest_file, dense_1_layer, psqt_cached);
        score = calculate_score(dense_1_layer, psqt_cached, side_to_play);
        // undo changes
        add_remove_piece(b, moved_piece, true, dest_rank * 8 + dest_file, dense_1_layer, psqt_cached);
        if (captured_piece > 0) {
            add_remove_piece(b, captured_piece, false, dest_rank * 8 + dest_file, dense_1_layer, psqt_cached);
        } else if (enpassant_capture_square > 0 && enpassant_captured_piece > 0) {
            add_remove_piece(b, enpassant_captured_piece, false, enpassant_capture_square, dense_1_layer, psqt_cached);
        }
        add_remove_piece(b, moved_piece, false, src_rank * 8 + src_file, dense_1_layer, psqt_cached);
    } else {
        // need to redo the whole board so just start over
        mvector<MODEL_FIRST_LAYER_WIDTH, int16_t> layer;
        int psqt = 0;
        b.apply_move(move);
        recalculate_dense1_layer(b, layer, psqt);
        score = calculate_score(layer, psqt, b.get_side_to_play());
        b.undo_move(move);
    }

    return score;
}

void NNUEEvaluation::add_remove_piece(const Fenboard &b, int colored_piece_type, bool remove, int piece_pos, mvector<MODEL_FIRST_LAYER_WIDTH, int16_t> &layer, int &psqt)
{
    piece_t piece_type = colored_piece_type & PIECE_MASK;
    Color piece_color = (colored_piece_type > bb_king ? Black : White);
    // always put white on the first half of the nnue, black on second half
    for (int king_color = 0; king_color<= 1; king_color++) {
        int king_square, half_adj;

        if (king_color == Black) {
            half_adj = MODEL_FIRST_LAYER_WIDTH/2;
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
            layer.sub_vector(half_adj, model->model_input_weights[dense_index], MODEL_FIRST_LAYER_WIDTH/2);
            // for (int k = 0; k < MODEL_FIRST_LAYER_WIDTH/2; k++) {
            //     layer.mdata[k + half_adj] -= model->model_input_weights[dense_index][k];
            // }
        } else {
            layer.add_vector(half_adj, model->model_input_weights[dense_index], MODEL_FIRST_LAYER_WIDTH/2);
            // for (int k = 0; k < MODEL_FIRST_LAYER_WIDTH/2; k++) {
            //     layer.mdata[k + half_adj] += model->model_input_weights[dense_index][k];
            // }
        }
        if (remove ^ (king_color == Black)) {
            psqt -= model->model_psqt[dense_index];
        } else {
            psqt += model->model_psqt[dense_index];
        }
    }

}


int NNUEEvaluation::calculate_score(const mvector<MODEL_FIRST_LAYER_WIDTH, int16_t> &input_layer, int psqt, Color side_to_play) const
{
    mvector<MODEL_FIRST_LAYER_WIDTH, int8_t> dense_layer;
    mvector<MODEL_HIDDEN_LAYER_WIDTH, int8_t> dense_n_layer;
    mvector<MODEL_HIDDEN_LAYER_WIDTH, int8_t> scratch_layer;
    mvector<MODEL_HIDDEN_LAYER_WIDTH, int8_t> *last_layer;
    mvector<MODEL_HIDDEN_LAYER_WIDTH, int8_t> *next_layer;
#ifdef DEBUG
    std::cout << "concat layer" << std::endl;
    dump_matrix(input_layer);
#endif
    dense_layer.copy_from(input_layer, 0, UNITY);

    // std::cout << "relu concat layer" << std::endl;
    // dump_matrix(dense_layer);

    if (side_to_play == White) {
        dense_layer.matrix_multiply_add_div_relu(m_dense_1_weights_forward, model_dense_bias_promoted.data[0], dense_n_layer, UNITY, 0, UNITY);
    } else {
        dense_layer.matrix_multiply_add_div_relu(m_dense_1_weights_flipped, model_dense_bias_promoted.data[0], dense_n_layer, UNITY, 0, UNITY);
    }


    // std::cout << "dense layer" << std::endl;
    // dump_matrix(dense_n_layer);

    last_layer = &dense_n_layer;
    next_layer = &scratch_layer;

    for (int i = 0; i < MODEL_DENSE_LAYERS - 1; i++) {
        /*
        std::cout << "bias" << std::endl;
        dump_matrix(model_dense_bias_promoted.data[i+1]);
        std::cout << "weights" << std::endl;
        dump_matrix(model_dense_weights_promoted.data[i]);
        */
        last_layer->matrix_multiply_add_div_relu(model_dense_weights_promoted.data[i], model_dense_bias_promoted.data[i+1], *next_layer, UNITY, 0, UNITY);
        // std::cout << "last layer" << std::endl;
        // dump_matrix(*next_layer);
        std::swap(next_layer, last_layer);
    }
    // std::cout << "SCORE: " << last_layer->dot_product(model_cp_weights_promoted) << " + " << model->model_cp_bias <<  " PTS: " << psqt << std::endl;
    float score = last_layer->dot_product(model_cp_weights_promoted) / UNITY + model->model_cp_bias;

    // nnue calculated with respect to side-to-play but engine wants respect to white
    if (side_to_play == Black) {
        score = -score;
    }
    // meanwhile psqt is calculated with respect to white
    score = (score + psqt / 2) * 100.0 / UNITY;

    // don't output scores that are more extreme than forced mate
    if (score > VERY_GOOD) {
        score = VERY_GOOD;
    } else if (score < VERY_BAD) {
        score = -VERY_BAD;
    }

    return score;
}

void NNUEEvaluation::recalculate_dense1_layer(const Fenboard &b, mvector<MODEL_FIRST_LAYER_WIDTH, int16_t> &layer, int &psqt)
{
    int i;
    for (i = 0; i < MODEL_FIRST_LAYER_WIDTH/2; i++) {
        layer.mdata[i] = model->model_input_bias[i];
        layer.mdata[i + MODEL_FIRST_LAYER_WIDTH/2] = model->model_input_bias[i];
    }
    psqt = 0;
    //psqt = model->model_psqt_bias;

    // always put white on the first half of the nnue, black on second half
    for (int king_color = 0; king_color <= 1; king_color++) {
        int king_square, half_adj;

        if (king_color == Black) {
            half_adj = MODEL_FIRST_LAYER_WIDTH/2;
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

                    for (int k = 0; k < MODEL_FIRST_LAYER_WIDTH/2; k++) {
                        layer.mdata[k + half_adj] += model->model_input_weights[dense_index][k];
                    }
                    if (king_color >= 1) {
                        psqt -= model->model_psqt[dense_index];
                    } else {
                        psqt += model->model_psqt[dense_index];
                    }
                 }
             }
         }
    }
}


int NNUEEvaluation::evaluate(const Fenboard &b)
{
    recalculate_dense1_layer(b, dense_1_layer, psqt_cached);

    return calculate_score(dense_1_layer, psqt_cached, b.get_side_to_play());
}