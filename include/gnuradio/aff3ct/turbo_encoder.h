/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDED_AFF3CT_TURBO_ENCODER_H
#define INCLUDED_AFF3CT_TURBO_ENCODER_H

#include <gnuradio/aff3ct/api.h>
#include <gnuradio/fec/generic_encoder.h>
#include <string>

namespace gr {
namespace aff3ct {

/*!
 * \brief AFF3CT turbo encoder (LTE constituent codes, terminated trellis).
 * \ingroup aff3ct
 *
 * \details
 * FEC encoder object for the gr-fec deployment blocks (extended, tagged, async).
 * One call encodes one frame: \p K unpacked bits (one bit per byte, 0 or 1)
 * into \p N unpacked bits.
 *
 * The mother code is the AFF3CT turbo code with two LTE recursive systematic
 * convolutional encoders (8 states, polynomials {013, 015}) and terminated
 * trellises: N_cw = 3K + 12. Any N is reached by rate matching: if N < N_cw,
 * parity bits are uniformly punctured (systematic and tail bits are always
 * transmitted, so N >= K + 12); if N > N_cw, bits are cyclically repeated.
 *
 * The encoder and the decoder must be built with the same K, N, interleaver
 * and seed.
 */
class AFF3CT_API turbo_encoder : virtual public gr::fec::generic_encoder
{
public:
    typedef std::shared_ptr<turbo_encoder> sptr;

    /*!
     * \param K           number of information bits per frame.
     * \param N           number of transmitted bits per frame (K + 12 <= N).
     * \param interleaver "AUTO" (LTE if K is an LTE size, RANDOM otherwise),
     *                    "LTE" or "RANDOM".
     * \param seed        seed of the RANDOM interleaver.
     * \param aff3ct_args extra AFF3CT command line arguments for the codec
     *                    (e.g. "--itl-type GOLDEN"), applied last.
     */
    static sptr make(int K,
                     int N,
                     const std::string& interleaver = "AUTO",
                     int seed = 0,
                     const std::string& aff3ct_args = "");

    //! The interleaver actually used ("LTE" or "RANDOM" when AUTO was asked).
    virtual std::string interleaver() const = 0;
};

} // namespace aff3ct
} // namespace gr

#endif /* INCLUDED_AFF3CT_TURBO_ENCODER_H */
