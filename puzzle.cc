#include <iostream>
#include <fstream>
#include <time.h>
#include "search.hh"
#include "evaluate.hh"
#include "chess.hh"
#include "pgn.hh"

bool expect_move(Board &b, int depth, const std::string &expected_move, int &nodecount)
{
	int score;

	move_t move = alphabeta(b, SimpleEvaluation(), depth*2, White, score, nodecount);
	move_t expected_move_parsed = b.read_move(expected_move, White);
	if (expected_move_parsed != move) {
		std::cout << b << std::endl;
		std::cout << "expected " << expected_move << " but got ";
		b.print_move(move, std::cout);
		std::cout << std::endl;
		return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " file.png" << std::endl;
		return 1;
	}
	std::ifstream puzzles(argv[1]);
	std::map<std::string, std::string> game_metadata;
	std::vector<std::pair<std::string, std::string> > movelist;
	Board b;
	int passed = 0;
	int attempts = 0;
	int nodes = 0;
	double elapsed = 0;

	if (!puzzles) {
	  std::cerr << "Couldn't read " << argv[1] << std::endl;
	  exit(1);
	}

	while (!puzzles.eof()) {
		movelist.clear();
		game_metadata.clear();
		read_pgn(puzzles, game_metadata, movelist);
		if (game_metadata["FEN"] != "" && game_metadata["Black"] == "White to move") {
			b.set_fen(game_metadata["FEN"]);
			bool result = false; 
			int puzzle_nodecount = 0;
			clock_t start = clock();
			if (game_metadata["White"] == "Mate in one") {
				result = expect_move(b, 1, movelist[0].first, puzzle_nodecount);
			} else if (game_metadata["White"] == "Mate in two") {
				result = expect_move(b, 2, movelist[0].first, puzzle_nodecount);
			} else if (game_metadata["White"] == "Mate in three") {
				result = expect_move(b, 3, movelist[0].first, puzzle_nodecount);
			}
			elapsed += (clock() - start) *1.0 / CLOCKS_PER_SEC;
			attempts++;
			nodes += puzzle_nodecount;
			if (result) {
				passed++;
			} else {
				std::cout << " in " << game_metadata["Event"] << std::endl;
			}
			if (attempts % 200 == 0) {
				std::cout << "Puzzles solved: " << passed << "/" << attempts << " using " << nodes << " nodes at " << nodes/elapsed << " nodes/sec or " << attempts/elapsed << " puzzles/sec" << std::endl;

			}
		}
	}

	std::cout << "Puzzles solved: " << passed << "/" << attempts << " using " << nodes << " nodes at " << nodes/elapsed << " nodes/sec or " << attempts/elapsed << " puzzles/sec" << std::endl;
}
