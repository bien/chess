#include <boost/program_options.hpp>
#include <iostream>
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
            ("debug", po::value<int>(), "set debug level")
            ("quiescent", po::bool_switch(), "get quiescent eval")
            ("no-tt", po::bool_switch(), "turn off transposition table")
            ("moves", po::bool_switch(), "list moves")
            ("nnue", po::bool_switch(), "use nnue eval")
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
        if (vm["nnue"].as<bool>()){
            e = new NNUEEvaluation();
        } else {
            e = new SimpleBitboardEvaluation();
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
        if (vm.count("debug")) {
            search_debug = vm["debug"].as<int>();
        }
        if (vm.count("alpha")) {
            alpha = vm["alpha"].as<int>();
        }
        if (vm.count("beta")) {
            beta = vm["beta"].as<int>();
        }

        if (vm.count("fen")) {
            b.set_fen(vm["fen"].as<std::string>());
        } else {
            b.set_starting_position();
        }

        if (vm["moves"].as<bool>()) {
            Acquisition<MoveSorter> move_iter(&s);
            move_iter->reset(&b, &s, b.get_side_to_play(), true, 0, true);
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
            while (1) {
                b.apply_move(next_move);
                int ignore;
                int alpha = VERY_BAD;
                int beta = VERY_GOOD;
                if (s.read_transposition(b.get_hash(), next_move, 0, alpha, beta, ignore)) {
                    std::cout << " " << move_to_uci(next_move);
                } else {
                    break;
                }
            }
            std::cout << std::endl << "Node count = " << s.nodecount << " quiescent count = " << s.qnodecount << std::endl;
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