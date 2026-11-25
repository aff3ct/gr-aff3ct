#!/usr/bin/env python3
#
# Copyright (c) 2026 aff3ct
#
# SPDX-License-Identifier: MIT
#
"""
Frame error rate of an AFF3CT code through GNU Radio (codec_encoder and codec_decoder in
the FEC Extended blocks, BPSK on AWGN), next to the AFF3CT simulator on the same command
line. The two FER should agree within their confidence intervals.

    ./fer_vs_simulator.py --ebn0 2.0 --simulator aff3ct -- -C LDPC --enc-type LDPC_5G \\
        -K 512 -N 1024 --dec-type BP_HORIZONTAL_LAYERED --dec-implem NMS --dec-norm 0.75 \\
        --dec-simd INTER -p 16

The simulator is run only if --simulator gives its executable.
"""

import argparse
import math
import re
import subprocess
import sys
import time

import numpy as np
from gnuradio import blocks, fec, gr
from gnuradio import aff3ct


def gr_fer(args, ebn0, max_fe, max_frames, seed):
    enc = aff3ct.codec_encoder.make(args)
    dec = aff3ct.codec_decoder.make(args)
    K, N, F = enc.info_bits(), enc.coded_bits(), dec.frames()
    sigma = math.sqrt(1.0 / (2.0 * K / N * 10 ** (ebn0 / 10)))
    batch = max(F, -(-2048 // F) * F)
    rng = np.random.default_rng(seed)
    n_fe = n_fra = 0
    while n_fe < max_fe and n_fra < max_frames:
        bits = rng.integers(0, 2, K * batch).astype(np.uint8)
        tb = gr.top_block()
        src = blocks.vector_source_b(bits, False)
        snk = blocks.vector_sink_b()
        tb.connect(src, fec.extended_encoder(enc, threading=None, puncpat="11"), snk)
        tb.run()
        y = 1.0 - 2.0 * np.array(snk.data(), dtype=np.float32)
        y += sigma * rng.standard_normal(len(y)).astype(np.float32)
        tb = gr.top_block()
        src = blocks.vector_source_f(2.0 * y / sigma**2, False)
        snk = blocks.vector_sink_b()
        tb.connect(src, fec.extended_decoder(dec, threading=None, puncpat="11"), snk)
        tb.run()
        out = np.array(snk.data(), dtype=np.uint8)
        n_fe += int(np.any((out != bits).reshape(batch, K), axis=1).sum())
        n_fra += batch
    return n_fe, n_fra


def sim_fer(simulator, args, ebn0, max_fe, max_frames, drop):
    words = args.split()
    for opt in drop:  # an option and its value
        while opt in words:
            i = words.index(opt)
            del words[i:i + 2]
    cmd = [simulator, "-m", str(ebn0), "-M", str(ebn0 + 0.01), "-e", str(max_fe),
           "--sim-max-fra", str(max_frames), "--sim-seed", "1"] + words
    out = subprocess.run(cmd, capture_output=True, text=True).stdout
    for line in out.splitlines():
        # | Es/N0 | Eb/N0 || FRA | BE | FE | BER | FER || ...
        m = re.match(r"\s*\S+\s*\|\s*(\S+)\s*\|\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|", line)
        if m:
            return int(m.group(4)), int(m.group(2))
    raise RuntimeError("no result from the simulator:\n" + out[-2000:])


def interval(fe, n):
    # 95 % Wilson interval of a proportion
    p, z = fe / n, 1.96
    c = (p + z * z / (2 * n)) / (1 + z * z / n)
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / (1 + z * z / n)
    return max(0.0, c - h), c + h


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ebn0", type=float, required=True)
    ap.add_argument("--max-fe", type=int, default=200)
    ap.add_argument("--max-frames", type=int, default=200000)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--simulator", help="AFF3CT simulator executable")
    ap.add_argument("--sim-drop", action="append", default=[], metavar="OPT",
                    help="option (and its value) not given to the simulator, e.g. "
                         "--enc-fb-noise: the simulator then designs the polar frozen bits "
                         "for the simulated SNR")
    ap.add_argument("args", nargs=argparse.REMAINDER, help="AFF3CT codec command line (after --)")
    o = ap.parse_args()
    args = " ".join(a for a in o.args if a != "--")

    t = time.time()
    fe, n = gr_fer(args, o.ebn0, o.max_fe, o.max_frames, o.seed)
    lo, hi = interval(fe, n)
    print(f"gr-aff3ct  FER {fe / n:.3e}  [{lo:.2e}, {hi:.2e}]  ({fe} / {n}, {time.time() - t:.0f} s)")
    if o.simulator:
        fe2, n2 = sim_fer(o.simulator, args, o.ebn0, o.max_fe, o.max_frames, o.sim_drop)
        lo2, hi2 = interval(fe2, n2)
        print(f"simulator  FER {fe2 / n2:.3e}  [{lo2:.2e}, {hi2:.2e}]  ({fe2} / {n2})")
        ok = lo <= hi2 and lo2 <= hi
        print("agree" if ok else "DISAGREE")
        sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
