// File: req_res.cpp
// Author: Robin J
// Description: Key value store for req and res.

#include "utils/req_res.h"
#include "constants.h"
#include "utils/buffer.h"
#include "utils/hashtable.h"
#include "utils/serialisation.h"
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <vector>

using std::string;

auto read_u32(const uint8_t*& cur, const uint8_t* end, uint32_t& out) -> bool {
    const size_t read_size = 4;
    if (cur + read_size > end) {
        return false;
    }
    memcpy(&out, cur, read_size);
    cur += read_size;
    return true;
}

auto read_str(const uint8_t*& cur, const uint8_t* end, size_t n, string& out) -> bool {
    if (cur + n > end) {
        return false;
    }
    out.assign(cur, cur + n);
    cur += n;
    return true;
}

// verifying if message is parseabe
auto parse_req(const uint8_t* data, size_t size, std::vector<std::string>& out) -> int32_t {
    const uint8_t* end = data + size;
    uint32_t nstr = 0;
    if (!read_u32(data, end, nstr)) {
        return -1;
    }

    if (nstr > k_max_msg) {
        return -1; // safety limit
    }

    while (out.size() < nstr) {
        uint32_t len = 0;
        if (!read_u32(data, end, len)) {
            return -1;
        }
        out.push_back(std::string());
        if (!read_str(data, end, len, out.back())) {
            return -1;
        }
    }
    if (data != end) {
        return -1;
    }
    return 0;
}

static void patch_res_len(Buffer& out, uint32_t offset) {
    // Because after finding offset aka our start point we allocate length.
    uint32_t res_len = (buf_size(&out) - offset) - 4;
    memcpy(out.data_start + offset, &res_len, 4);
}

static HashTable g_data;

void do_request(std::vector<std::string>& cmd, Buffer& out) {
    auto offset = buf_size(&out);
    const uint32_t def_len = 0;
    buf_append(out, (const uint8_t*)&def_len, 4); // length placeholder, patched below

    auto res_status = RES_OK;
    if (cmd.size() == 2 && cmd[0] == "get") {
        auto val = g_data.hash_get(cmd[1]);
        if (val == std::nullopt) {
            res_status = RES_NX;
            append_nil(out);
        } else {
            append_str(out, std::string_view(val->get()));
        }
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        g_data.hash_set(cmd[1], cmd[2]);
        append_int(out, res_status);
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        append_int(out, g_data.hash_remove(cmd[1]) ? res_status : RES_NX);
    } else {
        res_status = RES_ERR;
        append_err(out, res_status, "unknown command");
    }
    patch_res_len(out, offset);
}
