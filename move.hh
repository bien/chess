#ifndef MOVE_H_
#define MOVE_H_

typedef unsigned char piece_t;
const piece_t bb_all = 0;
const piece_t bb_pawn = 1;
const piece_t bb_knight = 2;
const piece_t bb_bishop = 3;
const piece_t bb_rook = 4;
const piece_t bb_queen = 5;
const piece_t bb_king = 6;
const int PIECE_MASK = 7;
const int WhiteMask = 0;
const int BlackMask = 8;

const int ACTOR_OFFSET = 19;
const int ACTOR_MASK = 0x0380000;
const int CAPTURE_PIECE_POS = 12;
const int PROMO_PIECE_POS = 15;
const int ENPASSANT_FLAG = 0x38000;
const int MOVE_FROM_CHECK = 0x40000;
const int UNUSED_MASK = 0x01c00000;
const int ENPASSANT_POS = 25;
const int ENPASSANT_STATE_MASK = 0xf << ENPASSANT_POS;
const int INVALIDATES_CASTLE_K = 0x20000000;
const int INVALIDATES_CASTLE_Q = 0x40000000;
const int GIVES_CHECK = 0x80000000;

typedef unsigned int move_t;
typedef unsigned char BoardPos;
enum Color { White = 0, Black = 1 };
const int EMPTY = 0;
const int INVALID = 8;

static constexpr Color get_opposite_color(Color color) {
    return color == White ? Black : White;
}

BoardPos get_source_pos(move_t move);
BoardPos get_dest_pos(move_t move);

unsigned char get_board_rank(BoardPos bp);
unsigned char get_board_file(BoardPos bp);

static constexpr piece_t make_piece(piece_t type, Color color)
{
    if (color == Black && type != EMPTY) {
        return type | BlackMask;
    } else {
        return type;
    }
}

constexpr BoardPos make_board_pos(int rank, int file)
{
    return rank * 8 + file;
}

constexpr BoardPos algebra_to_square(char file, int rank)
{
    return make_board_pos(rank - 1, file - 'a');
}

piece_t get_captured_piece(move_t move, Color color=White);
piece_t get_promotion(move_t move, Color color=White);
void get_source(move_t move, unsigned char &rank, unsigned char &file);
void get_dest(move_t move, unsigned char &rank, unsigned char &file);

bool get_invalidates_queenside_castle(move_t move);
bool get_invalidates_kingside_castle(move_t move);

const int LOGICAL_RANKS = 8;
const int LOGICAL_FILES = 8;
const int MEMORY_RANKS = 8;
const int MEMORY_FILES = 8;

inline BoardPos get_source_pos(move_t move)
{
    return make_board_pos(((move >> 9) & 0x7), (move >> 6) & 0x7);
}

constexpr piece_t get_actor(move_t move)
{
    return (move & ACTOR_MASK) >> ACTOR_OFFSET;
}

inline BoardPos get_dest_pos(move_t move)
{
    return make_board_pos(((move >> 3) & 0x7), move & 0x7);
}

inline unsigned char get_board_rank(BoardPos bp)
{
    return bp / MEMORY_FILES - (MEMORY_RANKS - LOGICAL_RANKS) / 2;
}

inline unsigned char get_board_file(BoardPos bp)
{
    return bp % MEMORY_FILES - (MEMORY_FILES - LOGICAL_FILES) / 2;
}


#endif
