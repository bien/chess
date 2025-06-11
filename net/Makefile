CXX=g++
CC=gcc
CFLAGS = -Wall -g -march=native -I.. -fPIC -I/opt/homebrew/include -O3
CXXFLAGS = $(CFLAGS) -std=c++20 -DDEBUG
SRCS = test.cc nnue.cc
OBJS = ../bitboard.o ../fenboard.o ../evaluate.o ../pgn.o ../search.o  ../move.o
LDFLAGS = -L/opt/homebrew/lib -lzstd

all: test

clean:
	rm -f *.o test

test: $(OBJS) trainlib.o
	$(CXX) $^ -o $@

Makefile.deps: Makefile $(SRCS)
	$(CXX) $(CFLAGS) -MM $(SRCS) > $@

%.dylib: %.o $(OBJS)
	$(CXX) $(LDFLAGS) -dynamiclib -flat_namespace $^ -o $@

%.so: %.o $(OBJS)
	$(CXX) -shared $^ -o $@

trainlib: ../libengine.a trainlib.o
	$(CXX) $(LDFLAGS) $^ -o $@

include Makefile.deps
