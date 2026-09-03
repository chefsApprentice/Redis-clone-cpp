// File: tests/zset_test.cpp
// Unit tests for the sorted-set container (ZSet): score ordering, name
// tie-breaks, score updates, lookup, deletion and range navigation.

#include "utils/avl.h"
#include "utils/container_of.h"
#include "utils/zset.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

// ZSet has no destructor: every case disposes its members before the guard
// goes out of scope, mirroring do_del's dispose-then-free path.
struct ZSetGuard {
    ZSet zs;
    ~ZSetGuard() { ZSet::dispose_zset(zs); }
};

auto first(ZSet* zs) -> ZNode* {
    // AVL roots sit at the median, not the minimum: the in-order minimum is
    // down the left spine, so iteration anchored on `first` walks left.
    AvlNode* node = zs->root;
    if (node == nullptr) {
        return nullptr;
    }
    while (node->left != nullptr) {
        node = node->left;
    }
    return container_of(node, &ZNode::tree);
}

std::vector<std::string> names(ZSet* zs) {
    std::vector<std::string> out;
    for (ZNode* n = first(zs); n != nullptr; n = ZSet::znode_offset(n, +1)) {
        out.emplace_back(n->name, n->len);
    }
    return out;
}

void add(ZSetGuard& g, const std::string& name, double score) {
    ZSet::zset_insert(&g.zs, name.data(), name.size(), score);
}

} // namespace

TEST(ZSetTest, IteratesInScoreOrder) {
    ZSetGuard g;
    add(g, "c", 3);
    add(g, "a", 1);
    add(g, "b", 2);

    EXPECT_EQ(names(&g.zs), (std::vector<std::string>{"a", "b", "c"}));
}

TEST(ZSetTest, EqualScoresOrderedByName) {
    ZSetGuard g;
    add(g, "banana", 1);
    add(g, "apple", 1);
    add(g, "cherry", 1);

    EXPECT_EQ(names(&g.zs), (std::vector<std::string>{"apple", "banana", "cherry"}));
}

TEST(ZSetTest, DuplicateMemberUpdatesScore) {
    ZSetGuard g;
    EXPECT_TRUE(ZSet::zset_insert(&g.zs, "m", 1, 1.0));
    EXPECT_FALSE(ZSet::zset_insert(&g.zs, "m", 1, 5.0)); // existing member

    ZNode* node = ZSet::zset_lookup(&g.zs, "m", 1);
    ASSERT_NE(node, nullptr);
    EXPECT_DOUBLE_EQ(node->score, 5.0);
    EXPECT_EQ(avl_cnt(g.zs.root), 1u); // updated, not duplicated
}

TEST(ZSetTest, LookupMissingReturnsNull) {
    ZSetGuard g;
    add(g, "m", 1);
    EXPECT_EQ(ZSet::zset_lookup(&g.zs, "nope", 4), nullptr);
    EXPECT_EQ(ZSet::zset_lookup(&g.zs, "m", 1), first(&g.zs));
}

TEST(ZSetTest, DeleteByKey) {
    ZSetGuard g;
    add(g, "a", 1);
    add(g, "b", 2);
    add(g, "c", 3);

    const std::string b = "b";
    ZSet::zset_delete(&g.zs, &b);

    EXPECT_EQ(ZSet::zset_lookup(&g.zs, "b", 1), nullptr);
    EXPECT_EQ(names(&g.zs), (std::vector<std::string>{"a", "c"}));
    EXPECT_EQ(avl_cnt(g.zs.root), 2u);
}

TEST(ZSetTest, DeleteMissingKeyDoesNothing) {
    ZSetGuard g;
    add(g, "a", 1);

    const std::string absent = "absent";
    ZSet::zset_delete(&g.zs, &absent); // must not crash on a missing member

    EXPECT_EQ(avl_cnt(g.zs.root), 1u);
    EXPECT_EQ(names(&g.zs), (std::vector<std::string>{"a"}));
}

TEST(ZSetTest, UpdateMovesNodeInOrder) {
    ZSetGuard g;
    add(g, "a", 1);
    add(g, "b", 2);
    add(g, "c", 3);
    EXPECT_FALSE(ZSet::zset_insert(&g.zs, "b", 1, 4.0)); // existing member: updates, moves past c

    EXPECT_EQ(names(&g.zs), (std::vector<std::string>{"a", "c", "b"}));
}

TEST(ZSetTest, SeekGeFindsFirstNodeAtOrAbove) {
    ZSetGuard g;
    add(g, "m01", 1);
    add(g, "m02", 2);
    add(g, "m03", 3);
    add(g, "m04", 4);
    add(g, "m05", 5);

    EXPECT_EQ(ZSet::zset_seekge(&g.zs, 2.5, "", 0)->score, 3);
    EXPECT_EQ(ZSet::zset_seekge(&g.zs, 3, "", 0)->score, 3); // exact hit
    EXPECT_EQ(ZSet::zset_seekge(&g.zs, 0, "", 0)->score, 1); // before the first
    EXPECT_EQ(ZSet::zset_seekge(&g.zs, 5.5, "", 0), nullptr); // past the last
}

TEST(ZSetTest, OffsetNavigatesBothDirections) {
    ZSetGuard g;
    add(g, "m01", 1);
    add(g, "m02", 2);
    add(g, "m03", 3);

    ZNode* first_node = ZSet::zset_seekge(&g.zs, 0, "", 0);
    ZNode* third = ZSet::znode_offset(first_node, +2);
    ASSERT_NE(third, nullptr);
    EXPECT_EQ(std::string(third->name, third->len), "m03");

    ZNode* second = ZSet::znode_offset(third, -1);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(std::string(second->name, second->len), "m02");

    EXPECT_EQ(ZSet::znode_offset(third, +1), nullptr); // past the end
    EXPECT_EQ(ZSet::znode_offset(nullptr, +1), nullptr);
}