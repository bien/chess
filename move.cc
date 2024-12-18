#include "move.hh"
#include <cassert>

piece_t get_captured_piece(move_t move, Color color)
{
    return make_piece((move >> 16) & PIECE_MASK, color);
}

piece_t get_promotion(move_t move, Color color)
{
    return make_piece((move >> 24) & PIECE_MASK, color);
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

BoardPos make_board_pos(int rank, int file)
{
    assert(rank >= 0 && rank < 8);
    assert(file >= 0 && file < 8);

    return ((MEMORY_RANKS - LOGICAL_RANKS) / 2 + rank) * MEMORY_FILES + (MEMORY_FILES - LOGICAL_FILES) / 2 + file;
}

bool get_invalidates_queenside_castle(move_t move)
{
    return move & INVALIDATES_CASTLE_Q;
}
bool get_invalidates_kingside_castle(move_t move)
{
    return move & INVALIDATES_CASTLE_K;    
}
