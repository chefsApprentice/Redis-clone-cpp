// File: hashtable.cpp
// Author: Robin J
// Description: Key value store for req and res. Uses hash table implementation with chaining and
// linked list and intrusive data struct. Using FNV hash. Stores a list of pointers with calloc that
// can grow for reszing, pointing to memory allocated intrusive ndoes using RAII.

#include "./hashtable.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <list>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <sys/types.h>
#include <utility>
using std::string;

// struct KeyValueConfig {
//     const string &key;
//     const string &value;
// };

auto HashTable::allocate_buckets(size_t count) -> HashNode** {
    auto** result = static_cast<HashNode**>(calloc(count, sizeof(HashNode*)));

    if (result == nullptr) {
        throw std::bad_alloc{};
    }

    return result;
}

auto HashTable::migrate_one_bucket() -> void {
    if (hashset_old == nullptr) {
        return;
    }

    if (old_update_index >= old_size) {
        free((void*)hashset_old);
        hashset_old = nullptr;
        old_size = 0;
        old_update_index = 0;
    }

    HashNode* cur = hashset_old[old_update_index];
    while (cur != nullptr) {
        HashNode* next = cur->next;
        hashset_single_set(hashset_new, new_size, cur->key, cur->value);
        cur = next;
    }
    old_update_index++;
}

auto HashTable::hashset_grow() -> void {

    if (hashset_old != nullptr) {
        return;
    }

    hashset_old = hashset_new;
    old_size = new_size;

    u_long size = 2 * new_size;
    hashset_new = allocate_buckets(size);
    new_size = size;
    new_entries = 0;
    old_update_index = 0;
}

auto HashTable::hashset_single_set(HashNode** hashset, const size_t& bucket_count,
                                   const string& key, const string& value) -> void {
    if (hashset == nullptr) {
        return;
    }
    size_t bucket = hash(key) % bucket_count;
    HashNode* cur = hashset[bucket];
    bool key_exists = false;
    while (cur != nullptr) {
        if (cur->key == key) {
            cur->value = value;
            key_exists = true;
            break;
        }
        cur = cur->next;
    }
    if (!key_exists) {
        if (hashset == hashset_old) {
            return;
        }
        auto node = std::make_unique<HashNode>();
        node->key = key;
        node->value = value;
        HashNode* node_ptr = node.get();
        node_ptr->next = hashset_new[bucket];
        hashset_new[bucket] = node_ptr;

        nodes.push_back(std::move(node));
        auto itr = std::prev(nodes.end());
        itr->get()->owner = itr;
        ++new_entries;
    }
};

auto HashTable::hash(const string& key) -> uint64_t {
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

auto HashTable::hash_get(const string& key)
    -> std::optional<std::reference_wrapper<const std::string>> {
    size_t new_bucket = hash(key) % new_size;
    HashNode* cur = hashset_new[new_bucket];
    while (cur != nullptr) {
        if (cur->key == key) {
            return cur->value;
        }
        cur = cur->next;
    }

    if (hashset_old != nullptr) {
        size_t old_bucket = hash(key) % old_size;
        const HashNode* cur_old = hashset_old[old_bucket];
        while (cur_old != nullptr) {
            if (cur_old->key == key) {
                return cur_old->value;
            }
            cur_old = cur_old->next;
        }
    }

    return std::nullopt;
}

auto HashTable::hash_set(const string& key, const string& value) -> int {
    // Move old ones from current index
    migrate_one_bucket();
    // Update new one.
    hashset_single_set(hashset_new, new_size, key, value);
    // ensure old copies are updated
    hashset_single_set(hashset_old, old_size, key, value);
    if (static_cast<double>(new_entries) / static_cast<double>(new_size) > LOAD_THRESHOLD &&
        hashset_old != nullptr) {
        hashset_grow();
    }
    return 0;
};

auto HashTable::hash_remove(const string& key) -> bool {
    HashNode* target = nullptr;
    // Remove from old one, and move pointer along if collision.
    if (hashset_old != nullptr) {
        size_t bucket = hash(key) % old_size;
        HashNode* cur = hashset_old[bucket];
        HashNode* prev = nullptr;
        while (cur != nullptr) {
            if (cur->key == key) {
                target = cur;
                if (prev != nullptr) {
                    prev->next = cur->next;
                } else {
                    hashset_old[bucket] = cur->next;
                }
                break;
            }
            prev = cur;
            cur = cur->next;
        }
    }
    // Remove from new, and move pointer along if collision.
    size_t bucket = hash(key) % new_size;
    HashNode* cur = hashset_new[bucket];
    HashNode* prev = nullptr;
    while (cur != nullptr) {
        if (cur->key == key) {
            target = cur;
            if (prev != nullptr) {
                prev->next = cur->next;
            } else {
                hashset_new[bucket] = cur->next;
            }
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    // free from node list.
    if (target == nullptr) {
        return false;
    }

    nodes.erase(target->owner);
    return true;
};
