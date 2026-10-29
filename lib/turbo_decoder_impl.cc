/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#include "turbo_decoder_impl.h"
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace gr {
namespace aff3ct {

namespace {

template <typename B, typename Q>
class engine : public turbo_decoder_engine
{
private:
    std::unique_ptr<::aff3ct::tools::Codec_turbo<B, Q>> d_codec;
    std::unique_ptr<::aff3ct::module::Quantizer<float, Q>> d_qnt; // fixed point only
    int d_frames;
    mipp::vector<Q> d_Y_N;
    mipp::vector<B> d_V_K;

public:
    engine(const turbo_params& p, int frames, std::string& itl) : d_codec(build_turbo_codec<B, Q>(p, itl))
    {
        auto& dec = d_codec->get_decoder_siho();
        // the INTER decoder of libaff3ct processes one frame per SIMD lane: a different
        // count means that libaff3ct and gr-aff3ct were built with different SIMD flags
        // (buffer alignment and register width would not match)
        if (p.simd == "INTER" && dec.get_n_frames_per_wave() != (size_t)mipp::N<Q>()) {
            std::stringstream msg;
            msg << "gr-aff3ct: AFF3CT and gr-aff3ct were built with different SIMD instruction "
                   "sets (gr-aff3ct: "
                << mipp::InstructionFullType << ", " << mipp::N<Q>()
                << " lanes; libaff3ct: " << dec.get_n_frames_per_wave()
                << " lanes). Rebuild both with the same flags (e.g. -march=native).";
            throw std::runtime_error(msg.str());
        }
        d_frames = frames > 0 ? frames : (int)dec.get_n_frames_per_wave();
        d_codec->set_n_frames(d_frames);
        // set_n_frames() regenerates the interleaver without reseeding it: a RANDOM
        // interleaver would then differ from the encoder one
        d_codec->get_interleaver().set_seed(p.seed);

        const int N_cw = dec.get_N();
        if (!std::is_floating_point<Q>::value) {
            // same quantization as the AFF3CT simulator for 16-bit turbo codes: 6 bits, 3 fractional
            d_qnt.reset(new ::aff3ct::module::Quantizer_pow2_fast<float, Q>(N_cw, 3, 6));
            d_qnt->set_n_frames(d_frames);
        }
        d_Y_N.resize((size_t)d_frames * N_cw);
        d_V_K.resize((size_t)d_frames * dec.get_K());
    }

    int N_cw() const override { return d_codec->get_decoder_siho().get_N(); }
    int frames() const override { return d_frames; }

    void decode(const float* Y_N, unsigned char* V_K) override
    {
        auto& dec = d_codec->get_decoder_siho();
        if (d_qnt) {
            d_qnt->process((const float*)Y_N, (Q*)d_Y_N.data());
            dec.decode_siho(d_Y_N.data(), d_V_K.data());
        } else {
            dec.decode_siho((const Q*)Y_N, d_V_K.data());
        }
        for (size_t i = 0; i < d_V_K.size(); i++)
            V_K[i] = (unsigned char)d_V_K[i];
    }
};

std::unique_ptr<turbo_decoder_engine> make_engine(const turbo_params& p,
                                                  const std::string& precision,
                                                  int frames,
                                                  std::string& itl)
{
    if (precision == "int16")
        return std::unique_ptr<turbo_decoder_engine>(new engine<int16_t, int16_t>(p, frames, itl));
    if (precision == "float")
        return std::unique_ptr<turbo_decoder_engine>(new engine<int32_t, float>(p, frames, itl));
    throw std::invalid_argument("gr-aff3ct: precision must be int16 or float");
}

turbo_params make_params(int K,
                         int n_ite,
                         const std::string& interleaver,
                         int seed,
                         const std::string& simd,
                         const std::string& aff3ct_args)
{
    if (simd != "INTER" && simd != "INTRA" && simd != "SEQ")
        throw std::invalid_argument("gr-aff3ct: simd must be INTER, INTRA or SEQ");
    turbo_params p;
    p.K = K;
    p.n_ite = n_ite;
    p.interleaver = interleaver;
    p.seed = seed;
    p.simd = simd == "SEQ" ? "" : simd;
    p.aff3ct_args = aff3ct_args;
    return p;
}

} // namespace

turbo_decoder::sptr turbo_decoder::make(int K,
                                                   int N,
                                                   int n_ite,
                                                   const std::string& interleaver,
                                                   int seed,
                                                   const std::string& precision,
                                                   const std::string& simd,
                                                   int frames,
                                                   float llr_scale,
                                                   const std::string& aff3ct_args)
{
    return turbo_decoder::sptr(new turbo_decoder_impl(
        K, N, n_ite, interleaver, seed, precision, simd, frames, llr_scale, aff3ct_args));
}

turbo_decoder_impl::turbo_decoder_impl(int K,
                                       int N,
                                       int n_ite,
                                       const std::string& interleaver,
                                       int seed,
                                       const std::string& precision,
                                       const std::string& simd,
                                       int frames,
                                       float llr_scale,
                                       const std::string& aff3ct_args)
    : gr::fec::generic_decoder("aff3ct_turbo_decoder"),
      d_K(K),
      d_llr_scale(llr_scale),
      d_engine(make_engine(make_params(K, n_ite, interleaver, seed, simd, aff3ct_args),
                           precision,
                           frames,
                           d_itl)),
      d_rm(K, d_engine->N_cw(), N),
      d_Y_N((size_t)d_engine->frames() * d_engine->N_cw())
{
}

void turbo_decoder_impl::generic_work(const void* in_buffer, void* out_buffer)
{
    const auto* in = static_cast<const float*>(in_buffer);
    const int N = d_rm.N(), N_cw = d_rm.N_cw();
    for (int f = 0; f < frames(); f++)
        d_rm.dematch(in + (size_t)f * N, d_Y_N.data() + (size_t)f * N_cw, d_llr_scale);
    d_engine->decode(d_Y_N.data(), static_cast<unsigned char*>(out_buffer));
}

bool turbo_decoder_impl::set_frame_size(unsigned int frame_size)
{
    if (frame_size == (unsigned int)get_output_size())
        return true;
    d_logger->error("frame size is fixed by frames * K = {:d}, cannot change it to {:d}",
                    get_output_size(),
                    frame_size);
    return false;
}

} // namespace aff3ct
} // namespace gr
