#include <iostream>
#include <fstream>
#include <time.h>
#include "search.hh"
#include "evaluate.hh"
#include "pgn.hh"
#include "bitboard.hh"

bool expect_move(Search &search, Fenboard &b, int depth, const std::vector<std::string> &expected_move, int &nodecount)
{
    search.max_depth = depth;
    move_t move = search.alphabeta(b, White);
    nodecount = search.nodecount;
    for (std::vector<std::string>::const_iterator iter = expected_move.begin(); iter != expected_move.end(); iter++) {
        move_t expected_move_parsed = b.read_move(*iter, White);
        if (expected_move_parsed == move) {
            return true;
        }
    }

    if (expected_move.size() == 0) {
        std::cerr << "Error: couldn't find expected move" << std::endl;
        return false;
    }
    b.get_fen(std::cout);
    std::cout << std::endl;
    std::cout << "at depth=" << depth << " expected " << expected_move.front() << " but got ";
    b.print_move(move, std::cout);
    std::cout << std::endl;
    return false;
}

void get_first_move_choices(const movelist_tree &move_choices, std::vector<std::string> &first_moves)
{
    int target_moveno = move_choices.front().front().moveno;
    for (auto iter = move_choices.begin(); iter != move_choices.end(); iter++) {
        if (iter->front().is_white && iter->front().moveno == target_moveno) {
            first_moves.push_back(iter->front().move);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " file.png" << std::endl;
        return 1;
    }
    std::ifstream puzzles(argv[1]);
    std::map<std::string, std::string> game_metadata;
    movelist_tree move_choices;
    std::vector<std::string> first_move;
    Fenboard b;
    int passed = 0;
    int attempts = 0;
    int nodes = 0;
    double elapsed = 0;

    if (!puzzles) {
      std::cerr << "Couldn't read " << argv[1] << std::endl;
      exit(1);
    }

    SimpleBitboardEvaluation simple;
    Search search(&simple);
    search.use_mtdf = true;
    search.use_quiescent_search = true;
    search.use_pruning = true;

    while (!puzzles.eof()) {
        move_choices.clear();
        game_metadata.clear();
        first_move.clear();
        search.reset();
        read_pgn_options(puzzles, game_metadata, move_choices);
        if (game_metadata["FEN"] != "" && game_metadata["Black"] == "White to move") {
            b.set_fen(game_metadata["FEN"]);
            bool result = false;
            int puzzle_nodecount = 0;
            clock_t start = clock();
            int depth = 8;
            if (game_metadata["White"] == "Mate in one") {
                depth = 2;
            } else if (game_metadata["White"] == "Mate in two") {
                depth = 4;
            } else if (game_metadata["White"] == "Mate in three") {
                depth = 6;
            } else if (game_metadata["White"] == "Simple endgames") {
                depth = 10;
            }
            get_first_move_choices(move_choices, first_move);
            result = expect_move(search, b, depth, first_move, puzzle_nodecount);
            elapsed += (clock() - start) *1.0 / CLOCKS_PER_SEC;
            attempts++;
            nodes += puzzle_nodecount;
            if (result) {
                passed++;
            } else {
                std::cout << " in " << game_metadata["Event"] << std::endl;
            }
            if (attempts % 200 == 0) {
                std::cout << "Puzzles solved: " << passed << "/" << attempts << " using " << nodes << " nodes at " << nodes/elapsed << " nodes/sec or " << attempts/elapsed << " puzzles/sec" << std::endl;

            }
        }
    }

    std::cout << "Puzzles solved: " << passed << "/" << attempts << " using " << nodes << " nodes at " << nodes/elapsed << " nodes/sec or " << attempts/elapsed << " puzzles/sec" << std::endl;
}
