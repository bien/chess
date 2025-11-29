CXX = g++
INCLUDES = -Inet -I. -I/usr/local/include -I/opt/homebrew/include
CXXFLAGS = -Wall -g -std=c++20 -march=native $(INCLUDES) -O3
ENGINE_SRCS = net/psqt.cc bitboard.cc fenboard.cc move.cc search.cc evaluate.cc pgn.cc nnueeval.cc nnue-2-layer-64.cc
ENGINE_OBJS = $(ENGINE_SRCS:.cc=.o)
OTHER_SRCS = magicsquares.cc puzzle.cc test.cc cmdeval.cc
LDFLAGS =  -L/opt/homebrew/lib -lboost_program_options
DSYMUTIL = dsymutil

all: uciinterface test

libengine.a: $(ENGINE_OBJS)
	ar rcs $@ $^

annotate: annotate.o $(ENGINE_OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@
	$(DSYMUTIL) $@

cmdeval: cmdeval.o $(ENGINE_OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@
	codesign --entitlements entitlements.plist -f -s "-" --options runtime $@

chess: bitboard.o fenboard.o test.o move.o
	$(CXX) $^ -o $@

uciinterface: $(ENGINE_OBJS) uciinterface.o
	$(CXX) $^ -o $@

magicsquares: magicsquares.o bitboard.o
	$(CXX) $^ -o $@

start: start.o $(ENGINE_OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

test: testexe puzzle
	./testexe games/Fischer.pgn
	./puzzle lichess_db_puzzle.csv.head

testexe: $(ENGINE_OBJS) test.o
	$(CXX) $^ -o $@

puzzle: puzzle.o $(ENGINE_OBJS)
	$(CXX) $^ -o $@
	$(DSYMUTIL) $@

clean:
	rm -f *.o *.a $(ENGINE_OBJS) annotate uciinterface testexe puzzle cmdeval magicsquares start

Makefile.deps: Makefile $(ENGINE_SRCS) $(OTHER_SRCS)
	$(CXX) -MM $(ENGINE_SRCS) $(INCLUDES) $(OTHER_SRCS) > $@

include Makefile.deps
