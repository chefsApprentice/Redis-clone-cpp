// File: tests/serialisation_test.cpp

#include "utils/serialisation.h"
#include "utils/req_res.h"
#include "utils/buffer.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using std::string;

// Build `b` with the append_* writers, then assert its serialised form and
// that the parser consumed the whole buffer.
static void check_value(Buffer& b, const std::string& expect, int32_t expect_used) {
    std::string s;
    const int32_t used = print_response(b.data_start, buf_size(&b), s);
    EXPECT_EQ(used, expect_used);
    EXPECT_EQ(s, expect);
}

TEST(SerialisationTest, Nil) {
    Buffer b;
    append_nil(b);
    check_value(b, "(nil)\n", 1);
}

TEST(SerialisationTest, Str) {
    Buffer b;
    append_str(b, "hello");
    check_value(b, "(str) hello\n", 10);

    Buffer empty;
    append_str(empty, "");
    check_value(empty, "(str) \n", 5);

    // Bytes are copied verbatim, NUL included.
    Buffer nul;
    append_str(nul, std::string_view("a\0b", 3));
    check_value(nul, std::string("(str) a\0b\n", 10), 8);
}

TEST(SerialisationTest, Int) {
    const int64_t vals[] = {
        0,
        -1,
        std::numeric_limits<int64_t>::min(),
        std::numeric_limits<int64_t>::max(),
    };
    for (const int64_t v : vals) {
        Buffer b;
        append_int(b, v);
        check_value(b, "(int) " + std::to_string(v) + "\n", 9);
    }
}

TEST(SerialisationTest, Dbl) {
    Buffer b1;
    append_dbl(b1, 0.5);
    check_value(b1, "(dbl) 0.5\n", 9);

    Buffer b2;
    append_dbl(b2, -2.5);
    check_value(b2, "(dbl) -2.5\n", 9);

    Buffer b3;
    append_dbl(b3, 1e300);
    check_value(b3, "(dbl) 1e+300\n", 9); // pins %g formatting
}

TEST(SerialisationTest, Err) {
    Buffer b;
    append_err(b, 1, "unknown command");
    check_value(b, "(err) 1 unknown command\n", 24);
}

TEST(SerialisationTest, ArrEmpty) {
    Buffer b;
    append_arr_header(b, 0);
    check_value(b, "(arr) len=0\n(arr) end\n", 5);
}

TEST(SerialisationTest, ArrNested) {
    Buffer b;
    append_arr_header(b, 2);
    append_str(b, "hi");
    append_arr_header(b, 1);
    append_int(b, 42);
    check_value(b, "(arr) len=2\n(str) hi\n(arr) len=1\n(int) 42\n(arr) end\n(arr) end\n",
                static_cast<int32_t>(buf_size(&b)));
}

// A full payload cut short at the exact boundary must be rejected; reduce
// sizes here, never via check_value.
TEST(SerialisationTest, TruncatedStr) {
    Buffer b;
    append_str(b, "1234567890"); // n=10, only 3 payload bytes in some cases below
    std::string s;
    EXPECT_EQ(print_response(b.data_start, 8, s), -1); // 1 tag + 4 len + 3 payload
    EXPECT_EQ(print_response(b.data_start, 5, s), -1); // header only
    EXPECT_EQ(print_response(b.data_start, 4, s), -1); // tag + partial length
    EXPECT_EQ(print_response(b.data_start, 0, s), -1);
}

TEST(SerialisationTest, TruncatedErr) {
    Buffer b;
    append_err(b, 7, "boom");
    std::string s;
    EXPECT_EQ(print_response(b.data_start, 5, s), -1); // tag + code only
    EXPECT_EQ(print_response(b.data_start, 1, s), -1); // tag only
}

TEST(SerialisationTest, TruncatedInt) {
    Buffer b;
    append_int(b, 42);
    std::string s;
    EXPECT_EQ(print_response(b.data_start, 8, s), -1); // tag + 7 bytes
    EXPECT_EQ(print_response(b.data_start, 1, s), -1); // tag only
}

TEST(SerialisationTest, TruncatedDbl) {
    Buffer b;
    append_dbl(b, 0.5);
    std::string s;
    EXPECT_EQ(print_response(b.data_start, 8, s), -1); // tag + 7 bytes
    EXPECT_EQ(print_response(b.data_start, 1, s), -1); // tag only
}

TEST(SerialisationTest, TruncatedArr) {
    // Header claims 1 child that is missing entirely.
    Buffer b;
    append_arr_header(b, 1);
    std::string s;
    EXPECT_EQ(print_response(b.data_start, 5, s), -1);

    // Header + child cut short: int child truncated to 4 bytes.
    Buffer b2;
    append_arr_header(b2, 1);
    append_int(b2, 42);
    EXPECT_EQ(print_response(b2.data_start, 9, s), -1); // 5 header + 4 child bytes
}

TEST(SerialisationTest, BadTag) {
    Buffer b;
    const uint8_t bad = 0x7F;
    buf_append(b, &bad, 1);
    std::string s;
    EXPECT_EQ(print_response(b.data_start, 1, s), -1);
}

// Through the real server path: g_data is a file-static global shared across
// cases, so keys used here are unique to this file.
TEST(SerialisationTest, doRequestIntegration) {
    const auto run = [](const std::vector<std::string>& requested, const std::string& expect) {
        std::vector<std::string> cmd = requested;
        Buffer buf;
        do_request(cmd, buf);
        uint32_t len = 0;
        memcpy(&len, buf.data_start, 4);
        ASSERT_EQ(4 + (size_t)len, buf_size(&buf)); // patch_res_len regression check
        std::string s;
        const int32_t used = print_response(buf.data_start + 4, len, s);
        ASSERT_EQ(used, (int32_t)len); // no trailing garbage inside the frame
        EXPECT_EQ(s, expect);
    };

    run({"get", "missing-key"}, "(nil)\n");
    run({"set", "s_k", "hi"}, "(int) 0\n"); // RES_OK
    run({"get", "s_k"}, "(str) hi\n");
    run({"del", "absent-key"}, "(int) 2\n"); // RES_NX: "Not eXist"
    run({"set", "d_k", "v"}, "(int) 0\n");   // RES_OK
    run({"del", "d_k"}, "(int) 0\n");        // RES_OK
    run({"noflub"}, "(err) 1 unknown command\n"); // code = RES_ERR
    run({"get"}, "(err) 1 unknown command\n");    // arity mismatch, code = RES_ERR
}