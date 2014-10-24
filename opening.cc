#include <iostream>
#include <time.h>
#include "search.hh"
#include "evaluate.hh"
#include "chess.hh"

int main(int argc, char **argv)
{
	int depth = 4;
	if (argc > 1) {
		depth = atoi(argv[1]);
	}
	Board b;
	SimpleEvaluation simple;
	Search search(&simple);
	search.use_pruning = false;
	search.use_transposition_table = false;
	search.use_mtdf = false;
	clock_t start = clock();
	
	move_t move = search.alphabeta(b, depth, White);
	double elapsed = (clock() - start) *1.0 / CLOCKS_PER_SEC;
	std::cout << "minimax only   : ";
	b.print_move(move, std::cout);
	std::cout << " nodes: " << search.nodecount << " score: " << search.score << " secs: " << elapsed << std::endl;
	
	search.reset();
	search.use_pruning = true;
	start = clock();
	
	move = search.alphabeta(b, depth, White);
	elapsed = (clock() - start) *1.0 / CLOCKS_PER_SEC;
	std::cout << " with pruning  : ";
	b.print_move(move, std::cout);
	std::cout << " nodes: " << search.nodecount << " score: " << search.score << " secs: " << elapsed << std::endl;

	search.reset();
	search.use_transposition_table = true;
	start = clock();
	
	move = search.alphabeta(b, depth, White);
	elapsed = (clock() - start) *1.0 / CLOCKS_PER_SEC;
	std::cout << " with transpose: ";
	b.print_move(move, std::cout);
	std::cout << " nodes: " << search.nodecount << " score: " << search.score << " secs: " << elapsed << std::endl;

	search.reset();
	search.use_mtdf = true;
	start = clock();
	
	move = search.alphabeta(b, depth, White);
	elapsed = (clock() - start) *1.0 / CLOCKS_PER_SEC;
	std::cout << " with mtdf     : ";
	b.print_move(move, std::cout);
	std::cout << " nodes: " << search.nodecount << " score: " << search.score << " secs: " << elapsed << std::endl;
	return 0;
}
