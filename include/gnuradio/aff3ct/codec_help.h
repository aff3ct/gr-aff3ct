/* -*- c++ -*- */
/*
 * Copyright (c) 2026 aff3ct
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDED_AFF3CT_CODEC_HELP_H
#define INCLUDED_AFF3CT_CODEC_HELP_H

#include <gnuradio/aff3ct/api.h>
#include <string>

namespace gr {
namespace aff3ct {

/*!
 * \brief Help of the AFF3CT command line options accepted by codec_encoder and
 * codec_decoder for a code type (e.g. "POLAR", "LDPC"), as printed by the simulator.
 * \ingroup aff3ct
 */
AFF3CT_API std::string codec_help(const std::string& code);

} // namespace aff3ct
} // namespace gr

#endif /* INCLUDED_AFF3CT_CODEC_HELP_H */
