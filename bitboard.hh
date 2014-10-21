#ifndef BITBOARD_H_
#define BITBOARD_H_

#include <ostream>
#include <vector>

typedef unsigned short move_t;
typedef unsigned char piece_t;
typedef unsigned char board_pos_t;

const int EMPTY = 0;
const int PAWN = 1;
const int KNIGHT = 2;
const int BISHOP = 3;
const int ROOK = 4;
const int QUEEN = 5;
const int KING = 6;
const int INVALID = 8;
const int PIECE_MASK = 7;

enum Color { White, Black };

const int bitmask_types = 7;

class Bitboard
{
public:
	Bitboard();
	void reset();

	void legal_moves(bool is_white, std::vector<move_t> &moves) const;
	piece_t get_piece(board_pos_t) const;
	void get_fen(std::ostream &os) const;
	void set_fen(const std::string &fen);
	bool king_in_check(Color) const;

	void apply_move(move_t);
	void undo_move(move_t);
	void print_move(move_t, std::ostream &os) const;
	
	move_t read_move(const std::string &s, Color color) const;
	
private:
	void print_pos(char pos, std::ostream &os) const;
	void get_point_moves(uint64_t piece_bitmask, uint64_t illegal_dest, const uint64_t *piece_moves, std::vector<move_t> &moves) const;
	void get_directional_moves(uint64_t piece_bitmask, uint64_t capture_dest, uint64_t illegal_dest, uint64_t *piece_moves, std::vector<move_t> &moves) const;
	
	Color side_to_play;
	bool in_check;
	char castle;
	char enpassant_target;
	short move_count;
	
	char charboard[64];
	uint64_t piece_bitmasks[2*bitmask_types];
};

std::ostream &operator<<(std::ostream &os, const Bitboard &b);

#endif
