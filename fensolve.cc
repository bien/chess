#include <iostream>
#include <fstream>
#include <time.h>
#include "search.hh"
#include "evaluate.hh"
#include "chess.hh"
#include "pgn.hh"

int main(int argc, char **argv)
{
	std::string fen;
	int depth = 4;
	int argn = 1;
	if (argc > argn && strcmp(argv[argn], "-v") == 0) {
		search_debug = 1;
		argn++;
	}
	else if (argc > argn && strcmp(argv[argn], "-V") == 0) {
		search_debug = 2;
		argn++;
	}
	if (argn >= argc - 1) {
		std::cerr << "Usage: " << argv[0] << " [-v] depth <fen>" << std::endl;
		return 1;
	}
	depth = atoi(argv[argn++]);
	while (argn < argc) {
		fen += argv[argn++];
		fen += " ";
	}
	Board b;
	b.set_fen(fen);
	int nodes = 0;
	double elapsed = 0;
	
	SimpleEvaluation simple;
	Search search(&simple);
	search.use_transposition_table = true;
	search.use_mtdf = true;
	
	clock_t start = clock();
	move_t move = search.alphabeta(b, depth, b.get_side_to_play());
	elapsed += (clock() - start) *1.0 / CLOCKS_PER_SEC;
	nodes = search.nodecount;
	std::cout << "Found: ";
	b.print_move(move, std::cout);
	std::cout << " using " << nodes << " nodes at " << nodes/elapsed << " nodes/sec" << std::endl;

	return 0;
}
