#include "pgn.hh"

void read_pgn(std::istream &input, std::map<std::string, std::string> &metadata, std::vector<std::pair<std::string, std::string> > &movelist)
{
	std::string line;
	while (!input.eof()) {
		char buf[256];
		input.getline(buf, sizeof(buf), '\n');
		line = buf;
		if (line[line.length() - 1] == '\r') {
			line = line.substr(0, line.length() - 1);
		}
		if (line[0] == '[') {
			int spacepos = line.find(' ');
			int lastquote = line.rfind('"');
			metadata[line.substr(1, spacepos-1)] = line.substr(spacepos+2, lastquote-spacepos-2);
		}
		else if (line == "" && !movelist.empty()) {
			break;
		}
		else if (line != "") {
			int pos = -1;
			unsigned int space, endmove;
			bool done = false;
			while (!done && line.find('.', pos+1) != std::string::npos) {
				pos = line.find('.', pos+1);
				space = line.find(' ', pos+1);
				if (space == std::string::npos) {
					break;
				}
				endmove = line.find(' ', space+1);
				
				if (endmove == std::string::npos || endmove > line.length()) {
					endmove = line.length();
					done = true;
				}
				movelist.push_back(std::pair<std::string, std::string>(
					line.substr(pos + 1, space - pos - 1), line.substr(space+1, endmove - space - 1)));
				pos = endmove + 1;
			}
		}
	}
}
