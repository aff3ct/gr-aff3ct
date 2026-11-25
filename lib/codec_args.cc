/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#include "codec_args.h"
#include <gnuradio/aff3ct/codec_help.h>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace gr {
namespace aff3ct {

namespace {

namespace fa = ::aff3ct::factory;

// words separated by spaces; quotes keep the spaces of a word (--dec-h-path "my file.alist")
std::vector<std::string> split(const std::string& s)
{
    std::vector<std::string> words;
    std::string w;
    bool in_word = false;
    char quote = 0;
    for (char c : s) {
        if (quote) {
            if (c == quote)
                quote = 0;
            else
                w += c;
        } else if (c == '"' || c == '\'') {
            quote = c;
            in_word = true;
        } else if (std::isspace((unsigned char)c)) {
            if (in_word)
                words.push_back(w);
            w.clear();
            in_word = false;
        } else {
            w += c;
            in_word = true;
        }
    }
    if (quote)
        throw std::invalid_argument("gr-aff3ct: a quote is not closed in the AFF3CT arguments");
    if (in_word)
        words.push_back(w);
    return words;
}

// the codec factory of each -C value, as in factory::Launcher (puncturers as in BFER_std)
fa::Codec_SIHO* make_codec_factory(const std::string& code)
{
    if (code == "POLAR") {
        auto f = new fa::Codec_polar();
        f->enable_puncturer();
        return f;
    }
    if (code == "TURBO") {
        auto f = new fa::Codec_turbo();
        f->enable_puncturer();
        return f;
    }
    if (code == "LDPC") {
        auto f = new fa::Codec_LDPC();
        f->enable_puncturer();
        return f;
    }
    if (code == "TURBO_DB") {
        auto f = new fa::Codec_turbo_DB();
        f->enable_puncturer();
        return f;
    }
    if (code == "POLAR_MK")
        return new fa::Codec_polar_MK();
    if (code == "POLAR_PAC")
        return new fa::Codec_polar_PAC();
    if (code == "POLAR_PT")
        return new fa::Codec_polar_PT();
    if (code == "TPC")
        return new fa::Codec_turbo_product();
    if (code == "RSC")
        return new fa::Codec_RSC();
    if (code == "RSC_DB")
        return new fa::Codec_RSC_DB();
    if (code == "CONV")
        return new fa::Codec_conv();
    if (code == "REP")
        return new fa::Codec_repetition();
    if (code == "RA")
        return new fa::Codec_RA();
    if (code == "BCH")
        return new fa::Codec_BCH();
    if (code == "RS")
        return new fa::Codec_RS();
    if (code == "UNCODED")
        return new fa::Codec_uncoded();
    throw std::invalid_argument(
        "gr-aff3ct: unknown code type '-C " + code +
        "' (POLAR, POLAR_MK, POLAR_PAC, POLAR_PT, TURBO, TURBO_DB, TPC, LDPC, REP, RA, RSC, "
        "RSC_DB, CONV, BCH, RS or UNCODED)");
}

// fixed point format of the AFF3CT simulator, see the store_args() of src/Launcher/Code/*
void set_quantizer_defaults(fa::Quantizer& qnt, const std::string& code, bool int8)
{
    if (code == "POLAR" || code == "POLAR_MK" || code == "POLAR_PAC" || code == "POLAR_PT") {
        qnt.n_bits = 6;
        qnt.n_decimals = 1;
    } else if (code == "REP" || code == "LDPC") {
        qnt.n_bits = 6;
        qnt.n_decimals = 2;
    } else if (code == "TURBO" || code == "TURBO_DB") {
        qnt.n_bits = 6;
        qnt.n_decimals = int8 ? 2 : 3;
    } else if (code == "TPC") {
        qnt.n_bits = int8 ? 6 : 8;
        qnt.n_decimals = int8 ? 2 : 3;
    } else if (code == "RSC" || code == "RSC_DB") {
        qnt.n_bits = 6;
        qnt.n_decimals = int8 ? 1 : 3;
    }
}

// The simulator generates the polar frozen bits for each SNR (codec->set_noise()) unless
// --enc-fb-noise is given. A flowgraph has no SNR: the frozen bits are generated once, from
// the design noise, by the same set_noise() call (the codec is kept adaptive: Codec_polar_MK
// does not support a fixed noise, its noise object goes out of scope before generate()).
template <class F>
void read_design_noise(fa::Codec_SIHO* cdc, float& noise, bool& noise_ep)
{
    auto f = dynamic_cast<F*>(cdc);
    if (f == nullptr || f->fbg == nullptr)
        return;
    const auto& type = f->fbg->type;
    noise_ep = type == "BEC";
    if (f->fbg->noise != -1.f) {
        noise = f->fbg->noise;
        f->fbg->noise = -1.f;
        return;
    }
    const auto opt = "--" + f->fbg->get_prefix();
    if (type == "GA" || type == "TV" || type == "BEC")
        throw std::invalid_argument("gr-aff3ct: the frozen bits of a polar code must be fixed: add " +
                                    opt + "-noise <sigma> (design noise of the '" + type +
                                    "' construction), or use " + opt + "-gen-method 5G");
    noise = 1.f; // not used by this construction
}

// The options accepted for a codec: those of the codec, the CRC and the quantizer, as in
// the simulator, without what the gr-aff3ct objects deduce or set themselves.
void describe(const std::vector<fa::Factory*>& factories,
              const fa::CRC& crc,
              const fa::Quantizer& qnt,
              cli::Argument_map_info& infos)
{
    fa::Factory::get_description(factories, infos);
    // deduced from the codec, as in the simulator (src/Launcher/Simulation/BFER_std.cpp)
    infos.erase({ crc.get_prefix() + "-info-bits", "K" });
    infos.erase({ qnt.get_prefix() + "-size", "N" });
    // the number of frames is a parameter of the gr-aff3ct objects
    std::vector<cli::Argument_tag> fra;
    for (auto& i : infos)
        if (i.first[0].size() > 4 && i.first[0].compare(i.first[0].size() - 4, 4, "-fra") == 0)
            fra.push_back(i.first);
    for (auto& t : fra)
        infos.erase(t);
}

} // namespace

std::string codec_help(const std::string& code)
{
    std::unique_ptr<fa::Codec_SIHO> cdc(make_codec_factory(code));
    fa::CRC crc;
    fa::Quantizer qnt;
    std::vector<fa::Factory*> factories = { cdc.get(), &crc, &qnt };
    cli::Argument_map_info infos;
    describe(factories, crc, qnt, infos);
    std::ostringstream help;
    const std::string usage = "-C " + code; // "Usage: -C POLAR -N <integer> -K <integer> ..."
    const char* argv[] = { usage.c_str() };
    cli::Argument_handler handler(1, argv, help);
    handler.print_help(infos, fa::Factory::create_groups(factories));
    return help.str();
}

codec_args::codec_args(const std::string& args) : precision("float")
{
    // AFF3CT reads some data files (e.g. the 5G polar sequence) from the directories of
    // AFF3CT_CLI_PATH, by default from the directory of the executable (python here)
    if (std::string(GR_AFF3CT_DATA_DIR).size() > 0)
        setenv("AFF3CT_CLI_PATH", GR_AFF3CT_DATA_DIR, 0);

    // the code type selects the factory, the precision the types of the modules and -F
    // the size of the buffers: they are read here, AFF3CT reads the rest
    std::vector<std::string> words = { "gr-aff3ct" };
    auto in = split(args);
    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == "-C" || in[i] == "--sim-cde-type") {
            if (i + 1 == in.size())
                throw std::invalid_argument("gr-aff3ct: -C needs a code type");
            code = in[++i];
        } else if (in[i] == "-p" || in[i] == "--sim-prec") {
            const std::string p = i + 1 < in.size() ? in[++i] : "";
            if (p != "8" && p != "16" && p != "32")
                throw std::invalid_argument("gr-aff3ct: -p must be 8, 16 (fixed point) or 32 (float)");
            precision = p == "8" ? "int8" : p == "16" ? "int16" : "float";
        } else if (in[i] == "-F" || in[i] == "--sim-inter-fra") {
            const std::string f = i + 1 < in.size() ? in[++i] : "";
            if (f.empty() || f.size() > 6 || f.find_first_not_of("0123456789") != std::string::npos ||
                std::stoi(f) == 0)
                throw std::invalid_argument("gr-aff3ct: -F needs a number of frames (1 or more)");
            frames = std::stoi(f);
        } else
            words.push_back(in[i]);
    }
    if (code.empty())
        throw std::invalid_argument("gr-aff3ct: the code type is missing (-C POLAR, -C TURBO...)");

    cdc.reset(make_codec_factory(code));
    if (precision != "float")
        set_quantizer_defaults(qnt, code, precision == "int8");
    // the SIMD quantizer (the default of the simulator, STD, is slower than some decoders)
    qnt.implem = "FAST";

    std::vector<fa::Factory*> factories = { cdc.get(), &crc, &qnt };
    cli::Argument_map_info infos;
    describe(factories, crc, qnt, infos);

    std::vector<const char*> argv;
    for (auto& w : words)
        argv.push_back(w.c_str());
    std::ostringstream help;
    cli::Argument_handler handler((int)argv.size(), argv.data(), help);
    std::vector<std::string> warnings, errors;
    auto vals = handler.parse_arguments(infos, warnings, errors);
    // the only warnings are unknown arguments: a typo must not be ignored silently
    for (auto& w : warnings)
        errors.push_back(w + " (only the options of the codec, --crc-*, --qnt-*, -p and -F are accepted)");
    if (errors.empty()) {
        try {
            fa::Factory::store(factories, vals);
        } catch (std::exception& e) {
            errors.push_back(e.what());
        }
    }
    if (!errors.empty()) {
        std::string msg = "gr-aff3ct: invalid AFF3CT arguments for '-C " + code + "':";
        for (auto& e : errors)
            msg += "\n  " + e;
        throw std::invalid_argument(msg);
    }

    // encoders of the simulator that do not encode the information bits (they need the
    // matching source, or the decoder to know the transmitted codeword)
    if (cdc->enc->type == "AZCW" || cdc->enc->type == "COSET" || cdc->enc->type == "USER")
        throw std::invalid_argument("gr-aff3ct: the '" + cdc->enc->type +
                                    "' encoder only makes sense in the AFF3CT simulator");

    // AFF3CT builds the Galois field of a BCH code in the type of the bits: GF(2^8) and
    // larger do not fit in 8 bits (out of bounds writes)
    if (precision == "int8" && (code == "BCH" || code == "TPC")) {
        auto tpc = dynamic_cast<fa::Encoder_turbo_product*>(cdc->enc.get());
        const int N_bch = tpc ? tpc->sub->N_cw : cdc->N_cw;
        if (N_bch >= 255)
            throw std::invalid_argument("gr-aff3ct: the BCH codes of length 255 and more do not "
                                        "support the 8-bit precision, use -p 16 or float");
    }

    crc.K = cdc->K - crc_size();
    if (crc.K <= 0)
        throw std::invalid_argument("gr-aff3ct: the CRC is larger than K");
    qnt.size = cdc->N;

    read_design_noise<fa::Codec_polar>(cdc.get(), fb_noise, fb_noise_ep);
    read_design_noise<fa::Codec_polar_PAC>(cdc.get(), fb_noise, fb_noise_ep);
    read_design_noise<fa::Codec_polar_PT>(cdc.get(), fb_noise, fb_noise_ep);
    read_design_noise<fa::Codec_polar_MK>(cdc.get(), fb_noise, fb_noise_ep);
}

int codec_args::itl_seed() const
{
    if (cdc->itl == nullptr || cdc->itl->core == nullptr)
        return -1;
    return cdc->itl->core->seed;
}

} // namespace aff3ct
} // namespace gr
