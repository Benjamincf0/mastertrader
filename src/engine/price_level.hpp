#pragma once
#include "common/types.hpp"
#include "order.hpp"
#include <list>

namespace exchange {
struct PriceLevel {
  std::list<Order> orders;
};
} // namespace exchange
