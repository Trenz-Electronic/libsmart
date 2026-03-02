#include <catch2/catch_test_macros.hpp>
#include "wav_verify.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Byte-level write helpers (little-endian)
// ---------------------------------------------------------------------------

static void put_u32_le(std::vector<uint8_t>& buf, size_t off, uint32_t v) {
	buf[off+0] = static_cast<uint8_t>(v);
	buf[off+1] = static_cast<uint8_t>(v >> 8);
	buf[off+2] = static_cast<uint8_t>(v >> 16);
	buf[off+3] = static_cast<uint8_t>(v >> 24);
}

static void push_u16(std::vector<uint8_t>& b, uint16_t v) {
	b.push_back(static_cast<uint8_t>(v));
	b.push_back(static_cast<uint8_t>(v >> 8));
}

static void push_u32(std::vector<uint8_t>& b, uint32_t v) {
	b.push_back(static_cast<uint8_t>(v));
	b.push_back(static_cast<uint8_t>(v >> 8));
	b.push_back(static_cast<uint8_t>(v >> 16));
	b.push_back(static_cast<uint8_t>(v >> 24));
}

static void push_cc(std::vector<uint8_t>& b, const char* cc) {
	for (int i = 0; i < 4; i++)
		b.push_back(static_cast<uint8_t>(cc[i]));
}

static void push_zeros(std::vector<uint8_t>& b, size_t n) {
	b.insert(b.end(), n, 0);
}

static void push_str(std::vector<uint8_t>& b, const std::string& s) {
	for (char c : s)
		b.push_back(static_cast<uint8_t>(c));
	b.push_back(0);
}

static void dump_file(const char* path, const std::vector<uint8_t>& buf) {
	FILE* f = fopen(path, "wb");
	if (f) { fwrite(buf.data(), 1, buf.size(), f); fclose(f); }
}

static void fix_riff_size(std::vector<uint8_t>& w) {
	put_u32_le(w, 4, static_cast<uint32_t>(w.size() - 8));
}

// ---------------------------------------------------------------------------
// Reusable chunk builders
// ---------------------------------------------------------------------------

static void push_fmt_chunk(std::vector<uint8_t>& b) {
	push_cc(b, "fmt ");
	push_u32(b, 16);
	push_u16(b, 1);       // PCM
	push_u16(b, 2);       // channels
	push_u32(b, 44100);   // sample rate
	push_u32(b, 176400);  // avg bytes/sec
	push_u16(b, 4);       // block align
	push_u16(b, 16);      // bits per sample
}

struct CuePointDef {
	uint32_t id;
	uint32_t position;
	uint32_t sample_offset;
};

static void push_cue_chunk(std::vector<uint8_t>& b,
                            const std::vector<CuePointDef>& pts) {
	uint32_t n = static_cast<uint32_t>(pts.size());
	push_cc(b, "cue ");
	push_u32(b, 4 + n * 24);
	push_u32(b, n);
	for (auto& p : pts) {
		push_u32(b, p.id);
		push_u32(b, p.position);
		push_cc(b, "data");
		push_u32(b, 0);
		push_u32(b, 0);
		push_u32(b, p.sample_offset);
	}
}

// ---------------------------------------------------------------------------
// build_minimal_wav: RIFF + fmt + data  (stereo 16-bit 44100, 100 samples)
// Total: 12 + 24 + 408 = 444 bytes.  data ckSize field is at offset 40.
// ---------------------------------------------------------------------------

static std::vector<uint8_t> build_minimal_wav() {
	std::vector<uint8_t> w;
	push_cc(w, "RIFF");
	push_u32(w, 0);
	push_cc(w, "WAVE");
	push_fmt_chunk(w);
	push_cc(w, "data");
	push_u32(w, 400);
	push_zeros(w, 400);
	fix_riff_size(w);
	return w;
}

// ---------------------------------------------------------------------------
// build_wav_with_cue_and_label: RIFF + fmt + cue(1) + LIST/adtl(1 labl) + data
// Correctly word-aligned.
// ---------------------------------------------------------------------------

static std::vector<uint8_t> build_wav_with_cue_and_label(const std::string& label) {
	std::vector<uint8_t> w;
	push_cc(w, "RIFF");
	push_u32(w, 0);
	push_cc(w, "WAVE");
	push_fmt_chunk(w);
	push_cue_chunk(w, {{1, 0, 0}});

	uint32_t labl_data = 4 + static_cast<uint32_t>(label.size()) + 1;
	uint32_t labl_padded = labl_data + (labl_data & 1);
	uint32_t list_data = 4 + 8 + labl_padded;

	push_cc(w, "LIST");
	push_u32(w, list_data);
	push_cc(w, "adtl");
	push_cc(w, "labl");
	push_u32(w, labl_data);
	push_u32(w, 1);
	push_str(w, label);
	if (labl_data & 1)
		w.push_back(0);

	if (list_data & 1)
		w.push_back(0);

	push_cc(w, "data");
	push_u32(w, 400);
	push_zeros(w, 400);
	fix_riff_size(w);
	return w;
}

// ===========================================================================
// Baseline: verify helpers produce valid WAVs
// ===========================================================================

TEST_CASE("baseline: helpers produce valid WAVs", "[wav-faults]") {
	SECTION("minimal wav") {
		auto w = build_minimal_wav();
		auto r = wav_verify(w.data(), w.size());
		INFO(r.summary());
		CHECK(r.valid);
		CHECK(r.has_fmt);
		CHECK(r.has_data);
		CHECK(r.data_ck_size == 400);
	}

	SECTION("wav with cue and even-length label") {
		auto w = build_wav_with_cue_and_label("tes");
		auto r = wav_verify(w.data(), w.size());
		INFO(r.summary());
		CHECK(r.valid);
		CHECK(r.has_cue);
		CHECK(r.has_list_adtl);
	}

	SECTION("wav with cue and odd-length label") {
		auto w = build_wav_with_cue_and_label("ab");
		auto r = wav_verify(w.data(), w.size());
		INFO(r.summary());
		CHECK(r.valid);
	}
}

// ===========================================================================
// P1: Missing pad byte after odd-sized chunk
// ===========================================================================

TEST_CASE("P1: missing pad byte after odd-sized chunk", "[wav-faults]") {
	std::vector<uint8_t> w;
	push_cc(w, "RIFF");
	push_u32(w, 0);
	push_cc(w, "WAVE");
	push_fmt_chunk(w);
	push_cue_chunk(w, {{1, 0, 0}});

	// LIST/adtl with labl "ab": data = dwName(4) + "ab\0"(3) = 7 bytes (odd)
	// No pad byte after labl, no pad byte after LIST.
	// LIST ckSize = "adtl"(4) + "labl"(4) + ckSize(4) + data(7) = 19 (odd)
	push_cc(w, "LIST");
	push_u32(w, 19);
	push_cc(w, "adtl");
	push_cc(w, "labl");
	push_u32(w, 7);
	push_u32(w, 1);
	w.push_back('a');
	w.push_back('b');
	w.push_back(0);
	// NO pad bytes — this is the fault

	push_cc(w, "data");
	push_u32(w, 400);
	push_zeros(w, 400);
	fix_riff_size(w);

	dump_file("/tmp/fault_p1.wav", w);

	auto r = wav_verify(w.data(), w.size());
	INFO(r.summary());

	// wav_verify sees LIST with odd ckSize=19, finds 'd' (0x64) where pad
	// byte should be → reports P1_NO_PAD
	CHECK(r.has_issue_tagged("P1_NO_PAD"));
}

// ===========================================================================
// P2: ckSize inflated by 4-byte alignment padding
// ===========================================================================

TEST_CASE("P2: ckSize inflated by 4-byte alignment padding", "[wav-faults]") {
	std::vector<uint8_t> w;
	push_cc(w, "RIFF");
	push_u32(w, 0);
	push_cc(w, "WAVE");
	push_fmt_chunk(w);
	push_cue_chunk(w, {{1, 0, 0}});

	// labl true data: dwName(4) + "ab\0"(3) = 7 bytes
	// Inflated ckSize = 8 (4-byte aligned), extra byte is 0x00
	// ckSize 8 is even → no word-alignment pad needed after labl
	push_cc(w, "LIST");
	push_u32(w, 20);       // "adtl"(4) + labl sub-chunk(4+4+8 = 16)
	push_cc(w, "adtl");
	push_cc(w, "labl");
	push_u32(w, 8);        // inflated ckSize (true size is 7)
	push_u32(w, 1);
	w.push_back('a');
	w.push_back('b');
	w.push_back(0);
	w.push_back(0);         // padding byte counted in ckSize — this is the fault

	push_cc(w, "data");
	push_u32(w, 400);
	push_zeros(w, 400);
	fix_riff_size(w);

	dump_file("/tmp/fault_p2.wav", w);

	auto r = wav_verify(w.data(), w.size());
	INFO(r.summary());

	CHECK(r.has_issue_tagged("P2_PADDED_CKSIZE"));
}

// ===========================================================================
// P3: data ckSize smaller than actual payload
// ===========================================================================

TEST_CASE("P3: data ckSize smaller than actual payload", "[wav-faults]") {
	auto w = build_minimal_wav();

	// data ckSize at offset 40: change 400 → 333 (simulates integer division)
	// RIFF ckSize stays correct for actual file size (444 bytes)
	put_u32_le(w, 40, 333);

	dump_file("/tmp/fault_p3.wav", w);

	auto r = wav_verify(w.data(), w.size());
	INFO(r.summary());

	CHECK(r.has_data);
	CHECK(r.data_ck_size == 333);
	// data ckSize=333 is not a multiple of blockAlign=4
	CHECK(r.has_issue_tagged("P3_DATA_NOT_BLOCK_ALIGNED"));
}

// ===========================================================================
// P4: data ckSize larger than actual payload
// ===========================================================================

TEST_CASE("P4: data ckSize larger than actual payload", "[wav-faults]") {
	auto w = build_minimal_wav();

	// data ckSize at offset 40: change 400 → 500 (claims more than exists)
	// RIFF ckSize unchanged → data extends past RIFF payload end
	put_u32_le(w, 40, 500);

	dump_file("/tmp/fault_p4.wav", w);

	auto r = wav_verify(w.data(), w.size());
	INFO(r.summary());

	// wav_verify detects this: the data chunk overflows the RIFF payload
	CHECK(r.has_issue_tagged("CHUNK_OVERFLOW"));
}

// ===========================================================================
// P5: dwPosition set to sequential index instead of sample offset
// ===========================================================================

TEST_CASE("P5: dwPosition set to sequential index instead of sample offset", "[wav-faults]") {
	std::vector<uint8_t> w;
	push_cc(w, "RIFF");
	push_u32(w, 0);
	push_cc(w, "WAVE");
	push_fmt_chunk(w);

	// 2 cue points: dwPosition = 0,1 (sequential index)
	// but dwSampleOffset = 100,500 (actual sample positions)
	push_cue_chunk(w, {
		{1, 0, 100},
		{2, 1, 500},
	});

	push_cc(w, "data");
	push_u32(w, 4000);
	push_zeros(w, 4000);
	fix_riff_size(w);

	dump_file("/tmp/fault_p5.wav", w);

	auto r = wav_verify(w.data(), w.size());
	INFO(r.summary());

	CHECK(r.has_cue);
	CHECK(r.cue_points_declared == 2);
	CHECK(r.has_issue_tagged("P5_SEQ_POSITION"));
}

// ===========================================================================
// P7: Non-standard chunk ordering (cue before fmt)
// ===========================================================================

TEST_CASE("P7: non-standard chunk ordering (cue before fmt)", "[wav-faults]") {
	std::vector<uint8_t> w;
	push_cc(w, "RIFF");
	push_u32(w, 0);
	push_cc(w, "WAVE");

	// Non-standard order: cue → LIST/adtl → fmt → data
	push_cue_chunk(w, {{1, 0, 0}});

	// labl with "tes" → data = dwName(4) + "tes\0"(4) = 8 bytes (even)
	push_cc(w, "LIST");
	push_u32(w, 20);
	push_cc(w, "adtl");
	push_cc(w, "labl");
	push_u32(w, 8);
	push_u32(w, 1);
	w.push_back('t');
	w.push_back('e');
	w.push_back('s');
	w.push_back(0);

	push_fmt_chunk(w);

	push_cc(w, "data");
	push_u32(w, 400);
	push_zeros(w, 400);
	fix_riff_size(w);

	dump_file("/tmp/fault_p7.wav", w);

	auto r = wav_verify(w.data(), w.size());
	INFO(r.summary());

	CHECK(r.has_fmt);
	CHECK(r.has_data);
	CHECK(r.has_cue);
	CHECK(r.has_list_adtl);
	// File is structurally valid — fmt is still before data in this test
	CHECK(r.valid);
	// fmt IS before data here, so P7 should not fire
	CHECK_FALSE(r.has_issue_tagged("P7_FMT_AFTER_DATA"));
}

// ===========================================================================
// P7b: data chunk before fmt chunk (spec violation)
// ===========================================================================

TEST_CASE("P7b: data chunk before fmt chunk", "[wav-faults]") {
	std::vector<uint8_t> w;
	push_cc(w, "RIFF");
	push_u32(w, 0);
	push_cc(w, "WAVE");

	// Spec-violating order: cue → data → fmt
	push_cue_chunk(w, {{1, 0, 0}});

	push_cc(w, "data");
	push_u32(w, 400);
	push_zeros(w, 400);

	push_fmt_chunk(w);
	fix_riff_size(w);

	dump_file("/tmp/fault_p7b.wav", w);

	auto r = wav_verify(w.data(), w.size());
	INFO(r.summary());

	CHECK(r.has_fmt);
	CHECK(r.has_data);
	CHECK(r.has_issue_tagged("P7_FMT_AFTER_DATA"));
}

// ===========================================================================
// Stored-blob tests: load pre-built fault WAVs from tests/data/
// ===========================================================================

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "."
#endif

static std::string data_path(const char* name) {
	return std::string(TEST_DATA_DIR) + "/" + name;
}

TEST_CASE("stored blobs: verify fault detection", "[wav-faults][stored]") {
	SECTION("fault_p1.wav — missing pad byte") {
		auto r = wav_verify_file(data_path("fault_p1.wav"));
		INFO(r.summary());
		CHECK(r.has_issue_tagged("P1_NO_PAD"));
	}

	SECTION("fault_p2.wav — ckSize inflated by padding") {
		auto r = wav_verify_file(data_path("fault_p2.wav"));
		INFO(r.summary());
		CHECK(r.has_issue_tagged("P2_PADDED_CKSIZE"));
	}

	SECTION("fault_p3.wav — data ckSize not block-aligned") {
		auto r = wav_verify_file(data_path("fault_p3.wav"));
		INFO(r.summary());
		CHECK(r.has_issue_tagged("P3_DATA_NOT_BLOCK_ALIGNED"));
	}

	SECTION("fault_p5.wav — dwPosition != dwSampleOffset") {
		auto r = wav_verify_file(data_path("fault_p5.wav"));
		INFO(r.summary());
		CHECK(r.has_issue_tagged("P5_SEQ_POSITION"));
	}

	SECTION("fault_p7.wav — data before fmt") {
		auto r = wav_verify_file(data_path("fault_p7.wav"));
		INFO(r.summary());
		CHECK(r.has_issue_tagged("P7_FMT_AFTER_DATA"));
	}
}
