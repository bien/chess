#ifndef EVALUATE_H_
#define EVALUATE_H_

#include "search.hh"


class SimpleEvaluation : public Evaluation {
public:
	int evaluate(const Board &b) const;
	int delta_evaluate(Board &b, move_t move, int previous_score) const;

private:
	double compute_scores(int qct, int bct, int rct, int nct, int pct, int rpct, int ppawn, 
		int isopawn, int dblpawn, int nscore, int bscore, int kscore, int rhopenfile, int rfopenfile, int qscore) const;
	int delta_evaluate_piece(Board &b, piece_t piece, int rank, int file) const;
};


#endif
