/* Copyright 2019 Google LLC. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef RUY_RUY_SIZE_UTIL_H_
#define RUY_RUY_SIZE_UTIL_H_

#include <type_traits>

#include "ruy/check_macros.h"

#ifdef _WIN32
#include <intrin.h>
#endif

namespace ruy {

template <typename Integer>
inline Integer floor_log2(Integer n) {
  static_assert(std::is_integral<Integer>::value, "");
  static_assert(std::is_signed<Integer>::value, "");
  static_assert(sizeof(Integer) == 4 || sizeof(Integer) == 8, "");

  RUY_DCHECK_GE(n, 1);
#ifdef _MSC_VER
  unsigned long result;  // NOLINT[runtime/int]
  if (sizeof(Integer) == 4) {
    _BitScanReverse(&result, n);
  } else {
#if defined(_M_X64) || defined(_M_ARM64)
    // _BitScanReverse64 is supported only on 64-bit MSVC platforms
    _BitScanReverse64(&result, static_cast<unsigned __int64>(n));
#else
    // Emulate using 32-bit _BitScanReverse
    const uint32_t n_hi = uint64_t(n) >> 32;
    if (n_hi == 0) {
      _BitScanReverse(&result, static_cast<unsigned long>(n));
    } else {
      _BitScanReverse(&result, static_cast<unsigned long>(n_hi));
      result += 32;
    }
#endif  // defined(_M_X64) || defined(_M_ARM64)
  }
  return result;
#else
  if (sizeof(Integer) == 4) {
    return 31 - __builtin_clz(n);
  } else {
    return 63 - __builtin_clzll(n);
  }
#endif
}

template <typename Integer>
Integer ceil_log2(Integer n) {
  RUY_DCHECK_GE(n, 1);
  return n == 1 ? 0 : floor_log2(n - 1) + 1;
}

template <typename Integer>
constexpr bool is_pot(Integer value) {
  return (value > 0) && ((value & (value - 1)) == 0);
}

template <typename Integer>
Integer pot_log2(Integer n) {
  RUY_DCHECK(is_pot(n));
  return floor_log2(n);
}

template <typename Integer>
Integer round_down_pot(Integer value) {
  return static_cast<Integer>(1) << floor_log2(value);
}

template <typename Integer>
Integer round_up_pot(Integer value) {
  return static_cast<Integer>(1) << ceil_log2(value);
}

template <typename Integer, typename Modulo>
Integer round_down_pot(Integer value, Modulo modulo) {
  RUY_DCHECK_EQ(modulo & (modulo - 1), 0);
  return value & ~(modulo - 1);
}

template <typename Integer, typename Modulo>
Integer round_up_pot(Integer value, Modulo modulo) {
  return round_down_pot(value + modulo - 1, modulo);
}

}  // namespace ruy

#endif  // RUY_RUY_SIZE_UTIL_H_
