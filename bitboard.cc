#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include "fenboard.hh"
#include "bitboard.hh"

move_t Bitboard::make_move(board_pos_t src, board_pos_t dest) const
{
	return (static_cast<int>(dest) << 6) | src | (in_check << 18) | (castle << 19) | (enpassant_file << 23);
}

void Bitboard::get_source(move_t move, unsigned char &rank, unsigned char &file) const
{
	file = move & 0x7;
	rank = (move >> 3) & 0x7;
}

void Bitboard::get_dest(move_t move, unsigned char &rank, unsigned char &file) const
{
	file = (move >> 6) & 0x7;
	rank = (move >> 9) & 0x7;
}

unsigned char Bitboard::get_promotion(move_t move) const
{
	return (move >> 12) & 0x7;
}

move_t Bitboard::make_move(unsigned char srcrank, unsigned char srcfile, unsigned char source_piece, unsigned char destrank, unsigned char destfile, unsigned char captured_piece, unsigned char promote) const
{
	return srcfile | (srcrank << 3) | (destfile << 6) | (destrank << 12) | ((promote & PIECE_MASK) << 15) | (in_check << 18) | (castle << 19) | (enpassant_file << 23);
}

 template<uint64_t... args>
 struct ArrayHolder {
 	static const uint64_t data[sizeof...(args)];
 };

 template<uint64_t... args>
 const uint64_t ArrayHolder<args...>::data[sizeof...(args)] = { args ... };

 template<size_t N, template<size_t> class F, uint64_t... args>
 struct generate_array_impl {
 	typedef typename generate_array_impl<N-1, F, F<N>::value, args...>::result result;
 };

 template<template<size_t> class F, uint64_t... args>
 struct generate_array_impl<0, F, args...> {
 	typedef ArrayHolder<F<0>::value, args...> result;
 };

 template<size_t N, template<size_t> class F>
 struct generate_array {
 	typedef typename generate_array_impl<N-1, F>::result result;
 };

 template<template<board_pos_t, board_pos_t> class pred, size_t src, size_t dest>
 struct generate_bitboard {
	 static const uint64_t value = (pred<src, dest-1>::value ? (1ULL << (dest-1)) : 0) | generate_bitboard<pred, src, dest-1>::value;

 };

 template<template<board_pos_t, board_pos_t> class pred, size_t src>
 struct generate_bitboard<pred, src, 0> {
	 static const uint64_t value = 0;

 };

 template<template<board_pos_t, board_pos_t> class pred>
 struct CreateBitboard {
	 template<size_t src>
	 struct Generator {
		 static const uint64_t value = generate_bitboard<pred, src, 64>::value;
	 };
 };

 template<int x>
 struct Abs
 {
	 const static int value = x < 0 ? -x : x;
 };

 struct BitArrays {
	 template<board_pos_t src, board_pos_t dest>
	 struct is_knight_move {
		 const static int rankdiff = Abs<((src) / 8) - ((dest) / 8)>::value;
		 const static int filediff = Abs<((src) % 8) - ((dest) % 8)>::value;
		 const static bool value = (rankdiff == 1 && filediff == 2) || (rankdiff == 2 && filediff == 1);
	 };
	 template<board_pos_t src, board_pos_t dest>
	 struct is_king_move {
		 const static int rankdiff = Abs<((src) / 8) - ((dest) / 8)>::value;
		 const static int filediff = Abs<((src) % 8) - ((dest) % 8)>::value;
		 const static bool value = rankdiff == 1 && filediff == 1;
	 };
	 template<board_pos_t src, board_pos_t dest>
	 struct is_diagright_move {
		 const static int rankdiff = ((src) / 8) - ((dest) / 8);
		 const static int filediff = ((src) % 8) - ((dest) % 8);
		 const static bool value = rankdiff == filediff && filediff != 0;
	 };
	 template<board_pos_t src, board_pos_t dest>
	 struct is_diagleft_move {
		 const static int rankdiff = ((src) / 8) - ((dest) / 8);
		 const static int filediff = ((src) % 8) - ((dest) % 8);
		 const static bool value = rankdiff == -filediff && filediff != 0;
	 };
	 template<board_pos_t src, board_pos_t dest>
	 struct is_vertical_move {
		 const static int filediff = Abs<(src % 8) - (dest % 8)>::value;
		 const static bool value = (filediff == 0) && src != dest;
	 };
	 template<board_pos_t src, board_pos_t dest>
	 struct is_horizontal_move {
		 const static int rankdiff = Abs<(src / 8) - (dest / 8)>::value;
		 const static bool value = rankdiff == 0 && src != dest;
	 };
	 template <bool is_white>
	 struct is_pawn_capture {
		 template<board_pos_t src, board_pos_t dest>
		 struct move {
			 const static int rankdiff = (dest / 8) - (src / 8);
			 const static int filediff = Abs<(src % 8) - (dest % 8)>::value;
			 const static bool value = filediff == 1 && rankdiff == (is_white ? 1 : -1);
		 };
	 };
	 template <bool is_white>
	 struct is_pawn_simple_move {
		 template <board_pos_t src, board_pos_t dest>
		 struct move {
			 const static int rankdiff = (dest / 8) - (src / 8);
			 const static int filediff = Abs<(src % 8) - (dest % 8)>::value;
			 const static bool value = filediff == 0 && rankdiff == (is_white ? 1 : -1);
		 };
	 };
	 template <bool is_white>
	 struct is_pawn_double_move {
		 template <board_pos_t src, board_pos_t dest>
		 struct move {
			 const static int rankdiff = (dest / 8) - (src / 8);
			 const static int filediff = Abs<(src % 8) - (dest % 8)>::value;
			 const static bool value = filediff == 0 && rankdiff == (is_white ? 2 : -2) && src / 8 == (is_white ? 1 : 6);
		 };
	 };
	 typedef generate_array<64, CreateBitboard<BitArrays::is_knight_move>::Generator>::result knight_moves;
	 typedef generate_array<64, CreateBitboard<BitArrays::is_king_move>::Generator>::result king_moves;
	 typedef generate_array<64, CreateBitboard<BitArrays::is_vertical_move>::Generator>::result vertical_moves;
	 typedef generate_array<64, CreateBitboard<BitArrays::is_horizontal_move>::Generator>::result horizontal_moves;
	 typedef generate_array<64, CreateBitboard<BitArrays::is_diagleft_move>::Generator>::result diagleft_moves;
	 typedef generate_array<64, CreateBitboard<BitArrays::is_diagright_move>::Generator>::result diagright_moves;
	 typedef generate_array<64, CreateBitboard<BitArrays::is_pawn_capture<true>::move >::Generator>::result white_pawn_captures;
	 typedef generate_array<64, CreateBitboard<BitArrays::is_pawn_capture<false>::move >::Generator>::result black_pawn_captures;
	 typedef generate_array<64, CreateBitboard<BitArrays::is_pawn_simple_move<true>::move >::Generator>::result white_pawn_simple_moves;
	 typedef generate_array<64, CreateBitboard<BitArrays::is_pawn_simple_move<false>::move >::Generator>::result black_pawn_simple_moves;
	 typedef generate_array<64, CreateBitboard<BitArrays::is_pawn_double_move<true>::move >::Generator>::result white_pawn_double_moves;
	 typedef generate_array<64, CreateBitboard<BitArrays::is_pawn_double_move<false>::move >::Generator>::result black_pawn_double_moves;

//	 typedef generate_array<64, CreateCollision> collision_table;
 };
 /*
 typedef generate_array<64, CreateBitboard<BitArrays::is_bishop_move>::Generator>::result bishop_moves;
 typedef generate_array<64, CreateBitboard<BitArrays::is_rook_move>::Generator>::result rook_moves;
 typedef generate_array<64, CreateBitboard<BitArrays::is_pawn_capture<true>::move>::Generator>::result white_pawn_captures;
 typedef generate_array<64, CreateBitboard<BitArrays::is_pawn_capture<false>::move>::Generator>::result black_pawn_captures;
*/

Bitboard::Bitboard()
{
	reset();
}

void Bitboard::reset()
{
	side_to_play = White;
	in_check = false;
	castle = 0x0f;
	enpassant_file = -1;
	move_count = 0;
	memset(piece_bitmasks, 0, sizeof(piece_bitmasks));
	memset(charboard, 0, sizeof(charboard));
	set_starting_position();
}

/**
raw is the natural moves that a piece could make if the board were empty.
legal are the spaces that would be legal, ignoring the possibility of intervening pieces.
stop_slide are squares that block moves
origin is the current location of the piece.
if stop_at_first_positive is set, then return a maximum of one result.  (eg for captures)
*/
uint64_t Bitboard::compute_slide_dest(uint64_t raw, uint64_t legal, uint64_t stop_slide, int origin, bool stop_at_first_positive) const
{
	uint64_t result = 0;
	uint64_t original_raw = raw;

	// positive direction
	uint64_t shift_amount = origin + 1;
	if (shift_amount == 64) {
		raw = 0;
	} else {
		raw = raw >> shift_amount;
	}
	while (raw) {
		int pos = ffsll(raw);
		uint64_t dest = pos + shift_amount - 1;
		shift_amount += pos;
		raw >>= pos;
		if (legal & (1ULL << dest)) {
			result |= (1ULL << dest);
			if (stop_at_first_positive) {
				break;
			}
		} else if (stop_slide & (1ULL << dest)) {
			break;
		}
	}

	// negative direction
	raw = original_raw & ((1ULL << origin) - 1);
	while (raw) {
		int dest = flsll(raw) - 1;
		uint64_t dest_bitmask = (1ULL << dest);
		raw &= ~dest_bitmask;
		if (legal & dest_bitmask) {
			result |= dest_bitmask;
			if (stop_at_first_positive) {
				break;
			}
		} else if (stop_slide & dest_bitmask) {
			break;
		}
	}
	return result;
}

void Bitboard::read_bitmask_moves(int src, uint64_t dest_bitmask, std::vector<move_t> &moves) const
{
	while (dest_bitmask) {
		int destpos = flsll(dest_bitmask) - 1;
		moves.push_back(make_move(src, destpos));

		dest_bitmask &= ~(1ULL << destpos);
	}
}

void Bitboard::get_slide_moves(uint64_t piece_bitmask, uint64_t empty, uint64_t capture_targets, uint64_t blockers, const uint64_t *piece_moves, std::vector<move_t> &moves) const
{
	while (piece_bitmask) {
		int src = flsll(piece_bitmask) - 1;
		piece_bitmask &= ~(1UL << src);
		// capture moves
		read_bitmask_moves(src, compute_slide_dest(piece_moves[src], capture_targets, blockers, src, true), moves);
		// non-capture moves
		read_bitmask_moves(src, compute_slide_dest(piece_moves[src], empty, capture_targets | blockers, src, false), moves);
	}
}

void Bitboard::get_point_moves(uint64_t piece_bitmask, uint64_t legal_dest, const uint64_t *piece_moves, std::vector<move_t> &moves) const
{
	while (piece_bitmask) {
		int src = flsll(piece_bitmask) - 1;
		piece_bitmask &= ~(1UL << src);

		uint64_t dest_bitmask = piece_moves[src] & legal_dest;
		while (dest_bitmask) {
			int destpos = flsll(dest_bitmask) - 1;
			moves.push_back(make_move(src, destpos));
			dest_bitmask &= ~(1UL << destpos);
		}
	}
}

void Bitboard::legal_moves(Color c_color, std::vector<move_t> &moves, piece_t limit_to_this_piece) const
{
	int color = (c_color == White) ? 0 : 8;
	int othercolor = (c_color == White) ? 8 : 0;
	uint64_t empty_squares = ~piece_bitmasks[ALL + color] & ~piece_bitmasks[ALL + othercolor];

	// knight moves
	get_point_moves(piece_bitmasks[KNIGHT + color], ~piece_bitmasks[ALL + color], BitArrays::knight_moves::data, moves);
	// king moves
	get_point_moves(piece_bitmasks[KING + color], ~piece_bitmasks[ALL + color], BitArrays::king_moves::data, moves);

	// pawn moves
	if (c_color == White) {
		get_point_moves(piece_bitmasks[PAWN + color], piece_bitmasks[ALL + othercolor], BitArrays::white_pawn_captures::data, moves);
		get_point_moves(piece_bitmasks[PAWN + color], empty_squares, BitArrays::white_pawn_simple_moves::data, moves);
		get_point_moves(piece_bitmasks[PAWN + color], empty_squares & ((empty_squares & 0xff0000) << 8), BitArrays::white_pawn_double_moves::data, moves);
	} else {
		get_point_moves(piece_bitmasks[PAWN + color], piece_bitmasks[ALL + !color], BitArrays::black_pawn_captures::data, moves);
		get_point_moves(piece_bitmasks[PAWN + color], empty_squares, BitArrays::black_pawn_simple_moves::data, moves);
		get_point_moves(piece_bitmasks[PAWN + color], empty_squares & ((empty_squares & 0xff0000000000) >> 8), BitArrays::black_pawn_double_moves::data, moves);
	}

	// rook/queen moves
	get_slide_moves(piece_bitmasks[ROOK + color] | piece_bitmasks[QUEEN + color], empty_squares, piece_bitmasks[ALL + othercolor], piece_bitmasks[ALL + color], BitArrays::horizontal_moves::data, moves);
	get_slide_moves(piece_bitmasks[ROOK + color] | piece_bitmasks[QUEEN + color], empty_squares, piece_bitmasks[ALL + othercolor], piece_bitmasks[ALL + color], BitArrays::vertical_moves::data, moves);

	// bishop/queen moves
	get_slide_moves(piece_bitmasks[BISHOP + color] | piece_bitmasks[QUEEN + color], empty_squares, piece_bitmasks[ALL + othercolor], piece_bitmasks[ALL + color], BitArrays::diagleft_moves::data, moves);
	get_slide_moves(piece_bitmasks[BISHOP + color] | piece_bitmasks[QUEEN + color], empty_squares, piece_bitmasks[ALL + othercolor], piece_bitmasks[ALL + color], BitArrays::diagright_moves::data, moves);
}

piece_t Bitboard::get_piece(unsigned char rank, unsigned char file) const
{
	int bp = rank * 8 + file;
	unsigned char twosquare = charboard[bp / 2];
	if (file % 2 == 0) {
		return twosquare & 0xf;
	} else {
		return (twosquare >> 4) & 0xf;
	}
}

void Bitboard::set_piece(unsigned char rank, unsigned char file, piece_t piece)
{
	int bp = rank * 8 + file;
	unsigned char twosquare = charboard[bp / 2];
	if (file % 2 == 0) {
		charboard[bp / 2] = (twosquare & 0xf0) | piece;
	} else {
		charboard[bp / 2] = (twosquare & 0x0f) | (piece << 4);
	}
}

bool Bitboard::king_in_check(Color) const
{
	return in_check;
}

void Bitboard::update() // called whenever the board is updated
{
	// FIXME set in_check
	memset(piece_bitmasks, 0, sizeof(piece_bitmasks));
	for (int i = 0; i < 32; i++) {
		unsigned char twosquare = charboard[i];
		if (twosquare & 0x7) {
			piece_bitmasks[twosquare & 0xf] |= (1ULL << (2*i));
		}
		if (twosquare & 0x70) {
			piece_bitmasks[twosquare >> 4] |= (1ULL << (2*i + 1));
		}
	}
	for (int color = 0; color <= 1; color++) {
		for (int piece = 1; piece <= 6; piece++) {
			piece_bitmasks[color * 8] |= piece_bitmasks[color * 8 + piece];
		}
	}
}

std::ostream &operator<<(std::ostream &os, const Bitboard &b)
{
	b.get_fen(os);
	return os;
}

/*
int main()
{
	Bitboard b;
	b.set_starting_position();
	b.get_fen(std::cout);
	std::cout << std::endl;
	std::vector<move_t> moves;
	for (int i = 0; i < 4; i++) {
		b.set_piece(1, i, EMPTY);
	}
	b.update();
	b.get_fen(std::cout);
	std::cout << std::endl;
	b.legal_moves(true, moves);
	for (std::vector<move_t>::iterator iter = moves.begin(); iter != moves.end(); iter++) {
		b.print_move(*iter, std::cout);
		std::cout << std::endl;
	}

	moves.clear();
	b.legal_moves(false, moves);
	for (std::vector<move_t>::iterator iter = moves.begin(); iter != moves.end(); iter++) {
		b.print_move(*iter, std::cout);
		std::cout << std::endl;
	}
	return 0;

}
*/
