# Introduction

C++ routines for:
* UIO device management
* WAV file reading and writing
* Process creation
* Thread management
* Reading and writing files

This is in use in customer project firmware, both Petalinux and Debian.


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
