#include "utils/serialisation.h"
#include <cstdio>
#include <cstring>

void append_nil(Buffer& out) {
    const uint8_t t = static_cast<uint8_t>(Tag::TAG_NIL);
    buf_append(out, &t, 1);
}

void append_str(Buffer& out, std::string_view s) {
    const uint8_t t = static_cast<uint8_t>(Tag::TAG_STR);
    const uint32_t n = static_cast<uint32_t>(s.size());
    buf_append(out, &t, 1);
    buf_append(out, reinterpret_cast<const uint8_t*>(&n), 4);
    buf_append(out, reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

void append_int(Buffer& out, int64_t v) {
    const uint8_t t = static_cast<uint8_t>(Tag::TAG_INT);
    buf_append(out, &t, 1);
    buf_append(out, reinterpret_cast<const uint8_t*>(&v), 8);
}

void append_dbl(Buffer& out, double v) {
    const uint8_t t = static_cast<uint8_t>(Tag::TAG_DBL);
    buf_append(out, &t, 1);
    buf_append(out, reinterpret_cast<const uint8_t*>(&v), 8);
}

void append_err(Buffer& out, int32_t code, std::string_view msg) {
    const uint8_t t = static_cast<uint8_t>(Tag::TAG_ERR);
    const uint32_t n = static_cast<uint32_t>(msg.size());
    buf_append(out, &t, 1);
    buf_append(out, reinterpret_cast<const uint8_t*>(&code), 4);
    buf_append(out, reinterpret_cast<const uint8_t*>(&n), 4);
    buf_append(out, reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
}

void append_arr_header(Buffer& out, uint32_t count) {
    const uint8_t t = static_cast<uint8_t>(Tag::TAG_ARR);
    buf_append(out, &t, 1);
    buf_append(out, reinterpret_cast<const uint8_t*>(&count), 4);
}

auto print_response(const uint8_t* data, size_t size, std::string& dst) -> int32_t {
    if (size < 1)
        return -1;
    switch (static_cast<Tag>(data[0])) {
    case Tag::TAG_NIL:
        dst += "(nil)\n";
        return 1;
    case Tag::TAG_ERR: {
        if (size < 1 + 8)
            return -1;
        int32_t code = 0;
        uint32_t n = 0;
        memcpy(&code, &data[1], 4);
        memcpy(&n, &data[1 + 4], 4);
        if (size < 9 + static_cast<size_t>(n))
            return -1;
        dst += "(err) ";
        dst += std::to_string(code);
        dst += ' ';
        dst.append(reinterpret_cast<const char*>(&data[9]), n);
        dst += '\n';
        return static_cast<int32_t>(9 + n);
    }
    case Tag::TAG_STR: {
        if (size < 1 + 4)
            return -1;
        uint32_t n = 0;
        memcpy(&n, &data[1], 4);
        if (size < 5 + static_cast<size_t>(n))
            return -1;
        dst += "(str) ";
        dst.append(reinterpret_cast<const char*>(&data[5]), n);
        dst += '\n';
        return static_cast<int32_t>(5 + n);
    }
    case Tag::TAG_INT: {
        if (size < 1 + 8)
            return -1;
        int64_t val = 0;
        memcpy(&val, &data[1], 8);
        dst += "(int) ";
        dst += std::to_string(val);
        dst += '\n';
        return 9;
    }
    case Tag::TAG_DBL: {
        if (size < 1 + 8)
            return -1;
        double val = 0;
        memcpy(&val, &data[1], 8);
        char buf[64];
        const int n = snprintf(buf, sizeof(buf), "%g", val); // match C's %g exactly
        dst += "(dbl) ";
        dst.append(buf, static_cast<size_t>(n));
        dst += '\n';
        return 9;
    }
    case Tag::TAG_ARR: {
        if (size < 1 + 4)
            return -1;
        uint32_t count = 0;
        memcpy(&count, &data[1], 4);
        dst += "(arr) len=";
        dst += std::to_string(count);
        dst += '\n';
        size_t off = 5;
        for (uint32_t i = 0; i < count; ++i) {
            const int32_t used = print_response(&data[off], size - off, dst);
            if (used < 0)
                return used;
            off += static_cast<size_t>(used);
        }
        dst += "(arr) end\n";
        return static_cast<int32_t>(off);
    }
    default:
        return -1;
    }
}
