#pragma once

#include <cstdint>

namespace exchange {

using Price = std::int64_t;
using Qty = std::int64_t;
using OrderId = std::uint64_t;
using AccountId = std::uint64_t;
using SymbolId = std::uint32_t;

enum class Side { Buy, Sell };

} // namespace exchange
