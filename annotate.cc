#include <boost/program_options.hpp>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <set>
#include "pgn.hh"
#include "search.hh"
#include "evaluate.hh"
#include "fenboard.hh"
#include "nnueeval.hh"

namespace po = boost::program_options;

int main(int argc, char **argv)
{
    std::vector<std::string> pgnfiles;
    Fenboard b;
    Evaluation *e;
    Search *s;
    int games = 1;
    bool see_eval = false;

    try {

        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
            ("depth", po::value<int>(), "set depth")
            ("games", po::value<int>(), "number of games to annotate")
            ("qdepth", po::value<int>(), "set quiescent depth")
            ("debug", po::value<int>(), "set debug level")
            ("no-tt", po::bool_switch(), "turn off transposition table")
            ("search-features", po::bool_switch(), "turn on search features")
            ("see-eval", po::bool_switch(), "turn on see eval stats")
            ("no-nnue", po::bool_switch(), "disable nnue eval")
            ("input-file", po::value<std::vector<std::string> >(), "input file")
        ;

        po::positional_options_description p;
        p.add("input-file", -1);
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
        po::notify(vm);
        int depth = 6;

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }
        if (vm["search-features"].as<bool>()){
            search_features = 1;
        }
        if (vm["see-eval"].as<bool>()){
            see_eval = true;
        }
        if (vm.count("input-file")) {
            pgnfiles = vm["input-file"].as<std::vector<std::string>>();
        }
        if (vm["no-nnue"].as<bool>()){
            e = new SimpleBitboardEvaluation();
        } else {
            e = new NNUEEvaluation();
        }
        s = new Search(e);

        if (vm.count("depth")) {
            depth = vm["depth"].as<int>();
        }
        if (vm.count("games")) {
            games = vm["games"].as<int>();
        }
        s->max_depth = depth;
        if (vm.count("qdepth")) {
            s->quiescent_depth = vm["qdepth"].as<int>();
            if (s->quiescent_depth == 0) {
                s->use_quiescent_search = false;
            }
        }
        if (vm["no-tt"].as<bool>()) {
            s->use_transposition_table = false;
        }
        if (vm.count("debug")) {
            search_debug = vm["debug"].as<int>();
        }
        if (vm.count("window")) {
            s->mtdf_window_size = vm["window"].as<int>();
        }
    }
    catch(std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    catch(...) {
        std::cerr << "Exception of unknown type!\n";
    }
    // play some games
    uint64_t total_nodecount = 0;
    for (auto pgnfilename : pgnfiles) {
        std::ifstream pgnstream(pgnfilename);
        if (!pgnstream) {
            std::cout << "Cannot load " << pgnfilename << ": " << strerror(errno) << std::endl;
            return -1;
        }
        pgn_istream mypgn(pgnstream);
        for (int gameno = 0; gameno < games; gameno++) {
            b.set_starting_position();
            s->reset();
            std::map<std::string, std::string> game_metadata;
            std::vector<std::pair<move_annot, move_annot> > movelist;

            read_pgn(&mypgn, game_metadata, movelist);
            int plyno = 0;
            for (auto iter = movelist.begin(); iter != movelist.end(); ) {
                std::string move_text;
                if (b.get_side_to_play() == White) {
                    move_text = iter->first.move;
                } else if (iter->second.move.length() > 1){
                    move_text = iter->second.move;
                    ++iter;
                } else {
                    break;
                }
                move_t move = b.read_move(move_text, b.get_side_to_play());
                auto starttime = std::chrono::system_clock::now();
                move_t suggested_move = s->alphabeta(b);
                auto elapsed_usecs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - starttime).count();
                int result_score = s->score;
                std::cout << ((plyno / 2) + 1) << (b.get_side_to_play() == White ? ". " : "... ") << move_text;
                if (move == suggested_move) {
                    std::cout << " eval=" << result_score;
                }
                else {
                    move_t tt_move;
                    int tt_value = 0;
                    int tt_alpha = SCORE_MIN, tt_beta = SCORE_MAX;
                    if (s->read_transposition(b.get_zobrist_with_move(move), tt_move, 0, tt_alpha, tt_beta, tt_value)) {
                        std::cout << " eval=" << tt_value;
                    }
                    if (abs(result_score - tt_value) > 10) {
                        std::cout << " (";
                        b.print_move(suggested_move, std::cout);
                        std::cout << " eval=" << result_score;
                        std::cout << ")";
                    }
                }
                if (see_eval) {
                    Acquisition<MoveSorter> move_iter(s);
                    s->psqt_coeff = 1;
                    int suggested_move_see_score = 0;
                    std::vector<move_t> line;
                    std::vector<std::pair<move_t, int> > see_scores;
                    move_iter->reset(&b, s, line, false, 0, SCORE_MIN, SCORE_MAX, true, 0, 0, -1, true);
                    while (move_iter->has_more_moves()) {
                        move_t testmove = move_iter->next_move();
                        int score_parts[score_part_len];
                        move_iter->get_score_parts(&b, testmove, line, score_parts);
                        see_scores.push_back(std::make_pair(testmove, score_parts[score_part_exchange] + score_parts[score_part_psqt]));
                        if (testmove == suggested_move) {
                            suggested_move_see_score = see_scores.back().second;
                        }
                    }
                    std::sort(see_scores.begin(), see_scores.end(), [](const std::pair<move_t, int> &a, const std::pair<move_t, int> &b) {
                        return a.second > b.second;
                    });
                    if (see_scores.size() > 1) {
                        if (see_scores[0].second - 100 > suggested_move_see_score) {
                            std::cout << " see discount:";
                            b.print_move(see_scores[0].first, std::cout);
                            std::cout << " score=" << see_scores[0].second << " suggested=";
                            b.print_move(suggested_move, std::cout);
                            std::cout << " score=" << suggested_move_see_score;
                        }
                    }
                }

                std::cout << " time=" << elapsed_usecs / 1000.0 << "ms ";
                std::cout << "Node count=" << s->nodecount << " commenced=" << s->moves_commenced << " expanded=" << s->moves_expanded << " quiescent count = " << s->qnodecount;
                std::cout << std::endl;
                total_nodecount += s->nodecount;
                b.apply_move(move);
                plyno++;
                s->reset_counters();
            }
            std::cout << "game over" << std::endl;
        }
    }
    std::cout << "Total nodecount=" << total_nodecount << std::endl;
}