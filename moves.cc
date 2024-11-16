#include "fenboard.hh"
#include <iostream>

int main(int argc, char **argv)
{
    Fenboard b;
    std::vector<move_t> moves;
    move_t test_move;

    if (argc > 1) {
        b.set_fen(argv[1]);
    }
    if (argc > 3) {
        Color c = White;
        if (strcmp(argv[3], "b") == 0) {
            c = Black;
        }
        test_move = atoi(argv[2]);
        print_move_uci(test_move, std::cout);
        std::cout << ": ";
        if (b.is_legal_move(test_move, c)) {
            std::cout << "legal" << std::endl;
        } else {
            std::cout << "illegal" << std::endl;
        }
    } else {
        for (BitboardMoveIterator iter = b.get_legal_moves(b.get_side_to_play()); b.has_more_moves(iter); ) {
            move_t move = b.get_next_move(iter);
            b.print_move(move, std::cout);
            std::cout << std::endl;
        }
    }

    return 0;
}