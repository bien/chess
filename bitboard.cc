#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include "bitboard.hh"
#include "fenboard.hh"
#include "move.hh"
#include <cassert>
#include <type_traits>

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

 // FIX: switch from 8-bit to 6-bit board positions, add source piece, move-gives-check
 // for non-capture non-promote
void Bitboard::make_moves(std::vector<move_t> &dest,
         int source_pos,
         unsigned char source_piece,
         uint64_t dest_squares) const
{
//    move_t move = srcrank << 12 | srcfile << 8 | destrank << 4 | destfile;
    move_t move = (source_pos / 8) << 12 | (source_pos % 8) << 8;

    // save misc state
    if (in_check) {
        move |= MOVE_FROM_CHECK;
    }
    if (enpassant_file != -1) {
        move |= (0xf & enpassant_file) << 20;
    } else {
        move |= ENPASSANT_STATE_MASK;
    }
    move_t invalidates_castle = 0;
    if ((source_piece & PIECE_MASK) == bb_king) {
        if (can_castle(get_color(source_piece), true)) {
            invalidates_castle |= INVALIDATES_CASTLE_K;
        }
        if (can_castle(get_color(source_piece), false)) {
            invalidates_castle |= INVALIDATES_CASTLE_Q;
        }
    }
    else if ((source_piece & PIECE_MASK) == bb_rook && can_castle(get_color(source_piece), false) && source_pos % 8 == 0) {
        invalidates_castle |= INVALIDATES_CASTLE_Q;
    }
    else if ((source_piece & PIECE_MASK) == bb_rook && can_castle(get_color(source_piece), true) && source_pos % 8 == 7) {
        invalidates_castle |= INVALIDATES_CASTLE_K;
    }
    move |= invalidates_castle;

    int dest_pos = -1;
    while ((dest_pos = get_low_bit(dest_squares, dest_pos+1)) >= 0) {
        move_t copied = move;
        copied |= (dest_pos / 8) << 4 | (dest_pos % 8);
        if ((source_piece & PIECE_MASK) == bb_pawn && (source_pos % 8) != (dest_pos % 8)) {
            assert(false);
        }
        dest.push_back(copied);
    }
}

move_t Bitboard::make_move(unsigned char srcrank, unsigned char srcfile,
        unsigned char source_piece,
        unsigned char destrank, unsigned char destfile,
        unsigned char captured_piece, unsigned char promote) const
{
    move_t move = srcrank << 12 | srcfile << 8 | destrank << 4 | destfile;
    move |= (promote & PIECE_MASK) << 24;
    move |= (captured_piece & PIECE_MASK) << 16;
    if (((source_piece & PIECE_MASK) == bb_pawn) && (srcfile != destfile) && (this->get_piece(destrank, destfile) == EMPTY)) {
        move |= ENPASSANT_FLAG;
    }

    // save misc state
    if (in_check) {
        move |= MOVE_FROM_CHECK;
    }
    if (enpassant_file != -1) {
        move |= (0xf & enpassant_file) << 20;
    } else {
        move |= ENPASSANT_STATE_MASK;
    }
    move_t invalidates_castle = 0;
    if ((source_piece & PIECE_MASK) == bb_king) {
        if (can_castle(get_color(source_piece), true)) {
            invalidates_castle |= INVALIDATES_CASTLE_K;
        }
        if (can_castle(get_color(source_piece), false)) {
            invalidates_castle |= INVALIDATES_CASTLE_Q;
        }
    }
    else if ((source_piece & PIECE_MASK) == bb_rook && can_castle(get_color(source_piece), false) && srcfile == 0) {
        invalidates_castle |= INVALIDATES_CASTLE_Q;
    }
    else if ((source_piece & PIECE_MASK) == bb_rook && can_castle(get_color(source_piece), true) && srcfile == 7) {
        invalidates_castle |= INVALIDATES_CASTLE_K;
    }
    move |= invalidates_castle;

    return move;
}

//constexpr
uint64_t bishop_slide_moves(BoardPos src, uint64_t blockboard) {
    uint64_t x = 0;
    for (int dr = -1; dr <= 1; dr += 2) {
        for (int df = -1; df <= 1; df += 2) {
            for (int n = 1; n < 8; n++) {
                int r = src / 8 + dr * n;
                int f = src % 8 + df * n;
                int i = r * 8 + f;
                if (r < 0 || r >= 8 || f < 0 || f >= 8) {
                    break;
                }
                x |= (1ULL << i);
                if (blockboard & (1ULL << i)) {
                    break;
                }
            }
        }
    }
    return x;
}

uint64_t rook_slide_moves(BoardPos src, uint64_t blockboard) {
    // up
    uint64_t n = 0;

    for (int i = src + 8; i < 64; i+= 8) {
        n |= (1ULL << i);
        if (blockboard & (1ULL << i)) {
            break;
        }
    }
    for (int i = src + 1; i < 8 * (src / 8 + 1); i += 1) {
        n |= (1ULL << i);
        if (blockboard & (1ULL << i)) {
            break;
        }
    }
    for (int i = src - 8; i >= 0; i-= 8) {
        n |= (1ULL << i);
        if (blockboard & (1ULL << i)) {
            break;
        }
    }

    for (int i = src - 1; i >= 8 * (src / 8); i-= 1) {
        n |= (1ULL << i);
        if (blockboard & (1ULL << i)) {
            break;
        }
    }
    return n;
}

void display_bitboard(uint64_t n, int rank, int file)
{
    for (int i = 7; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
            if (rank == i && j == file) {
                printf("P");
            } else if (n & (static_cast<uint64_t>(1) << (i * 8 + j))) {
                printf("X");
            } else {
                printf("O");
            }
        }
        printf("\n");
    }
}


BoardPos bb_make_board_pos(int rank, int file) {
    return rank * 8 + file;
}

std::string bp_to_string(BoardPos bp)
{
    std::string s;
    s += static_cast<char>('a' + (bp % 8));
    s += static_cast<char>('1' + (bp / 8));
    return s;
}

// see https://www.chessprogramming.org/Magic_Bitboards and magicsquares.cc
constexpr uint64_t lateral_slide_magic_factor[] = {
    0x35fdab5668f5011, 0x6aa948ad66b11ee1, 0x7c912a277579ff35, 0x6b4b4b442c29af1d, 0x1001021e61c77001, 0x7817e7e762d1304d, 0x1b7581f77f908249, 0x17a400105c143c3d,
    0x7ab014ca25f5899d, 0x661236234996fb, 0x23a79dde279aaa99, 0x54c1ffb13e12e69, 0x43abb0030e54dfff, 0x1273000866242007, 0x459db9c336813a63, 0x53b01c417b76bf9d,
    0x7fccc1996c70ab6f, 0x76de788c255032f3, 0x55a61e275e4e2615, 0x31e0e0023b7e3a75, 0x2ea3dba814584cff, 0x3e52a24f4469123f, 0x4e864af148d2d455, 0xd1eb533640cda9d,
    0x1c71c5ba2dbb6ead, 0x546247234208035, 0x6248ede1e35be85, 0x131771ab4b4ac7c3, 0x26c44044408eca8d, 0xfe42865091052f7, 0x1cc0b34f0019e8c5, 0x59d1f5665e620051,
    0x6bc95b9a45348ec3, 0x67b17060276819c7, 0x3ab126086d2d36ad, 0x138380d531bb67f3, 0x6af14ef63ababaaf, 0x31b645e810f0f103, 0x45e7c35538e56af1, 0x3d241f026c36f07d,
    0x3459e94e6912b3cb, 0x413a28bf2740e80b, 0x693c28d2c76f6f7, 0x6f7b27c81b17e4fd, 0x768a0afd7db00117, 0x6fd6026e005a000b, 0x64886797415bdb53, 0x2fdd7fc32ce19ae5,
    0x57e6636722ad56b, 0x282547f37411852b, 0xd530cf734e61047, 0x400553d440976897, 0x525c7b3f269e6fff, 0x7ad892242e9336d9, 0x35d94a2b0242909b, 0x3011b06242ce7e83,
    0x6df47ffd76feb769, 0x4b204069400292a1, 0x6897f04820017825, 0x173550010098608d, 0x1012580059172c17, 0x280a180a098c0035, 0x236f18890c0a00c1, 0x1cd88f2871994861,
};

constexpr uint64_t diag_slide_magic_factor[] = {
    0x2e51637d05f70c79, 0x5aa0cdaf632af055, 0x3a34e741750be7a5, 0x359ae4095d15071d, 0x317f3740608e6567, 0x21d2037c74ef1c4f, 0x569c3bea52d18b41, 0x77cbb43107f05b9,
    0x2fe9bb2a2a05588b, 0x645974ad5847dbf3, 0x140f3a7b1f3d042f, 0x2b35fac20c2c25a1, 0x19f17c7c1a8e2441, 0x10b62aee762c7d9d, 0x40410ae906fdb9c9, 0x2c12b9e93ec1139b,
    0x74e40fb142d06ef1, 0x3e1571b136472cd5, 0x2ab6296c4a257719, 0x60a6cf7271d1a9bb, 0x4241a5a7333820a1, 0x7b97f4b440c04e7d, 0x5c68a02f285748fd, 0x78a2d6160fb1c089,
    0x6f6ebe325f22f47d, 0x4fe34b741e38cbc3, 0x1568b20964026183, 0x75b6e8012460a239, 0x25ec2c81438e3535, 0x13f6f07d4feffd85, 0x5f106b647511f7d1, 0x3afe9c8e231b71c3,
    0x47baa577407150db, 0x14c66a3817371ac9, 0x4258922b065608c5, 0x54ff06007fd7ff7f, 0x3eb2f5a45d2c8ba3, 0x52ecc33da36c23, 0x112365962ae9a115, 0x74c203a12837a383,
    0xe52869f68c14ba1, 0x523b0a4b09f86c57, 0x39b6fd082af0d5f9, 0x2a675e0119d0c38d, 0x3ef24fae0057bfd7, 0x64f9e40d11813f5d, 0x7752cee53c6ab735, 0x4fc2da202ab8a0c7,
    0x6558a2561ebf6d19, 0x3cae48f571ef179d, 0x7d98f7d42beecd27, 0x12ace70d78dc49a9, 0x2446ab1537236f0d, 0x423e1a2b0fe26aa7, 0x5f38c51c3566cfc1, 0x3223bd877a337de7,
    0x4242ab144471e9db, 0x25a85e8873b58743, 0x6d26bb8377861c65, 0x576aed2626567297, 0x25522ea14b852c77, 0x7d7e6d8d5a59afc1, 0x2b8affb124f757a1, 0x56da1bad028ad193,
};

struct BitboardCaptures {
    const static uint64_t *PregeneratedCapturesWhite[7];
    const static uint64_t *PregeneratedCapturesBlack[7];
    const static uint64_t **PregeneratedCaptures[2];

    const static uint64_t *PregeneratedMovesWhite[7];
    const static uint64_t *PregeneratedMovesBlack[7];
    const static uint64_t *PregeneratedMoves[15];

};

const uint64_t *BitboardCaptures::PregeneratedCapturesWhite[7] = {
     NULL, BitArrays::pawn_captures_white::data, BitArrays::knight_moves::data, NULL, NULL, NULL, BitArrays::king_moves::data
};
const uint64_t *BitboardCaptures::PregeneratedCapturesBlack[7] = {
     NULL, BitArrays::pawn_captures_black::data, BitArrays::knight_moves::data, NULL, NULL, NULL, BitArrays::king_moves::data
};

const uint64_t **BitboardCaptures::PregeneratedCaptures[2] = {
   BitboardCaptures::PregeneratedCapturesWhite,
   BitboardCaptures::PregeneratedCapturesBlack
};

const uint64_t *BitboardCaptures::PregeneratedMovesWhite[7] = {
     NULL, BitArrays::pawn_moves_white::data, BitArrays::knight_moves::data, NULL, NULL, NULL, BitArrays::king_moves::data
};
const uint64_t *BitboardCaptures::PregeneratedMovesBlack[7] = {
     NULL, BitArrays::pawn_moves_black::data, BitArrays::knight_moves::data, NULL, NULL, NULL, BitArrays::king_moves::data
};

const uint64_t *BitboardCaptures::PregeneratedMoves[15] = {
    NULL, BitArrays::pawn_moves_white::data, BitArrays::knight_moves::data, NULL, NULL, NULL, BitArrays::king_moves::data, NULL,
    NULL, BitArrays::pawn_moves_black::data, BitArrays::knight_moves::data, NULL, NULL, NULL, BitArrays::king_moves::data
};

static uint64_t **initialize_rook_magic()
{
    uint64_t **rook_magic = new uint64_t *[64];
    for (int start_pos = 0; start_pos < 64; start_pos++) {

        int array_size_nbits = BitArrays::rook_bitboard_bitcount::data[start_pos];
        rook_magic[start_pos] = new uint64_t[1ULL << array_size_nbits];
        memset(rook_magic[start_pos], 0, sizeof(uint64_t) * (1ULL << array_size_nbits));
        uint64_t block_board = BitArrays::rook_blockboard::data[start_pos];
        uint64_t magic_factor = lateral_slide_magic_factor[start_pos];
        for (unsigned int i = 0; i < (1ULL << (array_size_nbits - 1)); i++) {
            uint64_t projection = project_bitset(i, block_board);
            uint64_t hash_index = (projection * magic_factor)  >> (64 - array_size_nbits);
            uint64_t resolved = rook_slide_moves(start_pos, projection);
            assert(rook_magic[start_pos][hash_index] == 0 || rook_magic[start_pos][hash_index] == resolved);
            rook_magic[start_pos][hash_index] = resolved;
        }
    }
    return rook_magic;
}



static uint64_t **initialize_bishop_magic()
{
    uint64_t **bishop_magic = new uint64_t *[64];
    for (int start_pos = 0; start_pos < 64; start_pos++) {

        int array_size_nbits = BitArrays::bishop_bitboard_bitcount::data[start_pos];
        bishop_magic[start_pos] = new uint64_t[1ULL << array_size_nbits];
        memset(bishop_magic[start_pos], 0, sizeof(uint64_t) * (1ULL << array_size_nbits));
        uint64_t block_board = BitArrays::bishop_blockboard::data[start_pos];
        uint64_t magic_factor = diag_slide_magic_factor[start_pos];
        for (unsigned int i = 0; i < (1ULL << (array_size_nbits - 1)); i++) {
            uint64_t projection = project_bitset(i, block_board);
            uint64_t hash_index = (projection * magic_factor)  >> (64 - array_size_nbits);
            uint64_t resolved = bishop_slide_moves(start_pos, projection);
            assert(bishop_magic[start_pos][hash_index] == 0 || bishop_magic[start_pos][hash_index] == resolved);
            bishop_magic[start_pos][hash_index] = resolved;
        }
    }
    return bishop_magic;
}

const uint64_t **Bitboard::rook_magic = const_cast<const uint64_t **>(initialize_rook_magic());
const uint64_t **Bitboard::bishop_magic = const_cast<const uint64_t **>(initialize_bishop_magic());


Bitboard::Bitboard()
    : hash(0)
{
    memset(piece_bitmasks, 0, sizeof(piece_bitmasks));
    enpassant_file = -1;
    for (int i = 0; i < 64; i++) {
        attack_squares[i] = 0;
        attack_squares[i] |= BitboardCaptures::PregeneratedCaptures[White][bb_pawn][i];
        attack_squares[i] |= BitboardCaptures::PregeneratedCaptures[Black][bb_pawn][i];
        attack_squares[i] |= BitArrays::knight_moves::data[i];
        attack_squares[i] |= BitArrays::king_moves::data[i];
        attack_squares[i] |= rook_slide_moves(i, 0);
        attack_squares[i] |= bishop_slide_moves(i, 0);
    }
}

piece_t Bitboard::get_piece(unsigned char rank, unsigned char file) const
{
    uint64_t test = 1ULL << bb_make_board_pos(rank, file);
    for (int i = 0; i < 2; i++) {
        Color color = static_cast<Color>(i);
        if (piece_bitmasks[color * (bb_king + 1) + bb_all] & test) {
            for (int piece_type = bb_pawn; piece_type <= bb_king; piece_type++) {
                if (piece_bitmasks[color * (bb_king + 1) + piece_type] & test) {
                    return make_piece(piece_type, color);
                }
            }
        }
    }
    return 0;
}

void Bitboard::set_piece(unsigned char rank, unsigned char file, piece_t piece)
{
    uint64_t bit = 1ULL << bb_make_board_pos(rank, file);

    // remove old piece if any
    piece_t current_piece = get_piece(rank, file);
    if (current_piece > 0) {
        for (int i = 0; i < bb_all + 2 * (bb_king + 1); i++) {
            piece_bitmasks[i] &= ~bit;
        }
        update_zobrist_hashing_piece(rank, file, current_piece, false);
    }
    // add new piece if any
    if (piece != EMPTY) {
        int piece_type = piece & PIECE_MASK;
        Color color = (piece & BlackMask) == BlackMask ? Black : White;

        piece_bitmasks[color * (bb_king + 1) + bb_all] |= bit;
        piece_bitmasks[color * (bb_king + 1) + piece_type] |= bit;

        update_zobrist_hashing_piece(rank, file, piece, true);
    }
}

void Bitboard::next_pnk_move(Color color, piece_t piece_type, int &start_pos, uint64_t &dest_squares, int fl_flags, bool checks_only, bool exclude_checks, bool include_promo, bool exclude_promo) const
{
	/**
	FL_CAPTURES: consider attacks against enemy pieces
	FL_EMPTY: consider moves to empty squares
	FL_ALL: consider attacks against own side's pieces (defending own piece)
	*/
    uint64_t actors = piece_bitmasks[color * (bb_king + 1) + piece_type];
    uint64_t all_pieces = piece_bitmasks[bb_all] | piece_bitmasks[bb_all + (bb_king + 1)];

    while ((start_pos = get_low_bit(actors, start_pos)) > -1) {
        bool is_promo = (piece_type & PIECE_MASK) == bb_pawn && ((color == White && start_pos >= 48) || (color == Black && start_pos <= 15));
        dest_squares = 0;

        if (((fl_flags & (FL_CAPTURES | FL_ALL)) && !(exclude_promo && is_promo)) || (include_promo && is_promo)) {
            dest_squares = BitboardCaptures::PregeneratedCaptures[color][piece_type][start_pos];
            if (!(fl_flags & FL_ALL)) {
                // only capture opponent's pieces
                dest_squares &= piece_bitmasks[(1 - color) * (bb_king + 1) + bb_all];
            }
            // pawns can't capture on empty squares
            else if ((piece_type & PIECE_MASK) == bb_pawn) {
                dest_squares &= all_pieces;
            }

            // except enpassant where "capture" isn't in the destination square
            int ep_rank = color == White ? 4 : 3;
            if (enpassant_file >= 0 && (piece_type & PIECE_MASK) == bb_pawn && ep_rank == start_pos / 8 && ((enpassant_file + 1) == start_pos % 8 || (enpassant_file - 1) == start_pos % 8)) {
                dest_squares |= 1ULL << ((ep_rank + (color == White ? 1 : -1)) * 8 + enpassant_file);
            }
        }
        if (((fl_flags & (FL_EMPTY | FL_ALL)) && !(exclude_promo && is_promo)) || (include_promo && is_promo)) {
            uint64_t dest_squares_empty = BitboardCaptures::PregeneratedMoves[color * 8 + piece_type][start_pos] & ~all_pieces;
            int starting_king_pos = (color == White ? 4 : 60);
            if (dest_squares_empty != 0 && (piece_type & PIECE_MASK) == bb_pawn) {
                uint64_t two_square_dest = 0;
                if (start_pos / 8 == 1 && color == White) {
                    two_square_dest = 1ULL << (start_pos + 16);
                } else if (start_pos / 8 == 6 && color == Black) {
                    two_square_dest = 1ULL << (start_pos - 16);
                }
                two_square_dest &= ~all_pieces;
                dest_squares_empty |= two_square_dest;
            } else if (dest_squares_empty != 0 && (piece_type & PIECE_MASK) == bb_king && start_pos == starting_king_pos) {
                // castle king-side
                uint64_t castle_dest = 0;

                if (can_castle(color, true) &&
                    (dest_squares_empty & (1ULL << (starting_king_pos + 1))) &&
                    ((1ULL << (starting_king_pos + 2)) & ~all_pieces) &&
                    ((1ULL << (starting_king_pos + 3)) & piece_bitmasks[color * (bb_king + 1) + bb_rook])) {
                    castle_dest |= (1ULL << (starting_king_pos + 2));
                }
                // queen-side
                if (can_castle(color, false) &&
                    (dest_squares_empty & (1ULL << (starting_king_pos - 1))) &&
                    (((1ULL << (starting_king_pos - 3)) | (1ULL << (starting_king_pos - 2))) & all_pieces) == 0 &&
                    ((1ULL << (starting_king_pos - 4)) & piece_bitmasks[color * (bb_king + 1) + bb_rook])) {
                    castle_dest |= (1ULL << (starting_king_pos - 2));
                }

                dest_squares_empty |= castle_dest;
            }
            dest_squares |= dest_squares_empty;
        }
        // check prioritization logic

        if (checks_only || exclude_checks) {
            uint64_t check_squares = 0;
            uint64_t opponent_king_pos = piece_bitmasks[(1 - color) * (bb_king + 1) + bb_king];
            int opponent_king_square = get_low_bit(opponent_king_pos, 0);
            if ((piece_type & PIECE_MASK) == bb_knight) {
                check_squares = BitboardCaptures::PregeneratedCaptures[color][piece_type][opponent_king_square];
            } else if ((piece_type & PIECE_MASK) == bb_pawn) {
                check_squares = BitboardCaptures::PregeneratedCaptures[1 - color][piece_type][opponent_king_square];
            }

            if (checks_only) {
                dest_squares &= check_squares;
            } else if (exclude_checks) {
                dest_squares &= ~check_squares;
            }
        }

        if (dest_squares) {
            break;
        } else {
            start_pos += 1;
        }
    }
}



uint64_t Bitboard::get_king_slide_blockers(int king_pos, Color king_color) const
{
    uint64_t potential_blockers = 0;
    uint64_t bishop_moves = BitArrays::bishop_moves::data[king_pos];
    uint64_t rook_moves = BitArrays::rook_moves::data[king_pos];
    if (bishop_moves & (get_bitmask(get_opposite_color(king_color), bb_bishop) | get_bitmask(get_opposite_color(king_color), bb_queen))) {
        potential_blockers |= bishop_moves;
    }
    if (rook_moves & (get_bitmask(get_opposite_color(king_color), bb_rook) | get_bitmask(get_opposite_color(king_color), bb_queen))) {
        potential_blockers |= rook_moves;
    }
    return potential_blockers;
}

bool Bitboard::is_not_illegal_due_to_check(Color color, piece_t piece, int start_pos, int dest_pos, uint64_t covered_squares) const
{
    // special rules for king moves
    if (piece == bb_king) {
        // don't move into check
        if (covered_squares & (1ULL << dest_pos)) {
            return false;
        }
        else if (abs(start_pos - dest_pos) == 2) {
            // it's a castle: don't move out of check or through check
            if (in_check || (covered_squares & (1ULL << (start_pos + (dest_pos - start_pos) / 2)))) {
                return false;
            }
        }
        // fall through: might still need removes_check test below
    }
    /*
    printf("is_legal %c%d-%c%d in_check=%d\n",
        start_pos%8+'a', start_pos/8 + 1, dest_pos%8+'a', dest_pos/8 + 1,
        in_check);
    display_bitboard(covered_squares, -1, -1);
    */
    // enpassant captures can remove two pieces from sight of rook/queen on 4th rank
    if (!in_check && piece == bb_pawn && (start_pos % 8 != dest_pos % 8) && ((piece_bitmasks[bb_all + (1 - color) * (bb_king + 1)] & (1ULL << dest_pos)) == 0)) {
        uint64_t my_king = piece_bitmasks[bb_king + (bb_king + 1) * color];
        int king_square = get_low_bit(my_king, 0);
        uint64_t attackers = piece_bitmasks[(1 - color) * (bb_king + 1) + bb_rook] | piece_bitmasks[(1 - color) * (bb_king + 1) + bb_queen];
        uint64_t all_pieces = piece_bitmasks[bb_all] | piece_bitmasks[(bb_king + 1) + bb_all];

        int df = 0;
        if (start_pos > king_square) {
            df = 1;
        } else if (start_pos < king_square) {
            df = -1;
        } else {
            assert(false);
        }

        int rank = start_pos / 8;
        for (int file = king_square % 8 + df; file < 8 && file >= 0; file += df) {
            if (attackers & (1ULL << (rank * 8 + file))) {
                return false;
            } else if (file == start_pos % 8 || file == dest_pos % 8) {
                // continue
            } else if (all_pieces & (1ULL << (rank * 8 + file))) {
                break;
            }
        }

    }
    if (!in_check && discovers_check(start_pos, dest_pos, color, covered_squares, true)) {
        return false;
    }
    else if (in_check && !removes_check(piece, start_pos, dest_pos, color, covered_squares)) {
        return false;
    }
    return true;
}

uint64_t Bitboard::get_rook_moves(int start_pos, uint64_t blockers) const {
    // returns bits that rook at start_pos has access to, including bits in blockers
    uint64_t block_board = BitArrays::rook_blockboard::data[start_pos];
    uint64_t mask = block_board & blockers;
    uint64_t magic_index = (lateral_slide_magic_factor[start_pos] * mask);
    return rook_magic[start_pos][magic_index >> (64 - BitArrays::rook_bitboard_bitcount::data[start_pos])];
}

uint64_t Bitboard::get_bishop_moves(int start_pos, uint64_t blockers) const {
    // returns bits that bishop at start_pos has access to, including bits in blockers
    uint64_t block_board = BitArrays::bishop_blockboard::data[start_pos];
    uint64_t mask = block_board & blockers;
    uint64_t magic_index = (diag_slide_magic_factor[start_pos] * mask);
    return bishop_magic[start_pos][magic_index >> (64 - BitArrays::bishop_bitboard_bitcount::data[start_pos])];
}

uint64_t Bitboard::get_blocking_squares(int src, int dest) const
{
    // returns bits that break connection from src <-> dest
    uint64_t srcmask = 1ULL << src;
    uint64_t destmask = 1ULL << dest;

    if ((src / 8 == dest / 8) || (src % 8 == dest % 8)) {
        // same rank or file
        return get_rook_moves(src, destmask) & get_rook_moves(dest, srcmask);
    }
    else if (BitArrays::bishop_moves::data[src] & destmask) {
        return get_bishop_moves(src, destmask) & get_bishop_moves(dest, srcmask);
    }

    return 0;
}


// color indicates which king would be checked
// returns legal destination squares if any, given pseudo-legal dest_squares as candidates
uint64_t Bitboard::remove_discovered_checks(piece_t piece_type, int start_pos, uint64_t dest_squares, Color color, uint64_t covered_squares) const
{
    // requirement #1: source position is covered by opponent
    // requirement #2: direct attack from rook/bishop/queen to king

    bool source_pos_covered = covered_squares & (1ULL << start_pos);

    uint64_t source_pieces = (1ULL << start_pos);
    uint64_t original_all_pieces = piece_bitmasks[bb_all] | piece_bitmasks[bb_all + (bb_king + 1)];
    uint64_t all_pieces = original_all_pieces & ~source_pieces;
    uint64_t bishop_attackers = piece_bitmasks[(1 - color) * (bb_king + 1) + bb_bishop];
    uint64_t rook_attackers = piece_bitmasks[(1 - color) * (bb_king + 1) + bb_rook];
    uint64_t queen_attackers = piece_bitmasks[(1 - color) * (bb_king + 1) + bb_queen];
    uint64_t my_king = piece_bitmasks[bb_king + (bb_king + 1) * color];
    int king_square = get_low_bit(my_king, 0);

    // if start_pos isn't attacked then it can't discover checks, except that enpassant captures can remove two pieces from sight of rook/queen on 4th rank
    if (piece_type == bb_pawn && (start_pos / 8 == king_square / 8) && (start_pos / 8 == 3 || start_pos / 8 == 4) && ((piece_bitmasks[bb_all + (1 - color) * (bb_king + 1)] & dest_squares) == 0)) {
        uint64_t approved_squares = 0;
        int dest_pos = -1;
        // check each square individually (should only be 1-3 so speed not critical)
        while ((dest_pos = get_low_bit(dest_squares, dest_pos + 1)) >= 0) {
            bool approved = true;
            uint64_t removed_piece = 0;
            if (start_pos % 8 != dest_pos % 8) {
                // is a capture
                removed_piece = 1ULL << (8 * (start_pos / 8) + dest_pos % 8);
            }
            if (get_rook_moves(king_square, all_pieces & ~removed_piece) & (rook_attackers | queen_attackers) & (0xffULL << (8 * (start_pos / 8)))) {
                // this enpassant move discovers check
                approved = false;
            }
            if (approved) {
                approved_squares |= (1ULL << dest_pos);
            }
        }
        return approved_squares;
    }

    else if (source_pos_covered) {
        /* rook or queen attacks */
        uint64_t lateral_attackers = get_rook_moves(king_square, all_pieces) & (rook_attackers | queen_attackers);
        uint64_t diag_attackers = get_bishop_moves(king_square, all_pieces) & (bishop_attackers | queen_attackers);
        uint64_t all_attackers = lateral_attackers | diag_attackers;

        if (count_bits(all_attackers) > 1) {
            // 2 attackers, not possible to block
            return 0;
        }

        if (all_attackers) {
            // taking this piece off the board discovers check, but it can still stay on the pinned rank/file or capture pinning piece
            int attacker = get_low_bit(all_attackers, 0);
            assert(attacker >= 0);
            if (get_blocking_squares(attacker, king_square) & source_pieces) {
                return (get_blocking_squares(attacker, king_square) | all_attackers) & dest_squares;
            } else {
                return dest_squares;
            }
        }
    }

    return dest_squares;
}

// color indicates which king would be checked
bool Bitboard::discovers_check(int start_pos, int dest_pos, Color color, uint64_t covered_squares, bool inverted) const
{
    // requirement #1: source position is covered by opponent
    // requirement #2: direct attack from rook/bishop/queen to king

    bool source_pos_covered = covered_squares & (1ULL << start_pos);

    if (source_pos_covered) {
        uint64_t my_king = piece_bitmasks[bb_king + (bb_king + 1) * color];
        int king_square = get_low_bit(my_king, 0);
        uint64_t source_pieces = (1ULL << start_pos);
        uint64_t all_pieces = ((piece_bitmasks[bb_all] | piece_bitmasks[bb_all + (bb_king + 1)]) & ~source_pieces);

        /* rook or queen attacks */
        uint64_t block_board = BitArrays::rook_blockboard::data[king_square];
        if (block_board & source_pieces) {
            uint64_t mask = block_board & all_pieces;
            uint64_t magic_index = (lateral_slide_magic_factor[king_square] * mask);
            uint64_t all_dest_squares = rook_magic[king_square][magic_index >> (64 - BitArrays::rook_bitboard_bitcount::data[king_square])];
            uint64_t attackers = ~(1ULL << dest_pos) & all_dest_squares & (piece_bitmasks[(1 - color) * (bb_king + 1) + bb_rook] | piece_bitmasks[(1 - color) * (bb_king + 1) + bb_queen]);
            int attacker = -1;
            int dr = (king_square / 8) - (start_pos / 8);
            int df = (king_square % 8) - (start_pos % 8);
            int dest_dr = (king_square / 8) - (dest_pos / 8);
            int dest_df = (king_square % 8) - (dest_pos % 8);
            while ((attacker = get_low_bit(attackers, attacker + 1)) >= 0) {
                if (attacker == dest_pos) {
                    continue;
                }
                int adr = (start_pos / 8) - (attacker / 8);
                int adf = (start_pos % 8) - (attacker % 8);
                if ((adr == 0 && dr == 0 && (sign(df) == sign(adf)) && !(dest_dr == 0 && ((df > 0) == (dest_df > 0)) && abs(adf+df) > abs(dest_df))) ||
                    (adf == 0 && df == 0 && (sign(dr) == sign(adr)) && !(dest_df == 0 && ((dr > 0) == (dest_dr > 0)) && abs(adr+dr) > abs(dest_dr)))) {
                    return true;
                }
            }
        }

        /* bishop or queen attacks */
        block_board = BitArrays::bishop_blockboard::data[king_square];
        if (block_board & source_pieces) {
            uint64_t mask = block_board & all_pieces;
            uint64_t magic_index = (diag_slide_magic_factor[king_square] * mask);
            uint64_t all_dest_squares = bishop_magic[king_square][magic_index >> (64 - BitArrays::bishop_bitboard_bitcount::data[king_square])];

            uint64_t attackers = all_dest_squares & (piece_bitmasks[(1 - color) * (bb_king + 1) + bb_bishop] | piece_bitmasks[(1 - color) * (bb_king + 1) + bb_queen]);
            int attacker = -1;
            int dr = (king_square / 8) - (start_pos / 8);
            int df = (king_square % 8) - (start_pos % 8);
            int dest_dr = (king_square / 8) - (dest_pos / 8);
            int dest_df = (king_square % 8) - (dest_pos % 8);

            while ((attacker = get_low_bit(attackers, attacker + 1)) >= 0) {
                if (attacker == dest_pos) {
                    continue;
                }
                int adr = (king_square / 8) - (attacker / 8);
                int adf = (king_square % 8) - (attacker % 8);
                if (sign(dr) == sign(adr) && abs(adr) > abs(dr) &&
                    sign(df) == sign(adf) && abs(adf) > abs(df) &&
                    !(abs(dest_dr) == abs(dest_df) && sign(dest_dr) == sign(dr) && abs(adr) > abs(dest_dr) &&
                        sign(dest_df) == sign(df) && abs(adf) > abs(dest_df))) {
                    return true;
                }
            }
        }

    }

    return false;

}

uint64_t Bitboard::get_captures(Color color, piece_t piece_type, int start_pos) const
{
    uint64_t piece_covers = 0;
    switch (piece_type) {
    case bb_queen: case bb_bishop: case bb_rook:
        next_piece_slide(color, piece_type, start_pos, piece_covers, FL_CAPTURES | FL_EMPTY);
        break;
    case bb_knight: case bb_king:
        next_pnk_move(color, piece_type, start_pos, piece_covers, FL_CAPTURES | FL_EMPTY, false, false);
        break;
    case bb_pawn:
        piece_covers = BitboardCaptures::PregeneratedCaptures[color][bb_pawn][start_pos];
        break;
    }
    return piece_covers;
}

/* find dest_pos that removes check against the king of color color? */
uint64_t Bitboard::removes_check_dest(piece_t piece_type, int start_pos, uint64_t dest_squares, Color color, uint64_t covered_squares, uint64_t attackers) const
{
    // to remove check, the move must a) move the king, b) capture the threatening piece, or c) block a threatening piece
    //  these are not sufficient conditions since they do not account for double check or discovering another check
    uint64_t king_pos = piece_bitmasks[bb_king + (bb_king + 1) * color];
    int king_square = get_low_bit(king_pos, 0);
    uint64_t legal_captures = 0;

    if ((piece_type & PIECE_MASK) == bb_king) {
        // need to ensure the king isn't blocking its own check
        uint64_t covered = computed_covered_squares(color == White ? Black : White, king_pos, 0, king_pos);
        return ~covered & dest_squares;
    }

    // captures or blocks (other than with king, which should be covered by case a)
    else if (count_bits(attackers) == 1) {
        // can't move a piece which is already pinned
        dest_squares = remove_discovered_checks(piece_type, start_pos, dest_squares, color, covered_squares);
        legal_captures = dest_squares & attackers;
        int attacker = get_low_bit(attackers, 0);
        assert(attacker >= 0);

        // special enpassant rules: captured piece won't be on dest_squares
        if ((piece_type & PIECE_MASK) == bb_pawn && attacker % 8 == enpassant_file) {
            if (color == Black && (0xff000000 & (dest_squares << 8)) == attackers) {
                legal_captures = attackers >> 8;
            } else if (color == White && (0xff00000000ULL & (dest_squares >> 8)) == attackers) {
                legal_captures = attackers << 8;
            }
        }

        uint64_t legal_lateral_blocks = get_blocking_squares(king_square, attacker) & dest_squares;
        return legal_captures | legal_lateral_blocks;
    } else {
        // in case of double check, only king moves are legal
        return 0;
    }
}

/* does it remove check against the king of color color? */
bool Bitboard::removes_check(piece_t piece_type, int start_pos, int dest_pos, Color color, uint64_t covered_squares) const
{
    // to remove check, the move must a) move the king, b) capture the threatening piece, or c) block a threatening piece
    //  these are not sufficient conditions since they do not account for double check or discovering another check
    uint64_t opponent_pieces = piece_bitmasks[bb_all + (bb_king + 1) * (1 - color)];
    uint64_t my_king = piece_bitmasks[bb_king + (bb_king + 1) * color];

    if ((piece_type & PIECE_MASK) == bb_king) {
        // need to ensure the king isn't blocking its own check
        uint64_t covered = computed_covered_squares(color == White ? Black : White, my_king, 0, my_king);
        if (!(covered & (1ULL << dest_pos))) {
            return true;
        }
    }

    // capture
    piece_t dest_piece = get_piece(dest_pos / 8, dest_pos % 8) & PIECE_MASK;
    int captured_pos = dest_pos;
    piece_t captured_piece = dest_piece;

    if (piece_type == bb_pawn && start_pos % 8 != dest_pos % 8 && dest_piece == EMPTY) {
        captured_pos = 8 * (start_pos / 8) + (dest_pos % 8);
        captured_piece = bb_pawn;
    }

    if ((opponent_pieces & (1ULL << captured_pos)) &&
        (get_captures(color == White ? Black : White, captured_piece & PIECE_MASK, captured_pos) & my_king) &&
        !discovers_check(start_pos, dest_pos, color, covered_squares, true) &&
        !(computed_covered_squares(color == White ? Black : White, 1ULL << captured_pos, 1ULL << start_pos, 0) & my_king)) {
        // need to do an extra test for double-check
        return true;
    }
    // block
    if (discovers_check(dest_pos, start_pos, color, covered_squares, true) &&
        !discovers_check(start_pos, dest_pos, color, covered_squares, false) &&
        !(computed_covered_squares(color == White ? Black : White, (1ULL << start_pos), (1ULL << dest_pos), 1ULL << start_pos) & my_king)) {
        return true;
    }
    return false;
}

bool Bitboard::king_in_check(Color color) const
{
    uint64_t king_pos = piece_bitmasks[color * (bb_king + 1) + bb_king];
    int king_square = get_low_bit(king_pos, 0);
    uint64_t covered = computed_covered_squares(color == White ? Black : White, ~attack_squares[king_square], 0, 0);
    return covered & king_pos;
}


/* returns squares of color pieces that attack this square */
uint64_t Bitboard::square_attackers(int dest, Color color) const
{
    uint64_t attackers = 0;

    uint64_t all_pieces = piece_bitmasks[bb_all] | piece_bitmasks[bb_all + (bb_king + 1)];
    uint64_t bishop_attackers = piece_bitmasks[color * (bb_king + 1) + bb_bishop];
    uint64_t rook_attackers = piece_bitmasks[color * (bb_king + 1) + bb_rook];
    uint64_t queen_attackers = piece_bitmasks[color * (bb_king + 1) + bb_queen];

    piece_t pnk_pieces[3] = { bb_pawn, bb_knight, bb_king };
    /* rook or queen attacks */
    attackers |= get_rook_moves(dest, all_pieces) & (rook_attackers | queen_attackers);
    attackers |= get_bishop_moves(dest, all_pieces) & (bishop_attackers | queen_attackers);

    for (int i = 0; i < 3; i++) {
        int piece_type = pnk_pieces[i];
        attackers |= BitboardCaptures::PregeneratedCaptures[1 - color][piece_type][dest] & piece_bitmasks[color * (bb_king + 1) + piece_type];
    }


    return attackers;
}

/* returns squares that are empty or occupied by opposite color pieces, and currently attacked by pieces of Color color */
void Bitboard::computed_covered_squares_b(Color color, uint64_t exclude_pieces, uint64_t include_pieces, uint64_t exclude_block_pieces, int times, uint64_t *covered_squares) const
{
    for (int i = 0; i < times; i++) {
        covered_squares[i] = 0;
    }

    for (piece_t piece_type = bb_pawn; piece_type <= bb_king; piece_type++) {
        uint64_t piece_covers = 0;
        int start_pos = -1;

        uint64_t actors = piece_bitmasks[color * (bb_king + 1) + piece_type] & ~exclude_pieces;

        while ((start_pos = get_low_bit(actors, start_pos+1)) > -1) {

            switch (piece_type) {
                case bb_queen: case bb_bishop: case bb_rook:
                    next_piece_slide(color, piece_type, start_pos, piece_covers, FL_ALL | FL_EMPTY | FL_CAPTURES, exclude_block_pieces, exclude_pieces, include_pieces, false, false);
                    break;
                case bb_knight: case bb_king:
                    next_pnk_move(color, piece_type, start_pos, piece_covers, FL_ALL | FL_EMPTY | FL_CAPTURES, false, false);
                    break;
                case bb_pawn:
                    piece_covers = BitboardCaptures::PregeneratedCaptures[color][bb_pawn][start_pos];
                    break;
            }
            // carry adder
            uint64_t carry = piece_covers;
            for (int i = 0; i < times; i++) {
                if (i == times - 1) {
                    covered_squares[i] |= carry;
                } else {
                    uint64_t new_carry = covered_squares[i] & carry;
                    covered_squares[i] = covered_squares[i] ^ carry;
                    carry = new_carry;
                }
            }

            if (start_pos < 0) {
                break;
            }
        }
    }
}

/* returns squares that are empty or occupied by opposite color pieces, and currently attacked by pieces of Color color */
uint64_t Bitboard::computed_covered_squares(Color color, uint64_t exclude_pieces, uint64_t include_pieces, uint64_t exclude_block_pieces, int times) const
{
    uint64_t covered_squares[times];
    for (int i = 0; i < times; i++) {
        covered_squares[i] = 0;
    }

    for (piece_t piece_type = bb_pawn; piece_type <= bb_king; piece_type++) {
        uint64_t piece_covers = 0;
        int start_pos = -1;

        uint64_t actors = piece_bitmasks[color * (bb_king + 1) + piece_type] & ~exclude_pieces;

        while ((start_pos = get_low_bit(actors, start_pos+1)) > -1) {

            switch (piece_type) {
                case bb_queen: case bb_bishop: case bb_rook:
                    next_piece_slide(color, piece_type, start_pos, piece_covers, FL_ALL | FL_EMPTY | FL_CAPTURES, exclude_block_pieces, exclude_pieces, include_pieces, false, false);
                    break;
                case bb_knight: case bb_king:
                    next_pnk_move(color, piece_type, start_pos, piece_covers, FL_ALL | FL_EMPTY | FL_CAPTURES, false, false);
                    break;
                case bb_pawn:
                    piece_covers = BitboardCaptures::PregeneratedCaptures[color][bb_pawn][start_pos];
                    break;
            }
            // carry adder
            uint64_t carry = piece_covers;
            for (int i = 0; i < times; i++) {
                if (i == times - 1) {
                    covered_squares[i] |= carry;
                } else {
                    uint64_t new_carry = covered_squares[i] & carry;
                    covered_squares[i] = covered_squares[i] ^ carry;
                    carry = new_carry;
                }
            }

            if (start_pos < 0) {
                break;
            }
        }
    }
    return covered_squares[times - 1];
}

bool Bitboard::is_legal_move(move_t move, Color color) const
{
    unsigned char dest_rank, dest_file, src_rank, src_file;
    uint64_t dest_squares = 0;

    get_source(move, src_rank, src_file);
    get_dest(move, dest_rank, dest_file);

    int actor = src_rank * 8 + src_file;
    int dest_pos = dest_rank * 8 + dest_file;

    // is there a piece there
    piece_t piece = get_piece(src_rank, src_file);
    if ((piece > bb_king ? Black : White) != color) {
        // piece is wrong color
        return false;
    }
    else if (piece == 0) {
        // square is empty
        return false;
    }

    piece_t dest_piece = get_piece(dest_rank, dest_file);
    if (dest_piece != 0 && (dest_piece > bb_king ? White : Black) != color) {
        // can't capture our own piece
        return false;
    }
    // save this value because next_pnk_move might advance to next piece
    int original_actor = actor;

    switch(piece & PIECE_MASK) {
        case bb_pawn:
            next_pnk_move(color, piece & PIECE_MASK, actor, dest_squares, dest_piece == 0 ? FL_EMPTY : FL_CAPTURES, false, false);
            break;
        case bb_king: case bb_knight:
            next_pnk_move(color, piece & PIECE_MASK, actor, dest_squares, FL_ALL, false, false);
            break;
        case bb_bishop: case bb_rook: case bb_queen:
            next_piece_slide(color, piece & PIECE_MASK, actor, dest_squares, FL_ALL);
            break;
    }

    if (original_actor != actor || !(dest_squares & (1ULL << dest_pos))) {
        // not a move
        return false;
    }

    uint64_t covered_squares = computed_covered_squares(color == Black ? White : Black, 0, 0, 0);
    return is_not_illegal_due_to_check(color, piece & PIECE_MASK, actor, dest_pos, covered_squares);
}

void Bitboard::get_moves(Color side_to_play, bool checks, bool captures_or_promo, std::vector<move_t> &moves) const
{
    uint64_t king_slide_blockers = 0;
    uint64_t covered_squares = computed_covered_squares(side_to_play == Black ? White : Black, 0, 0, 0);
    uint64_t my_king = piece_bitmasks[bb_king + (bb_king + 1) * side_to_play];
    assert(my_king > 0);
    int king_square = get_low_bit(my_king, 0);
    uint64_t king_attackers = square_attackers(king_square, side_to_play == White ? Black : White);

    if (!in_check) {
        king_slide_blockers = get_king_slide_blockers(king_square, side_to_play);
    }

        // find the next piece to move
    for (piece_t piece_type = bb_pawn; piece_type <= bb_king; piece_type++) {
        uint64_t dest_squares = 0;
        int index = piece_type + side_to_play * (bb_king+1);
        int actor = -1;
        while ((actor = get_low_bit(piece_bitmasks[index], actor+1)) >= 0) {
            // get quasi-legal moves
            switch(piece_type) {
                case bb_king: case bb_pawn: case bb_knight:
                    next_pnk_move(side_to_play, piece_type, actor, dest_squares, captures_or_promo ? FL_CAPTURES : FL_EMPTY, checks, !checks, captures_or_promo, !captures_or_promo);
                    break;
                case bb_bishop: case bb_rook: case bb_queen:
                    next_piece_slide(side_to_play, piece_type, actor, dest_squares, captures_or_promo ? FL_CAPTURES : FL_EMPTY, 0, 0, 0, checks, !checks);
                    break;
            }
            if (actor < 0) {
                break;
            }
            // filter out illegal moves
            if (piece_type == bb_king) {
                // don't move into check
                dest_squares &= ~covered_squares;
                // castling: don't move out of check
                if (in_check) {
                    dest_squares &= BitArrays::king_moves::data[king_square];
                }
                // or through check king-side
                if (covered_squares & (1ULL << (king_square + 1))) {
                    dest_squares &= ~(1ULL << (king_square + 2));
                }
                // or through check queen-side
                if (covered_squares & (1ULL << (king_square - 1))) {
                    dest_squares &= ~(1ULL << (king_square - 2));
                }

                // fall through: might still need removes_check test below
            }
            if (!in_check && ((1ULL << actor) & king_slide_blockers) > 0) {
                dest_squares = remove_discovered_checks(piece_type, actor, dest_squares, side_to_play, covered_squares);
            }
            else if (in_check) {
                dest_squares = removes_check_dest(piece_type, actor, dest_squares, side_to_play, covered_squares, king_attackers);
            }

            if (!captures_or_promo) {
                make_moves(moves, actor, piece_type + (side_to_play == Black ? 8 : 0), dest_squares);
            } else {
                int dest_pos = -1;
                while ((dest_pos = get_low_bit(dest_squares, dest_pos+1)) >= 0) {
                    move_t move;
                    if (piece_type == bb_pawn && (dest_pos <= 7 || dest_pos >= 56)) {
                        if (captures_or_promo) {
                            for (piece_t promote_type = bb_queen; promote_type >= bb_knight; promote_type--) {
                                move = make_move(actor / 8, actor % 8,
                                    piece_type + (side_to_play == Black ? 8 : 0),
                                    dest_pos / 8, dest_pos % 8,
                                    get_piece(dest_pos / 8, dest_pos % 8),
                                    promote_type);
                                moves.push_back(move);
                            }
                        }
                    } else {
                        move = make_move(actor / 8, actor % 8,
                            piece_type + (side_to_play == Black ? 8 : 0),
                            dest_pos / 8, dest_pos % 8,
                            get_piece(dest_pos / 8, dest_pos % 8),
                            0);
                        moves.push_back(move);
                    }
                }
            }
        }
    }
}

void Bitboard::next_piece_slide(Color color, piece_t piece_type, int &start_pos, uint64_t &dest_squares, int fl_flags, uint64_t exclude_block_pieces, uint64_t exclude_source_pieces, uint64_t include_pieces, bool checks_only, bool exclude_checks) const
{
    uint64_t actors = piece_bitmasks[color * (bb_king + 1) + piece_type] & ~exclude_source_pieces;
    uint64_t all_pieces = ((piece_bitmasks[bb_all] | piece_bitmasks[bb_all + (bb_king + 1)]) & ~exclude_block_pieces) | include_pieces;

    while ((start_pos = get_low_bit(actors, start_pos)) > -1) {
        uint64_t block_board = 0;
        uint64_t all_dest_squares = 0;

        if (piece_type == bb_rook || piece_type == bb_queen) {
            block_board = BitArrays::rook_blockboard::data[start_pos];
            uint64_t mask = block_board & all_pieces;
            uint64_t magic_index = (lateral_slide_magic_factor[start_pos] * mask);
            all_dest_squares |= rook_magic[start_pos][magic_index >> (64 - BitArrays::rook_bitboard_bitcount::data[start_pos])];
        }
        if (piece_type == bb_bishop || piece_type == bb_queen) {
            block_board = BitArrays::bishop_blockboard::data[start_pos];
            uint64_t mask = block_board & all_pieces;
            uint64_t magic_index = (diag_slide_magic_factor[start_pos] * mask);
            all_dest_squares |= bishop_magic[start_pos][magic_index >> (64 - BitArrays::bishop_bitboard_bitcount::data[start_pos])];
        }

        dest_squares = 0;
        if (fl_flags & FL_ALL) {
            dest_squares = all_dest_squares;
        }
        else {
            if (fl_flags & FL_EMPTY) {
                dest_squares |= (all_dest_squares & ~all_pieces);
            }
            if (fl_flags & FL_CAPTURES) {
                dest_squares |= (all_dest_squares & piece_bitmasks[(1 - color) * (bb_king + 1) + bb_all]);
            }
        }
        if (checks_only || exclude_checks) {
            uint64_t check_squares = 0;
            uint64_t opponent_king_pos = piece_bitmasks[(1 - color) * (bb_king + 1) + bb_king];
            int opponent_king_square = get_low_bit(opponent_king_pos, 0);

            if (piece_type == bb_rook || piece_type == bb_queen) {
                block_board = BitArrays::rook_blockboard::data[opponent_king_square];
                uint64_t mask = block_board & all_pieces;
                uint64_t magic_index = (lateral_slide_magic_factor[opponent_king_square] * mask);
                check_squares |= rook_magic[opponent_king_square][magic_index >> (64 - BitArrays::rook_bitboard_bitcount::data[opponent_king_square])];
            }
            if (piece_type == bb_bishop || piece_type == bb_queen) {
                block_board = BitArrays::bishop_blockboard::data[opponent_king_square];
                uint64_t mask = block_board & all_pieces;
                uint64_t magic_index = (diag_slide_magic_factor[opponent_king_square] * mask);
                check_squares |= bishop_magic[opponent_king_square][magic_index >> (64 - BitArrays::bishop_bitboard_bitcount::data[opponent_king_square])];
            }

            if (checks_only) {
                dest_squares &= check_squares;
            } else if (exclude_checks) {
                dest_squares &= ~check_squares;
            }
        }


        if (dest_squares) {
            break;
        } else {
            start_pos += 1;
        }
    }
}



// zobrist hashing stuff

const uint64_t zobrist_hashes[] = {0xe02111c7659eae86L, 0x6da4e7350d2011b3L, 0x83cd50fcc552e5e9L, 0x78615f6439c6c6b4L, 0x51c92cc23497add2L,
0xc8fc6be58ad5a05fL, 0xd5e910e9d62f114dL, 0x7ef0daa1916a9daaL, 0xc6aac4139c2cc032L, 0x9003d09f97066fc5L, 0x6a78933e52670af4L, 0x6ad0ad6da26c3d19L,
0xefdc626850a3e9d9L, 0x23fbf0da01209946L, 0xff26e2a098ce09c0L, 0x95199b3215da66b0L, 0x7d6367bdd38c52faL, 0xf17fe7c7cf19d2b4L, 0x4837f5aba116f8a2L,
0x4f66830006502645L, 0xa4f58972cc89f3fcL, 0xdea8789b9dbbe477L, 0xad7b4393cd4bb600L, 0x6b10f5f748599647L, 0x85a319181ea35747L, 0x246fecd9109c5d52L,
0x168ed09595f7df38L, 0x8c6bba3aaaef340dL, 0xb40e45ed3369cb74L, 0xbde1a58739ec73L, 0x1b6e71b2ec8ce71eL, 0xadcb55dd6f0602fL, 0x24debf78ceb85ef6L,
0xed5067a033af109eL, 0x78f45cf85520a20eL, 0xbd4c56d2866b6ae1L, 0xf552502c3bdf332L, 0x4d08466a8cd4cc2L, 0xe8ee56ec12df978L, 0xcde413d7b5742c9L,
0xa64c9766137404b6L, 0x556e2a88ae578dbfL, 0x52e4fa59ca741895L, 0x84fd4c736ab37055L, 0x3d19378478a6004dL, 0x1efc8f3f6243b9efL, 0xeffcfbe872e27214L,
0xbb755410ad7518f4L, 0xe7687c398f501f7fL, 0xa9887346984cadf8L, 0xe7990a9b385b2fb1L, 0x18460e946b81774dL, 0xa3a9d9579eed6f2aL, 0xca6b9dfeb1ea3055L,
0xe981e73c0abdb693L, 0x9b6540d7e205f5L, 0x7023035da61ce9e8L, 0xba61e58796bb925fL, 0xb2519759680b076cL, 0x54d7165c0538a919L, 0x40653ffe5cc1940aL,
0x168f85affc87a4dbL, 0x486023b5ce1de6b2L, 0x413bd10a0e6775a8L, 0x4ceaab3e0186f2c3L, 0xe5431e2e2ffe953aL, 0x41f95e2d1f1bfb4bL, 0x1e35ec556e077c82L,
0xd09db63d56f72a8aL, 0x47eb45c372fc48cfL, 0xdab024345d3ef3a6L, 0x13bdba3d9ace3960L, 0xf11249f6d41b497L, 0xc02480cbc7f1066dL, 0x49bad8c786e36b5dL,
0xf028ee71487b0898L, 0x94c08209aaed2cbfL, 0x9cdda1876604163fL, 0xefee394ae0c73abL, 0xb0d8925119193e95L, 0xc1fac6ab7439df00L, 0x457b11d58de47b4cL,
0x2a6beb79a4d65462L, 0xb935574ce7253d05L, 0x7c880a4fe94f0adeL, 0x976918e8a8256b47L, 0x65751dc8eede76a6L, 0x9bb4ea5e00988851L, 0xdb6f0f8064d3e038L,
0xeeb896b7e4f148c1L, 0xccd1809cddb1ed91L, 0xf5b464f4b1e87bd7L, 0xe5ca8b119a28c4edL, 0x757fc2632e870eL, 0x25ea79e54c48e8e0L, 0x6c670f34b2499eb3L,
0xf272471309a57a74L, 0x58917386d789d83fL, 0x5f30a770efd0418fL, 0x7f8272e2fbab644fL, 0xa19a3efafc81ff9fL, 0x67686fe1a51d4219L, 0x768f99fe57436478L,
0x1a0550cd0e44ce6fL, 0xbb68cb6a763dd679L, 0x9f7ee195a353ea67L, 0xb8c3b743d0ee17cL, 0x68ee4cbc2f55001cL, 0xe710344acc6e063dL, 0x7e82d2727f6d00a3L,
0x1bec90d0f681f5ffL, 0x5e0fc6b9ff32e59eL, 0x32486f6467c74701L, 0xd5f922f18e167471L, 0x6611767b362e51deL, 0xdb736134ef606112L, 0x87e25b87d0b82117L,
0x592024d472e9b429L, 0xa843d541bd752ac4L, 0xc05fb377703e539eL, 0x2a842e03bfc34776L, 0xb6b731c087801de2L, 0x99836b1c5fb2052L, 0x86dda93f27aed1e3L,
0x689c3121f48b7b8dL, 0xf73f53b546e8a39fL, 0x1d129ae53308a565L, 0x18953add2057232bL, 0x50f59199d39dd3e2L, 0x792c499ab29e93f4L, 0x60a93caf5bc7357fL,
0x343d68828050e150L, 0x1221303626829d85L, 0x22317bccd7c156d9L, 0x941e3911603f810cL, 0xcc1d8cc38de647a1L, 0x5fbfc09833cfc3cdL, 0xda39dae0ede57888L,
0x1292c203e4b38826L, 0xb5f71770ae190a2eL, 0xe52a80282c3b626fL, 0x4907a1847a0a48f3L, 0x61ca184e64535f5dL, 0x7100abda1d4085a7L, 0x866fbbbab5984ec2L,
0xc067f5b4fff25e03L, 0x1b929ecf1c676e69L, 0xd311e446dd475ab5L, 0x7c3f7a6bf2f4c8acL, 0x63237ee4afc1a442L, 0xbe2693a016500fd7L, 0x90272e0ae2754308L,
0xe653afae59824ed9L, 0xbaa37d9f2a61036fL, 0x3acafc5157d5448aL, 0xf281134f50edb7deL, 0x4d22c8e9a33c5aecL, 0xa859f92794c928e0L, 0xdd5d00367dd385fL,
0x1d85c7ea04d773bfL, 0xccd60ec1f904e1a5L, 0x7316211557b7b24dL, 0x6d415afdc956148aL, 0xd44f2ec0cde1572dL, 0x30670c219d61d1a5L, 0x367fbd71ad47ced2L,
0xe7db7fecea67fe66L, 0xa5ba9bd21bca6ec4L, 0x61d3c0dda9dc3a08L, 0xe15299640169ae61L, 0xdcb655c24672aa3cL, 0x2f0293295d6addc5L, 0x16abc74cd7f5f4a1L,
0xc5dac2f5bdc67e5aL, 0x3fcbdb2198c2325bL, 0xf48bddbe2cff0ff5L, 0x3a2efba78ff061a3L, 0xd501e98587ffaa3cL, 0x4a06edfad212be20L, 0x9564bfbe936d0fa2L,
0xfe41617621975198L, 0xf6cb5308b994f866L, 0x9dbbe6e2d6960a45L, 0xd9f7023ce3832732L, 0x9c118152aadde3f8L, 0xa84f7f54395888beL, 0xb2bf8b0ca355c7b1L,
0x235fb4f73cc324c0L, 0xcad2f1760863afe2L, 0x82800fcd02183730L, 0x8d53ed151f951371L, 0xa409404098a69e1eL, 0xe76100c2a082d6d6L, 0x37a7c41f64c1a3a2L,
0x197e5d366662847cL, 0x657b6de87f7e7077L, 0x4dc814cecd1de71cL, 0xe31d53ed02b6e609L, 0xc924124966da58eL, 0x4c8fe37a001e064dL, 0xb106e3d58e7a9422L,
0xe2e7be07c8c7b7d5L, 0xa22224ae3e6485d8L, 0xa7f8b9614605e8aaL, 0x287fd02f9dd3b869L, 0x95851c81e159a1beL, 0x8956eb855f29a48L, 0x16b1bf0275df9c77L,
0x47e6fd3c3f3b7ee7L, 0xd47444ea9f5361c0L, 0xb5e89f2705fe52bfL, 0x13f066d7143f5588L, 0xc527f89fa1eae7a3L, 0x736a86dca1cf1540L, 0x32dbd3a6ad38e1e9L,
0x90837f579ebb8692L, 0xfbd28e74ebfb05ecL, 0x5eaca369b54b2e74L, 0xcd57eb0e7aa455eL, 0xa2a8f37d5188388cL, 0x67d8d658c1c3158cL, 0xfb7d9f62e85c1009L,
0x70b935e0022fc77cL, 0xaa0296547be79928L, 0x64186aff1ebaa64cL, 0x5334b277fa707270L, 0x2df92a55f3c4a073L, 0x6d7c4e9d9ae27ef7L, 0xae6b6f3c76f4a9f9L,
0x5a26c0bd2179bf5cL, 0x6eb9b2d5b1d834c2L, 0x183181073cd500dbL, 0xfde621a0121df43cL, 0x5317bf4c4b0ef577L, 0x4a9111532cfdaeecL, 0x427796da74538fdbL,
0x9c9256da3c6966feL, 0x24d6fe3fb5b1f860L, 0x12b3dae6aec8c5aeL, 0xa11f8fefaa955139L, 0xb2e48a164a7ab6a6L, 0x2868e5c6cbec09e5L, 0xf3c1ba3649e91fe2L,
0x51fbf818941d1f23L, 0x96962e35a0229b63L, 0x1a57a5960c59ae63L, 0x80706e6ab2fed9d5L, 0x6233aaf4a771afd8L, 0x29381ec58d077487L, 0xf6243cab4958effbL,
0xb25a1f03d14e197eL, 0x4eb418b37c000af7L, 0x5abf427e4872a0dfL, 0x52841f7082d3d090L, 0x8e35bee31b5cd60aL, 0x533aefb34061c9deL, 0x6334817a4b97f03fL,
0xe1ee72a2dd4f2271L, 0x468e76ba573e459bL, 0xcfbb76e816a06069L, 0x45479d3aa5cbc520L, 0x229b6f77bf240787L, 0xd2852845d0d8f033L, 0x312b325f1a53cfbcL,
0xea85bce6ea768a32L, 0x9b113597204cc6f3L, 0x10d0aa892fe6d551L, 0xaac58a164d2dca5cL, 0x7040b3632375e9dfL, 0xa7cf808f3ab85af4L, 0x8224a9a91e53c2deL,
0xe25e2f7cadabf5d7L, 0x857e5e8ba67c5bd6L, 0xddba1fe0d16d4c22L, 0x93faaf7ae785bccL, 0xa1fe0ec2dfd7a2ffL, 0xe4f5caf7614e771eL, 0xe9023c53a3645784L,
0x322e9df7056442e5L, 0x9e0fff3980764f63L, 0xa03d32cb0729a731L, 0x3ad238c85db5172fL, 0xc6dc67ef0655062eL, 0x1205f3d49b09f008L, 0xa248a320e1e38a14L,
0x565947feff72e498L, 0x2cc2b57ba4673d95L, 0x82f74ef0627e0e8aL, 0x2a4dfbeb2d282003L, 0xd7c9cba6a75f9abaL, 0x8f1cf955e1dd2511L, 0x819a7d4b8ae7bdbeL,
0x5c00466aaad1e533L, 0x66be08ad13cb5422L, 0x4a4b47719e33fadcL, 0xb45ddd989ed26560L, 0xce41d914171c194L, 0xcbea2cb487748238L, 0xc87720748b4be85fL,
0xe171855a7cfeb71fL, 0x77c04e6b4dba374L, 0x68c9f3b015709cfaL, 0x28e389d655662e9fL, 0x67dea09358345d2aL, 0xd52be7639ec3e817L, 0xc7cbff6bc4237d01L,
0x3a28a85c97f20b00L, 0x53ca278b825630f2L, 0x2729410ed2863fa6L, 0xeaf9e5c0fe74b9b9L, 0xec1c701b73759bafL, 0xc6a3edbb27292bd0L, 0x8992a19c36bf05f5L,
0xbff14ae110acd925L, 0xdc66f3a4f472506dL, 0x54cbb74d404d78caL, 0xd1c2c87f30741ecfL, 0xa8b16475cdd8041cL, 0xc8dcc544c4d60b9bL, 0xc07a401e348678c1L,
0xd983bd0cea7a14a6L, 0x1c0e4a71639398f2L, 0x831f60904daa6cecL, 0xd39bf1e44f3611eeL, 0xf4b3f97ba3851f4eL, 0x4df4bf61b324062aL, 0x1f8b7adfe0cad1fbL,
0xa5c32f956d4cdd2eL, 0x9e3864512051a6fcL, 0xd6ad473be74c5608L, 0x7cfa74eafd51bbdbL, 0x262590fd3121b856L, 0x8062ed983774c45eL, 0x52363fbbc8e14b3bL,
0x922f0bac68ade882L, 0x79b0426954e22028L, 0xbe64279d6cd8e65aL, 0x136ca9b2515cbe18L, 0xf1f1380f29a259cL, 0x9c3679e1788b27f4L, 0xa4d3b61cfc566367L,
0xf33a5b3df3922853L, 0xfeefdf0072229b6aL, 0x28c8b6f207ff3132L, 0x6604ed049cc7dd6cL, 0x4425bbc7cfc4d559L, 0xf34553c57c30509eL, 0x77b48debbf27690dL,
0xd87e8c51144746ddL, 0x509633e137966ba8L, 0x4f25f94261e3077bL, 0xdeea61427f656fa0L, 0xe5cbf40678fa9696L, 0xf4e5ac9ebf4af583L, 0xb46abd905f09e033L,
0xbc814b1c97cc5cc0L, 0x63275ebfa5b70e39L, 0x781a8ef629eefc3bL, 0xb4767ac91432e441L, 0xfcc341f601afc8b1L, 0x17a831ef567feabL, 0xbe732f7e6089a6fcL,
0x60ffbbcadf21d268L, 0xe120fc9c889932b5L, 0xd5afd5a0c3ed74f6L, 0x6e290b8ede6dcf94L, 0xc88d8bcbe646103L, 0x844390c286f6e72L, 0x839701715a3c0764L,
0x2196cfa630fd61f9L, 0x9e435c31208a28f5L, 0xaabe53e52f4371d8L, 0x94badadfbb9649aL, 0x6b3fce74d9a35fd9L, 0x64ab6452f8fbab85L, 0x35c22c367b4da29aL,
0x46ad3aa1645daec2L, 0xc2e992942902009eL, 0x2c95c926602d58dbL, 0xd4015b291f462348L, 0x6ada11c6ade62010L, 0xa5cab39455b2301fL, 0xb0831359121f846dL,
0x4cfdfed7e6361c5L, 0x2956dfbff81f6cfaL, 0x4f56a9cd47603be0L, 0x13a7eda86ec3b4f0L, 0x2501e0c2cee590fbL, 0xeece6bbbcdba15a3L, 0x6d88ee8202dbe0a8L,
0x7799508e8d54ad78L, 0x644219e2442ac442L, 0x63daf707b860c962L, 0xc0cb22a440582e24L, 0x100a42cc8ee935c1L, 0xc5fde50b1290efabL, 0x90ae541606075f77L,
0xc844b757dcdad199L, 0x2a34d306e70dec6dL, 0xefb6a23ad93e143cL, 0x26eb34c92e1886c7L, 0x9b5a78a2a34027cL, 0x2c0bd34e98ebdd64L, 0x327534f23278d685L,
0xeead60fa6e49821fL, 0x2dae76f316d4266bL, 0xc55e80c759ffb5d9L, 0xc492d56350f1aa5eL, 0x8851b7fb384161feL, 0xada5230348f95383L, 0xfcddb3b09f6e2c8fL,
0xe3015e14ed74aaa2L, 0xa336277bb9b6565cL, 0x39be7efcf6d80e75L, 0x17a810034fe55d57L, 0xe5fad285c8f38a5aL, 0xcd0ae0f936f5c891L, 0xe6e04ade4d2c4b20L,
0xca0130280d85a234L, 0x6b0617ec9749f729L, 0x64844c91522bc90eL, 0x2551aa8cbd0cb230L, 0x9a7af49e60af5bc0L, 0x495f4269368be4b4L, 0xf048c3a743deb60eL,
0x14bf98f0d7cf70f0L, 0x715c82bbe10f77edL, 0x510192977d399061L, 0xaf1f5aac7fc7fdf1L, 0x4c094267f9c4756aL, 0xcbadd93b1b098b62L, 0x431bd04788585e70L,
0x2c6e3007391f3b7bL, 0xd38d628a49efa311L, 0x2ce85221e8cda8c6L, 0xfbcff3faa7568279L, 0x4347aee7fc96290L, 0x2433b3d9b6cc48d3L, 0x2bfec561ce058010L,
0x702d6aaf2a48f533L, 0x907a594309caa4b4L, 0xdc8ec3baa1e6d877L, 0x159a402b1730ff1aL, 0xa91fdd7c3ddb47f3L, 0x8783f5c4565d0620L, 0x16c6bb0809049f39L,
0x4cccaabc1e34cb46L, 0xd8e512e7fe4980b6L, 0xe41f11dfb6f778e2L, 0xa22b5802918e7ec4L, 0xde7ca8efb0a24f8eL, 0xcbc8af2cb8c22a20L, 0x9b40e135f6548b02L,
0x847686607b3e7eadL, 0xd10c337a138ee1c6L, 0xe5c7a2db8b491d11L, 0xf80695e8e8e6179fL, 0xb548af291a24611aL, 0x8f3106516288a076L, 0xf55b4ac53d6b5aaL,
0xc4d55067aa0817a9L, 0x70debea86a52e853L, 0x3f30068ec8e06d62L, 0xbc70fabab8db83fbL, 0x65c5c9e60ba76b1aL, 0x2eaf5d6b2093c905L, 0x1ba3d2f7a75a1e4bL,
0x47cee6cb54411833L, 0xc4867d4476327bb2L, 0x240dff3933e709d3L, 0x16ca25f60d751d29L, 0xe88c6a9989541accL, 0xfef71918de26f506L, 0x5872b502d1706814L,
0x578a07d6d7aacd81L, 0x83a8cfcc7726dc91L, 0xcad547c51524a4ebL, 0x9b1aff51e47d0e5fL, 0xe07d4642413453e6L, 0x5dbd75a15a7ded63L, 0xd9e18fb5591d7e28L,
0x817fa3791c15c224L, 0x4da0c2fe8134b98eL, 0x1f0f85b48d68aec6L, 0xf30e4e1068194743L, 0xc2c4fc7eca79c219L, 0x1b1d9527732449e2L, 0x9dc3e308aceeeef7L,
0xbc3cb6f1dc0dd1feL, 0x86c3257f1caedd75L, 0x5a9c43815d61d9d8L, 0x248d80d5a1e11db8L, 0x1829a02a44bf95eL, 0xf76947d67be14811L, 0x841fd84e0b1fb4bdL,
0x899228f52fd93036L, 0x821763afe12bcf3eL, 0xea12ff65048146b4L, 0x43f4f086da963187L, 0xced27366883b67dbL, 0x7b77b8f3c2b7ef5cL, 0x5e410fc30a4a3030L,
0x48a0e20885e9643aL, 0x129611972701aef8L, 0x3b71f5a9073fbc54L, 0xca9829c7e16369b2L, 0x741178a4933b84e7L, 0xbdbc812b5c0702efL, 0xf15d7c579030954aL,
0x4dd39973f8abb5ccL, 0x5342d0d383297c34L, 0x93390567cf51ac43L, 0x42cff0c1275ef473L, 0xe96673f0e1d5b51L, 0xa908550bf3ddfb82L, 0x766691414984420cL,
0xb3ab0dde32a46044L, 0x999105a9f3d89303L, 0xa89761c7acda2976L, 0x8f880af64ba51ff1L, 0x6684eb8e828aba97L, 0x4d9b22d4381c7a7dL, 0x44e7218ffedae540L,
0xa3f33a8118f9df51L, 0xdd02da5392f8bce5L, 0xf2e637f70a7bd6faL, 0x685c56ca8601c8d7L, 0xc397296903e0b3d8L, 0xe60938944726a567L, 0x4310f514547ef454L,
0x5dfac4e8f9079525L, 0xe7959c6fb2db7225L, 0xd5fd515306230758L, 0x10fc97a7c04e1164L, 0x78070ba095f35bcbL, 0xf6c9c95a2d706830L, 0x94f8aa664c4a986bL,
0x42e81d5f73b14808L, 0x3555355369fe0511L, 0x4cdd8e592dea00beL, 0x9a07d10187e9d922L, 0x7dfc5f23be689befL, 0x6363c19bd833be3aL, 0xfa5fd87b72d19842L,
0xb04ad66c611d175fL, 0x91741ff100b99c1dL, 0xc56e18fd9ab5a36bL, 0xfeedbe55c77d2fa7L, 0x4794d630c5a37ab0L, 0x76e222ebfe86f90eL, 0x5e2c57d0732447d2L,
0x381bb75600ce09d2L, 0x6c9717402ffe3ba2L, 0x18902a4af51c48a9L, 0x269d1b37528b2124L, 0x224f7d41cf40dff7L, 0x2352f6f753dc6642L, 0x5b4ede250fbc1636L,
0x74cb2ba3e8bc13b8L, 0x81568593ba104275L, 0xdde92001dfa00b43L, 0x4f69b8ab685b1572L, 0xc85994815a27f540L, 0x5d14b824e80a8837L, 0xe7464fdeb9175686L,
0xef2b4eb6ed076cc7L, 0x18292902e3d2b995L, 0x80f8e50e65c6b50cL, 0x4377e593f689fcd3L, 0x718308f2257c5a4aL, 0xe7760efe150ce5b7L, 0x40331d2efbad0cb5L,
0x3e470afbb520ed0eL, 0xe506eff66c6c6f71L, 0x4ca4b0f6b1879c40L, 0x107855a52e545ed9L, 0x7fce0d68d94bd43dL, 0xaf0ec27188d7cc2aL, 0x8873914b339b9ccL,
0xb57ee03fd3fe167fL, 0xec95848a10a1e5e8L, 0x6f7b03c77d1160ffL, 0x7118d0873170827L, 0xc884948c1a879296L, 0xb2089f20565b1eeeL, 0xd6bc971ba5339c2aL,
0xa0add6f0169c97ceL, 0xa0a161655b02e8a4L, 0xc2ba1e6c8f6085aaL, 0xed69ee66d34ce425L, 0x593ca3d97ad702eaL, 0x9b2421b199cc73ceL, 0xb551d3a26227db98L,
0x8b07722e75dfd5c9L, 0xaa452dc13abf5764L, 0xf7d43bf792638550L, 0xa4c1c175e95c759dL, 0x239ed738984ad333L, 0xae511a9446fe5bceL, 0xc73e09aba777968cL,
0xd0c90bdcfe44d96eL, 0x4275d7fb13b5f2acL, 0x450009baec26b3c1L, 0xfcfee1e655a45399L, 0x9bbaf97417a95a98L, 0x22d021f4ba7a0cdfL, 0xf4c71cda9576ed3fL,
0xfa50cb4b9a710535L, 0x4c1cce67bfe50996L, 0x228f7927dbf93d28L, 0x8c2d1755c2db918bL, 0x66e2a7e5f69e85aL, 0x8bf3c29d4c0e1378L, 0x3ca180c97ebb2753L,
0xe8281eae32ae6461L, 0xa8904793996680feL, 0x926d802424e90fafL, 0x6ae259d3fef7281fL, 0x35bb82723bfbe8L, 0x33d36bfefc9dcd0bL, 0x7c6af0d34dd15e95L,
0xc424807b4202cdfdL, 0x2b680080aeed83e5L, 0xc99745e2584f8502L, 0x7f404447bf648debL, 0xe78214a4596dbc39L, 0xa09d41483fa401b2L, 0x530b911bbedef6ceL,
0x71bbf802a2fb63f9L, 0x556cd8c8c54de6e1L, 0x18ff267414192349L, 0x2c698665bd940a75L, 0x45954752ac5b5848L, 0x65326ee82e047d93L, 0xd478e0f4d11e941aL,
0x78c9663ba382cc90L, 0x13da002a8183528fL, 0xe98178ef7e119c17L, 0xa065aae5ed3eb488L, 0xf4bfef383e5a6f8eL, 0x94a7bc6f603d9d71L, 0x47b1d61792510eb9L,
0x4cd360b953fc50a0L, 0xaee0d693b01d512bL, 0xf1543905aa2fc955L, 0x8d46c6cec0f4dc48L, 0x194aa9342ab7b23bL, 0x874dd65215ecb504L, 0x6913b3816b2ca9dfL,
0x2bea9eb1511b5e03L, 0x725f2765d2fe43e9L, 0x2ea0f3244a199966L, 0xde758065e5b479d1L, 0xb95ff0cff6dd3dc3L, 0xb4f60408da11725cL, 0xc2a989c5ca2e38c8L,
0xa559a2f7b9514266L, 0x7d6b7e2fcb46076L, 0x68a5778a2301e76fL, 0x37968b096b011d75L, 0x7ad2c0be6374568bL, 0xd3611797da951937L, 0xeda4d5aea91cffe6L,
0x75bc1b48b79f40daL, 0xeb8ec999e5a10de6L, 0x2e5439638c0da31aL, 0x7ed51971282a4c18L, 0x8d4c9f5b29eef7a3L, 0x410db03ff3af3f11L, 0x27f85f6c3a2bac59L,
0xda95e2a866d7ae37L, 0x975aeee7d887f577L, 0x3ddd4a0cb8d9d3daL, 0xd148485023b51eb5L, 0xe70054d5e1b64128L, 0x330b39d04e1b1d03L, 0xef0a822189dc482dL,
0x7f9c3c0173a916deL, 0x50faca6f5be07032L, 0x1c1aad0fe80bc0aaL, 0xe4ba689001b12029L, 0xdbe31a2065c0ce77L, 0x550f3add13358788L, 0x24850728b54a9f68L,
0xe0fda853071afb3aL, 0x33d0530b8d55881cL, 0x472f46330217873dL, 0x9a09d4f4fec9e2e1L, 0x97ae2c0ca3a6823eL, 0x4c41c3a8a5e2d323L, 0xb4099e19e1d35618L,
0xa9cd9f523008eb0dL, 0x422839ca22c4214fL, 0x8dbc13e80839fdaL, 0xbff27499cebd408cL, 0x62ea4226b89a419fL, 0x49d82458d023c44L, 0xb6029268fc9f9baL,
0xa51f2fcab9da2237L, 0x30d72da8a044b884L, 0xdd26b42f5f30548cL, 0x6bc9902a9b0dbe55L, 0xf8d9941bff236f31L, 0xda532cccbaed1eacL, 0x24adee97eae8eab6L,
0xc5bc00ab303e18aaL, 0xe5a0cd0c0ee68505L, 0xb803f83714ba7cd7L, 0xecd22db01daf4be2L, 0xb7e3013063dae0f9L, 0xbeb0d27d9d4f3696L, 0xea90ea8a0cafde39L,
0x55119ec1634a78a6L, 0xe9c876fbdffe6a17L, 0x7260102290559326L, 0x384838973877f11bL, 0xcf0f27ad9ba7d178L, 0x312bff809bf641bbL, 0x6388ed00860e8d6aL,
0x2e49ad1e66103db3L, 0x113a3b7d2ec68c65L, 0x6b9f11e7a1690d6fL, 0x9b86b9531ab34f79L, 0x75405ef8a4e9f72bL, 0xeda01eaab23ac77eL, 0x70720a185fa40fe5L,
0xc677f98b794a3a01L, 0xecc87a715d2173f6L, 0x67a98349635526ebL, 0x4af24668cb1e4c50L, 0x6a4c072e85b31a9bL, 0x7e18d95b990809e9L, 0xb0c1605246e3bf70L,
0x3fc3bf840eee727L, 0xdb95752d76d072eL, 0x85f320ef285deb3eL, 0x88c7dcd0faec9f0aL, 0x8a0b6ccc87650903L, 0xf8ebb1f5c46d8290L, 0xa7810ba50118278cL,
0x976038179d1f9339L, 0x19894d576bfbd737L, 0x501dd63dcb86834bL, 0x93e4cf249a7587f6L, 0x770d569777f4bd9bL, 0xb372e2a560d485d1L, 0x497db9849db462a5L,
0x7a6ee0583af3ca69L, 0x92639f3f6a0f4f9cL, 0x4e5389ea670bc9f8L, 0x5e3fdc068ed5e0c1L, 0x9df0fbe1b66f35d8L, 0xc41abef4990b7e75L, 0x56dc198bbc8234e2L,
0x5480eb6bd18af32dL, 0x5dd9b3bec27bc9caL, 0xc11dfd71a656802cL, 0x3c4074336061bf1fL, 0x88c43cb125091498L, 0xea8cafba3b3d3002L, 0xde44f7a617f86c3dL,
0xe7dd9a9993bab11fL, 0x3a5ea8d4c50e09f5L, 0x4ceaaeaa98ec9705L, 0xe744382cef118f0aL, 0xa8b49a27dae1ced3L, 0x735ff7f4efb65e24L, 0x203f8619fa47a35fL,
0x4556c13218c31160L, 0xc2bbe874fd32a70aL, 0xe4852647a1ca9794L, 0x1202b702d73c17fbL, 0x8e70a33d8c93a08bL, 0xba4edff51d76581bL, 0xfc002332455a1731L,
0x729591de81b7d061L, 0xdf9e9cc050e43e26L, 0x5062a3dfd1c3d976L, 0x8d8813975cccccddL, 0x35e55704f86f6a99L, 0x9fa64d1b1e405b7cL/*, 0x84d2208f5ef0c058L */
};

void Bitboard::apply_move(move_t move)
{
    unsigned char destrank, destfile, sourcerank, sourcefile;
    get_source(move, sourcerank, sourcefile);
    get_dest(move, destrank, destfile);
    piece_t destpiece = get_piece(destrank, destfile);
    piece_t sourcepiece = get_piece(sourcerank, sourcefile);
    piece_t resultpiece = sourcepiece;
    Color color = get_color(sourcepiece);

    /*
    //#ifdef DEBUG
//    std::ostringstream debugging;
    ((Fenboard*)this)->get_fen(std::cout);
    std::cout << " -- ";
    ((Fenboard*)this)->print_move(move, std::cout);
    //#endif
//    std::cout << std::endl << "new hash: " << hash << " - ";
*/
    set_piece(destrank, destfile, sourcepiece);
    set_piece(sourcerank, sourcefile, EMPTY);

    bool allows_enpassant = false;

    if ((sourcepiece & PIECE_MASK) == bb_king) {
        set_can_castle(color, true, false);
        set_can_castle(color, false, false);
        // castling
        if (sourcefile == 4 && (destfile == 2 || destfile == 6)) {
            unsigned char rooksourcefile = 8, rookdestfile = 8;
            piece_t castlerook = bb_rook | (color == Black ? BlackMask : 0);

            switch (destfile) {
            case 2:
                rooksourcefile = 0;
                rookdestfile = 3;
                break;
            case 6:
                rooksourcefile = 7;
                rookdestfile = 5;
                break;
            }
            if (rooksourcefile != 8) {
                set_piece(destrank, rookdestfile, castlerook);
                set_piece(sourcerank, rooksourcefile, EMPTY);
            }
        }
    }
    else if ((sourcepiece & PIECE_MASK) == bb_rook) {
        switch (sourcefile) {
        case 0:
            set_can_castle(color, false, false);
            break;
        case 7:
            set_can_castle(color, true, false);
            break;
        }
    }
    else if ((sourcepiece & PIECE_MASK) == bb_pawn) {
        // set enpassant capability
        if (abs(destrank - sourcerank) == 2) {
            allows_enpassant = true;
            set_enpassant_file(destfile);
        }
        // pawn promote
        else if (destrank == 0 || destrank == 7) {
            resultpiece = (get_promotion(move) & PIECE_MASK) | (color == Black ? BlackMask : 0);
            set_piece(destrank, destfile, resultpiece);
        }
        // enpassant capture
        else if (sourcefile != destfile && destpiece == EMPTY) {
            set_piece(sourcerank, destfile, EMPTY);
        }
    }

    if (!allows_enpassant) {
        set_enpassant_file(-1);
    }
    side_to_play = get_opposite_color(color);
    if (color == Black) {
        move_count++;
    }

    this->in_check = this->king_in_check(this->side_to_play);
    update();

#ifdef DEBUG
	if (piece_bitmasks[(bb_king + 1) + bb_king] == 0 || piece_bitmasks[bb_king] == 0) {
	    std::cout << debugging.str() << std::endl;
	}
#endif
	assert(piece_bitmasks[(bb_king + 1) + bb_king] > 0);
	assert(piece_bitmasks[bb_king] > 0);
    int found_count = 1;
    auto old_count = seen_positions.find(hash);
    if (old_count != seen_positions.end()) {
        found_count += old_count->second;
    }
    seen_positions[hash] = found_count;
//    std::cout << " -> " << hash << std::endl;
}

void Bitboard::undo_move(move_t move)
{
    unsigned char destrank, destfile, sourcerank, sourcefile;
    get_source(move, sourcerank, sourcefile);
    get_dest(move, destrank, destfile);

    // pop seen positions
//    std::cout << "old hash: " << hash << " - ";
    auto iter = seen_positions.find(hash);
//    bool fail = (iter == seen_positions.end());
    assert(iter != seen_positions.end());
//    if (!fail) {
        if (iter->second <= 1) {
            seen_positions.erase(iter);
        } else {
            seen_positions[hash]--;
        }

//    }


    piece_t moved_piece = get_piece(destrank, destfile);
    Color color = get_color(moved_piece);
    piece_t captured_piece = get_captured_piece(move);
    if (captured_piece != EMPTY) {
        captured_piece = make_piece(captured_piece & PIECE_MASK, get_opposite_color(color));
    }
    in_check = move & MOVE_FROM_CHECK;

    // flags
    side_to_play = color;

    if ((move & INVALIDATES_CASTLE_K) != 0) {
        set_can_castle(color, true, true);
    }
    if ((move & INVALIDATES_CASTLE_Q) != 0) {
        set_can_castle(color, false, true);
    }
    // promote
    piece_t promote = get_promotion(move);
    if (promote & PIECE_MASK) {
        moved_piece = make_piece(bb_pawn, color);
    }

    set_piece(sourcerank, sourcefile, moved_piece);
    set_piece(destrank, destfile, captured_piece);

    // castle: put the rook back
    if ((moved_piece & PIECE_MASK) == bb_king && sourcefile == 4 && (destfile == 2 || destfile == 6)) {
        int rook = make_piece(bb_rook, color);
        unsigned char rook_dest_file = 8, rook_source_file = 8;
        switch (destfile) {
            case 2:
                rook_source_file = 0;
                rook_dest_file = 3;
                break;
            case 6:
                rook_source_file = 7;
                rook_dest_file = 5;
                break;
        }
        set_piece(destrank, rook_source_file, rook);
        set_piece(destrank, rook_dest_file, EMPTY);
    }

    // en passant capture
    if (move & ENPASSANT_FLAG) {
        set_piece(sourcerank, destfile, make_piece(bb_pawn, get_opposite_color(color)));
    }
    int new_enpassant_file = (move >> 20) & 0xf;
    if (new_enpassant_file > 8) {
        new_enpassant_file = -1;
    }
    set_enpassant_file(new_enpassant_file);

    if (color == Black) {
        move_count--;
    }
    /*
    std::cout << " -> " << hash << std::endl;
    if (fail) {
        assert(false);
    }
    */
}

char fen_repr(unsigned char p)
{
    switch (p) {
    case EMPTY: return 'x';
    case bb_pawn: return 'P';
    case bb_knight: return 'N';
    case bb_bishop: return 'B';
    case bb_rook: return 'R';
    case bb_queen: return 'Q';
    case bb_king: return 'K';
    case bb_pawn | BlackMask: return 'p';
    case bb_knight | BlackMask: return 'n';
    case bb_bishop | BlackMask: return 'b';
    case bb_rook | BlackMask: return 'r';
    case bb_queen | BlackMask: return 'q';
    case bb_king | BlackMask: return 'k';
    default: abort(); return 'X';
    }
}

void Bitboard::update_zobrist_hashing_piece(unsigned char rank, unsigned char file, piece_t piece, bool adding)
{
    Color color = get_color(piece);
    int pt = (piece & PIECE_MASK) - 1;
    if (pt >= 0) {
        int posindex = file + rank * 8;
        hash ^= zobrist_hashes[posindex + pt * 64 + color * (64 * 6)];
    }
}

void Bitboard::update_zobrist_hashing_move()
{
    hash ^= zobrist_hashes[64*6*2];
}

void Bitboard::update_zobrist_hashing_castle(Color color, bool kingside, bool enabling)
{
    int index = 0;
    if (color == Black) {
        index += 1;
    }
    if (kingside) {
        index += 1;
    }
    hash ^= zobrist_hashes[64*6*2 + 1 + index];
}

void Bitboard::update_zobrist_hashing_enpassant(int file, bool enabling)
{
    if (file != -1) {
        assert(file >= 0 && file < 8);
        hash ^= zobrist_hashes[64*6*2 + 1 + 4 + file];
    }
}

