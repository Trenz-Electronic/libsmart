# Introduction

C++ routines for:
* UIO device management
* WAV file reading and writing
* Process creation
* Thread management
* Reading and writing files

This is in use in customer project firmware, both Petalinux and Debian.


# Complete list of instructions on how to create Debian packages

```shell
apt install libcrack2-dev
mkdir build
cd build
cmake ..
cmake --build .
cpack -G DEB
```
