<h1 align="center">
  Redis clone in C++
  <br>
</h1>

<h4 align="center">A Redis-like in-memory key-value server built from scratch in modern C++ (C++17), following <a href="https://build-your-own.org/redis/">Build Your Own Redis</a>.</h4>

<p align="center">
  <a href="#key-features">Key Features</a> •
  <a href="#how-to-use">How To Use</a> •
  <a href="#project-structure">Project Structure</a> •
  <a href="#protocol">Protocol</a> •
  <a href="#credits">Credits</a> •
  <a href="#license">License</a>
</p>

## Key Features

* In-memory key-value store
  - String and sorted-set values, addressed by string keys in a single hash table.
* String commands
  - `SET`, `GET`, and `DEL`, with strict arity checking and typed error replies.
* Sorted set commands
  - `ZADD`, `ZREM`, `ZSCORE`, and `ZQUERY` — sorted sets are indexed by both name (hash table) and (score, name) (AVL tree).
* Custom binary serialisation protocol
  - Tagged wire format (nil, error, string, int, double, array) with a 4-byte length-prefixed framing for requests and replies.
* Non-blocking single-threaded server
  - A `poll()` event loop with per-connection state machines, buffered reads and writes, and incremental one-bucket-at-a-time hash-table rehashing.
* Connection lifecycle management
  - Idle connections are reaped after a timeout via a doubly-linked idle list and monotonic clock timers.
* Memory-safe by construction
  - Intrusive data structures (`HashNode`, `AvlNode`, `DList`) with typed `container_of()`; ownership documented per node; no leaks or double-frees (see tests).
* Pipelines
  - The client sends all requests in one go and reads all responses back — no round-trip latency between commands.
* Tested and linted
  - GTest unit tests for every data structure and the serialiser; ASan/UBSan enabled by default; clang-tidy and cppcheck wired into the build.

## How To Use

To clone and run this application, you'll need [Git](https://git-scm.com), a C++17 compiler (GCC/Clang), and [CMake](https://cmake.org) 3.15+ (Ninja recommended) installed on your computer. From your command line:

```bash
# Clone this repository
$ git clone https://github.com/chefsApprentice/Redis-clone-cpp

# Go into the repository
$ cd Redis-clone-cpp

# Configure and build (Debug + sanitizers + static analysis)
$ make build

# Run the server
$ ./build/tcp_server

# In a second terminal, run the client
$ ./build/tcp_client
```

> **Note**
> The server binds to `0.0.0.0` on port `1234` by default. The bundled client connects to `127.0.0.1:1234` and exercises every command in a scripted pipeline — strings (`SET`/`GET`/`DEL`) and sorted sets (`ZADD`/`ZSCORE`/`ZQUERY`/`ZREM`) — printing each typed reply.

### Example session

```text
$ ./build/tcp_server &
$ ./build/tcp_client
(int) 0
(str) value0
(str) value1
...
(dbl) 2.5
(arr) len=4
(str) member1
(dbl) 1
(str) member2
(dbl) 2.5
(arr) end
(int) 0
(nil)
```

Replies are serialised values printed in tagged form: `(int) 0` is the OK acknowledgement returned by `SET`, `DEL` and `ZREM`, `(nil)` is the `ZADD` acknowledgement (or a missing key), `(dbl)` and `(arr)` carry scores and query results.

Alternatively, drive the build directly with CMake:

```bash
cmake -S . -B build -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Project Structure

```text
src/
  tcp_server.cpp            poll()-based event loop, connection state machines
  tcp_client.cpp            scripted pipeline client with response printing
  utils/
    req_res.{h,cpp}         command dispatch and the key-value store
    serialisation.{h,cpp}   tagged wire format (encode + human-readable decode)
    hashtable.{h,cpp}       incrementally rehashing hash table
    avl.{h,cpp}             AVL tree with O(log N) offset navigation
    zset.{h,cpp}            sorted set: AVL index + hash index over one node
    buffer.{h,cpp}          growable byte buffer
    dlist.h                 intrusive doubly-linked list (idle connections)
    timer.h                 monotonic clock helpers
    container_of.h          typed container_of for intrusive nodes
tests/
  *.cpp                     GTest suites for the utilities
cmake/                      sanitizer, warning, and static-analysis setups
Makefile                    configure/build/test/check/clean targets
```

## Protocol

Requests are arrays of strings, length-prefixed: a 4-byte (little-endian) argument count followed by 4-byte length-prefixed strings. Replies are one serialised value under the same framing, tagged by type:

| Tag | Encoding |
| --- | --- |
| `nil`   | empty body |
| `err`   | `int32` code + `u32` len + message |
| `str`   | `u32` len + payload |
| `int`   | `int64` value |
| `dbl`   | `double` value |
| `arr`   | `u32` count + that many serialised values |

## Credits

This software is built while following:

- [Build Your Own Redis](https://build-your-own.org/redis/) — the protocol and data-structure walkthrough this codebase is based on
- [GoogleTest](https://github.com/google/googletest) — unit testing
- [Ninja](https://ninja-build.org) and [CMake](https://cmake.org) — the build system

## Related

- [Redis](https://redis.io) — the real thing this project models

## License
MIT
