#ifndef _MATRIX_H_
#define _MATRIX_H_

#include <cassert>
#include <cstdint>
#include <string.h>
#include <iostream>
#include <math.h>
#include <algorithm>
#ifdef __SSSE3__
#include <x86intrin.h>
#endif

#define MAX_RELU 64

template<int m, int n, typename ntype>
struct mmatrix {
    mmatrix() {
    }
    mmatrix(const ntype data[m][n]) {
        memcpy(this->mdata, data, m * n * sizeof(ntype));
    }

    mmatrix &operator=(const mmatrix &other) {
        memcpy(this->mdata, other.mdata, sizeof(this->mdata));
        return *this;
    }

    // for YMM align
    alignas(32) ntype mdata[m][n];
};

template<int n, typename ntype>
struct mvector {
    mvector() {
    }
    mvector(const ntype data[n]) {
        memcpy(this->mdata, data, n * sizeof(ntype));
    }

    // for YMM align
    alignas(32) ntype mdata[n];

    template <typename outtype>
    void clamp(mvector<n, outtype> &out, outtype min, outtype max) const {
        for (int i = 0; i < n ; i++) {
            out.mdata[i] = std::clamp(mdata[i], min, max);
        }
    }
    template<int p, typename btype, typename ctype, typename otype>
    void matrix_multiply_add_div(const mmatrix<p, n, btype> &bT, const mvector<p, ctype> &c, mvector<p, otype> &out, int unity) const {
        // out = a * b + c
        int j;

        for (j = 0; j < p; j++) {
            int sum = this->dot_product_n(bT.mdata[j]);
            out.mdata[j] = sum / unity + c.mdata[j];
        }
    }


    template<int p, typename btype, typename ctype, typename otype>
    void matrix_multiply_add_div_relu(const mmatrix<p, n, btype> &bT, const mvector<p, ctype> &c, mvector<p, otype> &out, int unity, int min, int max) {
        // clipped relu 0-1
        int j;

        for (j = 0; j < p; j++) {
            int sum = this->dot_product(bT.mdata[j]);
            out.mdata[j] = std::clamp(sum / unity + c.mdata[j], min, max);
        }
    }

    template<typename btype>
    int dot_product(const mvector<n, btype> a) {
        int sum = 0;

        for (int i = 0; i < n; i++) {
            sum += a.mdata[i] * mdata[i];
        }
        return sum;
    }

    template<typename btype>
    int dot_product(const btype a[n]) {
        int sum = 0;

        for (int i = 0; i < n; i++) {
            sum += a[i] * mdata[i];
        }
        return sum;
    }

};




#endif