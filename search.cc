#include "search.hh"
#include <limits.h>
#include <vector>
#include <utility>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <set>
#include <map>

int search_debug = 0;

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
	bool old_pruning = use_pruning;
	bool old_mtdf = use_mtdf;
	bool old_trans_table = use_transposition_table;
	use_pruning = false;
	use_mtdf = false;
	use_transposition_table = false;
	std::pair<move_t, int> result = alphabeta_with_memory(b, depth, color, 0, 0);
	score = result.second;
	if (color == Black) {
		score = -score;
	}
	use_pruning = old_pruning;
	use_mtdf = old_mtdf;
	use_transposition_table = old_trans_table;
	return result.first;
}

move_t Search::alphabeta(Board &b, int depth, Color color)
{
	nodecount = 0;
	move_t result;
	if (use_mtdf) {
		result = mtdf(b, depth, color, score, eval->evaluate(b));
	} else {
		std::pair<move_t, int> sub = alphabeta_with_memory(b, depth, color, SCORE_MIN, SCORE_MAX);
		score = sub.second;
		result = sub.first;
	}
	return result;
}

Search::Search(const Evaluation *eval, int transposition_table_size)
	: score(0), nodecount(0), transposition_table_size(transposition_table_size), use_transposition_table(true), use_pruning(true), eval(eval), min_score_prune_sorting(4), use_mtdf(true)
{
}


void Search::reset()
{
	score = 0;
	nodecount = 0;
	transposition_table.clear();
}

struct PrecomputedComparator {
	PrecomputedComparator(std::unordered_map<move_t, int> *cmp, bool invert) :
		cmp(cmp), invert(invert)
	{}

	bool operator()(move_t a, move_t b) {
		if (cmp->find(a) == cmp->end()) {
			return false;
		} else if (cmp->find(b) == cmp->end()) {
			return true;
		}
		if (invert) {
			return (*cmp)[b] < (*cmp)[a];
		} else {
			return (*cmp)[a] < (*cmp)[b];
		}
	}

	std::unordered_map<move_t, int> *cmp;
	bool invert;
};

move_t Search::mtdf(Board &b, int depth, Color color, int &score, int guess)
{
	score = guess;
	int upperbound = SCORE_MAX;
	int lowerbound = SCORE_MIN;
	move_t move = 0;
	int cumulative_nodes = nodecount;
	if (search_debug) {
		std::cout << "mtdf guess=" << guess << " depth=" << depth << std::endl;
	}
	
	do {
		int beta;
		if (score == lowerbound) {
			beta = score + 1;
		} else {
			beta = score;
		}
		std::pair<move_t, int> result = alphabeta_with_memory(b, depth, color, beta - 1, beta);
		move = result.first;
		score = result.second;
		if (score < beta) {
			upperbound = result.second;
		} else {
			lowerbound = result.second;
		}
		if (search_debug) {
			std::cout << "  mtdf a=" << beta - 1 << " b=" << beta << " score=" << score << " upper=" << upperbound << " lower=" << lowerbound << " move=";
			b.print_move(move, std::cout);
			std::cout << " nodes=" << nodecount - cumulative_nodes << std::endl;
			cumulative_nodes = nodecount;
		}
	} while (lowerbound < upperbound);
	
	return move;
}

std::pair<move_t, int> Search::alphabeta_with_memory(Board &b, int depth, Color color, int alpha, int beta)
{
	nodecount++;
	int is_white = color == White ? 1 : -1;
	TranspositionEntry bounds;
	bounds.depth = depth;
	
	// cutoff early if alpha or beta is impossibly high (ie if there is a checkmate higher in the tree)
	if (alpha > VERY_GOOD + depth) {
		return std::pair<move_t, int>(0, alpha);
	}
	else if (beta < VERY_BAD - depth) {
		return std::pair<move_t, int>(0, beta);
	}

	// check transposition table
	if (use_transposition_table) {
		auto transpose = transposition_table.find(b.get_hash());
		if (transpose != transposition_table.end()) {
			if (transpose->second.depth >= depth) {
				// found something at appropriate depth
				if (transpose->second.lower >= beta) {
					return std::pair<move_t, int>(transpose->second.move, transpose->second.lower);
				} else if (transpose->second.upper <= alpha) {
					return std::pair<move_t, int>(transpose->second.move, transpose->second.upper);
				}
				bounds = transpose->second;
				alpha = max(alpha, bounds.lower);
				beta = min(beta, bounds.upper);
			}
		}
	}
	
	if (depth == 0) {
		return std::pair<move_t, int>(0, eval->evaluate(b));
	}
	int original_alpha = alpha;
	int original_beta = beta;
	std::vector<move_t> moves;
	moves.reserve(50);
	b.legal_moves(color, moves);

	if (moves.empty()) {
		if (b.king_in_check(color)) {
			return std::pair<move_t, int>(0, is_white * (VERY_BAD - depth));
		} else {
			return std::pair<move_t, int>(0, 0);
		}
	}

	move_t best_move = 0;
	int best_score = 0;

	int currentnodescore = eval->evaluate(b);
	std::unordered_map<move_t, int> scores;
	std::unordered_map<move_t, uint64_t> hashes(moves.size() * 2);
	std::set<move_t> pruned;

	if (depth > min_score_prune_sorting) {
		for (std::vector<move_t>::iterator iter = moves.begin(); iter != moves.end(); iter++) {
			scores[*iter] = eval->delta_evaluate(b, *iter, currentnodescore);
		}

		PrecomputedComparator comp(&scores, color);
		std::sort(moves.begin(), moves.end(), comp);
	}
	for (std::vector<move_t>::iterator iter = moves.begin(); iter != moves.end(); iter++) {
		int subtree_score;
		move_t submove = 0;
		if (depth == 1) {
			subtree_score = eval->delta_evaluate(b, *iter, currentnodescore);
		} else {
			b.apply_move(*iter);
			std::pair<move_t, int> child = alphabeta_with_memory(b, depth - 1, get_opposite_color(color), alpha, beta);
			subtree_score = child.second;
			submove = child.first;
			hashes[*iter] = b.get_hash();
			b.undo_move(*iter);
		}
		
		if (search_debug) {
			scores[*iter] = subtree_score;
			if (submove == -1) {
				pruned.insert(*iter);
			}
		}

		if (iter == moves.begin() || (color == White && subtree_score > best_score) || (color == Black && subtree_score < best_score)) {
			best_score = subtree_score;
			// ignore pruned branches
			if (submove != -1) {
				best_move = *iter;
			}
		}
		if (use_pruning) {
			if (color == White) {
				alpha = max(alpha, best_score);
			}
			else {
				beta = min(beta, best_score);
			}
			if (alpha > beta) {
				// prune this branch
				best_move = -1;
				break;
			}
		}
	}
	
	if (search_debug > 1) {
		std::cout << "ab depth=" << depth << " color=" << color << " a=" << original_alpha << " b=" << original_beta << " h=" << std::hex << b.get_hash() << std::dec << std::endl;
		for (std::vector<move_t>::iterator iter = moves.begin(); iter != moves.end(); iter++) {
			std::cout << "  move=";
			b.print_move(*iter, std::cout);
			std::cout << " score=" << scores[*iter];
			if (pruned.find(*iter) != pruned.end()) {
				std::cout << "p";
			} else if (best_move == *iter) {
				std::cout << "*";
			}
			std::cout << " h=" << std::hex << hashes[*iter] << std::dec << std::endl;
		}
	}

	// write to transposition table
	if (use_transposition_table && transposition_table.size() < transposition_table_size && bounds.depth == depth) {
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
	return std::pair<move_t, int>(best_move, best_score);
}
