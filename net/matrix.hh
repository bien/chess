#ifndef _MATRIX_H_
#define _MATRIX_H_

#include <cstdint>
#include <string.h>
#include <iostream>
#include <math.h>

template<int m, int n, typename ntype>
struct matrix {
    matrix() {
    }
    matrix(const ntype data[m][n]) {
        memcpy(this->data, data, m * n * sizeof(ntype));
    }
    ntype data[m][n];
};

template<int n, typename ntype>
struct mvector {
    mvector(const ntype *data) {
        memcpy(this->data, data, n * sizeof(ntype));
    }
    ntype data[n];
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
void matrix_relu(const matrix<m, n, atype> &a, matrix<m, n, otype> &out, int min, int max) {
    int i, k;

    for (i = 0; i < m; i++) {
        for (k = 0; k < n; k++) {
            if (a.data[i][k] < min) {
                out.data[i][k] = min;
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
    int i, j, k;

    for (i = 0; i < m; i++) {
        for (j = 0; j < p; j++) {
            int64_t sum = 0;
            for (k = 0; k < n; k++) {
                sum += a.data[i][k] * bT.data[j][k];
            }
            sum /= divisor;
            sum += c.data[j];
            out.data[i][j] = sum;
        }
    }
}


template<int m, typename atype, int n, int p, typename ctype, typename otype>
void matrix_multiply_add_div_relu(const matrix<m, n, atype> &a, const matrix<p, n, atype> &bT, const mvector<p, ctype> &c, int divisor, int min, int max, matrix<m, p, otype> &out) {
    int i, j, k;

    for (i = 0; i < m; i++) {
        for (j = 0; j < p; j++) {
            int64_t sum = 0;
            for (k = 0; k < n; k++) {
                sum += a.data[i][k] * bT.data[j][k];
            }
            sum /= divisor;
            sum += c.data[j];
            if (sum > max) {
                sum = max;
            }
            else if (sum < min) {
                sum = min;
            }
            out.data[i][j] = sum;
        }
    }
}


#endif