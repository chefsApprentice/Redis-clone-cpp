#include <gtest/gtest.h>
#include "utils/buffer.h"

TEST(Buffer, StartsEmpty) {
    Buffer buffer;

    EXPECT_EQ(buf_size(&buffer), 0);
}
