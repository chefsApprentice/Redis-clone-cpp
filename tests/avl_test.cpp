// #include "../src/utils/avl.h"
// #include <algorithm>
// #include <cassert>
// #include <cstdint>
// #include <cstdio>
// #include <cstdlib>
// #include <set>
//
// struct Data {
//     AvlNode node;
//     uint32_t val = 0;
// };
//
// struct Container {
//     AvlNode* root = nullptr;
// };
//
// // C++ equivalent of:
// // container_of(ptr, Data, node)
// //
// // Data::node is the first member of Data, so the AvlNode pointer
// // has the same address as the Data object.
// static auto container_of(AvlNode* ptr) -> Data* { return reinterpret_cast<Data*>(ptr); }
//
// static auto container_of(const AvlNode* ptr) -> const Data* {
//     return reinterpret_cast<const Data*>(ptr);
// }
//
// static void add(Container& c, uint32_t val) {
//     Data* data = new Data();
//
//     avl_init(&data->node);
//     data->val = val;
//
//     AvlNode* cur = nullptr;
//     AvlNode** from = &c.root;
//
//     while (*from != nullptr) {
//         cur = *from;
//
//         uint32_t node_val = container_of(cur)->val;
//
//         from = (val < node_val) ? &cur->left : &cur->right;
//     }
//
//     *from = &data->node;
//     data->node.parent = cur;
//
//     c.root = avl_fix(&data->node);
// }
//
// static auto del(Container& c, uint32_t val) -> bool {
//     AvlNode* cur = c.root;
//
//     while (cur != nullptr) {
//         uint32_t node_val = container_of(cur)->val;
//
//         if (val == node_val) {
//             break;
//         }
//
//         cur = val < node_val ? cur->left : cur->right;
//     }
//
//     if (cur == nullptr) {
//         return false;
//     }
//
//     c.root = avl_del(cur);
//
//     delete container_of(cur);
//
//     return true;
// }
//
// static void avl_verify(AvlNode* parent, AvlNode* node) {
//     if (node == nullptr) {
//         return;
//     }
//
//     assert(node->parent == parent);
//
//     avl_verify(node, node->left);
//     avl_verify(node, node->right);
//
//     uint32_t expected = 1 + avl_cnt(node->left) + avl_cnt(node->right);
//
//     if (!(node->cnt == 1 + avl_cnt(node->left) + avl_cnt(node->right))) {
//         fprintf(stderr, "counts: %u %u %u \n", avl_cnt(node), avl_cnt(node->left),
//                 avl_cnt(node->right));
//     }
//     assert(node->cnt == 1 + avl_cnt(node->left) + avl_cnt(node->right));
//
//     uint32_t l = avl_height(node->left);
//     uint32_t r = avl_height(node->right);
//
//     if (l != r && l + 1 != r && l != r + 1) {
//         fprintf(stderr, "height %u %u \n", l, r);
//     }
//     assert(l == r || l + 1 == r || l == r + 1);
//
//     assert(node->height == 1 + std::max(l, r));
//
//     uint32_t val = container_of(node)->val;
//
//     if (node->left != nullptr) {
//         assert(node->left->parent == node);
//         assert(container_of(node->left)->val <= val);
//     }
//
//     if (node->right != nullptr) {
//         assert(node->right->parent == node);
//         assert(container_of(node->right)->val >= val);
//     }
// }
//
// static void extract(AvlNode* node, std::multiset<uint32_t>& extracted) {
//     if (node == nullptr) {
//         return;
//     }
//
//     extract(node->left, extracted);
//
//     extracted.insert(container_of(node)->val);
//
//     extract(node->right, extracted);
// }
//
// static void container_verify(Container& c, const std::multiset<uint32_t>& ref) {
//     avl_verify(nullptr, c.root);
//
//     assert(avl_cnt(c.root) == ref.size());
//
//     std::multiset<uint32_t> extracted;
//
//     extract(c.root, extracted);
//
//     assert(extracted == ref);
// }
//
// static void dispose(Container& c) {
//     while (c.root != nullptr) {
//         AvlNode* node = c.root;
//
//         c.root = avl_del(c.root);
//
//         delete container_of(node);
//     }
// }
//
// static void test_insert(uint32_t sz) {
//     for (uint32_t val = 0; val < sz; ++val) {
//         Container c;
//         std::multiset<uint32_t> ref;
//
//         for (uint32_t i = 0; i < sz; ++i) {
//             if (i == val) {
//                 continue;
//             }
//
//             add(c, i);
//             ref.insert(i);
//         }
//
//         container_verify(c, ref);
//
//         add(c, val);
//         ref.insert(val);
//
//         container_verify(c, ref);
//
//         dispose(c);
//     }
// }
//
// static void test_insert_dup(uint32_t sz) {
//     for (uint32_t val = 0; val < sz; ++val) {
//         Container c;
//         std::multiset<uint32_t> ref;
//
//         for (uint32_t i = 0; i < sz; ++i) {
//             add(c, i);
//             ref.insert(i);
//         }
//
//         container_verify(c, ref);
//
//         add(c, val);
//         ref.insert(val);
//
//         container_verify(c, ref);
//
//         dispose(c);
//     }
// }
//
// static void test_remove(uint32_t sz) {
//     for (uint32_t val = 0; val < sz; ++val) {
//         Container c;
//         std::multiset<uint32_t> ref;
//
//         for (uint32_t i = 0; i < sz; ++i) {
//             add(c, i);
//             ref.insert(i);
//         }
//
//         container_verify(c, ref);
//
//         assert(del(c, val));
//         ref.erase(val);
//
//         container_verify(c, ref);
//
//         dispose(c);
//     }
// }
//
// auto main() -> int {
//     Container c;
//
//     // Quick tests.
//     container_verify(c, {});
//
//     add(c, 123);
//     container_verify(c, {123});
//
//     assert(!del(c, 124));
//
//     assert(del(c, 123));
//
//     container_verify(c, {});
//
//     // Sequential insertion.
//     std::multiset<uint32_t> ref;
//
//     for (uint32_t i = 0; i < 100; i += 3) {
//         add(c, i);
//         ref.insert(i);
//
//         container_verify(c, ref);
//     }
//
//     // Random insertion.
//     for (uint32_t i = 0; i < 100; ++i) {
//         uint32_t val = static_cast<uint32_t>(rand()) % 1000;
//
//         add(c, val);
//         ref.insert(val);
//
//         container_verify(c, ref);
//     }
//
//     // Random deletion.
//     for (uint32_t i = 0; i < 200; ++i) {
//         uint32_t val = static_cast<uint32_t>(rand()) % 1000;
//
//         auto it = ref.find(val);
//
//         if (it == ref.end()) {
//             assert(!del(c, val));
//         } else {
//             assert(del(c, val));
//             ref.erase(it);
//         }
//
//         container_verify(c, ref);
//     }
//
//     // Insertion/deletion at various positions.
//     for (uint32_t i = 0; i < 200; ++i) {
//         test_insert(i);
//         test_insert_dup(i);
//         test_remove(i);
//     }
//
//     dispose(c);
//
//     return 0;
// }
