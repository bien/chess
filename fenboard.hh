#ifndef FEN_BOARD_HH_
#define FEN_BOARD_HH_

#include <string>
#include <ostream>
#include <vector>

const int EMPTY = 0;
const int PAWN = 1;
const int KNIGHT = 2;
const int BISHOP = 3;
const int ROOK = 4;
const int QUEEN = 5;
const int KING = 6;
const int INVALID = 8;
const int PIECE_MASK = 7;

const int WhiteMask = 0;
const int BlackMask = 8;


typedef unsigned char BoardPos;
const BoardPos InvalidPos = 8;

enum Color { White, Black };
typedef unsigned int move_t;
typedef unsigned char piece_t;

Color get_color(piece_t piece);
piece_t get_captured_piece(move_t move);


BoardPos make_board_pos(int rank, int file);
piece_t make_piece(piece_t type, Color color);
Color get_opposite_color(Color color);


/*
class PrimitiveBoard
{
public:
	piece_t get_piece(unsigned char rank, unsigned char file) const;
	void set_piece(unsigned char rank, unsigned char file, piece_t);
	void update(); // called whenever the board is updated
	void get_source(move_t move, unsigned char &rank, unsigned char &file);
	void get_dest(move_t move, unsigned char &rank, unsigned char &file);

	unsigned char get_promotion(move_t move);
	piece_t get_captured_piece(move_t move);
	piece_t set_color(piece_t, Color);

	move_t make_move(unsigned char srcrank, unsigned char srcfile, unsigned char source_piece, unsigned char destrank, unsigned char destfile, unsigned char captured_piece, unsigned char promote) const;
};
*/

const int ZOBRIST_HASH_COUNT = 6*2*64 + 1 + 4 + 8;

class FenBoard
{
public:
	FenBoard();
	void set_starting_position();
	void get_fen(std::ostream &os) const;
	void set_fen(const std::string &fen);
	void print_move(move_t, std::ostream &os) const;
	move_t read_move(const std::string &s, Color color) const;

	virtual piece_t get_piece(unsigned char rank, unsigned char file) const = 0;
	virtual void set_piece(unsigned char rank, unsigned char file, piece_t) = 0;
	virtual void get_source(move_t move, unsigned char &rank, unsigned char &file) const = 0;
	virtual void get_dest(move_t move, unsigned char &rank, unsigned char &file) const = 0;
	virtual unsigned char get_promotion(move_t move) const = 0;
	virtual uint64_t get_hash() const = 0;
	virtual bool king_in_check(Color) const = 0;
	// piece_t get_captured_piece(move_t move);
	// piece_t set_color(piece_t, Color);
	virtual void legal_moves(Color color, std::vector<move_t> &moves, piece_t limit_to_this_piece=0) const = 0;
	piece_t set_color(piece_t piece, Color color) const {
		if (color == Black) {
			return piece | BlackMask;
		} else {
			return piece & PIECE_MASK;
		}
	}

	void apply_move(move_t);
	void undo_move(move_t);

protected:
	Color side_to_play;
	char enpassant_file;
	short move_count;

	void invalidate_castle(Color);
	void invalidate_castle_side(Color, bool kingside);

	virtual void update() = 0; // called whenever the board is updated
	virtual move_t make_move(unsigned char srcrank, unsigned char srcfile, unsigned char source_piece, unsigned char destrank, unsigned char destfile, unsigned char captured_piece, unsigned char promote) const = 0;

	char castle;
	int get_castle_bit(Color color, bool kingside) const;
	bool can_castle(Color color, bool kingside) const;
	uint64_t hash;

private:
	char fen_repr(unsigned char piece) const;
	void fen_flush(std::ostream &os, int &empty) const;
	void fen_handle_space(unsigned char piece, std::ostream &os, int &empty) const;
	unsigned char read_piece(unsigned char c) const;

	move_t invalid_move(const std::string &s) const;

	void update_zobrist_hashing_piece(unsigned char rank, unsigned char file, piece_t piece, bool adding);
	void update_zobrist_hashing_castle(Color, bool kingside, bool enabling);
	void update_zobrist_hashing_move();
	void update_zobrist_hashing_enpassant(int file, bool enabling);

	static uint64_t zobrist_hashes[ZOBRIST_HASH_COUNT];
};

std::ostream &operator<<(std::ostream &os, const FenBoard &b);

#endif
