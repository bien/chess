#ifndef FEN_BOARD_HH_
#define FEN_BOARD_HH_

#include <string>
#include <ostream>
#include <vector>
#include "logicalboard.hh"

class Fenboard : public Logicalboard
{
public:
    Fenboard();
    virtual ~Fenboard() {}

    void set_starting_position();
    void reset();
    void get_fen(std::ostream &os) const;
    void set_fen(const std::string &fen);
    void print_move(move_t, std::ostream &os) const;
    move_t read_move(const std::string &s, Color color) const;
    move_t reinterpret_move(move_t original) const;
    Color get_side_to_play() const { return side_to_play; }
    short get_move_count() const { return move_count; }

private:
    char fen_repr(unsigned char piece) const;
    void fen_flush(std::ostream &os, int &empty) const;
    void fen_handle_space(unsigned char piece, std::ostream &os, int &empty) const;
    unsigned char read_piece(unsigned char c) const;

    move_t invalid_move(const std::string &s) const;
};

std::ostream &print_move_uci(move_t, std::ostream &os);
std::ostream &operator<<(std::ostream &os, const Fenboard &b);

#endif
