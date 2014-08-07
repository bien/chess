#include <iostream>
#include <fstream>
#include "search.hh"
#include "evaluate.hh"
#include "chess.hh"
#include "pgn.hh"

template <class T>
void assert_equals(T expected, T actual)
{
	if (expected != actual) {
		std::cout << "Test failed: expected " << expected << " but got " << actual << std::endl;
		abort();
	}
}

void expect_move(Board &b, int depth, const std::string &expected_move)
{
	int score;
	int nodecount;

	move_t move = alphabeta(b, SimpleEvaluation(), 2, White, score, nodecount);
	assert_equals(INT_MAX-depth*2+1, score);
	assert_equals(b.read_move(expected_move, White), move);
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