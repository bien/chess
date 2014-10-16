CXXFLAGS = -Wall -g -Wno-c++11-extensions

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

test: testexe puzzles
	./testexe
	./puzzles "polgar - fixed.pgn"

clean:
	rm -f *.o chess test

%.features: games/%.zip feature
	unzip -p $< | ./feature > $@
	

	