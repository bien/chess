#ifndef LOGICAL_BOARD_H_
#define LOGICAL_BOARD_H_

#include "bitboard.hh"
#include <vector>

class Logicalboard : public Bitboard {
public:
    Logicalboard();
    virtual ~Logicalboard() {}
    void apply_move(move_t);
    void undo_move(move_t);
    int times_seen() {
        int occurs = 0;
        for (auto iter = seen_positions.rbegin(); iter != seen_positions.rend(); iter++) {
            if (hash == *iter) {
                occurs++;
            }
        }
        return occurs;
    }
    uint64_t get_hash() const { return hash; }

private:
    std::vector<uint64_t> seen_positions;
    uint64_t hash;
    void update_zobrist_hashing_piece(unsigned char rank, unsigned char file, piece_t piece, bool adding);
    void update_zobrist_hashing_castle(Color, bool kingside, bool enabling);
    void update_zobrist_hashing_move();
    void update_zobrist_hashing_enpassant(int file, bool enabling);
};

#endif
