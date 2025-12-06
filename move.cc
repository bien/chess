#include "move.hh"
#include <cassert>

piece_t get_captured_piece(move_t move, Color color)
{
    return make_piece((move >> CAPTURE_PIECE_POS) & PIECE_MASK, color);
}

piece_t get_promotion(move_t move, Color color)
{
    piece_t piece = (move >> PROMO_PIECE_POS) & PIECE_MASK;
    if (piece > 0 && piece < bb_king) {
        return make_piece(piece, color);
    } else {
        return 0;
    }
}

void get_source(move_t move, unsigned char &rank, unsigned char &file)
{
    BoardPos bp = get_source_pos(move);
    rank = get_board_rank(bp);
    file = get_board_file(bp);
}
void get_dest(move_t move, unsigned char &rank, unsigned char &file)
{
    BoardPos bp = get_dest_pos(move);
    rank = get_board_rank(bp);
    file = get_board_file(bp);
}

bool get_invalidates_queenside_castle(move_t move)
{
    return move & INVALIDATES_CASTLE_Q;
}
bool get_invalidates_kingside_castle(move_t move)
{
    return move & INVALIDATES_CASTLE_K;
}
