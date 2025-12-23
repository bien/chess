#include <boost/program_options.hpp>
#include <iostream>
#include <vector>
#include "fenboard.hh"
#include "evaluate.hh"
#include "search.hh"
#include "move.hh"
#include "nnueeval.hh"
namespace po = boost::program_options;

void print_line(Fenboard &b, Search &s, move_t first_move)
{
    std::cout << "Line = " << move_to_uci(first_move);
    move_t next_move = first_move;
    // avoid loops
    std::vector<move_t> seen_moves;
    while (next_move != 0 && next_move != -1 && std::find(seen_moves.begin(), seen_moves.end(), next_move) == seen_moves.end()) {
        b.apply_move(next_move);
        seen_moves.push_back(next_move);
        int ignore;
        int alpha = VERY_BAD;
        int beta = VERY_GOOD;
        if (s.read_transposition(b.get_hash(), next_move, 0, alpha, beta, ignore)) {
            std::cout << " " << move_to_uci(next_move);
        } else {
            break;
        }
    }
    for (auto iter = seen_moves.rbegin(); iter != seen_moves.rend(); iter++) {
        b.undo_move(*iter);
    }
}

int main(int argc, char **argv)
{
    try {

        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
            ("fen", po::value<std::string>(), "set board state")
            ("line", po::value<std::string>(), "line to evaluate")
            ("depth", po::value<int>(), "set depth")
            ("alpha", po::value<int>(), "set alpha")
            ("qdepth", po::value<int>(), "set quiescent depth")
            ("beta", po::value<int>(), "set beta")
            ("window", po::value<int>(), "set mtdf window sie")
            ("debug", po::value<int>(), "set debug level")
            ("tt-check", po::value<uint64_t>(), "debug for hash value")
            ("hash", po::bool_switch(), "output hash value")
            ("use-pv", po::bool_switch(), "use principal value search")
            ("use-mtdf", po::bool_switch(), "use mtdf search")
            ("quiescent", po::bool_switch(), "get quiescent eval")
            ("no-tt", po::bool_switch(), "turn off transposition table")
            ("search-features", po::bool_switch(), "turn on search features")
            ("moves", po::bool_switch(), "list moves")
            ("only", po::value<std::string>(), "only move to consider")
            ("no-nnue", po::bool_switch(), "disable nnue eval")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }
        if (vm["search-features"].as<bool>()){
            search_features = 1;
        }
        Fenboard b;
        Evaluation *e;
        if (vm["no-nnue"].as<bool>()){
            e = new SimpleBitboardEvaluation();
        } else {
            e = new NNUEEvaluation();
        }
        Search s(e);

        s.recapture_first_bonus = 0;

        int depth = 0;
        int alpha = VERY_BAD;
        int beta = VERY_GOOD;

        if (vm.count("depth")) {
            depth = vm["depth"].as<int>();
        }
        if (vm.count("qdepth")) {
            s.quiescent_depth = vm["qdepth"].as<int>();
            if (s.quiescent_depth == 0) {
                s.use_quiescent_search = false;
            }
        }
        if (vm["no-tt"].as<bool>()) {
            s.use_transposition_table = false;
        }
        if (vm["use-pv"].as<bool>()) {
            s.use_mtdf = false;
            s.use_pv = true;
        } else if (vm["use-mtdf"].as<bool>()) {
            s.use_mtdf = true;
        } else {
            s.use_mtdf = false;
            s.use_iterative_deepening = false;
        }
        if (vm.count("debug")) {
            search_debug = vm["debug"].as<int>();
        }
        if (vm.count("tt-check")) {
            s.tt_hash_debug = vm["tt-check"].as<uint64_t>();
        }
        if (vm.count("alpha")) {
            alpha = vm["alpha"].as<int>();
        }
        if (vm.count("beta")) {
            beta = vm["beta"].as<int>();
        }

        if (vm.count("window")) {
            s.mtdf_window_size = vm["window"].as<int>();
        }

        if (vm.count("fen")) {
            b.set_fen(vm["fen"].as<std::string>());
        } else {
            b.set_starting_position();
        }

        if (vm["hash"].as<bool>()) {
            std::cout << "Hash: " << b.get_hash() << std::endl;
        }

        if (depth > 0 || vm["quiescent"].as<bool>()) {
            int result_score;
            move_t result_move;

            s.max_depth = depth;
            std::vector<move_t> line;
            if (vm.count("alpha") > 0 || vm.count("beta") > 0) {
                auto result_triple = s.negamax_with_memory(b, 0, alpha, beta, line);
                result_score = std::get<2>(result_triple);
                result_move = std::get<0>(result_triple);
            } else {
                result_move = s.alphabeta(b);
                result_score = s.score;

            }
            std::cout << "Move = " << move_to_uci(result_move) << " score = " << result_score << std::endl;
            std::cout << "Line = " << move_to_uci(result_move);
            print_line(b, s, result_move);
            std::cout << std::endl << "Node count = " << s.nodecount << " commenced=" << s.moves_commenced << " expanded=" << s.moves_expanded << " quiescent count = " << s.qnodecount << std::endl;
            float mrr_actual = 0;
            int sort_count = 0;
            for (int i = 0; i < NTH_SORT_FREQ_BUCKETS; i++) {
                sort_count += s.nth_sort_freq[i];
                mrr_actual += s.nth_sort_freq[i] * 1.0 / (i+1);
                if (s.nth_sort_freq[i] > 0) {
                    std::cout << i << "=" << s.nth_sort_freq[i] << " ";
                }
            }
            std::cout << std::endl << "MRR = " << mrr_actual / sort_count << std::endl;
            std::cout << "transposition stats: full_hits: " << s.transposition_full_hits
                << " partial hits: " << s.transposition_partial_hits
                << " insufficient_depth: " << s.transposition_insufficient_depth
                << " checks: " << s.transposition_checks
                << " conflicts: " << s.transposition_conflicts
                << std::endl;
            std::cout << "transposition stats: full_hits: " << (s.transposition_full_hits * 100.0 / s.transposition_checks)
                << " partial hits: " << (s.transposition_partial_hits * 100.0 / s.transposition_checks)
                << " insufficient_depth: " << (s.transposition_insufficient_depth * 100.0 / s.transposition_checks)
                << " misses: " << ((s.transposition_checks - s.transposition_full_hits - s.transposition_partial_hits - s.transposition_insufficient_depth) * 100.0 / s.transposition_checks)
                << std::endl;
        }
        std::vector<move_t> line;
        if (vm.count("line")) {
            std::string line = vm["line"].as<std::string>();
            size_t pos = 0;
            while (pos < line.size()) {
                std::string movetext;
                size_t comma = line.find(',', pos+1);
                if (comma == std::string::npos) {
                    movetext = line.substr(pos);
                    pos = line.size();
                } else {
                    movetext = line.substr(pos, comma - pos);
                    pos = comma + 1;
                }
                move_t prev_move = b.read_move(movetext, b.get_side_to_play());
                b.apply_move(prev_move);
                line.push_back(prev_move);
            }
        }
        if (vm["moves"].as<bool>()) {
            s.recapture_first_bonus = 0;
            // s.psqt_coeff = 0;
            Acquisition<MoveSorter> move_iter(&s);
            move_iter->reset(&b, &s, line, false, depth, alpha, beta, vm.count("only") == 0, 0, 0, -1, true);
            int static_score = e->evaluate(b);
            std::cout << "static score=" << static_score << std::endl;
            while (move_iter->has_more_moves()) {
                move_t move = move_iter->next_move();
                if (vm.count("only")) {
                    std::string only_move = vm["only"].as<std::string>();
                    if (b.read_move(only_move, b.get_side_to_play()) != move) {
                        continue;
                    }
                }
                int point_score = e->delta_evaluate(b, move, static_score);
                int score_parts[score_part_len];
                move_iter->get_score_parts(&b, move, line, score_parts);
                std::cout << "  " << move_to_uci(move) << " sort=" << move_iter->get_score(&b, move, line);
                std::cout << " see=" << score_parts[score_part_exchange];
                std::cout << " psqt=" << score_parts[score_part_psqt];
                std::cout << " score=" << point_score;
                std::cout << "      ";
                print_line(b, s, move);

                move_t tt_move;
                int tt_value = 0, tt_alpha = SCORE_MIN, tt_beta = SCORE_MAX;
                std::cout << " ";
                if (s.read_transposition(b.get_hash(), tt_move, 0, tt_alpha, tt_beta, tt_value)) {
                    std::cout << tt_value << " " << ((move & GIVES_CHECK) != 0 ? "1 " : "0 ") << (get_captured_piece(move) != 0 ? "1 " : "0 ") << (int)get_actor(move) << " ";
                }
                for (int i = 0; i < score_part_len; i++) {
                    std::cout << score_parts[i] << " ";
                }

                std::cout << std::endl;
            }
        }

    }
    catch(std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    catch(...) {
        std::cerr << "Exception of unknown type!\n";
    }

    return 0;
}