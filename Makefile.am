#libsmart_la_CXXFLAGS= -std=c++11
lib_LTLIBRARIES = libsmart.la
libsmart_la_SOURCES = \
	smart/Directory.cpp \
	smart/Disk.cpp \
	smart/File.cpp	\
	smart/MappedFile.cpp \
	smart/ModbusPacket.cpp\
	smart/Path.cpp \
	smart/Process.cpp \
	smart/Random.cpp \
	smart/SerialPort.cpp \
	smart/Symlink.cpp \
	smart/TimeScheduler.cpp \
	smart/UioDevice.cpp \
	smart/WavFormat.cpp \
	smart/getopt.cpp \
	smart/mylogf.cpp \
	smart/net.cpp \
	smart/string.cpp \
	smart/time.cpp \
	smart/hw/AxiDataCapture.cpp

# Headers will be under /usr/include, complete with relative path.
nobase_include_HEADERS = \
	smart/CircularBuffer.h \
	smart/Directory.h \
	smart/Disk.h \
	smart/File.h \
	smart/MappedFile.h \
	smart/ModbusPacket.h \
	smart/Path.h \
	smart/Process.h \
	smart/Random.h \
	smart/SerialPort.h \
	smart/Symlink.h \
	smart/TimeScheduler.h \
	smart/UioDevice.h \
	smart/WavFormat.h \
	smart/getopt.h \
	smart/memory.h \
	smart/mylogf.h \
	smart/net.h \
	smart/passwd.h \
	smart/proc.h \
	smart/string.h \
	smart/time.h \
	smart/hw/AxiDataCapture.h \
	smart/ts/Queue.h

libsmart_la_LDFLAGS = -version-info 1:0:0
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = uio
uio_SOURCES = src_uio/uio_main.cpp
uio_LDADD = libsmart.la
#uio_CXXFLAGS= -std=c++11
