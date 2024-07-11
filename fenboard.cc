#include <cstring>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include "fenboard.hh"
#include "move.hh"

Fenboard::Fenboard()
{
}

void Fenboard::set_starting_position()
{
    set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w kKqQ - 0 1");
    this->side_to_play = White;
}

move_t Fenboard::invalid_move(const std::string &s) const
{
    std::cerr << "Can't read move " << s << std::endl;
    std::cerr << this << std::endl;
    abort();
}

void Fenboard::reset()
{
    set_fen("8/8/8/8/8/8/8/8 w - - 0 1");
}

void Fenboard::set_fen(const std::string &fen)
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
            case 'p': this->set_piece(rank - 1, file - 1, bb_pawn | BlackMask); break;
            case 'r': this->set_piece(rank - 1, file - 1, bb_rook | BlackMask); break;
            case 'n': this->set_piece(rank - 1, file - 1, bb_knight | BlackMask); break;
            case 'b': this->set_piece(rank - 1, file - 1, bb_bishop | BlackMask); break;
            case 'q': this->set_piece(rank - 1, file - 1, bb_queen | BlackMask); break;
            case 'k': this->set_piece(rank - 1, file - 1, bb_king | BlackMask); break;
            case 'P': this->set_piece(rank - 1, file - 1, bb_pawn); break;
            case 'R': this->set_piece(rank - 1, file - 1, bb_rook); break;
            case 'N': this->set_piece(rank - 1, file - 1, bb_knight); break;
            case 'B': this->set_piece(rank - 1, file - 1, bb_bishop); break;
            case 'Q': this->set_piece(rank - 1, file - 1, bb_queen); break;
            case 'K': this->set_piece(rank - 1, file - 1, bb_king); break;
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
            case ' ': case 0: break;
            default: std::cerr << "Can't read fen" << fen << std::endl; abort();
        }
    }
    this->in_check = this->king_in_check(this->side_to_play);
    if (c == 0) {
        return;
    }
    // start with no castling ability, add it back based on fen contents
    this->set_can_castle(Black, true, false);
    this->set_can_castle(White, true, false);
    this->set_can_castle(Black, false, false);
    this->set_can_castle(White, false, false);
    while ((c = fen[pos++]) != 0 && c != ' ') {
        // castle status
        switch(c) {
            case 'k': this->set_can_castle(Black, true, true); break;
            case 'K': this->set_can_castle(White, true, true); break;
            case 'q': this->set_can_castle(Black, false, true); break;
            case 'Q': this->set_can_castle(White, false, true); break;
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
}


unsigned char Fenboard::read_piece(unsigned char c) const
{
    switch (c) {
        case 'N': return bb_knight;
        case 'R': return bb_rook;
        case 'B': return bb_bishop;
        case 'Q': return bb_queen;
        case 'K': return bb_king;
        case 'P': return bb_pawn;
    }
    return 0;
}

move_t Fenboard::read_move(const std::string &s, Color color) const
{
    int pos = 0;
    bool castle = false;
    bool bb_queenside_castle = false;
    bool check = false;

    piece_t piece;
    int srcrank = INVALID;
    int srcfile = INVALID;
    int destfile = INVALID;
    int destrank = INVALID;
    int promotion = 0;

    if (s[pos] == 'O') {
        castle = true;
    }
    else if (s[pos] >= 'A' && s[pos] <= 'Z') {
        piece = read_piece(s[pos++]);
    } else {
        piece = bb_pawn;
    }
    if (castle) {
        if (s[++pos] != '-') {
            invalid_move(s);
        }
        if (s[++pos] != 'O') {
            invalid_move(s);
        }
        if (s[pos + 1] == '-' && s[pos + 2] == 'O') {
            bb_queenside_castle = true;
            pos += 2;
        }
        if (s[pos+1] == '+') {
            check = true;
        }
    } else if (piece == bb_pawn && s.length() >= 4 && s[1] >= '1' && s[1] <= '8' && s[2] >= 'a' && s[2] <= 'h') {
        // uci style move: a7a8q
        srcfile = s[0] - 'a';
        srcrank = s[1] - '1';
        destfile = s[2] - 'a';
        destrank = s[3] - '1';
        piece = this->get_piece(srcrank, srcfile) & PIECE_MASK;
        if (s.length() >= 5 && s[4] >= 'a' && s[4] <= 'r') {
            switch (s[4]) {
                case 'q': case 'Q': promotion = bb_queen; break;
                case 'r': case 'R': promotion = bb_rook; break;
                case 'n': case 'N': promotion = bb_knight; break;
                case 'b': case 'B': promotion = bb_bishop; break;
                default: invalid_move(s); break;
            }
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
                    case 'q': case 'Q': promotion = bb_queen; break;
                    case 'r': case 'R': promotion = bb_rook; break;
                    case 'n': case 'N': promotion = bb_knight; break;
                    case 'b': case 'B': promotion = bb_bishop; break;
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
        piece_t piece = bb_king | (color == White ? WhiteMask : BlackMask);
        if (bb_queenside_castle) {
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
        BitboardMoveIterator candidates = this->get_legal_moves(color);
        while (this->has_more_moves(candidates)) {
            move_t move = this->get_next_move(candidates);
            unsigned char mdestfile, mdestrank, msourcefile, msourcerank;
            get_source(move, msourcerank, msourcefile);
            get_dest(move, mdestrank, mdestfile);
            unsigned int msourcepiece = this->get_piece(msourcerank, msourcefile) & PIECE_MASK;
            unsigned int mpromote = get_promotion(move);
            if (piece == msourcepiece && (srcrank == INVALID || srcrank == msourcerank) && (srcfile == INVALID || srcfile == msourcefile) && (destrank == INVALID || destrank == mdestrank) && (destfile == INVALID || destfile == mdestfile) && (promotion == mpromote)) {
                return move;
            }
        }
        std::cout << (*this) << std::endl << "couldn't find legal moves among: " << std::endl;
        candidates = this->get_legal_moves(color);
        while (this->has_more_moves(candidates)) {
            move_t move = this->get_next_move(candidates);
            print_move(move, std::cout);
            std::cout << std::endl;
        }
    }
    return invalid_move(s);
}

void Fenboard::print_move(move_t move, std::ostream &os) const
{
    unsigned char destrank, destfile, srcrank, srcfile;
    get_source(move, srcrank, srcfile);
    get_dest(move, destrank, destfile);

    const char *piece = "";
    piece_t piece_type = this->get_piece(srcrank, srcfile) & PIECE_MASK;
    switch (piece_type) {
        case bb_bishop: piece = "B"; break;
        case bb_knight: piece = "N"; break;
        case bb_rook: piece = "R"; break;
        case bb_queen: piece = "Q"; break;
        case bb_king: piece = "K"; break;
        case bb_pawn: piece = ""; break;
        default: piece = "?"; break;
    }

    if (piece_type == bb_king && srcfile == destfile + 2) {
        os << "O-O-O";
    } else if (piece_type == bb_king && srcfile == destfile - 2) {
        os << "O-O";
    } else {
        os << piece;
        switch (piece_type) {
            case bb_bishop: case bb_knight: case bb_rook: case bb_queen:
                os << static_cast<char>(srcfile + 'a') << static_cast<char>(srcrank + '1');
                break;
            case bb_pawn:
                if (get_captured_piece(move) != EMPTY) {
                    os << static_cast<char>(srcfile + 'a');
                }
                break;
        }
        os << (get_captured_piece(move) != EMPTY ? "x" : "")
            << static_cast<char>(destfile + 'a') << static_cast<char>(destrank + '1');
    }
    unsigned char promotion = get_promotion(move);
    if (promotion > 0) {
        os << "=";
        switch (promotion) {
            case bb_bishop: os << "B"; break;
            case bb_knight: os << "N"; break;
            case bb_rook: os << "R"; break;
            case bb_queen: os << "Q"; break;
        }
    }
    if (get_invalidates_kingside_castle(move)) {
        os << "xk";
    }
    if (get_invalidates_queenside_castle(move)) {
        os << "xq";
    }
}

// uci move format (e7e8q)
std::ostream &print_move_uci(move_t move, std::ostream &os)
{
    unsigned char destrank, destfile, srcrank, srcfile;
    get_source(move, srcrank, srcfile);
    get_dest(move, destrank, destfile);

    os << static_cast<char>(srcfile + 'a') << static_cast<char>(srcrank + '1')
       << static_cast<char>(destfile + 'a') << static_cast<char>(destrank + '1');
    unsigned char promotion = get_promotion(move);
    if (promotion > 0) {
        switch (promotion) {
            case bb_bishop: os << "b"; break;
            case bb_knight: os << "n"; break;
            case bb_rook: os << "r"; break;
            case bb_queen: os << "q"; break;
        }
    }
    return os;
}

char Fenboard::fen_repr(unsigned char p) const
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

void Fenboard::fen_flush(std::ostream &os, int &empty) const
{
    if (empty > 0) {
        os << empty;
        empty = 0;
    }
}

void Fenboard::fen_handle_space(unsigned char piece, std::ostream &os, int &empty) const
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

void Fenboard::get_fen(std::ostream &os) const
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
        default: std::cerr << "Unknown color " << (int)this->side_to_play << std::endl; abort();
    }
    os << " ";
    bool some_castling = 0;
    if (this->can_castle(White, true)) {
        os << "K";
        some_castling = 1;
    }
    if (this->can_castle(White, false)) {
        os << "Q";
        some_castling = 1;
    }
    if (this->can_castle(Black, true)) {
        os << "k";
        some_castling = 1;
    }
    if (this->can_castle(Black, false)) {
        os << "q";
        some_castling = 1;
    }
    if (!some_castling) {
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

std::ostream &operator<<(std::ostream &os, const Fenboard &b)
{
    b.get_fen(os);
    return os;
}
