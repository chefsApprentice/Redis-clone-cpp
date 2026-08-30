#include "./zset.h"
#include "utils/avl.h"
#include "utils/container_of.h"
#include "utils/hashtable.h"
#include <string>

static auto zless(AvlNode* lhs, AvlNode* rhs) -> bool {
    ZNode* zl = container_of(lhs, &ZNode::tree);
    ZNode* zr = container_of(rhs, &ZNode::tree);
    if (zl->score != zr->score) {
        return zl->score < zl->score;
    }
    int rv = memcmp(zl->name, zr->name, std::min(zl->len, zr->len));
    return (rv != 0) ? (rv < 0) : (zl->len < zr->len);
}

auto zless(AvlNode* node, double score, const char* name, size_t len) -> bool {
    ZNode* znode = container_of(node, &ZNode::tree);
    if (znode->score != score) {
        return znode->score < score;
    }
    int rv = memcmp(znode->name, name, std::min(znode->len, len));
    return (rv != 0) ? (rv < 0) : (znode->len < len);
}

static void zset_tree_insert(ZSet* zset, ZNode* node) {
    AvlNode* parent = nullptr;    // insert under this node
    AvlNode** from = &zset->root; // the incoming pointer to the next node
    while (*from != nullptr) {    // tree search
        parent = *from;
        from = zless(&node->tree, parent) ? &parent->left : &parent->right;
    }
    *from = &node->tree; // attach the new node
    node->tree.parent = parent;
    zset->root = avl_fix(&node->tree);
}

static void zset_update(ZSet* zset, ZNode* node, double score) {
    // detach the tree node
    zset->root = avl_del(&node->tree);
    avl_init(&node->tree);
    // reinsert the tree node
    node->score = score;
    zset_tree_insert(zset, node);
}

// I am setting tree and hashmap in two seperate ways, it is ugly but icba rn.
auto ZSet::zset_insert(ZSet* zset, const char* name, size_t len, double score) -> bool {
    if (ZNode* node = zset_lookup(zset, name, len)) {
        zset_update(zset, node, score);
        return false;
    }
    ZNode* node = ZNode::znode_new(name, len, score, &zset->hmap);
    zset_tree_insert(zset, node);
    return true;
};

auto zset_lookup(ZSet* zset, const char* name, size_t len) -> ZNode* {
    if (zset->root == nullptr) {
        return nullptr;
    }
    HashNode* found = zset->hmap.hash_get(std::string(name, len));
    return (found != nullptr) ? container_of(found, &ZNode::hnode) : nullptr;
}
auto zset_lookup(ZSet* zset, const string* key) -> ZNode* {
    if (zset->root == nullptr) {
        return nullptr;
    }
    HashNode* found = zset->hmap.hash_get(*key);
    return (found != nullptr) ? container_of(found, &ZNode::hnode) : nullptr;
}

void zset_delete(ZSet* zset, ZNode* node) {
    zset->hmap.hash_remove(node->name);
    zset->root = avl_del(&node->tree);
    ZNode::znode_del(node);
};

// void zset_clear(ZSet* zset, ZNode* node);

auto zset_seekge(ZSet* zset, double score, const char* name, size_t len) -> ZNode* {
    AvlNode* found = nullptr;
    for (AvlNode* node = zset->root; node != nullptr;) {
        if (zless(node, score, name, len)) {
            node = node->right; // node < key
        } else {
            found = node; // candidate
            node = node->left;
        }
    }
    return (found != nullptr) ? container_of(found, &ZNode::tree) : nullptr;
}

auto avl_offset(AvlNode* node, int64_t offset) -> AvlNode*;

auto znode_offset(ZNode* node, int64_t offset) -> ZNode* {
    AvlNode* tnode = (node != nullptr) ? avl_offset(&node->tree, offset) : nullptr;
    return (tnode != nullptr) ? container_of(tnode, &ZNode::tree) : nullptr;
}

