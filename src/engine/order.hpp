#pragma once
#include "common/types.hpp"

// Avoid using "using namespace exchange" in header files because it affects
// files that include them.
namespace exchange {
struct Order {
  OrderId id{};
  AccountId account_id{};
  SymbolId symbol_id{};
  Side side = Side::Buy; // Buy is the default.
  Price price{};
  // For partial order fills, there will be a quantity remaining in the book.
  Qty remaining{};
};
} // namespace exchange
