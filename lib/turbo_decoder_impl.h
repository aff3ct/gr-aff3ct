/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDED_AFF3CT_TURBO_DECODER_IMPL_H
#define INCLUDED_AFF3CT_TURBO_DECODER_IMPL_H

#include "rate_matcher.h"
#include "turbo_codec.h"
#include <gnuradio/aff3ct/turbo_decoder.h>

namespace gr {
namespace aff3ct {

//! Decodes F mother codewords of float LLRs into F*K bits (one per precision).
class turbo_decoder_engine
{
public:
    virtual ~turbo_decoder_engine() = default;
    virtual int N_cw() const = 0;
    virtual int frames() const = 0;
    virtual void decode(const float* Y_N, unsigned char* V_K) = 0;
};

class turbo_decoder_impl : public turbo_decoder
{
private:
    int d_K;
    float d_llr_scale;
    std::string d_itl;
    std::unique_ptr<turbo_decoder_engine> d_engine;
    rate_matcher d_rm;
    mipp::vector<float> d_Y_N; // F dematched mother codewords

public:
    turbo_decoder_impl(int K,
                       int N,
                       int n_ite,
                       const std::string& interleaver,
                       int seed,
                       const std::string& precision,
                       const std::string& simd,
                       int frames,
                       float llr_scale,
                       const std::string& aff3ct_args);
    ~turbo_decoder_impl() override = default;

    void generic_work(const void* in_buffer, void* out_buffer) override;
    double rate() override { return (double)d_K / d_rm.N(); }
    int get_input_size() override { return frames() * d_rm.N(); }
    int get_output_size() override { return frames() * d_K; }
    bool set_frame_size(unsigned int frame_size) override;
    const char* get_input_conversion() override { return "none"; }
    const char* get_output_conversion() override { return "none"; }
    int frames() const override { return d_engine->frames(); }
    std::string interleaver() const override { return d_itl; }
};

} // namespace aff3ct
} // namespace gr

#endif /* INCLUDED_AFF3CT_TURBO_DECODER_IMPL_H */
