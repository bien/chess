CXXFLAGS = -Wall -g

feature: extractfeatures.o chess.o pgn.o
	$(CXX) $^ -o $@

chess: moves.o chess.o
	$(CXX) $^ -o $@
	
testexe: chess.o test.o pgn.o
	$(CXX) $^ -o $@
	
test: testexe
	./testexe

clean:
	rm -f *.o chess test

%.features: games/%.zip feature
	gunzip < $< | ./feature > $@
	

	