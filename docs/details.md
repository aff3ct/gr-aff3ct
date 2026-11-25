# gr-aff3ct: details

## Formats and frames

* Encoder: input unpacked information bits (one bit per byte, 0 or 1), output N
  unpacked bits.
* Decoder: input float LLRs, output unpacked information bits.
  **AFF3CT convention: LLR = log(P(b=0) / P(b=1)), positive means bit 0.** For BPSK
  mapping 0 -> +1 on AWGN, LLR = 2y/sigma². With the opposite mapping, negate the LLRs.
* The decoder works on **several frames per call**, `-F` in the command line as in the
  simulator: its input item is `F * N` LLRs and its output `F * K` bits. Without `-F`,
  the decoder takes at least 16 frames, a whole number of SIMD waves.
  A number of frames that is not a multiple of the SIMD width works, the INTER decoders
  then also decode padding frames.
* With the **Extended** blocks (continuous stream) this is transparent. With the
  **Tagged** and **Async** blocks, a packet holds one or several whole frames: any number
  for the encoders, 1 to `F` codewords for the decoders (a larger packet is cut, with
  a message). The decoders process whole SIMD waves whatever the packet: set `-F` to
  the number of codewords of the packets. gr-fec gives its MTU to the objects when a block
  is built, which prints one message about the frame size.

## Command line options

* **K counts the CRC bits**, as in AFF3CT: with `--crc-poly 16-5G`, `-K 528` carries
  512 information bits per frame (`info_bits()`), the encoder input and decoder output.
* **Polar codes: the frozen bits are fixed when the object is built.** The simulator
  computes them for each SNR; here give the design noise (`--enc-fb-noise <sigma>`) for
  the GA, TV and BEC constructions, or use a construction without noise
  (`--enc-fb-gen-method 5G`, `RM`, `FILE`).
* **Data files**: some codes read AFF3CT data files (5G polar sequence, 5G LDPC base
  graphs, parity check matrices). They are installed with the AFF3CT simulator in
  `<prefix>/share/aff3ct-<version>/conf`; gr-aff3ct points `AFF3CT_CLI_PATH` there when
  this directory was found at build time and the variable is not set, so that a path
  such as `conf/dec/LDPC/MACKAY_504_1008.alist` is found as in the simulator (an
  absolute path works too).
* **Precision**: `-p` in the command line, as in the simulator: `-p 32` (float, default,
  every decoder), `-p 16` or `-p 8` (fixed point, faster with the SIMD decoders when the
  decoder supports it). The LLRs are multiplied by `llr_scale` (second argument of
  `codec_decoder.make`), then quantized with the fixed point format the simulator uses for this
  code (or `--qnt-bits` and `--qnt-dec`), by the SIMD quantizer (`--qnt-implem STD` for
  the one of the simulator).
* **SIMD strategy**: as in the simulator, e.g. `--dec-simd INTER` (LDPC, polar, RSC) or
  `--dec-sub-simd INTER` (turbo) decodes one frame per SIMD lane, the fastest.
* **Errors**: a wrong option, a missing file or a decoder that does not exist for this
  precision or this SIMD instruction set raises an exception when the object is built
  (a test frame is encoded and decoded), not in the running flowgraph. The encoders that
  only make sense in the simulator (`--enc-type AZCW`, `COSET`, `USER`) are refused, as
  is `int8` for the BCH codes of length 255 and more (BCH and TPC).

## Multithreading

Give a list of decoder objects to the FEC Extended Decoder (Parallelism 1, Dimension 1
= number of decoders) and choose `capillary` (power of 2) or `ordinary` threading.
Each object owns its AFF3CT modules: nothing is shared between threads.

## SIMD instruction set

AFF3CT selects its SIMD instruction set when it is compiled (MIPP, its SIMD layer, is
header-only), and gr-aff3ct includes the AFF3CT headers: **gr-aff3ct must be compiled
for the same instruction set as the libaff3ct it is loaded with** (register width,
number of frames decoded together, buffer alignment). A mismatch is detected when an
object is built ("AFF3CT and gr-aff3ct were built for different SIMD instruction sets").
The CMake configuration of gr-aff3ct handles both cases:

* **libaff3ct with glibc-hwcaps variants** (the Ubuntu packages, or AFF3CT built with
  `AFF3CT_HWCAPS`): a base library (SSE4.2) plus one library per x86-64 level in
  `<libdir>/glibc-hwcaps/x86-64-v3/` (AVX2) and `x86-64-v4/` (AVX-512); the dynamic
  loader of the glibc (2.33 or later) loads the best one for the CPU. gr-aff3ct detects
  the levels and builds `libgnuradio-aff3ct` in the same variants, so that the loader
  makes the same choice for both libraries. Nothing to set.
* **libaff3ct compiled for one instruction set** (from the sources): gr-aff3ct is
  compiled with `-march=native`, which matches an AFF3CT compiled with `-march=native`
  on the same machine. Otherwise give the flags of AFF3CT:
  `-DGR_AFF3CT_SIMD_FLAGS="-mavx2 -mfma"`.

`GR_AFF3CT_HWCAPS` (levels) and `GR_AFF3CT_SIMD_FLAGS` (flags of the base build)
override the detection. SSE4.2, AVX2 and AVX-512 (variants and single builds) pass the
QA tests.

## Building AFF3CT from the sources

```bash
git clone --recursive https://github.com/aff3ct/aff3ct.git
cd aff3ct
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-march=native -faligned-new" \
      -DAFF3CT_COMPILE_SHARED_LIB=ON \
      -DCMAKE_INSTALL_PREFIX=$HOME/.local/aff3ct
cmake --build build -j$(nproc)
cmake --install build
```

`-march=native` selects the best SIMD instruction set of the machine (SSE4.2, AVX2 or
AVX-512 on x86, NEON on ARM). To run the same binaries on several x86 machines, build
the variants instead: `-DCMAKE_CXX_FLAGS="-msse4.2 -faligned-new"
-DAFF3CT_HWCAPS="x86-64-v3;x86-64-v4"` (Linux). `-faligned-new` enables some decoders
(e.g. `BP_HORIZONTAL_LAYERED_LEGACY`). The simulator (`AFF3CT_COMPILE_EXE`, on by
default) also installs the data files in `share/aff3ct-<version>/conf`.

Then build gr-aff3ct as in the README, with `-DCMAKE_PREFIX_PATH=$HOME/.local/aff3ct`
added to the first `cmake` command. `cd build && ctest` runs the tests.

## Installing without sudo

Installing in `$HOME/.local` (no sudo): Python finds the module through the user
site-packages, but GRC must be told where the blocks are, in
`~/.config/gnuradio/config.conf` (absolute path, `~` is not expanded by GRC):

```ini
[grc]
local_blocks_path = /home/<user>/.local/share/gnuradio/grc/blocks
```

## Raspberry Pi

Same procedure on a 64-bit OS (Raspberry Pi OS 64-bit or Ubuntu arm64), natively on the
Pi, AFF3CT from the sources: `-march=native` selects NEON. Building AFF3CT takes a while;
use `-j2` or `-j3` on a Pi with 4 GB of RAM or less. Not tested yet.

## Repository layout

The module was created with `gr_modtool`, the GNU Radio tool that generates the skeleton
of an out-of-tree module. The code of gr-aff3ct itself is small:

| Path | Content |
|---|---|
| `include/gnuradio/aff3ct/` | public API: `codec_encoder.h`, `codec_decoder.h`, `codec_help.h` |
| `lib/codec_args.*` | parses the AFF3CT command line with the AFF3CT parsers |
| `lib/codec_modules.h` | builds the AFF3CT modules (codec, CRC, quantizer), chooses the frames per call |
| `lib/codec_encoder_impl.*`, `lib/codec_decoder_impl.*` | the gr-fec encoder and decoder objects |
| `lib/simd_check.cc` | checks that AFF3CT and gr-aff3ct were built for the same SIMD instruction set |
| `python/aff3ct/bindings/codec_*_python.cc` | Python bindings (pybind11) of the three headers |
| `python/aff3ct/qa_codec.py` | tests |
| `grc/` | GNU Radio Companion blocks |
| `examples/` | BER flowgraph, FER comparison with the simulator, throughput measurement |

The rest is the skeleton, mostly as generated: the CMake files, `cmake/`, `docs/`
(Doxygen), `include/gnuradio/aff3ct/api.h`, `python/aff3ct/__init__.py` and the other
files of `python/aff3ct/bindings/`.
