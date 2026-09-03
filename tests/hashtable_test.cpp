// File: tests/hashtable_test.cpp

#include "utils/container_of.h"
#include "utils/hashtable.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

using std::string;

// A heap-allocated item with an embedded HashNode, owned by TestStore below.
// Mirrors Entry in req_res.cpp: the table stores only the node address.
struct TestEntry {
    HashNode node;
    std::string key;
    std::string value;

    TestEntry(std::string k, std::string v) : key(std::move(k)), value(std::move(v)) {}
};

static auto entry_eq(const HashNode* node, const string& key) -> bool {
    return container_of(node, &TestEntry::node)->key == key;
}

// Owning wrapper around HashTable presenting the value-keyed API (hash_set /
// hash_get / hash_remove) these cases were written against. The table itself
// is non-owning, so the live TestEntries live in `entries`: hash_set inserts
// a new entry or updates an existing one, hash_remove unlinks and frees.
class TestStore {
    HashTable table;
    std::vector<std::unique_ptr<TestEntry>> entries;

  public:
    auto hash_set(const string& key, const string& value) -> void {
        if (HashNode* found = table.hash_get(key, entry_eq)) {
            container_of(found, &TestEntry::node)->value = value;
            return;
        }
        auto entry = std::unique_ptr<TestEntry>(new TestEntry(key, value));
        table.hash_add(key, &entry->node);
        entries.push_back(std::move(entry));
    }

    auto hash_get(const string& key) -> TestEntry* {
        HashNode* found = table.hash_get(key, entry_eq);
        return (found != nullptr) ? container_of(found, &TestEntry::node) : nullptr;
    }

    auto hash_remove(const string& key) -> void {
        HashNode* found = table.hash_remove(key, entry_eq);
        if (found == nullptr) {
            return;
        }
        TestEntry* removed = container_of(found, &TestEntry::node);
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (it->get() == removed) {
                entries.erase(it);
                return;
            }
        }
    }
};

// ============================================================
// Basic functionality
// ============================================================

TEST(HashTableTest, SetAndGet) {
    TestStore table;

    table.hash_set("name", "Alice");

    TestEntry* result = table.hash_get("name");

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->value, "Alice");
}

TEST(HashTableTest, MissingKeyReturnsEmpty) {
    TestStore table;

    TestEntry* result = table.hash_get("does-not-exist");

    EXPECT_EQ(result, nullptr);
}

// ============================================================
// Overwriting
// ============================================================

TEST(HashTableTest, SetExistingKeyUpdatesValue) {
    TestStore table;

    table.hash_set("name", "Alice");
    table.hash_set("name", "Bob");

    TestEntry* result = table.hash_get("name");

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->value, "Bob");
}

TEST(HashTableTest, RepeatedOverwrite) {
    TestStore table;

    for (int i = 0; i < 100; ++i) {
        table.hash_set("key", std::to_string(i));

        TestEntry* result = table.hash_get("key");

        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->value, std::to_string(i));
    }
}

// ============================================================
// Removal
// ============================================================

TEST(HashTableTest, RemoveExistingKey) {
    TestStore table;

    table.hash_set("name", "Alice");

    table.hash_remove("name");

    TestEntry* result = table.hash_get("name");

    EXPECT_EQ(result, nullptr);
}

TEST(HashTableTest, RemoveMissingKeyDoesNothing) {
    TestStore table;

    table.hash_set("name", "Alice");

    table.hash_remove("does-not-exist");

    TestEntry* result = table.hash_get("name");

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->value, "Alice");
}

TEST(HashTableTest, RemoveThenReinsert) {
    TestStore table;

    table.hash_set("name", "Alice");

    table.hash_remove("name");

    table.hash_set("name", "Bob");

    TestEntry* result = table.hash_get("name");

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->value, "Bob");
}

// ============================================================
// Growth
// ============================================================

TEST(HashTableTest, GrowsBeyondInitialCapacity) {
    TestStore table;

    constexpr int COUNT = 1000;

    for (int i = 0; i < COUNT; ++i) {
        table.hash_set("key-" + std::to_string(i), "value-" + std::to_string(i));
    }

    for (int i = 0; i < COUNT; ++i) {
        TestEntry* result = table.hash_get("key-" + std::to_string(i));

        ASSERT_NE(result, nullptr) << "Missing key-" << i;

        EXPECT_EQ(result->value, "value-" + std::to_string(i));
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
    TestStore table;

    for (int i = 0; i < 500; ++i) {
        const string key = "key-" + std::to_string(i);

        const string value = "value-" + std::to_string(i);

        table.hash_set(key, value);

        TestEntry* result = table.hash_get(key);

        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->value, value);

        // Periodically check an older key.
        if (i >= 10 && i % 10 == 0) {
            const string old_key = "key-" + std::to_string(i - 10);

            TestEntry* old_result = table.hash_get(old_key);

            ASSERT_NE(old_result, nullptr);

            EXPECT_EQ(old_result->value, "value-" + std::to_string(i - 10));
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
    TestStore table;

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
        table.hash_set(colliding_keys[i], "value-" + std::to_string(i));
    }

    for (std::size_t i = 0; i < colliding_keys.size(); ++i) {
        TestEntry* result = table.hash_get(colliding_keys[i]);

        ASSERT_NE(result, nullptr) << "Missing colliding key: " << colliding_keys[i];

        EXPECT_EQ(result->value, "value-" + std::to_string(i));
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
    TestStore table;

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
        table.hash_set(colliding_keys[i], "value-" + std::to_string(i));
    }

    // Remove first.
    table.hash_remove(colliding_keys[0]);

    // Remove middle.
    table.hash_remove(colliding_keys[2]);

    // Remove last.
    table.hash_remove(colliding_keys[4]);

    // Remaining nodes.
    TestEntry* result1 = table.hash_get(colliding_keys[1]);

    ASSERT_NE(result1, nullptr);
    EXPECT_EQ(result1->value, "value-1");

    TestEntry* result3 = table.hash_get(colliding_keys[3]);

    ASSERT_NE(result3, nullptr);
    EXPECT_EQ(result3->value, "value-3");

    // Removed nodes.
    EXPECT_EQ(table.hash_get(colliding_keys[0]), nullptr);

    EXPECT_EQ(table.hash_get(colliding_keys[2]), nullptr);

    EXPECT_EQ(table.hash_get(colliding_keys[4]), nullptr);
}

// ============================================================
// Growth + deletion
//
// Important for incremental rehashing.
// ============================================================

TEST(HashTableTest, RemoveKeysAfterGrowth) {
    TestStore table;

    constexpr int COUNT = 500;

    for (int i = 0; i < COUNT; ++i) {
        table.hash_set("key-" + std::to_string(i), "value-" + std::to_string(i));
    }

    // Remove every second key.
    for (int i = 0; i < COUNT; i += 2) {
        table.hash_remove("key-" + std::to_string(i));
    }

    // Odd keys should still exist.
    for (int i = 1; i < COUNT; i += 2) {
        TestEntry* result = table.hash_get("key-" + std::to_string(i));

        ASSERT_NE(result, nullptr) << "key-" << i << " disappeared";

        EXPECT_EQ(result->value, "value-" + std::to_string(i));
    }

    // Even keys should be gone.
    for (int i = 0; i < COUNT; i += 2) {
        EXPECT_EQ(table.hash_get("key-" + std::to_string(i)), nullptr);
    }
}

// ============================================================
// Large values
// ============================================================

TEST(HashTableTest, LargeValues) {
    TestStore table;

    string value(100000, 'A');

    table.hash_set("large", value);

    TestEntry* result = table.hash_get("large");

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->value, value);
}

// ============================================================
// Empty value
// ============================================================

TEST(HashTableTest, EmptyValue) {
    TestStore table;

    table.hash_set("empty", "");

    TestEntry* result = table.hash_get("empty");

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->value, "");
}

// ============================================================
// Mixed workload
// ============================================================

TEST(HashTableTest, MixedWorkload) {
    TestStore table;

    constexpr int COUNT = 1000;

    // Insert.
    for (int i = 0; i < COUNT; ++i) {
        table.hash_set("key-" + std::to_string(i), "value-" + std::to_string(i));
    }

    // Overwrite every third key.
    for (int i = 0; i < COUNT; i += 3) {
        table.hash_set("key-" + std::to_string(i), "updated-" + std::to_string(i));
    }

    // Delete every fifth key.
    for (int i = 0; i < COUNT; i += 5) {
        table.hash_remove("key-" + std::to_string(i));
    }

    // Verify final state.
    for (int i = 0; i < COUNT; ++i) {
        const string key = "key-" + std::to_string(i);

        TestEntry* result = table.hash_get(key);

        if (i % 5 == 0) {
            EXPECT_EQ(result, nullptr) << key << " should have been removed";
        } else if (i % 3 == 0) {
            ASSERT_NE(result, nullptr) << key << " should exist";

            EXPECT_EQ(result->value, "updated-" + std::to_string(i));
        } else {
            ASSERT_NE(result, nullptr) << key << " should exist";

            EXPECT_EQ(result->value, "value-" + std::to_string(i));
        }
    }
}