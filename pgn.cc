#include "pgn.hh"
#include <iostream>
#include <stdlib.h>

void read_annotation(const std::string &buf, int start_pos, int end_pos, std::string &eval, std::string &clock);
bool is_result(const std::string &candidate_move);

void skip_whitespace(const std::string &line, int &pos) {
    while (pos < line.length() && (line[pos] == ' ' || line[pos] == '\n' || line[pos] == '\r')) {
        pos++;
    }
}

bool read_move_annot(const std::string &line, int &start_pos, move_annot &move)
{
    int start_move, end_move;

    int pos = line.find('.', start_pos);

    if (pos < 0 || line.substr(start_pos, pos - start_pos).find_first_of("0123456789") != 0) {
        pos = start_pos;
        move.moveno = 0;
    } else {
        move.moveno = atoi(line.substr(start_pos, pos - start_pos).c_str());
        move.is_white = (line[++pos] != '.');
    }

    // skip any spaces or extra periods
    while (pos < line.length() && (line[pos] == ' ' || line[pos] == '.'))
        pos++;
    start_move = pos;
    end_move = line.find(' ', pos);
    if (start_move == -1 || end_move == -1) {
        return false;
    }
    if (line[end_move-1] == ')') {
        end_move--;
        pos--;
    }
    pos = end_move;
    move.move = line.substr(start_move, end_move - start_move);
    // results like 1/2-1/2 or random punctuation aren't moves
    if (is_result(move.move) || (move.move.find_first_of("0123456789Oabcdefgh") == std::string::npos)) {
        return false;
    }
    skip_whitespace(line, pos);

    // lichess move annotations: eg. 1. e4 { [%eval 0.2] [%clk 0:05:00] } 1... e5 { [%eval 0.17] [%clk 0:05:00] }
    if (line[pos] == '{') {
        int end_annotation = line.find('}', pos);
        if (end_annotation > pos) {
            read_annotation(line, pos, end_annotation, move.eval, move.clock);
            pos = end_annotation + 1;
        } else {
            std::cerr << "Unterminated move metadata around " << line.substr(pos - 5, 10) << std::endl;
            return false;
        }
    }
    start_pos = pos;
    return true;
}

void clean_line(std::vector<move_annot> &moveline) {
    // removes doubled moves
    move_annot *min_move;
    bool is_first = true;

    for (auto iter = moveline.rbegin(); iter != moveline.rend(); iter++) {
        if (is_first) {
            min_move = &moveline.back();
            is_first = false;
        } else {
            // case one: moving backwards
            if (iter->moveno < min_move->moveno || (iter->moveno == min_move->moveno && iter->is_white && !min_move->is_white)) {
                min_move = &*iter;
            }
            // case two: same or forwards -> omit
            else {
                // stupid c++ hack because moveline.erase(iter) doesn't work for reverse iter
                moveline.erase((iter+1).base());
            }
        }
    }
}

void read_pgn_options(pgn_input_stream *input, std::map<std::string, std::string> &metadata, movelist_tree &movelist)
{
    std::string line;
    int pos = 0;
    while (input->is_readable()) {
        std::string buf;
        input->read_line(buf);
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

    movelist_tree linestack;
    linestack.push_back(std::vector<move_annot>());

    while (pos < line.size() && pos >= 0) {
        // 1. e4 g6 2. Ne2 Bg7 3.
        move_annot move;

        skip_whitespace(line, pos);

        if (line[pos] == '(') {
            linestack.push_back(linestack.back());
            pos++;
        }
        else if (line[pos] == ')') {
            clean_line(linestack.back());
            movelist.push_back(linestack.back());
            linestack.pop_back();
            pos++;
        }
        else {
            if (!read_move_annot(line, pos, move)) {
                break;
            }
            if (move.moveno == 0 && linestack.back().back().is_white) {
                // guess moveno matches previous move since often omitted
                move.moveno = linestack.back().back().moveno;
                move.is_white = false;
            }
            linestack.back().push_back(move);
        }

    }

    for (auto iter = linestack.begin(); iter != linestack.end(); iter++) {
        movelist.push_back(*iter);
    }
}

bool is_result(const std::string &candidate_move)
{
    return (candidate_move[0] == '1' && (candidate_move == "1-0" || candidate_move == "1/2-1/2")) ||
        (candidate_move[0] == '0' && candidate_move == "0-1");
}

void read_annotation(const std::string &buf, int start_pos, int end_pos, std::string &eval, std::string &clock)
{
    while (start_pos < end_pos) {
        int open_bracket = buf.find('[', start_pos);
        if (open_bracket == std::string::npos || open_bracket >= end_pos) {
            break;
        }
        int space = buf.find(' ', open_bracket);
        if (space == std::string::npos) {
            break;
        }
        int close_bracket = buf.find(']', space);
        if (close_bracket == std::string::npos) {
            break;
        }
//        std::string key = buf.substr(open_bracket + 1, space - open_bracket - 1);
        if (buf.compare(open_bracket + 1, space - open_bracket - 1, "%clk") == 0) {
            std::string val = buf.substr(space + 1, close_bracket - space - 1);
            clock = val;
        }
        else if (buf.compare(open_bracket + 1, space - open_bracket - 1, "%eval") == 0) {
            std::string val = buf.substr(space + 1, close_bracket - space - 1);
            eval = val;
        }
        start_pos = close_bracket;
    }
}

void read_pgn(pgn_input_stream *input, std::map<std::string, std::string> &metadata, std::vector<std::pair<move_annot, move_annot> > &movelist, bool want_metadata)
{
    std::string line;
    int pos = 0;
    bool has_metadata = false;
    while (input->is_readable()) {
        std::string buf;
        input->read_line(buf);
        line.append(buf);
        while (line[0] == '\xef' || line[0] == '\xbb' || line[0] == '\xbf') {
            // skip unicode bom
            line = line.substr(1);
        }
        if (line[line.length() - 1] == '\r') {
            line = line.substr(0, line.length() - 1);
        }
        if (line[0] == '[') {
            if (!movelist.empty()) {
                break;
            }
            if (want_metadata) {
                int spacepos = line.find(' ');
                int lastquote = line.rfind('"');
                metadata[line.substr(1, spacepos-1)] = line.substr(spacepos+2, lastquote-spacepos-2);
            }
            has_metadata = true;
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
    if (!has_metadata && line.size() > 2) {
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
                return;
            }
        }
        // check for restatement of move number, eg. "1..." but don't get tripped by 1/2-1/2
        int move_pos = pos;
        bool found_dot = false;
        bool done = false;
        while (!done) {
            char c = line[move_pos];
            switch(c) {
                case '.':
                    found_dot = true;
                    /* fall through */
                case ' ': case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                    move_pos++;
                    break;
                default:
                    done = true;
                    break;
            }
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
                    return;
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
