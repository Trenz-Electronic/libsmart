#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Byte-level read helpers (little-endian, no alignment requirement)
// ---------------------------------------------------------------------------

inline uint16_t read_u16_le(const uint8_t* buf) {
    return static_cast<uint16_t>(buf[0] | (buf[1] << 8));
}

inline uint32_t read_u32_le(const uint8_t* buf) {
    return static_cast<uint32_t>(
        buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
}

inline std::string read_fourcc(const uint8_t* buf) {
    return std::string(reinterpret_cast<const char*>(buf), 4);
}

// ---------------------------------------------------------------------------
// Issue severity / issue record
// ---------------------------------------------------------------------------

enum class WavIssueLevel { error, warning, info };

struct WavIssue {
    WavIssueLevel level;
    std::string   tag;
    std::string   detail;
};

// ---------------------------------------------------------------------------
// Chunk info record (one per chunk discovered during iteration)
// ---------------------------------------------------------------------------

struct WavChunkInfo {
    std::string id;          // fourcc, e.g. "fmt ", "data"
    uint32_t    ck_size;     // value from the chunk header
    size_t      offset;      // byte offset of the chunk header in the buffer
};

// ---------------------------------------------------------------------------
// Verification result
// ---------------------------------------------------------------------------

struct WavVerifyResult {
    bool     valid = false;

    // RIFF
    bool     has_riff       = false;
    uint32_t riff_ck_size   = 0;
    bool     has_wave_form  = false;

    // fmt
    bool     has_fmt            = false;
    uint16_t format_tag         = 0;
    uint16_t channels           = 0;
    uint32_t samples_per_sec    = 0;
    uint32_t avg_bytes_per_sec  = 0;
    uint16_t block_align        = 0;
    uint16_t bits_per_sample    = 0;

    // data
    bool     has_data            = false;
    uint32_t data_ck_size        = 0;
    size_t   data_payload_offset = 0;

    // cue (optional)
    bool     has_cue              = false;
    uint32_t cue_points_declared  = 0;
    uint32_t cue_points_fit       = 0;

    // LIST/adtl (optional)
    bool     has_list_adtl  = false;
    uint32_t label_count    = 0;
    uint32_t file_count     = 0;

    // All chunks discovered, in order
    std::vector<WavChunkInfo> chunks;

    // Issues
    std::vector<WavIssue> issues;

    // Helpers
    bool has_errors() const {
        for (auto& i : issues)
            if (i.level == WavIssueLevel::error) return true;
        return false;
    }

    bool has_issue_tagged(const std::string& tag) const {
        for (auto& i : issues)
            if (i.tag == tag) return true;
        return false;
    }

    std::string summary() const {
        std::string s;
        s += "WAV verify: valid=" + std::string(valid ? "yes" : "no") + "\n";
        s += "  RIFF: " + std::string(has_riff ? "yes" : "no");
        if (has_riff)
            s += "  ckSize=" + std::to_string(riff_ck_size);
        s += "  WAVE=" + std::string(has_wave_form ? "yes" : "no") + "\n";
        if (has_fmt) {
            s += "  fmt: tag=" + std::to_string(format_tag)
                 + " ch=" + std::to_string(channels)
                 + " rate=" + std::to_string(samples_per_sec)
                 + " avgBps=" + std::to_string(avg_bytes_per_sec)
                 + " blockAlign=" + std::to_string(block_align)
                 + " bits=" + std::to_string(bits_per_sample) + "\n";
        }
        if (has_data)
            s += "  data: ckSize=" + std::to_string(data_ck_size)
                 + " payloadOff=" + std::to_string(data_payload_offset) + "\n";
        if (has_cue)
            s += "  cue: declared=" + std::to_string(cue_points_declared)
                 + " fit=" + std::to_string(cue_points_fit) + "\n";
        if (has_list_adtl)
            s += "  LIST/adtl: labels=" + std::to_string(label_count)
                 + " files=" + std::to_string(file_count) + "\n";
        s += "  chunks(" + std::to_string(chunks.size()) + "):";
        for (auto& c : chunks)
            s += " [" + c.id + " sz=" + std::to_string(c.ck_size)
                 + " @" + std::to_string(c.offset) + "]";
        s += "\n";
        if (!issues.empty()) {
            s += "  issues(" + std::to_string(issues.size()) + "):\n";
            for (auto& i : issues) {
                const char* lv = (i.level == WavIssueLevel::error)   ? "ERROR"
                               : (i.level == WavIssueLevel::warning) ? "WARN"
                               :                                       "INFO";
                s += "    " + std::string(lv) + " " + i.tag + ": " + i.detail + "\n";
            }
        }
        return s;
    }
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace wav_verify_detail {

inline void add_issue(WavVerifyResult& r, WavIssueLevel lv,
                      const std::string& tag, const std::string& detail) {
    r.issues.push_back({lv, tag, detail});
}

inline void parse_fmt(WavVerifyResult& r, const uint8_t* data, size_t len,
                      uint32_t ck_size) {
    r.has_fmt = true;
    if (ck_size < 16) {
        add_issue(r, WavIssueLevel::error, "FMT_TOO_SHORT",
                  "fmt ckSize=" + std::to_string(ck_size) + " < 16");
        return;
    }
    r.format_tag        = read_u16_le(data + 0);
    r.channels          = read_u16_le(data + 2);
    r.samples_per_sec   = read_u32_le(data + 4);
    r.avg_bytes_per_sec = read_u32_le(data + 8);
    r.block_align       = read_u16_le(data + 12);
    r.bits_per_sample   = read_u16_le(data + 14);

    // PCM consistency checks
    if (r.format_tag == 1) {
        uint16_t bytes_per_sample = r.bits_per_sample / 8;
        uint16_t expected_align = r.channels * bytes_per_sample;
        if (r.block_align != expected_align) {
            add_issue(r, WavIssueLevel::error, "BAD_BLOCK_ALIGN",
                      "blockAlign=" + std::to_string(r.block_align)
                      + " expected=" + std::to_string(expected_align));
        }
        uint32_t expected_avg = r.samples_per_sec * r.block_align;
        if (r.avg_bytes_per_sec != expected_avg) {
            add_issue(r, WavIssueLevel::error, "BAD_AVG_BYTES",
                      "avgBytesPerSec=" + std::to_string(r.avg_bytes_per_sec)
                      + " expected=" + std::to_string(expected_avg));
        }
    }
}

inline void parse_cue(WavVerifyResult& r, const uint8_t* data, size_t len,
                      uint32_t ck_size) {
    r.has_cue = true;
    if (ck_size < 4) return;
    r.cue_points_declared = read_u32_le(data);
    // Each cue point is 24 bytes
    r.cue_points_fit = (ck_size - 4) / 24;
    if (r.cue_points_declared != r.cue_points_fit) {
        add_issue(r, WavIssueLevel::warning, "CUE_COUNT_MISMATCH",
                  "declared=" + std::to_string(r.cue_points_declared)
                  + " fit=" + std::to_string(r.cue_points_fit));
    }
}

inline void parse_list(WavVerifyResult& r, const uint8_t* chunk_data,
                       size_t chunk_data_len, uint32_t ck_size,
                       const uint8_t* buf, size_t buf_len) {
    if (ck_size < 4) return;
    std::string form = read_fourcc(chunk_data);
    if (form != "adtl") return;

    r.has_list_adtl = true;

    // Iterate sub-chunks within LIST payload (after the 4-byte form type)
    size_t cursor = 4;
    while (cursor + 8 <= ck_size) {
        std::string sub_id = read_fourcc(chunk_data + cursor);
        uint32_t sub_sz    = read_u32_le(chunk_data + cursor + 4);

        if (cursor + 8 + sub_sz > ck_size) {
            add_issue(r, WavIssueLevel::error, "LIST_SUBCHUNK_OVERFLOW",
                      "sub-chunk '" + sub_id + "' at LIST offset "
                      + std::to_string(cursor) + " overflows LIST payload");
            break;
        }

        if (sub_id == "labl" || sub_id == "ltxt" || sub_id == "note") {
            r.label_count++;

            // Detect P2: check if ckSize includes padding bytes
            // A label's data is: 4-byte dwName + null-terminated string.
            // The true data length should be 4 + strlen(str) + 1.
            // If ckSize is larger due to 4-byte alignment padding, flag it.
            if (sub_id == "labl" && sub_sz > 4) {
                const uint8_t* str_start = chunk_data + cursor + 8 + 4;
                size_t str_max = sub_sz - 4;
                size_t str_len = 0;
                while (str_len < str_max && str_start[str_len] != 0) str_len++;
                uint32_t true_data = 4 + static_cast<uint32_t>(str_len) + 1; // dwName + string + null
                if (sub_sz > true_data) {
                    // Check if the extra bytes are zero padding
                    bool all_zero = true;
                    for (uint32_t p = true_data; p < sub_sz; p++) {
                        if ((chunk_data + cursor + 8)[p] != 0) { all_zero = false; break; }
                    }
                    if (all_zero) {
                        add_issue(r, WavIssueLevel::warning, "P2_PADDED_CKSIZE",
                                  "labl ckSize=" + std::to_string(sub_sz)
                                  + " includes " + std::to_string(sub_sz - true_data)
                                  + " padding byte(s)");
                    }
                }
            }
        } else if (sub_id == "file") {
            r.file_count++;
        }

        // Advance past sub-chunk, with word-alignment
        size_t advance = 8 + sub_sz;
        if (sub_sz & 1) advance++;  // pad byte
        cursor += advance;
    }
}

} // namespace wav_verify_detail

// ---------------------------------------------------------------------------
// Main verification function
// ---------------------------------------------------------------------------

inline WavVerifyResult wav_verify(const uint8_t* data, size_t len) {
    using namespace wav_verify_detail;
    WavVerifyResult r;

    // --- RIFF header (offsets 0-11) ---
    if (len < 12) {
        add_issue(r, WavIssueLevel::error, "MISSING_FMT", "buffer too short for RIFF header");
        add_issue(r, WavIssueLevel::error, "MISSING_DATA", "buffer too short for RIFF header");
        return r;
    }

    std::string magic = read_fourcc(data);
    if (magic != "RIFF") {
        add_issue(r, WavIssueLevel::error, "MISSING_FMT", "not a RIFF file");
        add_issue(r, WavIssueLevel::error, "MISSING_DATA", "not a RIFF file");
        return r;
    }

    r.has_riff     = true;
    r.riff_ck_size = read_u32_le(data + 4);

    std::string form = read_fourcc(data + 8);
    r.has_wave_form = (form == "WAVE");

    if (static_cast<size_t>(r.riff_ck_size) + 8 != len) {
        add_issue(r, WavIssueLevel::error, "RIFF_SIZE_MISMATCH",
                  "riff_ck_size+8=" + std::to_string(r.riff_ck_size + 8)
                  + " buffer_len=" + std::to_string(len));
    }

    // End of RIFF payload (clamp to buffer length for safety)
    size_t riff_end = std::min(static_cast<size_t>(r.riff_ck_size) + 8, len);

    // --- Iterate sub-chunks at cursor=12 ---
    size_t cursor = 12;
    while (cursor + 8 <= riff_end) {
        std::string ck_id = read_fourcc(data + cursor);
        uint32_t ck_size  = read_u32_le(data + cursor + 4);

        r.chunks.push_back({ck_id, ck_size, cursor});

        // Check chunk doesn't overflow RIFF payload
        if (cursor + 8 + ck_size > riff_end) {
            add_issue(r, WavIssueLevel::error, "CHUNK_OVERFLOW",
                      "chunk '" + ck_id + "' at offset " + std::to_string(cursor)
                      + " ckSize=" + std::to_string(ck_size)
                      + " extends past RIFF payload end=" + std::to_string(riff_end));
            break;
        }

        const uint8_t* ck_data = data + cursor + 8;
        size_t ck_data_len = ck_size;

        // Dispatch
        if (ck_id == "fmt ") {
            parse_fmt(r, ck_data, ck_data_len, ck_size);
        } else if (ck_id == "data") {
            r.has_data = true;
            r.data_ck_size = ck_size;
            r.data_payload_offset = cursor + 8;
        } else if (ck_id == "cue ") {
            parse_cue(r, ck_data, ck_data_len, ck_size);
        } else if (ck_id == "LIST") {
            parse_list(r, ck_data, ck_data_len, ck_size, data, len);
        }

        // Advance cursor: 8 (header) + ckSize + optional pad byte
        size_t advance = 8 + ck_size;
        if (ck_size & 1) {
            // Odd-sized chunk: check for pad byte
            size_t pad_pos = cursor + 8 + ck_size;
            if (pad_pos < riff_end) {
                if (data[pad_pos] != 0) {
                    add_issue(r, WavIssueLevel::warning, "P1_NO_PAD",
                              "chunk '" + ck_id + "' at offset " + std::to_string(cursor)
                              + " has odd ckSize=" + std::to_string(ck_size)
                              + " but pad byte is 0x"
                              + std::string(1, "0123456789abcdef"[(data[pad_pos] >> 4) & 0xf])
                              + std::string(1, "0123456789abcdef"[data[pad_pos] & 0xf])
                              + " instead of 0x00");
                }
                advance++;  // skip pad byte
            } else {
                add_issue(r, WavIssueLevel::warning, "P1_NO_PAD",
                          "chunk '" + ck_id + "' at offset " + std::to_string(cursor)
                          + " has odd ckSize=" + std::to_string(ck_size)
                          + " but no room for pad byte");
            }
        }
        cursor += advance;
    }

    // --- Post-parse checks ---
    if (!r.has_fmt)
        add_issue(r, WavIssueLevel::error, "MISSING_FMT", "no fmt chunk found");
    if (!r.has_data)
        add_issue(r, WavIssueLevel::error, "MISSING_DATA", "no data chunk found");

    r.valid = !r.has_errors();
    return r;
}

// ---------------------------------------------------------------------------
// File wrapper
// ---------------------------------------------------------------------------

inline WavVerifyResult wav_verify_file(const std::string& path) {
    WavVerifyResult r;

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        r.issues.push_back({WavIssueLevel::error, "FILE_OPEN_FAILED",
                            "cannot open: " + path});
        return r;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(f);
        r.issues.push_back({WavIssueLevel::error, "FILE_EMPTY",
                            "empty or unreadable: " + path});
        return r;
    }

    std::vector<uint8_t> buf(static_cast<size_t>(fsize));
    size_t nread = fread(buf.data(), 1, buf.size(), f);
    fclose(f);

    return wav_verify(buf.data(), nread);
}
