#include <iostream>
#include <fstream>
#include <string.h>
#include "fenboard.hh"
#include "pgn.hh"
#include <stdlib.h>
#include <random>
#include <zstd.h>

struct TrainingPosition {
    uint64_t piece_bitmasks[12];
    uint64_t piece_bitmasks_mirrored[12];
    int white_king_index;
    int black_king_index_mirrored;
    char white_k_castle;
    char white_q_castle;
    char black_k_castle;
    char black_q_castle;
    float cp_eval;
    float padding;
};

struct zstd_pgn_input : pgn_input_stream {
    zstd_pgn_input(std::ifstream *input) : input(input) {
        read_line_position = 0;
        buffer_in_length = ZSTD_DStreamInSize();
        buffer_in = (char*) malloc(buffer_in_length);
        zstd_input.src = buffer_in;
        buffer_out_length = ZSTD_DStreamOutSize();
        buffer_out = (char*) malloc(buffer_out_length);
        zstd_output.dst = buffer_out;
        dctx = ZSTD_createDCtx();
    }
    ~zstd_pgn_input() {
        ZSTD_freeDCtx(dctx);
        free(buffer_in);
        free(buffer_out);
    }
    bool is_readable() const {
        return input->good() && !input->eof();
    }
    virtual void read_line(std::string &line) {
        while (is_readable()) {
            // If we have something waiting in the buffer, use that
            // zstd_output: anything between [read_line_position, zstd_output.pos) is consumable
            if (read_line_position < zstd_output.pos) {
                char *next_newline = (char*)memchr((char*)zstd_output.dst + read_line_position, '\n', zstd_output.pos - read_line_position);
                if (next_newline != NULL) {
                    line.append((char*)zstd_output.dst + read_line_position, (next_newline - (char*)zstd_output.dst) - read_line_position);
                    read_line_position = (next_newline - (char*)zstd_output.dst) + 1;
                    break;
                } else {
                    line.append((char*)zstd_output.dst + read_line_position, zstd_output.pos - read_line_position);
                    read_line_position = zstd_output.pos;
                    // still no newline, just consume the buffer so we can continue
                }
            }
            // if there's data in the input buffer waiting to be compressed, do that now
            else if (zstd_input.pos < zstd_input.size) {
                zstd_output.size = buffer_out_length;
                zstd_output.pos = 0;
                size_t ret = ZSTD_decompressStream(dctx, &zstd_output, &zstd_input);
                if (ret < 0) {
                    std::cerr << ZSTD_getErrorName(ret) << std::endl;
                    abort();
                }
                read_line_position = 0;
            }

            // read more data into the input buffer
            else {
                input->read(buffer_in, buffer_in_length);
                zstd_input.pos = 0;
                zstd_input.size = input->gcount();
            }
        }
    }

    std::ifstream *input;
    ZSTD_inBuffer zstd_input;
    ZSTD_outBuffer zstd_output;

    char * buffer_in;
    size_t buffer_in_length;
    char * buffer_out;
    size_t buffer_out_length;
    ZSTD_DCtx* dctx;

    int read_line_position;
};


struct TrainingIterator {
public:
    TrainingIterator(const char *filename, int move_freq)
        : fs(filename), ply(0), next_ply(0), distrib(1, move_freq), gen(rd())
    {
        open_stream(filename);
    }
    TrainingIterator(const char *filename, int move_freq, unsigned int seed)
        : fs(filename), ply(0), next_ply(0), distrib(1, move_freq), gen(seed)
    {
        open_stream(filename);
    }
    ~TrainingIterator() {
        delete input_stream;
    }
    bool read_position(TrainingPosition *tp);
private:
    bool process_game(TrainingPosition *tp);
    void open_stream(const std::string &filename) {
        if (fs.fail()) {
            std::cerr << "Couldn't open " << filename <<  std::endl;
            abort();
        }
        if (filename.find(".zst") != std::string::npos) {
            input_stream = new zstd_pgn_input(&fs);
        } else {
            input_stream = new pgn_istream(fs);
        }
    }

    std::ifstream fs;
    pgn_input_stream *input_stream;
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
            next_ply = ply + distrib(gen);
            bool found_position = process_game(tp);
            if (found_position) {
                return true;
            }
        }

        // finished game, start new one
        game_metadata.clear();
        movelist.clear();

        if (!input_stream->is_readable()) {
            return false;
        }
        read_pgn(input_stream, game_metadata, movelist, false);
//        std::cout << "Read " << game_metadata["Site"] << std::endl;
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
                try {
                    tp->cp_eval = std::stof(score);
                } catch (const std::invalid_argument& ia) {
                    std::cerr << "Invalid score value: " << score << std::endl;
                    return false;
                }

                for (int i = 0; i < 12; i++) {
                    tp->piece_bitmasks[i] = b.piece_bitmasks[i + 1 + (i >= 6 ? 1 : 0)];
                    tp->piece_bitmasks_mirrored[i >= 6 ? i - 6 : i + 6] = mirror_bitmap_vertical(b.piece_bitmasks[i + 1 + (i >= 6 ? 1 : 0)]);
                }
                tp->white_king_index = get_low_bit(b.piece_bitmasks[bb_king], 0);
                tp->black_king_index_mirrored = mirror_idx_vertical(get_low_bit(b.piece_bitmasks[bb_king + bb_king + 1], 0));
                tp->white_k_castle = b.can_castle(White, true);
                tp->white_q_castle = b.can_castle(White, false);
                tp->black_k_castle = b.can_castle(Black, true);
                tp->black_q_castle = b.can_castle(Black, false);
                ply++;
                return true;
            }
        }
        ply++;
    }

    return false;
}

int main(int argc, char **argv)
{
    TrainingPosition tp;
    std::cout << sizeof(tp) << std::endl;
    if (argc > 1) {
        int count = 0;
        int maxcount = 1000;
        if (argc > 2) {
            maxcount = std::atoi(argv[2]);
        }
        TrainingIterator *ti = create_training_iterator(argv[1], 5, 0);
        TrainingPosition tp;
        bool has_more = true;
        while (has_more) {
            has_more = read_position(ti, &tp);
            count += 1;
            if (count > maxcount) {
                break;
            }
        }
        printf("Read %d positions\n", count);
    }
    return 0;
}
