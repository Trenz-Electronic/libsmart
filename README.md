# Introduction

![CMake on multiple platforms](https://github.com/Trenz-Electronic/libsmart/actions/workflows/cmake-multi-platform.yml/badge.svg)

C++ routines for:
* UIO device management
* WAV file reading and writing
* Process creation
* Thread management
* Reading and writing files

This is in use in customer project firmware, both Petalinux and Debian.

# Examples

The `examples/` directory contains sample programs:

* **uart_terminal** — Simple serial terminal for a 16550-compatible UART exposed via UIO. Multiplexes keyboard input and UART RX interrupts using `poll()`.
* **gpio_blink** — Blinks the lowest bit of a Xilinx AXI GPIO at 1 Hz via UIO.

# Building

```shell
sudo apt install libcrack2-dev
mkdir build
cd build
cmake ..
cmake --build .
```

# Running tests

```shell
cd build
cmake -DBUILD_TESTS=ON ..
cmake --build .
ctest --output-on-failure
```

# Creating Debian packages

```shell
cd build
cpack -G DEB
```
