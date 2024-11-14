#include "pgn.hh"
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

int main(int argc, char **argv)
{
    std::map<std::string, std::string> game_metadata;
    movelist_tree move_choices;

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " file.png" << std::endl;
        return 1;
    }
    std::ifstream puzzles(argv[1]);
  
    if (!puzzles) {
      std::cerr << "Couldn't read " << argv[1] << std::endl;
      exit(1);
    }
    while (!puzzles.eof()) {
        move_choices.clear();
        game_metadata.clear();
        read_pgn_options(puzzles, game_metadata, move_choices);
        std::cout << "*****" << std::endl;
        for (auto metadata_iter = game_metadata.begin(); metadata_iter != game_metadata.end(); metadata_iter++) {
            std::cout << metadata_iter->first << ": " << metadata_iter->second << std::endl;
        }
        for (auto line = move_choices.begin(); line != move_choices.end(); line++) {
            int moveno = 0;
            for (auto ply = line->begin(); ply != line->end(); ply++) {
                if (ply->is_white) {
                    std::cout << ply->moveno << ". " << ply->move << "  ";
                    moveno = ply->moveno;
                } else {
                    if (ply->moveno > moveno) {                    
                        std::cout << moveno << "...";
                    }
                    std::cout << ply->move << "  ";
                }
            }
            std::cout << std::endl;
        }
    }   
    
}