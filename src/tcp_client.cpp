// File: tcp_client.cpp
// Author: Robin J
// Description: Generic TCP client with sockets.

#include "./utils/buffer.h"
#include "./utils/constants.h"
#include <arpa/inet.h>
#include <assert.h>
#include <cstdint>
#include <errno.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

static void msg(const char* msg) { fprintf(stderr, "%s\n", msg); }

static void die(const char* msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static auto read_full(int fd, Buffer* buf, size_t n) -> int32_t {
    while (n > 0) {
        ssize_t rv = read(fd, buf->data_end, n);
        if (rv <= 0) {
            if (rv == 0) {
                fprintf(stderr, "read_full: got EOF\n");
            } else {
                fprintf(stderr, "read_full: read error: %s\n", strerror(errno));
            }
            return -1;
        }
        if ((size_t)rv > n) {
            fprintf(stderr, "read_full: rv > n, clamping\n");
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf->data_end += (size_t)rv;
    }
    return 0;
}

static auto write_all(int fd, Buffer* buf, size_t n) -> int32_t {
    while (n > 0) {
        ssize_t rv = write(fd, buf->data_start, n);
        if (rv <= 0) {
            return -1; // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf_consume(buf, (size_t)rv);
    }
    return 0;
}

// the `query` function was simply splited into `send_req` and `read_res`.
static auto send_req(int fd, const std::vector<std::string>& cmd) -> int32_t {
    uint32_t len = 4;
    for (const std::string& str : cmd) {
        len += 4 + str.size();
    }

    if (len > k_max_msg) {
        fprintf(stderr, "send_req: len too big %zu\n", len);
        return -1;
    }

    Buffer wbuf;
    buf_append(wbuf, (const uint8_t*)&len, 4);
    uint32_t cmd_size = cmd.size();
    buf_append(wbuf, (const uint8_t*)&cmd_size, 4);
    for (const std::string& s : cmd) {
        uint32_t s_size = s.size();
        buf_append(wbuf, (const uint8_t*)&s_size, 4);
        buf_append(wbuf, (const uint8_t*)s.data(), s.size());
    }

    int32_t ret = write_all(fd, &wbuf, buf_size(&wbuf));
    fprintf(stderr, "send_req: write_all returned %d\n", ret);
    return ret;
}

static auto read_res(int fd) -> int32_t {
    // 4 bytes header
    Buffer rbuf;
    buf_grow(&rbuf, 4);
    fprintf(stderr, "read_res: about to read 4-byte header\n");
    errno = 0;
    int32_t err = read_full(fd, &rbuf, 4);
    if (err != 0) {
        if (errno == 0) {
            fprintf(stderr, "read_res: got EOF\n");
        } else {
            fprintf(stderr, "read_res: read() error: %s\n", strerror(errno));
        }
        return err;
    }
    fprintf(stderr, "read_res: read 4-byte header\n");

    uint32_t len = 0;
    memcpy(&len, rbuf.data_start, 4); // assume little endian
    fprintf(stderr, "read_res: length = %u\n", len);
    if (len > k_max_msg) {
        fprintf(stderr, "read_res: length too big %u\n", len);
        return -1;
    }

    // reply body
    buf_grow(&rbuf, len);
    buf_consume(&rbuf, 4);
    fprintf(stderr, "read_res: about to read %u bytes body\n", len);
    err = read_full(fd, &rbuf, len);
    if (err) {
        if (errno == 0) {
            fprintf(stderr, "read_res: got EOF while reading body\n");
        } else {
            fprintf(stderr, "read_res: read() error while reading body: %s\n", strerror(errno));
        }
        return err;
    }
    fprintf(stderr, "read_res: read body\n");

    // do something
    uint32_t status = 0;
    memcpy(&status, rbuf.data_start, sizeof(status));
    fprintf(stderr, "status = %u\n", status);

    size_t data_len = len - sizeof(status);
    fprintf(stderr, "data = %.*s\n", (int)data_len,
            reinterpret_cast<char*>(rbuf.data_start + sizeof(status)));

    buf_consume(&rbuf, len);
    return 0;
}

auto main() -> int {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK); // 127.0.0.1
    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (rv != 0) {
        die("connect");
    }

    // std::vector<std::string> cmd1 = {"set", "name", "Alice"};
    // std::vector<std::string> cmd2 = {
    //     "get",
    //     "name",
    // };
    //
    // // multiple pipelined requests
    // auto query_list = {cmd1, cmd2};
    //
    // for (const std::vector<std::string>& cmd : query_list) {
    //     int32_t err = send_req(fd, cmd);
    //     if (err != 0) {
    //         goto L_DONE;
    //     }
    // }
    //
    // for (size_t i = 0; i < query_list.size(); ++i) {
    //     int32_t err = read_res(fd);
    //     if (err != 0) {
    //         goto L_DONE;
    //     }
    // }

    std::vector<std::vector<std::string>> query_list;

    constexpr size_t count = 9;

    // SET key0 value0 ... key8 value8
    for (size_t i = 0; i < count; ++i) {
        query_list.push_back({"set", "key" + std::to_string(i), "value" + std::to_string(i)});
    }

    // GET key0 ... key8
    for (size_t i = 0; i < count; ++i) {
        query_list.push_back({"get", "key" + std::to_string(i)});
    }

    // Send all requests (pipelined).
    for (const auto& cmd : query_list) {
        int32_t err = send_req(fd, cmd);
        if (err != 0) {
            goto L_DONE;
        }
    }

    // Read all responses.
    for (size_t i = 0; i < query_list.size(); ++i) {
        int32_t err = read_res(fd);
        if (err != 0) {
            goto L_DONE;
        }
    }

L_DONE:
    close(fd);
    return 0;
}
