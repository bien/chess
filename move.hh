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
BoardPos get_source_pos(move_t move);
BoardPos get_dest_pos(move_t move);
unsigned char get_board_rank(BoardPos bp);
unsigned char get_board_file(BoardPos bp);
BoardPos make_board_pos(int rank, int file);

bool get_invalidates_queenside_castle(move_t move);
bool get_invalidates_kingside_castle(move_t move);

#endif
