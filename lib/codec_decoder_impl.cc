/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#include "codec_decoder_impl.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace gr {
namespace aff3ct {

namespace {

template <typename B, typename Q>
class engine : public codec_decoder_engine
{
private:
    const codec_args& d_args;
    codec_modules<B, Q> d_m;
    int d_frames;
    mipp::vector<Q> d_Y_N1; // quantized LLRs (fixed point only)
    mipp::vector<Q> d_Y_N2; // depunctured LLRs (punctured codes only)
    mipp::vector<B> d_V_K1; // decoded bits, CRC included
    mipp::vector<B> d_V_K2; // information bits (with a CRC only)

public:
    engine(const codec_args& a, int frames) : d_args(a), d_m(a, frames), d_frames(d_m.frames)
    {
        const size_t F = d_m.n_frames;
        if (d_m.qnt)
            d_Y_N1.resize(F * a.N());
        if (a.punctured())
            d_Y_N2.resize(F * a.N_cw());
        d_V_K1.resize(F * a.K());
        if (d_m.crc)
            d_V_K2.resize(F * a.K_info());
    }

    int frames() const override { return d_frames; }
    int padded_frames() const override { return d_m.n_frames; }

    void decode(const float* Y_N, unsigned char* V_K, int n_fra) override
    {
        const int N = d_args.N(), N_cw = d_args.N_cw(), K = d_args.K(), K_info = d_args.K_info();
        const int F = d_m.call_frames;
        for (int f = 0; f < n_fra; f += F) {
            const Q* Y = reinterpret_cast<const Q*>(Y_N + (size_t)f * N); // float: Q is float
            if (d_m.qnt) {
                d_m.qnt->process(Y_N + (size_t)f * N, d_Y_N1.data() + (size_t)f * N);
                Y = d_Y_N1.data() + (size_t)f * N;
            }
            if (d_args.punctured()) {
                d_m.codec->get_puncturer().depuncture(Y, d_Y_N2.data() + (size_t)f * N_cw);
                Y = d_Y_N2.data() + (size_t)f * N_cw;
            }
            d_m.codec->get_decoder_siho().decode_siho(Y, d_V_K1.data() + (size_t)f * K);
            if (d_m.crc)
                d_m.crc->extract(d_V_K1.data() + (size_t)f * K, d_V_K2.data() + (size_t)f * K_info);
        }
        // a decoded bit is 0 or any other value (e.g. the sign bit for the polar decoders)
        const B* V = d_m.crc ? d_V_K2.data() : d_V_K1.data();
        const size_t n = (size_t)n_fra * K_info;
        for (size_t i = 0; i < n; i++)
            V_K[i] = V[i] != 0;
    }
};

std::unique_ptr<codec_decoder_engine> make_engine(const codec_args& a, int frames)
{
    check_simd_build();
    if (a.precision == "int8")
        return std::unique_ptr<codec_decoder_engine>(new engine<int8_t, int8_t>(a, frames));
    if (a.precision == "int16")
        return std::unique_ptr<codec_decoder_engine>(new engine<int16_t, int16_t>(a, frames));
    return std::unique_ptr<codec_decoder_engine>(new engine<int32_t, float>(a, frames));
}

} // namespace

codec_decoder::sptr codec_decoder::make(const std::string& aff3ct_args, float llr_scale)
{
    return without_stacktrace(
        [&] { return codec_decoder::sptr(new codec_decoder_impl(aff3ct_args, llr_scale)); });
}

codec_decoder_impl::codec_decoder_impl(const std::string& aff3ct_args, float llr_scale)
    : gr::fec::generic_decoder("aff3ct_codec_decoder"),
      d_args(aff3ct_args),
      d_llr_scale(llr_scale),
      d_engine(make_engine(d_args, d_args.frames)),
      d_Y_N((size_t)d_engine->padded_frames() * d_args.N()),
      d_frames(d_engine->frames()),
      d_filled(0)
{
    // some AFF3CT errors only show at the first decoding (e.g. a decoder that does not
    // support this SIMD instruction set): raise them here rather than in the flowgraph
    // thread, where they would stop the decoder block. The all-zero codeword is received
    // without noise (zero LLRs would make a sequential decoder search forever); it also
    // stays in the padding frames.
    std::fill(d_Y_N.begin(), d_Y_N.end(), 2.f);
    std::vector<unsigned char> V_K((size_t)get_output_size());
    d_engine->decode(d_Y_N.data(), V_K.data(), d_frames);
}

void codec_decoder_impl::generic_work(const void* in_buffer, void* out_buffer)
{
    const auto* in = static_cast<const float*>(in_buffer);
    const size_t N = (size_t)d_args.N(), n = (size_t)d_frames * N;
    // the frames of a longer previous packet become padding again
    if (d_frames < d_filled)
        std::fill(d_Y_N.begin() + n, d_Y_N.begin() + (size_t)d_filled * N, 2.f);
    d_filled = d_frames;
    for (size_t i = 0; i < n; i++)
        d_Y_N[i] = d_llr_scale * in[i];
    d_engine->decode(d_Y_N.data(), static_cast<unsigned char*>(out_buffer), d_frames);
}

// The Tagged and Async blocks of gr-fec give the size of each packet (in decoded bits) before
// generic_work(): a packet holds 1 to frames() codewords. They also give their MTU when they
// are built, which is larger than any packet.
bool codec_decoder_impl::set_frame_size(unsigned int frame_size)
{
    bool whole, fits;
    d_frames = packet_frames(frame_size, info_bits(), frames(), whole, fits);
    if (!fits)
        d_logger->info("tried to set a packet of {:d} bits; the maximum is frames * information "
                       "bits = {:d}",
                       frame_size,
                       frames() * info_bits());
    else if (!whole)
        d_logger->warn("a packet of {:d} bits is not a whole number of frames of {:d} information "
                       "bits: {:d} frame(s) decoded",
                       frame_size,
                       info_bits(),
                       d_frames);
    return whole && fits;
}

} // namespace aff3ct
} // namespace gr
