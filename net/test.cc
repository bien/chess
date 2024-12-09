#include <stdio.h>
#include "pgn.hh"
#include "bitboard.hh"
#include "nnue.hh"

int main(int argc, char **argv)
{
    Fenboard b;
    NNUEEvaluation nnue;
    move_t move = 0;

    if (argc > 1) {
        b.set_fen(argv[1]);
    } else {
        b.set_starting_position();
    }
    if (argc > 2) {
        move = b.read_move(argv[2], b.get_side_to_play());
    }

    std::cout << nnue.evaluate(b) << std::endl;
    if (move > 0) {
        std::cout << nnue.delta_evaluate(b, move, 0) << std::endl;
    }

    unsigned char features[512];
    int8_t weights[512];

    for (int i = 0; i < 512; i++) {
        if (i % 2 == 0) {
            features[i] = 0;
            weights[i] = 1;
        } else {
            features[i] = 2;
            weights[i] = 2;
        }
    }
    int result = matrix<1, 1, int, 1>::dotProduct_32(features, weights);
    std::cout << "dot32 = " << result << std::endl;
    result = matrix<1, 1, int, 1>::dotProduct_512(features, weights);
    std::cout << "dot512 = " << result << std::endl;

    return 0;
}
