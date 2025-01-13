CXX = g++-14
INCLUDES = -Inet -I.
CXXFLAGS = -Wall -g -std=c++20 -march=native $(INCLUDES) -DHALFKASINGLE -O
ENGINE_SRCS = bitboard.cc fenboard.cc move.cc search.cc evaluate.cc pgn.cc net/nnue.cc
ENGINE_OBJS = $(ENGINE_SRCS:.cc=.o) net/model.2.768.20250101.o
OTHER_SRCS = magicsquares.cc puzzle.cc test.cc

all: uciinterface test

chess: bitboard.o fenboard.o test.o move.o
	$(CXX) $^ -o $@

uciinterface: $(ENGINE_OBJS) uciinterface.o
	$(CXX) $^ -o $@

moves: moves.o $(ENGINE_OBJS)
	$(CXX) $^ -o $@

magicsquares: magicsquares.o bitboard.o
	$(CXX) $^ -o $@

start: start.o $(ENGINE_OBJS)
	$(CXX) $^ -o $@

test: testexe puzzle
	./testexe games/Fischer.pgn
	./puzzle "polgar - fixed.pgn"

playover: $(ENGINE_OBJS) playover.o
	$(CXX) $^ -o $@

testexe: $(ENGINE_OBJS) test.o
	$(CXX) $^ -o $@

puzzle: puzzle.o $(ENGINE_OBJS)
	$(CXX) $^ -o $@

clean:
	rm -f *.o $(ENGINE_OBJS) chess

Makefile.deps: Makefile $(ENGINE_SRCS) $(OTHER_SRCS)
	$(CXX) -MM $(ENGINE_SRCS) $(INCLUDES) $(OTHER_SRCS) > $@

include Makefile.deps
