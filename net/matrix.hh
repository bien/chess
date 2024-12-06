#ifndef _MATRIX_H_
#define _MATRIX_H_

#include <cstdint>
#include <string.h>
#include <iostream>
#include <math.h>
#ifdef __SSSE3__
#include <x86intrin.h>
int16_t dotProduct_256(const unsigned char features[], const int8_t * weights /* YMM_ALIGN */);
#endif

#define MAX_RELU 64

template<int m, int n, typename ntype>
struct matrix {
    matrix() {
    }
    matrix(const ntype data[m][n]) {
        memcpy(this->data, data, m * n * sizeof(ntype));
    }
    // for YMM align
    alignas(32) ntype data[m][n];
};

template<int n, typename ntype>
struct mvector {
    mvector(const ntype *data) {
        memcpy(this->data, data, n * sizeof(ntype));
    }
    // for YMM align
    alignas(32) ntype data[n];
};


template<int m, int n, typename atype>
void matrix_relu(matrix<m, n, atype> &a, int min, int max) {
    int i, k;

    for (i = 0; i < m; i++) {
        for (k = 0; k < n; k++) {
            if (a.data[i][k] < min) {
                a.data[i][k] = min;
            } else if (a.data[i][k] > max) {
                a.data[i][k] = max;
            }
        }
    }
}

template<int m, int n, typename atype>
void matrix_softmax_64ths(matrix<m, n, atype> &a) {
    // compute softmax assuming matrix is integers representing 1/64ths
    // return value in units of .0001% probability
    int i, k;

    for (i = 0; i < m; i++) {
        int rowmax = a.data[i][0];
        for (k = 1; k < n; k++) {
            if (a.data[i][k] > rowmax) {
                rowmax = a.data[i][k];
            }
        }
        double exp_x[n];
        double sum_exp = 0;
        for (k = 0; k < n; k++) {
            exp_x[k] = exp((static_cast<int>(a.data[i][k]) - rowmax)*1.0/64);
            sum_exp += exp_x[k];
        }
        for (k = 0; k < n; k++) {
            a.data[i][k] = 1000000 * exp_x[k] / sum_exp;
        }
    }
}


template<int m, int n, typename atype, typename otype>
void matrix_relu(const matrix<m, n, atype> &a, matrix<m, n, otype> &out, int max) {
    int i, k;

    for (i = 0; i < m; i++) {
        for (k = 0; k < n; k++) {
            if (a.data[i][k] < 0) {
                out.data[i][k] = 0;
            } else if (a.data[i][k] > max) {
                out.data[i][k] = max;
            } else {
                out.data[i][k] = a.data[i][k];
            }
        }
    }
}


template<int m, typename atype, int n, typename btype, int p, typename ctype, typename otype>
void matrix_multiply_add_div(const matrix<m, n, atype> &a, const matrix<p, n, btype> &bT, const mvector<p, ctype> &c, int divisor, matrix<m, p, otype> &out) {
    // out = a * b + c
    int i, j;

    for (i = 0; i < m; i++) {
        for (j = 0; j < p; j++) {
            int64_t sum = 0;
            int k = 0;
#ifdef __SSSE3__
            for (; k + 256 <= n; k += 256) {
                sum += dotProduct_256(&a.data[i][k], &bT.data[j][k]);
            }
#endif
            for (; k < n; k++) {
                sum += a.data[i][k] * bT.data[j][k];
            }
            sum /= divisor;
            sum += c.data[j];
            out.data[i][j] = sum;
        }
    }
}


template<int m, int n, int p, typename ctype, typename otype>
void matrix_multiply_add_div_relu(const matrix<m, n, uint8_t> &a, const matrix<p, n, int8_t> &bT, const mvector<p, ctype> &c, int max, matrix<m, p, otype> &out) {
    int i, j;

    for (i = 0; i < m; i++) {
        for (j = 0; j < p; j++) {
            int16_t sum = 0;
            int k = 0;
#ifdef __SSSE3__
            for (; k + 256 <= n; k += 256) {
                sum += dotProduct_256(&a.data[i][k], &bT.data[j][k]);
            }
#endif
            for (; k < n; k++) {
                sum += a.data[i][k] * bT.data[j][k];
            }
            sum /= MAX_RELU;
            sum += c.data[j];
            if (sum < 0) {
                sum = 0;
            }
            else if (sum > max) {
                sum = max;
            }
            out.data[i][j] = sum;
        }
    }
}


#endif