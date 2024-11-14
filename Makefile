CXXFLAGS = -Wall -g -std=c++14 -O3 -march=native
SRCS = bitboard.cc fenboard.cc test.cc move.cc magicsquares.cc search.cc evaluate.cc pgn.cc logicalboard.cc puzzle.cc endgames.cc uciinterface.cc

all: uciinterface test

chess: bitboard.o fenboard.o test.o move.o
	$(CXX) $^ -o $@

uciinterface: bitboard.o fenboard.o move.o search.o evaluate.o pgn.o logicalboard.o uciinterface.o
	$(CXX) $^ -o $@

moves: moves.o bitboard.o fenboard.o move.o search.o evaluate.o pgn.o logicalboard.o
	$(CXX) $^ -o $@

magicsquares: magicsquares.o bitboard.o
	$(CXX) $^ -o $@

start: start.o bitboard.o fenboard.o move.o search.o evaluate.o pgn.o logicalboard.o
	$(CXX) $^ -o $@

test: testexe puzzle
	./testexe games/Fischer.pgn
	./puzzle "polgar - fixed.pgn"

testexe: bitboard.o fenboard.o test.o move.o search.o evaluate.o pgn.o logicalboard.o
	$(CXX) $^ -o $@

pgn2training: bitboard.o fenboard.o pgn2training.o move.o pgn.o logicalboard.o
	$(CXX) $^ -o $@
	
pgnreader: pgnreader.o pgn.o bitboard.o fenboard.o move.o logicalboard.o
	$(CXX) $^ -o $@

endgames: bitboard.o fenboard.o endgames.o move.o logicalboard.o
	$(CXX) $^ -o $@

puzzle: bitboard.o fenboard.o puzzle.o move.o search.o evaluate.o pgn.o logicalboard.o
	$(CXX) $^ -o $@

clean:
	rm -f *.o chess

Makefile.deps: Makefile $(SRCS)
	$(CXX) -MM $(SRCS) > $@

include Makefile.deps
