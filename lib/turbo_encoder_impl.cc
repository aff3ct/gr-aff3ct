/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#include "turbo_encoder_impl.h"

namespace gr {
namespace aff3ct {

namespace {
std::unique_ptr<::aff3ct::tools::Codec_turbo<int32_t, float>>
make_codec(int K, const std::string& interleaver, int seed, const std::string& args, std::string& itl)
{
    turbo_params p;
    p.K = K;
    p.interleaver = interleaver;
    p.seed = seed;
    p.aff3ct_args = args;
    return build_turbo_codec<int32_t, float>(p, itl);
}
} // namespace

turbo_encoder::sptr turbo_encoder::make(
    int K, int N, const std::string& interleaver, int seed, const std::string& aff3ct_args)
{
    return turbo_encoder::sptr(
        new turbo_encoder_impl(K, N, interleaver, seed, aff3ct_args));
}

turbo_encoder_impl::turbo_encoder_impl(
    int K, int N, const std::string& interleaver, int seed, const std::string& aff3ct_args)
    : gr::fec::generic_encoder("aff3ct_turbo_encoder"),
      d_K(K),
      d_codec(make_codec(K, interleaver, seed, aff3ct_args, d_itl)),
      d_rm(K, d_codec->get_encoder().get_N(), N),
      d_U_K(K),
      d_X_N(d_codec->get_encoder().get_N())
{
}

void turbo_encoder_impl::generic_work(const void* in_buffer, void* out_buffer)
{
    const auto* in = static_cast<const unsigned char*>(in_buffer);
    auto* out = static_cast<unsigned char*>(out_buffer);

    for (int k = 0; k < d_K; k++)
        d_U_K[k] = in[k] & 1;
    d_codec->get_encoder().encode(d_U_K.data(), d_X_N.data());
    d_rm.match(d_X_N.data(), out);
}

bool turbo_encoder_impl::set_frame_size(unsigned int frame_size)
{
    if (frame_size == (unsigned int)d_K)
        return true;
    d_logger->error("frame size is fixed by K = {:d}, cannot change it to {:d}", d_K, frame_size);
    return false;
}

} // namespace aff3ct
} // namespace gr
