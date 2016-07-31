#include "chess.hh"
#include <iostream>

int main(int argc, char **argv)
{
	Board b;
	if (argc < 2) {
		std::cout << "Usage: fen-string" << std::endl;
		return 1;
	}
	char *fen = argv[1];
	b.set_fen(fen);
	Color color = Black;

	int counter = 0;
	MoveIterator iter(&b, color);
	for (; !iter.at_end(); ++iter) {
		b.print_move(*iter, std::cout);
		std::cout << std::endl;
		counter++;
	}
	std::cout << "count " << counter << std::endl;

	return 0;
}
