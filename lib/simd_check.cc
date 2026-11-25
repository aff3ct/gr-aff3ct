/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#include "codec_modules.h"
#include <cstdint>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace gr {
namespace aff3ct {

namespace {

// Lanes of an INTER decoder built by libaff3ct (one frame per SIMD lane), 0 if the
// library has no INTER decoder (build without SIMD).
size_t libaff3ct_lanes()
{
    try {
        codec_args a("-C RSC -K 32 --dec-simd INTER -p 16");
        codec_modules<int16_t, int16_t> m(a, 1);
        return m.codec->get_decoder_siho().get_n_frames_per_wave();
    } catch (std::exception&) {
        return 0;
    }
}

std::string g_error;

void check()
{
    const size_t lib = libaff3ct_lanes();
    const size_t own = mipp::N<int16_t>();
    if (lib == 0 || lib == own)
        return;
    std::stringstream msg;
    msg << "gr-aff3ct: AFF3CT and gr-aff3ct were built for different SIMD instruction sets "
           "(gr-aff3ct: "
        << mipp::InstructionFullType << ", " << own << " lanes of int16; libaff3ct: " << lib
        << " lanes). Build gr-aff3ct with the SIMD flags of libaff3ct: -march=native for "
           "both, or, with a libaff3ct that has glibc-hwcaps variants (packages), the same "
           "levels (GR_AFF3CT_HWCAPS, detected by default).";
    g_error = msg.str();
}

} // namespace

void check_simd_build()
{
    static std::once_flag once;
    std::call_once(once, check);
    if (!g_error.empty())
        throw std::runtime_error(g_error);
}

} // namespace aff3ct
} // namespace gr
