#include <algorithm>
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
#include "matrix.hh"

void assert_true(bool value)
{
    if (!value) {
        std::cout << "Test failed" << std::endl;
        abort();
    }
}

template <class T>
void assert_equals(T expected, T actual)
{
    if (expected != actual) {
        std::cout << "Test failed: expected " << expected << " but got " << actual << std::endl;
        abort();
    }
}

template <>
void assert_equals<move_t>(move_t expected, move_t actual)
{
    if (expected != actual) {
        std::cout << "Test failed: expected ";
        print_move_uci(expected, std::cout) << " but got ";
        print_move_uci(actual, std::cout) << std::endl;
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
void assert_contains(const std::vector<T> &expected, T actual, const std::string &msg = "")
{
    if (std::find(expected.begin(), expected.end(), actual) == expected.end()) {
        std::cout << "Test failed: unexpectedly got " << actual << " " << msg << std::endl;
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


void legal_moves(Fenboard *b, Color color, std::vector<move_t> &moves) {
    MoveSorter ms(b, color);
    while (ms.has_more_moves()) {
        moves.push_back(ms.next_move());
    }
}

void assert_legal_move(Fenboard *b, Color side_to_play, move_t move) {
    std::vector<move_t> move_list;
    std::ostringstream movetext;
    legal_moves(b, side_to_play, move_list);
    b->print_move(move, movetext);
    assert_contains(move_list, move, movetext.str());
}

void assert_illegal_move(Fenboard *b, Color side_to_play, move_t move) {
    std::vector<move_t> move_list;
    legal_moves(b, side_to_play, move_list);
    if (std::find(move_list.begin(), move_list.end(), move) != move_list.end()) {
        std::ostringstream movetext;
        b->print_move(move, movetext);
        std::cout << "Found unexpected value " << movetext.str() << std::endl;
        abort();
    }
}

void assert_moves(const std::string &fen, const std::vector<std::string> &expected_moves)
{
    Fenboard b;
    std::vector<move_t> legal;
    std::ostringstream movetext;

    b.set_fen(fen);
    legal_moves(&b, b.get_side_to_play(), legal);
    assert_equals(expected_moves.size(), legal.size());
    for (std::vector<move_t>::iterator iter = legal.begin(); iter != legal.end(); iter++) {
        movetext.str("");
        b.print_move(*iter, movetext);
        assert_contains(expected_moves, movetext.str());
    }
}

void test_legal_moves(std::string fischer_pgn_file)
{
    Fenboard b;
    std::ostringstream movetext;
    std::vector<move_t> legal_white, legal_black;
    const int64_t initial_hash = 12941059376081559502ULL;

    for (unsigned char r = 0; r < 8; r++) {
        for (unsigned char f = 0; f < 8; f++) {
            BoardPos bp = make_board_pos(r, f);
            assert_equals(f, get_board_file(bp));
            assert_equals(r, get_board_rank(bp));
        }
    }
    b.set_starting_position();
    assert_equals(static_cast<uint64_t>(initial_hash), b.get_hash());
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

    legal_moves(&b, White, legal_white);
    legal_moves(&b, Black, legal_black);
    std::vector<move_t> expected_white, expected_black;

    for (int i = 0; i < 8; i++) {
        for (int j = 3; j <= 6; j++) {
            std::ostringstream os;
            os << char('a' + i) << j;
            if (j >= 5) {
                expected_black.push_back(b.read_move(os.str(), Black));
                assert_legal_move(&b, Black, expected_black.back());
                assert_illegal_move(&b, White, expected_black.back());
            } else {
                expected_white.push_back(b.read_move(os.str(), White));
                assert_legal_move(&b, White, expected_white.back());
                assert_illegal_move(&b, Black, expected_white.back());
            }
        }
        if (i == 0 || i == 2 || i == 5 || i == 7) {
            std::ostringstream os;
            os << 'N' << char('a' + i) << 3;
            expected_white.push_back(b.read_move(os.str(), White));
            assert_legal_move(&b, White, expected_white.back());
            os.str("");
            os << 'N' << char('a' + i) << 6;
            expected_black.push_back(b.read_move(os.str(), Black));
            assert_legal_move(&b, Black, expected_black.back());
        }
    }

    assert_equals_unordered(expected_white, legal_white);
    assert_equals_unordered(expected_black, legal_black);

    std::ostringstream boardtext;
    boardtext << b;
    assert_equals(std::string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"), boardtext.str());


    assert_equals(static_cast<uint64_t>(initial_hash), b.get_hash());
    move_t e4 = b.read_move("e4", White);
    b.apply_move(e4);
    assert_equals(static_cast<uint64_t>(18262560039210235253ULL), b.get_hash());

    boardtext.str("");
    boardtext << b;
    assert_equals(std::string("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"), boardtext.str());

    assert_equals(make_piece(bb_pawn, White), b.get_piece(3, 4));
    assert_equals(static_cast<piece_t>(EMPTY), b.get_piece(1, 4));
    b.undo_move(e4);
    assert_equals(static_cast<uint64_t>(initial_hash), b.get_hash());

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
    std::vector<std::pair<move_annot, move_annot> > movelist;
    std::vector<move_t> moverecord;
    std::vector<std::string> boardrecord;
    std::ifstream fischer(fischer_pgn_file);
    if (!fischer) {
        std::cout << "Cannot load Fischer.pgn" << std::endl;
        assert_equals(0, 1);
    }

    // play one game
    pgn_istream fischerpgn(fischer);
    read_pgn(&fischerpgn, game_metadata, movelist);
    for (auto iter = movelist.begin(); iter != movelist.end(); iter++) {
        move_t move = b.read_move(iter->first.move, White);
        b.apply_move(move);
        moverecord.push_back(move);
        boardtext.str("");
        boardtext << b;
        boardrecord.push_back(boardtext.str());

        move = b.read_move(iter->second.move, Black);
        b.apply_move(move);
        moverecord.push_back(move);
        boardtext.str("");
        boardtext << b;
        boardrecord.push_back(boardtext.str());
    }

    boardtext.str("");
    boardtext << b;
    assert_equals(std::string("r2q4/ppp3bk/3p2pp/3P4/2n1Nn2/8/PPB3PP/R4R1K w - - 0 24"), boardtext.str());
    assert_equals(static_cast<uint64_t>(10878024775352243206ULL), b.get_hash());

    // play in reverse
    assert_equals(moverecord.size(), boardrecord.size());
    for (int i = moverecord.size() - 1; i >= 0; i--)
    {
        boardtext.str("");
        b.get_fen(boardtext);
        assert_equals(boardrecord[i], boardtext.str());
        b.undo_move(moverecord[i]);
    }

    boardtext.str("");
    boardtext << b;
    assert_equals(std::string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"), boardtext.str());
    assert_equals(static_cast<uint64_t>(initial_hash), b.get_hash());

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
        read_pgn(&fischerpgn, game_metadata, movelist);
        b.set_starting_position();
        for (auto iter = movelist.begin(); iter != movelist.end(); iter++) {
            move_t move = b.read_move(iter->first.move, White);
//            std::cout << board_to_fen(&b) << " + " << iter->first.move << " = " << move_to_algebra(&b, move) << std::endl;
            b.apply_move(move);
            if (iter->second.move.length() > 1) {
                move = b.read_move(iter->second.move, Black);
//                std::cout << board_to_fen(&b) << " + " << iter->second.move << " = " << move_to_algebra(&b, move) << std::endl;
                b.apply_move(move);
            }
        }
    }

    // bug in check computation
    b.set_fen("r4kr1/1b2R1n1/pq4p1/7Q/1p4P1/5P2/PPP4P/1K2R3 b - - 0 1");
    legal_black.clear();
    legal_moves(&b, Black, legal_black);
    for (std::vector<move_t>::iterator iter = legal_black.begin(); iter != legal_black.end(); iter++) {
        movetext.str("");
        b.print_move(*iter, movetext);
        assert_not_equals(std::string("Kxe7"), movetext.str());
    }

    b.set_fen("6r1/7k/2p1pPp1/3p4/8/7R/5PPP/5K2 b - - 1 1");
    legal_black.clear();
    legal_moves(&b, Black, legal_black);
    assert_equals(0, static_cast<int>(legal_black.size()));
    assert_equals(true, b.king_in_check(Black));

    // capture attacking piece
    assert_moves("r4kr1/1b2R1Q1/pq4p1/8/1p4P1/5P2/PPP4P/1K2R3 b - - 0 1", { "Rg8xg7" });
    assert_moves("r3Rkr1/6n1/pq4p1/4Q3/1p4P1/5b2/PPP4P/1K3R2 b - - 0 2", { "Kf7", "Ra8xe8", "Ng7xe8" });

    // pinned piece
    assert_moves("7k/6p1/6Pb/8/8/8/PPP5/1K5R b - - 0 4", { "Kg8" });
    assert_moves("7K/8/8/8/8/5B1R/6p1/7k b - - 0 1", { "Kg1" });
    assert_moves("8/8/8/6R1/8/4K2Q/6p1/6k1 b - - 0 1", { "Kf1" });

    // pinned pawn
    assert_moves("r4k2/8/5b2/8/1p6/1pP5/P7/K7 w - - 0 2", { "Kb1", "Kb2", "a3", "a4" });

    // capture pinning piece
    assert_moves("1r6/8/8/8/8/r1k5/Q7/K7 w - - 0 1", { "Qa2xa3+" });

    // check mate
    assert_moves("3R2k1/p1p2rpp/1q6/2p5/4P3/PQ6/1P4PP/7K b - - 0 1", {});

    // enpassant capture checking pawn
    assert_moves("3r1r2/pp2Qp2/7p/4PPpk/R5Pp/1P3N2/P3q2P/6K1 b - g3 0 1", { "hxg3ep" });
    assert_moves("8/8/6p1/6Pb/p2RpPk1/P3P1P1/6K1/8 b - f3 0 4", { "Kf5" });
    // moving pawn forward doesn't discover check here
    assert_moves("k7/8/8/8/8/Pp6/RP6/K7 w - - 0 2", { "a4", "Kb1" });

    // discovered check
    assert_moves("2r5/8/8/8/3k4/p7/1P6/BK6 w - - 0 2", { "bxa3+", "b3+", "b4+", "Ka2"});
    assert_moves("8/8/8/3B4/K6r/8/5BP1/7k w - - 0 2", { "g4+", "Bf2xh4", "Bf2d4", "Kb3", "Kb5", "Ka3", "Ka5", "Bd5e4", "Bd5c4" });
    assert_moves("kb6/8/8/2Pp4/8/8/6B1/7K w - d6 0 2", { "cxd6ep+", "Bg2xd5+", "Bg2e4", "Bg2f3", "c6", "Kg1", "Bg2f1", "Bg2h3" });

    // promote check
    assert_moves("8/5P2/1p6/pP6/P6p/8/5k2/7K w - - 0 67", { "Kh2", "f8=Q+", "f8=R+", "f8=B", "f8=N" });
    assert_moves("8/8/8/8/4K3/2N5/6p1/k5NB b - - 0 2", { "Kb2", "gxh1=Q+", "gxh1=R", "gxh1=B+", "gxh1=N" });

    // castle gives check
    assert_moves("8/8/8/8/8/7p/1pr4P/r1k1K2R w K - 0 1", { "Kf1", "O-O+", "Rh1g1", "Rh1f1" });
    // can't enpassant into check

    movetext.clear();
    b.set_fen("8/6r1/4B3/n1p1p1rb/2p1kPpR/1pR1P1b1/6P1/1K6 b - f3 0 2");
    legal_black.clear();
    legal_moves(&b, Black, legal_black);
    for (std::vector<move_t>::iterator iter = legal_black.begin(); iter != legal_black.end(); iter++) {
        movetext.str("");
        b.print_move(*iter, movetext);
        assert_not_equals(std::string("gxf3"), movetext.str());
        assert_not_equals(std::string("f3"), movetext.str());
    }

}

void test_move_finding()
{
    Fenboard b;
    SimpleEvaluation simple;
    Search search(&simple);
    move_t move;

    // white has mate in 1
    b.set_fen("rnbqkbnr/ppppp2p/5p2/6p1/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 1");
    search.max_depth = 2;
    move = search.minimax(b, White);
    assert_equals(VERY_GOOD - 1, search.score);
    search.reset();
    move = search.alphabeta(b, White);
    assert_equals(VERY_GOOD - 1, search.score);
    assert_equals(b.read_move("Qh5#", White), move);

    // white has mate in 2
    b.set_fen("r4kr1/1b2R1n1/pq4p1/4Q3/1p4P1/5P2/PPP4P/1K2R3 w - - 0 1");
    search.reset();
    search.use_killer_move = true;
    search.max_depth = 4;
    search.use_mtdf = false;
    search.use_pruning = true;
    search.use_quiescent_search = false;
    move = search.minimax(b, White);
    assert_equals(b.read_move("Rf7+", White), move);
    assert_equals(VERY_GOOD - 3, search.score);

    search.reset();
    move = search.alphabeta(b, White);
    assert_equals(b.read_move("Rf7+", White), move);
    assert_equals(VERY_GOOD - 3, search.score);

    b.set_fen("1Q6/8/8/8/8/k2K4/8/8 w - b6 0 1");
    search.reset();
    search.use_killer_move = true;
    search.max_depth = 4;
    search.use_mtdf = false;
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

    b.set_fen("r1b1k2r/p3bppp/2p2n2/1p2p3/3n4/2Pp4/PP1P1PPP/RNB1K1NR w KQkq - 0 12");
    search.reset();
    search.use_transposition_table = true;
    search.use_mtdf = true;
    search.use_quiescent_search = true;
    search.use_pruning = true;
    search.max_depth = 6;
    move = search.alphabeta(b, White);

    assert_equals(b.read_move("cxd4", White), move);
}

void test_matrix()
{
    alignas(32) unsigned char features[512];
    alignas(32) int8_t weights[512];

    for (int i = 0; i < 512; i++) {
        features[i] = 64;
        weights[i] = 64;
    }

    //int result = mmatrix<1, 1, int>::dotProduct_n(features, weights, 512);
//    assert_equals(2097152, result);
    /*
    int result = matrix<1, 1, int, 1>::dotProduct_32(features, weights);
    assert_equals(131072, result);
    result = matrix<1, 1, int, 1>::dotProduct_512(features, weights);
    assert_equals(2097152, result);
    */
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cout << "Expected arg: sample-game.pgn" << std::endl;
        return 1;
    }

    test_legal_moves(argv[1]);
    test_move_finding();
    test_matrix();
    return 0;
}
