#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <utility>

struct move_annot {
    std::string move;
    std::string clock;
    std::string eval;
};

void pgn_move_choices(std::istream &input, std::map<std::string, std::string> &metadata, std::map<std::pair<int, bool>, std::vector<std::string> > &move_choices);
void read_pgn(std::istream &input, std::map<std::string, std::string> &metadata, std::vector<std::pair<move_annot, move_annot> > &movelist);
