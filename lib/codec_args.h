/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDED_AFF3CT_CODEC_ARGS_H
#define INCLUDED_AFF3CT_CODEC_ARGS_H

#include <aff3ct.hpp>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace gr {
namespace aff3ct {

/*!
 * The codec part of an AFF3CT simulator command line, e.g.
 * "-C LDPC --enc-type LDPC_5G -K 512 -N 1024 --dec-type BP_HORIZONTAL_LAYERED
 * --dec-implem NMS --dec-norm 0.75 --dec-simd INTER -p 16".
 *
 * The code type (-C), the codec (encoder, decoder, puncturer, interleaver...),
 * the CRC (--crc-*) and the quantizer (--qnt-*) are parsed with the parsers of
 * AFF3CT, and completed as the simulator does: K includes the CRC bits, the
 * quantizer format depends on the code and the precision (SIMD quantizer by
 * default, --qnt-implem STD for the one of the simulator).
 *
 * Two options of the simulation are read too, with their meaning in the simulator: the
 * precision of the decoder (-p 8, 16 or 32) and its frames per call (-F).
 */
class codec_args
{
public:
    explicit codec_args(const std::string& args);

    std::string code;      //!< the -C value (POLAR, TURBO, LDPC...)
    std::string precision; //!< int8, int16 or float: -p 8, -p 16, -p 32 (default)
    int frames = 0;        //!< frames per call of the decoder: -F, 0 if not given
    std::unique_ptr<::aff3ct::factory::Codec_SIHO> cdc;
    ::aff3ct::factory::CRC crc;
    ::aff3ct::factory::Quantizer qnt;
    //! Design noise of the polar frozen bits (sigma, or erasure probability if fb_noise_ep),
    //! -1 if the code has no frozen bits.
    float fb_noise = -1.f;
    bool fb_noise_ep = false;

    int K() const { return cdc->K; }               //!< bits encoded, CRC included
    int K_info() const { return cdc->K - crc_size(); } //!< information bits
    int N_cw() const { return cdc->N_cw; }         //!< mother codeword size
    int N() const { return cdc->N; }               //!< transmitted bits
    int crc_size() const { return crc.type == "NO" ? 0 : crc.size; }
    bool punctured() const { return cdc->N != cdc->N_cw; }
    //! Seed of the interleaver, -1 if the codec has no interleaver.
    int itl_seed() const;
};

/*!
 * Calls \p f and rethrows the AFF3CT exceptions without their stack trace, as the parser of
 * the simulator does: the errors of a wrong command line read better in Python. (The
 * message is built by what(), so it is read here, while the stack trace is disabled.)
 */
inline std::mutex& stacktrace_mutex()
{
    static std::mutex mutex;
    return mutex;
}

template <class F>
auto without_stacktrace(F f) -> decltype(f())
{
    // the flag is global: one object is built at a time
    std::lock_guard<std::mutex> lock(stacktrace_mutex());
    const bool save = spu::tools::exception::no_stacktrace;
    spu::tools::exception::no_stacktrace = true;
    try {
        auto r = f();
        spu::tools::exception::no_stacktrace = save;
        return r;
    } catch (const spu::tools::exception& e) {
        const std::string msg = e.what();
        spu::tools::exception::no_stacktrace = save;
        throw std::runtime_error(msg);
    } catch (...) {
        spu::tools::exception::no_stacktrace = save;
        throw;
    }
}

} // namespace aff3ct
} // namespace gr

#endif /* INCLUDED_AFF3CT_CODEC_ARGS_H */
