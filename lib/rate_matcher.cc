/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#include "rate_matcher.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>

namespace gr {
namespace aff3ct {

rate_matcher::rate_matcher(int K, int N_cw, int N) : d_N_cw(N_cw)
{
    const int t = (N_cw - 3 * K) / 4;
    if (K <= 0 || t < 0 || N_cw != 3 * K + 4 * t)
        throw std::invalid_argument("rate_matcher: unexpected turbo codeword size");

    if (N >= N_cw) {
        d_pos.resize(N);
        for (int j = 0; j < N; j++)
            d_pos[j] = j % N_cw;
        return;
    }

    const int n_par = 2 * K;
    const int n_drop = N_cw - N;
    if (n_drop > n_par) {
        std::stringstream msg;
        msg << "'N' has to be >= K + " << 4 * t << " (only the parity bits are punctured, 'K' = " << K
            << ", 'N' = " << N << ")";
        throw std::invalid_argument(msg.str());
    }

    const int par_n = K + t;     // first parity bit of the natural encoder
    const int par_i = 2 * K + 3 * t; // first parity bit of the interleaved encoder
    std::vector<bool> keep(N_cw, true);
    for (int64_t i = 0; i < n_par; i++) {
        // Bresenham: drop n_drop bits evenly spread over the n_par parity bits
        if (((i + 1) * n_drop) / n_par != (i * n_drop) / n_par) {
            const int k = (int)(i / 2);
            keep[(i % 2 == 0) ? par_n + k : par_i + k] = false;
        }
    }

    d_pos.reserve(N);
    for (int p = 0; p < N_cw; p++)
        if (keep[p])
            d_pos.push_back(p);
}

} // namespace aff3ct
} // namespace gr
