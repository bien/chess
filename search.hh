#ifndef SEARCH_HH_
#define SEARCH_HH_

#include "fenboard.hh"
#include <unordered_map>
#include <tuple>
#include <utility>

extern int search_debug;

const int VERY_GOOD = 10000;
const int VERY_BAD = -VERY_GOOD;
const int SCORE_MAX = VERY_GOOD + 1000;
const int SCORE_MIN = VERY_BAD - 1000;

class Evaluation {
public:
	virtual int evaluate(const FenBoard &) const = 0;
	virtual int delta_evaluate(FenBoard &b, move_t move, int previous_score) const;
};

struct TranspositionEntry {
	short depth;
	int lower;
	int upper;
	move_t move;

	TranspositionEntry()
		: depth(0), lower(SCORE_MIN), upper(SCORE_MAX), move(-1)
	{}

	TranspositionEntry(const TranspositionEntry &entry)
		: depth(entry.depth), lower(entry.lower), upper(entry.upper), move(entry.move)
	{}
};

struct Search {
	Search(const Evaluation *eval, int transposition_table_size=1000 * 1000 * 50);
	move_t minimax(FenBoard &b, int depth, Color color);
	move_t alphabeta(FenBoard &b, const int depth, Color color);

	void reset();

	int score;
	int nodecount;
	// hash -> depth, (max, min)
	int transposition_table_size;
	bool use_transposition_table;
	std::unordered_map<uint64_t, TranspositionEntry> transposition_table;
	bool use_pruning;
	const Evaluation *eval;
	int min_score_prune_sorting;
	bool use_mtdf;

private:
	std::pair<move_t, int> alphabeta_with_memory(FenBoard &b, int depth, Color color, int alpha, int beta);
	move_t mtdf(FenBoard &b, int depth, Color color, int &score, int guess);
};

#endif
