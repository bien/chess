#ifndef SEARCH_HH_
#define SEARCH_HH_

#include "fenboard.hh"
#include <tuple>
#include <utility>

const int MATE = 20000;
const int VERY_GOOD = 10000;
const int VERY_BAD = -VERY_GOOD;
const int SCORE_MAX = VERY_GOOD + 1000;
const int SCORE_MIN = VERY_BAD - 1000;

extern const int search_debug;
const int TranspositionTableSize = 1000001;

class Evaluation {
public:
    virtual int evaluate(const Fenboard &) const = 0;
    virtual int delta_evaluate(Fenboard &b, move_t move, int previous_score) const;
};

template <typename Key, typename Value, template <int Size> class Hasher, int Size>
struct SimpleHash {
    struct KeyPair {
        Key first;
        Value second;
    };

    SimpleHash() {
        contents = new KeyPair[Size];
        clear();
    }

    ~SimpleHash() {
        delete [] contents;
    }
    void clear() {
        memset(valid, 0, sizeof(valid));
    }
    Value &operator[](Key key) {
        int hashed = Hasher<Size>::hash(key);
        contents[hashed].first = key;
        valid[hashed / 64] |= 1ULL << (hashed % 64);
        return contents[hashed].second;
    }
    std::pair<Key, Value> find(Key key) {
        int hashed = Hasher<Size>::hash(key);
        if (contents[hashed].first == key && ((valid[hashed / 64] & (1ULL << (hashed % 64))) != 0)) {
            return std::pair<Key, Value>(key, contents[hashed].second);
        } else {
            return end();
        }
    }
    std::pair<Key, Value> end() const {
        return std::pair<Key, Value>(Key(), Value());
    }
    KeyPair *contents;
    uint64_t valid[(Size + 63) / 64];
};

// use trivialhash because the input is already hashed
template <int Size>
struct TrivialHash {
    static constexpr unsigned int hash(uint64_t key)
    {
      return key % Size;
    }
};

struct TranspositionEntry {
    short depth;
    int lower;
    int upper;
    move_t move;

    TranspositionEntry()
        : depth(0), lower(SCORE_MIN), upper(SCORE_MAX), move(0)
    {}

    TranspositionEntry(const TranspositionEntry &entry)
        : depth(entry.depth), lower(entry.lower), upper(entry.upper), move(entry.move)
    {}

    bool operator==(const TranspositionEntry o) const {
        return o.move == move && o.upper == upper && o.lower == lower && o.depth == depth;
    }
};

struct SearchUpdate {
    virtual void operator()(move_t best_move, int depth, int nodecount, int score) const {}
};

const SearchUpdate NullSearchUpdate;

struct Search {
    Search(const Evaluation *eval, int transposition_table_size=1000 * 500 + 1);
    move_t minimax(Fenboard &b, Color color);
    move_t alphabeta(Fenboard &b, Color color, const SearchUpdate &s = NullSearchUpdate);

    void reset();

    int score;
    int nodecount;
    // hash -> depth, (max, min)
    int transposition_table_size;
    bool use_transposition_table;

    bool use_pruning;
    const Evaluation *eval;
    int min_score_prune_sorting;
    bool use_mtdf;
    bool use_iterative_deepening;
    bool use_quiescent_search;
    int quiescent_depth;

    int max_depth;

private:
    SimpleHash<uint64_t, TranspositionEntry, TrivialHash, TranspositionTableSize> transposition_table;
    std::pair<move_t, int> alphabeta_with_memory(Fenboard &b, int depth, Color color, int alpha, int beta);
    move_t mtdf(Fenboard &b, Color color, int &score, int guess);
};

#endif
