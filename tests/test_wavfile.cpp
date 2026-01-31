#include <catch2/catch_test_macros.hpp>
#include <smart/WavFileDisk.h>
#include <smart/WavFileSimple.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

static const char* test_wav_path = "/tmp/test_wavfile.wav";

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
