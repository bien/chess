#include "evaluate.hh"
#include "fenboard.hh"
#include "nnue.hh"
#include "matrix.hh"
#include <cstdint>
#include <string.h>
#include <iostream>

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
const mvector<32, int16_t> m_dense_1_bias(dense_1_bias);
const matrix<512, 32, int8_t> m_dense_1_weights(dense_1_weights);
const mvector<32, int16_t> m_dense_2_bias(dense_2_bias);
const matrix<32, 32, int8_t> m_dense_2_weights(dense_2_weights);
const mvector<3, int16_t> m_dense_3_bias(dense_3_bias);
const matrix<32, 3, int8_t> m_dense_3_weights(dense_3_weights);


constexpr int mirror_square(int sq) {
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

int NNUEEvaluation::delta_evaluate(Fenboard &b, move_t move, int previous_score) const {
    matrix<1, 512, int16_t> updated_input_layer;

    // first flip the input layer since side to move changed
    // memcpy(&updated_input_layer.data[0][0], &dense_1_layer.data[0][256], sizeof(dense_1_layer.data) / 2);
    // memcpy(&updated_input_layer.data[0][256], &dense_1_layer.data[0][0], sizeof(dense_1_layer.data) / 2);

    /*
    // remove source piece
    unsigned char src_rank, src_file;
    get_source(move_t move, src_rank, src_file);
    piece_t piece_type = b.get_piece(src_rank, src_file);
    if (piece_type & PIECE_MASK != bb_king) {
        // must be owned by side_to_move
    }
*/

    return calculate_score(updated_input_layer);
}

int NNUEEvaluation::calculate_score(const matrix<1, 512, int16_t> &input_layer) const
{
    matrix<1, 512, int8_t> dense_layer;
    matrix<1, 32, int8_t> dense_2_layer;
    matrix<1, 32, int8_t> dense_3_layer;
    matrix<1, 3, int16_t> output_layer;
    std::cout << "concat layer" << std::endl;
    dump_matrix(input_layer);
    matrix_relu(input_layer, dense_layer, 0, 64);
    std::cout << "relu concat layer" << std::endl;
    dump_matrix(dense_layer);
    matrix_multiply_add_div_relu(dense_layer, m_dense_1_weights, m_dense_1_bias, 64, 0, 64, dense_2_layer);
    std::cout << "layer 1-2 relu" << std::endl;
    dump_matrix(dense_2_layer);
    matrix_multiply_add_div_relu(dense_2_layer, m_dense_2_weights, m_dense_2_bias, 64, 0, 64, dense_3_layer);
    std::cout << "layer 2-3 relu" << std::endl;
    dump_matrix(dense_3_layer);

    matrix_multiply_add_div(dense_3_layer, m_dense_3_weights, m_dense_3_bias, 64, output_layer);
    std::cout << "output layer raw" << std::endl;
    dump_matrix(output_layer);
    matrix_softmax_64ths(output_layer);
    std::cout << "output layer softmax" << std::endl;
    dump_matrix(output_layer);

    // nnue calculated in 64ths of a win, but we want centipawns
    return output_layer.data[0][2] - output_layer.data[0][0];
}

int NNUEEvaluation::evaluate(const Fenboard &b)
{
    int i;
    for (i = 0; i < 256; i++) {
        dense_1_layer.data[0][i] = dense_bias[i];
        dense_1_layer.data[0][i + 256] = dense_bias[i];
    }

    std::cout << "concat layer bias" << std::endl;
    dump_matrix(dense_1_layer);

    for (int king_side = 0; king_side <= 1; king_side++) {
        int king_square;
        int half_adj = 0;
        if (b.get_side_to_play() != king_side) {
            half_adj = 256;
        }
        if (king_side == White) {
            king_square = get_low_bit(b.piece_bitmasks[bb_king], 0);
        } else {
            king_square = mirror_square(get_low_bit(b.piece_bitmasks[(bb_king + 1) + bb_king], 0));
        }
         for (int color_adj = 0; color_adj <= 1; color_adj++) {
             for (int piece_type = bb_pawn; piece_type <= bb_queen; piece_type++) {
                 int start = -1;
                 while ((start = get_low_bit(b.piece_bitmasks[color_adj * (bb_king + 1) + piece_type], start + 1)) >= 0) {
                     int rel_start = start;
                     if (king_side == Black) {
                         rel_start = mirror_square(rel_start);
                     }
                     // nnue piece_type is 0 based instead of 1 based
                     int dense_index = king_square * (64 * 10) + (piece_type - 1 + (color_adj == king_side ? 0 : 1) * 5) * 64 + rel_start;
                     for (int k = 0; k < 256; k++) {
                         dense_1_layer.data[0][k + half_adj] += dense_weights[dense_index][k];
                         std::cout << "set " << k + half_adj << " += dense[" << dense_index << "][" << k << "] (=" << static_cast<int>(dense_weights[dense_index][k]) << ")=> " << static_cast<int>(dense_1_layer.data[0][k + half_adj]) << std::endl;
                     }
                 }
             }
         }
    }

    return calculate_score(dense_1_layer);
}