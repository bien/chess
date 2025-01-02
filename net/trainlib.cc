#include <iostream>
#include <fstream>
#include <string.h>
#include "fenboard.hh"
#include "pgn.hh"
#include <stdlib.h>
#include <random>

const int RANDOM = 0;

struct TrainingPosition {
    uint64_t piece_bitmasks[12];
    uint64_t piece_bitmasks_mirrored[12];
    int white_king_index;
    int black_king_index_mirrored;
    float cp_eval;
    float padding;
};


struct TrainingIterator {
public:
    TrainingIterator(const char *filename, int move_freq)
        : input_stream(filename), distrib(1, move_freq), gen(rd())
    {}
    TrainingIterator(const char *filename, int move_freq, unsigned int seed)
        : input_stream(filename), distrib(1, move_freq), gen(seed)
    {}
    bool read_position(TrainingPosition *tp);
private:
    bool process_game(TrainingPosition *tp);

    std::ifstream input_stream;
    std::map<std::string, std::string> game_metadata;
    std::vector<std::pair<move_annot, move_annot> > movelist;

    unsigned int ply;
    unsigned int next_ply;
    std::uniform_int_distribution<> distrib;
    Fenboard b;
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen; // mersenne_twister_engine seeded with rd()
};

extern "C" {

TrainingIterator *create_training_iterator(const char *filename, int move_freq, unsigned int seed) {
    TrainingIterator *ti;
    if (seed == 0) {
        ti = new TrainingIterator(filename, move_freq);
    } else {
        ti = new TrainingIterator(filename, move_freq, seed);
    }
    return ti;
}
bool read_position(TrainingIterator *iter, TrainingPosition *tp) {
    return iter->read_position(tp);
}

void delete_training_iterator(TrainingIterator *ti) {
    delete ti;
}

}

bool TrainingIterator::read_position(TrainingPosition *tp)
{
    while (1) {
        // game is too short or has no annotations: skip
        if (movelist.size() > 3 && !movelist[0].first.eval.empty()) {
            bool found_position = process_game(tp);
            if (found_position) {
                return true;
            }
        }

        // finished game, start new one
        game_metadata.clear();
        movelist.clear();

        if (input_stream.eof()) {
            return false;
        }
        read_pgn(input_stream, game_metadata, movelist);
        ply = 0;
        b.set_starting_position();
    }
    return false;
}

int mirror_idx_vertical(int idx) {
    return idx ^ 56;
}

uint64_t mirror_bitmap_vertical(uint64_t x) {
    return  ( (x << 56)                           ) |
            ( (x << 40) & 0x00ff000000000000ULL ) |
            ( (x << 24) & 0x0000ff0000000000ULL ) |
            ( (x <<  8) & 0x000000ff00000000ULL ) |
            ( (x >>  8) & 0x00000000ff000000ULL ) |
            ( (x >> 24) & 0x0000000000ff0000ULL ) |
            ( (x >> 40) & 0x000000000000ff00ULL ) |
            ( (x >> 56) );
}

bool TrainingIterator::process_game(TrainingPosition *tp)
{
    while (ply < movelist.size()) {
        std::string move_text, next_move_text;
        Color side_to_play = ply % 2 == 0 ? White : Black;
        if (side_to_play == Black) {
            move_text = movelist[ply / 2].second.move;
            if ((ply / 2) + 1 >= movelist.size()) {
                // this was the last move; might not be quiescent so don't use it
                return false;
            }
            next_move_text = movelist[ply / 2 + 1].first.move;
        } else {
            move_text = movelist[ply / 2].first.move;
            next_move_text = movelist[ply / 2].second.move;
            if (next_move_text.empty()) {
                // this was the last move; might not be quiescent so don't use it
                return false;
            }
        }
        move_t move = b.read_move(move_text, side_to_play);
        b.apply_move(move);
        // only consider quiescent moves as ones that aren't check nor result in capture/check
        if (ply >= next_ply && move_text.find("+") == std::string::npos && next_move_text.find_first_of("+x") == std::string::npos) {
            // exclude forced mates
            std::string score = side_to_play == White ? movelist[ply/2].first.eval : movelist[ply/2].second.eval;
            if (score.find('#') == std::string::npos) {
                tp->cp_eval = std::stof(score);
                for (int i = 0; i < 12; i++) {
                    tp->piece_bitmasks[i] = b.piece_bitmasks[i + 1 + (i >= 6 ? 1 : 0)];
                    tp->piece_bitmasks_mirrored[i >= 6 ? i - 6 : i + 6] = mirror_bitmap_vertical(b.piece_bitmasks[i + 1 + (i >= 6 ? 1 : 0)]);
                }
                tp->white_king_index = get_low_bit(b.piece_bitmasks[bb_king], 0);
                tp->black_king_index_mirrored = mirror_idx_vertical(get_low_bit(b.piece_bitmasks[bb_king + bb_king + 1], 0));
                ply++;
                return true;
            }
        }
        ply++;
    }

    return false;
}
