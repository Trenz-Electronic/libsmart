#include <catch2/catch_test_macros.hpp>
#include <smart/WavFileDisk.h>
#include <smart/WavFileSimple.h>
#include "wav_verify.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

static const char* test_wav_path = "/tmp/test_wavfile.wav";

// 24-bit LE helpers for the 3-channel round-trip test
static void write_i24_le(uint8_t* dst, int32_t val)
{
	dst[0] = static_cast<uint8_t>(val & 0xFF);
	dst[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
	dst[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
}

static int32_t read_i24_le(const uint8_t* src)
{
	uint32_t raw = static_cast<uint32_t>(src[0])
	             | (static_cast<uint32_t>(src[1]) << 8)
	             | (static_cast<uint32_t>(src[2]) << 16);
	// sign-extend from 24-bit
	if (raw & 0x800000)
		raw |= 0xFF000000;
	return static_cast<int32_t>(raw);
}

#pragma pack(push, 1)
struct sample_stereo_16_t {
	int16_t ch0;
	int16_t ch1;
};
#pragma pack(pop)

// Helper: fill a buffer with a stereo sawtooth pattern
static void fill_sawtooth(uint8_t* buf, uint32_t num_samples)
{
	int16_t sig0 = 0;
	int16_t sig1 = 0;
	for (uint32_t i = 0; i < num_samples; i++) {
		auto sample = reinterpret_cast<sample_stereo_16_t*>(buf + i * sizeof(sample_stereo_16_t));
		sig0 += 65;
		sig1 -= 65;
		sample->ch0 = sig0;
		sample->ch1 = sig1;
	}
}

TEST_CASE("WavFile fillBuffer creates valid WAV in memory", "[wavfile]") {
	const uint32_t num_samples = 1000;
	const uint32_t data_bytes = num_samples * sizeof(sample_stereo_16_t);

	uint8_t sound_data[data_bytes];
	fill_sawtooth(sound_data, num_samples);

	smart::WavFile::RiffChunk riffchunk("WAVE");
	smart::WavFile::PcmChunk pcmchunk(&riffchunk, 2, 44100, 16);
	smart::WavFile::CueChunk cuechunk(&riffchunk);
	cuechunk.setWavPoint("CNFG", "data", 0);
	cuechunk.setWavPoint("TRIG", "data", 500);

	smart::WavFile::AssocListChunk listchunk(&riffchunk);
	smart::WavFile::LabelChunk triglabel(&listchunk, "TRIG", "Test trigger.");
	const char* filedata = "key=value\n";
	smart::WavFile::FileChunk configfile(&listchunk, "CNFG", "TXT", filedata, strlen(filedata) + 1);

	smart::WavFile::PcmDataChunk pcmdata(&riffchunk);
	pcmdata.addPiece(sound_data, data_bytes);

	uint32_t maxlen = riffchunk.getSize();
	REQUIRE(maxlen > 0);

	std::vector<uint8_t> file_buf(maxlen);
	uint8_t* ptr = file_buf.data();
	uint32_t written = riffchunk.fillBuffer(&ptr, maxlen);

	REQUIRE(written > 0);
	REQUIRE(file_buf[0] == 'R');
	REQUIRE(file_buf[1] == 'I');
	REQUIRE(file_buf[2] == 'F');
	REQUIRE(file_buf[3] == 'F');
}

TEST_CASE("WavFile writeFile creates file on disk", "[wavfile]") {
	const uint32_t num_samples = 1000;
	const uint32_t data_bytes = num_samples * sizeof(sample_stereo_16_t);

	uint8_t sound_data[data_bytes];
	fill_sawtooth(sound_data, num_samples);

	smart::WavFile::RiffChunk riffchunk("WAVE");
	smart::WavFile::PcmChunk pcmchunk(&riffchunk, 2, 44100, 16);
	smart::WavFile::PcmDataChunk pcmdata(&riffchunk);
	pcmdata.addPiece(sound_data, data_bytes);

	FILE* f = fopen(test_wav_path, "wb");
	REQUIRE(f != nullptr);
	uint32_t written = riffchunk.writeFile(f);
	fclose(f);

	REQUIRE(written > 0);

	// Verify the file starts with RIFF
	f = fopen(test_wav_path, "rb");
	REQUIRE(f != nullptr);
	char magic[4];
	fread(magic, 1, 4, f);
	fclose(f);
	REQUIRE(memcmp(magic, "RIFF", 4) == 0);

	std::remove(test_wav_path);
}

TEST_CASE("WavFileSimplePcm write and read back", "[wavfile]") {
	const uint32_t num_samples = 1000;
	const uint32_t data_bytes = num_samples * sizeof(sample_stereo_16_t);

	uint8_t sound_data[data_bytes];
	fill_sawtooth(sound_data, num_samples);

	// Write via WavFileSimplePcm
	{
		smart::WavFileSimplePcm simple(2, 44100, 16);
		simple.addData(sound_data, data_bytes);
		simple.addCuePoint("MARK", 100, "Test marker");

		FILE* f = fopen(test_wav_path, "wb");
		REQUIRE(f != nullptr);
		uint32_t written = simple.writeFile(f);
		fclose(f);
		REQUIRE(written > 0);
	}

	// Read back via WavFileDiskPcm
	{
		smart::WavFileDiskPcm reader(test_wav_path);
		REQUIRE(reader.hasData());
		REQUIRE(reader.getNumOfChannels() == 2);
		REQUIRE(reader.getSampleRate() == 44100);
		REQUIRE(reader.getBytesPerSample() == 4);  // 2 channels * 2 bytes
		REQUIRE(reader.getSampleCount() == num_samples);
	}

	std::remove(test_wav_path);
}

TEST_CASE("WavFileSimplePcm with cue points and associated data", "[wavfile]") {
	const uint32_t num_samples = 500;
	const uint32_t data_bytes = num_samples * sizeof(sample_stereo_16_t);

	uint8_t sound_data[data_bytes];
	fill_sawtooth(sound_data, num_samples);

	const char* filedata = "property1=hello\nproperty2=world\n";

	// Write
	{
		smart::WavFileSimplePcm simple(2, 44100, 16);
		simple.addData(sound_data, data_bytes);
		simple.addCuePoint("CNFG", 0);
		simple.addCuePoint("TRIG", 250, "Trigger point");
		simple.addAssocFile("CNFG", "TXT", filedata, strlen(filedata) + 1);

		FILE* f = fopen(test_wav_path, "wb");
		REQUIRE(f != nullptr);
		simple.writeFile(f);
		fclose(f);
	}

	// Read back and verify
	{
		smart::WavFileDiskPcm reader(test_wav_path);
		REQUIRE(reader.hasData());
		REQUIRE(reader.getSampleCount() == num_samples);

		// Verify associated file data
		auto assoc = reader.getAssocFile("CNFG");
		REQUIRE(assoc->_size > 0);
		REQUIRE(memcmp(assoc->_field, filedata, strlen(filedata)) == 0);

		// Verify label
		auto label = reader.getAssocLabel("TRIG");
		REQUIRE(label->_size > 0);
		REQUIRE(strncmp((const char*)label->_field, "Trigger point", 13) == 0);
	}

	std::remove(test_wav_path);
}

TEST_CASE("Round-trip: write then read back verifies sample integrity", "[wavfile]") {
	const uint32_t num_samples = 256;
	const uint32_t data_bytes = num_samples * sizeof(sample_stereo_16_t);

	uint8_t sound_data[data_bytes];
	fill_sawtooth(sound_data, num_samples);

	// Write
	{
		smart::WavFileSimplePcm simple(2, 44100, 16);
		simple.addData(sound_data, data_bytes);

		FILE* f = fopen(test_wav_path, "wb");
		REQUIRE(f != nullptr);
		simple.writeFile(f);
		fclose(f);
	}

	// Read back and verify sample data
	{
		smart::WavFileDiskPcm reader(test_wav_path);
		REQUIRE(reader.hasData());

		auto it = reader.getIterator(0);
		REQUIRE(it != nullptr);

		uint32_t bytes_per_sample = reader.getBytesPerSample();
		auto first_sample = it->getSample(1);
		REQUIRE(first_sample->_size == bytes_per_sample);

		// First sample should match what we wrote
		auto* s = reinterpret_cast<sample_stereo_16_t*>(first_sample->_field);
		REQUIRE(s->ch0 == 65);
		REQUIRE(s->ch1 == -65);
	}

	std::remove(test_wav_path);
}

// ===========================================================================
// wav_verify tests
// ===========================================================================

TEST_CASE("wav_verify: in-memory buffer from fillBuffer", "[wavfile][verify]") {
	const uint32_t num_samples = 1000;
	const uint32_t data_bytes = num_samples * sizeof(sample_stereo_16_t);

	uint8_t sound_data[data_bytes];
	fill_sawtooth(sound_data, num_samples);

	smart::WavFile::RiffChunk riffchunk("WAVE");
	smart::WavFile::PcmChunk pcmchunk(&riffchunk, 2, 44100, 16);
	smart::WavFile::PcmDataChunk pcmdata(&riffchunk);
	pcmdata.addPiece(sound_data, data_bytes);

	uint32_t maxlen = riffchunk.getSize();
	std::vector<uint8_t> file_buf(maxlen);
	uint8_t* ptr = file_buf.data();
	uint32_t written = riffchunk.fillBuffer(&ptr, maxlen);
	REQUIRE(written > 0);

	auto r = wav_verify(file_buf.data(), written);
	INFO(r.summary());

	// RIFF structure
	REQUIRE(r.has_riff);
	REQUIRE(r.has_wave_form);

	// fmt fields
	REQUIRE(r.has_fmt);
	REQUIRE(r.format_tag == 1);       // PCM
	REQUIRE(r.channels == 2);
	REQUIRE(r.samples_per_sec == 44100);
	REQUIRE(r.bits_per_sample == 16);
	REQUIRE(r.block_align == 4);      // 2 channels * 2 bytes

	// data chunk
	REQUIRE(r.has_data);
	REQUIRE(r.data_ck_size == data_bytes);

	// RIFF size consistency
	REQUIRE(r.riff_ck_size + 8 == written);
}

TEST_CASE("wav_verify_file: file written with writeFile", "[wavfile][verify]") {
	const uint32_t num_samples = 1000;
	const uint32_t data_bytes = num_samples * sizeof(sample_stereo_16_t);

	uint8_t sound_data[data_bytes];
	fill_sawtooth(sound_data, num_samples);

	smart::WavFile::RiffChunk riffchunk("WAVE");
	smart::WavFile::PcmChunk pcmchunk(&riffchunk, 2, 44100, 16);
	smart::WavFile::PcmDataChunk pcmdata(&riffchunk);
	pcmdata.addPiece(sound_data, data_bytes);

	const char* path = "/tmp/test_wavfile_verify.wav";
	FILE* f = fopen(path, "wb");
	REQUIRE(f != nullptr);
	uint32_t written = riffchunk.writeFile(f);
	fclose(f);
	REQUIRE(written > 0);

	auto r = wav_verify_file(path);
	INFO(r.summary());

	REQUIRE(r.has_riff);
	REQUIRE(r.has_wave_form);
	REQUIRE(r.has_fmt);
	REQUIRE(r.has_data);
	REQUIRE(r.format_tag == 1);
	REQUIRE(r.channels == 2);
	REQUIRE(r.samples_per_sec == 44100);
	REQUIRE(r.data_ck_size == data_bytes);

	std::remove(path);
}

TEST_CASE("wav_verify: WavFileSimplePcm with cue, labels, files", "[wavfile][verify]") {
	const uint32_t num_samples = 500;
	const uint32_t data_bytes = num_samples * sizeof(sample_stereo_16_t);

	uint8_t sound_data[data_bytes];
	fill_sawtooth(sound_data, num_samples);

	const char* filedata = "property1=hello\nproperty2=world\n";

	smart::WavFileSimplePcm simple(2, 44100, 16);
	simple.addData(sound_data, data_bytes);
	simple.addCuePoint("CNFG", 0);
	simple.addCuePoint("TRIG", 250, "Trigger point");
	simple.addAssocFile("CNFG", "TXT", filedata, strlen(filedata) + 1);

	const char* path = "/tmp/test_wavfile_verify_cue.wav";
	FILE* f = fopen(path, "wb");
	REQUIRE(f != nullptr);
	simple.writeFile(f);
	fclose(f);

	auto r = wav_verify_file(path);
	INFO(r.summary());

	// Basic structure
	REQUIRE(r.has_riff);
	REQUIRE(r.has_wave_form);
	REQUIRE(r.has_fmt);
	REQUIRE(r.has_data);

	// Cue chunk
	REQUIRE(r.has_cue);
	REQUIRE(r.cue_points_declared == 2);

	// LIST/adtl
	REQUIRE(r.has_list_adtl);
	REQUIRE(r.label_count >= 1);   // "Trigger point" label
	REQUIRE(r.file_count >= 1);    // CNFG file

	std::remove(path);
}

TEST_CASE("wav_verify: detect P1/P2 issues with odd-length labels", "[wavfile][verify]") {
	const uint32_t num_samples = 100;
	const uint32_t data_bytes = num_samples * sizeof(sample_stereo_16_t);

	uint8_t sound_data[data_bytes];
	fill_sawtooth(sound_data, num_samples);

	// Use a label string whose null-terminated length is odd, to trigger P1/P2
	// "ab" -> strlen=2, +1 null = 3 bytes string data, +4 dwName = 7 bytes total -> odd ckSize
	smart::WavFileSimplePcm simple(2, 44100, 16);
	simple.addData(sound_data, data_bytes);
	simple.addCuePoint("TEST", 50, "ab");

	const char* path = "/tmp/test_wavfile_verify_p1p2.wav";
	FILE* f = fopen(path, "wb");
	REQUIRE(f != nullptr);
	simple.writeFile(f);
	fclose(f);

	auto r = wav_verify_file(path);
	INFO(r.summary());

	// Check that at least one of the known library issues is detected
	bool has_p1 = r.has_issue_tagged("P1_NO_PAD");
	bool has_p2 = r.has_issue_tagged("P2_PADDED_CKSIZE");
	// The library has known issues P1 and/or P2 with odd-length label data
	CHECK((has_p1 || has_p2));

	std::remove(path);
}

TEST_CASE("wav_verify: 24-bit 3-channel odd-frame round-trip", "[wavfile][verify]") {
	// blockAlign = 3 ch * 3 bytes = 9 (odd), exercising non-trivial packing.
	// The library's PcmDataChunk rounds rows to 4-byte alignment internally
	// (_row_length=12 for this format), so num_frames must satisfy
	// num_frames * 9 % 12 == 0, i.e. num_frames is a multiple of 4.
	const uint32_t num_frames = 136;
	const uint32_t num_channels = 3;
	const uint32_t bytes_per_frame = num_channels * 3; // 3 ch * 3 bytes = 9
	const uint32_t data_bytes = num_frames * bytes_per_frame; // 136 * 9 = 1224

	// Generate counter data: frame 0 = {1,2,3}, frame 1 = {4,5,6}, ...
	std::vector<uint8_t> sound_data(data_bytes);
	int32_t counter = 1;
	for (uint32_t f = 0; f < num_frames; f++) {
		for (uint32_t ch = 0; ch < num_channels; ch++) {
			write_i24_le(sound_data.data() + f * bytes_per_frame + ch * 3, counter);
			counter++;
		}
	}

	const char* path = "/tmp/test_wavfile_24bit_3ch.wav";

	// Write via WavFileSimplePcm
	{
		smart::WavFileSimplePcm simple(num_channels, 48000, 24);
		simple.addData(sound_data.data(), data_bytes);

		FILE* f = fopen(path, "wb");
		REQUIRE(f != nullptr);
		uint32_t written = simple.writeFile(f);
		fclose(f);
		REQUIRE(written > 0);
	}

	// Structural verification via wav_verify
	{
		auto r = wav_verify_file(path);
		INFO(r.summary());

		REQUIRE(r.has_fmt);
		REQUIRE(r.format_tag == 1);
		REQUIRE(r.channels == 3);
		REQUIRE(r.bits_per_sample == 24);
		REQUIRE(r.block_align == 9);
		REQUIRE(r.samples_per_sec == 48000);
		REQUIRE(r.avg_bytes_per_sec == 48000u * 9u);
		REQUIRE(r.has_data);
		REQUIRE(r.data_ck_size == 1224);
	}

	// Read back and verify every sample
	{
		smart::WavFileDiskPcm reader(path);
		REQUIRE(reader.hasData());
		REQUIRE(reader.getBytesPerSample() == 9);
		REQUIRE(reader.getSampleCount() == 136);

		auto it = reader.getIterator(0);
		REQUIRE(it != nullptr);

		counter = 1;
		for (uint32_t f = 0; f < num_frames; f++) {
			auto sample = it->getSampleInc();
			REQUIRE(sample->_field != nullptr);
			REQUIRE(sample->_size == bytes_per_frame);

			const auto* p = static_cast<const uint8_t*>(sample->_field);
			for (uint32_t ch = 0; ch < num_channels; ch++) {
				int32_t got = read_i24_le(p + ch * 3);
				REQUIRE(got == counter);
				counter++;
			}
		}
	}

	std::remove(path);
}
