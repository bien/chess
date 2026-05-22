#include "move.hh"
#include <cstdint>
#include <cstring>

struct TTEntry {
    uint64_t hash;
    uint64_t value;
};

class TranspositionTable {
public:
    TranspositionTable(int size_log2)
        : transposition_conflicts(0), transposition_table_size_log2(size_log2)
    {
        transposition_table = new TTEntry[1ULL << transposition_table_size_log2];
    }
    ~TranspositionTable() {
        delete[] transposition_table;
    }
    void reset() {
        std::memset(transposition_table, 0, sizeof(TTEntry) * (1ULL << transposition_table_size_log2));
    }
    uint64_t transposition_conflicts;

    bool fetch_tt_entry(uint64_t hash, move_t &move, int16_t &value, unsigned char &depth, unsigned char &type) const {
        uint64_t storage = tt_entry(hash);
        if (storage == 0) {
            return false;
        }
        type = storage & 0x3;
        if (type <= 0) {
            return false;
        }
        depth = (storage >> 2) & 0x1f;
        value = (storage >> 16) & 0xffff;
        move = (storage >> 32);

        return true;
    }
    void insert_tt_entry(uint64_t hash, move_t move, int16_t value, unsigned char depth, unsigned char type) {
        move_t old_move;
        int16_t old_value;
        unsigned char old_depth, old_type;

        // don't overwrite deeper value already present
        if (fetch_tt_entry(hash, old_move, old_value, old_depth, old_type) && old_depth > depth) {
            return;
        }

        uint64_t storage = (static_cast<uint64_t>(move) << 32) | (0xffff0000ULL & (static_cast<int16_t>(value) << 16));
        storage |= (depth & 0x1f) << 2;
        storage |= type & 0x3;
        set_tt_entry(hash, storage);
    }
private:
    void set_tt_entry(uint64_t hash, uint64_t value) {
        int table_size = (1ULL << transposition_table_size_log2);
        int index = hash & (table_size-1);
        for (int i = 0; i < 4; i++) {
            TTEntry &entry = transposition_table[(index + (1<<i)-1) % table_size];
            if (entry.hash == hash || entry.hash == 0) {
                entry.hash = hash;
                entry.value = value;
            }
        }
        TTEntry &entry = transposition_table[index];
        entry.hash = hash;
        entry.value = value;
    }

    uint64_t tt_entry(uint64_t hash) const {
        int table_size = (1ULL << transposition_table_size_log2);
        int index = hash & (table_size-1);
        for (int i = 0; i < 4; i++) {
            TTEntry &entry = transposition_table[(index + (1<<i)-1) % table_size];
            if (entry.hash == hash) {
                return entry.value;
            }
        }
        return 0;
    }

    TTEntry *transposition_table;
    int transposition_table_size_log2;
};