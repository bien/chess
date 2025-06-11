CXX = g++
INCLUDES = -Inet -I.  -I/opt/homebrew/include
CXXFLAGS = -Wall -g -std=c++20 -march=native $(INCLUDES) -fsanitize=address
ENGINE_SRCS = bitboard.cc fenboard.cc move.cc search.cc evaluate.cc pgn.cc nnueeval.cc nnue-2-layer-16.cc
ENGINE_OBJS = $(ENGINE_SRCS:.cc=.o)
OTHER_SRCS = magicsquares.cc puzzle.cc test.cc
LDFLAGS = -fsanitize=address

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
	$(CXX) $^ -o $@

magicsquares: magicsquares.o bitboard.o
	$(CXX) $^ -o $@

start: start.o libengine.a
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
	rm -f *.o $(ENGINE_OBJS) chess

Makefile.deps: Makefile $(ENGINE_SRCS) $(OTHER_SRCS)
	$(CXX) -MM $(ENGINE_SRCS) $(INCLUDES) $(OTHER_SRCS) > $@

include Makefile.deps
