// utils/hashtable.h
#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <sys/types.h>

using std::string;

class HashNode {
    friend class HashTable;
    string key;
    HashNode* next = nullptr;
    std::list<std::unique_ptr<HashNode>>::iterator owner;
};

class HashTable {
    constexpr static const double LOAD_THRESHOLD = 0.75;

  private:
    // hashset is non owning pointer to HashNode which is stored in our nodes. We use calloc to grow
    // our pointers but RAII for the nodes themselves.
    HashNode** hashset_old = nullptr;
    HashNode** hashset_new = nullptr;
    std::list<std::unique_ptr<HashNode>> nodes;

    size_t old_size = 0;
    size_t old_update_index = old_size - 1;

    // Number of buckets
    size_t new_size = 8;
    size_t new_entries = 0;

    auto hashset_grow() -> void;

    auto hashset_contains(const HashNode** hashset, const u_long& entries, const string& key)
        -> int;

    static auto allocate_buckets(size_t count) -> HashNode**;
    auto bucket_insert(HashNode** buckets, size_t bucket_count, HashNode* node) -> void;
    auto migrate_one_bucket() -> void;
    auto hashset_single_add(HashNode** hashset, const size_t& bucket_count, const string& key)
        -> HashNode*;

  public:
    HashTable() { hashset_new = allocate_buckets(new_size); }

    ~HashTable() {
        free((void*)hashset_old);
        free((void*)hashset_new);
    }

    auto static hash(const string& key) -> uint64_t;

    auto hash_get(const string& key) -> HashNode*;
    auto hash_add(const string& key) -> HashNode*;
    auto hash_remove(const string& key) -> bool;
};
