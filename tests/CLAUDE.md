# WAV File Test Suite

## Background

The WAV writer (`smart/WavFile.h`, `smart/WavFile.cpp`) was reviewed against
the RIFF and WAV specifications (Multimedia Programming Interface and Data
Specifications 1.0, Microsoft/IBM 1991). Seven problems were identified and
documented in `WAVFILE_PROBLEMS.md` at the repository root. This file describes
each problem, the fix applied, and how the test suite covers it.

## Problems and Fixes

### P1. Missing RIFF word-alignment pad bytes [HIGH] — Fixed

**What was wrong:** The RIFF spec requires every chunk to start on a 2-byte
boundary. If a chunk's data has an odd byte count, a zero pad byte must follow.
Neither `fillBuffer()` nor `writeFile()` ever wrote pad bytes.

**What broke:** Strict RIFF parsers could not locate chunks after an odd-sized
one (e.g., a `labl` chunk for string "ab" — 7 bytes of data). Tolerant parsers
like Audacity masked the problem.

**Fix:** Added `Chunk::getPadSize()` (returns 1 when `ckSize` is odd).
Updated the compound-chunk branches of `getDataSize()`, `fillBuffer()`, and
`writeFile()` to account for and write pad bytes after each child chunk. The
pad byte is NOT included in `ckSize`, per spec.

### P2. LeafChunk inflated ckSize with 4-byte alignment [MEDIUM] — Fixed

**What was wrong:** The `LeafChunk` constructor rounded the data allocation up
to a 4-byte boundary. The extra bytes became part of the data buffer and were
counted in `ckSize`. A label "ab" would have `ckSize=8` instead of `ckSize=7`.

**What broke:** Readers trusting `ckSize` would interpret the padding bytes as
real content (e.g., a garbage byte appended to a label string).

**Fix:** Removed the 4-byte rounding from the `LeafChunk` constructor.
`malloc()` already returns suitably aligned memory, so the original alignment
trap concern does not apply to the logical data size.

### P3. Rate-reduced writeFile produced ckSize/data mismatch [MEDIUM] — Fixed

**What was wrong:** When `_ratefactor > 1`, `PcmDataChunk::getDataSize()` used
floor division (`raw / _ratefactor`), but the write loop produces
`ceil(total_samples / _ratefactor)` output samples. When the sample count was
not divisible by the rate factor, the header claimed a different size than what
was actually written.

**What broke:** The `data` chunk header could be off by a few bytes. The RIFF
container size would also be wrong, producing a corrupt WAV.

**Fix:** Changed `getDataSize()` to use ceiling division:
`output_samples = (total_samples - 1) / _ratefactor + 1`, matching the write
loop's actual output.

### P4. Row-length truncation vs. actual bytes written [MEDIUM] — Fixed

**What was wrong:** When `_row_length > 0` and `_ratefactor == 1`,
`getDataSize()` truncated the result to a multiple of `_row_length`. But
`writeFile()` delegated to `Chunk::writeFile()`, which wrote the full
untruncated data buffers. Result: more bytes written than `ckSize` claimed.

**What broke:** `ckSize` was smaller than the actual data on disk. Readers
would stop early, losing trailing samples. The RIFF container size would also
be wrong.

**Fix:** `PcmDataChunk::writeFile()` now detects when `getDataSize()` is
smaller than `Chunk::getDataSize()` and limits the bytes written accordingly.

### P5. dwPosition used as sequential index [LOW] — Fixed

**What was wrong:** `CueChunk::setPoint()` set `dwPosition` to the cue point's
sequential index (0, 1, 2, ...) instead of the sample position.

**What broke:** Per spec, `dwPosition` should be the sample offset in the play
order. Most readers ignore this field when `dwSampleOffset` is present, so the
practical impact was minimal.

**Fix:** Changed `point->dwPosition = hdr->dwCuePoints` to
`point->dwPosition = sample_offset`.

### P6. No RF64 / large file support [LOW] — Not fixed

All size fields are `uint32_t`, so files exceeding ~4 GB will silently produce
corrupt headers. This is a larger architectural change and is not addressed by
the current test suite.

### P7. Non-standard chunk ordering [LOW] — Not a bug

`WavFileSimple.h` produces chunks in order: `fmt`, `cue`, `LIST(adtl)`,
`data`. The spec requires `fmt` before `data` (satisfied). Some strict readers
prefer `data` immediately after `fmt`, but this ordering was never a spec
violation. The writer was never changed for P7.

The verifier does check for the real spec violation (data before fmt →
`P7_FMT_AFTER_DATA`), but this is defensive — the library never produced files
with this problem. The test blob `fault_p7.wav` is a synthetic hand-crafted
file that demonstrates this detection.

## Reader Fixes

After the P1/P2 writer fixes, files produced by the library can have odd
`ckSize` values (previously masked by 4-byte rounding). The disk reader
(`WavFileDiskPcm`) was updated so that `seekFileEndOfChunk()` and
`inFileRange()` skip the pad byte when `ckSize` is odd. This is
backward-compatible: files from the old writer always had even `ckSize` values,
so the pad-byte logic never activates for them.

## Test Structure

### test_wavfile.cpp — Round-trip tests

These tests write WAV files using the library, then verify the output with
`wav_verify` (structural validator) and/or `WavFileDiskPcm` (read-back).

| Test | What it covers |
|------|---------------|
| `WavFile fillBuffer creates valid WAV in memory` | Basic in-memory WAV creation |
| `WavFile writeFile creates file on disk` | Basic file output |
| `WavFileSimplePcm write and read back` | Cue points, labels, associated files, read-back |
| `WavFileSimplePcm with cue points and associated data` | Cue read-back, label read-back, assoc file read-back |
| `Round-trip: write then read back verifies sample integrity` | Sample-level data verification |
| `wav_verify: in-memory buffer from fillBuffer` | Structural validation of fillBuffer output |
| `wav_verify_file: file written with writeFile` | Structural validation of writeFile output |
| `wav_verify: WavFileSimplePcm with cue, labels, files` | Structure + **P5 round-trip** (`CHECK_FALSE(P5_SEQ_POSITION)`) |
| `wav_verify: detect P1/P2 issues with odd-length labels` | **P1/P2 round-trip**: writer with odd-length label "ab" produces clean output |
| `wav_verify: ratefactor>1 round-trip (P3)` | **P3 round-trip**: 1001 samples with ratefactor=3, verifies `data_ck_size == 1336` and valid structure |
| `wav_verify: row_length truncation round-trip (P4)` | **P4 round-trip**: 24-bit mono with 13 bytes (not a multiple of `_row_length`=4), verifies `data_ck_size == 12` |
| `wav_verify: 24-bit 3-channel odd-frame round-trip` | 24-bit 3-channel format with `block_align=9`, sample-level read-back |

### test_wav_faults.cpp — Fault detection tests

These tests use hand-crafted binary WAV blobs to verify that `wav_verify`
correctly detects each class of problem. The blobs are built at runtime using
byte-level helpers, and also stored as binary files in `tests/data/`.

| Test | Fault injected | Expected detection |
|------|---------------|-------------------|
| `baseline: helpers produce valid WAVs` | None | `valid == true` |
| `P1: missing pad byte after odd-sized chunk` | Odd-sized LIST with no pad byte | `P1_NO_PAD` |
| `P2: ckSize inflated by 4-byte alignment padding` | labl `ckSize=8` instead of 7 | `P2_PADDED_CKSIZE` |
| `P3: data ckSize smaller than actual payload` | data `ckSize=333` (not a multiple of `block_align=4`) | `P3_DATA_NOT_BLOCK_ALIGNED` |
| `P4: data ckSize larger than actual payload` | data `ckSize=500` overflows RIFF | `CHUNK_OVERFLOW` |
| `P5: dwPosition set to sequential index` | `dwPosition=0,1` but `dwSampleOffset=100,500` | `P5_SEQ_POSITION` |
| `P7: non-standard chunk ordering (cue before fmt)` | cue before fmt, but fmt still before data | No `P7_FMT_AFTER_DATA` (not a violation) |
| `P7b: data chunk before fmt chunk` | data chunk appears before fmt | `P7_FMT_AFTER_DATA` |
| `stored blobs: verify fault detection` | Loads `tests/data/fault_p*.wav` | Same tags as runtime tests |

### tests/data/ — Stored fault blobs

Pre-built binary WAV files with known faults, used by the stored-blob test
section. Each file is under 5 KB.

| File | Fault |
|------|-------|
| `fault_p1.wav` | Odd-sized labl chunk with no pad byte |
| `fault_p2.wav` | labl `ckSize` inflated to 4-byte boundary |
| `fault_p3.wav` | data `ckSize` not a multiple of `block_align` |
| `fault_p5.wav` | `dwPosition` is sequential index, not sample offset |
| `fault_p7.wav` | data chunk appears before fmt chunk |

### wav_verify.h — Structural validator

Header-only WAV verifier. Parses RIFF/WAVE structure and reports issues.

| Tag | Level | Meaning |
|-----|-------|---------|
| `RIFF_SIZE_MISMATCH` | error | `riff_ck_size + 8` does not match buffer length |
| `CHUNK_OVERFLOW` | error | A chunk extends past the RIFF payload end |
| `MISSING_FMT` | error | No `fmt` chunk found |
| `MISSING_DATA` | error | No `data` chunk found |
| `FMT_TOO_SHORT` | error | `fmt` chunk smaller than 16 bytes |
| `BAD_BLOCK_ALIGN` | error | `blockAlign` does not match `channels * bytesPerSample` |
| `BAD_AVG_BYTES` | error | `avgBytesPerSec` does not match `samplesPerSec * blockAlign` |
| `LIST_SUBCHUNK_OVERFLOW` | error | Sub-chunk within LIST overflows LIST payload |
| `CUE_COUNT_MISMATCH` | warning | Declared cue point count does not match what fits |
| `P1_NO_PAD` | warning | Odd-sized chunk missing its pad byte |
| `P2_PADDED_CKSIZE` | warning | labl `ckSize` includes zero-padding beyond the null-terminated string |
| `P3_DATA_NOT_BLOCK_ALIGNED` | warning | data `ckSize` is not a multiple of `blockAlign` |
| `P5_SEQ_POSITION` | info | `dwPosition` differs from `dwSampleOffset` for a data-chunk cue point |
| `P7_FMT_AFTER_DATA` | warning | data chunk appears before fmt chunk |
| `FILE_OPEN_FAILED` | error | Cannot open file (file wrapper only) |
| `FILE_EMPTY` | error | File is empty or unreadable (file wrapper only) |
