/*
 * Copyright 2019 The libgav1 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBGAV1_SRC_UTILS_COMMON_H_
#define LIBGAV1_SRC_UTILS_COMMON_H_

#if defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
#if defined(_M_X64) || defined(_M_ARM) || defined(_M_ARM64)
#pragma intrinsic(_BitScanReverse64)
#define HAVE_BITSCANREVERSE64
#endif  // defined(_M_X64) || defined(_M_ARM) || defined(_M_ARM64)
#endif  // defined(_MSC_VER)

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/utils/bit_mask_set.h"
#include "src/utils/constants.h"

namespace libgav1 {

// Aligns |value| to the desired |alignment|. |alignment| must be a power of 2.
template <typename T>
inline T Align(T value, T alignment) {
  assert(alignment != 0);
  const T alignment_mask = alignment - 1;
  return (value + alignment_mask) & ~alignment_mask;
}

inline int32_t Clip3(int32_t value, int32_t low, int32_t high) {
  return value < low ? low : (value > high ? high : value);
}

#if defined(__GNUC__)

inline int CountLeadingZeros(uint32_t n) {
  assert(n != 0);
  return __builtin_clz(n);
}

inline int CountLeadingZeros(uint64_t n) {
  assert(n != 0);
  return __builtin_clzll(n);
}

#elif defined(_MSC_VER)

inline int CountLeadingZeros(uint32_t n) {
  unsigned long first_set_bit;  // NOLINT(runtime/int)
  const unsigned char bit_set = _BitScanReverse(
      &first_set_bit, static_cast<unsigned long>(n));  // NOLINT(runtime/int)
  assert(bit_set != 0);
  static_cast<void>(bit_set);
  return 31 - static_cast<int>(first_set_bit);
}

inline int CountLeadingZeros(uint64_t n) {
  unsigned long first_set_bit;  // NOLINT(runtime/int)
#if defined(HAVE_BITSCANREVERSE64)
  const unsigned char bit_set =
      _BitScanReverse64(&first_set_bit, static_cast<unsigned __int64>(n));
#else   // !defined(HAVE_BITSCANREVERSE64)
  const auto n_hi = static_cast<unsigned long>(n >> 32);  // NOLINT(runtime/int)
  if (n_hi != 0 && _BitScanReverse(&first_set_bit, n_hi)) {
    return 31 - static_cast<int>(first_set_bit);
  }
  const unsigned char bit_set = _BitScanReverse(
      &first_set_bit, static_cast<unsigned long>(n));  // NOLINT(runtime/int)
#endif  // defined(HAVE_BITSCANREVERSE64)
  assert(bit_set != 0);
  static_cast<void>(bit_set);
  return 63 - static_cast<int>(first_set_bit);
}

#undef HAVE_BITSCANREVERSE64

#else  // !defined(__GNUC__) && !defined(_MSC_VER)

template <const int kMSB, typename T>
inline int CountLeadingZeros(T n) {
  assert(n != 0);
  const T msb = T{1} << kMSB;
  int count = 0;
  while ((n & msb) == 0) {
    ++count;
    n <<= 1;
  }
  return count;
}

inline int CountLeadingZeros(uint32_t n) { return CountLeadingZeros<31>(n); }

inline int CountLeadingZeros(uint64_t n) { return CountLeadingZeros<63>(n); }

#endif  // defined(__GNUC__)

inline int FloorLog2(int32_t n) {
  assert(n > 0);
  return 31 - CountLeadingZeros(static_cast<uint32_t>(n));
}

inline int FloorLog2(uint32_t n) {
  assert(n > 0);
  return 31 - CountLeadingZeros(n);
}

inline int FloorLog2(int64_t n) {
  assert(n > 0);
  return 63 - CountLeadingZeros(static_cast<uint64_t>(n));
}

inline int FloorLog2(uint64_t n) {
  assert(n > 0);
  return 63 - CountLeadingZeros(n);
}

inline int CeilLog2(unsigned int n) {
  // The expression FloorLog2(n - 1) + 1 is undefined not only for n == 0 but
  // also for n == 1, so this expression must be guarded by the n < 2 test. An
  // alternative implementation is:
  // return (n == 0) ? 0 : FloorLog2(n) + static_cast<int>((n & (n - 1)) != 0);
  return (n < 2) ? 0 : FloorLog2(n - 1) + 1;
}

inline int32_t RightShiftWithRounding(int32_t value, int bits) {
  assert(bits >= 0);
  return (value + ((1 << bits) >> 1)) >> bits;
}

inline uint32_t RightShiftWithRounding(uint32_t value, int bits) {
  assert(bits >= 0);
  return (value + ((1 << bits) >> 1)) >> bits;
}

// This variant is used when |value| can exceed 32 bits. Although the final
// result must always fit into int32_t.
inline int32_t RightShiftWithRounding(int64_t value, int bits) {
  assert(bits >= 0);
  return static_cast<int32_t>((value + ((int64_t{1} << bits) >> 1)) >> bits);
}

inline int32_t RightShiftWithRoundingSigned(int32_t value, int bits) {
  return (value >= 0) ? RightShiftWithRounding(value, bits)
                      : -RightShiftWithRounding(-value, bits);
}

// This variant is used when |value| can exceed 32 bits. Although the final
// result must always fit into int32_t.
inline int32_t RightShiftWithRoundingSigned(int64_t value, int bits) {
  return (value >= 0) ? RightShiftWithRounding(value, bits)
                      : -RightShiftWithRounding(-value, bits);
}

constexpr int DivideBy2(int n) { return n >> 1; }
constexpr int DivideBy4(int n) { return n >> 2; }
constexpr int DivideBy8(int n) { return n >> 3; }
constexpr int DivideBy16(int n) { return n >> 4; }
constexpr int DivideBy32(int n) { return n >> 5; }
constexpr int DivideBy64(int n) { return n >> 6; }
constexpr int DivideBy128(int n) { return n >> 7; }

// Convert |value| to unsigned before shifting to avoid undefined behavior with
// negative values.
inline int LeftShift(int value, int bits) {
  assert(bits >= 0);
  assert(value >= -(int64_t{1} << (31 - bits)));
  assert(value <= (int64_t{1} << (31 - bits)) - ((bits == 0) ? 1 : 0));
  return static_cast<int>(static_cast<uint32_t>(value) << bits);
}
inline int MultiplyBy2(int n) { return LeftShift(n, 1); }
inline int MultiplyBy4(int n) { return LeftShift(n, 2); }
inline int MultiplyBy8(int n) { return LeftShift(n, 3); }
inline int MultiplyBy16(int n) { return LeftShift(n, 4); }
inline int MultiplyBy32(int n) { return LeftShift(n, 5); }
inline int MultiplyBy64(int n) { return LeftShift(n, 6); }

constexpr int Mod32(int n) { return n & 0x1f; }
constexpr int Mod64(int n) { return n & 0x3f; }

//------------------------------------------------------------------------------
// Bitstream functions

constexpr bool IsIntraFrame(FrameType type) {
  return type == kFrameKey || type == kFrameIntraOnly;
}

inline TransformClass GetTransformClass(TransformType tx_type) {
  constexpr BitMaskSet kTransformClassVerticalMask(
      kTransformTypeIdentityDct, kTransformTypeIdentityAdst,
      kTransformTypeIdentityFlipadst);
  if (kTransformClassVerticalMask.Contains(tx_type)) {
    return kTransformClassVertical;
  }
  constexpr BitMaskSet kTransformClassHorizontalMask(
      kTransformTypeDctIdentity, kTransformTypeAdstIdentity,
      kTransformTypeFlipadstIdentity);
  if (kTransformClassHorizontalMask.Contains(tx_type)) {
    return kTransformClassHorizontal;
  }
  return kTransformClass2D;
}

inline int RowOrColumn4x4ToPixel(int row_or_column4x4, Plane plane,
                                 int8_t subsampling) {
  return MultiplyBy4(row_or_column4x4) >> (plane == kPlaneY ? 0 : subsampling);
}

constexpr PlaneType GetPlaneType(Plane plane) {
  return static_cast<PlaneType>(plane != kPlaneY);
}

// 5.11.44.
constexpr bool IsDirectionalMode(PredictionMode mode) {
  return mode >= kPredictionModeVertical && mode <= kPredictionModeD67;
}

// 5.9.3.
//
// |a| and |b| are order hints, treated as unsigned |order_hint_bits|-bit
// integers.
//
// If enable_order_hint is false, returns 0. If enable_order_hint is true,
// returns the signed difference a - b using "modular arithmetic". More
// precisely, the signed difference a - b is treated as a signed
// |order_hint_bits|-bit integer and cast to an int. The returned difference is
// between -(1 << (order_hint_bits - 1)) and (1 << (order_hint_bits - 1)) - 1
// (inclusive).
//
// NOTE: |a| and |b| are the |order_hint_bits| least significant bits of the
// actual values. This function returns the signed difference between the
// actual values. The returned difference is correct as long as the actual
// values are not more than 1 << (order_hint_bits - 1) - 1 apart.
//
// Example: Suppose |order_hint_bits| is 4. Then |a| and |b| are in the range
// [0, 15], and the actual values for |a| and |b| must not be more than 7
// apart. (If the actual values for |a| and |b| are exactly 8 apart, this
// function cannot tell whether the actual value for |a| is before or after the
// actual value for |b|.)
//
// First, consider the order hints 2 and 6. For this simple case, we have
//   GetRelativeDistance(2, 6, true, 4) = 2 - 6 = -4, and
//   GetRelativeDistance(6, 2, true, 4) = 6 - 2 = 4.
//
// On the other hand, consider the order hints 2 and 14. The order hints are
// 12 (> 7) apart, so we need to use the actual values instead. The actual
// values may be 34 (= 2 mod 16) and 30 (= 14 mod 16), respectively. Therefore
// we have
//   GetRelativeDistance(2, 14, true, 4) = 34 - 30 = 4, and
//   GetRelativeDistance(14, 2, true, 4) = 30 - 34 = -4.
inline int GetRelativeDistance(int a, int b, bool enable_order_hint,
                               int order_hint_bits) {
  if (!enable_order_hint) {
    assert(order_hint_bits == 0);
    return 0;
  }
  assert(order_hint_bits > 0);
  assert(a >= 0 && a < (1 << order_hint_bits));
  assert(b >= 0 && b < (1 << order_hint_bits));
  const int diff = a - b;
  const int m = 1 << (order_hint_bits - 1);
  return (diff & (m - 1)) - (diff & m);
}

inline bool IsBlockSmallerThan8x8(BlockSize size) {
  return size < kBlock8x8 && size != kBlock4x16;
}

// Returns true if the either the width or the height of the block is equal to
// four.
inline bool IsBlockDimension4(BlockSize size) {
  return size < kBlock8x8 || size == kBlock16x4;
}

// Converts bitdepth 8, 10, and 12 to array index 0, 1, and 2, respectively.
constexpr int BitdepthToArrayIndex(int bitdepth) { return (bitdepth - 8) >> 1; }

// Maps a square transform to an index between [0, 4]. kTransformSize4x4 maps
// to 0, kTransformSize8x8 maps to 1 and so on.
inline int TransformSizeToSquareTransformIndex(TransformSize tx_size) {
  assert(kTransformWidth[tx_size] == kTransformHeight[tx_size]);

  // The values of the square transform sizes happen to be in the right
  // ranges, so we can just divide them by 4 to get the indexes.
  static_assert(0 <= kTransformSize4x4 && kTransformSize4x4 < 4, "");
  static_assert(4 <= kTransformSize8x8 && kTransformSize8x8 < 8, "");
  static_assert(8 <= kTransformSize16x16 && kTransformSize16x16 < 12, "");
  static_assert(12 <= kTransformSize32x32 && kTransformSize32x32 < 16, "");
  static_assert(16 <= kTransformSize64x64 && kTransformSize64x64 < 20, "");
  return DivideBy4(tx_size);
}

// Gets the corresponding Y/U/V position, to set and get filter masks
// in deblock filtering.
// Returns luma_position if it's Y plane, whose subsampling must be 0.
// Returns the odd position for U/V plane, if there is subsampling.
constexpr int GetDeblockPosition(const int luma_position,
                                 const int subsampling) {
  return luma_position | subsampling;
}

// Returns the size of the residual buffer required to hold the residual values
// for a block or frame of size |rows| by |columns| (taking into account
// |subsampling_x|, |subsampling_y| and |residual_size|). |residual_size| is the
// number of bytes required to represent one residual value.
inline size_t GetResidualBufferSize(const int rows, const int columns,
                                    const int subsampling_x,
                                    const int subsampling_y,
                                    const size_t residual_size) {
  // The subsampling multipliers are:
  //   Both x and y are subsampled: 3 / 2.
  //   Only x or y is subsampled: 2 / 1 (which is equivalent to 4 / 2).
  //   Both x and y are not subsampled: 3 / 1 (which is equivalent to 6 / 2).
  // So we compute the final subsampling multiplier as follows:
  //   multiplier = (2 + (4 >> subsampling_x >> subsampling_y)) / 2.
  const int subsampling_multiplier_num =
      2 + (4 >> subsampling_x >> subsampling_y);
  return (residual_size * rows * columns * subsampling_multiplier_num) >> 1;
}

// This function is equivalent to:
// std::min({kTransformWidthLog2[tx_size] - 2,
//           kTransformWidthLog2[left_tx_size] - 2,
//           2});
constexpr LoopFilterTransformSizeId GetTransformSizeIdWidth(
    TransformSize tx_size, TransformSize left_tx_size) {
  return static_cast<LoopFilterTransformSizeId>(
      static_cast<int>(tx_size > kTransformSize4x16 &&
                       left_tx_size > kTransformSize4x16) +
      static_cast<int>(tx_size > kTransformSize8x32 &&
                       left_tx_size > kTransformSize8x32));
}

}  // namespace libgav1

#endif  // LIBGAV1_SRC_UTILS_COMMON_H_
