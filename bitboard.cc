#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include "bitboard.hh"

move_t make_move(board_pos_t src, board_pos_t dest)
{
	return (static_cast<int>(dest) << 6) | src;
}
	
enum bitboard_pieces { pawn, knight, bishop, rook, queen, king, all };

// bitboard: &1 = a1, &2 = b1, &4=c1, etc.
const uint64_t bitboard_initialization[] = { 0xff00, 0x42, 0x24, 0x81, 0x08, 0x10, 0xffff,
	0xffL << 48, 0x42L << 56, 0x24L << 56, 0x81L << 56, 0x8L << 56, 0x10L << 56, 0xffffL << 48 };

const char charboard_initialization[] = { 4, 2, 3, 5, 6, 3, 2, 4,
	 						 1, 1, 1, 1, 1, 1, 1, 1,
						 	 0, 0, 0, 0, 0, 0, 0, 0,
						 	 0, 0, 0, 0, 0, 0, 0, 0,
						 	 0, 0, 0, 0, 0, 0, 0, 0,
						 	 0, 0, 0, 0, 0, 0, 0, 0,
							 9, 9, 9, 9, 9, 9, 9, 9,
							 0xc, 0xa, 0xb, 0xd, 0xe, 0xb, 0xa, 0xc
						 };
/*						 
 const char charboard_initialization_nibble[] = { 0x42, 0x35, 0x63, 0x24,
 	0x11, 0x11, 0x11, 0x11,
 	0, 0, 0, 0,
 	0, 0, 0, 0,
 	0, 0, 0, 0,
 	0, 0, 0, 0,
 	0x99, 0x99, 0x99, 0x99,
 	0xca, 0xbd, 0xeb, 0xac
 };
*/						 
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
	 struct is_rook_move {
		 const static int rankdiff = Abs<(src / 8) - (dest / 8)>::value;
		 const static int filediff = Abs<(src % 8) - (dest % 8)>::value;
		 const static bool value = (rankdiff == 0 || filediff == 0) && rankdiff != filediff;
	 };
	 template<board_pos_t src, board_pos_t dest>
	 struct is_bishop_move {
		 const static int rankdiff = Abs<(src / 8) - (dest / 8)>::value;
		 const static int filediff = Abs<(src % 8) - (dest % 8)>::value;
		 const static bool value = rankdiff == filediff && filediff != 0;	 	
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
	: side_to_play(White), 	
	  in_check(false),
		castle(0x0f),
		enpassant_target(-1),
		move_count(0)

{
	memcpy(piece_bitmasks, bitboard_initialization, sizeof(bitboard_initialization));
	memcpy(charboard, charboard_initialization, sizeof(charboard_initialization));
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
	int color = is_white ? 0 : 7;
	int othercolor = is_white ? 7 : 0;
	get_point_moves(piece_bitmasks[knight + color], ~piece_bitmasks[all + color], BitArrays::knight_moves::data, moves);
	get_point_moves(piece_bitmasks[king + color], ~piece_bitmasks[all + color], BitArrays::king_moves::data, moves);
	uint64_t empty_squares = ~piece_bitmasks[all + color] & ~piece_bitmasks[all + othercolor];

	if (is_white) {
		get_point_moves(piece_bitmasks[pawn + color], piece_bitmasks[all + othercolor], BitArrays::white_pawn_captures::data, moves);
		get_point_moves(piece_bitmasks[pawn + color], empty_squares, BitArrays::white_pawn_simple_moves::data, moves);
		get_point_moves(piece_bitmasks[pawn + color], empty_squares & ((empty_squares & 0xff0000) << 8), BitArrays::white_pawn_double_moves::data, moves);
	} else {
		get_point_moves(piece_bitmasks[pawn + color], piece_bitmasks[all + !color], BitArrays::black_pawn_captures::data, moves);
		get_point_moves(piece_bitmasks[pawn + color], empty_squares, BitArrays::black_pawn_simple_moves::data, moves);
		get_point_moves(piece_bitmasks[pawn + color], empty_squares & ((empty_squares & 0xff0000000000) >> 8), BitArrays::black_pawn_double_moves::data, moves);
	}
}

void Bitboard::print_pos(char pos, std::ostream &os) const
{
	os << static_cast<char>((pos % 8) + 'a') << static_cast<char>((pos / 8) + '1');
}

void Bitboard::print_move(move_t move, std::ostream &os) const
{
	char dest = (move >> 6);
	char src = (move & 0x3f);
	print_pos(src, os);
	os << "-";
	print_pos(dest, os);
}

int main()
{
	Bitboard b;
	std::vector<move_t> moves;
	
	b.legal_moves(true, moves);
	for (std::vector<move_t>::iterator iter = moves.begin(); iter != moves.end(); iter++) {
		b.print_move(*iter, std::cout);
		std::cout << std::endl;
	}
	return 0;
}
