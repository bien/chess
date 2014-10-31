#include <iostream>
#include <fstream>
#include <time.h>
#include "search.hh"
#include "evaluate.hh"
#include "chess.hh"
#include "pgn.hh"

bool expect_move(Search &search, Board &b, int depth, const std::vector<std::string> &expected_move, int &nodecount)
{
	move_t move = search.alphabeta(b, depth, White);
	nodecount = search.nodecount;
	for (std::vector<std::string>::const_iterator iter = expected_move.begin(); iter != expected_move.end(); iter++) {
		move_t expected_move_parsed = b.read_move(*iter, White);
		if (expected_move_parsed == move) {
			return true;
		}
	}
	if (expected_move.size() == 0) {
		std::cerr << "Error: couldn't find expected move" << std::endl;
		return false;
	}
	std::cout << b << std::endl;
	std::cout << "at depth=" << depth << " expected " << expected_move.front() << " but got ";
	b.print_move(move, std::cout);
	std::cout << std::endl;
	return false;
}

void get_first_move_choices(const std::map<std::pair<int, bool>, std::vector<std::string> > &move_choices, std::vector<std::string> &first_moves)
{
	int best_move = 1000;
	for (auto iter = move_choices.begin(); iter != move_choices.end(); iter++) {
		if (iter->first.second == true && iter->first.first < best_move) {
			first_moves = iter->second;
			best_move = iter->first.first;
		}
	}
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " file.png" << std::endl;
		return 1;
	}
	std::ifstream puzzles(argv[1]);
	std::map<std::string, std::string> game_metadata;
	std::map<std::pair<int, bool>, std::vector<std::string> > move_choices;
	std::vector<std::string> first_move;
	Board b;
	int passed = 0;
	int attempts = 0;
	int nodes = 0;
	double elapsed = 0;

	if (!puzzles) {
	  std::cerr << "Couldn't read " << argv[1] << std::endl;
	  exit(1);
	}
	
	SimpleEvaluation simple;
	Search search(&simple);
	search.use_transposition_table = true;
	search.use_mtdf = true;

	while (!puzzles.eof()) {
		move_choices.clear();
		game_metadata.clear();
		search.reset();
		pgn_move_choices(puzzles, game_metadata, move_choices);
		if (game_metadata["FEN"] != "" && game_metadata["Black"] == "White to move") {
			b.set_fen(game_metadata["FEN"]);
			bool result = false; 
			int puzzle_nodecount = 0;
			clock_t start = clock();
			int depth = 3;
			if (game_metadata["White"] == "Mate in one") {
				depth = 2;
			} else if (game_metadata["White"] == "Mate in two") {
				depth = 4;
			} else if (game_metadata["White"] == "Mate in three") {
				depth = 6;
			}
			get_first_move_choices(move_choices, first_move);
			result = expect_move(search, b, depth, first_move, puzzle_nodecount);
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
