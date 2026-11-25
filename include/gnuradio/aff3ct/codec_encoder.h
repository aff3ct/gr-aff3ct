/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDED_AFF3CT_CODEC_ENCODER_H
#define INCLUDED_AFF3CT_CODEC_ENCODER_H

#include <gnuradio/aff3ct/api.h>
#include <gnuradio/fec/generic_encoder.h>
#include <string>

namespace gr {
namespace aff3ct {

/*!
 * \brief Encoder of any AFF3CT code, described by its AFF3CT command line.
 * \ingroup aff3ct
 *
 * \details
 * FEC encoder object for the gr-fec deployment blocks (extended, tagged, async).
 * The code is described with the options of the AFF3CT simulator, e.g.
 * "-C LDPC --enc-type LDPC_5G -K 512 -N 1024 --dec-type BP_HORIZONTAL_LAYERED
 * --dec-implem NMS --dec-norm 0.75 --dec-simd INTER -p 16". Only the options of the codec,
 * the CRC (--crc-*) and the quantizer (--qnt-*) are accepted, with -p and -F for the
 * decoder (see gr::aff3ct::codec_decoder). Give the same command line to both.
 *
 * With a CRC, K counts the CRC bits (as in AFF3CT): one frame carries
 * K - CRC size information bits.
 *
 * Input: info_bits() unpacked bits (one bit per byte). Output: N unpacked bits.
 */
class AFF3CT_API codec_encoder : virtual public gr::fec::generic_encoder
{
public:
    typedef std::shared_ptr<codec_encoder> sptr;

    /*!
     * \param aff3ct_args AFF3CT command line of the codec (-C, -K, -N, --enc-*, --dec-*,
     *                    --pct-*, --itl-*, --crc-*...).
     */
    static sptr make(const std::string& aff3ct_args);

    //! Information bits per frame (K minus the CRC bits).
    virtual int info_bits() const = 0;
    //! Transmitted bits per frame.
    virtual int coded_bits() const = 0;
};

} // namespace aff3ct
} // namespace gr

#endif /* INCLUDED_AFF3CT_CODEC_ENCODER_H */
