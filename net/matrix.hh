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


template<int m, typename atype, int n, typename btype, int p, typename otype>
void matrix_multiply(matrix<m, n, atype> &a, matrix<n, p, btype> &b, matrix<m, p, otype> &out) {
    int i, j, k;

    for (i = 0; i < m; i++) {
        for (j = 0; j < p; j++) {
            out.data[i][j] = 0;
            for (k = 0; k < n; k++) {
                out.data[i][j] += a.data[i][k] * b.data[k][j];
            }
        }
    }
}

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

const int exp_table_negative_64ths[] = {
    64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 53, 52, 51, 50,
    49, 49, 48, 47, 46, 46, 45, 44, 43, 43, 42, 41, 41, 40, 40, 39,
    38, 38, 37, 37, 36, 35, 35, 34, 34, 33, 33, 32, 32, 31, 31, 30,
    30, 29, 29, 28, 28, 27, 27, 27, 26, 26, 25, 25, 25, 24, 24, 23,
    23, 23, 22, 22, 22, 21, 21, 21, 20, 20, 20, 19, 19, 19, 18, 18,
    18, 18, 17, 17, 17, 16, 16, 16, 16, 15, 15, 15, 15, 14, 14, 14,
    14, 14, 13, 13, 13, 13, 13, 12, 12, 12, 12, 12, 11, 11, 11, 11,
    11, 10, 10, 10, 10, 10, 10, 9, 9, 9, 9, 9, 9, 9, 8, 8
};

const int exp_table_positive_64ths[] = {
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 76, 77, 78, 79, 80,
    82, 83, 84, 86, 87, 88, 90, 91, 93, 94, 96, 97, 99, 100, 102, 103,
    105, 107, 108, 110, 112, 114, 115, 117, 119, 121, 123, 125, 127, 129, 131, 133,
    135, 137, 139, 141, 144, 146, 148, 151, 153, 155, 158, 160, 163, 166, 168, 171,
    173, 176, 179, 182, 185, 188, 191, 194, 197, 200, 203, 206, 209, 213, 216, 219,
    223, 226, 230, 234, 237, 241, 245, 249, 253, 257, 261, 265, 269, 273, 278, 282,
    286, 291, 295, 300, 305, 310, 315, 319, 325, 330, 335, 340, 345, 351, 356, 362,
    368, 374, 379, 385, 392, 398, 404, 410, 417, 423, 430, 437, 444, 451, 458, 465
};

constexpr uint16_t exp_64ths(int16_t x) {
    // compute 64*exp(x/64)
    if (x >= 0) {
        if (x >= 128) {
            x = 127;
        }
        return exp_table_positive_64ths[x];
    } else {
        if (x <= -127) {
            x = -127;
        }
        return exp_table_negative_64ths[-x];
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
            atype old_value = a.data[i][k];
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
void matrix_multiply_add_div(const matrix<m, n, atype> &a, const matrix<n, p, btype> &b, const mvector<p, ctype> &c, int divisor, matrix<m, p, otype> &out) {
    // out = a * b + c
    int i, j, k;

    for (i = 0; i < m; i++) {
        for (j = 0; j < p; j++) {
            int64_t sum = 0;
            for (k = 0; k < n; k++) {
                sum += a.data[i][k] * b.data[k][j];
            }
            sum /= divisor;
            sum += c.data[j];
            out.data[i][j] = sum;
        }
    }
}


template<int m, typename atype, int n, int p, typename ctype, typename otype>
void matrix_multiply_add_div_relu(const matrix<m, n, atype> &a, const matrix<n, p, atype> &b, const mvector<p, ctype> &c, int divisor, int min, int max, matrix<m, p, otype> &out) {
    int i, j, k;

    for (i = 0; i < m; i++) {
        for (j = 0; j < p; j++) {
            int64_t sum = 0;
            for (k = 0; k < n; k++) {
                sum += a.data[i][k] * b.data[k][j];
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


template<int m, int n, int p>
void matrix_multiply_upper(matrix<m, n, unsigned char> &a, matrix<n, p, unsigned char> &b, matrix<m, p, unsigned char> &out) {
    int i, j, k;

    for (i = 0; i < m; i++) {
        for (j = 0; j < p; j++) {
            uint16_t sum = 0;
            for (k = 0; k < n; k++) {
                sum += a.data[i][k] * b.data[k][j];
            }
            out.data[i][j] = sum >> 8;
        }
    }
}


#endif