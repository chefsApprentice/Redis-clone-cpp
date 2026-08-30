#include "utils/avl.h"
#include "utils/hashtable.h"
#include <cstring>
#include <string>
#include <utility>
#include <variant>

struct ZNode {
    // data structure nodes
    AvlNode tree;
    HashNode hnode;
    // data
    double score = 0;
    size_t len = 0;
    char name[0]; // flexible array

    static auto znode_new(const char* name, size_t len, double score, HashTable* hmap) -> ZNode* {
        auto* node = (ZNode*)malloc(sizeof(ZNode) + len); // struct + array
        avl_init(&node->tree);
        node->hnode = *hmap->hash_add(name);
        // node->hmap.next = nullptr;
        // node->hmap.hcode = str_hash((uint8_t*)name, len);
        node->score = score;
        node->len = len;
        memcpy(&node->name[0], name, len);
        return node;
    }

    static void znode_del(ZNode* node) { free(node); }
};

struct ZSet {
    AvlNode* root = nullptr; // index by (score, name)
    HashTable hmap;          // index by name

    static auto zset_insert(ZSet* zset, const char* name, size_t len, double score) -> bool;
    static auto zset_lookup(ZSet* zset, const char* name, size_t len) -> ZNode*;
    static auto zset_lookup(ZSet* zset, const string *key) -> ZNode*;
    static void zset_delete(ZSet* zset, ZNode* node);
    static void zset_clear(ZSet* zset, ZNode* node);
    static auto znode_offset(ZNode* node, int64_t offset) -> ZNode*;
    static auto avl_offset(AvlNode* node, int64_t offset) -> AvlNode*;
    static auto zset_seekge(ZSet* zset, double score, const char* name, size_t len) -> ZNode*;
    static void zset_update(ZSet* zset, ZNode* node, double score);
    static void zset_tree_insert(ZSet* zset, ZNode* node);
    static auto zless(AvlNode* node, double score, const char* name, size_t len) -> bool;
    static auto zless(AvlNode* lhs, AvlNode* rhs) -> bool;
};

struct Entry {
    struct HashNode node; // hashtable node
    std::string key;
    // value: either a string or a sorted set
    std::variant<std::string, ZSet> value;

    Entry() = default;
    Entry(std::string key, std::variant<std::string, ZSet> val)
        : key(std::move(key)), value(std::move(val)) {}

    [[nodiscard]] auto is_str() const -> bool { return std::holds_alternative<std::string>(value); }
    [[nodiscard]] auto is_zset() const -> bool { return std::holds_alternative<ZSet>(value); }
};
