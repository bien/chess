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
int search_features = 0;
const int LIMITED_QUIESCENT_DEPTH = 4;
// const int MAX_HISTORY = 10000;
// const int HISTORY_SCALER = 500;

static int vertical_mirror(int square) {
    return square ^ 56;
}

move_t Search::minimax(Fenboard &b)
{
    nodecount = 0;
    bool old_pruning = use_pruning;
    bool old_mtdf = use_mtdf;
    use_pruning = false;
    use_mtdf = false;
    std::vector<move_t> line;
    std::tuple<move_t, move_t, int> result = negamax_with_memory(b, 0, SCORE_MIN, SCORE_MAX, line);
    score = std::get<2>(result);
    if (b.get_side_to_play() == Black) {
        score = -score;
    }
    use_pruning = old_pruning;
    use_mtdf = old_mtdf;
    return std::get<0>(result);
}

move_t Search::alphabeta(Fenboard &b, SearchUpdate *s)
{
    nodecount = 0;
    move_t result = 0;

    if (use_iterative_deepening) {
        int old_max_depth = max_depth;
        int guess_score = 0;
        if (millis_available > 0) {
            deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(millis_available);
        }

        try {
            for (int iter_depth = (old_max_depth % 2 == 1 ? 1 : 0); iter_depth <= old_max_depth; iter_depth += 2) {
                max_depth = iter_depth;
                if (use_mtdf) {
                    result = mtdf(b, score, guess_score, 0, result);
                } else if (use_pv) {
                    std::tuple<move_t, move_t, int> sub;
                    std::vector<move_t> line;
                    int alpha = guess_score - 25;
                    int beta = guess_score + 25;
                    int backoff = 200;
                    while (true) {
                        sub = negamax_with_memory(b, 0, alpha, beta, line, result);
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
                if (s != NULL) {
                    (*s)(result, max_depth, nodecount, score);
                }
                guess_score = score;
                if (millis_available > 0 && std::chrono::system_clock::now() > deadline) {
                    break;
                }
            }
        }
        catch (const std::exception &e) {
            std::cout << "Search interrupted: " << e.what() << std::endl;
        }
        max_depth = old_max_depth;
    } else {
        std::tuple<move_t, move_t, int> sub;
        std::vector<move_t> line;
        sub = negamax_with_memory(b, 0, SCORE_MIN, SCORE_MAX, line);
        score = std::get<2>(sub);
        if (b.get_side_to_play() == Black){
            score = -score;
        }
        result = std::get<0>(sub);
    }
    return result;
}

Search::Search(Evaluation *eval, int transposition_table_size_log2)
    : score(0), nodecount(0), qnodecount(0), transposition_table_size_log2(transposition_table_size_log2), use_transposition_table(true),
        use_pruning(true), eval(eval), min_score_prune_sorting(2), use_mtdf(false), use_pv(true), use_iterative_deepening(true),
        use_quiescent_search(true), use_killer_move(true), mtdf_window_size(10), quiescent_depth(2), millis_available(0), max_depth(8), soft_deadline(true)

{
    srandom(clock());
    transposition_checks = 0;
    transposition_partial_hits = 0;
    transposition_full_hits = 0;
    transposition_insufficient_depth = 0;
    transposition_conflicts = 0;
    recapture_first_bonus = 0;
    moves_expanded = 0;
    moves_commenced = 0;
    handeval_coeff = 0;
    psqt_coeff = 1;
    exchange_coeff = 1;
    history_coeff = 2;
    hint_coeff = 0;
    quiescent_positive_capture_only = false;
    quiescent_single_capture_square_only = false;

    transposition_table = new uint64_t[1ULL<<transposition_table_size_log2];
    reset();
    for (int i = 0; i < max_depth; i++) {
        move_sorter_pool.push_back(new MoveSorter());
    }
    for (int i = 0; i < NTH_SORT_FREQ_BUCKETS; i++) {
        nth_sort_freq[i] = 0;
    }
}

void Search::reset()
{
    reset_counters();
    memset(transposition_table, 0, sizeof(uint64_t) * (1ULL<<transposition_table_size_log2));
    memset(&history_bonus2, 0, sizeof(history_bonus2));
    memset(&refutation_table2, 0, sizeof(refutation_table2));
    memset(&followup_table1, 0, sizeof(followup_table1));
    memset(&distant_table1, 0, sizeof(distant_table1));
}

void Search::reset_counters()
{
    score = 0;
    nodecount = 0;
    qnodecount = 0;
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
        std::vector<move_t> line;
        if (b.get_side_to_play() == White) {
            result = negamax_with_memory(b, 0, alpha, beta, line, move);
            score = std::get<2>(result);
        } else {
            result = negamax_with_memory(b, 0, -beta, -alpha, line, move);
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

void Search::history_cutoff(Color side_to_play, int depth_to_go, move_t move, int move_rank, const std::vector<move_t> &line, bool high)
{
    piece_t actor = get_actor(move);
    int bonus1 = (depth_to_go + 1) * (depth_to_go + 1);
    int bonus2 = bonus1;
    assert(actor > 0 && actor <= bb_king);
    if (high) {
        bonus1 = bonus1 * move_rank;
        bonus2 = bonus2 * (move_rank * move_rank);
    } else {
        bonus1 = -bonus1 * std::max(1, 3 - move_rank);
        bonus2 = -bonus2 * (std::max(1, 3 - move_rank) * std::max(1, 3 - move_rank));
    }
    history_bonus2[side_to_play][actor-1][get_dest_pos(move)] += bonus2;

    if (!line.empty()) {
        piece_t counter_actor = get_actor(line.back());
        assert(counter_actor != 0);
        refutation_table2[actor-1][get_dest_pos(move)][counter_actor-1][get_dest_pos(line.back())] += bonus2;
    }
    if (line.size() >= 2) {
        move_t past_move = line[line.size() - 2];
        piece_t counter_actor = get_actor(past_move);
        assert(counter_actor != 0);
        followup_table1[actor-1][get_dest_pos(move)][counter_actor-1][get_dest_pos(past_move)] += bonus1;
    }
    for (int distance = 3; distance <= line.size(); distance += 2) {
        move_t past_move = line[line.size() - distance];
        piece_t counter_actor = get_actor(past_move);
        assert(counter_actor != 0);
        distant_table1[actor-1][get_dest_pos(move)][counter_actor-1][get_dest_pos(past_move)] += bonus1;
    }
}

void emit_sort_feature(const Fenboard &b, move_t move, int alpha, int beta, int score, const std::string &result, int score_parts[score_part_len]) {
    if (search_features && alpha < beta) {
        std::cout << result << " " << b.get_hash() << " " << alpha << " " << beta << " " << score << " " << move_to_uci(move) << " " << ((move & GIVES_CHECK) != 0 ? "1 " : "0 ") << (get_captured_piece(move) != 0 ? "1 " : "0 ") << (int)get_actor(move) << " ";
        for (int i = 0; i < score_part_len; i++) {
            std::cout << score_parts[i] << " ";
        }
        std::cout << std::endl;
    }
}

struct Counter {
    std::map<move_t, int> counts;
    void add(move_t value) {
        counts[value] += 1;
    }
    move_t maximum() {
        move_t best_move = 0;
        int best_count = -1;
        for (const auto &pair : counts) {
            if (pair.second > best_count) {
                best_count = pair.second;
                best_move = pair.first;
            }
        }
        return best_move;
    }
};

// principal move, principal reply, cp score
std::tuple<move_t, move_t, int> Search::negamax_with_memory(Fenboard &b, int depth, int alpha, int beta, std::vector<move_t> &line, move_t hint, int static_score)
{
    if (depth < 3 && millis_available > 0 && !soft_deadline && std::chrono::system_clock::now() > deadline) {
        throw std::runtime_error("Time limit exceeded");
    }

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
        move_iter->reset(&b, this, line, is_quiescent, std::max(0, max_depth - depth), alpha, beta, depth_to_go > 2, hint, tt_hint);

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
        int quiescent_depth_so_far = depth - max_depth;
        if (is_quiescent) {
            // null move isn't necessarily valid if we're in check
            if (!b.king_in_check(b.get_side_to_play()) || quiescent_depth_so_far > LIMITED_QUIESCENT_DEPTH) {
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
        Counter killer_move_counter;
        while (move_iter->has_more_moves() && ((first && !initialized_null_move_eval) || !is_quiescent || move_iter->next_gives_check_or_capture())) {
            move_index++;
            int subtree_score;
            // bool checked_futility = false;
            move_t move = move_iter->next_move();
            int score_parts[score_part_len];
            move_iter->get_score_parts(&b, move, line, score_parts);

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
            if (depth + 1 >= max_depth + quiescent_depth) {
                subtree_score = child_static_eval;
                submove = 0;
            } else {
                b.apply_move(move);

                if (use_killer_move && submove != 0) {
                    killer_move_counter.add(submove);
                }

                start_nodecount = nodecount;
                start_qnodecount = qnodecount;
                std::tuple<move_t, move_t, int> child;
                line.push_back(move);
                move_t killer_move = killer_move_counter.maximum();
                if (use_pv && alpha < beta) {
                    child = negamax_with_memory(b, depth + 1, -alpha, -alpha, line, killer_move, child_static_eval);
                    if (-std::get<2>(child) > alpha) {
                        child = negamax_with_memory(b, depth + 1, -beta, -alpha - 1, line, killer_move, child_static_eval);
                    }
                } else {
                    child = negamax_with_memory(b, depth + 1, -beta, -alpha, line, killer_move, child_static_eval);
                }
                line.pop_back();

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
                std::cout << " (depth=" << depth << " nodes=" << elapsed_nodes << " qnodes=" << elapsed_qnodes << ")";
            }

            if (subtree_score < alpha) {
                if (search_debug >= depth + 1) {
                    std::cout << "~";
                }
                emit_sort_feature(b, move, alpha, original_beta, subtree_score, "faillow", score_parts);
                history_cutoff(b.get_side_to_play(), depth_to_go, move, move_index, line, false);
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
                        emit_sort_feature(b, move, original_alpha, original_beta, best_score, "failhigh", score_parts);
                        history_cutoff(b.get_side_to_play(), depth_to_go, move, move_index, line, true);
                        // pruned = true;
                        break;
                    } else {
                        emit_sort_feature(b, move, original_alpha, original_beta, best_score, "mid", score_parts);
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


void MoveSorter::get_score_parts(const Fenboard *b, move_t move, const std::vector<move_t> &line, int parts[score_part_len]) const
{
    move_t ignore;
    int16_t tt_value;
    unsigned char tt_depth, tt_type;

    memset(parts, 0, sizeof(int) * score_part_len);

    if (s != NULL && s->fetch_tt_entry(b->get_zobrist_with_move(move), ignore, tt_value, tt_depth, tt_type)) {
        if (tt_type == TT_EXACT) {
            parts[score_part_trans] = tt_value + 1000;
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

    int psqt_king_square = -1;
    int psqt_dest_square = -1;

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

        int central = centralization[dest_sq] - centralization[src_sq];
        // want best score first
        parts[score_part_king_handeval] = invalidate_castle_penalty * 50 + central;
    } else {
        psqt_king_square = get_low_bit(b->piece_bitmasks[side_to_play * (bb_king + 1) + bb_king], 0);
        int source_square = get_source_pos(move);
        psqt_dest_square = get_dest_pos(move);

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
               parts[score_part_exchange] += piece_points[actor] * 100;
            }
            // disprefer giving the piece away
            if (((1ULL << psqt_dest_square) & coverer) > 0) {
               parts[score_part_exchange] -= piece_points[actor] * 100;
            }
        }

        if (side_to_play == Black) {
            psqt_king_square = vertical_mirror(psqt_king_square);
            source_square = vertical_mirror(source_square);
            psqt_dest_square = vertical_mirror(psqt_dest_square);
        }

        int dense_index_actor = psqt_king_square * (64 * 10) + (actor - 1) * 64 + source_square;
        int dense_index_dest = psqt_king_square * (64 * 10) + (actor - 1) * 64 + psqt_dest_square;

        parts[score_part_psqt] = -psqt_weights[dense_index_actor];
        parts[score_part_psqt] += psqt_weights[dense_index_dest];
        if (capture > 0 /* && score_part_exchange == 0 */) {
            parts[score_part_psqt] -= psqt_weights[psqt_king_square * (64 * 10) + (capture - 1 + 5) * 64 + psqt_dest_square];
        }
        if (promo > 0) {
            parts[score_part_psqt] += psqt_weights[psqt_king_square * (64 * 10) + (promo - 1) * 64 + psqt_dest_square];
        }
    }
    if (actor == bb_king) {
        parts[score_part_exchange] = piece_points[capture] * 100;
    }
    else if (s != NULL) {
        parts[score_part_exchange] = 100 * b->static_exchange_eval(b->get_side_to_play(), dest_sq, capture, actor);
        // std::cout << "SEE capt=" << parts[score_part_exchange];
        // don't double-count captured piece
        if (s->exchange_coeff > 0 && s->psqt_coeff > 0 && capture != 0 && psqt_king_square >= 0 && psqt_dest_square >= 0 && parts[score_part_exchange] == 100 * PIECE_VALUE[capture]) {
            parts[score_part_exchange] += (s->exchange_coeff / s->psqt_coeff) * psqt_weights[psqt_king_square * (64 * 10) + (capture - 1 + 5) * 64 + psqt_dest_square];
//            std::cout << " SEE psqt-adj=" << parts[score_part_exchange];
        }

        if (opp_covered_squares & (1ULL << src_sq)) {
            int null_move_capture = 100 * b->static_exchange_eval(get_opposite_color(b->get_side_to_play()), src_sq, actor, 0);
            if (null_move_capture > 0) {
                parts[score_part_exchange] += null_move_capture;
            }
            // std::cout << " SEE psqt-save=" << parts[score_part_exchange];
       }
        // std::cout << std::endl;
        if (s->recapture_first_bonus != 0 && capture != 0 && recapture_on_sq == dest_sq) {
            parts[score_part_exchange] += s->recapture_first_bonus - piece_points[actor] * 100;
        }
    }

    if (s != NULL) {
        assert(actor > 0 && actor <= bb_king);
        int dest_square = get_dest_pos(move);
        parts[score_part_history2] = s->history_bonus2[b->get_side_to_play()][actor - 1][dest_square];
        if (line.size() > 0) {
            move_t previous_move = line.back();
            int counter_actor = get_actor(previous_move);
            parts[score_part_refutation2] = s->refutation_table2[actor-1][get_dest_pos(move)][counter_actor-1][get_dest_pos(previous_move)];
            if (line.size() >= 2) {
                move_t past_move = line[line.size() - 2];
                piece_t counter_actor = get_actor(past_move);
                assert(counter_actor != 0);
                parts[score_part_followup1] = s->followup_table1[actor-1][get_dest_pos(move)][counter_actor-1][get_dest_pos(past_move)];
            }
            for (int distance = 3; distance <= line.size(); distance += 2) {
                move_t past_move = line[line.size() - distance];
                piece_t counter_actor = get_actor(past_move);
                assert(counter_actor != 0);
                parts[score_part_distant1] = s->distant_table1[actor-1][get_dest_pos(move)][counter_actor-1][get_dest_pos(past_move)];
            }
        }
    }
    parts[score_part_hint] = (move == hint ? 1 : 0);
}

int MoveSorter::get_score(const Fenboard *b, move_t move, const std::vector<move_t> &line) const
{
    move_t ignore;
    int16_t tt_value;
    unsigned char tt_depth, tt_type;

    if (s != NULL && s->fetch_tt_entry(b->get_zobrist_with_move(move), ignore, tt_value, tt_depth, tt_type)) {
        if (tt_type == TT_EXACT) {
            return tt_value + 1000;
        }
    }

    int score_parts[score_part_len];
    get_score_parts(b, move, line, score_parts);
    int value = score_parts[score_part_trans];
    if (s != NULL) {
        value +=
            score_parts[score_part_king_handeval] * s->handeval_coeff +
            score_parts[score_part_psqt] * s->psqt_coeff +
            score_parts[score_part_exchange] * s->exchange_coeff +
            score_parts[score_part_hint] * s->hint_coeff;

        auto history_idx = score_part_history2;
        auto refut_idx = score_part_refutation2;
        auto followup_idx = score_part_followup1;
        auto distant_idx = score_part_distant1;
        int followup_mult = 0, distant_mult = 0;
        int history_value = 0;
        if (score_parts[history_idx] > 0) {
            history_value += log2l(score_parts[history_idx]) * 10;
        } else if (score_parts[history_idx] < 0){
            history_value -= log2l(-score_parts[history_idx]) * 10;
        }
        if (score_parts[refut_idx] > 0) {
            history_value += log2l(score_parts[refut_idx]) * 10;
        } else if (score_parts[refut_idx] < 0){
            history_value -= log2l(-score_parts[refut_idx]) * 10;
        }
        if (score_parts[followup_idx] > 0) {
            history_value += log2l(score_parts[followup_idx]) * followup_mult;
        } else if (score_parts[followup_idx] < 0){
            history_value -= log2l(-score_parts[followup_idx]) * followup_mult;
        }
        if (score_parts[distant_idx] > 0) {
            history_value += log2l(score_parts[distant_idx]) * distant_mult;
        } else if (score_parts[distant_idx] < 0){
            history_value -= log2l(-score_parts[distant_idx]) * distant_mult;
        }
        value += history_value * s->history_coeff;
    }
    return value;
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

                if (opp_covered_squares == 0) {
                    opp_covered_squares = b->computed_covered_squares(get_opposite_color(b->get_side_to_play()), INCLUDE_ALL);
                }

                int start = buffer.size();

                b->get_moves(b->get_side_to_play(),
                    phase == P_CHECK_CAPTURE || phase == P_CHECK_NOCAPTURE,
                    phase == P_CHECK_CAPTURE || phase == P_NOCHECK_CAPTURE,
                    move_iter,
                    buffer);

                if (s != NULL && phase == P_NOCHECK_CAPTURE && captures_checks_only) {
                    // skip moves that don't capture on recapture_on_sq
                    for (auto iter = buffer.begin() + start; iter != buffer.end(); iter++) {
                        bool exclude = false;
                        if (s->quiescent_positive_capture_only && b->static_exchange_eval(b->get_side_to_play(), get_dest_pos(*iter), get_captured_piece(*iter), get_actor(*iter)) < 0) {
                            exclude = true;
                        } else if (s->quiescent_single_capture_square_only && recapture_on_sq != 0 && get_dest_pos(*iter) != recapture_on_sq) {
                            exclude = true;
                        }
                        if (exclude) {
                            buffer.erase(iter);
                            iter--;
                        }
                    }
                }
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
                        move_scores[*iter] = get_score(b, *iter, *line);
                    }
                    std::sort(buffer.begin() + start, buffer.end(), MoveCmp(&move_scores));
                }
                break;
        }
        phase++;
    }
}
void MoveSorter::reset(const Fenboard *b, Search *s, const std::vector<move_t> &line, bool captures_checks_only, int depth, int alpha, int beta, bool do_sort, move_t hint, move_t transposition_hint, char recapture_on_sq, bool verbose)
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
    this->line = &line;
    this->recapture_on_sq = recapture_on_sq;
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
