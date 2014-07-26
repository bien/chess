#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include "chess.hh"

const BoardPos InvalidPos = 8;
const int CAPTURED_PIECE_MASK = 0x70000;
const int MOVE_FROM_CHECK = 0x80000;
const int ENPASSANT_STATE_MASK = 0xf00000;
const int PROMOTE_MASK = 0x7000000;
const int ENPASSANT_FLAG = 0x8000000;
const int INVALIDATES_CASTLE = 0x10000000;

unsigned char get_board_rank(BoardPos bp);
unsigned char get_board_file(BoardPos bp);
bool is_legal_pos(BoardPos bp);
void get_vector(BoardPos origin, BoardPos bp, char &drank, char &dfile);
inline BoardPos add_vector(BoardPos bp, int delta_rank, int delta_file);

BoardPos get_source_pos(move_t move);
BoardPos get_dest_pos(move_t move);

int get_castle_bit(Color color, bool kingside);

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

Color get_color(piece_t piece)
{
	return piece & BlackMask ? Black : White;
}

Color get_opposite_color(piece_t piece)
{
	return piece & BlackMask ? White : Black;
}

Color get_opposite_color(Color color)
{
	return color == White ? Black : White;
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
	if (file < 0 || file >= 8) {
		std::cout << "break here" << std::endl;
	}
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

piece_t get_captured_piece(move_t move, Color color)
{
	return make_piece((move >> 16) & PIECE_MASK, color);
}

move_t Board::make_move(BoardPos source, piece_t source_piece, BoardPos dest, piece_t dest_piece, piece_t promote) const
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
	if (enpassant_target != -1) {
		move |= (0xf & enpassant_target) << 20;
	} else {
		move |= ENPASSANT_STATE_MASK;
	}
	bool invalidates_castle = false;
	if ((source_piece & PIECE_MASK) == KING && (can_castle(get_color(source_piece), true) || can_castle(get_color(source_piece), false))) {
		invalidates_castle = true;
	}
	else if ((source_piece & PIECE_MASK) == ROOK && can_castle(get_color(source_piece), false) && get_board_file(source) == 0) {
		invalidates_castle = true;
	}
	else if ((source_piece & PIECE_MASK) == ROOK && can_castle(get_color(source_piece), true) && get_board_file(source) == 7) {
		invalidates_castle = true;
	}
	if (invalidates_castle) {
		move |= INVALIDATES_CASTLE;
	}

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

piece_t Board::get_piece(BoardPos bp) const
{
	unsigned char twosquare = data[bp >> 1];
	if (bp & 0x1) {
		return twosquare & 0xf;
	} else {
		return twosquare >> 4;
	}
}

void Board::set_piece(BoardPos bp, piece_t piece)
{
	unsigned char twosquare = data[bp >> 1];
	if (bp & 0x1) {
		twosquare = (twosquare & 0xf0) | piece;
	} else {
		twosquare = (piece << 4) | (twosquare & 0x0f);
	}
	data[bp >> 1] = twosquare;
}

move_t Board::invalid_move(const std::string &s) const
{
	std::cerr << "Can't read move " << s << std::endl;
	std::cerr << *this << std::endl;
	abort();
}

void Board::set_fen(const std::string &fen)
{
	unsigned char rank = 8, file = 1;
	unsigned int pos = 0;
	while (pos < fen.length()) {
		char c = fen[pos++];
		if (c == ' ') {
			break;
		}
		else if (c == '/') {
			rank--; 
			file = 1; 
			continue;
		}
			
		BoardPos bp = make_board_pos(rank-1, file-1);
		switch (c) {
			case 'p': set_piece(bp, PAWN | BlackMask); break;
			case 'r': set_piece(bp, ROOK | BlackMask); break;
			case 'n': set_piece(bp, KNIGHT | BlackMask); break;
			case 'b': set_piece(bp, BISHOP | BlackMask); break;
			case 'q': set_piece(bp, QUEEN | BlackMask); break;
			case 'k': set_piece(bp, KING | BlackMask); break;
			case 'P': set_piece(bp, PAWN); break;
			case 'R': set_piece(bp, ROOK); break;
			case 'N': set_piece(bp, KNIGHT); break;
			case 'B': set_piece(bp, BISHOP); break;
			case 'Q': set_piece(bp, QUEEN); break;
			case 'K': set_piece(bp, KING); break;
		}
		if (c > '0' && c < '9') {
			for (int i = 0; i < c - '0'; i++) {
				set_piece(make_board_pos(rank-1, file-1), EMPTY);
				file++;
			}
		} else {
			file++;
		}
	}
	
	char c; 
	// next comes the color
	while ((c = fen[pos++]) != 0 && c != ' ') {
		switch(c) {
			case 'w': side_to_play = White; break;
			case 'b': side_to_play = Black; break;
			case ' ': break;
			case 0: return;
			default: std::cerr << "Can't read fen" << fen << std::endl; abort();
		}
	}
	if (c == 0) {
		return;
	}
	castle = 0;
	while ((c = fen[pos++]) != 0 && c != ' ') {
		// castle status
		switch(c) {
			case 'k': castle |= 1 << get_castle_bit(Black, true); break;
			case 'K': castle |= 1 << get_castle_bit(White, true); break;
			case 'q': castle |= 1 << get_castle_bit(Black, false); break;
			case 'Q': castle |= 1 << get_castle_bit(White, false); break;
			case '-': case ' ': break;
			case 0: return;
			default: std::cerr << "Can't read fen" << fen << std::endl; abort();
		}
	}
	
	if (c == 0) {
		return;
	}
	
	if ((c = fen[pos++]) != 0) {
		if (c >= 'a' && c <= 'h') {
			enpassant_target = c - 'a';
		}
	}
	
}

unsigned char read_piece(unsigned char c)
{
	switch (c) {
		case 'N': return KNIGHT;
		case 'R': return ROOK; 
		case 'B': return BISHOP; 
		case 'Q': return QUEEN; 
		case 'K': return KING; 
		case 'P': return PAWN; 
	}
	return 0;
}

move_t Board::read_move(const std::string &s, Color color) const
{
	int pos = 0;
	bool castle = false;
	bool queenside_castle = false;
	bool check = false;
	
	piece_t piece;
	int srcrank = INVALID;
	int srcfile = INVALID;
	int destfile = INVALID;
	int destrank = INVALID;
	int promotion = 0;

	std::vector<move_t> candidates;
	
	if (s[pos] == 'O') {
		castle = true;
	}
	else if (s[pos] >= 'A' && s[pos] <= 'Z') {
		piece = read_piece(s[pos++]);
	} else {
		piece = PAWN;
	}
	if (castle) {
		if (s[++pos] != '-') {
			invalid_move(s);
		}
		if (s[++pos] != 'O') {
			invalid_move(s);
		}
		if (s[pos + 1] == '-' && s[pos + 2] == 'O') {
			queenside_castle = true;
			pos += 2;
		}
		if (s[pos+1] == '+') {
			check = true;
		}
	} else {
		while (s[pos] != 0) {
			if (s[pos] >= '1' && s[pos] <= '8') {
				if (destrank != INVALID) {
					srcrank = destrank;
				}
				destrank = s[pos] - '1';
			}
			else if (s[pos] >= 'a' && s[pos] <= 'h') {
				if (destfile != INVALID) {
					srcfile = destfile;
				}
				destfile = s[pos] - 'a';
			}
			else if (s[pos] == '+') {
				check = true;
			}
			else if (s[pos] == '=') {
				switch (s[pos + 1]) {
					case 'q': case 'Q': promotion = QUEEN; break;
					case 'r': case 'R': promotion = ROOK; break;
					case 'n': case 'N': promotion = KNIGHT; break;
					case 'b': case 'B': promotion = BISHOP; break;
					default: invalid_move(s); break;
				}
				pos += 1;
			} else if (s[pos] != 'x') {
				invalid_move(s);
			}
			pos++;
		}
	}
	if (castle) {
		piece_t piece = make_piece(KING, color);
		if (queenside_castle) {
			if (color == White) {
				return make_move(make_board_pos(0, 4), piece, make_board_pos(0, 2), EMPTY, 0);
			} else {
				return make_move(make_board_pos(7, 4), piece, make_board_pos(7, 2), EMPTY, 0);
			}
		} else {
			if (color == White) {
				return make_move(make_board_pos(0, 4), piece, make_board_pos(0, 6), EMPTY, 0);
			} else {
				return make_move(make_board_pos(7, 4), piece, make_board_pos(7, 6), EMPTY, 0);
			}
		}
	} else {
		legal_moves(color, candidates, piece);
		for (unsigned int i = 0; i < candidates.size(); i++) {
			move_t move = candidates[i];
			BoardPos cdestpos = get_dest_pos(move);
			BoardPos csourcepos = get_source_pos(move);
			piece_t csourcepiece = get_piece(csourcepos);
			piece_t cpromote = get_promotion(move, color);
			if (piece == (PIECE_MASK & csourcepiece) && (srcrank == INVALID || srcrank == get_board_rank(csourcepos)) && (srcfile == INVALID || srcfile == get_board_file(csourcepos)) && (destrank == INVALID || destrank == get_board_rank(cdestpos)) && (destfile == INVALID || destfile == get_board_file(cdestpos)) && (promotion == (cpromote & PIECE_MASK))) {
				return move;
			}
		}
		std::cout << "couldn't find legal moves among: " << std::endl;
		for (unsigned int i = 0; i < candidates.size(); i++) {
			print_move(candidates[i], std::cout);
			std::cout << std::endl;
		}
	}
	return invalid_move(s);
}

Board::Board()
{
	reset();
}

void Board::reset()
{
	standard_initial();
	side_to_play = White;
	castle = 0x0f;
	in_check = false;
	enpassant_target = -1;
	move_count = 0;
}

Board::Board(const Board &copy)
	: side_to_play(copy.side_to_play), in_check(copy.in_check), castle(copy.castle), enpassant_target(copy.enpassant_target), move_count(copy.move_count)
{
	memcpy(this->data, copy.data, sizeof(data)); 
}

int get_castle_bit(Color color, bool kingside)
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

bool Board::can_castle(Color color, bool kingside) const
{
	int bit = get_castle_bit(color, kingside);
	return castle & (1 << bit);
}

void Board::invalidate_castle(Color color)
{
	if (color == White) {
		castle = castle & 0xc;
	} else {
		castle = castle & 0x3;
	}
}

void Board::invalidate_castle_side(Color color, bool kingside)
{
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

void Board::standard_initial()
{
	memset(data, 0x88, sizeof(data));
	memcpy(&data[MEMORY_FILES * 2 / 2], initial_board, sizeof(initial_board));	
}

void Board::repeated_move(BoardPos base, piece_t piece, char drank, char dfile, Color capture, std::vector<move_t> &moves) const
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

BoardPos Board::get_capture(BoardPos base, char drank, char dfile, Color capture) const
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

void Board::single_move(BoardPos base, piece_t piece, char drank, char dfile, Color capture, std::vector<move_t> &moves) const
{
	BoardPos pos = base;
	pos = add_vector(pos, drank, dfile);
	piece_t owner = get_piece(pos);
	if (is_legal_pos(pos) && (owner == EMPTY || (((owner & 0x8) != 0) == (capture == Black)))) {
		// found what we want to capture
		moves.push_back(make_move(base, piece, pos, owner, 0));
	} 
}

void Board::pawn_move(BoardPos source, BoardPos dest, std::vector<move_t> &moves) const
{
	piece_t pawn = get_piece(source);
	piece_t capture = get_piece(dest);
	if (get_board_rank(dest) == 0 || get_board_rank(dest) == 7)
	{
		moves.push_back(make_move(source, pawn, dest, capture, KNIGHT));
		moves.push_back(make_move(source, pawn, dest, capture, BISHOP));
		moves.push_back(make_move(source, pawn, dest, capture, ROOK));
		moves.push_back(make_move(source, pawn, dest, capture, QUEEN));
	}
	else {
		moves.push_back(make_move(source, pawn, dest, capture, 0));
	}
}
void Board::pawn_capture(BoardPos bp, char dfile, Color piece_color, std::vector<move_t> &moves) const
{
	BoardPos dest = add_vector(bp, piece_color == White ? 1 : -1, dfile);
	piece_t square = get_piece(dest);
	if ((square & PIECE_MASK) && ((piece_color == White && (square & 0x8)) || (piece_color == Black && ((square & 0x8) == 0))))
	{
		pawn_move(bp, dest, moves);
	}
	// en passant captures
	else if (square == EMPTY && enpassant_target == get_board_file(dest) && get_board_rank(bp) == (piece_color == White ? 4 : 3) && piece_color == side_to_play)
	{
		pawn_move(bp, dest, moves);
	}
}

void Board::pawn_advance(BoardPos bp, Color piece_color, std::vector<move_t> &moves) const
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

void Board::calculate_moves(Color color, BoardPos bp, piece_t piece, std::vector<move_t> &moves, bool exclude_pawn_advance) const
{
	if (((color == White && (piece & 0x08) == 0) || (color == Black && (piece & 0x08))))
	{
		switch (piece & 0x07)
		{
			case ROOK:
				repeated_move(bp, piece, 1, 0, get_opposite_color(color), moves);
				repeated_move(bp, piece, -1, 0, get_opposite_color(color), moves);
				repeated_move(bp, piece, 0, 1, get_opposite_color(color), moves);
				repeated_move(bp, piece, 0, -1, get_opposite_color(color), moves);
				break;
			case BISHOP:
				repeated_move(bp, piece, 1, 1, get_opposite_color(color), moves);
				repeated_move(bp, piece, -1, 1, get_opposite_color(color), moves);
				repeated_move(bp, piece, -1, -1, get_opposite_color(color), moves);
				repeated_move(bp, piece, 1, -1, get_opposite_color(color), moves);
				break;
			case QUEEN:
				repeated_move(bp, piece, 1, 0, get_opposite_color(color), moves);
				repeated_move(bp, piece, -1, 0, get_opposite_color(color), moves);
				repeated_move(bp, piece, 0, 1, get_opposite_color(color), moves);
				repeated_move(bp, piece, 0, -1, get_opposite_color(color), moves);
				repeated_move(bp, piece, 1, 1, get_opposite_color(color), moves);
				repeated_move(bp, piece, -1, 1, get_opposite_color(color), moves);
				repeated_move(bp, piece, -1, -1, get_opposite_color(color), moves);
				repeated_move(bp, piece, 1, -1, get_opposite_color(color), moves);
				break;
			case KING:
				single_move(bp, piece, 1, 0, get_opposite_color(color), moves);
				single_move(bp, piece, -1, 0, get_opposite_color(color), moves);
				single_move(bp, piece, 0, 1, get_opposite_color(color), moves);
				single_move(bp, piece, 0, -1, get_opposite_color(color), moves);
				single_move(bp, piece, 1, 1, get_opposite_color(color), moves);
				single_move(bp, piece, -1, 1, get_opposite_color(color), moves);
				single_move(bp, piece, -1, -1, get_opposite_color(color), moves);
				single_move(bp, piece, 1, -1, get_opposite_color(color), moves);
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
				single_move(bp, piece, 1, 2, get_opposite_color(color), moves);
				single_move(bp, piece, 2, 1, get_opposite_color(color), moves);
				single_move(bp, piece, -1, 2, get_opposite_color(color), moves);
				single_move(bp, piece, -2, 1, get_opposite_color(color), moves);
				single_move(bp, piece, -1, -2, get_opposite_color(color), moves);
				single_move(bp, piece, -2, -1, get_opposite_color(color), moves);
				single_move(bp, piece, 1, -2, get_opposite_color(color), moves);
				single_move(bp, piece, 2, -1, get_opposite_color(color), moves);
				break;
			case PAWN:
				pawn_capture(bp, 1, color, moves);
				pawn_capture(bp, -1, color, moves);
				if (!exclude_pawn_advance) {
					pawn_advance(bp, color, moves);
				}
				break;
		}
	}
}

void Board::get_moves(Color color, std::vector<move_t> &moves, bool exclude_pawn_advance, piece_t piece) const
{
	piece_t colored_piece = make_piece(piece, color);
	for (BoardPos i = (make_board_pos(0, 0) & 0xfe); i <= (make_board_pos(7, 7) | 0x1); i += 2)
	{
		unsigned char twosquare = data[i >> 1];
		if (twosquare & 0x77) {
			if ((color == Black && (twosquare & 0x88)) || (color == White && ((twosquare & 0x88) != 0x88))) {
				if (piece == 0) {
					calculate_moves(color, i, twosquare >> 4, moves, exclude_pawn_advance);
					calculate_moves(color, i+1, twosquare & 0xf, moves, exclude_pawn_advance);
				} else {
					if (twosquare >> 4 == colored_piece) {
						calculate_moves(color, i, twosquare >> 4, moves, exclude_pawn_advance);
					}
					if ((twosquare & 0xf) == colored_piece) {
						calculate_moves(color, i+1, twosquare & 0xf, moves, exclude_pawn_advance);
					}
				}
			}
		}
	}
}

void Board::legal_moves(Color color, std::vector<move_t> &moves, piece_t piece_to_limit) const
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
				get_moves(get_opposite_color(color), opposite_color_moves, true);
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

BoardPos Board::find_piece(piece_t piece) const
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
bool Board::discovers_check(move_t move, Color color) const
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

void Board::apply_move(move_t move)
{
	BoardPos destpos = get_dest_pos(move);
	BoardPos sourcepos = get_source_pos(move);
	piece_t destpiece = get_piece(destpos);
	piece_t sourcepiece = get_piece(sourcepos);
	Color color = get_color(sourcepiece);
	
	set_piece(destpos, sourcepiece);
	set_piece(sourcepos, EMPTY);
	enpassant_target = 8;

	if ((sourcepiece & PIECE_MASK) == KING) {
		invalidate_castle(color);
		// castling
		if (get_board_file(sourcepos) == 4) {
			switch (get_board_file(destpos)) {
			case 2:
				set_piece(make_board_pos(get_board_rank(sourcepos), 3), ROOK | (color == Black ? BlackMask : 0));
				set_piece(make_board_pos(get_board_rank(sourcepos), 0), EMPTY);
				break;
			case 6:
				set_piece(make_board_pos(get_board_rank(sourcepos), 5), ROOK | (color == Black ? BlackMask : 0));
				set_piece(make_board_pos(get_board_rank(sourcepos), 7), EMPTY);
				break;
			}
		}
	}
	else if ((sourcepiece & PIECE_MASK) == ROOK) {
		switch (get_board_file(sourcepos)) {
		case 0:
			invalidate_castle_side(color, false);
			break;
		case 7:
			invalidate_castle_side(color, true);
			break;
		}
	}
	else if ((sourcepiece & PIECE_MASK) == PAWN) {
		// set enpassant capability
		if (abs(get_board_rank(destpos) - get_board_rank(sourcepos)) == 2) {
			enpassant_target = get_board_file(destpos);
		}
		// pawn promote
		else if (get_board_rank(destpos) == 0 || get_board_rank(destpos) == 7) {
			set_piece(destpos, get_promotion(move, color));
		}
		// enpassant capture
		else if (get_board_file(sourcepos) != get_board_file(destpos) && destpiece == EMPTY) {
			set_piece(make_board_pos(get_board_rank(sourcepos), get_board_file(destpos)), EMPTY);
		}
	}
	side_to_play = get_opposite_color(color);
	in_check = king_in_check(side_to_play);
	if (color == White) {
		move_count++;
	}
}

void Board::undo_move(move_t move)
{
	BoardPos destpos = get_dest_pos(move);
	BoardPos sourcepos = get_source_pos(move);
	piece_t moved_piece = get_piece(destpos);
	Color color = get_color(moved_piece);
	piece_t captured_piece = get_captured_piece(move, get_opposite_color(color));
	
	// flags
	side_to_play = color;
	in_check = move & MOVE_FROM_CHECK;
	if ((move & INVALIDATES_CASTLE) != 0) {
		if ((moved_piece & PIECE_MASK) == KING) {
			if (color == White) {
				castle |= 0x3;
			} else {
				castle |= 0xc;
			}
		} else {
			int bit = get_castle_bit(color, get_board_file(sourcepos) == 7);
			castle |= (1 << bit);
		}
	}
	
	set_piece(sourcepos, moved_piece);
	set_piece(destpos, captured_piece);

	// promote
	piece_t promote = get_promotion(move, color);
	if (promote & PIECE_MASK) {
		set_piece(sourcepos, make_piece(PAWN, color));
	}
	
	// castle
	if ((moved_piece & PIECE_MASK) == KING && get_board_file(sourcepos) == 4) {
		switch (get_board_file(destpos)) {
			case 2:
				set_piece(make_board_pos(get_board_rank(destpos), 0), make_piece(ROOK, color));
				set_piece(make_board_pos(get_board_rank(destpos), 3), EMPTY);
				break;
			case 6:
				set_piece(make_board_pos(get_board_rank(destpos), 7), make_piece(ROOK, color));
				set_piece(make_board_pos(get_board_rank(destpos), 5), EMPTY);
				break;
		}
			
	}

	// en passant capture
	if (move & ENPASSANT_FLAG) {
		set_piece(make_board_pos(get_board_rank(sourcepos), get_board_file(destpos)), make_piece(PAWN, get_opposite_color(color)));
	}
	enpassant_target = (move >> 20) & 0xf;
	if (enpassant_target > 8) {
		enpassant_target = -1;
	}
	if (color == White) {
		move_count--;
	}
}

bool Board::removes_check(move_t move, Color color) const
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
		calculate_moves(get_opposite_color(color), destpos, destpiece, precluded_moves, true);
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
	
	Board copy(*this);
	copy.apply_move(move);
	return !copy.king_in_check(color);
}

bool Board::king_in_check(Color color) const
{
	BoardPos king = find_piece(make_piece(KING, color));
	std::vector<move_t> opponent_moves;
	for (piece_t piece_type = 1; piece_type <= 6; piece_type++) {
		calculate_moves(color, king, make_piece(piece_type, color), opponent_moves, true);
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

void Board::print_move(move_t move, std::ostream &os) const
{
	BoardPos destpos = get_dest_pos(move);
	BoardPos sourcepos = get_source_pos(move);
	os << static_cast<char>(get_board_file(sourcepos) + 'a') << static_cast<char>(get_board_rank(sourcepos) + '1') << '-' << static_cast<char>(get_board_file(destpos) + 'a') << static_cast<char>(get_board_rank(destpos) + '1');
	piece_t promotion = get_promotion(move, White);
	if (promotion > 0) {
		os << "=";
		switch (promotion) {
			case BISHOP: os << "B"; break;
			case KNIGHT: os << "N"; break;
			case ROOK: os << "R"; break;
			case QUEEN: os << "Q"; break;
		}
	}
}

char Board::fen_repr(piece_t p) const
{
	switch (p) {
	case EMPTY: return 'x';
	case PAWN: return 'P';
	case KNIGHT: return 'N';
	case BISHOP: return 'B';
	case ROOK: return 'R';
	case QUEEN: return 'Q';
	case KING: return 'K';
	case PAWN | BlackMask: return 'p';
	case KNIGHT | BlackMask: return 'n';
	case BISHOP | BlackMask: return 'b';
	case ROOK | BlackMask: return 'r';
	case QUEEN | BlackMask: return 'q';
	case KING | BlackMask: return 'k';
	default: return 'X';
	}
}

void Board::fen_flush(std::ostream &os, int &empty) const
{
	if (empty > 0) {
		os << empty;
		empty = 0;
	}
}

void Board::fen_handle_space(piece_t piece, std::ostream &os, int &empty) const
{
	char c = fen_repr(piece);
	if (c == 'x') {
		empty++;
	}
	else if (c == 'X') {
		std::cout << "Error printing fen representation" << std::endl;
		abort();
	}
	else if (empty > 0) {
		fen_flush(os, empty);
		os << c;
	}
	else {
		os << c;
	}
}

void Board::get_fen(std::ostream &os) const
{
	int empty = 0;
	for (int rank = 8; rank >= 1; rank--) {
		for (int file = 1; file <= 8; file++) {
			fen_handle_space(get_piece(make_board_pos(rank-1, file-1)), os, empty);
		}
		fen_flush(os, empty);
		if (rank > 1) {
				os << "/";
		}
	}
	switch (side_to_play) {
		case White: os << " w"; break;
		case Black: os << " b"; break;
		default: abort();
	}
	os << " ";
	if (can_castle(White, true)) {
		os << "K";
	}
	if (can_castle(White, false)) {
		os << "Q";
	}
	if (can_castle(Black, true)) {
		os << "k";
	} 
	if (can_castle(Black, false)) {
		os << "q";
	}
	if (!castle) {
		os << "-";
	}
	os << " ";
	if (enpassant_target < 0 || enpassant_target >= 8) {
		os << "-";
	} else if (side_to_play == White) {
		os << static_cast<char>(enpassant_target + 'a') << "6";
	} else {
		os << static_cast<char>(enpassant_target + 'a') << "3";
	}
	os << " 0 0";
}

std::ostream &operator<<(std::ostream &os, const Board &b)
{
	b.get_fen(os);
	return os;
}

