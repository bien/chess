INCLUDES = -Inet -I.
CXXFLAGS = -Wall -g -std=c++20 -O3 -march=native $(INCLUDES)
ENGINE_SRCS = bitboard.cc fenboard.cc move.cc search.cc evaluate.cc pgn.cc logicalboard.cc net/nnue.cc
ENGINE_OBJS = $(ENGINE_SRCS:.cc=.o) net/nnue-staged-dual-1epochb-42.o
OTHER_SRCS = magicsquares.cc puzzle.cc test.cc

all: uciinterface test

chess: bitboard.o fenboard.o test.o move.o
	$(CXX) $^ -o $@

uciinterface: bitboard.o fenboard.o move.o search.o evaluate.o pgn.o logicalboard.o uciinterface.o
	$(CXX) $^ -o $@

moves: moves.o bitboard.o fenboard.o move.o search.o evaluate.o pgn.o logicalboard.o
	$(CXX) $^ -o $@

magicsquares: magicsquares.o bitboard.o
	$(CXX) $^ -o $@

start: start.o $(ENGINE_OBJS)
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

puzzle: puzzle.o $(ENGINE_OBJS)
	$(CXX) $^ -o $@

clean:
	rm -f *.o $(ENGINE_OBJS) chess

Makefile.deps: Makefile $(ENGINE_SRCS) $(OTHER_SRCS)
	$(CXX) -MM $(ENGINE_SRCS) $(INCLUDES) $(OTHER_SRCS) > $@

include Makefile.deps
