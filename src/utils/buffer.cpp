#include "buffer.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t buf_capacity(Buffer *buf) {
  if (!buf->buf_start)
    return 0;
  return (size_t)(buf->buf_end - buf->buf_start);
}

size_t buf_size(Buffer *buf) {
  if (!buf->data_start)
    return 0;
  return (size_t)(buf->data_end - buf->data_start);
}

bool buf_realloc(Buffer *buf, size_t new_size) {
  size_t size = buf_size(buf);
  if (new_size < size) {
    return false;
  }

  uint8_t *new_buf = (uint8_t *)malloc(new_size);
  if (!new_buf) {
    return false;
  }

  if (buf->buf_start && size > 0) {
    memcpy(new_buf, buf->data_start, size);
  }
  free(buf->buf_start);

  buf->buf_start = new_buf;
  buf->buf_end = new_buf + new_size;
  buf->data_start = new_buf;
  buf->data_end = new_buf + size;

  return true;
}

bool buf_grow(Buffer *buf, size_t min_capacity) {
  size_t capacity = buf_capacity(buf);

  if (capacity >= min_capacity)
    return true;

  size_t new_cap = capacity ? capacity : 16;
  while (new_cap < min_capacity) {
    new_cap *= 2;
  }

  return buf_realloc(buf, new_cap);
}

auto buf_append(Buffer &buf, const uint8_t *data, size_t len) -> bool {
  size_t size = buf_size(&buf);
  size_t cap = buf_capacity(&buf);

  if (size + len > cap) {
    if (!buf_grow(&buf, size + len))
      return false;
  }

  memcpy(buf.data_end, data, len);
  buf.data_end += len;
  return true;
}

void buf_consume(Buffer *buf, size_t len) {
  size_t size = buf_size(buf);
  if (len > size) {
    abort();
  }

  buf->data_start += len;

  // optional: reset pointers when empty
  if (buf->data_start == buf->data_end) {
    buf->data_start = buf->data_end = buf->buf_start;
  }
}

void buf_free(Buffer *buf) {
  std::free(buf->buf_start);
  *buf = {};
}
