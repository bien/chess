#include "pgn.hh"
#include <stdlib.h>

void pgn_move_choices(std::istream &input, std::map<std::string, std::string> &metadata, std::map<std::pair<int, bool>, std::vector<std::string> > &move_choices)
{
	std::string line;
	while (!input.eof()) {
		char buf[256];
		input.getline(buf, sizeof(buf), '\n');
		line = buf;
		if (line.size() > 0 && line[line.length() - 1] == '\r') {
			line = line.substr(0, line.length() - 1);
		}
		if (line.size() > 0 && line[0] == '[') {
			if (!move_choices.empty()) {
				break;
			}
			int spacepos = line.find(' ');
			int lastquote = line.rfind('"');
			metadata[line.substr(1, spacepos-1)] = line.substr(spacepos+2, lastquote-spacepos-2);
		}
		else if (line == "" && !move_choices.empty()) {
			break;
		}
		else if (line != "") {
			int pos = -1;
			int space, endmove, dot;
			bool done = false;
			while (!done && line.find('.', pos+1) != std::string::npos) {
				dot = line.find('.', pos+1);
				while (pos < 0 || line[pos] < '0' || line[pos] > '9') {
					pos++;
				}
				space = line.find(' ', dot+1);
				if (space == std::string::npos) {
					break;
				}
				endmove = line.find(' ', space+1);
				
				if (endmove == std::string::npos || endmove > line.length()) {
					endmove = line.length();
					done = true;
				}
				int moveno = atoi(line.substr(pos, dot - pos).c_str());
				if (line[dot+1] == '.') {
					// black
					while (line[++dot] == '.')
						;
					move_choices[std::pair<int, bool>(moveno, false)].push_back(line.substr(dot+1, space - dot - 1));
				} else {
					// white+black
					move_choices[std::pair<int, bool>(moveno, true)].push_back(line.substr(dot+1, space - dot - 1));
					move_choices[std::pair<int, bool>(moveno, false)].push_back(line.substr(space+1, endmove-space-1));
				}
				pos = endmove + 1;
			}
		}
	}	
}

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
			if (!movelist.empty()) {
				break;
			}
			int spacepos = line.find(' ');
			int lastquote = line.rfind('"');
			metadata[line.substr(1, spacepos-1)] = line.substr(spacepos+2, lastquote-spacepos-2);
		}
		else if (line == "" && !movelist.empty()) {
			break;
		}
		else if (line != "") {
			int pos = -1;
			int space, endmove;
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
				if (line[pos+1] == '.') {
					while (line[++pos] == '.')
						;
					movelist.push_back(std::pair<std::string, std::string>(
						"", line.substr(pos + 1, space - pos - 1)));

				} else {
					movelist.push_back(std::pair<std::string, std::string>(
						line.substr(pos + 1, space - pos - 1), line.substr(space+1, endmove - space - 1)));
				}
				pos = endmove + 1;
			}
		}
	}
}
