CFLAGS = -Wall -g -march=native -I..
CXXFLAGS = $(CFLAGS) -std=c++20
SRCS = test.cc nnue.cc
OBJS = nnue.o nnue-elite-5-12.o ../bitboard.o ../fenboard.o ../move.o ../evaluate.o ../pgn.o ../logicalboard.o

all: test

clean:
	rm -f *.o test


test: test.o $(OBJS)
	$(CXX) $^ -o $@

Makefile.deps: Makefile $(SRCS)
	$(CXX) $(CFLAGS) -MM $(SRCS) > $@

include Makefile.deps
