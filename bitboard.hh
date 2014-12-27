#ifndef BITBOARD_H_
#define BITBOARD_H_

#include <ostream>
#include <vector>

typedef uint32_t move_t;
typedef unsigned char piece_t;
typedef unsigned char board_pos_t;

const int bitmask_types = 7;
const int ALL = 0;

class Bitboard
{
public:
	Bitboard();
	void reset();

	void legal_moves(bool is_white, std::vector<move_t> &moves) const;
	bool king_in_check(Color) const;

	void apply_move(move_t);
	void undo_move(move_t);
	
	piece_t get_piece(unsigned char rank, unsigned char file) const;
	void set_piece(unsigned char rank, unsigned char file, piece_t);
	void update(); // called whenever the board is updated
	void get_source(move_t move, unsigned char &rank, unsigned char &file) const;
	void get_dest(move_t move, unsigned char &rank, unsigned char &file) const;
	unsigned char get_promotion(move_t move) const;

	move_t make_move(unsigned char srcrank, unsigned char srcfile, unsigned char source_piece, unsigned char destrank, unsigned char destfile, unsigned char captured_piece, unsigned char promote) const;
	
protected:
	Color side_to_play;
	char enpassant_file;
	short move_count;
	char castle;
	int get_castle_bit(Color color, bool kingside) const;
	bool can_castle(Color color, bool kingside) const;

private:
	void get_point_moves(uint64_t piece_bitmask, uint64_t legal_dest, const uint64_t *piece_moves, std::vector<move_t> &moves) const;
	move_t make_move(board_pos_t src, board_pos_t dest) const;
	uint64_t compute_slide_dest(uint64_t raw, uint64_t legal, uint64_t stop_slide, int origin, bool stop_at_first_positive) const;
	void read_bitmask_moves(int src, uint64_t dest_bitmask, std::vector<move_t> &moves) const;
	void get_slide_moves(uint64_t piece_bitmask, uint64_t empty, uint64_t capture_targets, uint64_t blockers, const uint64_t *piece_moves, std::vector<move_t> &moves) const;
	
	bool in_check;
	
	unsigned char charboard[32];
	uint64_t piece_bitmasks[15];
};

std::ostream &operator<<(std::ostream &os, const Bitboard &b);

#endif
