/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDED_AFF3CT_CODEC_DECODER_IMPL_H
#define INCLUDED_AFF3CT_CODEC_DECODER_IMPL_H

#include "codec_modules.h"
#include <gnuradio/aff3ct/codec_decoder.h>

namespace gr {
namespace aff3ct {

//! Decodes frames of float LLRs into K_info bits each (one implementation per precision).
class codec_decoder_engine
{
public:
    virtual ~codec_decoder_engine() = default;
    virtual int frames() const = 0;
    //! frames rounded up to whole waves of the decoder (the input holds padded_frames())
    virtual int padded_frames() const = 0;
    //! Decodes the first \p n frames of Y_N (at most frames()), in whole calls of the modules.
    virtual void decode(const float* Y_N, unsigned char* V_K, int n) = 0;
};

class codec_decoder_impl : public codec_decoder
{
private:
    codec_args d_args;
    float d_llr_scale;
    std::unique_ptr<codec_decoder_engine> d_engine;
    mipp::vector<float> d_Y_N; // scaled LLRs, padded to whole waves, aligned for AFF3CT
    int d_frames;              // frames of the current packet, at most frames()
    int d_filled;              // frames of d_Y_N written by the previous packet

public:
    codec_decoder_impl(const std::string& aff3ct_args, float llr_scale);
    ~codec_decoder_impl() override = default;

    // The input of generic_work() became const in GNU Radio 3.10.11: both signatures are
    // declared (without "override"), the one of the installed gr-fec is the virtual one.
    void generic_work(const void* in_buffer, void* out_buffer);
    void generic_work(void* in_buffer, void* out_buffer)
    {
        generic_work(static_cast<const void*>(in_buffer), out_buffer);
    }
    double rate() override { return (double)info_bits() / coded_bits(); }
    int get_input_size() override { return d_frames * coded_bits(); }
    int get_output_size() override { return d_frames * info_bits(); }
    bool set_frame_size(unsigned int frame_size) override;
    const char* get_input_conversion() override { return "none"; }
    const char* get_output_conversion() override { return "none"; }
    int frames() const override { return d_engine->frames(); }
    int info_bits() const override { return d_args.K_info(); }
    int coded_bits() const override { return d_args.N(); }
};

} // namespace aff3ct
} // namespace gr

#endif /* INCLUDED_AFF3CT_CODEC_DECODER_IMPL_H */
