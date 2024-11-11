#include "pgn.hh"
#include <iostream>
#include <stdlib.h>

void pgn_move_choices(std::istream &input, std::map<std::string, std::string> &metadata, std::map<std::pair<int, bool>, std::vector<std::string> > &move_choices)
{
    std::string line;
    int moveno = 0;
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
                if (line.substr(pos, dot - pos).find_first_not_of("0123456789") == std::string::npos) {
                    int new_moveno = atoi(line.substr(pos, dot - pos).c_str());
                    if (new_moveno != moveno + 1) {
                        // confused
                        break;
                    }
                    moveno = new_moveno;
                }
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

bool is_result(const std::string &candidate_move)
{
    return candidate_move == "1-0" || candidate_move == "0-1" || candidate_move == "1/2-1/2";
}

void read_annotation(const std::string &buf, int start_pos, int end_pos, std::string &eval, std::string &clock)
{
    while (start_pos < end_pos) {
        int open_bracket = buf.find('[', start_pos);
        int close_bracket = buf.find(']', open_bracket);
        int space = buf.find(' ', open_bracket);
        if (open_bracket < 0 || close_bracket < 0 || space < open_bracket || space >= close_bracket) {
            break;
        }
        std::string key = buf.substr(open_bracket + 1, space - open_bracket - 1);
        std::string val = buf.substr(space + 1, close_bracket - space - 1);
        if (key == "%clk") {
            clock = val;
        }
        else if (key == "%eval") {
            eval = val;
        }
        start_pos = close_bracket;
    }
}

void read_pgn(std::istream &input, std::map<std::string, std::string> &metadata, std::vector<std::pair<move_annot, move_annot> > &movelist)
{
    std::string line;
    int pos = 0;
    while (!input.eof() && !input.fail()) {
        std::string buf;
        std::getline(input, buf, '\n');
        line.append(buf);
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
            // don't need this content anymore
            line.clear();
        }
        else if (buf.size() <= 1 && line.size() > 2) {
            // newline ends the game
            break;
        }
        else if (line.size() > 0) {
            // process actual moves
            line.append(" ");
        }
    }
    if (metadata.empty() && line.size() > 2) {
        std::cerr << "Error: no pgn header, are you sure this is pgn? " << std::endl;
        abort();
    }
    while (pos < line.size() && pos >= 0) {
        // 1. e4 g6 2. Ne2 Bg7 3.
        int start_white_move, end_white_move, start_black_move, end_black_move;

        pos = line.find('.', pos);

        if (pos < 0) {
            break;
        }

        // skip any spaces or extra periods
        while (line[++pos] == ' ')
            ;
        start_white_move = pos;
        end_white_move = line.find(' ', pos);
        if (start_white_move == -1 || end_white_move == -1) {
            break;
        }
        pos = end_white_move;
        move_annot white_move;
        white_move.move = line.substr(start_white_move, end_white_move - start_white_move);
        while (line[++pos] == ' ')
            ;
        // lichess move annotations: eg. 1. e4 { [%eval 0.2] [%clk 0:05:00] } 1... e5 { [%eval 0.17] [%clk 0:05:00] }
        if (line[pos] == '{') {
            int end_annotation = line.find('}', pos);
            if (end_annotation > pos) {
                read_annotation(line, pos, end_annotation, white_move.eval, white_move.clock);
                pos = end_annotation + 1;
            } else {
                std::cerr << "Unterminated move metadata around " << line.substr(pos - 5, 10) << std::endl;
                abort();
            }
        }
        // check for restatement of move number, eg. "1..." but don't get tripped by 1/2-1/2
        int move_pos = pos;
        bool found_dot = false;
        while (line[move_pos] == '.' || line[move_pos] == ' ' || (line[move_pos] >= '0' && line[move_pos] <= '9')) {
            if (line[move_pos] == '.') {
                found_dot = true;
            }
            move_pos++;
        }
        if (found_dot) {
            pos = move_pos;
        }
        start_black_move = pos;
        end_black_move = line.find(' ', start_black_move);
        pos = end_black_move;

        move_annot black_move;
        if (end_black_move >= 0 && start_black_move >= 0) {
            black_move.move = line.substr(start_black_move, end_black_move - start_black_move);
            while (line[++pos] == ' ')
                ;
            if (line[pos] == '{') {
                int end_annotation = line.find('}', pos);
                if (end_annotation > pos) {
                    read_annotation(line, pos, end_annotation, black_move.eval, black_move.clock);
                    pos = end_annotation + 1;
                } else {
                    std::cerr << "Unterminated move metadata around " << line.substr(pos - 5, 10) << std::endl;
                    abort();
                }
            }
        }
        if (is_result(white_move.move)) {
            break;
        }
        if (is_result(black_move.move)) {
            black_move.move = "";
        }

        movelist.push_back(std::pair<move_annot, move_annot>(white_move, black_move));
    }
}
