#include <cstring>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include "chess.hh"

bool is_legal_pos(BoardPos bp);
void get_vector(BoardPos origin, BoardPos bp, char &drank, char &dfile);
inline BoardPos add_vector(BoardPos bp, int delta_rank, int delta_file);

int get_castle_bit(Color color, bool kingside);

int64_t hash_keys[64 * 8];

template <class T>
T abs(T a)
{
	if (a < 0) {
		return -a;
	} else {
		return a;
	}
}

bool bitboard_getbit(uint64_t data, int pos)
{
	return data & (static_cast<unsigned long long>(1) << pos);
}

void bitboard_setbit(uint64_t &bitboard, int pos, bool value)
{
	if (value) {
		bitboard |= (static_cast<unsigned long long>(1) << pos);
	} else {
		bitboard &= ~(static_cast<unsigned long long>(1) << pos);
	}
}

int sign(int n)
{
	if (n < 0) {
		return -1;
	} else if (n > 0) {
		return 1;
	} else {
		return 0;
	}
}

Color get_opposite_color(piece_t piece)
{
	return piece & BlackMask ? White : Black;
}

unsigned char get_board_rank(BoardPos bp)
{
	return bp / MEMORY_FILES - (MEMORY_RANKS - LOGICAL_RANKS) / 2;
}

unsigned char get_board_file(BoardPos bp)
{
	return bp % MEMORY_FILES - (MEMORY_FILES - LOGICAL_FILES) / 2;
}

bool is_legal_pos(BoardPos bp)
{
	return (get_board_rank(bp) & 0x8) == 0 && (get_board_file(bp) & 0x8) == 0;
}

BoardPos get_source_pos(move_t move)
{
	return make_board_pos((move >> 12 & 0x7), (move >> 8) & 0x7);
}

BoardPos get_dest_pos(move_t move)
{
	return make_board_pos((move >> 4 & 0x7), move & 0x7);
}

BoardPos make_board_pos(int rank, int file)
{
	assert(rank >= 0 && rank < 8);
	assert(file >= 0 && file < 8);

	return ((MEMORY_RANKS - LOGICAL_RANKS) / 2 + rank) * MEMORY_FILES + (MEMORY_FILES - LOGICAL_FILES) / 2 + file;
}

void get_vector(BoardPos origin, BoardPos bp, char &drank, char &dfile)
{
	drank = get_board_rank(bp) - get_board_rank(origin);
	dfile = get_board_file(bp) - get_board_file(origin);
}

piece_t get_promotion(move_t move, Color color)
{
	return make_piece((move >> 24) & PIECE_MASK, color);
}

piece_t get_captured_piece(move_t move)
{
	return (move >> 16) & PIECE_MASK;
}

piece_t SimpleBoard::get_captured_piece(move_t move) const
{
	return get_captured_piece(move);
}

void SimpleBoard::get_source(move_t move, unsigned char &rank, unsigned char &file) const
{
	BoardPos bp = get_source_pos(move);
	rank = get_board_rank(bp);
	file = get_board_file(bp);
}
void SimpleBoard::get_dest(move_t move, unsigned char &rank, unsigned char &file) const
{
	BoardPos bp = get_dest_pos(move);
	rank = get_board_rank(bp);
	file = get_board_file(bp);
}

move_t SimpleBoard::make_move(unsigned char srcrank, unsigned char srcfile, unsigned char source_piece, unsigned char destrank, unsigned char destfile, unsigned char captured_piece, unsigned char promote) const
{
	return make_move(make_board_pos(srcrank, srcfile), source_piece, make_board_pos(destrank, destfile), captured_piece, promote);
}

move_t SimpleBoard::make_move(BoardPos source, piece_t source_piece, BoardPos dest, piece_t dest_piece, piece_t promote) const
{
	move_t move = get_board_rank(source) << 12 | get_board_file(source) << 8 | get_board_rank(dest) << 4 | get_board_file(dest);
	move |= (promote & PIECE_MASK) << 24;
	move |= (dest_piece & PIECE_MASK) << 16;
	if (((source_piece & PIECE_MASK) == PAWN) && (get_board_file(source) != get_board_file(dest)) && (dest_piece == EMPTY)) {
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
	if ((source_piece & PIECE_MASK) == KING) {
		if (can_castle(get_color(source_piece), true)) {
			invalidates_castle |= INVALIDATES_CASTLE_K;
		}
		if (can_castle(get_color(source_piece), false)) {
			invalidates_castle |= INVALIDATES_CASTLE_Q;
		}
	}
	else if ((source_piece & PIECE_MASK) == ROOK && can_castle(get_color(source_piece), false) && get_board_file(source) == 0) {
		invalidates_castle |= INVALIDATES_CASTLE_Q;
	}
	else if ((source_piece & PIECE_MASK) == ROOK && can_castle(get_color(source_piece), true) && get_board_file(source) == 7) {
		invalidates_castle |= INVALIDATES_CASTLE_K;
	}
	move |= invalidates_castle;

	return move;
}

BoardPos add_vector(BoardPos bp, int delta_rank, int delta_file)
{
	return bp + delta_file + delta_rank * MEMORY_FILES;
}

piece_t make_piece(piece_t type, Color color)
{
	if (color == Black && type != EMPTY) {
		return type | BlackMask;
	} else {
		return type;
	}
}

piece_t SimpleBoard::get_piece(unsigned char rank, unsigned char file) const
{
	return get_piece(make_board_pos(rank, file));
}

piece_t SimpleBoard::get_piece(BoardPos bp) const
{
	unsigned char twosquare = data[bp >> 1];
	if (bp & 0x1) {
		return twosquare & 0xf;
	} else {
		return twosquare >> 4;
	}
}

void SimpleBoard::set_piece(unsigned char rank, unsigned char file, piece_t piece)
{
	BoardPos bp = make_board_pos(rank, file);
	set_piece(bp, piece);
}

void SimpleBoard::set_piece(BoardPos bp, piece_t piece)
{
	unsigned char twosquare = data[bp >> 1];
	if (bp & 0x1) {
		twosquare = (twosquare & 0xf0) | piece;
	} else {
		twosquare = (piece << 4) | (twosquare & 0x0f);
	}
	data[bp >> 1] = twosquare;
}


SimpleBoard::SimpleBoard()
{
	reset();
}

void SimpleBoard::reset()
{
	standard_initial();
	side_to_play = White;
	castle = 0x0f;
	in_check = false;
	enpassant_file = -1;
	move_count = 0;
}

SimpleBoard::SimpleBoard(const SimpleBoard &copy)
	: in_check(copy.in_check)
{
	this->side_to_play = copy.side_to_play;
	this->castle = copy.castle;
	this->enpassant_file = copy.enpassant_file;
	this->move_count = copy.move_count;
	memcpy(this->data, copy.data, sizeof(data));
}

int SimpleBoard::get_castle_bit(Color color, bool kingside) const
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

bool SimpleBoard::can_castle(Color color, bool kingside) const
{
	int bit = get_castle_bit(color, kingside);
	return castle & (1 << bit);
}

void FenBoard::invalidate_castle(Color color)
{
	invalidate_castle_side(color, true);
	invalidate_castle_side(color, false);
}

void FenBoard::invalidate_castle_side(Color color, bool kingside)
{
	if (can_castle(color, kingside)) {
		update_zobrist_hashing_castle(color, kingside, false);
	}
	castle = castle & ~(1 << get_castle_bit(color, kingside));
}


const unsigned char initial_board[] = { 0x84, 0x23, 0x56, 0x32, 0x48,
								0x81, 0x11, 0x11, 0x11, 0x18,
								0x80, 0x00, 0x00, 0x00, 0x08,
								0x80, 0x00, 0x00, 0x00, 0x08,
								0x80, 0x00, 0x00, 0x00, 0x08,
								0x80, 0x00, 0x00, 0x00, 0x08,
								0x89, 0x99, 0x99, 0x99, 0x98,
								0x8c, 0xab, 0xde, 0xba, 0xc8
};

void SimpleBoard::standard_initial()
{
	memset(data, 0x88, sizeof(data));
	memcpy(&data[MEMORY_FILES * 2 / 2], initial_board, sizeof(initial_board));
}

void SimpleBoard::repeated_move(BoardPos base, piece_t piece, char drank, char dfile, Color capture, std::vector<move_t> &moves) const
{
	BoardPos pos = base;
	while (true) {
		pos = add_vector(pos, drank, dfile);
		piece_t owner = get_piece(pos);
		if (owner == EMPTY) {
			moves.push_back(make_move(base, piece, pos, owner, 0));
		} else if ((owner & 0x7) && ((owner & 0x8) != 0) == (capture == Black)) {
			// found what we want to capture
			moves.push_back(make_move(base, piece, pos, owner, 0));
			break;
		} else {
			// found wrong color or edge of the board
			break;
		}
	}
}

BoardPos SimpleBoard::get_capture(BoardPos base, char drank, char dfile, Color capture) const
{
	BoardPos pos = base;
	while (true) {
		pos = add_vector(pos, drank, dfile);
		piece_t owner = get_piece(pos);
		if (owner == EMPTY) {
			continue;
		} else if (((owner & 0x8) != 0) == (capture == Black)) {
			// found what we want to capture
			return pos;
		} else {
			// found wrong color or edge of the board
			return InvalidPos;
		}
	}
}

void SimpleBoard::single_move(BoardPos base, piece_t piece, char drank, char dfile, Color capture, std::vector<move_t> &moves) const
{
	BoardPos pos = base;
	pos = add_vector(pos, drank, dfile);
	piece_t owner = get_piece(pos);
	if (is_legal_pos(pos) && (owner == EMPTY || (((owner & 0x8) != 0) == (capture == Black)))) {
		// found what we want to capture
		moves.push_back(make_move(base, piece, pos, owner, 0));
	}
}

void SimpleBoard::pawn_move(BoardPos source, BoardPos dest, std::vector<move_t> &moves) const
{
	piece_t pawn = get_piece(source);
	piece_t capture = get_piece(dest);
	if (get_board_rank(dest) == 0 || get_board_rank(dest) == 7)
	{
		moves.push_back(make_move(source, pawn, dest, capture, QUEEN));
		moves.push_back(make_move(source, pawn, dest, capture, KNIGHT));
		moves.push_back(make_move(source, pawn, dest, capture, ROOK));
		moves.push_back(make_move(source, pawn, dest, capture, BISHOP));
	}
	else {
		moves.push_back(make_move(source, pawn, dest, capture, 0));
	}
}
void SimpleBoard::pawn_capture(BoardPos bp, char dfile, Color piece_color, bool support_mode, std::vector<move_t> &moves) const
{
	BoardPos dest = add_vector(bp, piece_color == White ? 1 : -1, dfile);
	piece_t square = get_piece(dest);
	Color capture_color = get_opposite_color(piece_color);
	Color target_piece_color = get_color(square);
	if ((support_mode && is_legal_pos(dest)) || ((square & PIECE_MASK) != 0 && capture_color == target_piece_color))
	{
		pawn_move(bp, dest, moves);
	}
	// en passant captures
	else if (square == EMPTY && enpassant_file == get_board_file(dest) && get_board_rank(bp) == (piece_color == White ? 4 : 3) && piece_color == side_to_play)
	{
		pawn_move(bp, dest, moves);
	}
}

void SimpleBoard::pawn_advance(BoardPos bp, Color piece_color, std::vector<move_t> &moves) const
{
	int direction = piece_color == White ? 1 : -1;
	BoardPos dest = add_vector(bp, direction, 0);
	if (get_piece(dest) == EMPTY) {
		pawn_move(bp, dest, moves);
		if ((get_board_rank(bp) == 1 && piece_color == White) || (get_board_rank(bp) == 6 && piece_color == Black)) {
			dest = add_vector(dest, direction, 0);
			// push two squares
			if (get_piece(dest) == EMPTY) {
				moves.push_back(make_move(bp, make_piece(PAWN, piece_color), dest, EMPTY, 0));
			}
		}
	}
}

void SimpleBoard::calculate_moves(Color color, BoardPos bp, piece_t piece, std::vector<move_t> &moves, bool exclude_pawn_advance, bool support_mode) const
{
	if (((color == White && (piece & 0x08) == 0) || (color == Black && (piece & 0x08))))
	{
		Color color_capture;
		if (support_mode) {
			color_capture = color;
		} else {
			color_capture = get_opposite_color(color);
		}
		switch (piece & 0x07)
		{
			case ROOK:
				repeated_move(bp, piece, 1, 0, color_capture, moves);
				repeated_move(bp, piece, -1, 0, color_capture, moves);
				repeated_move(bp, piece, 0, 1, color_capture, moves);
				repeated_move(bp, piece, 0, -1, color_capture, moves);
				break;
			case BISHOP:
				repeated_move(bp, piece, 1, 1, color_capture, moves);
				repeated_move(bp, piece, -1, 1, color_capture, moves);
				repeated_move(bp, piece, -1, -1, color_capture, moves);
				repeated_move(bp, piece, 1, -1, color_capture, moves);
				break;
			case QUEEN:
				repeated_move(bp, piece, 1, 0, color_capture, moves);
				repeated_move(bp, piece, -1, 0, color_capture, moves);
				repeated_move(bp, piece, 0, 1, color_capture, moves);
				repeated_move(bp, piece, 0, -1, color_capture, moves);
				repeated_move(bp, piece, 1, 1, color_capture, moves);
				repeated_move(bp, piece, -1, 1, color_capture, moves);
				repeated_move(bp, piece, -1, -1, color_capture, moves);
				repeated_move(bp, piece, 1, -1, color_capture, moves);
				break;
			case KING:
				single_move(bp, piece, 1, 0, color_capture, moves);
				single_move(bp, piece, -1, 0, color_capture, moves);
				single_move(bp, piece, 0, 1, color_capture, moves);
				single_move(bp, piece, 0, -1, color_capture, moves);
				single_move(bp, piece, 1, 1, color_capture, moves);
				single_move(bp, piece, -1, 1, color_capture, moves);
				single_move(bp, piece, -1, -1, color_capture, moves);
				single_move(bp, piece, 1, -1, color_capture, moves);
				if (can_castle(color, true) && ((bp == make_board_pos(0, 4) && color == White) || (bp == make_board_pos(7, 4) && color == Black))) {
					// king-side castling
					BoardPos kingrook = get_capture(bp, 0, 1, color);
					if (kingrook == make_board_pos(get_board_rank(bp), 7) && (get_piece(kingrook) & PIECE_MASK) == ROOK) {
						moves.push_back(make_move(bp, piece, make_board_pos(get_board_rank(bp), 6), EMPTY, 0));
					}
				}
				else if (can_castle(color, false) && ((bp == make_board_pos(0, 4) && color == White) || (bp == make_board_pos(7, 0) && color == Black))) {
					// queen-side castling
					BoardPos queenrook = get_capture(bp, 0, -1, color);
					if (queenrook == make_board_pos(get_board_rank(bp), 0) && (get_piece(queenrook) & PIECE_MASK) == ROOK) {
						moves.push_back(make_move(bp, piece, make_board_pos(get_board_rank(bp), 2), EMPTY, 0));
					}
				}
				break;
			case KNIGHT:
				single_move(bp, piece, 1, 2, color_capture, moves);
				single_move(bp, piece, 2, 1, color_capture, moves);
				single_move(bp, piece, -1, 2, color_capture, moves);
				single_move(bp, piece, -2, 1, color_capture, moves);
				single_move(bp, piece, -1, -2, color_capture, moves);
				single_move(bp, piece, -2, -1, color_capture, moves);
				single_move(bp, piece, 1, -2, color_capture, moves);
				single_move(bp, piece, 2, -1, color_capture, moves);
				break;
			case PAWN:
				pawn_capture(bp, 1, color, support_mode, moves);
				pawn_capture(bp, -1, color, support_mode, moves);
				if (!exclude_pawn_advance) {
					pawn_advance(bp, color, moves);
				}
				break;
		}
	}
}

void SimpleBoard::get_moves(Color color, std::vector<move_t> &moves, bool support_mode, piece_t piece) const
{
	piece_t colored_piece = make_piece(piece, color);
	for (BoardPos i = (make_board_pos(0, 0) & 0xfe); i <= (make_board_pos(7, 7) | 0x1); i += 2)
	{
		unsigned char twosquare = data[i >> 1];
		if (twosquare & 0x77) {
			if ((color == Black && (twosquare & 0x88)) || (color == White && ((twosquare & 0x88) != 0x88))) {
				if (piece == 0) {
					calculate_moves(color, i, twosquare >> 4, moves, support_mode, support_mode);
					calculate_moves(color, i+1, twosquare & 0xf, moves, support_mode, support_mode);
				} else {
					if (twosquare >> 4 == colored_piece) {
						calculate_moves(color, i, twosquare >> 4, moves, support_mode, support_mode);
					}
					if ((twosquare & 0xf) == colored_piece) {
						calculate_moves(color, i+1, twosquare & 0xf, moves, support_mode, support_mode);
					}
				}
			}
		}
	}
}

void SimpleBoard::legal_moves(Color color, std::vector<move_t> &moves, piece_t piece_to_limit) const
{
	get_moves(color, moves, false, piece_to_limit);
	uint64_t covered_squares = 0;
	for (unsigned int i = 0; i < moves.size(); i++)
	{
		bool is_illegal = false;
		// special rules for king moves
		if ((get_piece(get_source_pos(moves[i])) & PIECE_MASK) == KING) {
			// lazily compute covered_squares once for all moves
			if (covered_squares == 0) {
				std::vector<move_t> opposite_color_moves;
				SimpleBoard king_free(*this);
				king_free.set_piece(get_source_pos(moves[i]), EMPTY);
				king_free.get_moves(get_opposite_color(color), opposite_color_moves, true);
				for (unsigned int j = 0; j < opposite_color_moves.size(); j++) {
					BoardPos bp = get_dest_pos(opposite_color_moves[j]);
					bitboard_setbit(covered_squares, get_board_rank(bp) * 8 + get_board_file(bp), 1);
				}
			}
			BoardPos dest = get_dest_pos(moves[i]);
			BoardPos source = get_source_pos(moves[i]);
			// don't move into check
			if (bitboard_getbit(covered_squares, get_board_rank(dest) * 8 + get_board_file(dest))) {
				is_illegal = true;
			}
			else if (abs(get_board_file(source) - get_board_file(dest)) == 2) {
				// it's a castle: don't move out of check or through check
				if (in_check || bitboard_getbit(covered_squares, get_board_rank(dest) * 8 + get_board_file(source) + (get_board_file(dest) - get_board_file(source)) / 2)) {
					is_illegal = true;
				}
			}
		}
		else if (!in_check && discovers_check(moves[i], color)) {
			is_illegal = true;
		}
		else if (in_check && !removes_check(moves[i], color)) {
			is_illegal = true;
		}

		if (is_illegal) {
			moves.erase(moves.begin() + i);
			i--;
		}
	}
}

void SimpleBoard::update()
{
	in_check = king_in_check(side_to_play);
}

BoardPos SimpleBoard::find_piece(piece_t piece) const
{
	for (BoardPos i = (make_board_pos(0, 0) & 0xfe); i <= (make_board_pos(7, 7) | 0x1); i += 2)
	{
		unsigned char twosquare = data[i >> 1];
		if (twosquare >> 4 == piece) {
			return i;
		} else if ((twosquare & 0xf) == piece) {
			return i + 1;
		}
	}
	return InvalidPos;
}

bool get_unit_vector(char drank, char dfile, char &unit_drank, char &unit_dfile)
{
	if (drank == 0) {
		unit_dfile = sign(dfile);
		unit_drank = 0;
	} else if (dfile == 0) {
		unit_drank = sign(drank);
		unit_dfile = 0;
	} else if (abs(drank) == abs(dfile)) {
		unit_drank = sign(drank);
		unit_dfile = sign(dfile);
	} else {
		return false;
	}
	return true;
}

// color indicates which king would be checked
bool SimpleBoard::discovers_check(move_t move, Color color) const
{
	BoardPos source = get_source_pos(move);
	BoardPos king = find_piece(make_piece(KING, color));
	char drank, dfile;
	get_vector(king, source, drank, dfile);
	char unit_drank = 0, unit_dfile = 0;
	bool simple_vector = get_unit_vector(drank, dfile, unit_drank, unit_dfile);
	if (!simple_vector) {
		return false;
	}

	// check whether there is clear line of sight from source to king
	BoardPos piece_to_king = get_capture(source, -unit_drank, -unit_dfile, get_color(get_piece(king)));
	piece_t piece_to_check;
	if (piece_to_king != king) {
		return false;
	}
	// follow line of sight to candidate capturing piece
	BoardPos place_to_check = get_capture(source, unit_drank, unit_dfile, get_opposite_color(get_piece(king)));

	// is the destination of the moving piece between king and place_to_check?
	char capture_drank, capture_dfile;
	char destdrank, destdfile, unit_destdrank, unit_destdfile;
	get_vector(king, place_to_check, capture_drank, capture_dfile);
	get_vector(king, get_dest_pos(move), destdrank, destdfile);
	if (get_unit_vector(destdrank, destdfile, unit_destdrank, unit_destdfile)) {
		if (abs(destdrank) <= abs(capture_drank) && abs(destdfile) <= abs(capture_dfile) && unit_destdrank == unit_drank && unit_destdfile == unit_dfile) {
			return false;
		}
	}

	// can the piece now attack the king?
	piece_to_check = get_piece(place_to_check);
	switch (piece_to_check & PIECE_MASK) {
		case BISHOP:
			return unit_drank != 0 && unit_dfile != 0;
		case ROOK:
			return unit_drank == 0 || unit_dfile == 0;
		case QUEEN:
			return true;
	}
	return false;
}


bool SimpleBoard::removes_check(move_t move, Color color) const
{
	// to remove check, the move must a) move the king, b) capture the threatening piece, or c) block a threatening piece
	//  these are not sufficient conditions since they do not account for double check or discovering another check
	BoardPos destpos = get_dest_pos(move);
	BoardPos sourcepos = get_source_pos(move);
	piece_t destpiece = get_piece(destpos);
	piece_t sourcepiece = get_piece(sourcepos);

	BoardPos king = find_piece(make_piece(KING, color));
	bool might_remove_check = false;

	// captures
	if (destpiece != EMPTY || (move & ENPASSANT_FLAG)) {
		if ((move & ENPASSANT_FLAG) != 0) {
			destpos = make_board_pos(get_board_rank(sourcepos), get_board_file(destpos));
			destpiece = get_piece(destpos);
		}
		std::vector<move_t> precluded_moves;
		calculate_moves(get_opposite_color(color), destpos, destpiece, precluded_moves, true, false);
		for (unsigned int i = 0; i < precluded_moves.size(); i++) {
			if (get_dest_pos(precluded_moves[i]) == king) {
				might_remove_check = true;
				break;
			}
		}
	}

	// blocks
	if (!might_remove_check && discovers_check(make_move(destpos, destpiece, sourcepos, sourcepiece, 0), color)) {
		might_remove_check = true;
	}

	if (!might_remove_check) {
		return false;
	}

	SimpleBoard copy(*this);
	copy.apply_move(move);
	return !copy.king_in_check(color);
}

bool SimpleBoard::king_in_check(Color color) const
{
	BoardPos king = find_piece(make_piece(KING, color));
	std::vector<move_t> opponent_moves;
	opponent_moves.reserve(32);
	for (piece_t piece_type = 1; piece_type <= 6; piece_type++) {
		calculate_moves(color, king, make_piece(piece_type, color), opponent_moves, true, false);
		for (unsigned int i = 0; i < opponent_moves.size(); i++) {
			BoardPos attacker = get_dest_pos(opponent_moves[i]);
			if (get_piece(attacker) == make_piece(piece_type, get_opposite_color(color))) {
				return true;
			}
		}
		opponent_moves.clear();
	}
	return false;
}
