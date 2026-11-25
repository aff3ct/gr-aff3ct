/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDED_AFF3CT_CODEC_DECODER_H
#define INCLUDED_AFF3CT_CODEC_DECODER_H

#include <gnuradio/aff3ct/api.h>
#include <gnuradio/fec/generic_decoder.h>
#include <string>

namespace gr {
namespace aff3ct {

/*!
 * \brief Decoder of any AFF3CT code, described by its AFF3CT command line.
 * \ingroup aff3ct
 *
 * \details
 * FEC decoder object for the gr-fec deployment blocks (extended, tagged, async),
 * matching gr::aff3ct::codec_encoder built with the same command line.
 *
 * Input: float LLRs, AFF3CT convention (LLR = log(P(b=0)/P(b=1)), so a positive
 * value means bit 0). Output: unpacked information bits (one bit per byte, CRC
 * removed).
 *
 * Two options of the simulator are read in the command line, in addition to those
 * of the codec:
 *  - "-p 8", "-p 16" or "-p 32": precision of the decoder, fixed point on 8 or 16
 *    bits (faster when the decoder supports it) or float (default). In fixed point
 *    the LLRs are quantized as in the simulator (format of the code, or --qnt-bits
 *    and --qnt-dec).
 *  - "-F n": frames decoded per call (input frames()*N floats, output
 *    frames()*info_bits() bits). By default at least 16, a whole number of SIMD
 *    waves. With the Tagged or Async gr-fec blocks, a packet holds 1 to frames()
 *    codewords: set -F to the number of codewords of the packets.
 */
class AFF3CT_API codec_decoder : virtual public gr::fec::generic_decoder
{
public:
    typedef std::shared_ptr<codec_decoder> sptr;

    /*!
     * \param aff3ct_args AFF3CT command line of the codec, as for codec_encoder.
     * \param llr_scale   factor applied to the input LLRs (before quantization).
     */
    static sptr make(const std::string& aff3ct_args, float llr_scale = 1.0f);

    //! Number of frames decoded per call (the most, for a packet).
    virtual int frames() const = 0;
    //! Information bits per frame (K minus the CRC bits).
    virtual int info_bits() const = 0;
    //! Received LLRs per frame.
    virtual int coded_bits() const = 0;
};

} // namespace aff3ct
} // namespace gr

#endif /* INCLUDED_AFF3CT_CODEC_DECODER_H */
