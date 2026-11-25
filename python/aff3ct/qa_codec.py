#!/usr/bin/env python3
#
# Copyright (c) 2026 aff3ct
#
# SPDX-License-Identifier: MIT
#

import numpy as np
import pmt
from gnuradio import gr, gr_unittest, blocks, fec

try:
    from gnuradio.aff3ct import codec_encoder, codec_decoder, codec_help
except ImportError:
    import os
    import sys
    dirname, filename = os.path.split(os.path.abspath(__file__))
    sys.path.append(os.path.join(dirname, "bindings"))
    from gnuradio.aff3ct import codec_encoder, codec_decoder, codec_help


def run_chain(args, n_frames, ebn0_db=None, seed=0):
    """bits -> extended encoder -> BPSK (0 -> +1) -> [AWGN] -> LLR -> extended decoder."""
    enc = codec_encoder.make(args)
    dec = codec_decoder.make(args)
    K, N = enc.info_bits(), enc.coded_bits()
    n_frames = -(-n_frames // dec.frames()) * dec.frames()

    rng = np.random.default_rng(seed)
    bits = rng.integers(0, 2, K * n_frames).astype(np.uint8)

    tb = gr.top_block()
    src = blocks.vector_source_b(bits, False)
    snk_cw = blocks.vector_sink_b()
    tb.connect(src, fec.extended_encoder(enc, threading=None, puncpat="11"), snk_cw)
    tb.run()
    cw = np.array(snk_cw.data(), dtype=np.float32)
    assert len(cw) == N * n_frames

    y = 1.0 - 2.0 * cw
    if ebn0_db is not None:
        sigma = np.sqrt(1.0 / (2.0 * K / N * 10 ** (ebn0_db / 10)))
        y = y + sigma * rng.standard_normal(len(y)).astype(np.float32)
        llr = 2.0 * y / sigma**2
    else:
        llr = 2.0 * y

    tb = gr.top_block()
    src = blocks.vector_source_f(llr.astype(np.float32), False)
    snk = blocks.vector_sink_b()
    tb.connect(src, fec.extended_decoder(dec, threading=None, puncpat="11"), snk)
    tb.run()
    out = np.array(snk.data(), dtype=np.uint8)
    assert len(out) == len(bits)
    fe = np.any((out != bits).reshape(n_frames, K), axis=1).sum()
    return int((out != bits).sum()), int(fe), n_frames


def run_tagged(enc, dec, K, sizes, seed=0):
    """Packets of sizes[i] frames through the Tagged blocks, without noise: (bits, decoded)."""
    bits = np.random.default_rng(seed).integers(0, 2, K * sum(sizes)).astype(np.uint8)
    tags, offset = [], 0
    for n in sizes:
        tags.append(gr.tag_utils.python_to_tag(
            (offset, pmt.intern("packet_len"), pmt.from_long(K * n))))
        offset += K * n
    mtu = K * max(sizes) // 8 + 1
    tb = gr.top_block()
    src = blocks.vector_source_b(bits.tolist(), False, 1, tags)
    snk = blocks.vector_sink_b()
    tb.connect(src,
               fec.tagged_encoder(enc, gr.sizeof_char, gr.sizeof_char, "packet_len", mtu),
               blocks.char_to_float(),
               blocks.multiply_const_ff(-4.0),  # bit 0 -> LLR +2, bit 1 -> LLR -2
               blocks.add_const_ff(2.0),
               fec.tagged_decoder(dec, gr.sizeof_float, gr.sizeof_char, "packet_len", mtu),
               snk)
    tb.run()
    return bits, np.array(snk.data(), dtype=np.uint8)


# command lines; the 5G LDPC base graphs are AFF3CT data files (conf/), installed
# with the AFF3CT simulator: these cases are skipped when the files are not installed
NOISELESS = [
    ("-C POLAR -K 512 -N 1024 --enc-fb-noise 0.7"),
    ("-C POLAR -K 120 -N 160 --enc-fb-noise 0.7 --pct-type SHORTLAST --dec-type ASCL -L 8 "
     "--crc-poly 24-LTEA"),
    ("-C POLAR -K 512 -N 1024 --enc-fb-noise 0.7 --dec-simd INTRA -p 8"),
    ("-C TURBO -K 424 --enc-sub-std LTE -i 6 --dec-sub-simd INTER -p 16"),
    ("-C TURBO -K 1784 --enc-sub-std CCSDS -i 10 --dec-sub-max MAXS"),
    ("-C LDPC --enc-type LDPC_5G -K 512 -N 1024 --qnt-bits 10 --qnt-dec 4 "
     "--dec-type BP_HORIZONTAL_LAYERED_LEGACY --dec-no-synd -i 10 --dec-implem OMS "
     "--dec-off 8 --dec-simd INTER -p 16"),
    ("-C LDPC --enc-type LDPC_DVBS2 -K 7200 -N 16200"),
    ("-C BCH -N 255 -K 239 -T 2"),
    ("-C RS -N 127 -K 101 -T 13"),
    ("-C REP -K 128 -N 512 -p 8"),
    ("-C RA -K 256 -N 1024 --itl-type RANDOM"),
    ("-C RSC -K 1024 --dec-simd INTER -p 16"),
    ("-C RSC_DB -K 1024 --enc-std DVB-RCS2 -p 8"),
    ("-C TURBO_DB -K 1504 -N 1880 --enc-sub-std DVB-RCS2 -i 8 --crc-poly 32-GZIP -p 16"),
    ("-C CONV --enc-poly {0171,0133} --dec-type VITERBI -K 64"),
    ("-C POLAR_PAC -N 128 -K 64 --enc-fb-gen-method RM -D FANO --dec-perr-snr 2.35 "
     "--dec-delta 1 --enc-conv 0o33 --dec-implem NAIVE"),
    ("-C UNCODED -K 256"),
]


class qa_codec(gr_unittest.TestCase):

    def test_001_noiseless_roundtrip(self):
        for args in NOISELESS:
            try:
                be, fe, nf = run_chain(args, 16)
            except RuntimeError as e:
                if "couldn't be opened" in str(e) or "does not exist" in str(e):
                    print(f"skipped (AFF3CT data files not installed): {args}")
                    continue
                raise
            self.assertEqual(be, 0, f"{args}: {be} bit errors")

    def test_002_awgn(self):
        # FER of the AFF3CT simulator on the same command line (examples/fer_vs_simulator.py)
        cases = [
            ("-C TURBO -K 424 --enc-sub-std LTE -i 6 --dec-sub-simd INTER -p 16", 1.5, 3e-2),
            ("-C POLAR -K 512 -N 1024 --enc-fb-noise 0.7 --dec-type SCL -L 8 --crc-poly 16-5G",
             2.0, 3e-2),
            ("-C BCH -N 255 -K 239 -T 2", 7.0, 3e-2),
        ]
        for args, ebn0, fer_max in cases:
            be, fe, nf = run_chain(args, 2048, ebn0_db=ebn0)
            self.assertLess(fe / nf, fer_max, f"{args}: FER {fe / nf}")
            self.assertGreater(fe, 0, f"{args}: no frame error, the noise is not applied")

    def test_003_sizes(self):
        args = "-C POLAR -K 528 -N 1024 --enc-fb-noise 0.7 --crc-poly 16-5G"
        enc = codec_encoder.make(args)
        dec = codec_decoder.make(args + " -F 4")
        self.assertEqual(enc.info_bits(), 512)  # K counts the CRC bits, as in AFF3CT
        self.assertEqual(enc.coded_bits(), 1024)
        self.assertEqual(dec.frames(), 4)
        self.assertEqual(fec.get_encoder_input_size(enc), 512)
        self.assertEqual(fec.get_encoder_output_size(enc), 1024)
        self.assertEqual(fec.get_decoder_input_size(dec), 4 * 1024)
        self.assertEqual(fec.get_decoder_output_size(dec), 4 * 512)

    def test_004_frames(self):
        # the encoder works on one frame, the decoder on several: the RANDOM interleaver
        # must be the same in both
        for frames in (1, 3, 8):
            be, fe, nf = run_chain(f"-C RA -K 256 -N 1024 --itl-type RANDOM --itl-seed 7 -F {frames}",
                                   24)
            self.assertEqual(be, 0, f"RA, {frames} frames per call: {be} bit errors")
        be, fe, nf = run_chain("-C TURBO -K 424 --enc-sub-std LTE --dec-sub-simd INTER -p 16 -F 5",
                               24)
        self.assertEqual(be, 0)

    def test_005_errors(self):
        with self.assertRaises(Exception):
            codec_encoder.make("-K 512 -N 1024")  # no code type
        with self.assertRaises(Exception):
            codec_encoder.make("-C FOO -K 512")
        with self.assertRaises(Exception):
            codec_encoder.make("-C POLAR -K 512 -N 1024 --enc-fb-noise 0.7 --foo 3")
        with self.assertRaises(Exception):
            codec_decoder.make("-C POLAR -K 512 -N 1024")  # frozen bits not fixed
        with self.assertRaises(Exception):
            codec_decoder.make("-C BCH -N 255 -K 239 -T 2 -p 64")  # no double precision
        with self.assertRaises(Exception):
            codec_decoder.make("-C BCH -N 255 -K 239 -T 2 -F 0")
        with self.assertRaises(Exception):
            codec_decoder.make("-C BCH -N 255 -K 239 -T 2 -m 3.0")  # option of the simulation
        with self.assertRaises(Exception):
            # GF(2^8) does not fit in 8 bits (out of bounds writes in AFF3CT)
            codec_decoder.make("-C BCH -N 255 -K 239 -T 2 -p 8")
        with self.assertRaises(Exception):
            # the COSET encoder of the simulator does not encode the information bits
            codec_encoder.make("-C POLAR -K 512 -N 1024 --enc-fb-noise 0.7 --enc-type COSET")
        with self.assertRaises(Exception):
            codec_encoder.make('-C POLAR -K 512 -N 1024 --enc-fb-noise "0.7')  # open quote
        self.assertEqual(
            codec_encoder.make('-C POLAR -K 512 -N 1024 --enc-fb-noise "0.7"').info_bits(), 512)

    def test_007_packets(self):
        # Tagged blocks: a packet holds one or several frames, at most frames() to decode
        for args in (
            "-C POLAR -K 256 -N 512 --enc-fb-noise 0.7 --dec-type SCL -L 4 --crc-poly 16-CCITT -F 4",
            "-C RSC -K 64 --dec-simd INTER -p 16",
            "-C BCH -N 127 -K 92 -T 5 -F 5",
        ):
            enc = codec_encoder.make(args)
            dec = codec_decoder.make(args)
            sizes = [1, 3, dec.frames(), 2, 1, min(4, dec.frames())]
            bits, out = run_tagged(enc, dec, enc.info_bits(), sizes)
            self.assertEqual(len(out), len(bits), args)
            self.assertEqual(int((out != bits).sum()), 0, args)

    def test_006_help(self):
        h = codec_help("POLAR")
        self.assertIn("Usage: -C POLAR", h)
        for opt in ("--dec-type", "--enc-fb-noise", "--crc-poly", "--qnt-bits"):
            self.assertIn(opt, h)
        self.assertNotIn("--enc-fra", h)  # set by the gr-aff3ct objects
        with self.assertRaises(ValueError):
            codec_help("FOO")


if __name__ == "__main__":
    gr_unittest.run(qa_codec)
