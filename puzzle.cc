#include <iostream>
#include <fstream>
#include "search.hh"
#include "evaluate.hh"
#include "chess.hh"
#include "pgn.hh"

void expect_move(Board &b, int depth, const std::string &expected_move)
{
	int score;
	int nodecount;

	move_t move = alphabeta(b, SimpleEvaluation(), depth+1, White, score, nodecount);
	move_t expected_move_parsed = b.read_move(expected_move, White);
	if (expected_move_parsed != move || score != INT_MAX-depth*2+1) {
		std::cout << b << std::endl;
		std::cout << "expected " << expected_move << " but got ";
		b.print_move(move, std::cout);
		std::cout << std::endl;
		abort();
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
	std::vector<std::pair<std::string, std::string> > movelist;
	Board b;

	while (!puzzles.eof()) {
		movelist.clear();
		game_metadata.clear();
		read_pgn(puzzles, game_metadata, movelist);
		if (game_metadata["FEN"] != "") {
			std::cout << game_metadata["Event"] << std::endl;
			b.set_fen(game_metadata["FEN"]);
			if (game_metadata["White"] == "Mate in one") {
				expect_move(b, 1, movelist[0].first);
			} else if (game_metadata["White"] == "Mate in two") {
				expect_move(b, 2, movelist[0].first);
			} else if (game_metadata["White"] == "Mate in three") {
				expect_move(b, 3, movelist[0].first);
			}
		}
	}
}