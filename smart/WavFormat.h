/// \file  WavFormat.h
/// \brief	Declarations of WAV file functions.
///
/// \version 	1.0
/// \date		2017
/// \copyright	SPDX: BSD-3-Clause 2016-2017 Trenz Electronic GmbH

#pragma once

#include <string>	// std::string
#include <stdio.h>	// FILE*
#include <vector>		// std::vector

namespace smart {

/// Functions for processing WAV files.
namespace WavFormat {

/// Write a WAV format header in a file.
/// @param filename Output filename.
/// @param nchannels Number of channels.
/// @param bits_per_sample Bits per sample.
/// @param sample_rate Sample rate, in Hz.
/// @param data_block_size Total number of bytes of the samples.
void writeHeader(
	const std::string&	filename,
	FILE*				fout,
	const unsigned int	nchannels,
	const unsigned int	bits_per_sample,
	const unsigned int	sample_rate,
	const unsigned int	data_block_size);

/// Write a WAV formatted file.
/// @param filename Output filename.
/// @param nchannels Number of channels.
/// @param bits_per_sample Bits per sample.
/// @param sample_rate Sample rate, in Hz.
/// @param sample_buffer Pointer to the samples.
/// @param data_block_size Total number of bytes of the samples.
void writeFile(
	const std::string&	filename,
	const unsigned int	nchannels,
	const unsigned int	bits_per_sample,
	const unsigned int	sample_rate,
	const void*			sample_buffer,
	const unsigned int	data_block_size);

/// Read WAV header and position the file pointer at the beginning of the data.
void readHeader(
	FILE*				fin,
	unsigned int&		nchannels,
	unsigned int&		bits_per_sample,
	unsigned int&		sample_rate,
	unsigned int&		total_bytes);

/// Write a WAV format header in a buffer
/// @param filename Output filename.
/// @param nchannels Number of channels.
/// @param bits_per_sample Bits per sample.
/// @param sample_rate Sample rate, in Hz.
/// @param data_block_size Total number of bytes of the samples.
void makeHeader(
	std::vector<uint8_t> &buffer,
	const unsigned int	nchannels,
	const unsigned int	bits_per_sample,
	const unsigned int	sample_rate,
	const unsigned int	data_block_size);

} // namespace WavFormat
} // namespace smart
