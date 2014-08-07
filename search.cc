#include "search.hh"
#include <limits.h>
#include <vector>
#include <utility>
#include <algorithm>
#include <unordered_map>
#include <iostream>
#include <sstream>

move_t nega_alphabeta(Board &b, const Evaluation &eval, int depth, Color color, int &score, int alpha, int beta, bool use_pruning, int &nodecount);

int Evaluation::delta_evaluate(Board &b, move_t move) const
{
	int score;
	b.apply_move(move);
	score = evaluate(b);
	b.undo_move(move);
	return score;
}

move_t minimax(Board &b, const Evaluation &eval, int depth, Color color, int &score, int &nodecount)
{
	nodecount = 0;
	move_t result = nega_alphabeta(b, eval, depth, color, score, 0, 0, false, nodecount);
	if (color == Black) {
		score = -score;
	}
	return result;
}

move_t alphabeta(Board &b, const Evaluation &eval, int depth, Color color, int &score, int &nodecount)
{
	nodecount = 0;
	move_t result = nega_alphabeta(b, eval, depth, color, score, INT_MIN, INT_MAX, true, nodecount);
	if (color == Black) {
		score = -score;
	}
	return result;
}


move_t nega_alphabeta(Board &b, const Evaluation &eval, int depth, Color color, int &score, int alpha, int beta, bool use_pruning, int &nodecount)
{
	nodecount++;
	int sign = color == White ? 1 : -1;
	if (depth == 0) {
		return sign * eval.evaluate(b);
	}
	std::vector<move_t> moves;
	b.legal_moves(color, moves);
	
	if (moves.empty()) {
		if (b.king_in_check(color)) {
			score = -INT_MAX;
			return 0;
		} else {
			score = 0;
			return 0;
		}
	}
	
	move_t best_move = 0;
	int best_score = 0;
	
	// TODO: sort moves here
	
	for (std::vector<move_t>::iterator iter = moves.begin(); iter != moves.end(); iter++) {
		int subtree_score;
		if (depth == 1) {
			subtree_score = sign * eval.delta_evaluate(b, *iter);
		} else {
			b.apply_move(*iter);
			nega_alphabeta(b, eval, depth - 1, get_opposite_color(color), subtree_score, -beta, -alpha, use_pruning, nodecount);
			b.undo_move(*iter);
			subtree_score = -subtree_score;
		}
		// checkmate decay
		if (subtree_score > INT_MAX / 2) {
			subtree_score--;
		} else if (subtree_score < INT_MIN / 2) {
			subtree_score++;
		} 
		if (iter == moves.begin() || subtree_score > best_score) {
			best_score = subtree_score;
			best_move = *iter;
		}
		if (best_score > alpha) {
			alpha = best_score;
		}
		if (use_pruning && alpha > beta) {
			// prune this branch
			return 0;
		}
	}
	
	score = best_score;
	return best_move;
}