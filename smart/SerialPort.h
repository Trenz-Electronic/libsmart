///
/// \file SerialPort.h
/// \brief Utility functions for handling the serial port.

#pragma once

namespace smart { namespace SerialPort {

/// Open the serial port and return an ordinary file descriptor.
/// Use syscalls read() and write() to read from the file. Call close() when finished.
/// \param devicePath Path to the device file.
/// \param baudrate Baud rate, one of 50,100, ... 9600, .... 115200 ... 4000000.
int open(const char* devicePath, unsigned int baudrate);


} } // namespace smart::SerialPort

