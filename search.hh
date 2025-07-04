#ifndef SEARCH_HH_
#define SEARCH_HH_

#include <cstring>
#include "fenboard.hh"
#include <tuple>
#include <utility>

const int MATE = 20000;
const int VERY_GOOD = 10000;
const int VERY_BAD = -VERY_GOOD;
const int SCORE_MAX = VERY_GOOD + 1000;
const int SCORE_MIN = VERY_BAD - 1000;

extern int search_debug;
const int TranspositionTableSize = 1000000001;

class Evaluation {
public:
    virtual int evaluate(const Fenboard &) = 0;
    virtual int delta_evaluate(Fenboard &b, move_t move, int previous_score) = 0;
    virtual bool endgame(const Fenboard &b, int &eval) const = 0;
    virtual ~Evaluation() {}
};


const int TT_EXACT = 3;
const int TT_UPPER = 1;
const int TT_LOWER = 2;

struct SearchUpdate {
    virtual void operator()(move_t best_move, int depth, int nodecount, int score) const {}
};

const SearchUpdate NullSearchUpdate;

struct MoveSorter {
    MoveSorter();
    MoveSorter(const Fenboard *b, Color side_to_play, bool do_sort=true, move_t hint=0) {
        reset(b, side_to_play, do_sort, hint);
    }
    bool has_more_moves();
    bool next_gives_check() const;
    move_t next_move();
    void reset(const Fenboard *b, Color side_to_play, bool do_sort=true, move_t hint=0);

    bool operator()(move_t a, move_t b) const;

    int get_score(move_t) const;
    unsigned int index;
    unsigned int last_check;
    const Fenboard *b;
    std::vector<move_t> buffer;
    PackedMoveIterator move_iter;
    int phase;
    Color side_to_play;
    bool do_sort;
    move_t hint;
};

struct Search {
    Search(Evaluation *eval, int transposition_table_size=1000 * 500 + 1);
    move_t minimax(Fenboard &b, Color color);
    move_t alphabeta(Fenboard &b, Color color, const SearchUpdate &s = NullSearchUpdate);

    void reset();

    int score;
    int nodecount;
    // hash -> depth, (max, min)
    int transposition_table_size;
    bool use_transposition_table;

    bool use_pruning;
    Evaluation *eval;
    int min_score_prune_sorting;
    bool use_mtdf;
    bool use_iterative_deepening;
    bool use_quiescent_search;
    int quiescent_depth;
    bool use_killer_move;

    int time_available;
    int max_depth;
    bool soft_deadline;

    int transposition_checks;
    int transposition_partial_hits;
    int transposition_full_hits;
    int transposition_insufficient_depth;

private:
    uint64_t *transposition_table;
    std::tuple<move_t, move_t, int> alphabeta_with_memory(Fenboard &b, int depth, Color color, int alpha, int beta, move_t hint=0);
    move_t mtdf(Fenboard &b, Color color, int &score, int guess, time_t deadline=0, move_t hint=0);
    move_t timed_iterative_deepening(Fenboard &b, Color color, const SearchUpdate &s);

    bool fetch_tt_entry(uint64_t hash, move_t &move, int16_t &value, unsigned char &depth, unsigned char &type) {
        uint64_t storage = transposition_table[hash % transposition_table_size];
        type = storage & 0x3;
        depth = (storage >> 2) & 0x1f;
        value = (storage >> 16) & 0xffff;
        move = (storage >> 32);
        unsigned short checksum = hash & 0xff00;
        return type > 0 && checksum == (storage & 0xff00);
    }

    void insert_tt_entry(uint64_t hash, move_t move, int16_t value, unsigned char depth, unsigned char type) {
        uint64_t storage = (static_cast<uint64_t>(move) << 32) | (0xffff0000ULL & (static_cast<int16_t>(value) << 16));
        storage |= (depth & 0x1f) << 2;
        storage |= type & 0x3;
        unsigned short checksum = hash & 0xff00;
        storage |= checksum;
        transposition_table[hash % transposition_table_size] = storage;
    }

    std::vector<MoveSorter *> move_sorter_pool;
    MoveSorter *get_move_sorter(unsigned int depth) {
        if (move_sorter_pool.empty()) {
            return new MoveSorter();
        }
        MoveSorter *ms = move_sorter_pool.back();
        move_sorter_pool.pop_back();
        return ms;
    }
    void release_move_sorter(MoveSorter *ms) {
        move_sorter_pool.push_back(ms);
    }
};

#endif
