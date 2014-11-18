#include <cstring>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include "chess.hh"

template <class PrimitiveBoard>
void FenBoard<PrimitiveBoard>::set_starting_position() 
{
	set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w kKqQ - 0 1");
}

template <class PrimitiveBoard>
move_t FenBoard<PrimitiveBoard>::invalid_move(const std::string &s) const
{
	std::cerr << "Can't read move " << s << std::endl;
	std::cerr << *this << std::endl;
	abort();
}

template <class PrimitiveBoard>
void FenBoard<PrimitiveBoard>::set_fen(const std::string &fen)
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
			
		switch (c) {
			case 'p': this->set_piece(rank - 1, file - 1, PAWN | BlackMask); break;
			case 'r': this->set_piece(rank - 1, file - 1, ROOK | BlackMask); break;
			case 'n': this->set_piece(rank - 1, file - 1, KNIGHT | BlackMask); break;
			case 'b': this->set_piece(rank - 1, file - 1, BISHOP | BlackMask); break;
			case 'q': this->set_piece(rank - 1, file - 1, QUEEN | BlackMask); break;
			case 'k': this->set_piece(rank - 1, file - 1, KING | BlackMask); break;
			case 'P': this->set_piece(rank - 1, file - 1, PAWN); break;
			case 'R': this->set_piece(rank - 1, file - 1, ROOK); break;
			case 'N': this->set_piece(rank - 1, file - 1, KNIGHT); break;
			case 'B': this->set_piece(rank - 1, file - 1, BISHOP); break;
			case 'Q': this->set_piece(rank - 1, file - 1, QUEEN); break;
			case 'K': this->set_piece(rank - 1, file - 1, KING); break;
		}
		if (c > '0' && c < '9') {
			for (int i = 0; i < c - '0'; i++) {
				this->set_piece(rank-1, file-1, EMPTY);
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
			case 'w': this->side_to_play = White; break;
			case 'b': this->side_to_play = Black; break;
			case ' ': break;
			case 0: return;
			default: std::cerr << "Can't read fen" << fen << std::endl; abort();
		}
	}
	if (c == 0) {
		return;
	}
	this->castle = 0;
	while ((c = fen[pos++]) != 0 && c != ' ') {
		// castle status
		switch(c) {
			case 'k': this->castle |= 1 << this->get_castle_bit(Black, true); break;
			case 'K': this->castle |= 1 << this->get_castle_bit(White, true); break;
			case 'q': this->castle |= 1 << this->get_castle_bit(Black, false); break;
			case 'Q': this->castle |= 1 << this->get_castle_bit(White, false); break;
			case '-': case ' ': break;
			case 0: return;
			default: std::cerr << "Can't read fen" << fen << std::endl; abort();
		}
	}
	
	if (c == 0) {
		return;
	}

	this->enpassant_file = -1;
	
	if ((c = fen[pos++]) != 0) {
		if (c >= 'a' && c <= 'h') {
			this->enpassant_file = c - 'a';
		}
	}
	while (pos < fen.size() && fen[pos++] == ' ')
		;

	int halfmoves = 0;
	this->move_count = 0;
	bool on_halfmoves = true;
	while (pos < fen.size() && (c = fen[pos++]) != 0) {
		switch(c) {
			case ' ': on_halfmoves = false; break;
			case 0: break;
			default:
				if (c <= '9' && c >= '0') {
					if (on_halfmoves) {
						halfmoves = halfmoves * 10 + (c - '0');
					} else {
						this->move_count = this->move_count * 10 + (c - '0');
					}
				}
				else {
					std::cerr << "Can't read fen" << fen << std::endl; abort();
				}
				break;
		}
	}
	this->update();
}

template <class PrimitiveBoard>
unsigned char FenBoard<PrimitiveBoard>::read_piece(unsigned char c) const
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

template <class PrimitiveBoard>
move_t FenBoard<PrimitiveBoard>::read_move(const std::string &s, Color color) const
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
			} else if (s[pos] == '#') {
				// mate = true;
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
		piece_t piece = KING | (color == White ? WhiteMask : BlackMask);
		if (queenside_castle) {
			if (color == White) {
				return this->make_move(0, 4, piece, 0, 2, EMPTY, 0);
			} else {
				return this->make_move(7, 4, piece, 7, 2, EMPTY, 0);
			}
		} else {
			if (color == White) {
				return this->make_move(0, 4, piece, 0, 6, EMPTY, 0);
			} else {
				return this->make_move(7, 4, piece, 7, 6, EMPTY, 0);
			}
		}
	} else {
		this->legal_moves(color, candidates, piece);
		for (unsigned int i = 0; i < candidates.size(); i++) {
			move_t move = candidates[i];
			unsigned char mdestfile, mdestrank, msourcefile, msourcerank;
			this->get_source(candidates[i], msourcerank, msourcefile);
			this->get_dest(candidates[i], mdestrank, mdestfile);
			unsigned int msourcepiece = this->get_piece(msourcerank, msourcefile) & PIECE_MASK;
			unsigned int mpromote = this->get_promotion(move);
			if (piece == msourcepiece && (srcrank == INVALID || srcrank == msourcerank) && (srcfile == INVALID || srcfile == msourcefile) && (destrank == INVALID || destrank == mdestrank) && (destfile == INVALID || destfile == mdestfile) && (promotion == mpromote)) {
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

template <class PrimitiveBoard>
void FenBoard<PrimitiveBoard>::print_move(move_t move, std::ostream &os) const
{
	unsigned char destrank, destfile, srcrank, srcfile;
	this->get_source(move, srcrank, srcfile);
	this->get_dest(move, destrank, destfile);
	
	os << static_cast<char>(srcfile + 'a') << static_cast<char>(srcrank + '1') << '-' << static_cast<char>(destfile + 'a') << static_cast<char>(destrank + '1');
	unsigned char promotion = this->get_promotion(move);
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

template <class PrimitiveBoard>
char FenBoard<PrimitiveBoard>::fen_repr(unsigned char p) const
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

template <class PrimitiveBoard>
void FenBoard<PrimitiveBoard>::fen_flush(std::ostream &os, int &empty) const
{
	if (empty > 0) {
		os << empty;
		empty = 0;
	}
}

template <class PrimitiveBoard>
void FenBoard<PrimitiveBoard>::fen_handle_space(unsigned char piece, std::ostream &os, int &empty) const
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

template <class PrimitiveBoard>
void FenBoard<PrimitiveBoard>::get_fen(std::ostream &os) const
{
	int empty = 0;
	for (int rank = 8; rank >= 1; rank--) {
		for (int file = 1; file <= 8; file++) {
			fen_handle_space(this->get_piece(rank-1, file-1), os, empty);
		}
		fen_flush(os, empty);
		if (rank > 1) {
				os << "/";
		}
	}
	switch (this->side_to_play) {
		case White: os << " w"; break;
		case Black: os << " b"; break;
		default: abort();
	}
	os << " ";
	if (this->can_castle(White, true)) {
		os << "K";
	}
	if (this->can_castle(White, false)) {
		os << "Q";
	}
	if (this->can_castle(Black, true)) {
		os << "k";
	} 
	if (this->can_castle(Black, false)) {
		os << "q";
	}
	if (!this->castle) {
		os << "-";
	}
	os << " ";
	if (this->enpassant_file < 0 || this->enpassant_file >= 8) {
		os << "-";
	} else if (this->side_to_play == White) {
		os << static_cast<char>(this->enpassant_file + 'a') << "6";
	} else {
		os << static_cast<char>(this->enpassant_file + 'a') << "3";
	}
	os << " 0 " << this->move_count;
}


template <class PrimitiveBoard>
std::ostream &operator<<(std::ostream &os, const FenBoard<PrimitiveBoard> &b)
{
	b.get_fen(os);
	return os;
}
