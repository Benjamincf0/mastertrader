#include <gtest/gtest.h>
#include "common/types.hpp"
#include "engine/order.hpp"
#include "engine/order_book.hpp"

TEST(Setup, ArithmeticWorks) {

    exchange::OrderId potato = 2;
    EXPECT_EQ(potato + 2, 4);
}
