#include <chrono>
#include <iostream>
#include <fstream>
#include <string>
#include <time.h>
#include "search.hh"
#include "evaluate.hh"
#include "nnue.hh"
#include "pgn.hh"
#include "bitboard.hh"

void tokenize(const std::string &s, unsigned char delimiter, std::vector<std::string> &tokens)
{
    std::string::size_type pos = 0;
    while (pos != std::string::npos) {
        unsigned int nextdelim = s.find(delimiter, pos);
        if (nextdelim == std::string::npos) {
            tokens.push_back(s.substr(pos, s.length() - pos));
            break;
        }
        tokens.push_back(s.substr(pos, nextdelim - pos));
        pos = nextdelim + 1;
    }
}

struct UciSearchUpdate : SearchUpdate {
    void operator()(move_t best_move, int depth, int nodecount, int score) const
    {
        std::cout << "info currmove ";
        print_move_uci(best_move, std::cout) << std::endl;
        std::cout << "info depth " << depth
            << " score cp " << score
            << " nodes " << nodecount
            << std::endl;
    }

};

int main(int argc, char **argv)
{
    Fenboard b;
    b.set_starting_position();
    UciSearchUpdate searchUpdate;
    search_debug = 1;
    std::string line;
    std::ofstream logging("server.log", std::ofstream::out | std::ofstream::ate);
    std::istream *input = &std::cin;
    if (argc > 1) {
        input = new std::ifstream(argv[1]);
    }

    NNUEEvaluation simple;
    Search search(&simple);
    search.use_transposition_table = true;
    search.use_mtdf = true;
    search.use_quiescent_search = true;
    search.use_iterative_deepening = true;
    search.use_pruning = true;
    search.quiescent_depth = 4;
    search.max_depth = 10;

    while (true) {
        std::getline(*input, line);
        logging << line << std::endl;
        search.reset();
        if (line.rfind("isready", 0) == 0) {
            std::cout << "readyok" << std::endl;
        }
        else if (line.rfind("ucinewgame", 0) == 0) {
            b.set_starting_position();
        }
        else if (line.rfind("uci", 0) == 0) {
            std::cout << "id name lobsterbot" << std::endl;
            std::cout << "option name Hash type spin default 1 min 1 max 1024" << std::endl;
            std::cout << "option name depth type spin default 7 min 1 max 1024" << std::endl;
            std::cout << "option name quiescentlimit type spin default 4 min 0 max 1024" << std::endl;
            std::cout << "option name debug type spin default 0 min 0 max 8" << std::endl;
            std::cout << "uciok" << std::endl;
        }
        else if (line.rfind("setoption", 0) == 0) {
            // setoption name depth value 5
            std::vector<std::string> tokens;
            tokenize(line, ' ', tokens);
            if (tokens[2] == "depth" && tokens.size() > 4) {
                search.max_depth = stoi(tokens[4]);
                std::cout << "set depth = " << search.max_depth << std::endl;
            }
            else if (tokens[2] == "quiescentlimit" && tokens.size() > 4) {
                search.quiescent_depth = stoi(tokens[4]);
                std::cout << "set quiescentlimit = " << search.quiescent_depth << std::endl;
            }
            else if (tokens[2] == "debug" && tokens.size() > 4) {
                search_debug = stoi(tokens[4]);
                std::cout << "set debug = " << search_debug << std::endl;
            }
        }
        else if (line.rfind("position", 0) == 0) {
            std::vector<std::string> tokens;
            tokenize(line, ' ', tokens);
            unsigned int pos = 1;
            if (tokens.size() > 2 && tokens[1] == "fen") {
                b.set_fen(tokens[2]);
                pos += 2;
            }
            if (tokens[1] == "startpos") {
                b.set_starting_position();
                pos += 1;
            }
            if (tokens.size() >= 2 + pos && tokens[pos] == "moves") {
                for (unsigned int i = pos + 1; i < tokens.size(); i++) {
                    b.apply_move(b.read_move(tokens[i], b.get_side_to_play()));
                }
            }
        }
        else if (line.rfind("go", 0) == 0) {
            std::map<std::string, std::string> options;
            std::string key, value;
            int movemillisecs = 0;
            unsigned int pos = line.find(' ', 1);
            while (pos != std::string::npos) {
                unsigned int endpos = line.find(' ', pos + 1);
                if (endpos == std::string::npos) {
                    break;
                }
                key = line.substr(pos + 1, endpos - pos - 1);
                pos = line.find(' ', endpos + 1);
                if (pos == std::string::npos) {
                    pos = line.length();
                }
                value = line.substr(endpos + 1, pos - endpos + 1);
                logging << "**set option " << key << " = " << value << "***" << std::endl;
                options[key] = value;
                if (key == "movetime") {
                    movemillisecs = std::stoi(value);
                }
            }
            // find a move
            int time_millis = 30000;
            if (movemillisecs == 0) {
                // use (increment + remaining time / 15) * 90%
                int increment_millis;
                int time_millis;
                if (b.get_side_to_play() == White) {
                    increment_millis = std::stoi(options["winc"]);
                    time_millis = std::stoi(options["wtime"]);
                } else {
                    increment_millis = std::stoi(options["binc"]);
                    time_millis = std::stoi(options["btime"]);
                }
                movemillisecs = (increment_millis + time_millis / 30) * .9;
            }
            auto starttime = std::chrono::system_clock::now();

            search.time_available = movemillisecs / 1000;
            search.soft_deadline = time_millis > 60000;
            std::cout << "time allocation " << search.time_available << std::endl;
            move_t move = search.alphabeta(b, b.get_side_to_play(), searchUpdate);
            auto elapsed_usecs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - starttime).count();
            std::cout << "info currmove ";
            print_move_uci(move, std::cout) << " currmovenumber " << b.get_move_count() << std::endl;
            std::cout << "info depth " << search.max_depth
                    << " score cp " << search.score
                    << " time " << elapsed_usecs / 1000
                    << " nodes " << search.nodecount
                    << " nps " << static_cast<uint64_t>(search.nodecount) * 1000 * 1000 / elapsed_usecs << std::endl;
            std::cout << "bestmove ";
            print_move_uci(move, std::cout) << std::endl;
        }
        else if (line.rfind("quit", 0) == 0) {
            break;
        }
    }
    return 0;
}