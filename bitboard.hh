#ifndef BITBOARD_H_
#define BITBOARD_H_

#define HAS_FFSLL

#include <vector>
#include <cstdint>
#include <strings.h>

typedef unsigned char piece_t;
const piece_t bb_all = 0;
const piece_t bb_pawn = 1;
const piece_t bb_knight = 2;
const piece_t bb_bishop = 3;
const piece_t bb_rook = 4;
const piece_t bb_queen = 5;
const piece_t bb_king = 6;

enum Color { White = 0, Black = 1 };

static constexpr Color get_opposite_color(Color color) {
    return color == White ? Black : White;
}

typedef unsigned int move_t;
typedef unsigned char BoardPos;
const BoardPos InvalidPos = 8;
const int EMPTY = 0;
const int INVALID = 8;
const int PIECE_MASK = 7;
const int WhiteMask = 0;
const int BlackMask = 8;

const int MOVE_FROM_CHECK = 0x80000;
const int ENPASSANT_STATE_MASK = 0xf00000;
const int ENPASSANT_FLAG = 0x8000000;
const int INVALIDATES_CASTLE_K = 0x10000000;
const int INVALIDATES_CASTLE_Q = 0x20000000;

const int FL_ALL = 4;
const int FL_CAPTURES = 2;
const int FL_EMPTY = 1;

static constexpr Color get_color(piece_t piece) {
    return piece & 0x8 ? Black : White;
}

class Bitboard;

static constexpr piece_t make_piece(piece_t type, Color color)
{
    if (color == Black && type != EMPTY) {
        return type | BlackMask;
    } else {
        return type;
    }
}

class BitboardMoveIterator {
public:
    BitboardMoveIterator(Color color);
    virtual ~BitboardMoveIterator() {}
    move_t get_move(const Bitboard *board) const;
    void advance(const Bitboard *board);
    void push_move_front(move_t move);

    bool in_captures() const {
        return captures_promote_type & 0x8;
    }
    void set_promote_type(piece_t piece) {
        captures_promote_type = (captures_promote_type & 0x8) | (piece & PIECE_MASK);
    }

    int get_promote_type() const {
        return captures_promote_type & PIECE_MASK;
    }

    char start_pos;
    char colored_piece_type;
    char dest_pos;
    char captures_promote_type;
    bool processed_checks;

    uint64_t dest_squares;
    uint64_t covered_squares;
    uint64_t king_slide_blockers;

    move_t inserted_move = 0;
} ;


class Bitboard
{
    friend class BitboardMoveIterator;
    friend class SimpleBitboardEvaluation;
    friend class SimpleEvaluation;
    friend class NNUEEvaluation;
public:
    Bitboard();

    piece_t get_piece(unsigned char rank, unsigned char file) const;
    void set_piece(unsigned char rank, unsigned char file, piece_t);
    bool is_legal_move(move_t move, Color color) const;
    bool operator==(const Bitboard&) const = default;

    BitboardMoveIterator get_legal_moves(Color color) const {
        BitboardMoveIterator iter(color);
        iter.advance(this);
        return iter;
    }
    move_t get_next_move(BitboardMoveIterator &iter) const {
        move_t move = iter.get_move(this);
        iter.advance(this);
        return move;
    }
    bool has_more_moves(BitboardMoveIterator iter) const {
        return (iter.colored_piece_type & PIECE_MASK) <= bb_king;
    }
    bool king_in_check(Color) const;
    uint64_t get_bitmask(Color color, piece_t piece_type) const {
        return piece_bitmasks[color * (bb_king + 1) + (PIECE_MASK & piece_type)];
    }

    bool can_castle(Color color, bool kingside) const {
        int bit = get_castle_bit(color, kingside);
        return castle & (1 << bit);
    }
    void set_can_castle(Color color, bool kingside, bool can_castle) {
        int bit = get_castle_bit(color, kingside);
        if (can_castle) {
            castle |= (1 << bit);
        } else {
            castle &= ~(1 << bit);
        }
    }

protected:
    move_t make_move(unsigned char srcrank, unsigned char srcfile,
            unsigned char source_piece,
            unsigned char destrank, unsigned char destfile,
            unsigned char captured_piece, unsigned char promote) const;

    // called whenever the board is updated
    void update() {}

    Color side_to_play;
    short move_count;
    char enpassant_file;
    bool in_check;

private:
    int get_castle_bit(Color color, bool kingside) const {
        int bit = 0;
        if (kingside) {
            bit += 1;
        }
        if (color == Black) {
            bit += 2;
        }
        return bit;
    }

public:
    uint64_t piece_bitmasks[2 * (bb_king + 1)];
private:
    uint64_t attack_squares[64];
    char castle;
    void next_pnk_move(Color color, piece_t, int &start_pos, uint64_t &dest_squares, int fl_flags, bool checks_only, bool exclude_checks) const;
    void next_piece_slide(Color color, piece_t piece_type, int &start_pos, uint64_t &dest_squares, int fl_flags, uint64_t exclude_block_pieces=0, uint64_t exclude_source_pieces=0, uint64_t include_pieces=0, bool checks_only=false, bool exclude_checks=false) const;

    bool is_not_illegal_due_to_check(Color color, piece_t piece, int start_pos, int dest_pos, uint64_t covered_squares) const;
    bool discovers_check(int start_pos, int dest_pos, Color color, uint64_t covered_squares, bool inverted) const;
    uint64_t get_captures(Color color, piece_t piece_type, int start_pos) const;
    bool removes_check(piece_t piece_type, int start_pos, int dest_pos, Color color, uint64_t covered_squares) const;
    uint64_t computed_covered_squares(Color color, uint64_t exclude_pieces, uint64_t include_pieces, uint64_t exclude_block_pieces) const;

    uint64_t get_king_slide_blockers(int king_pos, Color king_color) const;

    const static uint64_t **rook_magic;
    const static uint64_t **bishop_magic;
};

const int Mod67BitPos[] = {
    0, 0, 1, 39, 2, 15, 40, 23, 3, 12, 16, 59, 41, 19, 24, 54, 4, 0, 13, 10, 17,
    62, 60, 28, 42, 30, 20, 51, 25, 44, 55, 47, 5, 32, 0, 38, 14, 22, 11, 58, 18,
    53, 63, 9, 61, 27, 29, 50, 43, 46, 31, 37, 21, 57, 52, 8, 26, 49, 45, 36, 56,
    7, 48, 35, 6, 34, 33

};

constexpr int count_bits(uint64_t bitset) {
    int c = 0;
    for (c = 0; bitset; c++) {
        bitset &= (bitset - 1);
    }
    return c;
}

constexpr int get_low_bit(uint64_t bitset, unsigned int start) {
    if (start > 0) {
        bitset = bitset >> start;
    }
    if (bitset == 0 || start >= 64) {
        return -1;
    }
    // https://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightBinSearch
#ifdef HAS_FFSLL
    return start + ffsll(bitset) - 1;
#else
    return start + Mod67BitPos[(bitset & -bitset) % 67];
#endif
}

constexpr uint64_t project_bitset(uint64_t bitset, uint64_t bitmask)
{
    uint64_t projection = 0;
    int location = 0;
    for (int i = 0; i < count_bits(bitmask); i++) {
        location = get_low_bit(bitmask, location);
        if (location == -1) {
            break;
        }
        if (bitset & (1ULL << i)) {
            projection |= (1ULL << location);
        }
        location++;
    }
    return projection;
}

uint64_t bishop_slide_moves(BoardPos src, uint64_t blockboard);
uint64_t rook_slide_moves(BoardPos src, uint64_t blockboard);

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

 template<template<BoardPos, BoardPos> class pred, size_t src, size_t dest>
 struct generate_bitboard {
     static const uint64_t value = (pred<src, dest-1>::value ? (static_cast<uint64_t>(1) << (dest-1)) : 0) | generate_bitboard<pred, src, dest-1>::value;
 };

 template<template<BoardPos, BoardPos> class pred, size_t src>
 struct generate_bitboard<pred, src, 0> {
     static const uint64_t value = 0;

 };

 template<template<BoardPos, BoardPos> class pred>
 struct CreateBitboard {
     template<size_t src>
     struct Generator {
         static const uint64_t value = generate_bitboard<pred, src, 64>::value;
     };
 };

template<size_t src>
struct RookBitCounter {
    static const int value = (src % 8 == 0 || src % 8 == 7 ? 6 : 5) + (src / 8 == 0 || src / 8 == 7 ? 6 : 5) + 1;
};

constexpr int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

template<size_t src>
struct BishopBitCounter {
    static const int value = max(0, 6 - max(src / 8, src % 8)) + max(0, 6 - max(7 - src/8, src %8)) + max(0, 6 - max(7 - src/8, 7 - src %8)) + max(0, 6 - max(src/8, 7 - src %8)) + 1;
};

 template<int x>
 struct Abs
 {
     const static int value = x < 0 ? -x : x;
 };

struct BitArrays {
     template<BoardPos src, BoardPos dest>
     struct is_knight_move {
         const static int rankdiff = Abs<((src) / 8) - ((dest) / 8)>::value;
         const static int filediff = Abs<((src) % 8) - ((dest) % 8)>::value;
         const static bool value = (rankdiff == 1 && filediff == 2) || (rankdiff == 2 && filediff == 1);
     };
     template<BoardPos src, BoardPos dest>
     struct is_king_move {
         const static int rankdiff = Abs<((src) / 8) - ((dest) / 8)>::value;
         const static int filediff = Abs<((src) % 8) - ((dest) % 8)>::value;
         const static bool value = rankdiff <= 1 && filediff <= 1 && (rankdiff > 0 || filediff > 0);
     };
     template<BoardPos src, BoardPos dest>
     struct is_rook_move {
         const static int rankdiff = (src / 8) - (dest / 8);
         const static int filediff = (src % 8) - (dest % 8);
         const static bool is_valid_move = (rankdiff == 0 || filediff == 0) && rankdiff != filediff;
         const static bool value = is_valid_move;
     };
     template<BoardPos src, BoardPos dest>
     struct is_bishop_move {
         const static int rankdiff = Abs<(src / 8) - (dest / 8)>::value;
         const static int filediff = Abs<(src % 8) - (dest % 8)>::value;
         const static bool is_valid_move = rankdiff == filediff && filediff != 0;
         const static bool value = is_valid_move;
     };


     template<BoardPos src, BoardPos dest>
     struct is_rook_blockboard {
         const static int rankdiff = (src / 8) - (dest / 8);
         const static int filediff = (src % 8) - (dest % 8);
         const static bool is_valid_move = (rankdiff == 0 || filediff == 0) && rankdiff != filediff;
         const static bool value = is_valid_move &&
            ((rankdiff == 0 && (dest % 8 > 0 && dest % 8 < 7)) || (filediff == 0 && (dest / 8 > 0 && dest / 8 < 7)));
     };

     template<BoardPos src, BoardPos dest>
     struct is_bishop_blockboard {
         const static int rankdiff = Abs<(src / 8) - (dest / 8)>::value;
         const static int filediff = Abs<(src % 8) - (dest % 8)>::value;
         const static bool is_valid_move = rankdiff == filediff && filediff != 0;
         const static bool value = is_valid_move && dest / 8 > 0 && dest / 8 < 7 && dest % 8 > 0 && dest % 8 < 7;
     };

     template <bool is_white>
     struct is_pawn_capture {
         template<BoardPos src, BoardPos dest>
         struct move {
             const static int rankdiff = (src / 8) - (dest / 8);
             const static int filediff = Abs<(src % 8) - (dest % 8)>::value;
             const static bool value = filediff == 1 && rankdiff == (is_white ? -1 : 1);
         };
     };

     template <bool is_white>
     struct is_pawn_move {
         template<BoardPos src, BoardPos dest>
         struct move {
             const static int rankdiff = (src / 8) - (dest / 8);
             const static int filediff = Abs<(src % 8) - (dest % 8)>::value;
             const static bool value = filediff == 0 && (rankdiff == (is_white ? -1 : 1));
         };
     };

public:
     typedef generate_array<64, CreateBitboard<BitArrays::is_knight_move>::Generator>::result knight_moves;
     typedef generate_array<64, CreateBitboard<BitArrays::is_king_move>::Generator>::result king_moves;
     typedef generate_array<64, CreateBitboard<BitArrays::is_bishop_move>::Generator>::result bishop_moves;
     typedef generate_array<64, CreateBitboard<BitArrays::is_rook_move>::Generator>::result rook_moves;
     typedef generate_array<64, CreateBitboard<BitArrays::is_rook_blockboard>::Generator>::result rook_blockboard;
     typedef generate_array<64, CreateBitboard<BitArrays::is_bishop_blockboard>::Generator>::result bishop_blockboard;
     typedef generate_array<64, CreateBitboard<BitArrays::is_pawn_capture<true>::move >::Generator>::result pawn_captures_white;
     typedef generate_array<64, CreateBitboard<BitArrays::is_pawn_capture<false>::move >::Generator>::result pawn_captures_black;
     typedef generate_array<64, CreateBitboard<BitArrays::is_pawn_move<true>::move >::Generator>::result pawn_moves_white;
     typedef generate_array<64, CreateBitboard<BitArrays::is_pawn_move<false>::move >::Generator>::result pawn_moves_black;

     typedef generate_array<64, RookBitCounter>::result rook_bitboard_bitcount;
     typedef generate_array<64, BishopBitCounter>::result bishop_bitboard_bitcount;

 };

void display_bitboard(uint64_t n, int rank, int file);


#endif
