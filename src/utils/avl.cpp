// utils/avl.cpp
#include <cassert>
#include "./utils/avl.h"

static auto max(uint32_t lhs, uint32_t rhs) -> uint32_t { return lhs < rhs ? rhs : lhs; }

// maintain the height and cnt field
static void avl_update(AvlNode* node) {
    node->height = 1 + max(avl_height(node->left), avl_height(node->right));
    node->cnt = 1 + avl_cnt(node->left) + avl_cnt(node->right);
}

static auto rot_left(AvlNode* node) -> AvlNode* {
    AvlNode* parent = node->parent;
    AvlNode* new_node = node->right;
    AvlNode* inner = new_node->left;
    // node <-> inner
    node->right = inner;
    if (inner) {
        inner->parent = node;
    }
    // parent <- new_node
    new_node->parent = parent;
    // new_node <-> node
    new_node->left = node;
    node->parent = new_node;
    // auxiliary data
    avl_update(node);
    avl_update(new_node);
    return new_node;
}

static auto rot_right(AvlNode* node) -> AvlNode* {
    AvlNode* parent = node->parent;
    AvlNode* new_node = node->left;
    AvlNode* inner = new_node->right;
    // node <-> inner
    node->left = inner;
    if (inner) {
        inner->parent = node;
    }
    // parent <- new_node
    new_node->parent = parent;
    // new_node <-> node
    new_node->right = node;
    node->parent = new_node;
    // auxiliary data
    avl_update(node);
    avl_update(new_node);
    return new_node;
}

// the left subtree is taller by 2
static auto avl_fix_left(AvlNode* node) -> AvlNode* {
    if (avl_height(node->left->left) < avl_height(node->left->right)) {
        node->left = rot_left(node->left); // Transformation 2
    }
    return rot_right(node); // Transformation 1
}

// the right subtree is taller by 2
static auto avl_fix_right(AvlNode* node) -> AvlNode* {
    if (avl_height(node->right->right) < avl_height(node->right->left)) {
        node->right = rot_right(node->right);
    }
    return rot_left(node);
}

// fix imbalanced nodes and maintain invariants until the root is reached
auto avl_fix(AvlNode* node) -> AvlNode* {
    while (true) {
        AvlNode** from = &node; // save the fixed subtree here
        AvlNode* parent = node->parent;
        if (parent) {
            // attach the fixed subtree to the parent
            from = parent->left == node ? &parent->left : &parent->right;
        } // else: save to the local variable `node`
        // auxiliary data
        avl_update(node);
        // fix the height difference of 2
        uint32_t l = avl_height(node->left);
        uint32_t r = avl_height(node->right);
        if (l == r + 2) {
            *from = avl_fix_left(node);
        } else if (l + 2 == r) {
            *from = avl_fix_right(node);
        }
        // root node, stop
        if (!parent) {
            return *from;
        }
        // continue to the parent node because its height may be changed
        node = parent;
    }
}
static auto avl_del_easy(AvlNode* node) -> AvlNode* {
    assert(!node->left || !node->right);
    AvlNode* child = node->left ? node->left : node->right;
    AvlNode* parent = node->parent;
    if (child) {
        child->parent = parent;
    }
    if (!parent) {
        return child;
    }
    AvlNode** from =
        parent->left == node ? &parent->left : &parent->right;
    *from = child;
    return parent;
}

auto avl_del(AvlNode* node) -> AvlNode* {
    if (!node->left || !node->right) {
        AvlNode* root = avl_del_easy(node);
        if (root) {
            return avl_fix(root);
        }
        return nullptr;
    }

    // Find successor.
    AvlNode* victim = node->right;
    while (victim->left) {
        victim = victim->left;
    }

    // Remove successor from its old position.
    // This changes node->right if victim == node->right.
    AvlNode* fix_from = avl_del_easy(victim);
    // Replace node's contents with successor's contents.
    *victim = *node;
    if (victim->left) {
        victim->left->parent = victim;
    }
    if (victim->right) {
        victim->right->parent = victim;
    }

    // Reattach victim where node used to be.
    AvlNode* parent = node->parent;
    if (!parent) {
        victim->parent = nullptr;
    } else {
        victim->parent = parent;

        if (parent->left == node) {
            parent->left = victim;
        } else {
            parent->right = victim;
        }
    }

    avl_update(victim);
    // Rebalance from the part of the tree whose structure actually shrank.
    if (fix_from == node) {
        fix_from = victim;
    }
    return avl_fix(fix_from);
}

// offset into the succeeding or preceding node.
// note: the worst-case is O(log N) regardless of how long the offset is.
auto avl_offset(AvlNode* node, int64_t offset) -> AvlNode* {
    int64_t pos = 0; // the rank difference from the starting node
    while (offset != pos) {
        if (pos < offset && pos + avl_cnt(node->right) >= offset) {
            // the target is inside the right subtree
            node = node->right;
            pos += avl_cnt(node->left) + 1;
        } else if (pos > offset && pos - avl_cnt(node->left) <= offset) {
            // the target is inside the left subtree
            node = node->left;
            pos -= avl_cnt(node->right) + 1;
        } else {
            // go to the parent
            AvlNode* parent = node->parent;
            if (!parent) {
                return nullptr;
            }
            if (parent->right == node) {
                pos -= avl_cnt(node->left) + 1;
            } else {
                pos += avl_cnt(node->right) + 1;
            }
            node = parent;
        }
    }
    return node;
}

static auto successor(AvlNode *node) -> AvlNode * {
    // find the leftmost node in the right subtree
    if (node->right != nullptr) {
        for (node = node->right; node->left != nullptr; node = node->left) {}
        return node;
    }
    // find the ancestor where I'm the rightmost node in the left subtree
    while (AvlNode *parent = node->parent) {
        if (node == parent->left) {
            return parent;
        }
        node = parent;
    }
    return nullptr;
}
static auto predecessor(AvlNode *node) -> AvlNode * {
   // find the rightmost node in the left subtree
    if (node->left != nullptr) {
        for (node = node->left; node->right != nullptr; node = node->right) {}
        return node;
    }
    // find the ancestor where I'm the rightmost node in the left subtree
    while (AvlNode *parent = node->parent) {
        if (node == parent->right) {
            return parent;
        }
        node = parent;
    }
    return nullptr;
}
