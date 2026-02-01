# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

Prerequisites: `apt install libcrack2-dev`

```bash
# Basic build
mkdir -p build && cd build && cmake .. && cmake --build . -j4

# Build with tests
cmake -DBUILD_TESTS=ON .. && cmake --build . -j4

# Run all tests
ctest --output-on-failure

# Run tests with verbose output
./tests/test_smart

# Run specific test by name
./tests/test_smart "trim removes whitespace"

# Create Debian packages
cpack -G DEB
```

Alternatively, `libsmart_tools/run` provides a Docker-based build environment.

## Architecture

**libsmart** is a C++20 utility library for embedded Linux systems (Petalinux, Debian). Everything is in the `smart` namespace.

### Core Components

- **UioDevice** (`smart/UioDevice.h`) - Linux Userspace I/O device access with memory-mapped regions and IRQ handling. Supports non-coherent DMA buffer synchronization.

- **CircularBuffer** (`smart/CircularBuffer.h`) - Lock-free circular buffer template using atomics. Note: capacity is size-1 due to the empty/full distinction method.

- **ts::Queue** (`smart/ts/Queue.h`) - Thread-safe FIFO queue using mutex protection.

- **hw::AxiDataCapture** (`smart/hw/AxiDataCapture.h`) - AXI DMA data capture for FPGA interfaces.

### String/Path Utilities

- `smart::int_of`, `smart::uint_of` - String to integer with hex (0x) support
- `smart::split` - String splitting (always returns at least 1 element)
- `smart::ends_with` - Suffix check (requires suffix strictly shorter than string)
- `smart::Path::combine` - Path joining (always adds separator, doesn't normalize)

### WAV File Components

- **WavFile** (`smart/WavFile.h`) - Base WAV file writer producing standard RIFF/WAVE with `fmt` and `data` chunks. Supports `fillBuffer` (in-memory) and `writeFile` (to disk).
- **WavFileDisk** (`smart/WavFileDisk.h`) - Streaming WAV writer for incremental sample output to disk.
- **WavFileSimple** (`smart/WavFileSimple.h`) - Extended WAV writer supporting cue points and LIST/adtl metadata (labels, notes, files).
- **wav_verify** (`tests/wav_verify.h`) - Header-only WAV structure validator. `wav_verify()` checks an in-memory buffer; `wav_verify_file()` reads from disk. Reports issues (errors/warnings/info) and parses fmt, data, cue, and LIST/adtl chunks.

WAV format reference documentation is in `doc/` — see `doc/CLAUDE.md` for a guide to each document.

### Build Outputs

- `libsmart.so` - Shared library
- `uio` - CLI tool for UIO device interaction (links against libcrack2)
- `wav-verify` - CLI tool for WAV file structure verification
