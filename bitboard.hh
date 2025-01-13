#ifndef BITBOARD_H_
#define BITBOARD_H_

#define HAS_FFSLL

#include <vector>
#include <map>
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


constexpr int sign(int value) {
    if (value > 0) {
        return 1;
    } else {
        return -1;
    }
}

constexpr int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
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

class Bitboard
{
    friend class SimpleBitboardEvaluation;
    friend class SimpleEvaluation;
    friend class NNUEEvaluation;
    friend class MoveSorter;
public:
    Bitboard();

    piece_t get_piece(unsigned char rank, unsigned char file) const;
    void set_piece(unsigned char rank, unsigned char file, piece_t);
    bool is_legal_move(move_t move, Color color) const;
    bool operator==(const Bitboard&) const = default;
    void get_moves(Color side_to_play, bool checks, bool captures_or_promo, std::vector<move_t> &moves) const;

    bool king_in_check(Color) const;
    uint64_t get_bitmask(Color color, piece_t piece_type) const {
        return piece_bitmasks[color * (bb_king + 1) + (PIECE_MASK & piece_type)];
    }

    bool can_castle(Color color, bool kingside) const {
        int bit = get_castle_bit(color, kingside);
        return castle & (1 << bit);
    }
    void set_can_castle(Color color, bool kingside, bool enabled) {
        int bit = get_castle_bit(color, kingside);
        if (enabled) {
            castle |= (1 << bit);
        } else {
            castle &= ~(1 << bit);
        }
        if (can_castle(color, kingside) != enabled) {
            update_zobrist_hashing_castle(color, kingside, enabled);
        }
    }

    Color get_side_to_play() const { return side_to_play; }
    void set_side_to_play(Color stp) {
        if (stp != side_to_play) {
            update_zobrist_hashing_move();
        }
        side_to_play = stp;
    }
    char get_enpassant_file() const {
        return enpassant_file;
    }

    void set_enpassant_file(char newfile) {
        if (newfile != enpassant_file) {
            update_zobrist_hashing_enpassant(enpassant_file, false);
            update_zobrist_hashing_enpassant(newfile, true);
        }
        enpassant_file = newfile;
    }

protected:
    move_t make_move(unsigned char srcrank, unsigned char srcfile,
            unsigned char source_piece,
            unsigned char destrank, unsigned char destfile,
            unsigned char captured_piece, unsigned char promote) const;

    // for non-capture non-promote
    void make_moves(std::vector<move_t> &dest,
            int srcfile,
            unsigned char source_piece,
            uint64_t dest_squares) const;

    // called whenever the board is updated
    void update() {}

    short move_count;
    bool in_check;

private:
    Color side_to_play;
    char enpassant_file;
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
    void computed_covered_squares_b(Color color, uint64_t exclude_pieces, uint64_t include_pieces, uint64_t exclude_block_pieces, int times, uint64_t *covered_squares) const;
private:
    uint64_t attack_squares[64];
    char castle;
    void next_pnk_move(Color color, piece_t, int &start_pos, uint64_t &dest_squares, int fl_flags, bool checks_only, bool exclude_checks, bool include_promo=false, bool exclude_promo=false) const;
    void next_piece_slide(Color color, piece_t piece_type, int &start_pos, uint64_t &dest_squares, int fl_flags, uint64_t exclude_block_pieces=0, uint64_t exclude_source_pieces=0, uint64_t include_pieces=0, bool checks_only=false, bool exclude_checks=false) const;

    bool is_not_illegal_due_to_check(Color color, piece_t piece, int start_pos, int dest_pos, uint64_t covered_squares) const;
    bool discovers_check(int start_pos, int dest_pos, Color color, uint64_t covered_squares, bool inverted) const;
    uint64_t get_captures(Color color, piece_t piece_type, int start_pos) const;
    bool removes_check(piece_t piece_type, int start_pos, int dest_pos, Color color, uint64_t covered_squares) const;
    uint64_t computed_covered_squares(Color color, uint64_t exclude_pieces, uint64_t include_pieces, uint64_t exclude_block_pieces, int times=1) const;

    uint64_t get_king_slide_blockers(int king_pos, Color king_color) const;

    uint64_t square_attackers(int dest, Color color) const;
    uint64_t removes_check_dest(piece_t piece_type, int start_pos, uint64_t dest_squares, Color color, uint64_t covered_squares, uint64_t attackers) const;
    uint64_t remove_discovered_checks(piece_t piece_type, int start_pos, uint64_t dest_squares, Color color, uint64_t covered_squares) const;
    uint64_t get_blocking_squares(int src, int dest) const;
    uint64_t get_bishop_moves(int start_pos, uint64_t blockers) const;
    uint64_t get_rook_moves(int start_pos, uint64_t blockers) const;

    const static uint64_t **rook_magic;
    const static uint64_t **bishop_magic;


public:
    void apply_move(move_t);
    void undo_move(move_t);
    int times_seen() {
        auto iter = seen_positions.find(hash);
        if (iter != seen_positions.end()) {
            return iter->second;
        } else {
            return 0;
        }
    }
    uint64_t get_hash() const { return hash; }

protected:
    std::map<uint64_t, int> seen_positions;
private:
    uint64_t hash;
    void update_zobrist_hashing_piece(unsigned char rank, unsigned char file, piece_t piece, bool adding);
    void update_zobrist_hashing_castle(Color, bool kingside, bool enabling);
    void update_zobrist_hashing_move();
    void update_zobrist_hashing_enpassant(int file, bool enabling);

};

void display_bitboard(uint64_t n, int rank, int file);


#endif
