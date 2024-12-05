#include "evaluate.hh"
#include "fenboard.hh"
#include "nnue.hh"
#include "move.hh"
#include "matrix.hh"
#include <cstdint>
#include <stdio.h>
#include <string.h>

extern "C" {
extern const int8_t dense_bias[256];
extern const int8_t dense_weights[40960][256];

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
        int half = m / 2;
        for (int i = 0; i < m; i++) {
            int index;
            if (i >= half) {
                index = i - half;
            } else {
                index = i + half;
            }
            for (int j = 0; j < n; j++) {
                data[i][j] = dense_1_weights[index][j];
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
const matrix<32, 512, int8_t> m_dense_1_weights_forward(dense_1_weightsT.data);
const WeightsFlip<32, 512, int8_t> dense_1_weights_flip(dense_1_weightsT.data);
const matrix<32, 512, int8_t> m_dense_1_weights_flipped(dense_1_weights_flip.data);

const MatrixTranspose<32, 32, int8_t> dense_2_weightsT(dense_2_weights);
const matrix<32, 32, int8_t> m_dense_2_weights(dense_2_weightsT.data);
const MatrixTranspose<3, 32, int8_t> dense_3_weightsT(dense_3_weights);
const matrix<3, 32, int8_t> m_dense_3_weights(dense_3_weightsT.data);


static int mirror_square(int sq) {
    return (sq % 8) + 8 * (7 - (sq / 8));
}

template<int m, int n, typename atype>
void dump_matrix(const matrix<m, n, atype> &x)
{
    for (int i = 0; i < m; i++) {
        for (int k = 0; k < n; k++) {
            std::cout << static_cast<int>(x.data[i][k]) << ", ";
        }
        std::cout << std::endl;
    }
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

    if ((moved_piece & PIECE_MASK) != bb_king) {
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
        matrix<1, 512, int8_t> layer;
        b.apply_move(move);
        recalculate_dense1_layer(b, layer);
        score = calculate_score(layer, b.get_side_to_play());
        b.undo_move(move);
    }

    return score;
}

void NNUEEvaluation::add_remove_piece(const Fenboard &b, int colored_piece_type, bool remove, int piece_pos, matrix<1, 512, int8_t> &layer)
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
        int rel_pos;
        if (king_color == Black) {
            rel_pos = mirror_square(piece_pos);
        } else {
            rel_pos = piece_pos;
        }
        // nnue piece_type is 0 based instead of 1 based
        int dense_index = king_square * (64 * 10) + (piece_type - 1 + (piece_color == king_color ? 0 : 1) * 5) * 64 + rel_pos;
        if (remove) {
            for (int k = 0; k < 256; k++) {
                dense_1_layer.data[0][k + half_adj] -= dense_weights[dense_index][k];
            }
        } else {
            for (int k = 0; k < 256; k++) {
                dense_1_layer.data[0][k + half_adj] += dense_weights[dense_index][k];
            }
        }
    }

}


int NNUEEvaluation::calculate_score(const matrix<1, 512, int8_t> &input_layer, Color side_to_play) const
{
    matrix<1, 512, int8_t> dense_layer;
    matrix<1, 32, int8_t> dense_2_layer;
    matrix<1, 32, int8_t> dense_3_layer;
    matrix<1, 3, int16_t> output_layer;
    matrix_relu(input_layer, dense_layer, 0, 64);
    if (side_to_play == White) {
        matrix_multiply_add_div_relu(dense_layer, m_dense_1_weights_forward, m_dense_1_bias, 64, 0, 64, dense_2_layer);
    } else {
        matrix_multiply_add_div_relu(dense_layer, m_dense_1_weights_flipped, m_dense_1_bias, 64, 0, 64, dense_2_layer);
    }
    matrix_multiply_add_div_relu(dense_2_layer, m_dense_2_weights, m_dense_2_bias, 64, 0, 64, dense_3_layer);

    matrix_multiply_add_div(dense_3_layer, m_dense_3_weights, m_dense_3_bias, 64, output_layer);
    matrix_softmax_64ths(output_layer);

    // nnue calculated in .0001% of a win
    int score = output_layer.data[0][2] - output_layer.data[0][0];
    // nnue calculated with respect to side-to-play but engine wants respect to white
    if (side_to_play == Black) {
        score = -score;
    }
    return score;
}

void NNUEEvaluation::recalculate_dense1_layer(const Fenboard &b, matrix<1, 512, int8_t> &layer)
{
    int i;
    for (i = 0; i < 256; i++) {
        layer.data[0][i] = dense_bias[i];
        layer.data[0][i + 256] = dense_bias[i];
    }

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
         for (int piece_color = 0; piece_color <= 1; piece_color++) {
             for (int piece_type = bb_pawn; piece_type <= bb_queen; piece_type++) {
                 int start = -1;
                 while ((start = get_low_bit(b.piece_bitmasks[piece_color * (bb_king + 1) + piece_type], start + 1)) >= 0) {
                     int rel_start = start;
                     if (king_color == Black) {
                         rel_start = mirror_square(rel_start);
                     }
                     // nnue piece_type is 0 based instead of 1 based
                     int dense_index = king_square * (64 * 10) + (piece_type - 1 + (piece_color == king_color ? 0 : 1) * 5) * 64 + rel_start;
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