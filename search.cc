#include "search.hh"
#include <limits.h>
#include <vector>
#include <utility>
#include <algorithm>
#include <iostream>
#include <sstream>

bool search_debug = false;

template<class T>
T max(T a, T b)
{
	return a >= b ? a : b;
}

template<class T>
T min(T a, T b)
{
	return a < b ? a : b;
}

int Evaluation::delta_evaluate(Board &b, move_t move, int previous_score) const
{
	int score;
	b.apply_move(move);
	score = evaluate(b);
	b.undo_move(move);
	return score;
}

move_t Search::minimax(Board &b, int depth, Color color)
{
	nodecount = 0;
	use_pruning = false;
	use_mtdf = false;
	move_t result = alphabeta_with_memory(b, depth, color, score, 0, 0);
	if (color == Black) {
		score = -score;
	}
	return result;
}

move_t Search::alphabeta(Board &b, int depth, Color color)
{
	nodecount = 0;
	move_t result;
	if (use_mtdf) {
		result = mtdf(b, depth, color, score, eval->evaluate(b));
	} else {
		result = alphabeta_with_memory(b, depth, color, score, VERY_BAD, VERY_GOOD);
	}
	return result;
}

Search::Search(const Evaluation *eval, int transposition_table_size)
	: score(0), nodecount(0), transposition_table_size(transposition_table_size), use_transposition_table(true), transposition_table(transposition_table_size), use_pruning(true), eval(eval), min_score_prune_sorting(3), use_mtdf(true)
{}


void Search::reset()
{
	score = 0;
	nodecount = 0;
	transposition_table.clear();
}

struct PrecomputedComparator {
	PrecomputedComparator(std::unordered_map<move_t, int> cmp, bool invert) {
		this->cmp = cmp;
		this->invert = invert;
	}

	bool operator()(move_t a, move_t b) {
		if (invert) {
			return cmp[a] <= cmp[b];
		} else {
			return cmp[a] > cmp[b];
		}
	}

	std::unordered_map<move_t, int> cmp;
	bool invert;
};

move_t Search::mtdf(Board &b, int depth, Color color, int &score, int guess)
{
	score = guess;
	int upperbound = VERY_GOOD;
	int lowerbound = VERY_BAD;
	move_t move = 0;
	
	do {
		int beta;
		if (score == lowerbound) {
			beta = score + 1;
		} else {
			beta = score;
		}
		move = alphabeta_with_memory(b, depth, color, score, beta - 1, beta);
		if (score < beta) {
			upperbound = score;
		} else {
			lowerbound = score;
		}
	} while (lowerbound < upperbound);
	return move;
}

move_t Search::alphabeta_with_memory(Board &b, int depth, Color color, int &score, int alpha, int beta)
{
	nodecount++;
	int is_white = color == White ? 1 : -1;
	TranspositionEntry bounds;
	bounds.depth = depth;
	int call_alpha = alpha, call_beta = beta;
	
	// check transposition table
	if (use_transposition_table) {
		auto transpose = transposition_table.find(b.get_hash());
		if (transpose != transposition_table.end()) {
			if (transpose->second.depth >= depth) {
				// found something at appropriate depth
				if (transpose->second.lower >= beta) {
					score = transpose->second.lower;
					return 0;
				} else if (transpose->second.upper <= alpha) {
					score = transpose->second.upper;
					return 0;
				}
				bounds = transpose->second;
				alpha = max(alpha, bounds.lower);
				beta = min(beta, bounds.upper);
			}
		}
	}
	
	if (depth == 0) {
		return eval->evaluate(b);
	}
	int original_alpha = alpha;
	int original_beta = beta;
	std::vector<move_t> moves;
	moves.reserve(50);
	b.legal_moves(color, moves);
	
	if (moves.empty()) {
		if (b.king_in_check(color)) {
			score = is_white * (VERY_GOOD - depth);
			return 0;
		} else {
			score = 0;
			return 0;
		}
	}

	move_t best_move = 0;
	int best_score = 0;

	int currentnodescore = eval->evaluate(b);
	std::unordered_map<move_t, int> scores(moves.size() * 2);

	if (depth > min_score_prune_sorting) {
		for (std::vector<move_t>::iterator iter = moves.begin(); iter != moves.end(); iter++) {
			scores[*iter] = eval->delta_evaluate(b, *iter, currentnodescore);
		}
		if (depth > min_score_prune_sorting) {
			std::sort(moves.begin(), moves.end(), PrecomputedComparator(scores, color));
		}
	}
	for (std::vector<move_t>::iterator iter = moves.begin(); iter != moves.end(); iter++) {
		int subtree_score;
		int submove = 0;
		if (depth == 1) {
			subtree_score = eval->delta_evaluate(b, *iter, currentnodescore);
		} else {
			b.apply_move(*iter);
			submove = alphabeta_with_memory(b, depth - 1, get_opposite_color(color), subtree_score, alpha, beta);
			b.undo_move(*iter);
		}
		
		if (search_debug) {
			moves[*iter] = subtree_score;
		}

		// ignore pruned branches
		if (submove == -1) {
			continue;
		}
		if (iter == moves.begin() || (color == White && subtree_score > best_score) || (color == Black && subtree_score < best_score)) {
			best_score = subtree_score;
			best_move = *iter;
		}
		if (color == White) {
			alpha = max(alpha, best_score);
		}
		else {
			beta = min(beta, best_score);
		}
		if (use_pruning && alpha > beta) {
			// prune this branch
			best_move = -1;
			break;
		}
	}

	score = best_score;

	// write to transposition table
	if (use_transposition_table && transposition_table.size() < transposition_table_size) {
		if (best_score <= original_alpha) {
			bounds.upper = best_score;
		} else if (best_score > original_alpha && best_score < original_beta) {
			bounds.lower = best_score;
			bounds.upper = best_score;
			bounds.move = best_move;
		} else if (best_score >= original_beta) {
			bounds.lower = best_score;
		}
		transposition_table[b.get_hash()] = bounds;
	}
	return best_move;
}
