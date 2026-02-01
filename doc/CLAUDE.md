# Introduction
This directory holds reference documentation for file formats used by libsmart.

# WAV / RIFF WAVE file format

When working on WAV-related code (`smart/WavFile.h`, `smart/WavFileDisk.h`, `smart/WavFileSimple.h`, `tests/wav_verify.h`, `apps/wav-verify`), consult these references:

- **WAVE_Specification.md** — Primary reference. Readable markdown covering the RIFF/WAVE structure, `fmt` chunk variants (PCM, non-PCM, extensible), `fact` chunk, `data` chunk, format codes, and full byte-layout examples. Start here for any WAV format question.
- **riffmci.pdf** — Original Microsoft RIFF/WAVE specification v1.0 (1991). Pages 56-65 cover WAVE. Authoritative for base chunk definitions and RIFF container rules.
- **RIFFNEW.pdf** — Microsoft Revision 3.0 update (1994). Pages 12-22 cover WAVE extensions including the `fact` chunk requirement for non-PCM formats and `cbSize` extension field.
- **Multiple_channel_audio_data_and_WAVE_files.pdf** — Microsoft spec for `WAVE_FORMAT_EXTENSIBLE` (0xFFFE), multi-channel speaker masks, and `wValidBitsPerSample`.
- **rfc2361.txt** — IANA registry of WAVE format codec codes (`wFormatTag` values).
- **Pages_from_mmreg.h.pdf** - List of chunks, this document shows a huge number of (proprietary) compressed formats, most of which are now obsolete.
