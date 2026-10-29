/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDED_AFF3CT_TURBO_ENCODER_IMPL_H
#define INCLUDED_AFF3CT_TURBO_ENCODER_IMPL_H

#include "rate_matcher.h"
#include "turbo_codec.h"
#include <gnuradio/aff3ct/turbo_encoder.h>

namespace gr {
namespace aff3ct {

class turbo_encoder_impl : public turbo_encoder
{
private:
    int d_K;
    std::string d_itl;
    std::unique_ptr<::aff3ct::tools::Codec_turbo<int32_t, float>> d_codec;
    rate_matcher d_rm;
    mipp::vector<int32_t> d_U_K;
    mipp::vector<int32_t> d_X_N;

public:
    turbo_encoder_impl(int K,
                       int N,
                       const std::string& interleaver,
                       int seed,
                       const std::string& aff3ct_args);
    ~turbo_encoder_impl() override = default;

    void generic_work(const void* in_buffer, void* out_buffer) override;
    double rate() override { return (double)d_K / d_rm.N(); }
    int get_input_size() override { return d_K; }
    int get_output_size() override { return d_rm.N(); }
    bool set_frame_size(unsigned int frame_size) override;
    const char* get_input_conversion() override { return "none"; }
    const char* get_output_conversion() override { return "none"; }
    std::string interleaver() const override { return d_itl; }
};

} // namespace aff3ct
} // namespace gr

#endif /* INCLUDED_AFF3CT_TURBO_ENCODER_IMPL_H */
