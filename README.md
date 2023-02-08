Introduction
------------

C++ routines for:
* UIO device management
* WAV file reading and writing
* Process creation
* Thread management
* Reading and writing files

This is in use in customer project firmware, both Petalinux and Debian.


Usage
-----
# Update the timestamps on the files
./touch-configure.sh
# Traditional configure script
./configure
# Traditional make
make


Requirements
------------

apt install libcrack2-dev


Development
-----------


Problem:  # configure.ac: AX_CXX_COMPILE_STDCXX_11 requires this.
Solution: apt-get install autoconf-archive


After changing Makefile.am or autoconf.ac, run the following command:
	./autogen.sh
