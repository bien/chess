#include "fenboard.hh"
#include "search.hh"
#include <iostream>

int main(int argc, char **argv)
{
    Fenboard b;
    std::vector<move_t> moves;
    move_t test_move;

    if (argc > 1) {
        b.set_fen(argv[1]);
    } else {
        b.set_starting_position();
    }
    if (argc > 3) {
        Color c = White;
        if (strcmp(argv[3], "b") == 0) {
            c = Black;
        }
        test_move = atoi(argv[2]);
        print_move_uci(test_move, std::cout);
    } else {
        MoveSorter ms(&b, b.get_side_to_play());
        while (ms.has_more_moves()) {
            move_t move = ms.next_move();
            b.print_move(move, std::cout);
            std::cout << " ms=" << ms.get_score(move) << std::endl;
        }
    }

    return 0;
}