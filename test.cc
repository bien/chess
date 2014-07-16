#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <set>
#include "chess.hh"
#include "pgn.hh"

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

template <class T>
void assert_equals_unordered(const std::vector<T> &a, const std::vector<T> &b)
{
	std::set<T> aset(a.begin(), a.end());
	std::set<T> bset(b.begin(), b.end());
	
	for (typename std::set<T>::const_iterator aiter = aset.begin(); aiter != aset.end(); aiter++) {
		if (bset.find(*aiter) == bset.end()) {
			std::cout << "Couldn't find expected value " << *aiter << std::endl;
			abort();
		}
	}
	for (typename std::set<T>::const_iterator biter = bset.begin(); biter != bset.end(); biter++) {
		if (aset.find(*biter) == aset.end()) {
			std::cout << "Found unexpected value " << *biter << std::endl;
			abort();
		}
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
			assert_equals(static_cast<piece_t>(EMPTY), b.get_piece(make_board_pos(r, i)));
		}
	}

	std::vector<move_t> legal_white, legal_black;
	b.legal_moves(White, legal_white);
	b.legal_moves(Black, legal_black);
	std::vector<move_t> expected_white, expected_black;

	for (int i = 0; i < 8; i++) {
		for (int j = 3; j <= 6; j++) {
			std::ostringstream os;
			os << char('a' + i) << j;
			if (j >= 5) {
				expected_black.push_back(b.read_move(os.str(), Black));
			} else {
				expected_white.push_back(b.read_move(os.str(), White));
			}
		}
		if (i == 0 || i == 2 || i == 5 || i == 7) {
			std::ostringstream os;
			os << 'N' << char('a' + i) << 3;
			expected_white.push_back(b.read_move(os.str(), White));
			os.str("");
			os << 'N' << char('a' + i) << 6;
			expected_black.push_back(b.read_move(os.str(), Black));
		}
	}
	assert_equals_unordered(expected_white, legal_white);
	assert_equals_unordered(expected_black, legal_black);

	std::ostringstream boardtext;
	boardtext << b;
	assert_equals(std::string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 0"), boardtext.str());
	
	move_t e4 = b.read_move("e4", White);
	b.apply_move(e4);

	boardtext.str("");
	boardtext << b;
	assert_equals(std::string("rnbqkbnr/pppp1ppp/8/4p3/8/8/PPPPPPPP/RNBQKBNR b KQkq e3 0 0"), boardtext.str());
	
	assert_equals(make_piece(PAWN, White), b.get_piece(make_board_pos(3, 4)));
	assert_equals(static_cast<piece_t>(EMPTY), b.get_piece(make_board_pos(1, 4)));
	b.undo_move(e4);
	
	boardtext.str("");
	boardtext << b;
	assert_equals(std::string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 0"), boardtext.str());
	
	for (int i = 0; i < 8; i++) {
		assert_equals(make_piece(pieces[i], White), b.get_piece(make_board_pos(0, i)));
		assert_equals(make_piece(pieces[i], Black), b.get_piece(make_board_pos(7, i)));
		assert_equals(make_piece(PAWN, Black), b.get_piece(make_board_pos(6, i)));
		assert_equals(make_piece(PAWN, White), b.get_piece(make_board_pos(1, i)));
		for (int r = 2; r < 6; r++) {
			assert_equals(static_cast<piece_t>(EMPTY), b.get_piece(make_board_pos(r, i)));
		}
	}
	
	std::map<std::string, std::string> game_metadata;
	std::vector<std::pair<std::string, std::string> > movelist;
	std::ifstream fischer("games/Fischer.pgn");
	read_pgn(fischer, game_metadata, movelist);
	for (std::vector<std::pair<std::string, std::string> >::iterator iter = movelist.begin(); iter != movelist.end(); iter++) {
		move_t move = b.read_move(iter->first, White);
		b.apply_move(move);
		move = b.read_move(iter->second, Black);
		b.apply_move(move);
	}
	
	boardtext.str("");
	boardtext << b;
	assert_equals(std::string("r4r1k/ppb3pp/8/2N1nN2/3p4/3P2PP/PPP3BK/R2Q4 w - - 0 0"), boardtext.str());

	return 0;
}