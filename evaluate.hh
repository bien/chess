#ifndef EVALUATE_H_
#define EVALUATE_H_

#include "search.hh"
#include "fenboard.hh"

class SimpleEvaluation : public Evaluation {
public:
    virtual int evaluate(const Fenboard &b);
    virtual int delta_evaluate(Fenboard &b, move_t move, int previous_score);
    bool endgame(const Fenboard &b, int &eval) const;
    virtual ~SimpleEvaluation() {}

protected:
    int compute_scores(int qct, int bct, int rct, int nct, int pct, int rpct, int ppawn,
        int isopawn, int dblpawn, int nscore, int bscore, int kscore, int rhopenfile, int rfopenfile, int qscore) const;
private:
    int delta_evaluate_piece(Fenboard &b, piece_t piece, int rank, int file) const;
};

const int NUM_FEATURES = 18;

class SimpleBitboardEvaluation : public SimpleEvaluation {
public:
    int evaluate(const Fenboard &b);
    void get_features(const Fenboard &b, int *features);
    virtual ~SimpleBitboardEvaluation() {}
private:

};

#endif
