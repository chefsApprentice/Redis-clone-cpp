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
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <variant>
#include <vector>

using std::string;

auto read_u32(const uint8_t*& cur, const uint8_t* end, uint32_t& out) -> bool {
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

namespace {

// the whole key-value store: a hash table of Entry, one per key
HashTable g_data;

// identifies an Entry by its key; used by the hash table's lookups
auto entry_eq(const HashNode* node, const std::string& key) -> bool {
    return container_of(node, &Entry::node)->key == key;
}

auto str2dbl(const std::string& s, double& out) -> bool {
    char* endp = nullptr;
    out = strtod(s.c_str(), &endp);
    return endp == s.c_str() + s.size() && !std::isnan(out);
}

auto str2int(const std::string& s, int64_t& out) -> bool {
    char* endp = nullptr;
    out = strtoll(s.c_str(), &endp, 10);
    return endp == s.c_str() + s.size();
}

auto response_begin(Buffer& out) -> uint32_t {
    const auto offset = static_cast<uint32_t>(buf_size(&out));
    const uint32_t def_len = 0;
    buf_append(out, (const uint8_t*)&def_len, 4); // length placeholder, patched below
    return offset;
}

void patch_res_len(Buffer& out, uint32_t offset) {
    // Because after finding offset aka our start point we allocate length.
    uint32_t res_len = static_cast<uint32_t>(buf_size(&out) - offset) - 4;
    memcpy(out.data_start + offset, &res_len, 4);
}

void arr_patch(Buffer& out, size_t ctx, uint32_t val) { memcpy(out.data_start + ctx, &val, 4); }
//  Do functions
//  /////////////////////////////////////////////////////////////////////////////////////////////////

auto do_set(const std::vector<std::string>& cmd, Buffer& out) -> void {
    HashNode* found = g_data.hash_get(cmd[1], entry_eq);
    if (found != nullptr) {
        Entry* ent = container_of(found, &Entry::node);
        if (ent->is_zset()) {
            append_err(out, RES_BAD_ARG, "Trying to set string on non-string field.");
            return;
            // dispose the live ZSet first, else the variant destructor leaks every ZNode
            // ZSet::dispose_zset(std::get<ZSet>(ent->value));
        }
        ent->value = cmd[2];
    } else {
        // the Entry owns its embedded HashNode: hand its address to the table.
        // RAII guard: hash_add may throw bad_alloc; release only once inserted.
        auto entry = std::make_unique<Entry>(cmd[1], cmd[2]);
        g_data.hash_add(cmd[1], &entry->node);
        entry.release(); // ownership transferred to g_data
    }
    append_int(out, RES_OK);
}

auto do_get(const std::vector<std::string>& cmd, Buffer& out) -> void {
    HashNode* found = g_data.hash_get(cmd[1], entry_eq);
    if (found == nullptr) {
        append_nil(out);
        return;
    }
    Entry* ent = container_of(found, &Entry::node);
    if (!ent->is_str()) {
        append_err(out, RES_BAD_ARG, "not a string value");
        return;
    }
    const std::string& str = std::get<std::string>(ent->value);
    append_str(out, str);
}

auto do_del(const std::vector<std::string>& cmd, Buffer& out) -> void {
    // unlink before freeing: the table holds the address of the node embedded
    // in the Entry, so it must leave the table first
    HashNode* found = g_data.hash_remove(cmd[1], entry_eq);
    if (found == nullptr) {
        append_int(out, RES_NX);
        return;
    }
    Entry* ent = container_of(found, &Entry::node);
    if (ent->is_zset()) {
        ZSet::dispose_zset(std::get<ZSet>(ent->value));
    }
    std::unique_ptr<Entry> owner(ent); // RAII free, destructor calls on leaving function.
    append_int(out, RES_OK);
}

auto do_zadd(const std::vector<std::string>& cmd, Buffer& out) -> void {
    double score = 0;
    if (!str2dbl(cmd[2], score)) {
        append_err(out, RES_BAD_ARG, "expect float");
        return;
    }
    HashNode* node = g_data.hash_get(cmd[1], entry_eq);
    Entry* ent = nullptr;
    if (node != nullptr) {
        ent = container_of(node, &Entry::node);
        if (!ent->is_zset()) {
            append_err(out, RES_BAD_ARG, "Key does not point to a zset.");
            return;
        }
    } else {
        auto entry = std::make_unique<Entry>(cmd[1], ZSet{});
        g_data.hash_add(cmd[1], &entry->node);
        ent = entry.release();
    }
    const std::string& name = cmd[3];
    auto& zset = std::get<ZSet>(ent->value);
    ZSet::zset_insert(&zset, name.data(), name.size(), score);
    append_nil(out);
}

auto do_zrem(const std::vector<std::string>& cmd, Buffer& out) -> void {
    HashNode* node = g_data.hash_get(cmd[1], entry_eq);
    if (node == nullptr) {
        append_err(out, RES_NX, "Target not found");
        return;
    }
    Entry* ent = container_of(node, &Entry::node);
    if (!ent->is_zset()) {
        append_err(out, RES_BAD_ARG, "Wrong type, expected zset.");
        return;
    }
    auto& zset = std::get<ZSet>(ent->value);
    ZSet::zset_delete(&zset, &cmd[2]);
    append_int(out, RES_OK);
}

auto do_zscore(const std::vector<std::string>& cmd, Buffer& out) -> void {
    HashNode* node = g_data.hash_get(cmd[1], entry_eq);
    if (node == nullptr) {
        append_err(out, RES_NX, "Target not found");
        return;
    }
    Entry* ent = container_of(node, &Entry::node);
    if (!ent->is_zset()) {
        append_err(out, RES_BAD_ARG, "Wrong type, expected zset.");
        return;
    }
    auto& zset = std::get<ZSet>(ent->value);
    ZNode* znode = ZSet::zset_lookup(&zset, &cmd[2]);
    if (znode == nullptr) {
        append_err(out, RES_NX, "Target znode not found");
        return;
    }
    append_dbl(out, znode->score);
}

static void do_zquery(std::vector<std::string>& cmd, Buffer& out) {
    // parse the arguments and lookup the KV pair
    double score = 0;
    if (!str2dbl(cmd[2], score)) {
        append_err(out, RES_ERR, "expect fp number");
        return;
    }
    const std::string& name = cmd[3];
    int64_t znode_off = 0;
    int64_t limit = 0;
    if (!str2int(cmd[4], znode_off) || !str2int(cmd[5], limit)) {
        append_err(out, RES_BAD_ARG, "expect int");
        return;
    }

    HashNode* node = g_data.hash_get(cmd[1], entry_eq);
    if (node == nullptr) {
        append_err(out, RES_NX, "Target couldn't be found.");
        return;
    }
    Entry* ent = container_of(node, &Entry::node);
    if (!ent->is_zset()) {
        append_err(out, RES_BAD_ARG, "Not a zset.");
        return;
    }
    auto& zset = std::get<ZSet>(ent->value);
    // 1. seek to the key
    ZNode* znode = ZSet::zset_seekge(&zset, score, name.data(), name.size());
    // 2. offset
    znode = ZSet::znode_offset(znode, znode_off);
    // 3. iterate, appending (name, score) pairs as a tagged array
    append_arr_header(out, 0);             // TAG_ARR + count placeholder
    const size_t ctx = buf_size(&out) - 4; // count field sits right after the tag byte
    uint32_t n = 0;
    while ((znode != nullptr) && n < limit) {
        append_str(out, std::string(znode->name, znode->len));
        append_dbl(out, znode->score);
        znode = ZSet::znode_offset(znode, +1);
        n += 2;
    }
    arr_patch(out, ctx, n);
}
} // namespace
// verifying if message is parseable
auto parse_req(const uint8_t* data, size_t size, std::vector<std::string>& out) -> int32_t {
    const uint8_t* end = data + size;
    uint32_t nstr = 0;
    if (!read_u32(data, end, nstr)) {
        return -1;
    }
    if (nstr > k_max_msg) {
        return -1; // safety limit
    }
    out.clear();
    out.reserve(nstr);
    while (out.size() < nstr) {
        uint32_t len = 0;
        if (!read_u32(data, end, len)) {
            return -1;
        }
        out.emplace_back();
        if (!read_str(data, end, len, out.back())) {
            return -1;
        }
    }
    return (data == end) ? 0 : -1;
}

void do_request(std::vector<std::string>& cmd, Buffer& out) {
    const uint32_t offset = response_begin(out);

    if (cmd.size() == 2 && cmd[0] == "get") {
        do_get(cmd, out);
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        do_set(cmd, out);
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        do_del(cmd, out);
    } else if (cmd.size() == 4 && cmd[0] == "zadd") {
        do_zadd(cmd, out);
    } else if (cmd.size() == 3 && cmd[0] == "zrem") {
        do_zrem(cmd, out);
    } else if (cmd.size() == 3 && cmd[0] == "zscore") {
        do_zscore(cmd, out);
    } else if (cmd.size() == 6 && cmd[0] == "zquery") {
        do_zquery(cmd, out);
    } else {
        append_err(out, RES_ERR, "unknown command");
    }

    patch_res_len(out, offset);
}
