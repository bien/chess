CXX=g++
CC=gcc
CFLAGS = -Wall -g -march=native -I.. -O3 -fPIC
CXXFLAGS = $(CFLAGS) -std=c++20 -DDEBUG
SRCS = test.cc nnue.cc
OBJS = nnue-elite-5-stockfish-9.o ../bitboard.o ../fenboard.o ../move.o ../evaluate.o ../pgn.o ../logicalboard.o nnue.o

all: test

clean:
	rm -f *.o test

test: $(OBJS) test.o
	$(CXX) $^ -o $@

Makefile.deps: Makefile $(SRCS)
	$(CXX) $(CFLAGS) -MM $(SRCS) > $@

%.dylib: %.o $(OBJS)
	$(CXX) -dynamiclib -undefined suppress -flat_namespace $^ -o $@

%.so: %.o $(OBJS)
	$(CXX) -shared $^ -o $@

include Makefile.deps
