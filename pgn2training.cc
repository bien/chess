#include <iostream>
#include <fstream>
#include <string.h>
#include "fenboard.hh"
#include "pgn.hh"
#include <stdlib.h>
#include <random>

const int RANDOM = 0;

void process_game(std::map<std::string, std::string> &game_metadata,
        std::vector<std::pair<move_annot, move_annot> > &movelist, int target_index, bool target_side_white)
{
    Fenboard b;

    b.set_starting_position();

    std::string last_clock;
    std::string last_eval;
    std::string last_move;
    std::string next_move;
    move_t next_move_parsed = 0;
    int moveno = 1;

    for (auto iter = movelist.begin(); iter != movelist.end(); iter++) {
        move_t move = b.read_move(iter->first.move, White);
        b.apply_move(move);
        if (moveno == target_index) {
            // have black's move: apply black's move
            if (iter->second.move.length() > 1 && !target_side_white) {
                last_clock = iter->second.clock;
                last_eval = iter->second.eval;
                last_move = iter->second.move;
                move = b.read_move(iter->second.move, Black);
                b.apply_move(move);
                iter++;
                if (iter != movelist.end()) {
                    next_move = iter->first.move;
                    if (next_move.length() > 1) {
                        next_move_parsed = b.read_move(next_move, White);
                    }
                }
            } else {
                // white made last move: don't apply black's move
                last_clock = iter->first.clock;
                last_eval = iter->first.eval;
                last_move = iter->first.move;
                next_move = iter->second.move;
                if (next_move.length() > 1) {
                    next_move_parsed = b.read_move(next_move, Black);
                }
            }
            break;
        } else {
            move = b.read_move(iter->second.move, Black);
            b.apply_move(move);
        }
        moveno++;
    }
    int result;
    if (game_metadata["Result"] == "1-0") {
        result = 1;
    } else if (game_metadata["Result"] == "0-1") {
        result = -1;
    } else if (game_metadata["Result"] == "1/2-1/2") {
        result = 0;
    } else if (game_metadata["Result"] == "*") {
        return;
    } else {
        std::cerr << "Can't read Result field from pgn header " << game_metadata["Result"] << std::endl;
        abort();
    }
    std::string url = game_metadata["Site"];
    if (url.length() <= 1) {
        url = game_metadata["LichessURL"];
    }
    print_move_uci(next_move_parsed, std::cout) << "\t";
    std::cout << moveno << "\t" << url << "\t" << game_metadata["TimeControl"] << "\t" << game_metadata["WhiteElo"] << "\t" << game_metadata["BlackElo"] << "\t"
        << result << "\t" << last_clock << "\t" << last_eval << "\t" << last_move << "\t" << next_move << "\t" << game_metadata["Termination"] << "\t";
    b.get_fen(std::cout);
    std::cout << std::endl;

}

int main(int argc, char **argv)
{
    int move_index;
    std::istream *input_stream = &std::cin;
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    int repeat_times = 20;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " move-index [file.pgn]" << std::endl;
        return 1;
    }

    if (strcmp(argv[1], "random") == 0) {
        move_index = RANDOM;
    } else {
        move_index = atoi(argv[1]);
    }

    if (argc > 2) {
        input_stream = new std::ifstream(argv[2]);
        if (input_stream->fail()) {
            std::cout << "failed to open " << argv[2] << '\n';
            return -1;
        }
    }

    while (!input_stream->eof()) {
        std::map<std::string, std::string> game_metadata;
        std::vector<std::pair<move_annot, move_annot> > movelist;
        unsigned int target_index = move_index;
        bool target_side_white = false;

        movelist.clear();
        game_metadata.clear();
        read_pgn(*input_stream, game_metadata, movelist);
        if (movelist.empty() || movelist.size() < 3) {
            continue;
        }
        for (int i = 0; i < repeat_times; i++){
            if (move_index < 0) {
                target_index = movelist.size() + move_index;
            } else if (move_index == RANDOM) {
                std::uniform_int_distribution<> distrib(1, movelist.size() - 2);
                std::uniform_int_distribution<> side_to_play(0, 1);

                target_index = distrib(gen);
                target_side_white = side_to_play(gen) == 1;
            }
            if (target_index <= 0 || target_index > movelist.size()) {
                // move doesn't exist; skip
                continue;
            }
            process_game(game_metadata, movelist, target_index, target_side_white);
        }
    }

    return 0;
}
