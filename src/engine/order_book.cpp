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

bool OrderBook::cancel_order(OrderId id) {
  auto found = order_id_lookup.find(id);
  if (found == order_id_lookup.end()) {
    return false;
  }

  const OrderLocation &location = found->second;
  if (location.side == Side::Buy) {
    auto level = bids.find(location.price);
    level->second.orders.erase(location.position);
    if (level->second.orders.empty()) {
      bids.erase(level);
    }
  } else {
    auto level = asks.find(location.price);
    level->second.orders.erase(location.position);
    if (level->second.orders.empty()) {
      asks.erase(level);
    }
  }

  order_id_lookup.erase(found);
  return true;
}

} // namespace exchange
