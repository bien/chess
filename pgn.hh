#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <utility>

void pgn_move_choices(std::istream &input, std::map<std::string, std::string> &metadata, std::map<std::pair<int, bool>, std::vector<std::string> > &move_choices);
void read_pgn(std::istream &input, std::map<std::string, std::string> &metadata, std::vector<std::pair<std::string, std::string> > &movelist);
