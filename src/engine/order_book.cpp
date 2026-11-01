#include "order_book.hpp"
#include "order.hpp"
#include <iterator>

namespace exchange {
void OrderBook::add_resting_order(const Order &order) {
  if (order_id_lookup.contains(order.id)) {
    return;
  }

  if (order.side == Side::Buy) {
    PriceLevel &level = bids[order.price];
    level.orders.push_back(order);
    order_id_lookup.emplace(
        order.id,
        OrderLocation{order.side, order.price, std::prev(level.orders.end())});

  } else if (order.side == Side::Sell) {
    PriceLevel &level = asks[order.price];
    level.orders.push_back(order);
    order_id_lookup.emplace(
        order.id,
        OrderLocation{order.side, order.price, std::prev(level.orders.end())});
  }
}

} // namespace exchange
