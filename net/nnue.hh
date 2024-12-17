#include "evaluate.hh"
#include "matrix.hh"

const int UNITY = 64;

class NNUEEvaluation : public SimpleEvaluation {
public:
    int evaluate(const Fenboard &b);
    int delta_evaluate(Fenboard &b, move_t move, int previous_score);

private:
    void add_remove_piece(const Fenboard &b, int colored_piece_type, bool remove, int piece_pos, matrix<1, 512, int16_t, UNITY> &layer);
    void recalculate_dense1_layer(const Fenboard &b, matrix<1, 512, int16_t, UNITY> &layer);
    int calculate_score(const matrix<1, 512, int16_t, UNITY> &input_layer, Color side_to_play) const;
    matrix<1, 512, int16_t, UNITY> dense_1_layer;
};