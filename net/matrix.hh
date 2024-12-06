#ifndef _MATRIX_H_
#define _MATRIX_H_

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
                this->data[i][k] = 1000000 * exp_x[k] / sum_exp;
            }
        }
    }


    template<typename otype>
    void matrix_relu(matrix<m, n, otype, unity> &out) const {
        // clipped relu 0-1
        int i, k;

        for (i = 0; i < m; i++) {
            for (k = 0; k < n; k++) {
                if (this->data[i][k] < 0) {
                    out.data[i][k] = 0;
                } else if (this->data[i][k] > unity) {
                    out.data[i][k] = unity;
                } else {
                    out.data[i][k] = this->data[i][k];
                }
            }
        }
    }



    template<typename btype>
    int dot_product_n(const ntype *a, const btype *b) {
        int sum = 0;
        int k = 0;
#ifdef __SSSE3__
        if (n % 512 == 0) {
            for (; k + 512 <= n; k += 512) {
                sum += dotProduct_512(&a[k], &b[k]);
            }
        } else if (n % 32 == 0) {
            for (; k + 32 <= n; k += 32) {
                sum += dotProduct_32(&a[k], &b[k]);
            }
        }
#endif
        for (; k < n; k++) {
            sum += a[k] * b[k];
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
            for (j = 0; j < p; j++) {
                int sum = dot_product_n(this->data[i], bT.data[j]);
                sum /= unity;
                sum += c.data[j];
                if (sum < 0) {
                    sum = 0;
                }
                else if (sum > unity) {
                    sum = unity;
                }
                out.data[i][j] = sum;
            }
        }
    }

#ifdef __SSSE3__
static int16_t dotProduct_32(const unsigned char features[], const int8_t * weights /* XMM_ALIGN */) {
    // reads 32 bytes
   __m256i r0;
   __m256i* a = (__m256i*) features;
   __m256i* b = (__m256i*) weights;
   r0 = _mm256_maddubs_epi16 (a[0], b[0]);

   r0 = _mm256_hadds_epi16   (r0, r0); // 8 shorts
   r0 = _mm256_hadds_epi16   (r0, r0); // 4 shorts
   r0 = _mm256_hadds_epi16   (r0, r0); // 2 shorts
   r0 = _mm256_hadds_epi16   (r0, r0); // 1 final short
   return _mm_extract_epi16(_mm256_castsi256_si128(r0), 0);
}

static int16_t dotProduct_512(const unsigned char features[], const int8_t * weights /* XMM_ALIGN */) {
    // reads 256 bytes
   __m256i r0, r1, r2, r3, r4, r5, r6, r7, r8;
   __m256i* a = (__m256i*) features;
   __m256i* b = (__m256i*) weights;

   for (int i = 0; i < 2; i++) {
       r0 = _mm256_maddubs_epi16 (a[0], b[0]);
       r1 = _mm256_maddubs_epi16 (a[1], b[1]);
       r2 = _mm256_maddubs_epi16 (a[2], b[2]);
       r3 = _mm256_maddubs_epi16 (a[3], b[3]);
       r4 = _mm256_maddubs_epi16 (a[4], b[4]);
       r5 = _mm256_maddubs_epi16 (a[5], b[5]);
       r6 = _mm256_maddubs_epi16 (a[6], b[6]);
       r7 = _mm256_maddubs_epi16 (a[7], b[7]);
       r0 = _mm256_adds_epi16    (r0, r1);
       r2 = _mm256_adds_epi16    (r2, r3);
       r4 = _mm256_adds_epi16    (r4, r5);
       r6 = _mm256_adds_epi16    (r6, r7);

       r0 = _mm256_adds_epi16    (r0, r2);
       r4 = _mm256_adds_epi16    (r4, r6);

       if (i == 0) {
           r8 = _mm256_adds_epi16    (r0, r4); // 16 shorts
           a += 8;
           b += 8;
       } else {
           r0 = _mm256_adds_epi16    (r0, r4); // 16 shorts
       }
   }

   r0 = _mm256_adds_epi16    (r0, r8); // 16 shorts
   r0 = _mm256_hadds_epi16   (r0, r0); // 8 shorts
   r0 = _mm256_hadds_epi16   (r0, r0); // 4 shorts
   r0 = _mm256_hadds_epi16   (r0, r0); // 2 shorts
   r0 = _mm256_hadds_epi16   (r0, r0); // 1 final short
   return _mm_extract_epi16(_mm256_castsi256_si128(r0), 0);
}
#endif

};




#endif