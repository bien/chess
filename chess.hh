#ifndef CHESS_H_
#define CHESS_H_

#include <vector>
#include <ostream>
#include <string>
#include "fenboard.hh"

const int LOGICAL_RANKS = 8;
const int LOGICAL_FILES = 8;

const int MEMORY_RANKS = 12;
const int MEMORY_FILES = 10;
const int MEMORY_SIZE = MEMORY_RANKS * MEMORY_FILES / 2;


const int VERY_GOOD = 10000;
const int VERY_BAD = -VERY_GOOD;
const int SCORE_MAX = VERY_GOOD + 1000;
const int SCORE_MIN = VERY_BAD - 1000;

const int ZOBRIST_HASH_COUNT = 6*2*64 + 1 + 4 + 8;

typedef unsigned char BoardPos;

BoardPos make_board_pos(int rank, int file);
Color get_opposite_color(Color color);

BoardPos get_source_pos(move_t move);
BoardPos get_dest_pos(move_t move);
piece_t get_promotion(move_t move, Color color);

piece_t make_piece(piece_t type, Color color);

unsigned char get_board_rank(BoardPos bp);
unsigned char get_board_file(BoardPos bp);
Color get_color(piece_t piece);

class SimpleBoard
{
public:
	SimpleBoard();
	SimpleBoard(const SimpleBoard &);
	void reset();

	void legal_moves(Color color, std::vector<move_t> &moves, piece_t limit_to_this_piece=0) const;
	piece_t get_piece(BoardPos) const;
	bool king_in_check(Color) const;
	bool can_castle(Color, bool kingside) const;

	void apply_move(move_t);
	void undo_move(move_t);
	
	uint64_t get_hash() const { return hash; }
	Color side_to_play;
	
	void update();

private:
	bool in_check;
	
protected:
	char castle;
	char enpassant_file;
	short move_count;
	void invalidate_castle(Color);
	void invalidate_castle_side(Color, bool kingside);
	int get_castle_bit(Color color, bool kingside) const;
	void set_piece(unsigned char rank, unsigned char file, piece_t);
	piece_t get_piece(unsigned char rank, unsigned char file) const;
	
	move_t make_move(BoardPos source, piece_t source_piece, BoardPos dest, piece_t dest_piece, piece_t promote) const;
	move_t make_move(unsigned char srcrank, unsigned char srcfile, unsigned char source_piece, unsigned char destrank, unsigned char destfile, unsigned char captured_piece, unsigned char promote) const;
	void get_source(move_t move, unsigned char &rank, unsigned char &file) const;
	void get_dest(move_t move, unsigned char &rank, unsigned char &file) const;
	unsigned char get_promotion(move_t move) const {
		return ::get_promotion(move, White) & PIECE_MASK;
	}
	

private:
	void set_piece(BoardPos bp, piece_t);
	void standard_initial();
	void update_zobrist_hashing_piece(BoardPos pos, piece_t piece, bool adding);
	void update_zobrist_hashing_castle(Color, bool kingside, bool enabling);
	void update_zobrist_hashing_move();
	void update_zobrist_hashing_enpassant(int file, bool enabling);

	bool discovers_check(move_t, Color) const;
	BoardPos find_piece(piece_t) const;

	// support mode: include pawn captures to same color, include piece captures to same color
	void get_moves(Color, std::vector<move_t> &moves, bool support_mode=false, piece_t limit_to_this_piece=0) const;
	void calculate_moves(Color color, BoardPos bp, piece_t piece, std::vector<move_t> &moves, bool exclude_pawn_advance, bool support_mode) const;

	void pawn_advance(BoardPos bp, Color piece_color, std::vector<move_t> &moves) const;
	void pawn_capture(BoardPos bp, char dfile, Color piece_color, bool support_mode, std::vector<move_t> &moves) const;
	void pawn_move(BoardPos source, BoardPos dest, std::vector<move_t> &moves) const;

	void single_move(BoardPos base, piece_t, char drank, char dfile, Color capture, std::vector<move_t> &moves) const;
	BoardPos get_capture(BoardPos base, char drank, char dfile, Color capture) const;
	void repeated_move(BoardPos base, piece_t, char drank, char dfile, Color capture, std::vector<move_t> &moves) const;
	bool removes_check(move_t move, Color color) const;

	char data[MEMORY_SIZE];
	uint64_t hash;

	static uint64_t zobrist_hashes[ZOBRIST_HASH_COUNT];
};

typedef FenBoard<SimpleBoard> Board;

#endif
