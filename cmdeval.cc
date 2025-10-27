#include <boost/program_options.hpp>
#include <iostream>
#include <set>
#include "fenboard.hh"
#include "evaluate.hh"
#include "search.hh"
#include "nnueeval.hh"
namespace po = boost::program_options;

int main(int argc, char **argv)
{
    try {

        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
            ("fen", po::value<std::string>(), "set board state")
            ("depth", po::value<int>(), "set depth")
            ("alpha", po::value<int>(), "set alpha")
            ("beta", po::value<int>(), "set beta")
            ("window", po::value<int>(), "set mtdf window sie")
            ("debug", po::value<int>(), "set debug level")
            ("tt-check", po::value<uint64_t>(), "debug for hash value")
            ("hash", po::bool_switch(), "output hash value")
            ("use-pv", po::bool_switch(), "use principal value search")
            ("quiescent", po::bool_switch(), "get quiescent eval")
            ("no-tt", po::bool_switch(), "turn off transposition table")
            ("moves", po::bool_switch(), "list moves")
            ("no-nnue", po::bool_switch(), "disable nnue eval")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        Fenboard b;
        Evaluation *e;
        if (vm["no-nnue"].as<bool>()){
            e = new SimpleBitboardEvaluation();
        } else {
            e = new NNUEEvaluation();
        }
        Search s(e);

        int depth = 0;
        int alpha = VERY_BAD;
        int beta = VERY_GOOD;
        int result;

        if (vm.count("depth")) {
            depth = vm["depth"].as<int>();
        }
        if (vm["no-tt"].as<bool>()) {
            s.use_transposition_table = false;
        }
        if (vm["use-pv"].as<bool>()) {
            s.use_mtdf = false;
            s.use_pv = true;
        } else {
            s.use_mtdf = true;
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

        if (vm["moves"].as<bool>()) {
            Acquisition<MoveSorter> move_iter(&s);
            move_iter->reset(&b, &s, vm["quiescent"].as<bool>(), depth, alpha, beta, true, 0, 0, true);
            while (move_iter->has_more_moves()) {
                move_t move = move_iter->next_move();
                std::cout << "  " << move_to_uci(move) << " sort=" << move_iter->get_score(&b, 0, move) << std::endl;
            }
        }
        if (depth > 0 || vm["quiescent"].as<bool>()) {
            int result_score;
            move_t result_move;

            s.max_depth = depth;
            if (vm.count("alpha") > 0 || vm.count("beta") > 0) {
                auto result_triple = s.negamax_with_memory(b, 0, alpha, beta);
                result_score = std::get<2>(result_triple);
                result_move = std::get<0>(result_triple);
            } else {
                result_move = s.alphabeta(b);
                result_score = s.score;

            }
            std::cout << "Move = " << move_to_uci(result_move) << " score = " << result_score << std::endl;
            std::cout << "Line = " << move_to_uci(result_move);
            move_t next_move = result_move;
            // avoid loops
            std::set<move_t> seen_moves;
            while (next_move != 0 && next_move != -1 && seen_moves.find(next_move) == seen_moves.end()) {
                b.apply_move(next_move);
                int ignore;
                int alpha = VERY_BAD;
                int beta = VERY_GOOD;
                if (s.read_transposition(b.get_hash(), next_move, 0, alpha, beta, ignore)) {
                    std::cout << " " << move_to_uci(next_move);
                } else {
                    break;
                }
                seen_moves.insert(next_move);
            }
            std::cout << std::endl << "Node count = " << s.nodecount << " expanded=" << s.moves_expanded << " quiescent count = " << s.qnodecount << std::endl;
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