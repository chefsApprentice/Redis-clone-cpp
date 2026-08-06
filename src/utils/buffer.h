// utils/buffer.h
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>

struct Buffer {
  uint8_t *buf_start = nullptr;
  uint8_t *buf_end = nullptr;
  uint8_t *data_start = nullptr;
  uint8_t *data_end = nullptr;

  ~Buffer() { free(buf_start); }
};

auto buf_capacity(Buffer *buf) -> size_t;

auto buf_size(Buffer *buf) -> size_t;

auto buf_realloc(Buffer *buf, size_t new_size) -> bool;

auto buf_grow(Buffer *buf, size_t min_capacity) -> bool;

void buf_consume(Buffer *buf, size_t len);

auto buf_append(Buffer &buf, const uint8_t *data, size_t len) -> bool;

void buf_free(Buffer *buf);
