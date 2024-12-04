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
    return 0;
}
