#ifndef EVALUATE_H_
#define EVALUATE_H_

#include "search.hh"


class SimpleEvaluation : public Evaluation {
public:
	virtual int evaluate(const Board &b) const;
};


#endif
