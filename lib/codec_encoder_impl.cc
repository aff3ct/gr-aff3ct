/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#include "codec_encoder_impl.h"
#include <vector>

namespace gr {
namespace aff3ct {

codec_encoder::sptr codec_encoder::make(const std::string& aff3ct_args)
{
    return without_stacktrace(
        [&] { return codec_encoder::sptr(new codec_encoder_impl(aff3ct_args)); });
}

codec_encoder_impl::codec_encoder_impl(const std::string& aff3ct_args)
    : gr::fec::generic_encoder("aff3ct_codec_encoder"),
      d_args(aff3ct_args),
      d_m((check_simd_build(), d_args), 1, false),
      d_U_K1(d_args.K_info()),
      d_U_K2(d_args.K()),
      d_X_N1(d_args.N_cw()),
      d_X_N2(d_args.N())
{
    // raise the AFF3CT errors of the first encoding here (see codec_decoder_impl)
    std::vector<unsigned char> U(info_bits()), X(coded_bits());
    generic_work(U.data(), X.data());
}

void codec_encoder_impl::generic_work(const void* in_buffer, void* out_buffer)
{
    const auto* in = static_cast<const unsigned char*>(in_buffer);
    auto* out = static_cast<unsigned char*>(out_buffer);

    for (int f = 0; f < d_frames; f++, in += d_U_K1.size(), out += d_X_N2.size()) {
        for (size_t k = 0; k < d_U_K1.size(); k++)
            d_U_K1[k] = in[k] & 1;
        const int32_t* U_K = d_U_K1.data();
        if (d_m.crc) {
            d_m.crc->build(d_U_K1.data(), d_U_K2.data());
            U_K = d_U_K2.data();
        }
        d_m.codec->get_encoder().encode(U_K, d_X_N1.data());
        const int32_t* X_N = d_X_N1.data();
        if (d_args.punctured()) {
            d_m.codec->get_puncturer().puncture(d_X_N1.data(), d_X_N2.data());
            X_N = d_X_N2.data();
        }
        for (size_t n = 0; n < d_X_N2.size(); n++)
            out[n] = X_N[n] != 0;
    }
}

// The Tagged and Async blocks of gr-fec give the size of each packet (in information bits)
// before generic_work(): a packet holds one or several frames. They also give their MTU when
// they are built.
bool codec_encoder_impl::set_frame_size(unsigned int frame_size)
{
    bool whole, fits;
    d_frames = packet_frames(frame_size, info_bits(), 0, whole, fits);
    if (!whole)
        d_logger->warn("a packet of {:d} bits is not a whole number of frames of {:d} information "
                       "bits: {:d} frame(s) encoded",
                       frame_size,
                       info_bits(),
                       d_frames);
    return whole;
}

} // namespace aff3ct
} // namespace gr
