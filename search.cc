#include "search.hh"
#include <limits.h>
#include <vector>
#include <utility>
#include <algorithm>
#include <iostream>
#include <sstream>

bool search_debug = false;

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
	move_t result = nega_alphabeta(b, depth, color, score, 0, 0);
	if (color == Black) {
		score = -score;
	}
	return result;
}

move_t Search::alphabeta(Board &b, int depth, Color color)
{
	nodecount = 0;
	move_t result = nega_alphabeta(b, depth, color, score, VERY_BAD, VERY_GOOD);
	if (color == Black) {
		score = -score;
	}
	return result;
}

Search::Search(const Evaluation *eval)
	: score(0), nodecount(0), use_transposition_table(true), use_pruning(true), eval(eval)
{}


void Search::reset()
{
	score = 0;
	nodecount = 0;
	transposition_table.clear();
}

struct PrecomputedComparator {
	PrecomputedComparator(std::unordered_map<move_t, int> cmp) {
		this->cmp = cmp;
	}

	bool operator()(move_t a, move_t b) {
		return cmp[a] > cmp[b];
	}

	std::unordered_map<move_t, int> cmp;
};

move_t Search::nega_alphabeta(Board &b, int depth, Color color, int &score, int alpha, int beta)
{
	int initial_alpha = alpha, initial_beta = beta;
	nodecount++;
	int sign = color == White ? 1 : -1;
	
	// check transposition table
	if (use_transposition_table) {
		std::unordered_map<uint64_t, std::pair<int, int> >::iterator transpose = transposition_table.find(b.get_hash());
		if (transpose != transposition_table.end()) {
			if (transpose->second.first >= depth) {
				score = transpose->second.second;
				return 0;
			}
		}
	}
	
	if (depth == 0) {
		return sign * eval->evaluate(b);
	}
	std::vector<move_t> moves;
	moves.reserve(50);
	b.legal_moves(color, moves);
	
	if (moves.empty()) {
		if (b.king_in_check(color)) {
			score = VERY_BAD + depth;
			return 0;
		} else {
			score = 0;
			return 0;
		}
	}

	move_t best_move = 0;
	int best_score = 0;
	int min_score_prune_sorting = 3;

	int currentnodescore = eval->evaluate(b);
	std::unordered_map<move_t, int> scores, subtree_scores, pruning;
	if (search_debug || depth > min_score_prune_sorting) {
		for (std::vector<move_t>::iterator iter = moves.begin(); iter != moves.end(); iter++) {
			scores[*iter] = sign * eval->delta_evaluate(b, *iter, currentnodescore);
		}
		if (depth > min_score_prune_sorting) {
			std::sort(moves.begin(), moves.end(), PrecomputedComparator(scores));
		}
	}
	for (std::vector<move_t>::iterator iter = moves.begin(); iter != moves.end(); iter++) {
		int subtree_score;
		int submove = 0;
		if (depth == 1) {
			subtree_score = sign * eval->delta_evaluate(b, *iter, currentnodescore);
		} else {
			b.apply_move(*iter);
			submove = nega_alphabeta(b, depth - 1, get_opposite_color(color), subtree_score, -beta, -alpha);
			b.undo_move(*iter);
			subtree_score = -subtree_score;
		}
		if (search_debug) {
			subtree_scores[*iter] = subtree_score;
			if (submove == -1) {
				pruning[*iter] = 1;
			}
		}
		// ignore pruned branches
		if (submove == -1) {
			continue;
		}
		if (iter == moves.begin() || subtree_score > best_score) {
			best_score = subtree_score;
			best_move = *iter;
		}
		if (best_score > alpha) {
			alpha = best_score;
		}
		if (use_pruning && alpha >= beta) {
			// prune this branch
			best_move = -1;
			break;
		}
	}

	if (search_debug) {
		std::cout << "searchdebug: " << b << " depth " << depth << " alpha " << initial_alpha << " beta " << initial_beta << std::endl;
		for (std::vector<move_t>::iterator iter = moves.begin(); iter != moves.end(); iter++) {
			std::ostringstream movetext;
			b.print_move(*iter, movetext);
			std::cout << " move: " << movetext.str() << " est: " << scores[*iter] << " computed: " << subtree_scores[*iter];
			if (*iter == best_move) {
				std::cout << "*";
			}
			if (pruning[*iter]) {
				std::cout << "?";
			}
			std::cout << std::endl;
		}
	}
	
	score = best_score;
	
	// write to transposition table
	if (use_transposition_table) {
		transposition_table[b.get_hash()] = std::pair<int, int>(depth, score);
	}
	return best_move;
}