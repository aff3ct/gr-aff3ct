/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDED_AFF3CT_RATE_MATCHER_H
#define INCLUDED_AFF3CT_RATE_MATCHER_H

#include <algorithm>
#include <vector>

namespace gr {
namespace aff3ct {

/*!
 * Maps the N_cw = 3K + 4t bits of the AFF3CT turbo mother codeword (buffered
 * layout, t tail bits per constituent stream) to N transmitted bits.
 *
 * Mother codeword layout:
 *   [sys K][tail sys n t][par n K][tail par n t][tail sys i t][par i K][tail par i t]
 *
 * N < N_cw: the N_cw - N removed bits are uniformly spread over the 2K parity
 * bits, taken alternately from both encoders. N > N_cw: the mother codeword is
 * sent, then cyclically repeated from its first bit.
 */
class rate_matcher
{
public:
    rate_matcher(int K, int N_cw, int N);

    int N() const { return (int)d_pos.size(); }
    int N_cw() const { return d_N_cw; }

    //! out[j] = in[pos[j]], in has N_cw elements, out has N.
    template <typename TI, typename TO>
    void match(const TI* in, TO* out) const
    {
        for (size_t j = 0; j < d_pos.size(); j++)
            out[j] = (TO)in[d_pos[j]];
    }

    //! out[pos[j]] += scale * in[j], in has N elements, out has N_cw (punctured bits get 0).
    void dematch(const float* in, float* out, float scale) const
    {
        std::fill(out, out + d_N_cw, 0.f);
        for (size_t j = 0; j < d_pos.size(); j++)
            out[d_pos[j]] += scale * in[j];
    }

private:
    int d_N_cw;
    std::vector<int> d_pos;
};

} // namespace aff3ct
} // namespace gr

#endif /* INCLUDED_AFF3CT_RATE_MATCHER_H */
