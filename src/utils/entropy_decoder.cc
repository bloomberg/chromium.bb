// Copyright 2019 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/utils/entropy_decoder.h"

#include <cassert>
#include <cstring>

#include "src/utils/common.h"
#include "src/utils/compiler_attributes.h"
#include "src/utils/constants.h"

#if defined(__ARM_NEON__) || defined(__aarch64__) || \
    (defined(_MSC_VER) && defined(_M_ARM))
#define LIBGAV1_ENTROPY_DECODER_ENABLE_NEON 1
#else
#define LIBGAV1_ENTROPY_DECODER_ENABLE_NEON 0
#endif

#if LIBGAV1_ENTROPY_DECODER_ENABLE_NEON
#include <arm_neon.h>
#endif

namespace libgav1 {
namespace {

constexpr uint32_t kReadBitMask = ~255;
// This constant is used to set the value of |bits_| so that bits can be read
// after end of stream without trying to refill the buffer for a reasonably long
// time.
constexpr int kLargeBitCount = 0x4000;
constexpr int kCdfPrecision = 6;
constexpr int kMinimumProbabilityPerSymbol = 4;

// This function computes the "cur" variable as specified inside the do-while
// loop in Section 8.2.6 of the spec. This function is monotonically
// decreasing as the values of index increases (note that the |cdf| array is
// sorted in decreasing order).
uint32_t ScaleCdf(uint32_t values_in_range_shifted, const uint16_t* const cdf,
                  int index, int symbol_count) {
  return ((values_in_range_shifted * (cdf[index] >> kCdfPrecision)) >> 1) +
         (kMinimumProbabilityPerSymbol * (symbol_count - index));
}

void UpdateCdf(uint16_t* const cdf, const int symbol_count, const int symbol) {
  const uint16_t count = cdf[symbol_count];
  // rate is computed in the spec as:
  //  3 + ( cdf[N] > 15 ) + ( cdf[N] > 31 ) + Min(FloorLog2(N), 2)
  // In this case cdf[N] is |count|.
  // Min(FloorLog2(N), 2) is 1 for symbol_count == {2, 3} and 2 for all
  // symbol_count > 3. So the equation becomes:
  //  4 + (count > 15) + (count > 31) + (symbol_count > 3).
  // Note that the largest value for count is 32 (it is not incremented beyond
  // 32). So using that information:
  //  count >> 4 is 0 for count from 0 to 15.
  //  count >> 4 is 1 for count from 16 to 31.
  //  count >> 4 is 2 for count == 31.
  // Now, the equation becomes:
  //  4 + (count >> 4) + (symbol_count > 3).
  // Since (count >> 4) can only be 0 or 1 or 2, the addition can be replaced
  // with bitwise or. So the final equation is:
  // (4 | (count >> 4)) + (symbol_count > 3).
  const int rate = (4 | (count >> 4)) + static_cast<int>(symbol_count > 3);
  // Hints for further optimizations:
  //
  // 1. clang can vectorize this for loop with width 4, even though the loop
  // contains an if-else statement. Therefore, it may be advantageous to use
  // "i < symbol_count" as the loop condition when symbol_count is 8, 12, or 16
  // (a multiple of 4 that's not too small).
  //
  // 2. The for loop can be rewritten in the following form, which would enable
  // clang to vectorize the loop with width 8:
  //
  //   const int rounding = (1 << rate) - 1;
  //   for (int i = 0; i < symbol_count - 1; ++i) {
  //     const uint16_t a = (i < symbol) ? kCdfMaxProbability : rounding;
  //     cdf[i] += static_cast<int16_t>(a - cdf[i]) >> rate;
  //   }
  //
  // The subtraction (a - cdf[i]) relies on the overflow semantics of unsigned
  // integer arithmetic. The result of the unsigned subtraction is cast to a
  // signed integer and right-shifted. This requires the right shift of a
  // signed integer be an arithmetic shift, which is true for clang, gcc, and
  // Visual C++.
  for (int i = 0; i < symbol_count - 1; ++i) {
    if (i < symbol) {
      cdf[i] += (kCdfMaxProbability - cdf[i]) >> rate;
    } else {
      cdf[i] -= cdf[i] >> rate;
    }
  }
  cdf[symbol_count] += static_cast<uint16_t>(count < 32);
}

// Define the UpdateCdfN functions. UpdateCdfN is a specialized implementation
// of UpdateCdf based on the fact that symbol_count == N. UpdateCdfN uses the
// SIMD instruction sets if available.

#if LIBGAV1_ENTROPY_DECODER_ENABLE_NEON

// The UpdateCdf() method contains the following for loop:
//
//   for (int i = 0; i < symbol_count - 1; ++i) {
//     if (i < symbol) {
//       cdf[i] += (kCdfMaxProbability - cdf[i]) >> rate;
//     } else {
//       cdf[i] -= cdf[i] >> rate;
//     }
//   }
//
// It can be rewritten in the following two forms, which are amenable to SIMD
// implementations:
//
//   const int rounding = (1 << rate) - 1;
//   for (int i = 0; i < symbol_count - 1; ++i) {
//     const uint16_t a = (i < symbol) ? kCdfMaxProbability : rounding;
//     cdf[i] += static_cast<int16_t>(a - cdf[i]) >> rate;
//   }
//
// or:
//
//   const int rounding = (1 << rate) - 1;
//   for (int i = 0; i < symbol_count - 1; ++i) {
//     const uint16_t a = (i < symbol) ? (kCdfMaxProbability - rounding) : 0;
//     cdf[i] -= static_cast<int16_t>(cdf[i] - a) >> rate;
//   }
//
// The following ARM NEON implementations use the second form, which seems
// slightly faster.
//
// The cdf array has symbol_count + 1 elements. The first symbol_count elements
// are the CDF. The last element is a count that is initialized to 0 and may
// grow up to 32. The for loop in UpdateCdf updates the CDF in the array. Since
// cdf[symbol_count - 1] is always 0, the for loop does not update
// cdf[symbol_count - 1]. However, it would be correct to have the for loop
// update cdf[symbol_count - 1] anyway: since symbol_count - 1 >= symbol, the
// for loop would take the else branch when i is symbol_count - 1:
//      cdf[i] -= cdf[i] >> rate;
// Since cdf[symbol_count - 1] is 0, cdf[symbol_count - 1] would still be 0
// after the update. The ARM NEON implementations take advantage of this in the
// following two cases:
// 1. When symbol_count is 8 or 16, the vectorized code updates the first
//    symbol_count elements in the array.
// 2. When symbol_count is 7, the vectorized code updates all the 8 elements in
//    the cdf array. Since an invalid CDF value is written into cdf[7], the
//    count in cdf[7] needs to be fixed up after the vectorized code.

void UpdateCdf5(uint16_t* const cdf, const int symbol) {
  uint16x4_t cdf_vec = vld1_u16(cdf);
  const uint16_t count = cdf[5];
  const int rate = (4 | (count >> 4)) + 1;
  const uint16x4_t zero = vdup_n_u16(0);
  const uint16x4_t cdf_max_probability =
      vdup_n_u16(kCdfMaxProbability + 1 - (1 << rate));
  const uint16x4_t index = vcreate_u16(0x0003000200010000);
  const uint16x4_t symbol_vec = vdup_n_u16(symbol);
  const uint16x4_t mask = vclt_u16(index, symbol_vec);
  const uint16x4_t a = vbsl_u16(mask, cdf_max_probability, zero);
  const int16x4_t diff = vreinterpret_s16_u16(vsub_u16(cdf_vec, a));
  const int16x4_t negative_rate = vdup_n_s16(-rate);
  const uint16x4_t delta = vreinterpret_u16_s16(vshl_s16(diff, negative_rate));
  cdf_vec = vsub_u16(cdf_vec, delta);
  vst1_u16(cdf, cdf_vec);
  cdf[5] = count + static_cast<uint16_t>(count < 32);
}

// This version works for |symbol_count| = 7, 8, or 9.
template <int symbol_count>
void UpdateCdf7To9(uint16_t* const cdf, const int symbol) {
  static_assert(symbol_count >= 7 && symbol_count <= 9, "");
  uint16x8_t cdf_vec = vld1q_u16(cdf);
  const uint16_t count = cdf[symbol_count];
  const int rate = (4 | (count >> 4)) + 1;
  const uint16x8_t zero = vdupq_n_u16(0);
  const uint16x8_t cdf_max_probability =
      vdupq_n_u16(kCdfMaxProbability + 1 - (1 << rate));
  const uint16x8_t index = vcombine_u16(vcreate_u16(0x0003000200010000),
                                        vcreate_u16(0x0007000600050004));
  const uint16x8_t symbol_vec = vdupq_n_u16(symbol);
  const uint16x8_t mask = vcltq_u16(index, symbol_vec);
  const uint16x8_t a = vbslq_u16(mask, cdf_max_probability, zero);
  const int16x8_t diff = vreinterpretq_s16_u16(vsubq_u16(cdf_vec, a));
  const int16x8_t negative_rate = vdupq_n_s16(-rate);
  const uint16x8_t delta =
      vreinterpretq_u16_s16(vshlq_s16(diff, negative_rate));
  cdf_vec = vsubq_u16(cdf_vec, delta);
  vst1q_u16(cdf, cdf_vec);
  cdf[symbol_count] = count + static_cast<uint16_t>(count < 32);
}

void UpdateCdf7(uint16_t* const cdf, const int symbol) {
  UpdateCdf7To9<7>(cdf, symbol);
}

void UpdateCdf8(uint16_t* const cdf, const int symbol) {
  UpdateCdf7To9<8>(cdf, symbol);
}

void UpdateCdf11(uint16_t* const cdf, const int symbol) {
  uint16x8_t cdf_vec = vld1q_u16(cdf + 2);
  const uint16_t count = cdf[11];
  cdf[11] = count + static_cast<uint16_t>(count < 32);
  const int rate = (4 | (count >> 4)) + 1;
  if (symbol > 1) {
    cdf[0] += (kCdfMaxProbability - cdf[0]) >> rate;
    cdf[1] += (kCdfMaxProbability - cdf[1]) >> rate;
    const uint16x8_t zero = vdupq_n_u16(0);
    const uint16x8_t cdf_max_probability =
        vdupq_n_u16(kCdfMaxProbability + 1 - (1 << rate));
    const uint16x8_t symbol_vec = vdupq_n_u16(symbol);
    const int16x8_t negative_rate = vdupq_n_s16(-rate);
    const uint16x8_t index = vcombine_u16(vcreate_u16(0x0005000400030002),
                                          vcreate_u16(0x0009000800070006));
    const uint16x8_t mask = vcltq_u16(index, symbol_vec);
    const uint16x8_t a = vbslq_u16(mask, cdf_max_probability, zero);
    const int16x8_t diff = vreinterpretq_s16_u16(vsubq_u16(cdf_vec, a));
    const uint16x8_t delta =
        vreinterpretq_u16_s16(vshlq_s16(diff, negative_rate));
    cdf_vec = vsubq_u16(cdf_vec, delta);
    vst1q_u16(cdf + 2, cdf_vec);
  } else {
    if (symbol != 0) {
      cdf[0] += (kCdfMaxProbability - cdf[0]) >> rate;
      cdf[1] -= cdf[1] >> rate;
    } else {
      cdf[0] -= cdf[0] >> rate;
      cdf[1] -= cdf[1] >> rate;
    }
    const int16x8_t negative_rate = vdupq_n_s16(-rate);
    const uint16x8_t delta = vshlq_u16(cdf_vec, negative_rate);
    cdf_vec = vsubq_u16(cdf_vec, delta);
    vst1q_u16(cdf + 2, cdf_vec);
  }
}

void UpdateCdf13(uint16_t* const cdf, const int symbol) {
  uint16x8_t cdf_vec0 = vld1q_u16(cdf);
  uint16x8_t cdf_vec1 = vld1q_u16(cdf + 4);
  const uint16_t count = cdf[13];
  const int rate = (4 | (count >> 4)) + 1;
  const uint16x8_t zero = vdupq_n_u16(0);
  const uint16x8_t cdf_max_probability =
      vdupq_n_u16(kCdfMaxProbability + 1 - (1 << rate));
  const uint16x8_t symbol_vec = vdupq_n_u16(symbol);
  const int16x8_t negative_rate = vdupq_n_s16(-rate);

  uint16x8_t index = vcombine_u16(vcreate_u16(0x0003000200010000),
                                  vcreate_u16(0x0007000600050004));
  uint16x8_t mask = vcltq_u16(index, symbol_vec);
  uint16x8_t a = vbslq_u16(mask, cdf_max_probability, zero);
  int16x8_t diff = vreinterpretq_s16_u16(vsubq_u16(cdf_vec0, a));
  uint16x8_t delta = vreinterpretq_u16_s16(vshlq_s16(diff, negative_rate));
  cdf_vec0 = vsubq_u16(cdf_vec0, delta);
  vst1q_u16(cdf, cdf_vec0);

  index = vcombine_u16(vcreate_u16(0x0007000600050004),
                       vcreate_u16(0x000b000a00090008));
  mask = vcltq_u16(index, symbol_vec);
  a = vbslq_u16(mask, cdf_max_probability, zero);
  diff = vreinterpretq_s16_u16(vsubq_u16(cdf_vec1, a));
  delta = vreinterpretq_u16_s16(vshlq_s16(diff, negative_rate));
  cdf_vec1 = vsubq_u16(cdf_vec1, delta);
  vst1q_u16(cdf + 4, cdf_vec1);

  cdf[13] = count + static_cast<uint16_t>(count < 32);
}

void UpdateCdf16(uint16_t* const cdf, const int symbol) {
  uint16x8_t cdf_vec = vld1q_u16(cdf);
  const uint16_t count = cdf[16];
  const int rate = (4 | (count >> 4)) + 1;
  const uint16x8_t zero = vdupq_n_u16(0);
  const uint16x8_t cdf_max_probability =
      vdupq_n_u16(kCdfMaxProbability + 1 - (1 << rate));
  const uint16x8_t symbol_vec = vdupq_n_u16(symbol);
  const int16x8_t negative_rate = vdupq_n_s16(-rate);

  uint16x8_t index = vcombine_u16(vcreate_u16(0x0003000200010000),
                                  vcreate_u16(0x0007000600050004));
  uint16x8_t mask = vcltq_u16(index, symbol_vec);
  uint16x8_t a = vbslq_u16(mask, cdf_max_probability, zero);
  int16x8_t diff = vreinterpretq_s16_u16(vsubq_u16(cdf_vec, a));
  uint16x8_t delta = vreinterpretq_u16_s16(vshlq_s16(diff, negative_rate));
  cdf_vec = vsubq_u16(cdf_vec, delta);
  vst1q_u16(cdf, cdf_vec);

  cdf_vec = vld1q_u16(cdf + 8);
  index = vcombine_u16(vcreate_u16(0x000b000a00090008),
                       vcreate_u16(0x000f000e000d000c));
  mask = vcltq_u16(index, symbol_vec);
  a = vbslq_u16(mask, cdf_max_probability, zero);
  diff = vreinterpretq_s16_u16(vsubq_u16(cdf_vec, a));
  delta = vreinterpretq_u16_s16(vshlq_s16(diff, negative_rate));
  cdf_vec = vsubq_u16(cdf_vec, delta);
  vst1q_u16(cdf + 8, cdf_vec);

  cdf[16] = count + static_cast<uint16_t>(count < 32);
}

#else  // !LIBGAV1_ENTROPY_DECODER_ENABLE_NEON

void UpdateCdf5(uint16_t* const cdf, const int symbol) {
  UpdateCdf(cdf, 5, symbol);
}

void UpdateCdf7(uint16_t* const cdf, const int symbol) {
  UpdateCdf(cdf, 7, symbol);
}

void UpdateCdf8(uint16_t* const cdf, const int symbol) {
  UpdateCdf(cdf, 8, symbol);
}

void UpdateCdf11(uint16_t* const cdf, const int symbol) {
  UpdateCdf(cdf, 11, symbol);
}

void UpdateCdf13(uint16_t* const cdf, const int symbol) {
  UpdateCdf(cdf, 13, symbol);
}

void UpdateCdf16(uint16_t* const cdf, const int symbol) {
  UpdateCdf(cdf, 16, symbol);
}

#endif  // LIBGAV1_ENTROPY_DECODER_ENABLE_NEON

}  // namespace

#if !LIBGAV1_CXX17
constexpr uint32_t DaalaBitReader::kWindowSize;  // static.
#endif

DaalaBitReader::DaalaBitReader(const uint8_t* data, size_t size,
                               bool allow_update_cdf)
    : data_(data),
      size_(size),
      data_index_(0),
      allow_update_cdf_(allow_update_cdf) {
  window_diff_ = (WindowSize{1} << (kWindowSize - 1)) - 1;
  values_in_range_ = kCdfMaxProbability;
  bits_ = -15;
  PopulateBits();
}

// This is similar to the ReadSymbol() implementation but it is optimized based
// on the following facts:
//   * The probability is fixed at half. So some multiplications can be replaced
//     with bit operations.
//   * Symbol count is fixed at 2.
int DaalaBitReader::ReadBit() {
  const uint32_t curr =
      ((values_in_range_ & kReadBitMask) >> 1) + kMinimumProbabilityPerSymbol;
  const WindowSize zero_threshold = static_cast<WindowSize>(curr)
                                    << (kWindowSize - 16);
  int bit = 1;
  if (window_diff_ >= zero_threshold) {
    values_in_range_ -= curr;
    window_diff_ -= zero_threshold;
    bit = 0;
  } else {
    values_in_range_ = curr;
  }
  NormalizeRange();
  return bit;
}

int64_t DaalaBitReader::ReadLiteral(int num_bits) {
  assert(num_bits <= 32);
  assert(num_bits > 0);
  uint32_t literal = 0;
  int bit = num_bits - 1;
  do {
    // ARM can combine a shift operation with a constant number of bits with
    // some other operations, such as the OR operation.
    // Here is an ARM disassembly example:
    // orr w1, w0, w1, lsl #1
    // which left shifts register w1 by 1 bit and OR the shift result with
    // register w0.
    // The next 2 lines are equivalent to:
    // literal |= static_cast<uint32_t>(ReadBit()) << bit;
    literal <<= 1;
    literal |= static_cast<uint32_t>(ReadBit());
  } while (--bit >= 0);
  return literal;
}

int DaalaBitReader::ReadSymbol(uint16_t* const cdf, int symbol_count) {
  const int symbol = ReadSymbolImpl(cdf, symbol_count);
  if (allow_update_cdf_) {
    UpdateCdf(cdf, symbol_count, symbol);
  }
  return symbol;
}

bool DaalaBitReader::ReadSymbol(uint16_t* cdf) {
  const bool symbol = ReadSymbolImpl(cdf) != 0;
  if (allow_update_cdf_) {
    const uint16_t count = cdf[2];
    // rate is computed in the spec as:
    //  3 + ( cdf[N] > 15 ) + ( cdf[N] > 31 ) + Min(FloorLog2(N), 2)
    // In this case N is 2 and cdf[N] is |count|. So the equation becomes:
    //  4 + (count > 15) + (count > 31)
    // Note that the largest value for count is 32 (it is not incremented beyond
    // 32). So using that information:
    //  count >> 4 is 0 for count from 0 to 15.
    //  count >> 4 is 1 for count from 16 to 31.
    //  count >> 4 is 2 for count == 31.
    // Now, the equation becomes:
    //  4 + (count >> 4).
    // Since (count >> 4) can only be 0 or 1 or 2, the addition can be replaced
    // with bitwise or. So the final equation is:
    //  4 | (count >> 4).
    const int rate = 4 | (count >> 4);
    if (symbol) {
      cdf[0] += (kCdfMaxProbability - cdf[0]) >> rate;
    } else {
      cdf[0] -= cdf[0] >> rate;
    }
    cdf[2] += static_cast<uint16_t>(count < 32);
  }
  return symbol;
}

bool DaalaBitReader::ReadSymbolWithoutCdfUpdate(uint16_t* cdf) {
  return ReadSymbolImpl(cdf) != 0;
}

template <int symbol_count>
int DaalaBitReader::ReadSymbol(uint16_t* const cdf) {
  static_assert(symbol_count >= 3 && symbol_count <= 16, "");
  if (symbol_count == 4) {
    return ReadSymbol4(cdf);
  }
  int symbol;
  if (symbol_count == 8) {
    symbol = ReadSymbolImpl8(cdf);
  } else if (symbol_count <= 13) {
    symbol = ReadSymbolImpl(cdf, symbol_count);
  } else {
    symbol = ReadSymbolImplBinarySearch(cdf, symbol_count);
  }
  if (allow_update_cdf_) {
    if (symbol_count == 5) {
      UpdateCdf5(cdf, symbol);
    } else if (symbol_count == 7) {
      UpdateCdf7(cdf, symbol);
    } else if (symbol_count == 8) {
      UpdateCdf8(cdf, symbol);
    } else if (symbol_count == 11) {
      UpdateCdf11(cdf, symbol);
    } else if (symbol_count == 13) {
      UpdateCdf13(cdf, symbol);
    } else if (symbol_count == 16) {
      UpdateCdf16(cdf, symbol);
    } else {
      UpdateCdf(cdf, symbol_count, symbol);
    }
  }
  return symbol;
}

int DaalaBitReader::ReadSymbolImpl(const uint16_t* const cdf,
                                   int symbol_count) {
  assert(cdf[symbol_count - 1] == 0);
  --symbol_count;
  uint32_t curr = values_in_range_;
  int symbol = -1;
  uint32_t prev;
  const auto symbol_value =
      static_cast<uint32_t>(window_diff_ >> (kWindowSize - 16));
  uint32_t delta = kMinimumProbabilityPerSymbol * symbol_count;
  // Search through the |cdf| array to determine where the scaled cdf value and
  // |symbol_value| cross over.
  do {
    prev = curr;
    curr = (((values_in_range_ >> 8) * (cdf[++symbol] >> kCdfPrecision)) >> 1) +
           delta;
    delta -= kMinimumProbabilityPerSymbol;
  } while (symbol_value < curr);
  values_in_range_ = prev - curr;
  window_diff_ -= static_cast<WindowSize>(curr) << (kWindowSize - 16);
  NormalizeRange();
  return symbol;
}

int DaalaBitReader::ReadSymbolImplBinarySearch(const uint16_t* const cdf,
                                               int symbol_count) {
  assert(cdf[symbol_count - 1] == 0);
  assert(symbol_count > 1 && symbol_count <= 16);
  --symbol_count;
  const auto symbol_value =
      static_cast<uint32_t>(window_diff_ >> (kWindowSize - 16));
  // Search through the |cdf| array to determine where the scaled cdf value and
  // |symbol_value| cross over. Since the CDFs are sorted, we can use binary
  // search to do this. Let |symbol| be the index of the first |cdf| array
  // entry whose scaled cdf value is less than or equal to |symbol_value|. The
  // binary search maintains the invariant:
  //   low <= symbol <= high + 1
  // and terminates when low == high + 1.
  int low = 0;
  int high = symbol_count - 1;
  // The binary search maintains the invariants that |prev| is the scaled cdf
  // value for low - 1 and |curr| is the scaled cdf value for high + 1. (By
  // convention, the scaled cdf value for -1 is values_in_range_.) When the
  // binary search terminates, |prev| is the scaled cdf value for symbol - 1
  // and |curr| is the scaled cdf value for |symbol|.
  uint32_t prev = values_in_range_;
  uint32_t curr = 0;
  const uint32_t values_in_range_shifted = values_in_range_ >> 8;
  do {
    const int mid = DivideBy2(low + high);
    const uint32_t scaled_cdf =
        ScaleCdf(values_in_range_shifted, cdf, mid, symbol_count);
    if (symbol_value < scaled_cdf) {
      low = mid + 1;
      prev = scaled_cdf;
    } else {
      high = mid - 1;
      curr = scaled_cdf;
    }
  } while (low <= high);
  assert(low == high + 1);
  // At this point, |low| is the symbol that has been decoded.
  values_in_range_ = prev - curr;
  window_diff_ -= static_cast<WindowSize>(curr) << (kWindowSize - 16);
  NormalizeRange();
  return low;
}

int DaalaBitReader::ReadSymbolImpl(const uint16_t* const cdf) {
  assert(cdf[1] == 0);
  const auto symbol_value =
      static_cast<uint32_t>(window_diff_ >> (kWindowSize - 16));
  const uint32_t curr = ScaleCdf(values_in_range_ >> 8, cdf, 0, 1);
  const int symbol = static_cast<int>(symbol_value < curr);
  if (symbol == 1) {
    values_in_range_ = curr;
  } else {
    values_in_range_ -= curr;
    window_diff_ -= static_cast<WindowSize>(curr) << (kWindowSize - 16);
  }
  NormalizeRange();
  return symbol;
}

// Equivalent to ReadSymbol(cdf, 4), with the ReadSymbolImpl and UpdateCdf
// calls inlined.
int DaalaBitReader::ReadSymbol4(uint16_t* const cdf) {
  assert(cdf[3] == 0);
  uint32_t curr = values_in_range_;
  uint32_t prev;
  const auto symbol_value =
      static_cast<uint32_t>(window_diff_ >> (kWindowSize - 16));
  uint32_t delta = kMinimumProbabilityPerSymbol * 3;
  const uint32_t values_in_range_shifted = values_in_range_ >> 8;

  // Search through the |cdf| array to determine where the scaled cdf value and
  // |symbol_value| cross over. If allow_update_cdf_ is true, update the |cdf|
  // array.
  //
  // The original code is:
  //
  //  int symbol = -1;
  //  do {
  //    prev = curr;
  //    curr =
  //        ((values_in_range_shifted * (cdf[++symbol] >> kCdfPrecision)) >> 1)
  //        + delta;
  //    delta -= kMinimumProbabilityPerSymbol;
  //  } while (symbol_value < curr);
  //  if (allow_update_cdf_) {
  //    UpdateCdf(cdf, 4, symbol);
  //  }
  //
  // The do-while loop is unrolled with four iterations, and the UpdateCdf call
  // is inlined and merged into the four iterations.
  int symbol = 0;
  // Iteration 0.
  prev = curr;
  curr =
      ((values_in_range_shifted * (cdf[symbol] >> kCdfPrecision)) >> 1) + delta;
  if (symbol_value >= curr) {
    // symbol == 0.
    if (allow_update_cdf_) {
      // Inlined version of UpdateCdf(cdf, 4, /*symbol=*/0).
      const uint16_t count = cdf[4];
      cdf[4] += static_cast<uint16_t>(count < 32);
      const int rate = (4 | (count >> 4)) + 1;
#if LIBGAV1_ENTROPY_DECODER_ENABLE_NEON
      // 1. On Motorola Moto G5 Plus (running 32-bit Android 8.1.0), the ARM
      // NEON code is slower. Consider using the C version if __arm__ is
      // defined.
      // 2. The ARM NEON code (compiled for arm64) is slightly slower on
      // Samsung Galaxy S8+ (SM-G955FD).
      uint16x4_t cdf_vec = vld1_u16(cdf);
      const int16x4_t negative_rate = vdup_n_s16(-rate);
      const uint16x4_t delta = vshl_u16(cdf_vec, negative_rate);
      cdf_vec = vsub_u16(cdf_vec, delta);
      vst1_u16(cdf, cdf_vec);
#else
      cdf[0] -= cdf[0] >> rate;
      cdf[1] -= cdf[1] >> rate;
      cdf[2] -= cdf[2] >> rate;
#endif
    }
    goto found;
  }
  ++symbol;
  delta -= kMinimumProbabilityPerSymbol;
  // Iteration 1.
  prev = curr;
  curr =
      ((values_in_range_shifted * (cdf[symbol] >> kCdfPrecision)) >> 1) + delta;
  if (symbol_value >= curr) {
    // symbol == 1.
    if (allow_update_cdf_) {
      // Inlined version of UpdateCdf(cdf, 4, /*symbol=*/1).
      const uint16_t count = cdf[4];
      cdf[4] += static_cast<uint16_t>(count < 32);
      const int rate = (4 | (count >> 4)) + 1;
      cdf[0] += (kCdfMaxProbability - cdf[0]) >> rate;
      cdf[1] -= cdf[1] >> rate;
      cdf[2] -= cdf[2] >> rate;
    }
    goto found;
  }
  ++symbol;
  delta -= kMinimumProbabilityPerSymbol;
  // Iteration 2.
  prev = curr;
  curr =
      ((values_in_range_shifted * (cdf[symbol] >> kCdfPrecision)) >> 1) + delta;
  if (symbol_value >= curr) {
    // symbol == 2.
    if (allow_update_cdf_) {
      // Inlined version of UpdateCdf(cdf, 4, /*symbol=*/2).
      const uint16_t count = cdf[4];
      cdf[4] += static_cast<uint16_t>(count < 32);
      const int rate = (4 | (count >> 4)) + 1;
      cdf[0] += (kCdfMaxProbability - cdf[0]) >> rate;
      cdf[1] += (kCdfMaxProbability - cdf[1]) >> rate;
      cdf[2] -= cdf[2] >> rate;
    }
    goto found;
  }
  ++symbol;
  // |delta| is 0 for the last iteration.
  // Iteration 3.
  prev = curr;
  // Since cdf[3] is 0 and |delta| is 0, |curr| is also 0.
  curr = 0;
  // symbol == 3.
  if (allow_update_cdf_) {
    // Inlined version of UpdateCdf(cdf, 4, /*symbol=*/3).
    const uint16_t count = cdf[4];
    cdf[4] += static_cast<uint16_t>(count < 32);
    const int rate = (4 | (count >> 4)) + 1;
#if LIBGAV1_ENTROPY_DECODER_ENABLE_NEON
    // On Motorola Moto G5 Plus (running 32-bit Android 8.1.0), the ARM NEON
    // code is a tiny bit slower. Consider using the C version if __arm__ is
    // defined.
    uint16x4_t cdf_vec = vld1_u16(cdf);
    const uint16x4_t cdf_max_probability = vdup_n_u16(kCdfMaxProbability);
    const int16x4_t diff =
        vreinterpret_s16_u16(vsub_u16(cdf_max_probability, cdf_vec));
    const int16x4_t negative_rate = vdup_n_s16(-rate);
    const uint16x4_t delta =
        vreinterpret_u16_s16(vshl_s16(diff, negative_rate));
    cdf_vec = vadd_u16(cdf_vec, delta);
    vst1_u16(cdf, cdf_vec);
    cdf[3] = 0;
#else
    cdf[0] += (kCdfMaxProbability - cdf[0]) >> rate;
    cdf[1] += (kCdfMaxProbability - cdf[1]) >> rate;
    cdf[2] += (kCdfMaxProbability - cdf[2]) >> rate;
#endif
  }
found:
  // End of unrolled do-while loop.

  values_in_range_ = prev - curr;
  window_diff_ -= static_cast<WindowSize>(curr) << (kWindowSize - 16);
  NormalizeRange();
  return symbol;
}

int DaalaBitReader::ReadSymbolImpl8(const uint16_t* const cdf) {
  assert(cdf[7] == 0);
  uint32_t curr = values_in_range_;
  uint32_t prev;
  const auto symbol_value =
      static_cast<uint32_t>(window_diff_ >> (kWindowSize - 16));
  uint32_t delta = kMinimumProbabilityPerSymbol * 7;
  // Search through the |cdf| array to determine where the scaled cdf value and
  // |symbol_value| cross over.
  //
  // The original code is:
  //
  // int symbol = -1;
  // do {
  //   prev = curr;
  //   curr =
  //       (((values_in_range_ >> 8) * (cdf[++symbol] >> kCdfPrecision)) >> 1)
  //       + delta;
  //   delta -= kMinimumProbabilityPerSymbol;
  // } while (symbol_value < curr);
  //
  // The do-while loop is unrolled with eight iterations.
  int symbol = 0;

#define READ_SYMBOL_ITERATION                                                \
  prev = curr;                                                               \
  curr = (((values_in_range_ >> 8) * (cdf[symbol] >> kCdfPrecision)) >> 1) + \
         delta;                                                              \
  if (symbol_value >= curr) goto found;                                      \
  ++symbol;                                                                  \
  delta -= kMinimumProbabilityPerSymbol

  READ_SYMBOL_ITERATION;  // Iteration 0.
  READ_SYMBOL_ITERATION;  // Iteration 1.
  READ_SYMBOL_ITERATION;  // Iteration 2.
  READ_SYMBOL_ITERATION;  // Iteration 3.
  READ_SYMBOL_ITERATION;  // Iteration 4.
  READ_SYMBOL_ITERATION;  // Iteration 5.

  // The last two iterations can be simplified, so they don't use the
  // READ_SYMBOL_ITERATION macro.
#undef READ_SYMBOL_ITERATION

  // Iteration 6.
  prev = curr;
  curr =
      (((values_in_range_ >> 8) * (cdf[symbol] >> kCdfPrecision)) >> 1) + delta;
  if (symbol_value >= curr) goto found;  // symbol == 6.
  ++symbol;
  // |delta| is 0 for the last iteration.
  // Iteration 7.
  prev = curr;
  // Since cdf[7] is 0 and |delta| is 0, |curr| is also 0.
  curr = 0;
  // symbol == 7.
found:
  // End of unrolled do-while loop.

  values_in_range_ = prev - curr;
  window_diff_ -= static_cast<WindowSize>(curr) << (kWindowSize - 16);
  NormalizeRange();
  return symbol;
}

void DaalaBitReader::PopulateBits() {
#if defined(__aarch64__)
  // Fast path: read eight bytes and add the first six bytes to window_diff_.
  // This fast path makes the following assumptions.
  // 1. We assume that unaligned load of uint64_t is fast.
  // 2. When there are enough bytes in data_, the for loop below reads 6 or 7
  //    bytes depending on the value of bits_. This fast path always reads 6
  //    bytes, which results in more calls to PopulateBits(). We assume that
  //    making more calls to a faster PopulateBits() is overall a win.
  // NOTE: Although this fast path could also be used on x86_64, it hurts
  // performance (measured on Lenovo ThinkStation P920 running Linux). (The
  // reason is still unknown.) Therefore this fast path is only used on arm64.
  static_assert(kWindowSize == 64, "");
  if (size_ - data_index_ >= 8) {
    uint64_t value;
    // arm64 supports unaligned loads, so this memcpy call is compiled to a
    // single ldr instruction.
    memcpy(&value, &data_[data_index_], 8);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    value = __builtin_bswap64(value);
#endif
    value &= 0xffffffffffff0000;
    window_diff_ ^= static_cast<WindowSize>(value) >> (bits_ + 16);
    data_index_ += 6;
    bits_ += 6 * 8;
    return;
  }
#endif

  size_t data_index = data_index_;
  int bits = bits_;
  WindowSize window_diff = window_diff_;

  int shift = kWindowSize - 9 - (bits + 15);
  // The fast path above, if compiled, would cause clang 8.0.7 to vectorize
  // this loop. Since -15 <= bits_ <= -1, this loop has at most 6 or 7
  // iterations when WindowSize is 64 bits. So it is not profitable to
  // vectorize this loop. Note that clang 8.0.7 does not vectorize this loop if
  // the fast path above is not compiled.

#ifdef __clang__
#pragma clang loop vectorize(disable) interleave(disable)
#endif
  for (; shift >= 0 && data_index < size_; shift -= 8) {
    window_diff ^= static_cast<WindowSize>(data_[data_index++]) << shift;
    bits += 8;
  }
  if (data_index >= size_) {
    bits = kLargeBitCount;
  }

  data_index_ = data_index;
  bits_ = bits;
  window_diff_ = window_diff;
}

void DaalaBitReader::NormalizeRange() {
  const int bits_used = 15 - FloorLog2(values_in_range_);
  bits_ -= bits_used;
  window_diff_ = ((window_diff_ + 1) << bits_used) - 1;
  values_in_range_ <<= bits_used;
  if (bits_ < 0) PopulateBits();
}

// Explicit instantiations.
template int DaalaBitReader::ReadSymbol<3>(uint16_t* cdf);
template int DaalaBitReader::ReadSymbol<4>(uint16_t* cdf);
template int DaalaBitReader::ReadSymbol<5>(uint16_t* cdf);
template int DaalaBitReader::ReadSymbol<7>(uint16_t* cdf);
template int DaalaBitReader::ReadSymbol<8>(uint16_t* cdf);
template int DaalaBitReader::ReadSymbol<10>(uint16_t* cdf);
template int DaalaBitReader::ReadSymbol<11>(uint16_t* cdf);
template int DaalaBitReader::ReadSymbol<13>(uint16_t* cdf);
template int DaalaBitReader::ReadSymbol<14>(uint16_t* cdf);
template int DaalaBitReader::ReadSymbol<16>(uint16_t* cdf);

}  // namespace libgav1
