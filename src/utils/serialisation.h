#pragma once

#include "utils/buffer.h"
#include <cstdint>
#include <string>
#include <string_view>

// Serialised value wire format, one tag byte followed by the encoding below.
// Lengths/ints are stored in host (little-endian) byte order via memcpy,
// consistent with read_u32() in req_res.cpp (client comment: "assume little endian").
enum class Tag : uint8_t {
    TAG_NIL = 0, // nil
    TAG_ERR = 1, // int32 code + u32 len + msg
    TAG_STR = 2, // u32 len + bytes
    TAG_INT = 3, // int64
    TAG_DBL = 4, // double
    TAG_ARR = 5, // u32 count + count serialised values, in order
};

// Append one serialised value. buf_append failures are ignored, matching
// every existing call site (do_request, send_req).
void append_nil(Buffer& out);
void append_str(Buffer& out, std::string_view s);
void append_int(Buffer& out, int64_t v);
void append_dbl(Buffer& out, double v);
void append_err(Buffer& out, int32_t code, std::string_view msg);
void append_arr_header(Buffer& out, uint32_t count); // then append children in order

// Parse ONE serialised value starting at `data` with `size` bytes available.
// Appends the human-readable form to `dst`:
//   "(nil)\n", "(err) <code> <msg>\n", "(str) <bytes>\n", "(int) <v>\n",
//   "(dbl) <v>\n", "(arr) len=<n>\n" + one line per element + "(arr) end\n"
// Returns bytes consumed, or -1 when malformed/truncated.
// Does NOT print error text; caller decides (client prints "bad response").
int32_t print_response(const uint8_t* data, size_t size, std::string& dst);