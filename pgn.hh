#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <utility>

struct move_annot {
    std::string move;
    std::string clock;
    std::string eval;
    int moveno;
    bool is_white;
};

typedef std::vector<std::vector<move_annot> > movelist_tree;

void read_pgn_options(std::istream &input, std::map<std::string, std::string> &metadata, movelist_tree &movelist);
void read_pgn(std::istream &input, std::map<std::string, std::string> &metadata, std::vector<std::pair<move_annot, move_annot> > &movelist);
