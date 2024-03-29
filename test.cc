#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <set>
#include "pgn.hh"
#include "search.hh"
#include "evaluate.hh"
#include "move.hh"
#include "bitboard.hh"
#include "fenboard.hh"

template <class T>
void assert_equals(T expected, T actual)
{
    if (expected != actual) {
        std::cout << "Test failed: expected " << expected << " but got " << actual << std::endl;
        abort();
    }
}

template <class T>
void assert_not_equals(T expected, T actual)
{
    if (expected == actual) {
        std::cout << "Test failed: unexpectedly got " << actual << std::endl;
        abort();
    }
}

template <class T>
void assert_equals_unordered(const std::vector<T> &a, const std::vector<T> &b)
{
    std::set<T> aset(a.begin(), a.end());
    std::set<T> bset(b.begin(), b.end());

    for (typename std::set<T>::const_iterator aiter = aset.begin(); aiter != aset.end(); aiter++) {
        if (bset.find(*aiter) == bset.end()) {
            std::cout << "Couldn't find expected value " << *aiter << std::endl;
            abort();
        }
    }
    for (typename std::set<T>::const_iterator biter = bset.begin(); biter != bset.end(); biter++) {
        if (aset.find(*biter) == aset.end()) {
            std::cout << "Found unexpected value " << *biter << std::endl;
            abort();
        }
    }
}

void legal_moves(Bitboard *b, Color color, std::vector<move_t> &moves) {
    BitboardMoveIterator iter = b->get_legal_moves(color);
    while (b->has_more_moves(iter)) {
        moves.push_back(b->get_next_move(iter));
    }

}

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cout << "Expected arg: sample-game.pgn" << std::endl;
        return 1;
    }
    Fenboard b;
    for (unsigned char r = 0; r < 8; r++) {
        for (unsigned char f = 0; f < 8; f++) {
            BoardPos bp = make_board_pos(r, f);
            assert_equals(f, get_board_file(bp));
            assert_equals(r, get_board_rank(bp));
        }
    }
    b.set_starting_position();
    int pieces[8] = { bb_rook, bb_knight, bb_bishop, bb_queen, bb_king, bb_bishop, bb_knight, bb_rook };
    for (int i = 0; i < 8; i++) {
        assert_equals(make_piece(pieces[i], White), b.get_piece(0, i));
        assert_equals(make_piece(pieces[i], Black), b.get_piece(7, i));
        assert_equals(make_piece(bb_pawn, Black), b.get_piece(6, i));
        assert_equals(make_piece(bb_pawn, White), b.get_piece(1, i));
        for (int r = 2; r < 6; r++) {
            assert_equals(static_cast<piece_t>(EMPTY), b.get_piece(r, i));
        }
    }

    std::vector<move_t> legal_white, legal_black;
    legal_moves(&b, White, legal_white);
    legal_moves(&b, Black, legal_black);
    std::vector<move_t> expected_white, expected_black;

    for (int i = 0; i < 8; i++) {
        for (int j = 3; j <= 6; j++) {
            std::ostringstream os;
            os << char('a' + i) << j;
            if (j >= 5) {
                expected_black.push_back(b.read_move(os.str(), Black));
            } else {
                expected_white.push_back(b.read_move(os.str(), White));
            }
        }
        if (i == 0 || i == 2 || i == 5 || i == 7) {
            std::ostringstream os;
            os << 'N' << char('a' + i) << 3;
            expected_white.push_back(b.read_move(os.str(), White));
            os.str("");
            os << 'N' << char('a' + i) << 6;
            expected_black.push_back(b.read_move(os.str(), Black));
        }
    }
    assert_equals_unordered(expected_white, legal_white);
    assert_equals_unordered(expected_black, legal_black);

    std::ostringstream boardtext;
    boardtext << b;
    assert_equals(std::string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"), boardtext.str());

    assert_equals(static_cast<uint64_t>(0), b.get_hash());
    move_t e4 = b.read_move("e4", White);
    b.apply_move(e4);
    assert_equals(static_cast<uint64_t>(0x4ee64568aacaeabb), b.get_hash());

    boardtext.str("");
    boardtext << b;
    assert_equals(std::string("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"), boardtext.str());

    assert_equals(make_piece(bb_pawn, White), b.get_piece(3, 4));
    assert_equals(static_cast<piece_t>(EMPTY), b.get_piece(1, 4));
    b.undo_move(e4);
    assert_equals(static_cast<uint64_t>(0), b.get_hash());

    boardtext.str("");
    boardtext << b;
    assert_equals(std::string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"), boardtext.str());

    for (int i = 0; i < 8; i++) {
        assert_equals(make_piece(pieces[i], White), b.get_piece(0, i));
        assert_equals(make_piece(pieces[i], Black), b.get_piece(7, i));
        assert_equals(make_piece(bb_pawn, Black), b.get_piece(6, i));
        assert_equals(make_piece(bb_pawn, White), b.get_piece(1, i));
        for (int r = 2; r < 6; r++) {
            assert_equals(static_cast<piece_t>(EMPTY), b.get_piece(r, i));
        }
    }

    std::map<std::string, std::string> game_metadata;
    std::vector<std::pair<std::string, std::string> > movelist;
    std::vector<move_t> moverecord;
    std::vector<std::string> boardrecord;
    std::ifstream fischer(argv[1]);
    if (!fischer) {
        std::cout << "Cannot load Fischer.pgn" << std::endl;
        assert_equals(0, 1);
    }
    // play one game
    read_pgn(fischer, game_metadata, movelist);
    for (std::vector<std::pair<std::string, std::string> >::iterator iter = movelist.begin(); iter != movelist.end(); iter++) {
        move_t move = b.read_move(iter->first, White);
        std::cout << b << " ";
        b.print_move(move, std::cout);
        std::cout << " " << b.get_hash() << std::endl;
        b.apply_move(move);
        moverecord.push_back(move);
        boardtext.str("");
        boardtext << b;
        boardrecord.push_back(boardtext.str());

        move = b.read_move(iter->second, Black);
        std::cout << b << " ";
        b.print_move(move, std::cout);
        std::cout << " " << b.get_hash() << std::endl;
        b.apply_move(move);
        moverecord.push_back(move);
        boardtext.str("");
        boardtext << b;
        boardrecord.push_back(boardtext.str());
    }

    boardtext.str("");
    boardtext << b;
    assert_equals(std::string("r2q4/ppp3bk/3p2pp/3P4/2n1Nn2/8/PPB3PP/R4R1K w - - 0 24"), boardtext.str());
    assert_equals(static_cast<uint64_t>(0xf5d83e60125e3b39U), b.get_hash());

    // play in reverse
    assert_equals(moverecord.size(), boardrecord.size());
    for (int i = moverecord.size() - 1; i >= 0; i--)
    {
        boardtext.str("");
        b.get_fen(boardtext);
        assert_equals(boardrecord[i], boardtext.str());
        b.undo_move(moverecord[i]);

        std::cout << b << " ";
        b.print_move(moverecord[i], std::cout);
        std::cout << " " << b.get_hash() << std::endl;
    }

    boardtext.str("");
    boardtext << b;
    assert_equals(std::string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"), boardtext.str());
    assert_equals(static_cast<uint64_t>(0), b.get_hash());

    b.set_fen("r5k1/1p1brpbp/1npp2p1/q1n5/p1PNPP2/2N3PP/PPQ2B1K/3RRB2");
    move_t move = b.read_move("g4", White);
    b.apply_move(move);
    move = b.read_move("Rae8", Black);
    b.apply_move(move);
    boardtext.str("");
    boardtext << b;
    assert_equals(std::string("4r1k1/1p1brpbp/1npp2p1/q1n5/p1PNPPP1/2N4P/PPQ2B1K/3RRB2 w KQk - 0 2"), boardtext.str());

    b.set_fen("8/4k3/5p2/3BP1pP/5KP1/8/2b5/8 w - g6 0 1");
    move = b.read_move("hxg6", White);
    b.apply_move(move);
    boardtext.str("");
    boardtext << b;
    assert_equals(std::string("8/4k3/5pP1/3BP3/5KP1/8/2b5/8 b - - 0 1"), boardtext.str());

    // play the rest of the Fischer games
    while (!fischer.eof()) {
        movelist.clear();
        game_metadata.clear();
        read_pgn(fischer, game_metadata, movelist);
        b.set_starting_position();
        for (std::vector<std::pair<std::string, std::string> >::iterator iter = movelist.begin(); iter != movelist.end(); iter++) {
            move_t move = b.read_move(iter->first, White);
            b.apply_move(move);
            if (iter->second.length() > 1) {
                move = b.read_move(iter->second, Black);
                b.apply_move(move);
            }
        }
    }

    // bug in check computation
    std::ostringstream movetext;
    b.set_fen("r4kr1/1b2R1n1/pq4p1/7Q/1p4P1/5P2/PPP4P/1K2R3 w - - 0 1");
    legal_black.clear();
    legal_moves(&b, Black, legal_black);
    for (std::vector<move_t>::iterator iter = legal_black.begin(); iter != legal_black.end(); iter++) {
        movetext.str("");
        b.print_move(*iter, movetext);
        assert_not_equals(std::string("f8-e7"), movetext.str());
    }

    b.set_fen("6r1/7k/2p1pPp1/3p4/8/7R/5PPP/5K2 b - - 1 1");
    legal_black.clear();
    legal_moves(&b, Black, legal_black);
    assert_equals(0, static_cast<int>(legal_black.size()));
    assert_equals(true, b.king_in_check(Black));

    // white has mate in 1
    SimpleEvaluation simple;
    Search search(&simple);

    b.set_fen("3B1n2/NP2P3/b7/2kp2N1/8/2Kp4/8/8 w - - 0 1");
    search.max_depth = 2;
    move = search.minimax(b, White);
    movetext.str("");
    b.print_move(move, movetext);
    assert_equals(VERY_GOOD - 1, search.score);
    search.reset();
    move = search.alphabeta(b, White);
    assert_equals(VERY_GOOD - 1, search.score);

    // white has mate in 2
    b.set_fen("r4kr1/1b2R1n1/pq4p1/4Q3/1p4P1/5P2/PPP4P/1K2R3 w - - 0 1");

    search.reset();
    search.max_depth = 4;
    move = search.minimax(b, White);
    assert_equals(VERY_GOOD - 3, search.score);
    assert_equals(b.read_move("Rf7+", White), move);

    search.reset();
    move = search.alphabeta(b, White);
    assert_equals(VERY_GOOD - 3, search.score);
    assert_equals(b.read_move("Rf7+", White), move);

    b.set_fen("1Q6/8/8/8/8/k2K4/8/8 w - b6 0 1");
    search.reset();
    move = search.alphabeta(b, White);
    assert_equals(b.read_move("Kc3", White), move);
    assert_equals(VERY_GOOD - 3, search.score);

    b.set_fen("8/8/5p2/5B2/8/1K1R4/8/2k5 w - - 0 1");
    search.reset();
    move = search.alphabeta(b, White);
    assert_equals(b.read_move("Bg4", White), move);
    assert_equals(VERY_GOOD - 3, search.score);

    b.set_fen("3q1rk1/5pbp/5Qp1/8/8/2B5/5PPP/6K1 w - - 0 1");
    search.reset();
    search.max_depth = 2;
    move = search.alphabeta(b, White);
    assert_equals(b.read_move("Qxg7", White), move);
    assert_equals(VERY_GOOD - 1, search.score);

    b.set_fen("1Q6/8/8/8/8/k2K4/8/8 w - - 0 1");
    search.reset();
    search.max_depth = 4;
    move = search.alphabeta(b, White);
    assert_equals(b.read_move("Kc3", White), move);
    assert_equals(VERY_GOOD - 3, search.score);

    b.set_fen("5n2/2PPk1PR/8/4K3/8/8/8/8 w - - 0 1");
    search.reset();
    search.use_transposition_table = false;
    search.use_iterative_deepening = false;
    move = search.alphabeta(b, White);
    assert_equals(b.read_move("g8=N#", White), move);
    assert_equals(VERY_GOOD - 1, search.score);
    return 0;
}
