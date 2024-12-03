#include <stdio.h>
#include "pgn.hh"
#include "bitboard.hh"
#include "nnue.hh"

int main(int argc, char **argv)
{
    Fenboard b;
    NNUEEvaluation nnue;

    if (argc > 1) {
        b.set_fen(argv[1]);
    } else {
        b.set_starting_position();
    }

    std::cout << nnue.evaluate(b) << std::endl;
    return 0;
}
