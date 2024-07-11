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
const int search_debug = 0;

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
    std::pair<move_t, int> result = alphabeta_with_memory(b, 0, color, 0, 0);
    score = result.second;
    if (color == Black) {
        score = -score;
    }
    use_pruning = old_pruning;
    use_mtdf = old_mtdf;
    use_transposition_table = old_trans_table;
    return result.first;
}

move_t Search::alphabeta(Fenboard &b, Color color, const SearchUpdate &s)
{
    nodecount = 0;
    move_t result;
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
        std::pair<move_t, int> sub = alphabeta_with_memory(b, 0, color, SCORE_MIN, SCORE_MAX);
        score = sub.second;
        result = sub.first;
    }
    return result;
}

Search::Search(const Evaluation *eval, int transposition_table_size)
    : score(0), nodecount(0), transposition_table_size(transposition_table_size), use_transposition_table(false), use_pruning(true), eval(eval), min_score_prune_sorting(2), use_mtdf(true), use_iterative_deepening(true), use_quiescent_search(false), quiescent_depth(2)

{
    srandom(clock());
}

void Search::reset()
{
    score = 0;
    nodecount = 0;
    transposition_table.clear();
}

move_t Search::mtdf(Fenboard &b, Color color, int &score, int guess)
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
        if (score == lowerbound) {
            beta = score + 1;
        } else {
            beta = score;
        }
        std::pair<move_t, int> result = alphabeta_with_memory(b, 0, color, beta - 1, beta);
        if (result.first != -1) {
            move = result.first;
        }
        score = result.second;
        if (score < beta) {
            upperbound = result.second;
        } else {
            lowerbound = result.second;
        }
        if (search_debug) {
            std::cout << "  mtdf a=" << beta - 1 << " b=" << beta << " score=" << score << " upper=" << upperbound << " lower=" << lowerbound << " move=";
            b.print_move(move, std::cout);
            std::cout << std::endl;
        }
    } while (lowerbound < upperbound);

    return move;
}

std::pair<move_t, int> Search::alphabeta_with_memory(Fenboard &b, int depth, Color color, int alpha, int beta)
{
    nodecount++;

    TranspositionEntry bounds;
    bounds.depth = max_depth - depth;

    int is_white = color == White ? 1 : -1;

    // cutoff early if alpha or beta is impossibly high (ie if there is a checkmate higher in the tree)

    if (alpha > VERY_GOOD - depth) {
        return std::pair<move_t, int>(-1, VERY_GOOD - depth);
    }
    else if (beta < VERY_BAD + depth) {
        return std::pair<move_t, int>(-1, VERY_BAD + depth);
    }


    // check transposition table
    if (use_transposition_table) {
        auto transpose = transposition_table.find(b.get_hash());
        if (transpose != transposition_table.end()) {
            TranspositionEntry &entry = transpose.second;
            if (entry.depth >= max_depth - depth) {
                // found something at appropriate depth
                if (entry.lower >= beta) {
                    return std::pair<move_t, int>(entry.move, entry.lower);
                } else if (entry.upper <= alpha) {
                    return std::pair<move_t, int>(entry.move, entry.upper);
                }
                alpha = std::max(alpha, entry.lower);
                beta = std::min(beta, entry.upper);
            }
        }
    }

    if ((!use_quiescent_search && depth == max_depth) || (use_quiescent_search && depth == max_depth + quiescent_depth)) {
        return std::pair<move_t, int>(0, eval->evaluate(b));
    }

    int original_alpha = alpha;
    int original_beta = beta;
    BitboardMoveIterator iter = b.get_legal_moves(color);

    if (!b.has_more_moves(iter)) {

        if (search_debug) {
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
            return std::pair<move_t, int>(0, is_white * (VERY_BAD + depth));
        } else {
            return std::pair<move_t, int>(0, 0);
        }
    }

    move_t best_move = 0;
    int best_score = 0;

    bool first = true;


    int currentnodescore;
    bool evaluated_nodescore = false;
    int tie_count = 1;

    while (b.has_more_moves(iter)) {
        int subtree_score;
        move_t submove = 0;
        move_t move = b.get_next_move(iter);
        bool cutoff;



        if (search_debug) {
            for (int i = 0; i < depth * 2; i++) {
                std::cout << " ";
            }
            std::cout << (depth / 2 + 1) << ".";
            if (depth % 2 == 1) {
                std::cout << "..";
            }
            b.print_move(move, std::cout);
            std::cout << std::endl;
        }

        if (use_quiescent_search && get_captured_piece(move, White) != EMPTY) {
            cutoff = (depth >= max_depth + quiescent_depth - 1);
        } else {
            cutoff = (depth >= max_depth - 1);
        }

        if (cutoff) {
            if (!evaluated_nodescore) {
                currentnodescore = eval->evaluate(b);
                evaluated_nodescore = true;
            }

            subtree_score = eval->delta_evaluate(b, move, currentnodescore);
        } else {
            b.apply_move(move);
            std::pair<move_t, int> child = alphabeta_with_memory(b, depth + 1, get_opposite_color(color), alpha, beta);
            subtree_score = child.second;
            submove = child.first;
            b.undo_move(move);

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
            std::cout << " = " << subtree_score;

            if (cutoff) {
                std::cout << " ** cutoff";
            }
            std::cout << std::endl;
        }

        if (first || (color == White && subtree_score > best_score) || (color == Black && subtree_score < best_score)) {
            // ignore pruned branches
            best_score = subtree_score;
            if (submove != -1) {
                best_move = move;
            }
            tie_count = 1;
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
                    for (int i = 0; i < depth * 2; i++) {
                        std::cout << " ";
                    }
                    std::cout << "Pruned" << std::endl;
                }

                best_move = -1;
                break;
            }
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
    return std::pair<move_t, int>(best_move, best_score);
}
