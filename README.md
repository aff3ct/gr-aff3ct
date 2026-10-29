# gr-aff3ct

GNU Radio 3.10 out-of-tree module exposing [AFF3CT](https://github.com/aff3ct/aff3ct)
forward error correction codes as gr-fec encoder and decoder objects.

First code available: **turbo code with LTE constituent encoders** (two 8-state RSC
encoders {013, 015}, terminated trellises), **any K and any N**, decoded by the AFF3CT
SIMD BCJR decoders (max-log, int16 or float).

## How it plugs into a flowgraph

The objects follow the gr-fec API (`generic_encoder` / `generic_decoder`), so they are
used through the standard gr-fec deployment blocks, exactly like the CC or polar codes
of GNU Radio:

| Your chain | Encoder block | Decoder block |
|---|---|---|
| continuous stream, frames already aligned | FEC Extended Encoder | FEC Extended Decoder |
| tagged streams (`packet_len`) | FEC Tagged Encoder | FEC Tagged Decoder |
| PDUs | FEC Async Encoder | FEC Async Decoder |

In GRC: add an *AFF3CT Turbo Encoder Definition* and an *AFF3CT Turbo Decoder
Definition* variable (category AFF3CT), and give their ids to the deployment blocks.
`examples/aff3ct_turbo_ber.grc` is a complete BER test on AWGN.

In Python:

```python
from gnuradio import aff3ct, fec
enc = aff3ct.turbo_encoder.make(K=424, N=848)                  # rate 1/2
dec = aff3ct.turbo_decoder.make(K=424, N=848, n_ite=6)
encoder = fec.extended_encoder(enc, threading=None, puncpat="11")
decoder = fec.extended_decoder([aff3ct.turbo_decoder.make(424, 848) for _ in range(4)],
                               threading="capillary", puncpat="11")
```

### Formats

* Encoder: input K unpacked bits (one bit per byte, 0 or 1), output N unpacked bits.
* Decoder: input float LLRs, output unpacked bits.
  **AFF3CT convention: LLR = log(P(b=0) / P(b=1)), positive means bit 0.** For BPSK
  mapping 0 -> +1 on AWGN, LLR = 2y/sigma². With the opposite mapping, negate the LLRs.
* The decoder works on **several frames per call** (`frames`, 0 = SIMD width:
  32 in int16 with AVX-512, 16 with AVX2, 8 with SSE or NEON): its input item is
  `frames * N` LLRs and its output `frames * K` bits. With the Extended blocks this is
  transparent. With the Tagged / Async blocks, a packet must hold `frames` codewords,
  or use `frames=1` (slower, see below).

### Code parameters

* **K**: any value. Interleaver `AUTO` uses the LTE interleaver when K is an LTE size
  (40 to 6144), a random interleaver with a fixed seed otherwise. Encoder and decoder
  must use the same K, N, interleaver and seed.
* **N**: any value with N >= K + 12. The mother code has N_cw = 3K + 12 bits. For
  N < N_cw the parity bits are uniformly punctured (alternately in both encoders;
  systematic and tail bits are always sent). For N > N_cw the codeword is cyclically
  repeated and the decoder adds the LLRs of the copies. This is not the LTE rate
  matching (no sub-block interleaver, no circular buffer).
* Decoder: `n_ite` (default 6), `precision` (`int16`, default, or `float`), `simd`
  (`INTER`, default and fastest; `INTRA`, in int16 only with 128-bit SIMD, i.e. SSE or
  NEON, and with K multiple of 8; `SEQ`),
  `llr_scale` (int16 only: LLRs are multiplied by it, then quantized on 6 bits with
  3 fractional bits, saturating at ±3.875, as in the AFF3CT simulator).
* `aff3ct_args`: any extra AFF3CT command line option of the turbo codec, applied
  last, e.g. `--dec-sf-type LTE_VEC` (extrinsic scaling) or `--dec-sub-max MAXS`.

### Multithreading

Give a list of decoder objects to the FEC Extended Decoder (Parallelism 1, Dimension 1
= number of decoders) and choose `capillary` (power of 2) or `ordinary` threading.
Each object owns its AFF3CT modules: nothing is shared between threads.

## Performance

AMD Ryzen 9 9950X3D, K = 424, N = 1284 (rate 1/3), 6 iterations, int16, measured through
GNU Radio (vector source, FEC Extended Decoder, null sink), information throughput:

| SIMD build | Frames per call | 1 decoder | 2 decoders | 4 decoders |
|---|---|---|---|---|
| SSE4.2 (`-msse4.2`) | 8 | 40 Mb/s | 81 Mb/s | 160 Mb/s |
| AVX2 (`-mavx2 -mfma`) | 16 | 71 Mb/s | 145 Mb/s | 271 Mb/s |
| AVX-512 (`-march=native`) | 32 | 70 Mb/s | 132 Mb/s | 262 Mb/s |

(multiple decoders: `capillary` threading.) INTRA, SSE4.2, 1 decoder: 15 Mb/s.

Error correction is identical to the AFF3CT simulator (`aff3ct -C TURBO -K 424
--enc-sub-std LTE --dec-sub-simd INTER -p 16 -i 6`), e.g. FER 6e-1 at Eb/N0 = 0.5 dB,
1.4e-1 at 1 dB, 7e-3 to 8e-3 at 1.5 dB.

## Build and install

Requirements: GNU Radio 3.10 with its development files, CMake, a C++17 compiler,
pybind11.

```bash
sudo apt install gnuradio-dev pybind11-dev python3-pybind11 cmake g++ git
```

### 1. AFF3CT as a shared library

```bash
git clone --recursive https://github.com/aff3ct/aff3ct.git
cd aff3ct
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-march=native -faligned-new" \
      -DAFF3CT_COMPILE_EXE=OFF -DAFF3CT_COMPILE_SHARED_LIB=ON \
      -DCMAKE_INSTALL_PREFIX=$HOME/.local/aff3ct
cmake --build build -j$(nproc)
cmake --install build
```

`-march=native` selects the best SIMD instruction set of the machine (SSE4.2, AVX2 or
AVX-512 on x86, NEON on ARM). To target a given instruction set instead, e.g. to run the
same binaries on several machines, replace it with `-msse4.2` or `-mavx2 -mfma`.

**gr-aff3ct must be compiled with the same SIMD flags as AFF3CT**: the register width
fixes the number of frames decoded together and the buffer alignment. gr-aff3ct uses
`-march=native` by default; otherwise pass `-DGR_AFF3CT_NATIVE=OFF
-DCMAKE_CXX_FLAGS="-mavx2 -mfma"` (same flags as AFF3CT). A mismatch is detected when
a decoder is created ("AFF3CT and gr-aff3ct were built with different SIMD instruction
sets"). The three x86 variants (SSE4.2, AVX2, AVX-512) pass the QA tests.

### 2. gr-aff3ct

```bash
cd gr-aff3ct
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=$HOME/.local/aff3ct \
      -DCMAKE_INSTALL_PREFIX=/usr/local      # or $HOME/.local, see below
cmake --build build -j$(nproc)
sudo cmake --install build && sudo ldconfig
cd build && ctest -V                         # QA tests
```

The installed libraries carry an RPATH, so `LD_LIBRARY_PATH` is not needed.

Installing in `$HOME/.local` (no sudo): Python finds the module through the user
site-packages, but GRC must be told where the blocks are, in
`~/.config/gnuradio/config.conf` (absolute path, `~` is not expanded by GRC):

```ini
[grc]
local_blocks_path = /home/<user>/.local/share/gnuradio/grc/blocks
```

### Raspberry Pi

Same procedure on a 64-bit OS (Raspberry Pi OS 64-bit or Ubuntu arm64), natively on the
Pi: `-march=native` selects NEON. Building AFF3CT takes a while; use
`-j2` or `-j3` on a Pi with 4 GB of RAM or less. Not tested yet.

## Tests

`python/aff3ct/qa_turbo.py` (run by `ctest`): noiseless round trips through the gr-fec
Extended blocks (LTE and random interleavers, puncturing, repetition, int16 and float,
INTER and SEQ), a BER check on AWGN, sizes reported to gr-fec, rejection of N < K + 12.

## License

MIT for the gr-aff3ct code. The build scaffolding generated by `gr_modtool` from the
GNU Radio templates keeps its GPL-3.0-or-later header. See [LICENSE](LICENSE).
