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

struct pgn_input_stream {
    virtual bool is_readable() const = 0;
    virtual void read_line(std::string &line) = 0;
    virtual ~pgn_input_stream() {}
};

struct pgn_istream : pgn_input_stream {
    pgn_istream(std::istream &input) {
        this->input = &input;
    }
    bool is_readable() const {
        return !(input->eof() || input->bad());
    }
    void read_line(std::string &line) {
        std::getline(*input, line, '\n');
    }

    std::istream *input;
};

typedef std::vector<std::vector<move_annot> > movelist_tree;

void read_pgn_options(pgn_input_stream *input, std::map<std::string, std::string> &metadata, movelist_tree &movelist);
void read_pgn(pgn_input_stream *input, std::map<std::string, std::string> &metadata, std::vector<std::pair<move_annot, move_annot> > &movelist, bool want_metadata=true);
