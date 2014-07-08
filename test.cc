#include <stdlib.h>
#include <iostream>
#include "chess.hh"


char get_board_rank(BoardPos bp);
char get_board_file(BoardPos bp);
void get_vector(BoardPos origin, BoardPos bp, char &drank, char &dfile);
BoardPos make_board_pos(int rank, int file);

BoardPos get_source_pos(move_t move);
BoardPos get_dest_pos(move_t move);
piece_t make_piece(piece_t type, Color color);

template <class T>
void assert_equals(T expected, T actual)
{
	if (expected != actual) {
		std::cout << "Test failed: expected " << expected << " but got " << actual << std::endl;
		abort();
	}
}

int main()
{
	Board b;
	for (char r = 0; r < 8; r++) {
		for (char f = 0; f < 8; f++) {
			BoardPos bp = make_board_pos(r, f);
			assert_equals(f, get_board_file(bp));
			assert_equals(r, get_board_rank(bp));
		}
	}
	
	int pieces[8] = { ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK };
	for (int i = 0; i < 8; i++) {
		assert_equals(make_piece(pieces[i], White), b.get_piece(make_board_pos(0, i)));
		assert_equals(make_piece(pieces[i], Black), b.get_piece(make_board_pos(7, i)));
		assert_equals(make_piece(PAWN, Black), b.get_piece(make_board_pos(6, i)));
		assert_equals(make_piece(PAWN, White), b.get_piece(make_board_pos(1, i)));
		for (int r = 2; r < 6; r++) {
			assert_equals(static_cast<char>(EMPTY), b.get_piece(make_board_pos(r, i)));
		}
	}
	
	return 0;
}