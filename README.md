# gr-aff3ct

The forward error correction codes of [AFF3CT](https://github.com/aff3ct/aff3ct) in
GNU Radio 3.10: LDPC, polar, turbo, BCH, Reed-Solomon, convolutional and more, with the
fast SIMD decoders of AFF3CT.

A code is described by the command line of the AFF3CT simulator: a line that works in
the simulator works here. `codec_encoder` and
`codec_decoder` are gr-fec encoder and decoder objects, used in the standard FEC Extended,
Tagged and Async blocks of GNU Radio.

## Install

gr-aff3ct needs GNU Radio 3.10 and AFF3CT 4.7.4 or later.

**Ubuntu 22.04 and later, x86-64**: AFF3CT comes from its PPA.

```bash
sudo add-apt-repository ppa:aff3ct/aff3ct
sudo apt install libaff3ct-dev aff3ct-bin gnuradio-dev pybind11-dev python3-pybind11 \
                 libsndfile1-dev libgsl-dev cmake g++ git

git clone https://github.com/aff3ct/gr-aff3ct.git && cd gr-aff3ct
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build && sudo ldconfig
```

**Other systems (other Linux distributions, ARM, Raspberry Pi)**: build AFF3CT from its
sources first, then gr-aff3ct. GNU Radio 3.10 with its development files, pybind11, CMake
and a C++17 compiler come from the system (on Debian and its derivatives: the packages of
the line above, without the first two).

```bash
git clone --recursive https://github.com/aff3ct/aff3ct.git && cd aff3ct
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAFF3CT_COMPILE_SHARED_LIB=ON \
      -DCMAKE_CXX_FLAGS="-march=native -faligned-new" -DCMAKE_INSTALL_PREFIX=$HOME/.local/aff3ct
cmake --build build -j$(nproc) && cmake --install build && cd ..

git clone https://github.com/aff3ct/gr-aff3ct.git && cd gr-aff3ct
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$HOME/.local/aff3ct
cmake --build build -j$(nproc)
sudo cmake --install build && sudo ldconfig
```

`cd build && ctest` runs the tests. Install without sudo, binaries for several x86
machines: see [docs/details.md](docs/details.md).

## Use

In Python:

```python
from gnuradio import aff3ct, fec

code = ("-C LDPC --enc-type LDPC_5G -K 512 -N 1024 "
        "--dec-type BP_HORIZONTAL_LAYERED --dec-implem NMS --dec-norm 0.75 --dec-simd INTER -p 16")

encoder = fec.extended_encoder(aff3ct.codec_encoder.make(code), threading=None, puncpat="11")
decoder = fec.extended_decoder(aff3ct.codec_decoder.make(code), threading=None, puncpat="11")
```

`code` is a command line of the AFF3CT simulator, here a 5G LDPC code of 512 information
bits in 1024, decoded by 10 iterations of layered normalized min-sum, in 16-bit fixed
point. Each option is described in the AFF3CT documentation:
[LDPC codec](https://aff3ct.readthedocs.io/en/latest/user/simulation/parameters/codec/ldpc/codec.html),
[all codes](https://aff3ct.readthedocs.io/en/latest/user/simulation/parameters/codec/codec.html).
The same line given to the `aff3ct` simulator, with a range of SNR (`-m 1 -M 3`), prints
the error rate curve of the code.

The encoder takes unpacked bits (one bit per byte) and the decoder float LLRs, **positive
for a bit 0**. Give the same command line to both.

In GNU Radio Companion: add an *AFF3CT Codec Encoder Definition* and an *AFF3CT Codec
Decoder Definition* (category AFF3CT) and give their ids to the FEC blocks.
`examples/aff3ct_codec_ber.grc` is a complete BER test on an AWGN channel.

## Codes

| Code | Example |
|---|---|
| LDPC 5G (base graphs: data files) | `-C LDPC --enc-type LDPC_5G -K 512 -N 1024 --dec-type BP_HORIZONTAL_LAYERED --dec-implem NMS --dec-norm 0.75 --dec-simd INTER -p 16` |
| LDPC DVB-S2 | `-C LDPC --enc-type LDPC_DVBS2 -K 7200 -N 16200` |
| LDPC from a parity check matrix | `-C LDPC -K 504 -N 1008 --enc-type LDPC_H --dec-h-path conf/dec/LDPC/MACKAY_504_1008.alist` |
| Polar, 5G construction, CRC, SCL | `-C POLAR -K 528 -N 1024 --enc-fb-gen-method 5G --crc-poly 16-5G --dec-type SCL -L 8` |
| Polar, GA construction, shortened | `-C POLAR -K 120 -N 160 --enc-fb-noise 0.7 --pct-type SHORTLAST --dec-type ASCL -L 8 --crc-poly 24-LTEA` |
| Turbo LTE (mother code) | `-C TURBO -K 424 --enc-sub-std LTE -i 6 --dec-sub-simd INTER` |
| Turbo CCSDS | `-C TURBO -K 1784 --enc-sub-std CCSDS -i 10 --dec-sub-max MAXS` |
| BCH, Reed-Solomon | `-C BCH -N 255 -K 239 -T 2`, `-C RS -N 127 -K 101 -T 13` |
| Convolutional, Viterbi | `-C CONV --enc-poly {0171,0133} --dec-type VITERBI -K 64` |
| RSC, double binary RSC and turbo (DVB-RCS2) | `-C RSC -K 1024 --dec-simd INTER`, `-C TURBO_DB -K 1504 -N 1880 --enc-sub-std DVB-RCS2 --crc-poly 32-GZIP` |
| Turbo product code | `-C TPC -K 120 -N 127 --enc-ext -T 1 --dec-ite 8 --dec-p 4` |
| Polar variants, repetition, RA, uncoded | `-C POLAR_MK ...`, `-C POLAR_PAC ...`, `-C POLAR_PT ...`, `-C REP -K 128 -N 512`, `-C RA -K 256 -N 1024`, `-C UNCODED -K 256` |

`print(aff3ct.codec_help("LDPC"))` lists the options of a code type.

## Speed

* `-p 16` in the command line, as in the simulator, selects the fixed point decoder: twice
  the throughput of the float one for this LDPC code, for 0.1 dB. Remove it to decode in
  float.
* Give a list of decoders to the FEC Extended Decoder to use several cores.

The example above (5G LDPC code (1024,512), 10 iterations) on an AMD Ryzen 9 9950X3D with
AVX-512: 33 Mb/s per core, 121 Mb/s with 4 decoders.

## More

* [docs/details.md](docs/details.md): formats, frames per call, packets, options, SIMD,
  building from the sources.
* [docs/validation.md](docs/validation.md): throughput and error rates, checked against
  the AFF3CT simulator.

## License

MIT for the gr-aff3ct code. The build scaffolding generated by `gr_modtool` from the
GNU Radio templates keeps its GPL-3.0-or-later header. See [LICENSE](LICENSE).
