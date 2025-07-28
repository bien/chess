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
        move_t new_result = mtdf(b, color, new_score, guess, mtdf_deadline, result);
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

Search::Search(Evaluation *eval, int transposition_table_size)
    : score(0), nodecount(0), transposition_table_size(transposition_table_size), use_transposition_table(true),
        use_pruning(true), eval(eval), min_score_prune_sorting(2), use_mtdf(true), use_iterative_deepening(true),
        use_quiescent_search(false), quiescent_depth(2), use_killer_move(true), time_available(0), soft_deadline(true)

{
    srandom(clock());
    transposition_checks = 0;
    transposition_partial_hits = 0;
    transposition_full_hits = 0;
    transposition_table = new uint64_t[transposition_table_size];
    for (int i = 0; i < transposition_table_size; i++) {
        transposition_table[i] = 0;
    }
    for (int i = 0; i < max_depth; i++) {
        move_sorter_pool.push_back(new MoveSorter());
    }
}

void Search::reset()
{
    score = 0;
    nodecount = 0;
    for (int i = 0; i < transposition_table_size; i++) {
        transposition_table[i] = 0;
    }
}

move_t Search::mtdf(Fenboard &b, Color color, int &score, int guess, time_t deadline, move_t move)
{
    score = guess;
    int upperbound = SCORE_MAX;
    int lowerbound = SCORE_MIN;
    if (search_debug) {
        std::cout << "mtdf guess=" << guess << " depth=" << max_depth << std::endl;
    }
    const int window_size = 10;
    bool last_search = false;

    do {
        int beta;
        if (deadline > 0 && time(NULL) > deadline) {
            return 0;
        }
        int alpha = std::max(score - window_size, lowerbound);
        beta = std::min(upperbound, alpha + window_size);

        // if we know there's a checkmate then don't bother being cute
        if (beta < -9900) {
            alpha = SCORE_MIN;
            last_search = true;
        }
        else if (alpha > 9900) {
            beta = SCORE_MAX;
            last_search = true;
        }
        std::tuple<move_t, move_t, int> result = alphabeta_with_memory(b, 0, color, alpha, beta, move);
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
            std::cout << "  mtdf a=" << beta - 1 << " b=" << beta << " score=" << score << " lower=" << lowerbound << " upper=" << upperbound << " move=";
            b.print_move(move, std::cout);
            std::cout << std::endl;
        }
    } while (lowerbound < upperbound && !last_search);

    return move;
}

// principal move, principal reply, cp score
std::tuple<move_t, move_t, int> Search::alphabeta_with_memory(Fenboard &b, int depth, Color color, int alpha, int beta, move_t hint)
{
    nodecount++;

    int best_score = 31415;
    move_t best_response = -1;
    move_t best_move = -1;
    int original_alpha = alpha;
    int original_beta = beta;

    int is_white = color == White ? 1 : -1;

    // check for endgames
    int endgame_eval;
    if (eval->endgame(b, endgame_eval)) {
        return std::tuple<move_t, move_t, int>(-1, -1, endgame_eval);
    }

    // check for repetition
    if (b.times_seen() >= 3) {
        return std::tuple<move_t, move_t, int>(-1, -1, 0);
    }

    // check transposition table
    if (use_transposition_table) {
        transposition_checks += 1;

        move_t tt_move;
        int16_t tt_value;
        unsigned char tt_type, tt_depth;

        if (fetch_tt_entry(b.get_hash(), tt_move, tt_value, tt_depth, tt_type)) {
            if (tt_depth >= max_depth - depth) {
                // found something at appropriate depth
                int mate_depth_adjustment = 0;
                if ((tt_depth > max_depth - depth) && (tt_value > 9900 || tt_value < -9900)) {
                    // special adjustment for transposing into mate sequences -- need to change distance
                    mate_depth_adjustment = (tt_depth - max_depth + depth);
                }
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
                    return std::tuple<move_t, move_t, int>(tt_move, 0, tt_value > 0 ? tt_value - mate_depth_adjustment : tt_value + mate_depth_adjustment);
                }
            } else {
                transposition_insufficient_depth++;
            }
        }
    }

    if ((!use_quiescent_search && depth == max_depth) || (use_quiescent_search && depth == max_depth + quiescent_depth)) {
        best_score = eval->evaluate(b);
    } else {
        MoveSorter *move_iter = get_move_sorter(depth);
        move_iter->reset(&b, color, max_depth - depth > 2, hint);

        if (!move_iter->has_more_moves()) {
            release_move_sorter(move_iter);
            if (b.king_in_check(color)) {
                return std::tuple<move_t, move_t, int>(0, 0, is_white * (VERY_BAD + depth));
            } else {
                return std::tuple<move_t, move_t, int>(0, 0, 0);
            }
        }


        bool first = true;


        int currentnodescore;
        bool evaluated_nodescore = false;
        bool quiescent = true;
        // int bestindex = -1;
        int first_quiescent = -1;
        // bool pruned = false;
        while (move_iter->has_more_moves()) {
            int subtree_score;
//            move_t subresponse = 0;
            move_t submove = 0;
            move_t move = move_iter->next_move();
            bool cutoff;

            quiescent = get_captured_piece(move, color) == EMPTY && (move & GIVES_CHECK);
            if (quiescent && first_quiescent == -1) {
                first_quiescent = move_iter->index;
            }

            if (use_quiescent_search && !quiescent) {
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
                move_t killer_move = 0;

                if (use_killer_move && submove != 0) {
                    killer_move = submove;
                }

                std::tuple<move_t, move_t, int> child = alphabeta_with_memory(b, depth + 1, get_opposite_color(color), alpha, beta, killer_move);
                subtree_score = std::get<2>(child);
                submove = std::get<0>(child);
//                subresponse = std::get<1>(child);
                b.undo_move(move);
            }
            if (search_debug >= depth + 1) {
                for (int i = 0; i < depth * 2; i++) {
                    std::cout << " ";
                }
                std::cout << (depth / 2 + 1) << ".";
                if (depth % 2 == 1) {
                    std::cout << "..";
                }
                print_move_uci(move, std::cout) << " -> ";
                print_move_uci(submove, std::cout) << " = " << subtree_score;
                if (cutoff && !quiescent) {
                    std::cout << "Q";
                }
                if (!first) {
                    std::cout << " best was ";
                    print_move_uci(best_move, std::cout) << " = " << best_score;
                }
            }

            if (first || (color == White && subtree_score > best_score) || (color == Black && subtree_score < best_score)) {
                best_score = subtree_score;
                best_move = move;
                best_response = submove;
                // bestindex = move_iter->index;
//                tie_count = 1;

                if (use_pruning) {
                    if (color == White) {
                        alpha = std::max(alpha, best_score);
                    }
                    else {
                        beta = std::min(beta, best_score);
                    }
                    if (alpha > beta) {
                        // prune this branch
                        if (search_debug >= depth + 1) {
                            std::cout << "!" << std::endl;
                        }
                        // pruned = true;
                        break;
                    }
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
        release_move_sorter(move_iter);
//        std::cout << "M " << bestindex << "/" << iter.index << "/" << first_quiescent << "/" << (pruned ? "P" : "") << std::endl;
    }

    if (search_debug >= depth + 1) {
        std::cout << "depth=" << depth << " move=";
        print_move_uci(best_move, std::cout) << " score=" << best_score << std::endl;
    }


    // write to transposition table
    if (use_transposition_table) {
        unsigned char tt_type;
        if (best_score <= original_alpha) {
            tt_type = TT_UPPER;
        } else if (best_score >= original_beta) {
            tt_type = TT_LOWER;
        } else {
            tt_type = TT_EXACT;
        }
        insert_tt_entry(b.get_hash(), best_move, best_score, max_depth - depth, tt_type);

    }
    return std::tuple<move_t, move_t, int>(best_move, best_response, best_score);
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

int MoveSorter::get_score(move_t move) const
{
    piece_t capture = get_captured_piece(move) & PIECE_MASK;
    unsigned char src_rank, src_file, dest_rank, dest_file;
    get_source(move, src_rank, src_file);
    get_dest(move, dest_rank, dest_file);
    piece_t actor = b->get_piece(src_rank, src_file) & PIECE_MASK;
    piece_t promo = get_promotion(move);

    int src_sq = (src_rank * 8 + src_file);
    int dest_sq = (dest_rank * 8 + dest_file);
    /*
    uint64_t attacked = opp_covered & ~stp_covered;

    bool src_attacked = (1ULL << src_sq) & attacked;
    bool dest_attacked = (1ULL << dest_sq) & attacked;
*/
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
    }

    int score = (capture != 0 ? piece_points[capture] + piece_points[promo] - piece_points[actor]: 0) + invalidate_castle_penalty;
    int central = centralization[dest_sq] - centralization[src_sq];
    // want best score first
    return score * 10 + central;

}


struct MoveCmp {
    MoveCmp(const MoveSorter *ms)
        : ms(ms)
    {}
    bool operator()(move_t a, move_t b) const {
        // sort from most delta points to least delta points
        return ms->get_score(a) > ms->get_score(b);
    }
    const MoveSorter *ms;
};


MoveSorter::MoveSorter()
{
    buffer.reserve(32);
    index = 0;
    phase = 0;
}

void MoveSorter::reset(const Fenboard *b, Color side_to_play, bool do_sort, move_t hint)
{
    buffer.clear();
    move_iter.reset();
    this->side_to_play = side_to_play;
    this->do_sort = do_sort;
    this->hint = hint;
    this->b = b;
    index = 0;
    phase = 0;
    MoveCmp move_cmp(this);
    b->get_packed_legal_moves(side_to_play, move_iter);


    // checks captures
    b->get_moves(side_to_play, true, true, move_iter, buffer);
    if (do_sort) {
        std::sort(buffer.begin(), buffer.end(), move_cmp);
    }
    last_check = buffer.size() - 1;
}


bool MoveSorter::next_gives_check() const
{
    return index <= last_check;
}

bool MoveSorter::has_more_moves()
{
    MoveCmp move_cmp(this);
    if (index == buffer.size()) {
        while (phase < 3) {
            phase++;
            int start = buffer.size();
            if (phase == 1) {
                // checks non captures
                b->get_moves(side_to_play, true, false, move_iter, buffer);
                last_check = buffer.size() - 1;
            }
            if (phase == 2) {
                // other captures
                b->get_moves(side_to_play, false, true, move_iter, buffer);

                if (do_sort) {
                    std::sort(buffer.begin() + start, buffer.end(), move_cmp);
                }
            }
            if (phase == 3) {
                // non capture non checks
                b->get_moves(side_to_play, false, false, move_iter, buffer);
                if (do_sort) {
                    std::sort(buffer.begin() + start, buffer.end(), move_cmp);
                }
                if (hint != 0) {
                    auto match = std::find_if(buffer.begin() + start, buffer.end(), [hint = this->hint] (move_t m) { return (m & 0xfff) == hint; } );
                    if (match != buffer.end()) {
                        std::iter_swap(buffer.begin() + start, match);
                    }
                }
            }
        }
    }

    return index < buffer.size();
}

move_t MoveSorter::next_move()
{
    return buffer[index++];
}
