/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDED_AFF3CT_TURBO_CODEC_H
#define INCLUDED_AFF3CT_TURBO_CODEC_H

#include <aff3ct.hpp>
#include <memory>
#include <string>
#include <vector>

namespace gr {
namespace aff3ct {

//! Parameters shared by the encoder and the decoder objects.
struct turbo_params {
    int K;
    std::string interleaver = "AUTO"; // AUTO, LTE or RANDOM
    int seed = 0;
    int n_ite = 6;
    std::string simd = "";            // "", INTER or INTRA
    std::string aff3ct_args = "";
};

/*!
 * Builds an AFF3CT turbo codec exactly as the AFF3CT simulator does, by
 * parsing a command line ("-K ... --enc-sub-std LTE ..." + aff3ct_args).
 * With interleaver AUTO, the LTE interleaver is tried first and RANDOM is used
 * if K is not an LTE size. \p itl_used receives the interleaver kept.
 */
template <typename B, typename Q>
std::unique_ptr<::aff3ct::tools::Codec_turbo<B, Q>> build_turbo_codec(const turbo_params& p,
                                                                      std::string& itl_used);

} // namespace aff3ct
} // namespace gr

#endif /* INCLUDED_AFF3CT_TURBO_CODEC_H */
