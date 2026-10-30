#pragma once

#include "engine/price_level.hpp"
#include <functional>
// Uses a red-black tree. Sorted from low->high by dflt.
#include <map>

namespace exchange {
struct OrderBook {
  SymbolId symbol_id{};

  // Top of order book.
  std::map<Price, PriceLevel> asks;
  
  // Bottom of order book.
  std::map<Price, PriceLevel, std::greater<Price>> bids;
};
} // namespace exchange
