# Using gr-aff3ct in GNU Radio Companion

## Run the example

After the installation, open the example flowgraph:

```bash
gnuradio-companion examples/aff3ct_codec_ber.grc
```

It is also installed in `share/gnuradio/examples/aff3ct/` of the install prefix. Press
*Execute* (F6): random bits are encoded, sent as BPSK on an AWGN channel, decoded, and the
bit error rate is displayed. The slider sets Eb/N0.

With the default code (5G LDPC, K = 512, N = 1024) the display, which is log10 of the BER,
should read about:

| Eb/N0 | log10(BER) | BER |
|---|---|---|
| 1 dB | -1.2 | 6e-2 |
| 2 dB | -3.5 | 3e-4 |
| 3 dB | -7.4 | 4e-8 (wait for it: errors are rare) |

These are the values of the AFF3CT simulator for the same command line:

```bash
aff3ct -C LDPC --enc-type LDPC_5G -K 512 -N 1024 --dec-type BP_HORIZONTAL_LAYERED \
       --dec-implem NMS --dec-norm 0.75 --dec-simd INTER -p 16 -m 1 -M 3 -s 1
```

## Build your own flowgraph

gr-aff3ct does not add signal processing blocks. It adds two *definitions*, which describe
a code, and the standard FEC blocks of GNU Radio do the work.

1. In the block list, category **AFF3CT**, add an **AFF3CT Codec Encoder Definition** and
   an **AFF3CT Codec Decoder Definition**. They are not connected to anything, like a
   variable. Each one has an id, shown on the block (`enc` and `dec` in the example).
2. In both, set **AFF3CT codec** to the same command line of the AFF3CT simulator, for
   instance the one above without `aff3ct` and without `-m 1 -M 3 -s 1`. See
   [details.md](details.md) for the accepted options.
3. In category **Error Coding**, add a **FEC Extended Encoder** and a **FEC Extended
   Decoder**. Set *Encoder Objects* to the id of the encoder definition and *Decoder
   Objects* to the id of the decoder definition. Set *Puncture Pattern* to `'11'` in both.
4. Connect them:
   - the encoder takes and gives unpacked bits (type byte, one bit per byte, values 0
     and 1);
   - the decoder takes soft values (type float) and gives unpacked bits.

The soft values must be LLRs, **positive for a bit 0**. In the example, the BPSK symbols
are `1 - 2 * bit` and the received samples are multiplied by `2 / sigma^2` before the
decoder. If your demodulator gives the opposite sign or another scale, set **LLR scale**
in the decoder definition (a negative value flips the sign).

## Several decoders in parallel

In the decoder definition, set *Parallelism* to 1 and *Dimension 1* to the number of
decoders, then *Threading Type* to *Capillary* in the FEC Extended Decoder. Each decoder
runs in its own thread. The example does this with 2 decoders.

## Blocks not found

If the category AFF3CT does not appear, GRC does not look where the blocks were installed.

- Installed with `sudo cmake --install`: the blocks are in
  `/usr/local/share/gnuradio/grc/blocks`. Check that this path is listed when GRC starts
  (it prints its block paths in the terminal).
- Installed without sudo: see "Installing without sudo" in [details.md](details.md).

If GRC shows the blocks but the flowgraph fails with `No module named 'gnuradio.aff3ct'`
or a library that is not found, run `sudo ldconfig` and check that Python finds the
module: `python3 -c "from gnuradio import aff3ct"`.
