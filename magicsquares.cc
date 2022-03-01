#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <vector>
#include "bitboard.hh"


void find_magic(int iterations, int start_pos, uint64_t &factor, uint64_t &addend, bool is_diag) {
    uint64_t block_board;
    if (is_diag) {
        block_board = BitArrays::bishop_blockboard::data[start_pos];
    } else {
        block_board = BitArrays::rook_blockboard::data[start_pos];
    }
    int array_size_nbits = count_bits(block_board) + 1;

    uint64_t data[1ULL << array_size_nbits];
    uint64_t v, b = 0;
    int iter;
    bool failed = false;
    uint64_t projections_memo[1ULL << (array_size_nbits - 1)];
    uint64_t resolutions_memo[1ULL << (array_size_nbits - 1)];

    for (int i = 0; i < (1ULL << (array_size_nbits - 1)); i++) {
        projections_memo[i] = project_bitset(i, block_board);
        if (is_diag) {
            resolutions_memo[i] = bishop_slide_moves(start_pos, projections_memo[i]);
        } else {
            resolutions_memo[i] = rook_slide_moves(start_pos, projections_memo[i]);
        }
    }

    for (iter = 0; iter < iterations; iter++) {
        v = (static_cast<uint64_t>(random()) << 32) | random();
        v |= 1;
        failed = false;

        memset(data, 0, sizeof(data));
        for (int i = 0; i < (1ULL << (array_size_nbits - 1)); i++) {
            uint64_t projection = projections_memo[i];
            uint64_t hash_index = (projection * v + b)  >> (64 - array_size_nbits);
            uint64_t resolved = resolutions_memo[i];
            if (data[hash_index] != 0 && data[hash_index] != resolved) {

                failed = true;
                break;
            }
            data[hash_index] = resolved;
        }
        if (!failed) {
            printf("Found v=%llx; b=%llx after %d iter\n", v, b, iter);
            factor = v;
            addend = b;
            break;
        }
        if (iter % 100000 == 0) {
            printf(".");
        }
    }
    if (failed) {
        printf("Failed after %d iter\n", iter);
        factor = 0;
        addend = 0;
    }
}

int main(int argc, char **argv) {
    setbuf(stdout, NULL);

    int iterations = 10*1000*1000;
    if (argc > 1) {
        iterations = atoi(argv[1]);
    }
    uint64_t lat_addends[64];
    uint64_t lat_factors[64];
    uint64_t diag_addends[64];
    uint64_t diag_factors[64];
    for (int i = 0; i < 64; i++) {
        find_magic(iterations, i, lat_factors[i], lat_addends[i], false);
        printf(".");
        find_magic(iterations, i, diag_factors[i], diag_addends[i], true);
        printf(".");
    }
    printf("\n");

    printf("constexpr uint64_t lateral_slide_magic_factor[] = {\n");
    for (int i = 0; i < 8; i++) {
        printf("    ");
        for (int j = 0; j < 8; j++) {
            printf("0x%llx, ", lat_factors[i * 8 + j]);
        }
        printf("\n");
    }
    printf("};\n");
    printf("\n");
    printf("constexpr uint64_t diag_slide_magic_factor[] = {\n");
    for (int i = 0; i < 8; i++) {
        printf("    ");
        for (int j = 0; j < 8; j++) {
            printf("0x%llx, ", diag_factors[i * 8 + j]);
        }
        printf("\n");
    }
    printf("};\n");

    return 0;
}
