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

#define GLMM

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
struct mvector;

template<int n, typename ntype, typename mvector_derived>
struct mvector_base {
    mvector_base() {
    }
    mvector_base(const ntype data[n]) {
        memcpy(this->mdata, data, n * sizeof(ntype));
    }

    // for YMM align
    alignas(32) ntype mdata[n];

    void copy_from(const mvector<n, short> &in, ntype min, ntype max) {
        for (int i = 0; i < n ; i++) {
            this->mdata[i] = std::clamp(in.mdata[i], static_cast<short>(min), static_cast<short>(max));
        }
    }

    template <typename outtype>
    void clamp(mvector<n, outtype> &out, ntype min, ntype max) const {
        for (int i = 0; i < n ; i++) {
            out.mdata[i] = std::clamp(mdata[i], min, max);
        }
    }

    void add_vector(int myoffset, const ntype *addend, int len) {
        for (int i = 0; i < len; i++) {
            this->mdata[i + myoffset] += addend[i];
        }
    }

    void sub_vector(int myoffset, const ntype *addend, int len) {
        for (int i = 0; i < len; i++) {
            this->mdata[i + myoffset] -= addend[i];
        }
    }

    template<int p, typename btype, typename ctype, typename otype>
    void matrix_multiply_add_div(const mmatrix<p, n, btype> &bT, const mvector<p, ctype> &c, mvector<p, otype> &out, int unity) const {
        // out = a * b + c
        int j;

        for (j = 0; j < p; j++) {
            int sum = ((mvector_derived*)this)->dot_product(bT.mdata[j]);
            out.mdata[j] = sum / unity + c.mdata[j];
        }
    }

    template<int p, typename btype, typename ctype, typename otype>
    void matrix_multiply_add_div_relu(const mmatrix<p, n, btype> &bT, const mvector<p, ctype> &c, mvector<p, otype> &out, int unity, int min, int max) {
        // clipped relu 0-1
        int j;

        for (j = 0; j < p; j++) {
            int sum = ((mvector_derived*)this)->dot_product(bT.mdata[j]);
            out.mdata[j] = std::clamp(sum / unity + c.mdata[j], min, max);
        }
    }

    int dot_product(const mvector_derived a) {
        return ((mvector_derived*)this)->dot_product(a.mdata);
    }

    int dot_product(const ntype a[n]) {
        return dot_product_simple(a, 0);
    }

    int dot_product_simple(const ntype a[n], int offset) {
        int sum = 0;

        for (int i = offset; i < n; i++) {
            sum += a[i] * mdata[i];
        }
        return sum;
    }

};

template<int n, typename ntype>
struct mvector : mvector_base<n, ntype, mvector<n, ntype> > {
};

#ifdef __ARM_NEON
#include <arm_neon.h>

template<int n>
struct mvector<n, int8_t> : mvector_base<n, int8_t, mvector<n, int8_t> > {

    int dot_product(const mvector<n, int8_t> a) {
        return dot_product(a.mdata);
    }

    int dot_product(const int8_t a[n]) {
        int16x8_t partialSumsNeon = vdupq_n_s16(0);
        int i = 0;
        for (; i < n/8; i++) {
            partialSumsNeon = vmlal_s8(partialSumsNeon, vld1_s8(&this->mdata[i*8]), vld1_s8(&a[i*8]));
        }
        int result = vaddvq_s16(partialSumsNeon);
        if (n % 8 != 0) {
            result += this->dot_product_simple(a, i*8);
        }
        return result;
    }


    void add_vector(int myoffset, const int8_t *addend, int len) {
        int i = 0;
        for (; i < len; i += 16) {
            vst1q_s8(&this->mdata[i + myoffset], vaddq_s8(vld1q_s8(&this->mdata[i + myoffset]), vld1q_s8(&addend[i])));
        }
        for (; i < len; i++) {
            this->mdata[i + myoffset] += addend[i];
        }
    }

    void copy_from(const mvector<n, short> &in, short min, short max) {
        int i = 0;
        int16x8_t zero = vdupq_n_s16(min);
        int16x8_t saturated = vdupq_n_s16(max);

        for (; i < n ; i += 8) {
            int16x8_t inputData = vld1q_s16(&in.mdata[i]);
            int16x8_t clamped = vminq_s16(vmaxq_s16(zero, inputData), saturated);
            int8x8_t compressed = vmovn_s16(clamped);
            vst1_s8(&this->mdata[i], compressed);
        }
        for (; i < n ; i++) {
            this->mdata[i] = std::clamp(in.mdata[i], static_cast<short>(min), static_cast<short>(max));
        }
    }
};

#endif

#endif