#pragma once

#include <cstdint>

struct AvlNode {
    AvlNode* parent = nullptr;
    AvlNode* left = nullptr;
    AvlNode* right = nullptr;
    uint32_t height = 0; // subtree height
    uint32_t cnt = 0;    // subtree size
};

inline void avl_init(AvlNode* node) {
    node->left = node->right = node->parent = nullptr;
    node->height = 1;
    node->cnt = 1;
}

// helpers
inline auto avl_height(AvlNode* node) -> uint32_t { return (node != nullptr) ? node->height : 0; }
inline auto avl_cnt(AvlNode* node) -> uint32_t { return (node != nullptr) ? node->cnt : 0; }

// API
auto avl_fix(AvlNode* node) -> AvlNode*;
auto avl_del(AvlNode* node) -> AvlNode*;
