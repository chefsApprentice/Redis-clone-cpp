// File: req_res.cpp
// Author: Robin J
// Description: Key value store for req and res.

#include "utils/req_res.h"
#include "constants.h"
#include "utils/buffer.h"
#include <cstdint>
#include <cstring>
#include <map>
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

// placeholder; implemented later
static std::map<std::string, std::string> g_data;

void do_request(std::vector<std::string>& cmd, Buffer& out) {
    auto offset = buf_size(&out);
    const uint32_t def_len = 0;
    buf_append(out, (const uint8_t*)&def_len, 4);

    auto res_status = RES_OK;
    if (cmd.size() == 2 && cmd[0] == "get") {
        auto iterator = g_data.find(cmd[1]);
        if (iterator == g_data.end()) {
            res_status = RES_NX;
            buf_append(out, (const uint8_t*)&res_status, 4);
            patch_res_len(out, offset);
            return;
        }
        const std::string& val = iterator->second;
        buf_append(out, (const uint8_t*)&res_status, 4);
        buf_append(out, (const uint8_t*)val.data(), val.size());
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        g_data[cmd[1]].swap(cmd[2]);
        buf_append(out, (const uint8_t*)&res_status, 4);
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        g_data.erase(cmd[1]);
        buf_append(out, (const uint8_t*)&res_status, 4);
    } else {
        const auto res_status = RES_ERR;
        buf_append(out, (const uint8_t*)&res_status, 4);
    }

    patch_res_len(out, offset);
}

void make_response(const Response& res, Buffer& out) {
    uint32_t res_len = 4 + (uint32_t)res.data.size();
    buf_append(out, (uint8_t*)&res_len, 4);
    buf_append(out, (uint8_t*)&res.status, 4);
    buf_append(out, res.data.data(), res.data.size());
}
