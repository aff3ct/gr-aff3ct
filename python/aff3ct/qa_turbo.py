#!/usr/bin/env python3
#
# Copyright (c) 2026 aff3ct
#
# SPDX-License-Identifier: MIT
#

import numpy as np
from gnuradio import gr, gr_unittest, blocks, fec

try:
    from gnuradio.aff3ct import turbo_encoder, turbo_decoder
except ImportError:
    import os
    import sys
    dirname, filename = os.path.split(os.path.abspath(__file__))
    sys.path.append(os.path.join(dirname, "bindings"))
    from gnuradio.aff3ct import turbo_encoder, turbo_decoder


def run_chain(K, N, n_frames, ebn0_db=None, seed=0, interleaver="AUTO", itl_seed=0, **dec_kwargs):
    """bits -> extended encoder -> BPSK (0 -> +1) -> [AWGN] -> LLR -> extended decoder."""
    enc = turbo_encoder.make(K, N, interleaver, itl_seed)
    dec = turbo_decoder.make(K, N, interleaver=interleaver, seed=itl_seed, **dec_kwargs)
    n_frames = -(-n_frames // dec.frames()) * dec.frames()

    rng = np.random.default_rng(seed)
    bits = rng.integers(0, 2, K * n_frames).astype(np.uint8)

    tb = gr.top_block()
    src = blocks.vector_source_b(bits.tolist(), False)
    fenc = fec.extended_encoder(enc, threading=None, puncpat="11")
    snk_cw = blocks.vector_sink_b()
    tb.connect(src, fenc, snk_cw)
    tb.run()
    cw = np.array(snk_cw.data(), dtype=np.float32)
    assert len(cw) == N * n_frames

    y = 1.0 - 2.0 * cw
    if ebn0_db is not None:
        R = K / N
        sigma = np.sqrt(1.0 / (2.0 * R * 10 ** (ebn0_db / 10)))
        y = y + sigma * rng.standard_normal(len(y)).astype(np.float32)
        llr = 2.0 * y / sigma**2
    else:
        llr = 2.0 * y

    tb = gr.top_block()
    src = blocks.vector_source_f(llr.astype(np.float32).tolist(), False)
    fdec = fec.extended_decoder(dec, threading=None, puncpat="11")
    snk = blocks.vector_sink_b()
    tb.connect(src, fdec, snk)
    tb.run()
    out = np.array(snk.data(), dtype=np.uint8)
    assert len(out) == len(bits)
    fe = np.any((out != bits).reshape(n_frames, K), axis=1).sum()
    return int((out != bits).sum()), int(fe), n_frames


class qa_turbo(gr_unittest.TestCase):

    def test_001_noiseless_roundtrip(self):
        cases = [
            (424, 3 * 424 + 12, {}),                                   # mother code, LTE itl
            (424, 848, {}),                                            # punctured to 1/2
            (424, 600, {"precision": "float", "simd": "SEQ"}),         # punctured, float
            (333, 1500, {}),                                           # RANDOM itl, repetition
            (500, 1000, {"precision": "float", "simd": "INTER"}),
            (336, 1020, {"interleaver": "RANDOM", "itl_seed": 42}),   # RANDOM on an LTE size
        ]
        for K, N, kw in cases:
            be, fe, nf = run_chain(K, N, 64, **kw)
            self.assertEqual(be, 0, f"K={K} N={N} {kw}: {be} bit errors")

    def test_002_awgn(self):
        # rate 1/3, K = 424, 6 iterations: FER ~ 1e-2 at 1.5 dB with AFF3CT
        be, fe, nf = run_chain(424, 1284, 2048, ebn0_db=1.5)
        self.assertLess(fe / nf, 5e-2)

    def test_003_rate(self):
        enc = turbo_encoder.make(424, 848)
        dec = turbo_decoder.make(424, 848)
        self.assertAlmostEqual(fec.get_encoder_rate(enc), 0.5) if hasattr(fec, "get_encoder_rate") else None
        self.assertEqual(fec.get_encoder_output_size(enc), 848)
        self.assertEqual(fec.get_decoder_input_size(dec), 848 * dec.frames())
        self.assertEqual(fec.get_decoder_output_size(dec), 424 * dec.frames())

    def test_004_bad_N(self):
        with self.assertRaises(Exception):
            turbo_encoder.make(424, 424 + 11)


if __name__ == "__main__":
    gr_unittest.run(qa_turbo)
