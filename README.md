# libsmart

![CMake on multiple platforms](https://github.com/Trenz-Electronic/libsmart/actions/workflows/cmake-multi-platform.yml/badge.svg)

A C++20 utility library for embedded Linux systems on Xilinx SoCs, targeting Petalinux and Debian.

## Features

* UIO device management
* WAV file reading and writing
* Process creation
* Thread management
* Reading and writing files

## Examples

The `examples/` directory contains sample programs:

* **uart_terminal** — Simple serial terminal for a 16550-compatible UART exposed via UIO. Multiplexes keyboard input and UART RX interrupts using `poll()`.
* **gpio_blink** — Blinks the lowest bit of a Xilinx AXI GPIO at 1 Hz via UIO.

## Building

```shell
sudo apt install libcrack2-dev
mkdir build
cd build
cmake ..
cmake --build .
```

## Running tests

```shell
cd build
cmake -DBUILD_TESTS=ON ..
cmake --build .
ctest --output-on-failure
```

## Creating Debian packages

```shell
cd build
cpack -G DEB
```
