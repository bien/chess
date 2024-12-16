#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include "bitboard.hh"
#include "move.hh"
#include <cassert>
#include <type_traits>


constexpr int sign(int value) {
    if (value > 0) {
        return 1;
    } else {
        return -1;
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
    if (((source_piece & PIECE_MASK) == bb_pawn) && (srcfile != destfile) && (captured_piece == EMPTY)) {
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
    for (int i = 0; i < bb_all + 2 * (bb_king + 1); i++) {
        piece_bitmasks[i] &= ~bit;
    }
    if (piece != EMPTY) {
        int piece_type = piece & PIECE_MASK;
        Color color = (piece & BlackMask) == BlackMask ? Black : White;

        piece_bitmasks[color * (bb_king + 1) + bb_all] |= bit;
        piece_bitmasks[color * (bb_king + 1) + piece_type] |= bit;
    }
}

void Bitboard::next_pnk_move(Color color, piece_t piece_type, int &start_pos, uint64_t &dest_squares, int fl_flags, bool checks_only, bool exclude_checks) const
{
	/**
	FL_CAPTURES: consider attacks against enemy pieces
	FL_EMPTY: consider moves to empty squares
	FL_ALL: consider attacks against own side's pieces (defending own piece)
	*/
    uint64_t actors = piece_bitmasks[color * (bb_king + 1) + piece_type];
    uint64_t all_pieces = piece_bitmasks[bb_all] | piece_bitmasks[bb_all + (bb_king + 1)];

    while ((start_pos = get_low_bit(actors, start_pos)) > -1) {
        dest_squares = 0;

        if (fl_flags & (FL_CAPTURES | FL_ALL)) {
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
        if (fl_flags & (FL_EMPTY | FL_ALL)) {
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


BitboardMoveIterator::BitboardMoveIterator(Color color)
    : start_pos(-1), colored_piece_type(color * 8 + 1), dest_pos(0), captures_promote_type(0x8), processed_checks(false), dest_squares(0), covered_squares(0), king_slide_blockers(0xffffffffffffffff)

{
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

/* returns squares that are empty or occupied by opposite color pieces, and currently attacked by pieces of Color color */
uint64_t Bitboard::computed_covered_squares(Color color, uint64_t exclude_pieces, uint64_t include_pieces, uint64_t exclude_block_pieces) const
{
    uint64_t covered_squares = 0;
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
            covered_squares |= piece_covers;

            if (start_pos < 0) {
                break;
            }
        }
    }
    return covered_squares;
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

void BitboardMoveIterator::push_move_front(move_t move)
{
    inserted_move = move;
}

void BitboardMoveIterator::advance(const Bitboard *board)
{
    bool done = false;
    if (inserted_move != 0) {
        inserted_move = 0;
        return;
    }

    while (!done) {
        Color color = colored_piece_type > bb_king ? Black : White;
        uint64_t opponent_king_pos = board->piece_bitmasks[(1 - color) * (bb_king + 1) + bb_king];
        assert(opponent_king_pos > 0);
        // iterate through all the promotion types
        if (get_promote_type() > 0 && get_promote_type() < bb_queen) {
            set_promote_type(get_promote_type() + 1);
        }
        // iterate through all the possible destination squares for this piece
        else if (dest_squares >> (1 + dest_pos)) {
            dest_pos = get_low_bit(dest_squares, dest_pos + 1);
            set_promote_type(0);
            if (dest_pos < 0) {
                dest_squares = 0;
                continue;
            }
        }
        // find the next piece to move
        else {
            dest_squares = 0;
            int index = colored_piece_type;
            // since we skip position 7 in the bitboard masks...
            if (index > bb_king) {
                index --;
            }
            start_pos = get_low_bit(board->piece_bitmasks[index], start_pos+1);
            if (start_pos >= 0) {
                int actor = start_pos;
                int flags = in_captures() ? FL_CAPTURES : FL_EMPTY;
                if (!processed_checks) {
                    flags = FL_CAPTURES | FL_EMPTY;
                }
                switch(colored_piece_type & PIECE_MASK) {
                    case bb_king: case bb_pawn: case bb_knight:
                        board->next_pnk_move(color, colored_piece_type & PIECE_MASK, actor, dest_squares, flags, !processed_checks, processed_checks);
                        break;
                    case bb_bishop: case bb_rook: case bb_queen:
                        board->next_piece_slide(color, colored_piece_type & PIECE_MASK, actor, dest_squares, flags, 0, 0, 0, !processed_checks, processed_checks);
                        break;
                }
                start_pos = actor;
                if (dest_squares != 0) {
                    dest_pos = get_low_bit(dest_squares, 0);
                }
            }
            if (start_pos < 0 || dest_squares == 0) {
                colored_piece_type++;
                start_pos = -1;
                if (!processed_checks && (colored_piece_type & PIECE_MASK) > bb_king) {
                    processed_checks = true;
                    colored_piece_type = bb_pawn | (colored_piece_type & 0x8);
                }
                else if (!in_captures() && (colored_piece_type & PIECE_MASK) > bb_king) {
                    start_pos = 64;
                    break;
                } else if ((colored_piece_type & PIECE_MASK) > bb_king) {
                    colored_piece_type = bb_pawn | (colored_piece_type & 0x8);
                    captures_promote_type = 0;
                    start_pos = -1;
                }
            }
            set_promote_type(0);
        }
        if ((colored_piece_type & PIECE_MASK) == bb_pawn && (dest_pos <= 7 || dest_pos >= 56) && get_promote_type() == 0) {
            set_promote_type(bb_knight);
        }

        if (!board->in_check && king_slide_blockers == 0xffffffffffffffff) {
            uint64_t my_king = board->piece_bitmasks[bb_king + (bb_king + 1) * color];
            int king_square = get_low_bit(my_king, 0);
            king_slide_blockers = board->get_king_slide_blockers(king_square, color);
        }

        if (dest_squares > 0) {
            if (!board->in_check && (colored_piece_type & PIECE_MASK) != bb_king && !((1ULL << start_pos) & king_slide_blockers)) {
                break;
            }
            else {
                if (covered_squares == 0) {
                    covered_squares = board->computed_covered_squares(colored_piece_type > bb_king ? White : Black, 0, 0, 0);
                }

                if (board->is_not_illegal_due_to_check(color, colored_piece_type & PIECE_MASK, start_pos, dest_pos, covered_squares)) {
                    break;
                }
            }
        }
    }
}

move_t BitboardMoveIterator::get_move(const Bitboard *board) const
{
    if (inserted_move != 0) {
        return inserted_move;
    }
    return board->make_move(start_pos / 8, start_pos % 8, colored_piece_type, dest_pos / 8, dest_pos % 8, board->get_piece(dest_pos / 8, dest_pos % 8), get_promote_type());
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
