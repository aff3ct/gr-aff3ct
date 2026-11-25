#!/usr/bin/env python3
#
# Copyright (c) 2026 aff3ct
#
# SPDX-License-Identifier: MIT
#
"""
Information throughput of an AFF3CT decoder through GNU Radio: vector source (noisy LLRs
of 1024 real codewords, repeated) -> FEC Extended Decoder -> null sink, best of 3 runs.

    ./decoder_throughput.py --decoders 1 2 4 -- -C LDPC --enc-type LDPC_5G -K 512 -N 1024 \\
        --dec-type BP_HORIZONTAL_LAYERED --dec-implem NMS --dec-norm 0.75 --dec-simd INTER -p 16
"""

import argparse
import math
import time

import numpy as np
from gnuradio import aff3ct, blocks, fec, gr


def llrs(args, n_frames, ebn0, seed=1):
    enc = aff3ct.codec_encoder.make(args)
    K, N = enc.info_bits(), enc.coded_bits()
    rng = np.random.default_rng(seed)
    tb = gr.top_block()
    src = blocks.vector_source_b(rng.integers(0, 2, K * n_frames).astype(np.uint8), False)
    snk = blocks.vector_sink_b()
    tb.connect(src, fec.extended_encoder(enc, threading=None, puncpat="11"), snk)
    tb.run()
    sigma = math.sqrt(1.0 / (2.0 * K / N * 10 ** (ebn0 / 10)))
    y = 1.0 - 2.0 * np.array(snk.data(), dtype=np.float32)
    y += sigma * rng.standard_normal(len(y)).astype(np.float32)
    return K, N, (2.0 * y / sigma**2).astype(np.float32)


def throughput(args, n_dec, llr, K, N, passes):
    decs = [aff3ct.codec_decoder.make(args) for _ in range(n_dec)]
    threading = None if n_dec == 1 else "capillary"
    best = 0.0
    for _ in range(3):
        tb = gr.top_block()
        src = blocks.vector_source_f(llr, True)
        head = blocks.head(gr.sizeof_float, len(llr) * passes)
        snk = blocks.null_sink(gr.sizeof_char)
        dec = fec.extended_decoder(decs[0] if n_dec == 1 else decs, threading=threading, puncpat="11")
        tb.connect(src, head, dec, snk)
        t = time.perf_counter()
        tb.run()
        best = max(best, len(llr) * passes / N * K / (time.perf_counter() - t) / 1e6)
    return best, decs[0].frames()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--decoders", type=int, nargs="+", default=[1, 2, 4])
    ap.add_argument("--ebn0", type=float, default=2.0)
    ap.add_argument("--passes", type=int, default=8, help="passes over 1024 frames per run")
    ap.add_argument("args", nargs=argparse.REMAINDER, help="AFF3CT codec command line (after --)")
    o = ap.parse_args()
    args = " ".join(a for a in o.args if a != "--")

    K, N, llr = llrs(args, 1024, o.ebn0)
    for n in o.decoders:
        mbps, F = throughput(args, n, llr, K, N, o.passes)
        print(f"{n} decoder(s), {F} frames per call: {mbps:.1f} Mb/s", flush=True)


if __name__ == "__main__":
    main()
