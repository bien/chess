#ifndef FEN_BOARD_HH_
#define FEN_BOARD_HH_

#include <string>
#include <ostream>

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

const int WhiteMask = 0;
const int BlackMask = 8;

typedef unsigned int move_t;
typedef unsigned char piece_t;

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

	move_t make_move(unsigned char srcrank, unsigned char srcfile, unsigned char source_piece, unsigned char destrank, unsigned char destfile, unsigned char captured_piece, unsigned char promote) const;
};
*/

template <typename PrimitiveBoard>
class FenBoard : public PrimitiveBoard
{
public:
	void set_starting_position();
	void get_fen(std::ostream &os) const;
	void set_fen(const std::string &fen);
	void print_move(move_t, std::ostream &os) const;
	move_t read_move(const std::string &s, Color color) const;
	
private:
	char fen_repr(unsigned char piece) const;
	void fen_flush(std::ostream &os, int &empty) const;
	void fen_handle_space(unsigned char piece, std::ostream &os, int &empty) const;
	unsigned char read_piece(unsigned char c) const;
	
	move_t invalid_move(const std::string &s) const;
};

template <class PrimitiveBoard, typename move_t>
std::ostream &operator<<(std::ostream &os, const FenBoard<PrimitiveBoard> &b);

#include "fenboard.cc"

#endif
