#include "fenboard.hh"
#include <iostream>

int main(int argc, char **argv)
{
    Fenboard b;
    std::vector<move_t> moves;

    if (argc > 1) {
        b.set_fen(argv[1]);
    }
    for (BitboardMoveIterator iter = b.get_legal_moves(b.get_side_to_play()); b.has_more_moves(iter); ) {
        move_t move = b.get_next_move(iter);
        b.print_move(move, std::cout);
        std::cout << std::endl;
    }

    return 0;
}