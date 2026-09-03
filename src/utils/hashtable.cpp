// utils/hashtable.cpp
// Author: Robin J
// Description: Non-owning chained hash table with incremental rehashing.
// Buckets store pointers to HashNode fields embedded in caller-owned objects
// (Entry, ZNode). The table never allocates or frees the nodes themselves;
// hash_remove() hands the unlinked node back so the caller can release the
// owning object.

#include "./hashtable.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <string>
using std::string;

auto HashTable::allocate_buckets(size_t count) -> HashNode** {
    auto** result = static_cast<HashNode**>(calloc(count, sizeof(HashNode*)));
    if (result == nullptr) {
        throw std::bad_alloc{};
    }
    return result;
}

HashTable::~HashTable() {
    // only the bucket arrays are ours; the nodes belong to the callers
    free((void*)hashset_old);
    free((void*)hashset_new);
}

HashTable::HashTable(HashTable&& other) noexcept
    : hashset_old(other.hashset_old),
      hashset_new(other.hashset_new),
      old_size(other.old_size),
      old_update_index(other.old_update_index),
      new_size(other.new_size),
      new_entries(other.new_entries) {
    other.hashset_old = nullptr;
    other.hashset_new = nullptr;
    other.old_size = 0;
    other.old_update_index = 0;
    other.new_size = 0;
    other.new_entries = 0;
}

auto HashTable::operator=(HashTable&& other) noexcept -> HashTable& {
    if (this != &other) {
        free((void*)hashset_old);
        free((void*)hashset_new);

        hashset_old = other.hashset_old;
        hashset_new = other.hashset_new;
        old_size = other.old_size;
        old_update_index = other.old_update_index;
        new_size = other.new_size;
        new_entries = other.new_entries;

        other.hashset_old = nullptr;
        other.hashset_new = nullptr;
        other.old_size = 0;
        other.old_update_index = 0;
        other.new_size = 0;
        other.new_entries = 0;
    }
    return *this;
}

auto HashTable::hash(const string& key) -> uint64_t {
    // Unsigned wrap around intended as part of algo.
    const u_int64_t FNV_offset_basis = 14695981039346656037ULL;
    const u_int64_t FNV_prime = 1099511628211ULL;
    auto hash = FNV_offset_basis;
    for (unsigned char chr : key) {
        hash = hash * FNV_prime;
        hash = hash ^ chr;
    }
    return hash;
}

// Promote the active table to "dying" and start rehashing into a fresh,
// doubled table. Only one resize may be in flight at a time.
auto HashTable::hashset_grow() -> void {
    if (hashset_old != nullptr) {
        return;
    }
    hashset_old = hashset_new;
    old_size = new_size;
    old_update_index = 0;

    new_size *= 2;
    hashset_new = allocate_buckets(new_size);
    new_entries = 0;
}

// Relink one old bucket's chain into the active table, then drop the old
// table once every bucket has been drained. Nodes move, never copy: after
// this, a node lives in exactly one of the two tables.
auto HashTable::migrate_one_bucket() -> void {
    if (hashset_old == nullptr) {
        return;
    }
    if (old_update_index < old_size) {
        HashNode** from = &hashset_old[old_update_index];
        while (*from != nullptr) {
            HashNode* node = *from;
            *from = node->next; // unlink from the dying table
            const size_t bucket = node->hcode % new_size;
            node->next = hashset_new[bucket];
            hashset_new[bucket] = node;
            ++new_entries;
        }
        ++old_update_index;
    }
    if (old_update_index >= old_size) {
        free((void*)hashset_old);
        hashset_old = nullptr;
        old_size = 0;
        old_update_index = 0;
    }
}

auto HashTable::hash_get(const string& key, KeyEq key_eq) -> HashNode* {
    const uint64_t hcode = hash(key);
    // nodes may still be waiting in the dying table while a resize is in
    // flight; probe both generations
    if (hashset_old != nullptr) {
        for (HashNode* cur = hashset_old[hcode % old_size]; cur != nullptr; cur = cur->next) {
            if (cur->hcode == hcode && key_eq(cur, key)) {
                return cur;
            }
        }
    }
    for (HashNode* cur = hashset_new[hcode % new_size]; cur != nullptr; cur = cur->next) {
        if (cur->hcode == hcode && key_eq(cur, key)) {
            return cur;
        }
    }
    return nullptr;
}

// Link a caller-owned node under `key`. The caller must have ensured the key
// is absent (e.g. by a prior hash_get); insertion is unconditional.
auto HashTable::hash_add(const string& key, HashNode* node) -> HashNode* {
    migrate_one_bucket(); // incremental resize step
    node->hcode = hash(key);
    const size_t bucket = node->hcode % new_size;
    node->next = hashset_new[bucket];
    hashset_new[bucket] = node;
    ++new_entries;

    if (static_cast<double>(new_entries) / static_cast<double>(new_size) > LOAD_THRESHOLD &&
        hashset_old == nullptr) {
        hashset_grow();
    }
    return node;
}

// Unlink the node for `key` (from whichever table currently holds it) and
// return it; the caller frees the owning object. Returns nullptr if absent.
auto HashTable::hash_remove(const string& key, KeyEq key_eq) -> HashNode* {
    const uint64_t hcode = hash(key);
    if (hashset_old != nullptr) {
        HashNode** ref = &hashset_old[hcode % old_size];
        while (*ref != nullptr) {
            HashNode* cur = *ref;
            if (cur->hcode == hcode && key_eq(cur, key)) {
                *ref = cur->next;
                return cur; // a node lives in exactly one table
            }
            ref = &cur->next;
        }
    }
    HashNode** ref = &hashset_new[hcode % new_size];
    while (*ref != nullptr) {
        HashNode* cur = *ref;
        if (cur->hcode == hcode && key_eq(cur, key)) {
            *ref = cur->next;
            return cur;
        }
        ref = &cur->next;
    }
    return nullptr;
}