/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDED_AFF3CT_TURBO_DECODER_H
#define INCLUDED_AFF3CT_TURBO_DECODER_H

#include <gnuradio/aff3ct/api.h>
#include <gnuradio/fec/generic_decoder.h>
#include <string>

namespace gr {
namespace aff3ct {

/*!
 * \brief AFF3CT turbo decoder (LTE constituent codes, terminated trellis).
 * \ingroup aff3ct
 *
 * \details
 * FEC decoder object for the gr-fec deployment blocks (extended, tagged, async),
 * matching gr::aff3ct::turbo_encoder (same K, N, interleaver and seed).
 *
 * Input: float LLRs, AFF3CT convention (LLR = log(P(b=0)/P(b=1)), so a positive
 * value means bit 0). Output: unpacked bits (one bit per byte).
 *
 * One call decodes \p frames frames at once (input F*N floats, output F*K bits).
 * The SIMD decoders are only fast when several frames are decoded together:
 * with the INTER strategy, one SIMD register holds one value of each of
 * F frames, so F should be a multiple of the SIMD width (0 selects it).
 */
class AFF3CT_API turbo_decoder : virtual public gr::fec::generic_decoder
{
public:
    typedef std::shared_ptr<turbo_decoder> sptr;

    /*!
     * \param K           number of information bits per frame.
     * \param N           number of received LLRs per frame (K + 12 <= N).
     * \param n_ite       number of turbo iterations.
     * \param interleaver "AUTO", "LTE" or "RANDOM" (see turbo_encoder).
     * \param seed        seed of the RANDOM interleaver.
     * \param precision   "int16" (fixed point, fastest) or "float".
     * \param simd        "INTER" (several frames per SIMD register, fastest),
     *                    "INTRA" (trellis states in SIMD registers) or "SEQ".
     * \param frames      frames decoded per call, 0 = SIMD width of the strategy.
     * \param llr_scale   factor applied to the input LLRs (before quantization
     *                    in int16: 6 bits with 3 fractional bits, i.e. +/-3.875).
     * \param aff3ct_args extra AFF3CT command line arguments for the codec
     *                    (e.g. "--dec-sf-type LTE_VEC --dec-sub-max MAXS"), applied last.
     */
    static sptr make(int K,
                     int N,
                     int n_ite = 6,
                     const std::string& interleaver = "AUTO",
                     int seed = 0,
                     const std::string& precision = "int16",
                     const std::string& simd = "INTER",
                     int frames = 0,
                     float llr_scale = 1.0f,
                     const std::string& aff3ct_args = "");

    //! Number of frames decoded per call.
    virtual int frames() const = 0;
    //! The interleaver actually used ("LTE" or "RANDOM" when AUTO was asked).
    virtual std::string interleaver() const = 0;
};

} // namespace aff3ct
} // namespace gr

#endif /* INCLUDED_AFF3CT_TURBO_DECODER_H */
