#ifndef MOVE_H_
#define MOVE_H_

#include "bitboard.hh"

BoardPos get_source_pos(move_t move);
BoardPos get_dest_pos(move_t move);

unsigned char get_board_rank(BoardPos bp);
unsigned char get_board_file(BoardPos bp);

BoardPos make_board_pos(int rank, int file);
piece_t get_captured_piece(move_t move, Color color=White);
piece_t get_promotion(move_t move, Color color=White);
void get_source(move_t move, unsigned char &rank, unsigned char &file);
void get_dest(move_t move, unsigned char &rank, unsigned char &file);
BoardPos make_board_pos(int rank, int file);

bool get_invalidates_queenside_castle(move_t move);
bool get_invalidates_kingside_castle(move_t move);

const int LOGICAL_RANKS = 8;
const int LOGICAL_FILES = 8;
const int MEMORY_RANKS = 8;
const int MEMORY_FILES = 8;

inline BoardPos get_source_pos(move_t move)
{
    return make_board_pos(((move >> 9) & 0x7), (move >> 6) & 0x7);
}

inline BoardPos get_dest_pos(move_t move)
{
    return make_board_pos(((move >> 3) & 0x7), move & 0x7);
}

inline unsigned char get_board_rank(BoardPos bp)
{
    return bp / MEMORY_FILES - (MEMORY_RANKS - LOGICAL_RANKS) / 2;
}

inline unsigned char get_board_file(BoardPos bp)
{
    return bp % MEMORY_FILES - (MEMORY_FILES - LOGICAL_FILES) / 2;
}


#endif
