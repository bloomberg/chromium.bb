// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_SATURATE_CAST_H_
#define UTIL_SATURATE_CAST_H_

#include <limits>
#include <type_traits>

namespace openscreen {

// Because of the way C++ signed versus unsigned comparison works (i.e., the
// type promotion strategy employed), extra care must be taken to range-check
// the input value. For example, if the current architecture is 32-bits, then
// any int32_t compared with a uint32_t will NOT promote to a int64_t↔int64_t
// comparison. Instead, it will become a uint32_t↔uint32_t comparison (!),
// which will sometimes produce invalid results.

// Case 1: "From" and "To" are either both signed, or are both unsigned. In
// this case, the smaller of the two types will be promoted to match the
// larger's size, and a valid comparison will be made.
template <typename To, typename From>
constexpr typename std::enable_if_t<
    std::is_integral<From>::value && std::is_integral<To>::value &&
        (std::is_signed<From>::value == std::is_signed<To>::value),
    To>
saturate_cast(From from) {
  if (from <= std::numeric_limits<To>::min()) {
    return std::numeric_limits<To>::min();
  }
  if (from >= std::numeric_limits<To>::max()) {
    return std::numeric_limits<To>::max();
  }
  return static_cast<To>(from);
}

// Case 2: "From" is signed, but "To" is unsigned.
template <typename To, typename From>
constexpr typename std::enable_if_t<
    std::is_integral<From>::value && std::is_integral<To>::value &&
        std::is_signed<From>::value && !std::is_signed<To>::value,
    To>
saturate_cast(From from) {
  if (from <= From{0}) {
    return To{0};
  }
  if (static_cast<typename std::make_unsigned_t<From>>(from) >=
      std::numeric_limits<To>::max()) {
    return std::numeric_limits<To>::max();
  }
  return static_cast<To>(from);
}

// Case 3: "From" is unsigned, but "To" is signed.
template <typename To, typename From>
constexpr typename std::enable_if_t<
    std::is_integral<From>::value && std::is_integral<To>::value &&
        !std::is_signed<From>::value && std::is_signed<To>::value,
    To>
saturate_cast(From from) {
  if (from >= static_cast<typename std::make_unsigned_t<To>>(
                  std::numeric_limits<To>::max())) {
    return std::numeric_limits<To>::max();
  }
  return static_cast<To>(from);
}

}  // namespace openscreen

#endif  // UTIL_SATURATE_CAST_H_
