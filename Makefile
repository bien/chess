CXX = g++
INCLUDES = -Inet -I.  -I/opt/homebrew/include
CXXFLAGS = -Wall -O3 -g -std=c++20 $(INCLUDES)
ENGINE_SRCS = bitboard.cc fenboard.cc move.cc search.cc evaluate.cc pgn.cc nnueeval.cc nnue-2-layer-256.cc
ENGINE_OBJS = $(ENGINE_SRCS:.cc=.o)
OTHER_SRCS = magicsquares.cc puzzle.cc test.cc
LDFLAGS =

all: uciinterface test

libengine.a: $(ENGINE_OBJS)
	ar rcs $@ $^

chess: bitboard.o fenboard.o test.o move.o
	$(CXX) $^ -o $@

uciinterface: $(ENGINE_OBJS) uciinterface.o
	$(CXX) $^ -o $@

moves: moves.o $(ENGINE_OBJS)
	$(CXX) $^ -o $@

neteval: neteval.o $(ENGINE_OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@

magicsquares: magicsquares.o bitboard.o
	$(CXX) $^ -o $@

start: start.o $(ENGINE_OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

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
	rm -f *.o *.a $(ENGINE_OBJS) chess

Makefile.deps: Makefile $(ENGINE_SRCS) $(OTHER_SRCS)
	$(CXX) -MM $(ENGINE_SRCS) $(INCLUDES) $(OTHER_SRCS) > $@

include Makefile.deps
