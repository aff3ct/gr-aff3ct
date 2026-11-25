/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDED_AFF3CT_CODEC_ENCODER_IMPL_H
#define INCLUDED_AFF3CT_CODEC_ENCODER_IMPL_H

#include "codec_modules.h"
#include <gnuradio/aff3ct/codec_encoder.h>

namespace gr {
namespace aff3ct {

class codec_encoder_impl : public codec_encoder
{
private:
    codec_args d_args;
    codec_modules<int32_t, float> d_m;
    mipp::vector<int32_t> d_U_K1; // information bits
    mipp::vector<int32_t> d_U_K2; // information and CRC bits
    mipp::vector<int32_t> d_X_N1; // mother codeword
    mipp::vector<int32_t> d_X_N2; // punctured codeword
    int d_frames = 1;             // frames of the current packet

public:
    codec_encoder_impl(const std::string& aff3ct_args);
    ~codec_encoder_impl() override = default;

    void generic_work(const void* in_buffer, void* out_buffer) override;
    double rate() override { return (double)info_bits() / coded_bits(); }
    int get_input_size() override { return d_frames * info_bits(); }
    int get_output_size() override { return d_frames * coded_bits(); }
    bool set_frame_size(unsigned int frame_size) override;
    const char* get_input_conversion() override { return "none"; }
    const char* get_output_conversion() override { return "none"; }
    int info_bits() const override { return d_args.K_info(); }
    int coded_bits() const override { return d_args.N(); }
};

} // namespace aff3ct
} // namespace gr

#endif /* INCLUDED_AFF3CT_CODEC_ENCODER_IMPL_H */
