// File: req_res.cpp
// Author: Robin J
// Description: Key value store for req and res.

#include "utils/req_res.h"
#include "constants.h"
#include "utils/buffer.h"
#include "utils/container_of.h"
#include "utils/hashtable.h"
#include "utils/serialisation.h"
#include "utils/zset.h"
#include <cstdint>
#include <cstring>
#include <map>
#include <math.h>
#include <optional>
#include <string>
#include <vector>

using std::string;

static ZSet g_data;

// error code for TAG_ERR
enum {
    ERR_UNKNOWN = 1, // unknown command
    ERR_TOO_BIG = 2, // response too big
    ERR_BAD_TYP = 3, // unexpected value type
    ERR_BAD_ARG = 4, // bad arguments
};

// data types of serialized data
enum {
    TAG_NIL = 0, // nil
    TAG_ERR = 1, // error code + msg
    TAG_STR = 2, // string
    TAG_INT = 3, // int64
    TAG_DBL = 4, // double
    TAG_ARR = 5, // array
};

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

static auto str2dbl(const std::string& s, double& out) -> bool {
    char* endp = NULL;
    out = strtod(s.c_str(), &endp);
    return endp == s.c_str() + s.size() && !isnan(out);
}

static auto str2int(const std::string& s, int64_t& out) -> bool {
    char* endp = NULL;
    out = strtoll(s.c_str(), &endp, 10);
    return endp == s.c_str() + s.size();
}

static void out_err(Buffer& out, uint32_t code, const std::string& msg) {
    auto res_status = RES_ERR;
    buf_append(out, (const uint8_t*)res_status, 1);
    buf_append(out, (const uint8_t*)&code, 4);
    buf_append(out, (const uint8_t*)msg.size(), 4);
    buf_append(out, (const uint8_t*)msg.data(), msg.size());
}

static void out_nil(Buffer& out) { buf_append(out, (const uint8_t*)TAG_NIL, 1); }

static void out_string(Buffer& out, string str) {
    buf_append(out, (const uint8_t*)TAG_STR, 1);
    buf_append(out, (const uint8_t*)str.length(), 4);
    buf_append(out, (const uint8_t*)str.data(), str.size());
}

//
// // verifying if message is parseabe
// auto parse_req(const uint8_t* data, size_t size, std::vector<std::string>& out) -> int32_t {
//     const uint8_t* end = data + size;
//     uint32_t nstr = 0;
//     if (!read_u32(data, end, nstr)) {
//         return -1;
//     }
//
//     if (nstr > k_max_msg) {
//         return -1; // safety limit
//     }
//
//     while (out.size() < nstr) {
//         uint32_t len = 0;
//         if (!read_u32(data, end, len)) {
//             return -1;
//         }
//         out.push_back(std::string());
//         if (!read_str(data, end, len, out.back())) {
//             return -1;
//         }
//     }
//     if (data != end) {
//         return -1;
//     }
//     return 0;
// }
//

static auto res_add_tag(Buffer& out) -> size_t {
    auto res_status = RES_OK;
    buf_append(out, (const uint8_t*)res_status, 1);
    return buf_size(&out) - 4;
};

static auto response_begin(Buffer& out, uint32_t offset) -> uint32_t {
    offset = buf_size(&out);
    const uint32_t def_len = 0;
    buf_append(out, (const uint8_t*)&def_len, 4); // length placeholder, patched below
    return offset;
}

static void patch_res_len(Buffer& out, uint32_t offset) {
    // Because after finding offset aka our start point we allocate length.
    uint32_t res_len = (buf_size(&out) - offset) - 4;
    memcpy(out.data_start + offset, &res_len, 4);
}

//  Do functions
//  /////////////////////////////////////////////////////////////////////////////////////////////////

static auto do_set(std::vector<std::string>& cmd, Buffer& out) -> void {
    HashNode* found = g_data.hmap.hash_get(cmd[1]);
    if (found != nullptr) {
        Entry* ent = container_of(found, &Entry::node);
        if (!ent->is_str()) {
            out_err(out, ERR_BAD_TYP, "a non-string value exists");
            return;
        }
        ent->value = cmd[2];
    } else {
        auto* entry = new Entry(cmd[1], cmd[2]);
        entry->node = *g_data.hmap.hash_add(cmd[1]);
    }
    out_nil(out);
}

static auto do_get(std::vector<std::string>& cmd, Buffer& out) -> void {
    HashNode* found = g_data.hmap.hash_get(cmd[1]);
    if (found == nullptr) {
        out_nil(out);
        return;
    }
    Entry* ent = container_of(found, &Entry::node);
    if (!ent->is_str()) {
        out_err(out, ERR_BAD_TYP, "not a string value");
        return;
    }
    auto& str = std::get<std::string>(ent->value);
    out_string(out, str);
}

static auto do_del(std::vector<std::string>& cmd, Buffer& out) -> void {
    HashNode* found = g_data.hmap.hash_get(cmd[1]);
    if (found == nullptr) {
        out_err(out, ERR_BAD_TYP, "a non-string value exists");
        return;
    }
    Entry* ent = container_of(found, &Entry::node);
    delete ent;
    out_nil(out);
}

// static void do_zquery(std::vector<std::string>& cmd, Buffer& out) {
//     // parse the arguments and lookup the KV pair
//     double score = 0;
//     if (!str2dbl(cmd[2], score)) {
//         out_err(out, RES_ERR, "expect fp number");
//         return;
//     }
//     const std::string& name = cmd[3];
//     int64_t znode_off = 0;
//     int64_t limit = 0;
//     if (!str2int(cmd[4], znode_off) || !str2int(cmd[5], limit)) {
//         out_err(out, RES_BAD_ARG, "expect int");
//         return;
//     }
//
//
//     // if (cmd.size() == 2 && cmd[0] == "get") {
//     // 1. seek to the key
//     ZNode* znode = g_data.zset_seekge(&g_data, score, name.data(), name.size());
//     // 2. offset
//     znode = ZSet::znode_offset(znode, znode_off);
//     // 3. iterate and output
//     size_t ctx = out_begin_arr(out);
//     uint32_t n = 0;
//     while (znode && n < limit) {
//         out_str(out, znode->name, znode->len);
//         out_dbl(out, znode->score);
//         znode = ZSet::znode_offset(znode, +1);
//     }
// }

void do_request(std::vector<std::string>& cmd, Buffer& out) {
    uint32_t offset = 0;
    response_begin(out, offset);

    if (cmd.size() == 2 && cmd[0] == "get") {
        do_get(cmd, out);
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        do_set(cmd, out);
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        do_del(cmd, out);
    } else if (cmd.size() == 1 && cmd[0] == "keys") {
        do_keys(cmd, out);
    } else if (cmd.size() == 4 && cmd[0] == "zadd") {
        do_zadd(cmd, out);
    } else if (cmd.size() == 3 && cmd[0] == "zrem") {
        do_zrem(cmd, out);
    } else if (cmd.size() == 3 && cmd[0] == "zscore") {
        do_zscore(cmd, out);
    } else if (cmd.size() == 6 && cmd[0] == "zquery") {
        do_zquery(cmd, out);
    } else {
        out_err(out, RES_UKNOWN, "unknown command.");
    }

    patch_res_len(out, offset);
}
