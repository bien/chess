#include <limits.h>
#include <vector>
#include <utility>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <set>
#include <stdlib.h>
#include <map>
#include "search.hh"
#include "bitboard.hh"
#include "move.hh"
int search_debug = 0;

int Evaluation::delta_evaluate(Fenboard &b, move_t move, int previous_score) const
{
    int score;
    b.apply_move(move);
    score = evaluate(b);
    b.undo_move(move);
    return score;
}

move_t Search::minimax(Fenboard &b, Color color)
{
    nodecount = 0;
    bool old_pruning = use_pruning;
    bool old_mtdf = use_mtdf;
    bool old_trans_table = use_transposition_table;
    use_pruning = false;
    use_mtdf = false;
    use_transposition_table = false;
    std::tuple<move_t, move_t, int> result = alphabeta_with_memory(b, 0, color, 0, 0);
    score = std::get<2>(result);
    if (color == Black) {
        score = -score;
    }
    use_pruning = old_pruning;
    use_mtdf = old_mtdf;
    use_transposition_table = old_trans_table;
    return std::get<0>(result);
}

move_t Search::timed_iterative_deepening(Fenboard &b, Color color, const SearchUpdate &s)
{
    time_t deadline = time(NULL) + time_available;
    move_t result;
    score = 0;
    int guess = eval->evaluate(b);
    int old_max_depth = max_depth;
    int depth = 2;
    while (time(NULL) < deadline && depth < old_max_depth) {
        max_depth = depth;
        int new_score = 0;
        move_t new_result = mtdf(b, color, new_score, guess, deadline);
        std::cout << "depth " << depth << " ";
        b.print_move(new_result, std::cout);
        std::cout << " eval=" << new_score << std::endl;
        if (new_result != 0) {
            result = new_result;
            score = new_score;
            s(result, max_depth, nodecount, score);
            guess = score;
        }
        depth += 1;
    }
    max_depth = old_max_depth;
    return result;
}

move_t Search::alphabeta(Fenboard &b, Color color, const SearchUpdate &s)
{
    nodecount = 0;
    move_t result;
    if (use_mtdf && use_iterative_deepening && time_available > 0) {
        return timed_iterative_deepening(b, color, s);
    }
    if (use_mtdf) {
        int guess = 0;
        if (!use_iterative_deepening || max_depth < 2) {
            guess = eval->evaluate(b);
        } else {
            int subscore = 0;
            max_depth -= 2;
            result = mtdf(b, color, subscore, guess);
            s(result, max_depth, nodecount, subscore);
            max_depth += 2;
            guess = subscore;
        }
        result = mtdf(b, color, score, guess);
    } else {
        std::tuple<move_t, move_t, int> sub = alphabeta_with_memory(b, 0, color, SCORE_MIN, SCORE_MAX);
        score = std::get<2>(sub);
        result = std::get<0>(sub);
    }
    return result;
}

Search::Search(const Evaluation *eval, int transposition_table_size)
    : score(0), nodecount(0), transposition_table_size(transposition_table_size), use_transposition_table(true), use_pruning(true), eval(eval), min_score_prune_sorting(2), use_mtdf(true), use_iterative_deepening(true), use_quiescent_search(false), quiescent_depth(2), use_killer_move(true), time_available(0)

{
    srandom(clock());
}

void Search::reset()
{
    score = 0;
    nodecount = 0;
    transposition_table.clear();
}

move_t Search::mtdf(Fenboard &b, Color color, int &score, int guess, time_t deadline)
{
    score = guess;
    int upperbound = SCORE_MAX;
    int lowerbound = SCORE_MIN;
    move_t move = 0;
    if (search_debug) {
        std::cout << "mtdf guess=" << guess << " depth=" << max_depth << std::endl;
    }

    do {
        int beta;
        if (deadline > 0 && time(NULL) > deadline) {
            return 0;
        }
        if (score == lowerbound) {
            beta = score + 1;
        } else {
            beta = score;
        }
        std::tuple<move_t, move_t, int> result = alphabeta_with_memory(b, 0, color, beta - 1, beta);
        if (std::get<0>(result) != -1) {
            move = std::get<0>(result);
        }
        score = std::get<2>(result);
        if (score < beta) {
            upperbound = score;
        } else {
            lowerbound = score;
        }
        if (search_debug) {
            std::cout << "  mtdf a=" << beta - 1 << " b=" << beta << " score=" << score << " upper=" << upperbound << " lower=" << lowerbound << " move=";
            b.print_move(move, std::cout);
            std::cout << std::endl;
        }
    } while (lowerbound < upperbound);

    return move;
}

// principal move, principal reply, cp score
std::tuple<move_t, move_t, int> Search::alphabeta_with_memory(Fenboard &b, int depth, Color color, int alpha, int beta, move_t hint)
{
    nodecount++;

    TranspositionEntry bounds;
    bounds.depth = max_depth - depth;

    int is_white = color == White ? 1 : -1;


    // cutoff early if alpha or beta is impossibly high (ie if there is a checkmate higher in the tree)

    if (alpha > VERY_GOOD - depth) {
        return std::tuple<move_t, move_t, int>(-1, -1, VERY_GOOD - depth);
    }
    else if (beta < VERY_BAD + depth) {
        return std::tuple<move_t, move_t, int>(-1, -1, VERY_BAD + depth);
    }


    // check transposition table
    if (use_transposition_table) {
        auto transpose = transposition_table.find(b.get_hash());
        if (transpose != transposition_table.end()) {
            TranspositionEntry &entry = transpose.second;
            if (entry.depth >= max_depth - depth) {
                // found something at appropriate depth
                if (entry.lower >= beta) {
                    return std::tuple<move_t, move_t, int>(entry.move, entry.response, entry.lower);
                } else if (entry.upper <= alpha) {
                    return std::tuple<move_t, move_t, int>(entry.move, entry.response, entry.upper);
                }
                alpha = std::max(alpha, entry.lower);
                beta = std::min(beta, entry.upper);
            }
        }
    }

    if ((!use_quiescent_search && depth == max_depth) || (use_quiescent_search && depth == max_depth + quiescent_depth)) {
        return std::tuple<move_t, move_t, int>(0, 0, eval->evaluate(b));
    }

    int original_alpha = alpha;
    int original_beta = beta;
    BitboardMoveIterator iter = b.get_legal_moves(color);

    if (!b.has_more_moves(iter)) {

        if (search_debug) {
            for (int i = 0; i < depth * 2; i++) {
                std::cout << " ";
            }
            std::cout << (depth / 2 + 1) << ".";
            if (depth % 2 == 1) {
                std::cout << "..";
            }

            for (int i = 0; i < 10 - depth * 2; i++) {
                std::cout << " ";
            }
            std::cout << " end ";
            if (b.king_in_check(color)) {
                std::cout << is_white * (VERY_BAD + depth) << std::endl;
            } else {
                std::cout << 0 << std::endl;
            }
        }

        if (b.king_in_check(color)) {
            return std::tuple<move_t, move_t, int>(0, 0, is_white * (VERY_BAD + depth));
        } else {
            return std::tuple<move_t, move_t, int>(0, 0, 0);
        }
    }

    move_t best_move = 0;
    move_t best_response = 0;
    int best_score = 0;

    bool first = true;


    int currentnodescore;
    bool evaluated_nodescore = false;
    int tie_count = 1;

    while (b.has_more_moves(iter)) {
        int subtree_score;
        move_t subresponse = 0;
        move_t submove = 0;
        move_t move = b.get_next_move(iter);
        bool cutoff;

        if (use_quiescent_search && get_captured_piece(move, White) != EMPTY) {
            cutoff = (depth >= max_depth + quiescent_depth - 1);
        } else {
            cutoff = (depth >= max_depth - 1);
        }

        if (search_debug) {
            for (int i = 0; i < depth * 2; i++) {
                std::cout << " ";
            }
            std::cout << (depth / 2 + 1) << ".";
            if (depth % 2 == 1) {
                std::cout << "..";
            }
            b.print_move(move, std::cout);
            std::cout << " " << alpha << " " << beta;
        }


        if (cutoff) {
            if (!evaluated_nodescore) {
                currentnodescore = eval->evaluate(b);
                evaluated_nodescore = true;
            }

            subtree_score = eval->delta_evaluate(b, move, currentnodescore);
            if (search_debug) {
                std::cout << " d= " << subtree_score;
            }

        } else {
            if (search_debug) {
                std::cout << std::endl;
            }
            b.apply_move(move);
            move_t killer_move = 0;
            
            if (use_killer_move && best_response != 0 && b.is_legal_move(best_response, b.get_side_to_play())) {
                killer_move = best_response;
            }
            
            std::tuple<move_t, move_t, int> child = alphabeta_with_memory(b, depth + 1, get_opposite_color(color), alpha, beta, killer_move);
            subtree_score = std::get<2>(child);
            submove = std::get<0>(child);
            subresponse = std::get<1>(child);
            b.undo_move(move);
            if (search_debug) {
                for (int i = 0; i < depth * 2; i++) {
                    std::cout << " ";
                }
                std::cout << (depth / 2 + 1) << ".";
                if (depth % 2 == 1) {
                    std::cout << "..";
                }
                b.print_move(move, std::cout);
            }
        }

        if (search_debug) {
            std::cout << " = " << subtree_score;
        }

        if (first || (color == White && subtree_score > best_score) || (color == Black && subtree_score < best_score)) {
            // ignore pruned branches
            best_score = subtree_score;
            if (submove != -1) {
                best_move = move;
                best_response = submove;
            }
            tie_count = 1;

            if (search_debug) {
                std::cout << "*";
            }
        // make things slightly less predictable
        } else if (submove != -1 && subtree_score == best_score) {
            tie_count++;
            if ((random() % 101) <= (101 / tie_count)) {
                best_move = move;
            }
        }
        first = false;

        if (use_pruning) {
            if (color == White) {
                alpha = std::max(alpha, best_score);
            }
            else {
                beta = std::min(beta, best_score);
            }
            if (alpha > beta) {
                // prune this branch
                if (search_debug) {
                    std::cout << "~" << std::endl;
                }

                best_move = -1;
                break;
            }
        }
        if (search_debug) {
            std::cout << " -> ";
            b.print_move(best_move, std::cout);
            std::cout << " -> ";
            b.print_move(best_response, std::cout);
            std::cout << std::endl;
        }

    }

    // write to transposition table
    if (use_transposition_table) {
        if (use_transposition_table && bounds.depth == max_depth - depth) {
            bool is_dirty = true;
            if (best_score <= original_alpha) {
                bounds.upper = best_score;
            } else if (best_score > original_alpha && best_score < original_beta) {
                bounds.lower = best_score;
                bounds.upper = best_score;
                bounds.move = best_move;
                bounds.response = best_response;
            } else if (best_score >= original_beta) {
                bounds.lower = best_score;
            } else {
                is_dirty = false;
            }
            if (is_dirty) {
                transposition_table[b.get_hash()] = bounds;
            }
        }
    }
/*
    if (best_move == 0) {
        if (evaluated_nodescore) {
            best_score = currentnodescore;
        } else {
            best_score = eval->evaluate(b);
        }
    }
*/
    return std::tuple<move_t, move_t, int>(best_move, best_response, best_score);
}
