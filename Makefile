CXXFLAGS = -Wall -g -std=c++14 -O2
SRCS = bitboard.cc fenboard.cc test.cc move.cc magicsquares.cc search.cc evaluate.cc pgn.cc logicalboard.cc puzzle.cc

all: test

chess: bitboard.o fenboard.o test.o move.o
	$(CXX) $^ -o $@

magicsquares: magicsquares.o bitboard.o
	$(CXX) $^ -o $@

test: testexe puzzle
	./testexe games/Fischer.pgn
	./puzzle "polgar - fixed.pgn"

testexe: bitboard.o fenboard.o test.o move.o search.o evaluate.o pgn.o logicalboard.o
	$(CXX) $^ -o $@

puzzle: bitboard.o fenboard.o puzzle.o move.o search.o evaluate.o pgn.o logicalboard.o
	$(CXX) $^ -o $@

clean:
	rm -f *.o chess

Makefile.deps: Makefile $(SRCS)
	$(CXX) -MM $(SRCS) > $@

include Makefile.deps
