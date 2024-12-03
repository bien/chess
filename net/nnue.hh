#include "evaluate.hh"
#include "matrix.hh"

class NNUEEvaluation : public SimpleEvaluation {
public:
    int evaluate(const Fenboard &b);
    int delta_evaluate(Fenboard &b, move_t move, int previous_score) const;

private:
    int calculate_score(const matrix<1, 512, int16_t> &input_layer) const;
    matrix<1, 512, int16_t> dense_1_layer;
};