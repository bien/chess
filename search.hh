#ifndef SEARCH_HH_
#define SEARCH_HH_

#include "chess.hh"

class Evaluation {
public:
	virtual int evaluate(const Board &) const = 0;
	virtual int delta_evaluate(Board &b, move_t move) const;
};

move_t minimax(Board &b, const Evaluation &eval, int depth, Color color, int &score, int &nodecount);
move_t alphabeta(Board &b, const Evaluation &eval, int depth, Color color, int &score, int &nodecount);

#endif
