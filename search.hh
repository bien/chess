#ifndef SEARCH_HH_
#define SEARCH_HH_

#include <iostream>
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

typedef struct LINE {
    int cmove;              // Number of moves in the line.
    move_t argmove[64];  // The line.
}   LINE;



const int TT_EXACT = 3;
const int TT_UPPER = 1;
const int TT_LOWER = 2;

struct SearchUpdate {
    virtual void operator()(move_t best_move, int depth, int nodecount, int score) const {}
};

const SearchUpdate NullSearchUpdate;
struct Search;
const int NTH_SORT_FREQ_BUCKETS = 40;

struct MoveSorter {
    MoveSorter();
    bool has_more_moves();
    bool next_gives_check() const;
    bool next_gives_check_or_capture() const {
        return index <= last_capture;
    }
    move_t next_move();
    void reset(const Fenboard *b, Search *s, Color side_to_play, int depth=0, bool do_sort=true, int score=0, move_t hint=0, bool verbose=false);

    bool operator()(move_t a, move_t b) const;

    int get_score(const Fenboard *b, int current_score,  move_t) const;
    int index;
    int last_check;
    int last_capture;
    std::vector<move_t> buffer;
    PackedMoveIterator move_iter;
    Color side_to_play;
    bool do_sort;
    move_t hint;
    Search *s;
    int current_score;
    bool verbose;
    mutable uint64_t covered_squares = 0;
};

struct Search {
    Search(Evaluation *eval, int transposition_table_size=10000 * 5000 + 1);
    move_t minimax(Fenboard &b, Color color);
    move_t alphabeta(Fenboard &b, const SearchUpdate &s = NullSearchUpdate);
    // int principal_variation(Fenboard &b, int depth, int alpha, int beta, move_t &best_move, move_t hint=0);

    void reset();

    int score;
    uint64_t nodecount;
    uint64_t qnodecount;
    // hash -> depth, (max, min)
    int transposition_table_size;
    bool use_transposition_table;

    bool use_pruning;
    Evaluation *eval;
    int min_score_prune_sorting;
    bool use_mtdf;
    bool use_pv;
    bool use_iterative_deepening;
    bool use_quiescent_search;
    bool use_killer_move;
    int mtdf_window_size;

    int time_available;
    int max_depth;
    bool soft_deadline;

    bool use_nega;

    int transposition_checks;
    int transposition_partial_hits;
    int transposition_full_hits;
    int transposition_insufficient_depth;

    int nth_sort_freq[NTH_SORT_FREQ_BUCKETS];

private:
    uint64_t *transposition_table;
    // std::tuple<move_t, move_t, int> alphabeta_with_memory(Fenboard &b, int depth, Color color, int alpha, int beta, move_t hint=0);
    public:
    std::tuple<move_t, move_t, int> negamax_with_memory(Fenboard &b, int depth, int alpha, int beta, move_t hint=0, int static_score=0, const std::string &line="");

public:
    move_t mtdf(Fenboard &b, int &score, int guess, time_t deadline=0, move_t hint=0);
    move_t timed_iterative_deepening(Fenboard &b, const SearchUpdate &s);
    bool read_transposition(uint64_t board_hash, move_t &move, int depth, int &alpha, int &beta, int &exact_value);
private:
    void write_transposition(uint64_t board_hash, move_t move, int best_score, int depth, int original_alpha, int original_beta);
    bool fetch_tt_entry(uint64_t hash, move_t &move, int16_t &value, unsigned char &depth, unsigned char &type) const {
        uint64_t storage = transposition_table[hash % transposition_table_size];
        unsigned short checksum = hash & 0xff00;
        if (type <= 0 || checksum != (storage & 0xff00)) {
            return false;
        }
        type = storage & 0x3;
        depth = (storage >> 2) & 0x1f;
        value = (storage >> 16) & 0xffff;
        move = (storage >> 32);
        return true;
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
public:
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
private:
    std::ostream &print_debug_move_header(Color side_to_play, int depth_so_far, move_t move) const {
        for (int i = 0; i < depth_so_far * 2; i++) {
            std::cout << " ";
        }
        std::cout << ((depth_so_far+1) / 2 + 1) << ".";
        if (side_to_play == Black) {
            std::cout << "..";
        }
        return print_move_uci(move, std::cout);

    };
    friend struct MoveSorter;
};

template <class T>
struct Acquisition {
public:
    Acquisition(Search *search) : search(search) {
        access = search->get_move_sorter(0);
    }
    T *access;
    ~Acquisition() {
        search->release_move_sorter(access);
    }
    T *operator->() {
        return access;
    }
private:
    Search *search;
};

#endif
