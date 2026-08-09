// File: hashtable.cpp
// Author: Robin J
// Description: Key value store for req and res. Uses hash table implementation with chaining and
// linked list and intrusive data struct. Using FNV hash.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <sys/types.h>
#include <vector>
using std::string;

// struct KeyValueConfig {
//     const string &key;
//     const string &value;
// };

class HashNode {
    friend class HashTable;

    string key;
    string value;
    HashNode* next = nullptr;
};

class HashTable {
    constexpr static const double LOAD_THRESHOLD = 0.75;

  private:
    HashNode** hashset_old;
    HashNode** hashset_new;

    u_long old_entries = 0;
    u_long old_size = 8;
    u_long new_entries = 0;
    // size is max number of buckets / entries.
    u_long new_size = 8;

    auto hashset_grow() -> void {
        free(hashset_old);
        hashset_old = hashset_new;
        old_size = new_size;
        old_entries = new_entries;

        u_long size = 2 * new_size * sizeof(HashNode);
        hashset_new = static_cast<HashNode**>(calloc(size, sizeof(HashNode)));
        new_size = size;
        new_entries = 0;
    }

    static auto hashset_contains(const HashNode** hashset, const u_long& entries, const string& key)
        -> int {
        size_t bucket = hash(key) % (entries * sizeof(HashNode));

        const HashNode cur = *hashset[bucket];
        while (cur.next != nullptr) {
            if (cur.key == key) {
                return 0;
            }
        }

        return -1;
    }

  public:
    static auto hash(const string& key) -> uint64_t {
        // Unsigned wrap around intended as part of algo.
        const u_int64_t FNV_offset_basis = 14695981039346656037;
        const u_int64_t FNV_prime = 1099511628211;
        auto hash = FNV_offset_basis;
        for (unsigned char chr : key) {
            hash = hash * FNV_prime;
            hash = hash ^ chr;
        }
        return hash;
    }

    auto hash_get(const string& key) -> const string& {
        size_t new_bucket = hash(key) % (new_entries * sizeof(HashNode));
        const HashNode cur = *hashset_new[new_bucket];
        while (cur.next != nullptr) {
            if (cur.key == key) {
                return cur.value;
            }
        }

        size_t old_bucket = hash(key) % (old_entries * sizeof(HashNode));
        const HashNode cur_old = *hashset_old[old_bucket];
        while (cur_old.next != nullptr) {
            if (cur_old.key == key) {
                return cur_old.value;
            }
        }

        return "";
    }

    auto hash_set(const string& key, const string& value) -> int {
        size_t bucket = hash(key) % new_size;
        HashNode& cur = *hashset_new[bucket];
        while (cur.next != nullptr) {
            if (cur.key == key) {
                cur.value = value;
            }
        }
        cur.key = key;
        cur.value = value;

        // Need to move old entries

        new_entries++;
        if (new_entries / new_size > LOAD_THRESHOLD) {
            hashset_grow();
        }
        return 0;
    };
};
// static std::map<std::string, std::string> g_data;
