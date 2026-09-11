#pragma once

#include <cstddef>
#include <cstdint>
inline constexpr size_t k_max_msg = 32 << 20; // likely larger than the kernel buffer
inline constexpr size_t read_size = 4;
inline constexpr uint64_t k_idle_timeout_ms = 1000;
enum {
    RES_OK = 0,
    RES_ERR = 1,
    RES_NX = 2, // NX stands for "Not eXist"
    RES_UKNOWN = 3,
    RES_BAD_ARG = 4,
};
