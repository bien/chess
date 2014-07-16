#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <utility>

void read_pgn(std::istream &input, std::map<std::string, std::string> &metadata, std::vector<std::pair<std::string, std::string> > &movelist);
