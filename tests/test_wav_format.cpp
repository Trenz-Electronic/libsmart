#include <catch2/catch_test_macros.hpp>
#include <smart/WavFormat.h>
#include "wav_verify.h"

#include <cstdio>
#include <cstring>
#include <vector>

TEST_CASE("makeHeader structure verification", "[wav]") {
    std::vector<uint8_t> header;
    smart::WavFormat::makeHeader(header, 2, 16, 44100, 1000);

    SECTION("header size is 44 bytes") {
        REQUIRE(header.size() == 44);
    }

    SECTION("RIFF magic at offset 0") {
        REQUIRE(header[0] == 'R');
        REQUIRE(header[1] == 'I');
        REQUIRE(header[2] == 'F');
        REQUIRE(header[3] == 'F');
    }

    SECTION("WAVE magic at offset 8") {
        REQUIRE(header[8] == 'W');
        REQUIRE(header[9] == 'A');
        REQUIRE(header[10] == 'V');
        REQUIRE(header[11] == 'E');
    }

    SECTION("fmt magic at offset 12") {
        REQUIRE(header[12] == 'f');
        REQUIRE(header[13] == 'm');
        REQUIRE(header[14] == 't');
        REQUIRE(header[15] == ' ');
    }

    SECTION("data magic at offset 36") {
        REQUIRE(header[36] == 'd');
        REQUIRE(header[37] == 'a');
        REQUIRE(header[38] == 't');
        REQUIRE(header[39] == 'a');
    }

    SECTION("format tag is 1 (PCM) at offset 20") {
        uint16_t format_tag = read_u16_le(&header[20]);
        REQUIRE(format_tag == 1);
    }
}

TEST_CASE("makeHeader parameter encoding", "[wav]") {
    std::vector<uint8_t> header;

    SECTION("mono channel encoding") {
        smart::WavFormat::makeHeader(header, 1, 16, 44100, 1000);
        uint16_t nChannels = read_u16_le(&header[22]);
        REQUIRE(nChannels == 1);
    }

    SECTION("stereo channel encoding") {
        smart::WavFormat::makeHeader(header, 2, 16, 44100, 1000);
        uint16_t nChannels = read_u16_le(&header[22]);
        REQUIRE(nChannels == 2);
    }

    SECTION("8-bit depth encoding") {
        smart::WavFormat::makeHeader(header, 1, 8, 44100, 1000);
        uint16_t bitsPerSample = read_u16_le(&header[34]);
        REQUIRE(bitsPerSample == 8);
    }

    SECTION("16-bit depth encoding") {
        smart::WavFormat::makeHeader(header, 1, 16, 44100, 1000);
        uint16_t bitsPerSample = read_u16_le(&header[34]);
        REQUIRE(bitsPerSample == 16);
    }

    SECTION("24-bit depth encoding") {
        smart::WavFormat::makeHeader(header, 1, 24, 44100, 1000);
        uint16_t bitsPerSample = read_u16_le(&header[34]);
        REQUIRE(bitsPerSample == 24);
    }

    SECTION("32-bit depth encoding") {
        smart::WavFormat::makeHeader(header, 1, 32, 44100, 1000);
        uint16_t bitsPerSample = read_u16_le(&header[34]);
        REQUIRE(bitsPerSample == 32);
    }

    SECTION("44100 Hz sample rate") {
        smart::WavFormat::makeHeader(header, 1, 16, 44100, 1000);
        uint32_t sampleRate = read_u32_le(&header[24]);
        REQUIRE(sampleRate == 44100);
    }

    SECTION("48000 Hz sample rate") {
        smart::WavFormat::makeHeader(header, 1, 16, 48000, 1000);
        uint32_t sampleRate = read_u32_le(&header[24]);
        REQUIRE(sampleRate == 48000);
    }

    SECTION("nAvgBytesPerSec calculation - mono 16-bit 44100Hz") {
        // nAvgBytesPerSec = sample_rate * channels * bytes_per_sample
        // = 44100 * 1 * 2 = 88200
        smart::WavFormat::makeHeader(header, 1, 16, 44100, 1000);
        uint32_t avgBytesPerSec = read_u32_le(&header[28]);
        REQUIRE(avgBytesPerSec == 88200);
    }

    SECTION("nAvgBytesPerSec calculation - stereo 16-bit 48000Hz") {
        // nAvgBytesPerSec = sample_rate * channels * bytes_per_sample
        // = 48000 * 2 * 2 = 192000
        smart::WavFormat::makeHeader(header, 2, 16, 48000, 1000);
        uint32_t avgBytesPerSec = read_u32_le(&header[28]);
        REQUIRE(avgBytesPerSec == 192000);
    }

    SECTION("nBlockAlign calculation - mono 16-bit") {
        // nBlockAlign = channels * bytes_per_sample = 1 * 2 = 2
        smart::WavFormat::makeHeader(header, 1, 16, 44100, 1000);
        uint16_t blockAlign = read_u16_le(&header[32]);
        REQUIRE(blockAlign == 2);
    }

    SECTION("nBlockAlign calculation - stereo 24-bit") {
        // nBlockAlign = channels * bytes_per_sample = 2 * 3 = 6
        smart::WavFormat::makeHeader(header, 2, 24, 44100, 1000);
        uint16_t blockAlign = read_u16_le(&header[32]);
        REQUIRE(blockAlign == 6);
    }

    SECTION("data block size encoding") {
        smart::WavFormat::makeHeader(header, 1, 16, 44100, 12345);
        uint32_t dataSize = read_u32_le(&header[40]);
        REQUIRE(dataSize == 12345);
    }
}

TEST_CASE("writeFile/readHeader roundtrip", "[wav]") {
    const char* temp_file = "/tmp/test_wav_format_roundtrip.wav";

    const unsigned int write_channels = 2;
    const unsigned int write_bits = 16;
    const unsigned int write_rate = 44100;
    const unsigned int write_data_size = 100;

    // Create sample data
    std::vector<uint8_t> sample_data(write_data_size, 0xAB);

    // Write the WAV file
    smart::WavFormat::writeFile(temp_file, write_channels, write_bits, write_rate,
                                 sample_data.data(), write_data_size);

    // Read it back
    FILE* fin = fopen(temp_file, "rb");
    REQUIRE(fin != nullptr);

    unsigned int read_channels = 0;
    unsigned int read_bits = 0;
    unsigned int read_rate = 0;
    unsigned int read_data_size = 0;

    smart::WavFormat::readHeader(fin, read_channels, read_bits, read_rate, read_data_size);
    fclose(fin);

    // Verify all parameters match
    REQUIRE(read_channels == write_channels);
    REQUIRE(read_bits == write_bits);
    REQUIRE(read_rate == write_rate);
    REQUIRE(read_data_size == write_data_size);

    // Clean up temp file
    std::remove(temp_file);
}
