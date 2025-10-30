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
#include "net/psqt.h"

int search_debug = 0;
const int LIMITED_QUIESCENT_DEPTH = 4;
const int MAX_HISTORY = 10000;
const int HISTORY_SCALER = 500;

static int vertical_mirror(int square) {
    return square ^ 56;
}

const int PIECE_VALUE[] = { 0, 1, 3, 3, 5, 8, 1000 };

move_t Search::minimax(Fenboard &b, Color color)
{
    nodecount = 0;
    bool old_pruning = use_pruning;
    bool old_mtdf = use_mtdf;
    use_pruning = false;
    use_mtdf = false;
    std::tuple<move_t, move_t, int> result = negamax_with_memory(b, 0, SCORE_MIN, SCORE_MAX);
    score = std::get<2>(result);
    if (color == Black) {
        score = -score;
    }
    use_pruning = old_pruning;
    use_mtdf = old_mtdf;
    return std::get<0>(result);
}

move_t Search::timed_iterative_deepening(Fenboard &b, const SearchUpdate &s)
{
    time_t deadline = time(NULL) + time_available;
    move_t result = 0;
    score = 0;
    int guess = eval->evaluate(b);
    int old_max_depth = max_depth;
    int depth = 2;
    do {
        max_depth = depth;
        int new_score = 0;
        int mtdf_deadline = deadline;
        if (soft_deadline) {
            mtdf_deadline = 0;
        }
        move_t new_result = mtdf(b, new_score, guess, mtdf_deadline, result);
        if (search_debug) {
            std::cout << "depth " << depth << " ";
            b.print_move(new_result, std::cout);
            std::cout << " eval=" << new_score << " timeused=" << (time_available - deadline + time(NULL)) << std::endl;
        }

        if (new_result != 0) {
            result = new_result;
            score = new_score;
            s(result, max_depth, nodecount, score);
            guess = score;
        }
        depth += 1;
    } while (time(NULL) < deadline && depth < old_max_depth);
    max_depth = old_max_depth;
    return result;
}

move_t Search::alphabeta(Fenboard &b, const SearchUpdate &s)
{
    nodecount = 0;
    move_t result = 0;

    if (use_mtdf && use_iterative_deepening && time_available > 0) {
        return timed_iterative_deepening(b, s);
    }
    if (use_iterative_deepening) {
        int old_max_depth = max_depth;
        int guess_score = 0;

        for (int iter_depth = (old_max_depth % 2 == 1 ? 1 : 0); iter_depth <= old_max_depth; iter_depth += 2) {
            max_depth = iter_depth;
            if (use_mtdf) {
                result = mtdf(b, score, guess_score, 0, result);
            } else if (use_pv) {
                std::tuple<move_t, move_t, int> sub;
                int alpha = guess_score - 25;
                int beta = guess_score + 25;
                int backoff = 200;
                while (true) {
                    sub = negamax_with_memory(b, 0, alpha, beta, result);
                    score = std::get<2>(sub);
                    if (search_debug) {
                        std::cout << "pv at depth=" << max_depth << " [" << alpha << "," << beta << "] -> " << score << std::endl;
                    }
                    if (score < alpha) {
                        beta = alpha;
                        alpha = score - backoff;
                    } else if (score > beta) {
                        alpha = beta;
                        beta = score + backoff;
                    } else {
                        break;
                    }
                    backoff *= 2;
                }
                result = std::get<0>(sub);
            }
            s(result, max_depth, nodecount, score);
            guess_score = score;
        }
        max_depth = old_max_depth;
    } else {
        std::tuple<move_t, move_t, int> sub;
        sub = negamax_with_memory(b, 0, SCORE_MIN, SCORE_MAX);
        score = std::get<2>(sub);
        if (b.get_side_to_play() == Black){
            score = -score;
        }
        result = std::get<0>(sub);
    }
    return result;
}

Search::Search(Evaluation *eval, int transposition_table_size_log2)
    : score(0), nodecount(0), qnodecount(0), transposition_table_size(1ULL << transposition_table_size_log2), use_transposition_table(true),
        use_pruning(true), eval(eval), min_score_prune_sorting(2), use_mtdf(false), use_pv(true), use_iterative_deepening(true),
        use_quiescent_search(true), use_killer_move(true), mtdf_window_size(10), quiescent_depth(5), time_available(0), max_depth(8), soft_deadline(true)

{
    srandom(clock());
    transposition_checks = 0;
    transposition_partial_hits = 0;
    transposition_full_hits = 0;
    transposition_insufficient_depth = 0;
    transposition_conflicts = 0;
    moves_expanded = 0;
    moves_commenced = 0;
    transposition_table = new uint64_t[transposition_table_size];
    for (int i = 0; i < transposition_table_size; i++) {
        transposition_table[i] = 0;
    }
    for (int i = 0; i < max_depth; i++) {
        move_sorter_pool.push_back(new MoveSorter());
    }
    for (int i = 0; i < NTH_SORT_FREQ_BUCKETS; i++) {
        nth_sort_freq[i] = 0;
    }

    memset(&history_bonus, 0, sizeof(history_bonus));
}

void Search::reset()
{
    score = 0;
    nodecount = 0;
    qnodecount = 0;
    for (int i = 0; i < transposition_table_size; i++) {
        transposition_table[i] = 0;
    }
    memset(&history_bonus, 0, sizeof(history_bonus));
    moves_expanded = 0;
    moves_commenced = 0;
}

move_t Search::mtdf(Fenboard &b, int &score, int guess, time_t deadline, move_t move)
{
    score = guess;
    int upperbound = SCORE_MAX;
    int lowerbound = SCORE_MIN;
    if (search_debug) {
        std::cout << "mtdf guess=" << guess << "(" << move_to_uci(move) << ") depth=" << max_depth << std::endl;
    }

    bool last_search = false;
    bool first = true;
    uint64_t mtdf_node_start = nodecount;

    do {
        int beta;
        if (deadline > 0 && time(NULL) > deadline) {
            return 0;
        }
        int alpha = std::max(score - mtdf_window_size, lowerbound);
        beta = std::min(upperbound, alpha + mtdf_window_size);

        // if we know there's a checkmate then don't bother being cute
        if (!first) {
            if (beta < -9900 + mtdf_window_size) {
                alpha = SCORE_MIN;
                last_search = true;
            }
            else if (alpha > 9900 - mtdf_window_size) {
                beta = SCORE_MAX;
                last_search = true;
            }
        }
        uint64_t start_nodecount = nodecount;
        if (search_debug) {
            std::cout << "  start mtdf a=" << alpha << " b=" << beta << " score=" << score << " move=";
            b.print_move(move, std::cout);
            std::cout << std::endl;
        }

        std::tuple<move_t, move_t, int> result;
        if (b.get_side_to_play() == White) {
            result = negamax_with_memory(b, 0, alpha, beta, move);
            score = std::get<2>(result);
        } else {
            result = negamax_with_memory(b, 0, -beta, -alpha, move);
            score = -std::get<2>(result);
        }
        if (std::get<0>(result) != -1 && std::get<0>(result) != 0) {
            move = std::get<0>(result);
        }
        if (search_debug) {
            std::cout << "  mtdf a=" << alpha << " b=" << beta << " score=" << score << " lower=" << lowerbound << " upper=" << upperbound << " move=";
            b.print_move(move, std::cout);
            std::cout << " nodes=" << (nodecount - start_nodecount) << std::endl;
        }
        if (score < beta && score >= alpha) {
            break;
        } else if (score < beta) {
            upperbound = score;
        } else {
            lowerbound = score;
        }
        first = false;
    } while (lowerbound < upperbound && !last_search);
    if (search_debug) {
        std::cout << "done mtdf nodes=" << (nodecount - mtdf_node_start) << std::endl;
    }
    return move;
}

// principal move, principal reply, cp score
std::tuple<move_t, move_t, int> Search::negamax_with_memory(Fenboard &b, int depth, int alpha, int beta, move_t hint, int static_score, const std::string &line)
{
    nodecount++;

    int best_score = INT_MIN;
    move_t best_response = -1;
    move_t best_move = -1;
    int original_alpha = alpha;
    int original_beta = beta;

    // check for endgames
    int endgame_eval;
    if (eval->endgame(b, endgame_eval)) {
        if (b.get_side_to_play() != White) {
            endgame_eval = -endgame_eval;
        }
        return std::tuple<move_t, move_t, int>(-1, -1, endgame_eval);
    }

    if (alpha > VERY_GOOD - depth) {
        // checkmate higher in the tree, no need to keep searching
        return std::tuple<move_t, move_t, int>(-1, -1, alpha - 1);
    }

    bool is_quiescent = (depth >= max_depth) && use_quiescent_search && depth <= max_depth + quiescent_depth;

    // check for repetition
    if (b.times_seen() >= 3) {
        return std::tuple<move_t, move_t, int>(-1, -1, 0);
    }

    bool tt_hint = false;
    move_t tt_move = 0;

    // check transposition table
    if (use_transposition_table) {

        int exact_value = 0;

        bool found = read_transposition(b.get_hash(), tt_move, max_depth - depth, alpha, beta, exact_value);
        if (found && alpha > beta) {
            return std::tuple<move_t, move_t, int>(tt_move, 0, exact_value);
        }
    }

    if (is_quiescent) {
        qnodecount++;
    }

    if (depth >= max_depth && !is_quiescent) {
        if (static_score >= VERY_BAD && static_score <= VERY_GOOD) {
            best_score = static_score;
        } else {
            best_score = eval->evaluate(b);
        }
        if (b.get_side_to_play() == Black) {
            best_score = -best_score;
        }
    } else {
        Acquisition<MoveSorter> move_iter(this);
        int depth_to_go = max_depth - depth;
        move_iter->reset(&b, this, is_quiescent, std::max(0, max_depth - depth), alpha, beta, depth_to_go > 2, alpha, hint, tt_hint);

        if (!move_iter->has_more_moves()) {
            if (b.king_in_check(b.get_side_to_play())) {
                return std::tuple<move_t, move_t, int>(0, 0, VERY_BAD + depth);
            } else {
                return std::tuple<move_t, move_t, int>(0, 0, 0);
            }
        }
        bool first = true;

        // null move
        int null_move_eval = 0;
        bool initialized_null_move_eval = false;
        if (is_quiescent) {
            int quiescent_depth = depth - max_depth;
            // null move isn't necessarily valid if we're in check
            if (!b.king_in_check(b.get_side_to_play()) || quiescent_depth > LIMITED_QUIESCENT_DEPTH) {
                initialized_null_move_eval = true;
                null_move_eval = eval->evaluate(b);
                if (b.get_side_to_play() == Black) {
                    null_move_eval = -null_move_eval;
                }
                best_score = null_move_eval;
                if (search_debug >= depth + 1) {
                    for (int i = 0; i < depth * 2; i++) {
                        std::cout << " ";
                    }
                    std::cout << (depth / 2 + 1) << ".";
                    if (depth % 2 == 1) {
                        std::cout << "..";
                    }
                    std::cout << "(empty) -> " << best_score << std::endl;
                }
            }
            if (best_score > beta) {
                return std::tuple<move_t, move_t, int>(0, 0, best_score);
            }
            else if (best_score > alpha) {
                alpha = best_score;
            }
        }

        move_t submove = 0;
        int static_eval = VERY_BAD - 1;
        if (depth_to_go == 1) {
            static_eval = eval->evaluate(b);
        }
        int best_index = -1;
        int move_index = -1;
        while (move_iter->has_more_moves() && ((first && !initialized_null_move_eval) || !is_quiescent || move_iter->next_gives_check_or_capture())) {
            move_index++;
            int subtree_score;
            bool checked_futility = false;
            move_t move = move_iter->next_move();

            // don't try quiescent moves that aren't check, promotion, capture a lesser value piece, and are beyond LIMITED_QUIESCENCE depth
            unsigned char sourcerank, sourcefile;
            get_source(move, sourcerank, sourcefile);
            if (is_quiescent
                && !(move & GIVES_CHECK)
                && (get_promotion(move) == 0)
                && (depth - max_depth > std::min(quiescent_depth, LIMITED_QUIESCENT_DEPTH))
                && PIECE_VALUE[b.get_piece(sourcerank, sourcefile)] > PIECE_VALUE[get_captured_piece(move)]) {
                break;
            }

            int child_static_eval = VERY_BAD - 1;
            if (depth_to_go <= 1) {
                child_static_eval = eval->delta_evaluate(b, move, static_eval);
            }
            uint64_t start_nodecount = 0;
            uint64_t start_qnodecount = 0;
            move_t subresponse;
            if (!use_quiescent_search || depth + 1 >= max_depth + quiescent_depth) {
                subtree_score = child_static_eval;
                submove = 0;
            } else {
                b.apply_move(move);
                move_t killer_move = 0;

                if (use_killer_move && submove != 0) {
                    killer_move = submove;
                }

                std::string subline = line;
                if (search_debug > 0) {
                    if (subline.size() > 0) {
                        subline.append(" ");
                    }
                    subline.append(move_to_uci(move));
                }

                start_nodecount = nodecount;
                start_qnodecount = qnodecount;
                std::tuple<move_t, move_t, int> child;
                if (use_pv && alpha < beta) {
                    child = negamax_with_memory(b, depth + 1, -alpha, -alpha, killer_move, child_static_eval, subline);
                    if (-std::get<2>(child) > alpha) {
                        child = negamax_with_memory(b, depth + 1, -beta, -alpha - 1, killer_move, child_static_eval, subline);
                    }
                } else {
                    child = negamax_with_memory(b, depth + 1, -beta, -alpha, killer_move, child_static_eval, subline);
                }

                subtree_score = -std::get<2>(child);
                subresponse = std::get<1>(child);
                submove = std::get<0>(child);
                b.undo_move(move);

            }
            uint64_t elapsed_nodes = nodecount - start_nodecount;
            uint64_t elapsed_qnodes = qnodecount - start_qnodecount;

            if (search_debug >= depth + 1) {
                for (int i = 0; i < depth * 2; i++) {
                    std::cout << " ";
                }
                std::cout << (depth / 2 + 1) << ".";
                if (depth % 2 == 1) {
                    std::cout << "..";
                }
                print_move_uci(move, std::cout) << " -> ";
                print_move_uci(submove, std::cout) << " = " << subtree_score << " (" << alpha << ", " << beta << ") ";
                if (is_quiescent) {
                    std::cout << "Q ";
                } else {
                    std::cout << "d=" << (max_depth - depth) << " ";

                }
                if (!first) {
                    std::cout << " best was ";
                    print_move_uci(best_move, std::cout) << " = " << best_score;
                }
                if (subresponse == 0 && submove != 0) {
                    std::cout << "T";
                }
                std::cout << " (depth=" << depth << " nodes=" << elapsed_nodes << " qnodes=" << elapsed_qnodes << ") (line=" << line << ")";
            }

            piece_t actor = (b.get_piece(get_source_pos(move)) & PIECE_MASK);
            if (subtree_score < alpha) {
                if (search_debug >= depth + 1) {
                    std::cout << "~";
                }
                int clamped_bonus = std::max(-(depth_to_go + 1) * (depth_to_go + 1), -MAX_HISTORY);
                assert(actor > 0 && actor <= bb_king);
                history_bonus[b.get_side_to_play()][actor-1][get_dest_pos(move)] //-= depth;
                   += clamped_bonus - history_bonus[b.get_side_to_play()][actor-1][get_dest_pos(move)] * clamped_bonus / MAX_HISTORY;
            }

            if ((!is_quiescent && first) || subtree_score > best_score) {
                best_score = subtree_score;
                best_move = move;
                best_response = submove;
                best_index = move_index;

                if (use_pruning) {
                    if (best_score > alpha) {
                        alpha = best_score;
                    }
                    if (alpha > beta) {
                        // prune this branch
                        if (search_debug >= depth + 1) {
                            std::cout << "!" << std::endl;
                        }

                        int clamped_bonus = std::min((depth_to_go + 1) * (depth_to_go + 1) * 5, MAX_HISTORY);
                        assert(actor > 0 && actor <= bb_king);
                        history_bonus[b.get_side_to_play()][actor-1][get_dest_pos(move)]
                           += clamped_bonus - history_bonus[b.get_side_to_play()][actor-1][get_dest_pos(move)] * clamped_bonus / MAX_HISTORY;
                        // pruned = true;
                        break;
                    }
/*
                    if ((move & GIVES_CHECK) == 0 && get_captured_piece(move) == 0 && !checked_futility && beta < 9000 && alpha > -9000) {
                        checked_futility = true;
                        if (!initialized_null_move_eval) {
                            null_move_eval = eval->evaluate(b);
                            initialized_null_move_eval = true;
                        }
                    }
                    */

                }


                if (search_debug >= depth + 1) {
                    std::cout << "*";
                }
            }
            first = false;
            if (search_debug >= depth + 1) {
                std::cout << std::endl;
            }
        }
        if (best_index >= 0) {
            nth_sort_freq[std::min(best_index, NTH_SORT_FREQ_BUCKETS -1)] += 1;
            move_counts[std::min(move_index, NTH_SORT_FREQ_BUCKETS -1)] += 1;
        }
    }

    if (search_debug >= depth + 1) {
        std::cout << "depth=" << depth << " move=";
        print_move_uci(best_move, std::cout) << " score=" << best_score;
        if (hint) {
            std::cout << "(hint=";
            print_move_uci(hint, std::cout);
            if (tt_hint) {
                std::cout << "t";
            }
            std::cout << ") ";
        }

        std::cout << std::endl;
    }

    // write to transposition table

    if (use_transposition_table) {
       write_transposition(b.get_hash(), best_move, best_score, max_depth - depth, original_alpha, original_beta);
    }

    return std::tuple<move_t, move_t, int>(best_move, best_response, best_score);
}

void Search::write_transposition(uint64_t board_hash, move_t move, int best_score, int depth, int original_alpha, int original_beta)
{
    unsigned char tt_type;
    if (best_score <= original_alpha) {
        tt_type = TT_UPPER;
    } else if (best_score >= original_beta) {
        tt_type = TT_LOWER;
    } else {
        tt_type = TT_EXACT;
    }
    if (depth < 0) {
        depth = 0;
    }
    // std::cout << "write " << board_hash << " " << tt_type << " " << best_score << " " << depth << std::endl;
    insert_tt_entry(board_hash, move, best_score, depth, tt_type);
}

bool Search::read_transposition(uint64_t board_hash, move_t &tt_move, int depth, int &alpha, int &beta, int &exact_value)
{
    transposition_checks += 1;

    int16_t tt_value = 0;
    unsigned char tt_type, tt_depth;

    if (board_hash == tt_hash_debug) {
        std::cout << "Reading tt entry for " << board_hash << " depth=" << (int) depth << " alpha=" << alpha << " beta=" << beta << std::endl;
    }

    if (fetch_tt_entry(board_hash, tt_move, tt_value, tt_depth, tt_type)) {
        // std::cout << "found " << board_hash << " " << tt_type << " " << tt_value << " " tt_depth << std::endl;
        if (tt_depth >= depth) {
            // found something at appropriate depth
            int mate_depth_adjustment = 0;
            if ((tt_depth > depth) && (tt_value > 9900 || tt_value < -9900)) {
                // special adjustment for transposing into mate sequences -- need to change distance
                mate_depth_adjustment = (tt_depth - depth);
            }
            int original_alpha = alpha;
            int original_beta = beta;
            if (tt_type == TT_EXACT) {
                transposition_full_hits++;
                beta = tt_value - mate_depth_adjustment;
                alpha = beta;
            } else if (tt_type == TT_LOWER) {
                transposition_partial_hits++;
                if (tt_value <= 9900) {
                    mate_depth_adjustment = 0;
                }
                alpha = std::max(alpha, tt_value - mate_depth_adjustment);
            } else if (tt_type == TT_UPPER) {
                transposition_partial_hits++;
                if (tt_value >= -9900) {
                    mate_depth_adjustment = 0;
                }
                beta = std::min(beta, tt_value + mate_depth_adjustment);
            }
            if (alpha >= beta) {
                exact_value = tt_value > 0 ? tt_value - mate_depth_adjustment : tt_value + mate_depth_adjustment;
            } else {
                exact_value = tt_value;
            }
            if (board_hash == tt_hash_debug) {
                std::cout << "Accept: adjusted bounds from (" << original_alpha << ", " << original_beta << ") to (" << alpha << ", " << beta << ")" << std::endl;
            }
            return true;
        } else {
            transposition_insufficient_depth++;
            if (board_hash == tt_hash_debug) {
                std::cout << "Reject: insufficient depth (needed " << depth << " have " << (int)tt_depth << ")" << std::endl;
            }

        }
    }
    return false;

}
const int piece_points[] = { 0, 1, 3, 3, 5, 9, 10000 };

const int centralization[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 0,
    0, 1, 2, 2, 2, 2, 1, 0,
    0, 1, 2, 3, 3, 2, 1, 0,
    0, 1, 2, 3, 3, 2, 1, 0,
    0, 1, 2, 2, 2, 2, 1, 0,
    0, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

int MoveSorter::get_score(const Fenboard *b, int current_score, move_t move) const
{
    move_t ignore;
    int16_t tt_value;
    unsigned char tt_depth, tt_type;

    if (s != NULL && s->fetch_tt_entry(b->get_zobrist_with_move(move), ignore, tt_value, tt_depth, tt_type)) {
        if (tt_type == TT_EXACT) {
            return tt_value + 1000;
        }
    }

    piece_t capture = get_captured_piece(move) & PIECE_MASK;
    unsigned char src_rank, src_file, dest_rank, dest_file;
    get_source(move, src_rank, src_file);
    get_dest(move, dest_rank, dest_file);
    piece_t actor = b->get_piece(src_rank, src_file) & PIECE_MASK;
    piece_t promo = get_promotion(move);

    int src_sq = (src_rank * 8 + src_file);
    int dest_sq = (dest_rank * 8 + dest_file);
    int invalidate_castle_penalty = 0;
    if (actor == bb_king) {
        if (dest_file - src_file == 2 || dest_file - src_file == -2) {
            invalidate_castle_penalty += 3;
        }
        if (get_invalidates_kingside_castle(move)) {
            invalidate_castle_penalty--;
        }
        if (get_invalidates_queenside_castle(move)) {
            invalidate_castle_penalty--;
        }

        int score = piece_points[capture] * 64 + invalidate_castle_penalty;
        int central = centralization[dest_sq] - centralization[src_sq];
        // want best score first
        return current_score + score * 10 + central;
    } else {
        int king_square = get_low_bit(b->piece_bitmasks[side_to_play * (bb_king + 1) + bb_king], 0);
        int source_square = get_source_pos(move);
        int dest_square = get_dest_pos(move);
        int score = 0;

        if (actor >= bb_knight && actor <= bb_queen) {
            if (covered_squares_q == ~0) {
                Color opponent = get_opposite_color(b->get_side_to_play());
                covered_squares_bn = b->computed_covered_squares(opponent, INCLUDE_PAWN);
                covered_squares_r = covered_squares_bn | b->computed_covered_squares(opponent, INCLUDE_BISHOP | INCLUDE_KNIGHT);
                covered_squares_q = covered_squares_r | b->computed_covered_squares(opponent, INCLUDE_ROOK);
            }
            uint64_t coverer = 0;
            switch(actor) {
                case bb_queen: coverer = covered_squares_q; break;
                case bb_rook: coverer = covered_squares_r; break;
                case bb_knight: case bb_bishop: coverer = covered_squares_bn; break;
            }
            // move piece out of attack
            if (((1ULL << source_square) & coverer) > 0) {
               score += piece_points[actor] * 100;
            }
            // disprefer giving the piece away
            if (((1ULL << dest_square) & coverer) > 0) {
               score -= piece_points[actor] * 100;
            }
        }

        if (side_to_play == Black) {
            king_square = vertical_mirror(king_square);
            source_square = vertical_mirror(source_square);
            dest_square = vertical_mirror(dest_square);
        }

        int dense_index_actor = king_square * (64 * 10) + (actor - 1) * 64 + source_square;
        int dense_index_dest = king_square * (64 * 10) + (actor - 1) * 64 + dest_square;

        score -= psqt_weights[dense_index_actor];
        score += psqt_weights[dense_index_dest];
        if (capture > 0) {
            score -= psqt_weights[king_square * (64 * 10) + (capture - 1 + 5) * 64 + dest_square];
        }
        if (promo > 0) {
            score += psqt_weights[king_square * (64 * 10) + (promo - 1) * 64 + dest_square];
        }
        int point_score = current_score + score;
        if (s != NULL && !(move & GIVES_CHECK) && capture == 0) {
            assert(actor > 0 && actor <= bb_king);
            return point_score + s->history_bonus[b->get_side_to_play()][actor - 1][dest_square] / HISTORY_SCALER;
        } else {
            return point_score;
        }
    }
}

struct MoveCmp {
    MoveCmp(const std::map<move_t, int> *scores)
        : scores(scores)
    {}
    bool operator()(move_t a, move_t b) const {
        // sort scores in descending order
        return scores->at(a) > scores->at(b);
    }
    const std::map<move_t, int> *scores;
};


MoveSorter::MoveSorter()
{
    buffer.reserve(32);
    index = 0;
}

void MoveSorter::load_more(const Fenboard *b) {
    while (buffer.size() <= index && phase <= P_NOCHECK_NO_CAPTURE){
        switch(phase) {

            case P_HINT:
                if (transposition_hint != 0) {
                    buffer.push_back(transposition_hint);
                }
                if (s != NULL) {
                    s->moves_commenced += 1;
                }
                break;

            case P_HINT_REINT:
                if (hint != 0) {
                    move_t move = b->reinterpret_move(hint, opp_covered_squares);
                    if (move != 0) {
                        buffer.push_back(move);
                    }
                    hint = move;
                }
                break;
            case P_CHECK_CAPTURE:
                b->get_packed_legal_moves(b->get_side_to_play(), move_iter, opp_covered_squares);
                if (s != NULL) {
                    s->moves_expanded += 1;
                }
                /* fall through */
            default:
                if (phase == P_NOCHECK_NO_CAPTURE && captures_checks_only && buffer.size() >= 1) {
                    // skip quiet moves
                    break;
                }
                int start = buffer.size();
                b->get_moves(b->get_side_to_play(),
                    phase == P_CHECK_CAPTURE || phase == P_CHECK_NOCAPTURE,
                    phase == P_CHECK_CAPTURE || phase == P_NOCHECK_CAPTURE,
                    move_iter,
                    buffer);
                if (transposition_hint != 0) {
                    auto location = std::find(buffer.begin() + start, buffer.end(), transposition_hint);
                    if (location != buffer.end()) {
                        buffer.erase(location);
                    }
                }
                if (hint != 0) {
                    auto location = std::find(buffer.begin() + start, buffer.end(), hint);
                    if (location != buffer.end()) {
                        buffer.erase(location);
                    }
                }
                if (do_sort) {
                    std::map<move_t, int> move_scores;
                    for (auto iter = buffer.begin() + start; iter != buffer.end(); iter++) {
                        move_scores[*iter] = get_score(b, current_score, *iter);
                    }
                    std::sort(buffer.begin() + start, buffer.end(), MoveCmp(&move_scores));
                }
                break;
        }
        phase++;
    }
}
void MoveSorter::reset(const Fenboard *b, Search *s, bool captures_checks_only, int depth, int alpha, int beta, bool do_sort, int score, move_t hint, move_t transposition_hint, bool verbose)
{
    index = 0;
    last_capture = 0;
    opp_covered_squares = 0;
    buffer.clear();
    move_iter.reset();
    this->side_to_play = b->get_side_to_play();
    this->do_sort = do_sort;
    this->captures_checks_only = captures_checks_only;
    this->hint = hint;
    this->transposition_hint = transposition_hint;
    this->s = s;
    this->b = b;
    this->verbose = verbose;
    if (s != NULL) {
        current_score = score;
    } else {
        current_score = 0;
    }
    phase = P_HINT;
}


bool MoveSorter::has_more_moves()
{
    if (index == buffer.size()) {
        load_more(b);
    }
    return index < buffer.size();
}

move_t MoveSorter::next_move()
{
    return buffer[index++];
}
