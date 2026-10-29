/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#include "turbo_codec.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>

namespace gr {
namespace aff3ct {

namespace {

std::vector<std::string> split(const std::string& s)
{
    std::vector<std::string> words;
    std::istringstream iss(s);
    std::string w;
    while (iss >> w)
        words.push_back(w);
    return words;
}

template <typename B, typename Q>
std::unique_ptr<::aff3ct::tools::Codec_turbo<B, Q>> try_build(const turbo_params& p,
                                                             const std::string& itl)
{
    std::vector<std::string> args = { "gr-aff3ct",
                                      "-K",
                                      std::to_string(p.K),
                                      "--enc-sub-std",
                                      "LTE",
                                      "--itl-type",
                                      itl,
                                      "--itl-seed",
                                      std::to_string(p.seed),
                                      "-i",
                                      std::to_string(p.n_ite) };
    if (!p.simd.empty()) {
        args.push_back("--dec-sub-simd");
        args.push_back(p.simd);
    }
    for (auto& a : split(p.aff3ct_args))
        args.push_back(a);

    std::vector<char*> argv;
    for (auto& a : args)
        argv.push_back(const_cast<char*>(a.c_str()));

    ::aff3ct::factory::Codec_turbo params_cdc;
    std::vector<::aff3ct::factory::Factory*> factories = { &params_cdc };
    std::ostringstream err;
    ::aff3ct::tools::Command_parser cp((int)argv.size(), argv.data(), factories, false, err);
    if (cp.parsing_failed()) {
        cp.print_errors();
        throw std::invalid_argument("gr-aff3ct: invalid AFF3CT arguments:\n" + err.str());
    }

    std::unique_ptr<::aff3ct::tools::Codec_turbo<B, Q>> codec(params_cdc.template build<B, Q>());
    // set_seed() also generates the interleaver lookup table (throws if K is not supported)
    codec->get_interleaver().set_seed(p.seed);
    return codec;
}

} // namespace

template <typename B, typename Q>
std::unique_ptr<::aff3ct::tools::Codec_turbo<B, Q>> build_turbo_codec(const turbo_params& p,
                                                                      std::string& itl_used)
{
    if (p.interleaver == "AUTO") {
        try {
            itl_used = "LTE";
            return try_build<B, Q>(p, itl_used);
        } catch (std::exception&) {
            itl_used = "RANDOM";
            return try_build<B, Q>(p, itl_used);
        }
    }
    if (p.interleaver != "LTE" && p.interleaver != "RANDOM")
        throw std::invalid_argument("gr-aff3ct: interleaver must be AUTO, LTE or RANDOM");
    itl_used = p.interleaver;
    return try_build<B, Q>(p, itl_used);
}

template std::unique_ptr<::aff3ct::tools::Codec_turbo<int32_t, float>>
build_turbo_codec<int32_t, float>(const turbo_params&, std::string&);
template std::unique_ptr<::aff3ct::tools::Codec_turbo<int16_t, int16_t>>
build_turbo_codec<int16_t, int16_t>(const turbo_params&, std::string&);

} // namespace aff3ct
} // namespace gr
