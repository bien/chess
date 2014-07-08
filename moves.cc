#include "chess.hh"
#include <iostream>

int main(int argc, char **argv)
{
	Board b;
	std::vector<move_t> moves;
	
	b.legal_moves(White, moves);
	
	for (unsigned int i = 0; i < moves.size(); i++) {
		b.print_move(moves[i], std::cout);
		std::cout << std::endl;
	}
	
	return 0;
}