#include <iostream>
#include <fstream>
#include <time.h>
#include "search.hh"
#include "evaluate.hh"
#include "pgn.hh"
#include "bitboard.hh"
#include "nnue.hh"

bool expect_move(Search &search, Fenboard &b, int depth, const std::vector<std::string> &expected_move, int &nodecount)
{
    if (depth != 0) {
        search.max_depth = depth;
    }
    Color side_to_play = b.get_side_to_play();
    move_t move = search.alphabeta(b, side_to_play);
    nodecount = search.nodecount;
    for (std::vector<std::string>::const_iterator iter = expected_move.begin(); iter != expected_move.end(); iter++) {
        move_t expected_move_parsed = b.read_move(*iter, side_to_play);
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

void get_first_move_choices(Color side_to_play, const movelist_tree &move_choices, std::vector<std::string> &first_moves)
{
    int target_moveno = move_choices.front().front().moveno;
    for (auto iter = move_choices.begin(); iter != move_choices.end(); iter++) {
        if (((side_to_play == White) == iter->front().is_white) && iter->front().moveno == target_moveno) {
            first_moves.push_back(iter->front().move);
        }
    }
}

struct Results {
    int passed;
    int attempts;
    int nodes;
    double elapsed;

    Results() : passed(0), attempts(0), nodes(0), elapsed(0)
    {}
};

void read_csv_puzzles(Fenboard &b, Search &search, std::ifstream &puzzles, Results &r)
{
    //lichess puzzles format
    //header: PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,GameUrl,OpeningTags

    int elo_scores = 0;

    while (!puzzles.eof()) {
        std::string line;
        std::vector<std::string> parts;
        std::getline(puzzles, line);

        size_t start = 0, stop;
        while (true) {
            stop = line.find(",", start);
            if (stop == std::string::npos) {
                break;
            } else {
                parts.push_back(line.substr(start, stop - start));
                start = stop + 1;
            }
        }
        if (parts.size() < 6 || parts[0] == "PuzzleId") {
            continue;
        }
        size_t first_move_space = parts[2].find(" ");
        size_t second_move_space = parts[2].find(" ", first_move_space + 1);
        if (first_move_space == std::string::npos) {
            std::cerr << "Error: don't have enough moves in " << parts[0] << std::endl;
            break;
        }
        if (second_move_space == std::string::npos) {
            second_move_space = parts[2].size();
        }
        std::string zero_move = parts[2].substr(0, first_move_space);
        std::string first_move = parts[2].substr(first_move_space + 1, second_move_space - first_move_space - 1);
        std::vector<std::string> first_move_choices;
        first_move_choices.push_back(first_move);
        int puzzle_nodecount = 0;
        b.set_fen(parts[1]);
        b.apply_move(b.read_move(zero_move, b.get_side_to_play()));
        clock_t starttime = clock();
        bool result = expect_move(search, b, 8, first_move_choices, puzzle_nodecount);
        r.elapsed += (clock() - starttime) *1.0 / CLOCKS_PER_SEC;
        r.attempts++;
        r.nodes += puzzle_nodecount;
        elo_scores += std::stoi(parts[3]);
        if (result) {
            r.passed++;
        } else {
            std::cout << " in " << parts[0] << std::endl;
        }
        if (r.attempts % 200 == 0) {
            std::cout << "Puzzles solved: " << r.passed << "/" << r.attempts << " using " << r.nodes << " nodes at " << r.nodes/r.elapsed << " nodes/sec or " << r.attempts/r.elapsed << " puzzles/sec" << std::endl;
        }
    }
    std::cout << "Elo avg " << (int)(elo_scores*1.0/r.attempts) << std::endl;
    std::cout << "Elo rating " << (int)((elo_scores - 400 * r.attempts + 800 * r.passed) / r.attempts) << std::endl;
}

void read_pgn_puzzles(Fenboard &b, Search &search, std::ifstream &puzzles, Results &r)
{
    std::map<std::string, std::string> game_metadata;
    movelist_tree move_choices;
    std::vector<std::string> first_move;
    double elapsed = 0;
    pgn_istream puzzle_pgn(puzzles);

    while (!puzzles.eof()) {
        move_choices.clear();
        game_metadata.clear();
        first_move.clear();
        search.reset();
        read_pgn_options(&puzzle_pgn, game_metadata, move_choices);
        if (game_metadata["FEN"] != "") {
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
            get_first_move_choices(b.get_side_to_play(), move_choices, first_move);
            result = expect_move(search, b, depth, first_move, puzzle_nodecount);
            elapsed += (clock() - start) *1.0 / CLOCKS_PER_SEC;
            r.attempts++;
            r.nodes += puzzle_nodecount;
            if (result) {
                r.passed++;
            } else {
                std::cout << " in " << game_metadata["Event"] << std::endl;
            }
            if (r.attempts % 200 == 0) {
                std::cout << "Puzzles solved: " << r.passed << "/" << r.attempts << " using " << r.nodes << " nodes at " << r.nodes/elapsed << " nodes/sec or " << r.attempts/elapsed << " puzzles/sec" << std::endl;

            }
        }
    }
    r.elapsed = elapsed;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " file.pgn" << std::endl;
        return 1;
    }
    std::ifstream puzzles(argv[1]);
    Fenboard b;

    if (!puzzles) {
      std::cerr << "Couldn't read " << argv[1] << std::endl;
      exit(1);
    }

    SimpleEvaluation simple;
    Search search(&simple);
    search.use_mtdf = true;
    search.use_quiescent_search = false;
    search.use_pruning = true;
    search.use_transposition_table = true;

    Results r;

    if (std::string(argv[1]).find(".pgn") != std::string::npos) {
        read_pgn_puzzles(b, search, puzzles, r);
    } else {
        read_csv_puzzles(b, search, puzzles, r);

    }

    std::cout << "Puzzles solved: " << r.passed << "/" << r.attempts << " using " << r.nodes << " nodes at " << r.nodes/r.elapsed << " nodes/sec or " << r.attempts/r.elapsed << " puzzles/sec" << std::endl;
}
