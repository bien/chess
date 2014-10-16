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

move_t minimax(Board &b, const Evaluation &eval, int depth, Color color, int &score, int &nodecount, std::unordered_map<uint64_t, std::pair<int, int> > &transposition_table);
move_t alphabeta(Board &b, const Evaluation &eval, int depth, Color color, int &score, int &nodecount, std::unordered_map<uint64_t, std::pair<int, int> > &transposition_table);

#endif
