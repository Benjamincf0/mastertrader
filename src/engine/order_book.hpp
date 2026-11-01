#pragma once

#include "common/types.hpp"
#include "engine/price_level.hpp"
#include <functional>
// Uses a red-black tree. Sorted from low->high by dflt.
#include <map>
#include <unordered_map>

namespace exchange {
struct OrderLocation {
  Side side;
  Price price;
  std::list<Order>::iterator position;
};
struct OrderBook {
  // This is the main data structure for the exchange. It keeps track of the
  // asks and the bids, using maps (which are implemented with self-balancing
  // binary trees so they offer O(logn) lookups). The order book is specific to
  // each ticker, which is why we have a symbol_id to identify the stock it is for.
  SymbolId symbol_id{};

  // Lowest ask first.
  std::map<Price, PriceLevel> asks;

  // Highest bid first.
  std::map<Price, PriceLevel, std::greater<Price>> bids;

  // Constant time lookups when we need to remove or find an order.
  std::unordered_map<OrderId, OrderLocation> order_id_lookup;

  // Adds orders that cannot be matched immediately to the order book.
  void add_resting_order(const Order& order);
};
} // namespace exchange
