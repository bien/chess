#include <iostream>
#include <fstream>
#include <string.h>
#include "fenboard.hh"
#include "pgn.hh"
#include <stdlib.h>

int main(int argc, char **argv)
{
    Fenboard b;
    int move_index;
    std::istream *input_stream = &std::cin;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " move-index [file.pgn]" << std::endl;
        return 1;
    }

    move_index = atoi(argv[1]);

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
        int target_index = move_index;
        int i = 0;

        movelist.clear();
        game_metadata.clear();
        read_pgn(*input_stream, game_metadata, movelist);
        if (move_index < 0) {
            target_index = movelist.size() + move_index;
        }
        if (movelist.empty()) {
            break;
        }
        if (target_index < 0 || target_index > movelist.size()) {
            // move doesn't exist; skip
            continue;
        }
        b.set_starting_position();

        std::string last_clock;
        std::string last_eval;
        std::string last_move;

        for (auto iter = movelist.begin(); iter != movelist.end(); iter++) {
            move_t move = b.read_move(iter->first.move, White);
            b.apply_move(move);
            if (iter->second.move.length() > 1) {
                move = b.read_move(iter->second.move, Black);
                b.apply_move(move);
            }
            if (i++ == target_index) {
                last_clock = iter->second.clock;
                last_eval = iter->second.eval;
                last_move = iter->second.move;
                break;
            }
        }
        int result;
        if (game_metadata["Result"] == "1-0") {
            result = 1;
        } else if (game_metadata["Result"] == "0-1") {
            result = -1;
        } else if (game_metadata["Result"] == "1/2-1/2") {
            result = 0;
        } else if (game_metadata["Result"] == "*") {
            continue;
        } else {
            std::cerr << "Can't read Result field from pgn header " << game_metadata["Result"] << std::endl;
            abort();
        }
        std::cout << game_metadata["Site"] << "\t" << game_metadata["WhiteElo"] << "\t" << game_metadata["BlackElo"] << "\t"
            << result << "\t" << last_clock << "\t" << last_eval << "\t" << last_move << "\t";
        b.get_fen(std::cout);
        std::cout << std::endl;
    }

    return 0;
}