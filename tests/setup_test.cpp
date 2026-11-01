#include "common/types.hpp"
#include "engine/order.hpp"
#include "engine/order_book.hpp"
#include <gtest/gtest.h>

using namespace exchange;

TEST(Setup, OrderBookWorks) {

  OrderBook order_book = OrderBook();

  // Sell 50 shares of stock 101 at 10.99$ each.
  Order sell1 = Order{1, 2934, 101, Side::Sell, 1099, 50};
  order_book.add_resting_order(sell1);

  // Sell 30 shares of stock 101 at 10.99$ each.
  Order sell2 = Order{2, 2934, 101, Side::Sell, 1099, 30};
  order_book.add_resting_order(sell2);

  ASSERT_EQ(order_book.asks.at(1099).orders.size(), 2);
  EXPECT_EQ(order_book.asks.at(1099).orders.front().id, 1);
  EXPECT_EQ(order_book.asks.at(1099).orders.back().id, 2);
  EXPECT_EQ(order_book.order_id_lookup.at(1).position->id, 1);
  EXPECT_EQ(order_book.order_id_lookup.at(2).position->id, 2);

  // Buy 50 shares of stock 101 at 10.99$ each.
  Order buy1 = Order{3, 2934, 101, Side::Buy, 1099, 50};
  order_book.add_resting_order(buy1);

  // Buy 30 shares of stock 101 at 10.99$ each.
  Order buy2 = Order{4, 2934, 101, Side::Buy, 1099, 30};
  order_book.add_resting_order(buy2);

  ASSERT_EQ(order_book.bids.at(1099).orders.size(), 2);
  EXPECT_EQ(order_book.bids.at(1099).orders.front().id, 3);
  EXPECT_EQ(order_book.bids.at(1099).orders.back().id, 4);
  EXPECT_EQ(order_book.order_id_lookup.at(3).position->id, 3);
  EXPECT_EQ(order_book.order_id_lookup.at(4).position->id, 4);
}

TEST(OrderBook, CancelRemovesOnlySelectedOrderAndEmptyLevels) {
  OrderBook book{};
  book.add_resting_order(Order{1, 2934, 101, Side::Sell, 1099, 50});
  book.add_resting_order(Order{2, 2934, 101, Side::Sell, 1099, 30});
  book.add_resting_order(Order{3, 2934, 101, Side::Buy, 1098, 20});

  EXPECT_FALSE(book.cancel_order(999));
  EXPECT_TRUE(book.cancel_order(1));
  ASSERT_EQ(book.asks.at(1099).orders.size(), 1);
  EXPECT_EQ(book.asks.at(1099).orders.front().id, 2);
  EXPECT_FALSE(book.order_id_lookup.contains(1));
  EXPECT_EQ(book.order_id_lookup.at(2).position->id, 2);

  EXPECT_TRUE(book.cancel_order(2));
  EXPECT_TRUE(book.asks.empty());
  EXPECT_TRUE(book.cancel_order(3));
  EXPECT_TRUE(book.bids.empty());
  EXPECT_TRUE(book.order_id_lookup.empty());
  EXPECT_FALSE(book.cancel_order(1));
}
