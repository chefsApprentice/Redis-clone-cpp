// utils/hashtable.h
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

using std::string;

inline constexpr size_t k_initial_buckets = 8;

// Intrusive hash table node: embed one of these in the object you want to
// store (Entry, ZNode). The table stores only the node address; ownership of
// the enclosing object stays with the caller. `hcode` is the FNV-1a hash of
// the key, precomputed on insert so lookups never rebuild it.
class HashNode {
    friend class HashTable;
    uint64_t hcode = 0;
    HashNode* next = nullptr;
};

// Non-owning chained hash table with incremental (one-bucket-at-a-time)
// rehashing. Buckets are raw pointer arrays; the table never allocates or
// frees HashNode objects. hash_remove() returns the unlinked node so the
// caller can free the owning object.
//
// The key text lives in the owning object (Entry::key / ZNode::name), not in
// HashNode, so lookups take an equality callback that compares a node's
// payload against the caller's key string.
class HashTable {
    constexpr static const double LOAD_THRESHOLD = 0.75;

  private:
    // hashset_new is the active table; hashset_old exists only while an
    // incremental resize is in flight. A node lives in exactly one of them.
    HashNode** hashset_old = nullptr;
    HashNode** hashset_new = nullptr;

    size_t old_size = 0;         // bucket count of the dying table
    size_t old_update_index = 0; // next old bucket to migrate
    size_t new_size = k_initial_buckets; // bucket count of the active table
    size_t new_entries = 0;      // node count of the active table

    auto hashset_grow() -> void;
    auto migrate_one_bucket() -> void;
    static auto allocate_buckets(size_t count) -> HashNode**;

  public:
    // Compares a table node's payload against a lookup key.
    using KeyEq = bool (*)(const HashNode* node, const string& key);

    HashTable() { hashset_new = allocate_buckets(new_size); }
    ~HashTable();

    // The table holds raw pointers and owns the bucket arrays: copy would
    // double-free; move transfers the arrays. A HashTable lives inside
    // ZSet/Entry and inside the global store, so it must stay address-stable.
    HashTable(const HashTable&) = delete;
    auto operator=(const HashTable&) -> HashTable& = delete;
    HashTable(HashTable&& other) noexcept;
    auto operator=(HashTable&& other) noexcept -> HashTable&;

    static auto hash(const string& key) -> uint64_t;

    auto hash_get(const string& key, KeyEq key_eq) -> HashNode*;
    // Links a caller-owned node under `key`. Precondition: the key is absent
    // (callers check with hash_get first); insertion is unconditional.
    auto hash_add(const string& key, HashNode* node) -> HashNode*;
    // Unlinks the node for `key` and returns it (nullptr if absent); the
    // caller is responsible for releasing the owning object.
    auto hash_remove(const string& key, KeyEq key_eq) -> HashNode*;
};