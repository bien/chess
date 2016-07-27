CXXFLAGS = -Wall -g -Wno-c++11-extensions -O2
SRCS = extractfeatures.cc chess.cc pgn.cc search.cc evaluate.cc test.cc puzzle.cc opening.cc

feature: extractfeatures.o chess.o pgn.o
	$(CXX) $^ -o $@

chess: moves.o chess.o
	$(CXX) $^ -o $@

bitboard: bitboard.o
	$(CXX) $^ -o $@

testexe: chess.o test.o pgn.o search.o evaluate.o
	$(CXX) $^ -o $@

puzzles: chess.o pgn.o search.o evaluate.o puzzle.o
	$(CXX) $^ -o $@

fensolve: chess.o pgn.o search.o evaluate.o fensolve.o
	$(CXX) $^ -o $@

opening: opening.o search.o evaluate.o chess.o
	$(CXX) $^ -o $@

test: testexe puzzles
	./testexe
	./puzzles "polgar - fixed.pgn"

clean:
	rm -f *.o chess test

%.features: games/%.zip feature
	unzip -p $< | ./feature > $@

Makefile.deps: Makefile $(SRCS)
	$(CXX) -MM $(SRCS) > $@
