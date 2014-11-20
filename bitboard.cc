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
	 static const uint64_t value = (pred<src, dest-1>::value ? (1 << (dest-1)) : 0) | generate_bitboard<pred, src, dest-1>::value;

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
	 struct is_vertical_move {
		 const static int rankdiff = Abs<(src / 8) - (dest / 8)>::value;
		 const static int filediff = Abs<(src % 8) - (dest % 8)>::value;
		 const static bool value = (filediff == 0) && src != dest;
	 };
	 template<board_pos_t src, board_pos_t dest>
	 struct is_horizontal_move {
		 const static int rankdiff = Abs<(src / 8) - (dest / 8)>::value;
		 const static int filediff = Abs<(src % 8) - (dest % 8)>::value;
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
}

void Bitboard::get_point_moves(uint64_t piece_bitmask, uint64_t legal_dest, const uint64_t *piece_moves, std::vector<move_t> &moves) const
{
	int shift_amount = 0;
		
	while (piece_bitmask) {
		int pos = ffsll(piece_bitmask);
		if (pos == 0) {
			break;
		}
		int src = pos + shift_amount - 1;
		shift_amount += pos;
		piece_bitmask = piece_bitmask >> pos;
		
		uint64_t dest_bitmask = piece_moves[src] & legal_dest;
		int dest_shift_amount = 0;
		while (dest_bitmask) {
			int destpos = ffsll(dest_bitmask);
			if (destpos == 0) {
				break;
			}
			moves.push_back(make_move(src, destpos + dest_shift_amount - 1));
			dest_shift_amount += destpos;
			dest_bitmask = dest_bitmask >> destpos;
		}
	}
}

void Bitboard::legal_moves(bool is_white, std::vector<move_t> &moves) const
{
	int color = is_white ? 0 : 8;
	int othercolor = is_white ? 8 : 0;
	get_point_moves(piece_bitmasks[KNIGHT + color], ~piece_bitmasks[ALL + color], BitArrays::knight_moves::data, moves);
	get_point_moves(piece_bitmasks[KING + color], ~piece_bitmasks[ALL + color], BitArrays::king_moves::data, moves);
	uint64_t empty_squares = ~piece_bitmasks[ALL + color] & ~piece_bitmasks[ALL + othercolor];

	if (is_white) {
		get_point_moves(piece_bitmasks[PAWN + color], piece_bitmasks[ALL + othercolor], BitArrays::white_pawn_captures::data, moves);
		get_point_moves(piece_bitmasks[PAWN + color], empty_squares, BitArrays::white_pawn_simple_moves::data, moves);
		get_point_moves(piece_bitmasks[PAWN + color], empty_squares & ((empty_squares & 0xff0000) << 8), BitArrays::white_pawn_double_moves::data, moves);
	} else {
		get_point_moves(piece_bitmasks[PAWN + color], piece_bitmasks[ALL + !color], BitArrays::black_pawn_captures::data, moves);
		get_point_moves(piece_bitmasks[PAWN + color], empty_squares, BitArrays::black_pawn_simple_moves::data, moves);
		get_point_moves(piece_bitmasks[PAWN + color], empty_squares & ((empty_squares & 0xff0000000000) >> 8), BitArrays::black_pawn_double_moves::data, moves);
	}
}

piece_t Bitboard::get_piece(unsigned char rank, unsigned char file) const
{
	int bp = rank * 8 + file;
	unsigned char twosquare = charboard[bp / 2];
	if (file % 1 == 0) {
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

int Bitboard::get_castle_bit(Color color, bool kingside) const
{
	int bit = 0;
	if (kingside) {
		bit += 1;
	}
	if (color == Black) {
		bit += 2;
	}
	return bit;
}


bool Bitboard::can_castle(Color color, bool kingside) const
{
	int bit = get_castle_bit(color, kingside);
	return castle & (1 << bit);
}


int main()
{
	FenBoard<Bitboard> b;
	b.set_starting_position();
	b.get_fen(std::cout);
	std::cout << std::endl;
	std::vector<move_t> moves;
	for (int i = 0; i < 4; i++) {
		b.set_piece(1, i, EMPTY);
	}
	b.update();
	
	b.legal_moves(true, moves);
	for (std::vector<move_t>::iterator iter = moves.begin(); iter != moves.end(); iter++) {
		b.print_move(*iter, std::cout);
		std::cout << std::endl;
	}
	return 0;
}
