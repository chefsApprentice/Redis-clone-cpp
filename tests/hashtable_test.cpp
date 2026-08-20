// File: tests/hashtable_test.cpp

#include "utils/hashtable.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

using std::string;

// ============================================================
// Basic functionality
// ============================================================

TEST(HashTableTest, SetAndGet) {
    HashTable table;

    ASSERT_EQ(table.hash_set("name", "Alice"), 0);

    auto result = table.hash_get("name");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get(), "Alice");
}

TEST(HashTableTest, MissingKeyReturnsEmpty) {
    HashTable table;

    auto result = table.hash_get("does-not-exist");

    EXPECT_FALSE(result.has_value());
}

// ============================================================
// Overwriting
// ============================================================

TEST(HashTableTest, SetExistingKeyUpdatesValue) {
    HashTable table;

    ASSERT_EQ(table.hash_set("name", "Alice"), 0);
    ASSERT_EQ(table.hash_set("name", "Bob"), 0);

    auto result = table.hash_get("name");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get(), "Bob");
}

TEST(HashTableTest, RepeatedOverwrite) {
    HashTable table;

    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(table.hash_set("key", std::to_string(i)), 0);

        auto result = table.hash_get("key");

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get(), std::to_string(i));
    }
}

// ============================================================
// Removal
// ============================================================

TEST(HashTableTest, RemoveExistingKey) {
    HashTable table;

    ASSERT_EQ(table.hash_set("name", "Alice"), 0);

    table.hash_remove("name");

    auto result = table.hash_get("name");

    EXPECT_FALSE(result.has_value());
}

TEST(HashTableTest, RemoveMissingKeyDoesNothing) {
    HashTable table;

    ASSERT_EQ(table.hash_set("name", "Alice"), 0);

    table.hash_remove("does-not-exist");

    auto result = table.hash_get("name");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get(), "Alice");
}

TEST(HashTableTest, RemoveThenReinsert) {
    HashTable table;

    ASSERT_EQ(table.hash_set("name", "Alice"), 0);

    table.hash_remove("name");

    ASSERT_EQ(table.hash_set("name", "Bob"), 0);

    auto result = table.hash_get("name");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get(), "Bob");
}

// ============================================================
// Growth
// ============================================================

TEST(HashTableTest, GrowsBeyondInitialCapacity) {
    HashTable table;

    constexpr int COUNT = 1000;

    for (int i = 0; i < COUNT; ++i) {
        ASSERT_EQ(table.hash_set("key-" + std::to_string(i), "value-" + std::to_string(i)), 0);
    }

    for (int i = 0; i < COUNT; ++i) {
        auto result = table.hash_get("key-" + std::to_string(i));

        ASSERT_TRUE(result.has_value()) << "Missing key-" << i;

        EXPECT_EQ(result->get(), "value-" + std::to_string(i));
    }
}

// ============================================================
// Fetch while repeatedly growing
//
// This is for the incremental rehashing
// implementation because some nodes can live in the old table
// while others live in the new table.
// ============================================================

TEST(HashTableTest, FetchDuringRepeatedGrowth) {
    HashTable table;

    for (int i = 0; i < 500; ++i) {
        const string key = "key-" + std::to_string(i);

        const string value = "value-" + std::to_string(i);

        ASSERT_EQ(table.hash_set(key, value), 0);

        auto result = table.hash_get(key);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get(), value);

        // Periodically check an older key.
        if (i >= 10 && i % 10 == 0) {
            const string old_key = "key-" + std::to_string(i - 10);

            auto old_result = table.hash_get(old_key);

            ASSERT_TRUE(old_result.has_value());

            EXPECT_EQ(old_result->get(), "value-" + std::to_string(i - 10));
        }
    }
}

// ============================================================
// Collision testing
//
// Initial table size is assumed to be 8 buckets.
// We deliberately find several keys that hash to bucket 0.
// ============================================================

TEST(HashTableTest, HandlesCollisions) {
    HashTable table;

    constexpr std::size_t BUCKET_COUNT = 8;

    std::vector<string> colliding_keys;

    for (int i = 0; i < 100000 && colliding_keys.size() < 10; ++i) {

        string key = "collision-" + std::to_string(i);

        if (HashTable::hash(key) % BUCKET_COUNT == 0) {
            colliding_keys.push_back(key);
        }
    }

    ASSERT_GE(colliding_keys.size(), 10u);

    for (std::size_t i = 0; i < colliding_keys.size(); ++i) {
        ASSERT_EQ(table.hash_set(colliding_keys[i], "value-" + std::to_string(i)), 0);
    }

    for (std::size_t i = 0; i < colliding_keys.size(); ++i) {
        auto result = table.hash_get(colliding_keys[i]);

        ASSERT_TRUE(result.has_value()) << "Missing colliding key: " << colliding_keys[i];

        EXPECT_EQ(result->get(), "value-" + std::to_string(i));
    }
}

// ============================================================
// Collision + deletion
//
// Tests removing:
//
//     first node
//     middle node
//     last node
//
// from a collision chain.
// ============================================================

TEST(HashTableTest, RemoveFromCollisionChain) {
    HashTable table;

    constexpr std::size_t BUCKET_COUNT = 8;

    std::vector<string> colliding_keys;

    for (int i = 0; i < 100000 && colliding_keys.size() < 5; ++i) {

        string key = "collision-" + std::to_string(i);

        if (HashTable::hash(key) % BUCKET_COUNT == 0) {
            colliding_keys.push_back(key);
        }
    }

    ASSERT_EQ(colliding_keys.size(), 5u);

    for (std::size_t i = 0; i < colliding_keys.size(); ++i) {
        ASSERT_EQ(table.hash_set(colliding_keys[i], "value-" + std::to_string(i)), 0);
    }

    // Remove first.
    table.hash_remove(colliding_keys[0]);

    // Remove middle.
    table.hash_remove(colliding_keys[2]);

    // Remove last.
    table.hash_remove(colliding_keys[4]);

    // Remaining nodes.
    auto result1 = table.hash_get(colliding_keys[1]);

    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->get(), "value-1");

    auto result3 = table.hash_get(colliding_keys[3]);

    ASSERT_TRUE(result3.has_value());
    EXPECT_EQ(result3->get(), "value-3");

    // Removed nodes.
    EXPECT_FALSE(table.hash_get(colliding_keys[0]).has_value());

    EXPECT_FALSE(table.hash_get(colliding_keys[2]).has_value());

    EXPECT_FALSE(table.hash_get(colliding_keys[4]).has_value());
}

// ============================================================
// Growth + deletion
//
// Important for incremental rehashing.
// ============================================================

TEST(HashTableTest, RemoveKeysAfterGrowth) {
    HashTable table;

    constexpr int COUNT = 500;

    for (int i = 0; i < COUNT; ++i) {
        ASSERT_EQ(table.hash_set("key-" + std::to_string(i), "value-" + std::to_string(i)), 0);
    }

    // Remove every second key.
    for (int i = 0; i < COUNT; i += 2) {
        table.hash_remove("key-" + std::to_string(i));
    }

    // Odd keys should still exist.
    for (int i = 1; i < COUNT; i += 2) {
        auto result = table.hash_get("key-" + std::to_string(i));

        ASSERT_TRUE(result.has_value()) << "key-" << i << " disappeared";

        EXPECT_EQ(result->get(), "value-" + std::to_string(i));
    }

    // Even keys should be gone.
    for (int i = 0; i < COUNT; i += 2) {
        EXPECT_FALSE(table.hash_get("key-" + std::to_string(i)).has_value());
    }
}

// ============================================================
// Large values
// ============================================================

TEST(HashTableTest, LargeValues) {
    HashTable table;

    string value(100000, 'A');

    ASSERT_EQ(table.hash_set("large", value), 0);

    auto result = table.hash_get("large");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get(), value);
}

// ============================================================
// Empty value
// ============================================================

TEST(HashTableTest, EmptyValue) {
    HashTable table;

    ASSERT_EQ(table.hash_set("empty", ""), 0);

    auto result = table.hash_get("empty");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get(), "");
}

// ============================================================
// Mixed workload
// ============================================================

TEST(HashTableTest, MixedWorkload) {
    HashTable table;

    constexpr int COUNT = 1000;

    // Insert.
    for (int i = 0; i < COUNT; ++i) {
        ASSERT_EQ(table.hash_set("key-" + std::to_string(i), "value-" + std::to_string(i)), 0);
    }

    // Overwrite every third key.
    for (int i = 0; i < COUNT; i += 3) {
        ASSERT_EQ(table.hash_set("key-" + std::to_string(i), "updated-" + std::to_string(i)), 0);
    }

    // Delete every fifth key.
    for (int i = 0; i < COUNT; i += 5) {
        table.hash_remove("key-" + std::to_string(i));
    }

    // Verify final state.
    for (int i = 0; i < COUNT; ++i) {
        const string key = "key-" + std::to_string(i);

        auto result = table.hash_get(key);

        if (i % 5 == 0) {
            EXPECT_FALSE(result.has_value()) << key << " should have been removed";
        } else if (i % 3 == 0) {
            ASSERT_TRUE(result.has_value()) << key << " should exist";

            EXPECT_EQ(result->get(), "updated-" + std::to_string(i));
        } else {
            ASSERT_TRUE(result.has_value()) << key << " should exist";

            EXPECT_EQ(result->get(), "value-" + std::to_string(i));
        }
    }
}
