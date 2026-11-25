# gr-aff3ct: measurements

## Throughput

Information throughput in Mb/s through GNU Radio, with 1 and with 4 decoders, on an AMD
Ryzen 9 9950X3D with AFF3CT 4.7.4. Vector source of noisy LLRs (Eb/N0 = 2 dB), FEC
Extended Decoder, null sink; several decoders: `capillary` threading. The SIMD level is
forced with `GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX512F` or `-AVX2,-AVX512F`.

Each line is reproduced by:

```bash
examples/decoder_throughput.py --decoders 1 4 --ebn0 2.0 -- <command line>
```

| Command line | SSE4.2 | AVX2 | AVX-512 |
|---|---|---|---|
| `-C LDPC --enc-type LDPC_5G -K 512 -N 1024 --dec-type BP_HORIZONTAL_LAYERED --dec-implem NMS --dec-norm 0.75 --dec-simd INTER` | 9 / 34 | 16 / 60 | 17 / 62 |
| `-C LDPC --enc-type LDPC_5G -K 512 -N 1024 --dec-type BP_HORIZONTAL_LAYERED --dec-implem NMS --dec-norm 0.75 --dec-simd INTER -p 16` | 16 / 61 | 28 / 104 | 33 / 121 |
| `-C TURBO -K 424 --enc-sub-std LTE -i 6 --dec-sub-simd INTER -p 16` | 46 / 157 | 77 / 256 | 75 / 257 |
| `-C POLAR -K 528 -N 1024 --enc-fb-gen-method 5G --crc-poly 16-5G -p 8` | 179 / 442 | 202 / 591 | 197 / 597 |
| `-C POLAR -K 528 -N 1024 --enc-fb-gen-method 5G --crc-poly 16-5G --dec-type SCL -L 8` | 12 / 44 | 12 / 45 | 13 / 49 |
| `-C BCH -N 255 -K 239 -T 2` | 95 / 324 | 95 / 337 | 99 / 323 |

## Validation against the AFF3CT simulator

Frame error rate through GNU Radio (`codec_encoder` and `codec_decoder` in the FEC
Extended blocks, BPSK, AWGN) and with the AFF3CT simulator on the same command line,
200 frame errors each. The two agree on every line (95 % intervals). AFF3CT 4.7.4.

Each line is reproduced by:

```bash
examples/fer_vs_simulator.py --ebn0 <Eb/N0> --simulator aff3ct -- <command line>
```

| Command line | Eb/N0 | gr-aff3ct | Simulator |
|---|---|---|---|
| `-C LDPC --enc-type LDPC_5G -K 512 -N 1024 --dec-type BP_HORIZONTAL_LAYERED --dec-implem NMS --dec-norm 0.75 --dec-simd INTER` | 2.0 dB | 1.54e-2 | 1.48e-2 |
| `-C LDPC --enc-type LDPC_5G -K 512 -N 1024 --dec-type BP_HORIZONTAL_LAYERED --dec-implem NMS --dec-norm 0.75 --dec-simd INTER -p 16` | 2.0 dB | 1.65e-2 | 1.87e-2 |
| `-C TURBO -K 424 --enc-sub-std LTE -i 6 --dec-sub-simd INTER -p 16` | 1.0 dB | 1.30e-1 | 1.39e-1 |
| `-C TURBO -K 424 --enc-sub-std LTE -i 6 --dec-sub-simd INTER -p 16` | 1.5 dB | 7.3e-3 | 8.2e-3 |
| `-C POLAR -K 528 -N 1024 --enc-fb-gen-method 5G --crc-poly 16-5G --dec-type SCL -L 8` | 2.0 dB | 2.7e-3 | 2.9e-3 |
| `-C LDPC -K 504 -N 1008 --enc-type LDPC_H --dec-h-path conf/dec/LDPC/MACKAY_504_1008.alist` | 2.75 dB | 7.7e-3 | 7.8e-3 |
| `-C BCH -N 255 -K 239 -T 2` | 6.0 dB | 4.7e-2 | 4.8e-2 |
| `-C RS -N 127 -K 101 -T 13` | 5.0 dB | 1.77e-1 | 2.01e-1 |
| `-C CONV --enc-poly {0171,0133} --dec-type VITERBI -K 64` | 3.0 dB | 1.04e-2 | 8.6e-3 |
| `-C RSC -K 1024 --dec-simd INTER -p 16` | 5.5 dB | 1.50e-2 | 1.48e-2 |
| `-C TURBO_DB -K 1504 -N 1880 --enc-sub-std DVB-RCS2 -i 8 --crc-poly 32-GZIP -p 16` | 3.5 dB | 1.79e-2 | 2.06e-2 |
| `-C TPC -K 120 -N 127 --enc-ext -T 1 --dec-ite 8 --dec-p 4` | 3.75 dB | 5.2e-2 | 5.1e-2 |

The 5G LDPC example reaches a frame error rate of 1e-2 at 2.1 dB with its 10 iterations
of normalized min-sum; the sum-product algorithm with 50 iterations reaches it at 1.5 dB.

## Tests

`python/aff3ct/qa_codec.py`, run by `ctest`: noiseless round trips through the gr-fec
Extended blocks for 17 configurations (polar SC, SCL and ASCL with CRC and shortening,
turbo LTE and CCSDS, LDPC 5G and DVB-S2, BCH, RS, repetition, RA, RSC, double binary
turbo with CRC, convolutional, PAC, uncoded; float, int16 and int8), FER checks on AWGN,
sizes reported to gr-fec, several frame counts per call, packets of several sizes through
the Tagged blocks, rejection of wrong command lines, `codec_help`.

`examples/fer_vs_simulator.py` compares the FER through GNU Radio with the AFF3CT
simulator on the same command line (table above), `examples/decoder_throughput.py`
measures the throughput of a decoder through GNU Radio.
