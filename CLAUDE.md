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

### Build Outputs

- `libsmart.so` - Shared library
- `uio` - CLI tool for UIO device interaction (links against libcrack2)
