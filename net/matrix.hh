#ifndef _MATRIX_H_
#define _MATRIX_H_

#include <cassert>
#include <cstdint>
#include <string.h>
#include <iostream>
#include <math.h>
#ifdef __SSSE3__
#include <x86intrin.h>
#endif

#define MAX_RELU 64


template<int n, typename ntype>
struct mvector {
    mvector(const ntype *data) {
        memcpy(this->data, data, n * sizeof(ntype));
    }
    // for YMM align
    alignas(32) ntype data[n];
};


// unity = N means that our units are in 1/N terms, ie N = 1
template<int m, int n, typename ntype, int unity>
struct matrix {
    matrix() {
    }
    matrix(const ntype data[m][n]) {
        memcpy(this->data, data, m * n * sizeof(ntype));
    }
    // for YMM align
    alignas(32) ntype data[m][n];


    void matrix_softmax() {
        // compute softmax assuming matrix is integers representing 1/64ths
        // return value in units of .0001% probability
        int i, k;

        for (i = 0; i < m; i++) {
            int rowmax = this->data[i][0];
            for (k = 1; k < n; k++) {
                if (this->data[i][k] > rowmax) {
                    rowmax = this->data[i][k];
                }
            }
            double exp_x[n];
            double sum_exp = 0;
            for (k = 0; k < n; k++) {
                exp_x[k] = exp((static_cast<int>(this->data[i][k]) - rowmax)*1.0/unity);
                sum_exp += exp_x[k];
            }
            for (k = 0; k < n; k++) {
                this->data[i][k] = 10000 * exp_x[k] / sum_exp;
            }
        }
    }


    template<typename otype>
    void matrix_relu(matrix<m, n, otype, unity> &out) const {
        // clipped relu 0-1
        int i;

        for (i = 0; i < m; i++) {

            int k = 0;
#ifdef __SSSE3__
            if (n % 32 == 0) {
                matrix_relu_n(&this->data[i][k], &out.data[i][k], n);
            }
#else
            for (; k < n; k++) {
                if (this->data[i][k] < 0) {
                    out.data[i][k] = 0;
                } else if (this->data[i][k] > unity) {
                    out.data[i][k] = unity;
                } else {
                    out.data[i][k] = this->data[i][k];
                }
            }
#endif
        }
    }

#ifdef __SSSE3__
    static void matrix_relu_n(const int8_t *input, uint8_t *output /* YMM_ALIGN */, int len) {
        // clipped relu 0-1

       __m256i minval, r1, maxval;
       __m256i* a = (__m256i*) input;
       __m256i* b = (__m256i*) output;

       maxval = _mm256_broadcastb_epi8(_mm_set_epi64x(0, unity));
       minval = _mm256_setzero_si256();

       for (int i = 0; i * 32 < len; i++) {
           r1 = _mm256_min_epi8(maxval, a[i]);
           b[i] = _mm256_max_epi8(minval, r1);
       }
    }


    static void matrix_relu_n(const int16_t *input, uint8_t *output /* YMM_ALIGN */, int len) {
        // clipped relu 0-1

        __m256i minval, r1, r2, r3, maxval;
        __m256i* a = (__m256i*) input;
        __m128i* b = (__m128i*) output;

        maxval = _mm256_broadcastw_epi16(_mm_set_epi64x(0, unity));
        minval = _mm256_setzero_si256();

        for (int i = 0; i * 16 < len; i++) {
            r1 = _mm256_min_epi16(maxval, a[i]);
            r2 = _mm256_max_epi16(minval, r1);
            r3 = _mm256_packus_epi16(r2, minval); // should give r2-low, 0, r2-high, 0
            __m128i high =  _mm256_extracti128_si256(r3, 1);
            __m128i lo = _mm256_castsi256_si128(r3);
            b[i] = _mm_unpacklo_epi64(lo, high);
       }
    }

#endif


    template<typename btype>
    int dot_product_n(const ntype *a, const btype *b) {
        int sum = 0;
        int k = 0;
#ifdef __SSSE3__
        if (n % 512 == 0) {
            for (; k + 512 <= n; k += 512) {
                sum += dotProduct_512(&a[k], &b[k]);
            }
        }
        else if (n % 32 == 0) {
            for (; k + 32 <= n; k += 32) {
                sum += dotProduct_32(&a[k], &b[k]);
            }
        } else
#endif
        {
            for (; k < n; k++) {
                sum += a[k] * b[k];
            }
        }
        return sum;
    }


    template<typename btype, int p, typename ctype, typename otype>
    void matrix_multiply_add_div(const matrix<p, n, btype, unity> &bT, const mvector<p, ctype> &c, matrix<m, p, otype, unity> &out) {
        // out = a * b + c
        int i, j;

        for (i = 0; i < m; i++) {
            for (j = 0; j < p; j++) {
                int sum = dot_product_n(this->data[i], bT.data[j]);
                sum /= unity;
                sum += c.data[j];
                out.data[i][j] = sum;
            }
        }
    }


    template<int p, typename ctype, typename otype>
    void matrix_multiply_add_div_relu(const matrix<p, n, int8_t, unity> &bT, const mvector<p, ctype> &c, matrix<m, p, otype, unity> &out) {
        // clipped relu 0-1
        int i, j;

        for (i = 0; i < m; i++) {
            int16_t sums[j];
            for (j = 0; j < p; j++) {
                int sum = dot_product_n(this->data[i], bT.data[j]);
                sums[j] = sum / unity + c.data[j];
            }
            matrix_relu_n(sums, out.data[i], p);
        }
    }

    typedef __m256i (*mm256_binop)(__m256i, __m256i);
    template <mm256_binop operation>
    static void vector_addsub(int16_t *src_dest, const int8_t *addends, int size) {
        int i = 0;
        // widen addends to 16 bits
        __m256i* a = (__m256i*) addends;
        __m256i* d = (__m256i*) src_dest;
        for (i = 0; i + 32 <= size / 32; i++) {
            __m256i addends16_lo = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(a[i]));
            __m256i addends16_hi = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(a[i], 1));


            d[i * 2] = operation(d[i * 2], addends16_lo);
            d[i * 2 + 1] = operation(d[i * 2 + 1], addends16_hi);
        }
    }

    static void vector_add(int16_t *src_dest, const int8_t *addends, int size) {
        vector_addsub<_mm256_add_epi32>(src_dest, addends, size);
        assert(size % 32 == 0);
    }

    static void vector_sub(int16_t *src_dest, const int8_t *addends, int size) {
        vector_addsub<_mm256_sub_epi32>(src_dest, addends, size);
        assert(size % 32 == 0);
    }
    /*
    static void vector_add(int16_t *src_dest, const int8_t *addends, int size) {
        int i = 0;
        // widen addends to 16 bits
        __m256i* a = (__m256i*) addends;
        __m256i* d = (__m256i*) src_dest;
        for (i = 0; i + 32 <= size / 32; i++) {
            __m256i addends16_lo = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(a[i]));
            __m256i addends16_hi = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(a[i], 1));


            d[i * 2] = _mm256_add_epi32(d[i * 2], addends16_lo);
            d[i * 2 + 1] = _mm256_add_epi32(d[i * 2 + 1], addends16_hi);
        }
        i = i * 32;
        for (; i < size; i++) {
            src_dest[i] += addends[i];
        }
    }

    static void vector_sub(int16_t *src_dest, const int8_t *addends, int size) {
        int i = 0;
        // widen addends to 16 bits
        __m256i* a = (__m256i*) addends;
        __m256i* d = (__m256i*) src_dest;
        for (int i = 0; i + 32 <= size / 32; i++) {
            __m256i addends16_lo = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(a[i]));
            __m256i addends16_hi = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(a[i], 1));


            d[i * 2] = _mm256_sub_epi32(d[i * 2], addends16_lo);
            d[i * 2 + 1] = _mm256_sub_epi32(d[i * 2 + 1], addends16_hi);
        }

        i = i * 32;
        for (; i < size; i++) {
            src_dest[i] -= addends[i];
        }
    }
    */

#ifdef __SSSE3__

    // from https://stackoverflow.com/questions/60108658/fastest-method-to-calculate-sum-of-all-packed-32-bit-integers-using-avx512-or-av

    static uint32_t hsum_epi32_avx(__m128i x)
    {
        __m128i hi64  = _mm_unpackhi_epi64(x, x);           // 3-operand non-destructive AVX lets us save a byte without needing a movdqa
        __m128i sum64 = _mm_add_epi32(hi64, x);
        __m128i hi32  = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));    // Swap the low two elements
        __m128i sum32 = _mm_add_epi32(sum64, hi32);
        return _mm_cvtsi128_si32(sum32);       // movd
    }

    // only needs AVX2
    static uint32_t hsum_8x32(__m256i v)
    {
        __m128i sum128 = _mm_add_epi32(
                     _mm256_castsi256_si128(v),
                     _mm256_extracti128_si256(v, 1)); // silly GCC uses a longer AXV512VL instruction if AVX512 is enabled :/
        return hsum_epi32_avx(sum128);
    }

    static int sum_words_epi16(__m256i x) {
        // widen to 32 bits
        __m256i x32 = _mm256_madd_epi16(x, _mm256_set1_epi16(1));

        int dotProduct = hsum_8x32(x32);
        return dotProduct;
    }

    static int dotProduct_32(const unsigned char features[], const int8_t * weights) {
        // reads 32 bytes
       __m256i r0;
       __m256i* a = (__m256i*) features;
       __m256i* b = (__m256i*) weights;
       r0 = _mm256_maddubs_epi16 (a[0], b[0]);

       return sum_words_epi16(r0);
    }
    static int dotProduct_512(const unsigned char features[], const int8_t * weights) {
        // reads 32 bytes
       __m256i r0, r1, sum_epi16, sum_epi32 = _mm256_setzero_si256();
       __m256i* a = (__m256i*) features;
       __m256i* b = (__m256i*) weights;
       for (int i = 0; i < 16; i += 2) {
           r0 = _mm256_maddubs_epi16 (a[i], b[i]);
           r1 = _mm256_maddubs_epi16 (a[i+1], b[i+1]);
           sum_epi16 = _mm256_add_epi16(r0, r1);
           sum_epi32 = _mm256_add_epi32(sum_epi32, _mm256_madd_epi16(sum_epi16, _mm256_set1_epi16(1)));
       }

       return hsum_8x32(sum_epi32);
    }
#endif

};




#endif