/// \file  WavFormat.cpp
/// \brief	Definitions of WAV file functions.
///
/// \version 	1.0
/// \date		2017
/// \copyright	SPDX: BSD-3-Clause 2016-2017 Trenz Electronic GmbH
#include <algorithm>	// std::min
#include <vector>		// std::vector

#include <stdint.h>		// uint32_t, etc.
#include <string.h>		// memcpy
#include "WavFormat.h"	// ourselves.
#include "File.h"		// File operations.
#include "string.h"		// ssprintf

namespace smart {
namespace WavFormat {

/// Construct a 32-bit word out of text, first char is LSB, fourth char is MSB.
#define	UINT32_OF_TEXT(magic_txt)	(			\
	(((uint32_t)(magic_txt[0])) << 0) |		\
	(((uint32_t)(magic_txt[1])) << 8) |		\
	(((uint32_t)(magic_txt[2])) << 16) |	\
	(((uint32_t)(magic_txt[3])) << 24) 		\
)

#pragma pack(push, 1)
/// RIFF file header.
struct RiffHeader {
	// 0
	/// The ASCII text string "RIFF".
	uint32_t	riffMagic;

	// 4
	/// The file size not including the "RIFF" description (4 bytes) and file size (4 bytes). This is file size - 8.
	uint32_t	fileSize;

	// 8
	/// The ASCII text string "WAVE".
	uint32_t	waveMagic;

	// 12
	/// The ASCII text string "fmt "(The space is also included).
	uint32_t	fmtMagic;

	// 16
	/// The size of the WAVE type format (2 bytes) + mono/stereo flag (2 bytes) + sample rate (4 bytes)
	/// + bytes per sec (4 bytes) + block alignment (2 bytes) + bits per sample (2 bytes).
	/// This is usually 16.
	uint32_t	fmtSectionSize;
};

/// A copy of Windows WAVEFORMATEX
struct WaveFormatEx {
	// 20
	uint16_t	wFormatTag;
	// 22
	uint16_t	nChannels;
	// 24
	uint32_t	nSamplesPerSec;
	// 28
	uint32_t	nAvgBytesPerSec;
	// 32
	uint16_t	nBlockAlign;
	// 34
	uint16_t	wBitsPerSample;
};

struct RiffFieldHeader {
	// magic number identifier
	uint32_t	magic;

	// 42
	/// Data block size, in bytes.
	uint32_t	size;
};

/// RIFF data block header.
// byte 38: The ASCII text string "data".
// byte 42: Data block size, in bytes.
typedef RiffFieldHeader RiffDataHeader;

struct WavFileHeader {
	RiffHeader			riff_header;
	WaveFormatEx		fmt;
	RiffDataHeader		data_header;
};
#pragma pack(pop)

// --------------------------------------------------------------------------------------------------------------------
/// Read the item, fail with exception.
template <class T>
static void _fread_sure(T& t, FILE* f, const char* func_name, const char* element_name, const unsigned int to_read = sizeof(T))
{
	const auto		r_read = fread(&t, 1, to_read, f);
	if (static_cast<unsigned int>(r_read) != to_read) {
		throw std::runtime_error(ssprintf("%s: cannot read %s, read returned %d", func_name, element_name, r_read));
	}
}


/// Create WAV header structures. Assumes the structures having already zeroed out.
static void fillHeader(
	WavFileHeader& header,
	const unsigned int	nchannels,
	const unsigned int	bits_per_sample,
	const unsigned int	sample_rate,
	const unsigned int	data_block_size)
{
	header.riff_header.riffMagic = UINT32_OF_TEXT("RIFF");
	header.riff_header.fileSize = sizeof(RiffHeader) + sizeof(WaveFormatEx) + data_block_size - 8;
	header.riff_header.waveMagic = UINT32_OF_TEXT("WAVE");
	header.riff_header.fmtMagic = UINT32_OF_TEXT("fmt ");
	header.riff_header.fmtSectionSize = sizeof(WaveFormatEx);

	const unsigned int	bytes_per_sample = nchannels * ((bits_per_sample + 7u) / 8u);
	header.fmt.wFormatTag = 1; // PCM
	header.fmt.nChannels = nchannels;
	header.fmt.nSamplesPerSec = sample_rate;
	header.fmt.nAvgBytesPerSec = sample_rate * bytes_per_sample;
	header.fmt.nBlockAlign = bytes_per_sample;
	header.fmt.wBitsPerSample = bits_per_sample;

	header.data_header.magic = UINT32_OF_TEXT("data");
	header.data_header.size = data_block_size;
}

// --------------------------------------------------------------------------------------------------------------------
void writeHeader(
		const std::string&	filename,
		FILE*				fout,
		const unsigned int	nchannels,
		const unsigned int	bits_per_sample,
		const unsigned int	sample_rate,
		const unsigned int	data_block_size)
{
	WavFileHeader		header;
	fillHeader(header, nchannels, bits_per_sample, sample_rate, data_block_size);
	File::writeAllBytes(filename, fout, &header, sizeof(header));
}

// --------------------------------------------------------------------------------------------------------------------
void writeFile(
	const std::string&	filename,
	const unsigned int	nchannels,
	const unsigned int	bits_per_sample,
	const unsigned int	sample_rate,
	const void*			sample_buffer,
	const unsigned int	data_block_size)
{
	FILE*	fout = fopen(filename.c_str(), "wb");
	if (fout == nullptr) {
		throw std::runtime_error(ssprintf("writeWav: cannot write file '%s'", filename.c_str()));
	} else {
		File	fp(fout);

		writeHeader(filename, fout, nchannels, bits_per_sample, sample_rate, data_block_size/*, alignment*/);
		// Without fflush/setvbuf the fwrite will generate an alignment exception on 32-bit ARM.
		// Probably because the header size is not aligned to 4-bytes.
		// This avoid the alignment fixup and hopefully speeds up things.
		fflush(fout);
		setvbuf(fout, nullptr, _IONBF, 0u);
		File::writeAllBytes(filename, fout, sample_buffer, data_block_size);
	}
}

// --------------------------------------------------------------------------------------------------------------------
void readHeader(
	FILE*				fin,
	unsigned int&		nchannels,
	unsigned int&		bits_per_sample,
	unsigned int&		sample_rate,
	unsigned int&		total_bytes)
{
	// 1.RIFF header.
	RiffHeader		riff_header = { 0 };
	_fread_sure(riff_header, fin, "readWavHeader", "riff header");
	if (riff_header.riffMagic != UINT32_OF_TEXT("RIFF")) {
		throw std::runtime_error(ssprintf("readWavHeader: RIFF magic incorrect"));
	}
	if (riff_header.waveMagic != UINT32_OF_TEXT("WAVE")) {
		throw std::runtime_error(ssprintf("readWavHeader: WAVE magic incorrect"));
	}
	if (riff_header.fmtMagic != UINT32_OF_TEXT("fmt ")) {
		throw std::runtime_error(ssprintf("readWavHeader: Format magic incorrect"));
	}

	// 2. Wave format.
	WaveFormatEx	fmt = { 0 };
	const unsigned int	fmt_to_read = std::min<unsigned int>(riff_header.fmtSectionSize, sizeof(fmt));
	_fread_sure(fmt, fin, "readWavHeader", "wave format", fmt_to_read);
	// 2b. There are files out there which have different value for the riff_header.fmtSectionSize.
	unsigned int	fmt_remaining = riff_header.fmtSectionSize - fmt_to_read;
	while (fmt_remaining > 0u) {
		char					buffer[512];
		const unsigned int		to_read = std::min<unsigned int>(fmt_remaining, sizeof(buffer));
		_fread_sure(buffer, fin, "readWavHeader", "extra bytes", to_read);
		fmt_remaining -= to_read;
	}
	nchannels = fmt.nChannels;
	bits_per_sample = fmt.wBitsPerSample;
	sample_rate = fmt.nSamplesPerSec;

	// 3. Skip fields that aren't the data header
	RiffFieldHeader field_header = { 0 };
	int result;
	while (field_header.magic != UINT32_OF_TEXT("data")) {
		// clearly the field wasnt data, so lets skip it
		if (field_header.size != 0) {
			result = std::fseek(fin, field_header.size, SEEK_CUR);
			if (result != 0)
				throw std::runtime_error(ssprintf("error while skipping field, aborting..."));
		}
		_fread_sure(field_header, fin, "readWavHeader", "generic field header");
	}

	// 4. Has to be the data header.
	total_bytes = field_header.size;
}

void makeHeader(
		std::vector<uint8_t> &buffer,
		const unsigned int	nchannels,
		const unsigned int	bits_per_sample,
		const unsigned int	sample_rate,
		const unsigned int	data_block_size)
{
	WavFileHeader		header;
	fillHeader(header, nchannels, bits_per_sample, sample_rate, data_block_size);
	buffer.resize(sizeof(header));
	memcpy(&buffer[0], &header, sizeof(header));
}

} // namespace WavFormat
} // namesapce smart
