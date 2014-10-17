#ifndef SEARCH_HH_
#define SEARCH_HH_

#include "chess.hh"
#include <unordered_map>

extern bool search_debug;

class Evaluation {
public:
	virtual int evaluate(const Board &) const = 0;
	virtual int delta_evaluate(Board &b, move_t move, int previous_score) const;
};

struct Search {
	Search(const Evaluation *eval);
	move_t minimax(Board &b, int depth, Color color);
	move_t alphabeta(Board &b, const int depth, Color color);
	move_t nega_alphabeta(Board &b, int depth, Color color, int &score, int alpha, int beta);
	
	void reset();
	
	int score;
	int nodecount;
	std::unordered_map<uint64_t, std::pair<int, int> > transposition_table;
	bool use_transposition_table;
	bool use_pruning;
	const Evaluation *eval;
};

#endif
