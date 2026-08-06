// utils/req_res.h
#pragma once

#include "utils/buffer.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

struct Response {
    uint32_t status = 0;
    std::vector<uint8_t> data;
};

void do_request(std::vector<std::string>& cmd, Response& out);
void make_response(const Response &res, Buffer &out);
auto read_u32(const uint8_t*& cur, const uint8_t* end, uint32_t& out) -> bool;
auto read_str(const uint8_t*& cur, const uint8_t* end, size_t n, std::string& out) -> bool;
auto parse_req(const uint8_t* data, size_t size, std::vector<std::string>& out) -> int32_t;
