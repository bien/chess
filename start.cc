#include <iostream>
#include <time.h>
#include "search.hh"
#include "evaluate.hh"
#include "nnueeval.hh"
#include "pgn.hh"
#include "bitboard.hh"


int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Usage: depth debug_level [fen]" << std::endl;
        return 1;
    }
    int depth = atoi(argv[1]);
    search_debug = atoi(argv[2]);

    Fenboard b;
    NNUEEvaluation simple;
    Search search(&simple);
    search.use_mtdf = false;
    search.use_pv = true;
    search.use_quiescent_search = false;
    search.use_iterative_deepening = true;
    search.use_pruning = true;
    search.max_depth = depth;
    search.use_transposition_table = true;
    search.quiescent_depth = 4;
    /*
    search.use_mtdf = false;
    search.use_quiescent_search = false;
    search.use_iterative_deepening = false;
    search.use_pruning = true;
    search.quiescent_depth = 10;
    search.time_available = 10;
*/
    if (argc > 3) {
        b.set_fen(argv[3]);
    } else {
        b.set_starting_position();
    }
    move_t move;

    move = search.alphabeta(b, b.get_side_to_play());
    b.print_move(move, std::cout);
    std::cout << " " << std::endl;
    b.apply_move(move);

    std::cout << "transposition stats: full_hits: " << search.transposition_full_hits
        << " partial hits: " << search.transposition_partial_hits
        << " insufficient_depth: " << search.transposition_insufficient_depth
        << " checks: " << search.transposition_checks
        << std::endl;
    std::cout << "transposition stats: full_hits: " << (search.transposition_full_hits * 100.0 / search.transposition_checks)
        << " partial hits: " << (search.transposition_partial_hits * 100.0 / search.transposition_checks)
        << " insufficient_depth: " << (search.transposition_insufficient_depth * 100.0 / search.transposition_checks)
        << " misses: " << ((search.transposition_checks - search.transposition_full_hits - search.transposition_partial_hits - search.transposition_insufficient_depth) * 100.0 / search.transposition_checks)
        << std::endl;

    return 0;
}
