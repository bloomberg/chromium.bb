// Copyright 2020 The libgav1 Authors
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

#include "src/dsp/cdef.h"
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_SSE4_1

#include <emmintrin.h>
#include <tmmintrin.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "src/dsp/dsp.h"
#include "src/dsp/x86/common_sse4.h"
#include "src/dsp/x86/transpose_sse4.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

// CdefDirection:
// Mirror values and pad to 16 elements.
alignas(16) constexpr uint32_t kDivisionTable[] = {840, 420, 280, 210, 168, 140,
                                                   120, 105, 120, 140, 168, 210,
                                                   280, 420, 840, 0};

// Used when calculating odd |cost[x]| values to mask off unwanted elements.
// Holds elements 1 3 5 X 5 3 1 X
alignas(16) constexpr uint32_t kDivisionTableOdd[] = {420, 210, 140, 0,
                                                      140, 210, 420, 0};

// Used to calculate |partial[0][i + j]| and |partial[4][7 + i - j]|. The input
// is |src[j]| and it is being added to |partial[]| based on the above indices.
// |x| is assumed to be reversed when called for partial[4].
template <int shift>
inline void AddPartial0Or4(const __m128i x, __m128i* b, __m128i* c) {
  if (shift == 0) {
    *b = _mm_add_epi16(*b, x);
  } else {
    *b = _mm_add_epi16(*b, _mm_slli_si128(x, shift << 1));
    *c = _mm_add_epi16(*c, _mm_srli_si128(x, (8 - shift) << 1));
  }
}

// |[i + j / 2]| effectively adds two values to the same index:
// partial[1][0 + 0 / 2] += src[j]
// partial[1][0 + 1 / 2] += src[j + 1]
// Horizontal add |a| to generate 4 values. Shift as necessary to add these
// values to |b| and |c|.
//
// |x| is assumed to be reversed when called for partial[3].
// Used to calculate |partial[1][i + j / 2]| and |partial[3][3 + i - j / 2]|.
template <int shift>
inline void AddPartial1Or3(const __m128i x, __m128i* dest_lo,
                           __m128i* dest_hi) {
  const __m128i paired = _mm_hadd_epi16(x, _mm_setzero_si128());
  if (shift == 0) {
    *dest_lo = _mm_add_epi16(*dest_lo, paired);
  } else {
    *dest_lo = _mm_add_epi16(*dest_lo, _mm_slli_si128(paired, shift << 1));
    if (shift > 4) {
      // Split |paired| between |b| and |c|.
      *dest_hi =
          _mm_add_epi16(*dest_hi, _mm_srli_si128(paired, (8 - shift) << 1));
    }
  }
}

// Simple add starting at [3] and stepping back every other row.
// Used to calculate |partial[5][3 - i / 2 + j]|.
template <int shift>
inline void AddPartial5(const __m128i a, __m128i* b, __m128i* c) {
  if (shift > 5) {
    *b = _mm_add_epi16(*b, a);
  } else {
    *b = _mm_add_epi16(*b, _mm_slli_si128(a, (3 - shift / 2) << 1));
    *c = _mm_add_epi16(*c, _mm_srli_si128(a, (5 + shift / 2) << 1));
  }
}

// Simple add.
// Used to calculate |partial[6][j]|
inline void AddPartial6(const __m128i a, __m128i* b) {
  *b = _mm_add_epi16(*b, a);
}

// Simple add starting at [0] and stepping forward every other row.
// Used to calculate |partial[7][i / 2 + j]|.
template <int shift>
inline void AddPartial7(const __m128i a, __m128i* b, __m128i* c) {
  if (shift < 2) {
    *b = _mm_add_epi16(*b, a);
  } else {
    *b = _mm_add_epi16(*b, _mm_slli_si128(a, (shift / 2) << 1));
    *c = _mm_add_epi16(*c, _mm_srli_si128(a, (8 - shift / 2) << 1));
  }
}

// Reverse the lower 8 bytes in the register.
// 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 input
// 7 6 5 4 3 2 1 0 8 9 10 11 12 13 14 15 output
inline __m128i Reverse8(const __m128i v) {
  const __m128i reverser = _mm_set_epi32(0, 0, 0x00010203, 0x04050607);
  return _mm_shuffle_epi8(v, reverser);
}

// Section 7.15.2 line 10 of first code block.
template <int i>
inline void AddPartial(const __m128i source, __m128i dest_lo[8],
                       __m128i dest_hi[8]) {
  // This is not normalized for signed arithmetic like the other partials, and
  // must be corrected at the end.
  dest_lo[2] = _mm_add_epi16(
      dest_lo[2],
      _mm_slli_si128(_mm_sad_epu8(source, _mm_setzero_si128()), i << 1));
  // Note: though we could save one subtraction and one convert by reversing
  // after creating |source_s16|, it would mean loading another register for
  // the shuffle mask.
  const __m128i signed_offset = _mm_set1_epi16(128);
  const __m128i reverse_source_s16 =
      _mm_sub_epi16(_mm_cvtepu8_epi16(Reverse8(source)), signed_offset);
  AddPartial0Or4<i>(reverse_source_s16, &dest_lo[4], &dest_hi[4]);
  AddPartial1Or3<i>(reverse_source_s16, &dest_lo[3], &dest_hi[3]);
  const __m128i source_s16 =
      _mm_sub_epi16(_mm_cvtepu8_epi16(source), signed_offset);
  AddPartial0Or4<i>(source_s16, &dest_lo[0], &dest_hi[0]);
  AddPartial1Or3<i>(source_s16, &dest_lo[1], &dest_hi[1]);
  AddPartial5<i>(source_s16, &dest_lo[5], &dest_hi[5]);
  AddPartial6(source_s16, &dest_lo[6]);
  AddPartial7<i>(source_s16, &dest_lo[7], &dest_hi[7]);
}

inline uint32_t SumVector_S32(__m128i a) {
  a = _mm_hadd_epi32(a, a);
  a = _mm_add_epi32(a, _mm_srli_si128(a, 4));
  return _mm_cvtsi128_si32(a);
}

inline __m128i Square_S32(__m128i a) { return _mm_mullo_epi32(a, a); }

// |cost[0]| and |cost[4]| square the input and sum with the corresponding
// element from the other end of the vector:
// |kDivisionTable[]| element:
// cost[0] += (Square(partial[0][i]) + Square(partial[0][14 - i])) *
//             kDivisionTable[i + 1];
// cost[0] += Square(partial[0][7]) * kDivisionTable[8];
// Because everything is being summed into a single value the distributive
// property allows us to mirror the division table and accumulate once.
inline uint32_t Cost0Or4(const __m128i a, const __m128i b,
                         const __m128i division_table[4]) {
  const __m128i a_lo = _mm_cvtepi16_epi32(a);
  const __m128i a_hi = _mm_cvtepi16_epi32(_mm_srli_si128(a, 8));
  const __m128i b_lo = _mm_cvtepi16_epi32(b);
  const __m128i b_hi = _mm_cvtepi16_epi32(_mm_srli_si128(b, 8));
  __m128i c = _mm_mullo_epi32(Square_S32(a_lo), division_table[0]);
  c = _mm_add_epi32(c, _mm_mullo_epi32(Square_S32(a_hi), division_table[1]));
  c = _mm_add_epi32(c, _mm_mullo_epi32(Square_S32(b_lo), division_table[2]));
  c = _mm_add_epi32(c, _mm_mullo_epi32(Square_S32(b_hi), division_table[3]));
  return SumVector_S32(c);
}

inline uint32_t CostOdd(const __m128i a, const __m128i b,
                        const __m128i division_table) {
  const __m128i a_lo_square = Square_S32(_mm_cvtepi16_epi32(a));
  const __m128i a_hi_square =
      Square_S32(_mm_cvtepi16_epi32(_mm_srli_si128(a, 8)));
  // Swap element 0 and element 2. This pairs partial[i][10 - j] with
  // kDivisionTable[2*j+1].
  const __m128i b_lo_square =
      _mm_shuffle_epi32(Square_S32(_mm_cvtepi16_epi32(b)), 0x06);
  // First terms are indices 3-7.
  __m128i c = _mm_srli_si128(a_lo_square, 12);
  c = _mm_add_epi32(c, a_hi_square);
  c = _mm_mullo_epi32(c, _mm_set1_epi32(kDivisionTable[7]));

  // cost[i] += (Square(base_partial[i][j]) + Square(base_partial[i][10 - j])) *
  //          kDivisionTable[2 * j + 1];
  const __m128i second_cost = _mm_add_epi32(a_lo_square, b_lo_square);
  c = _mm_add_epi32(c, _mm_mullo_epi32(second_cost, division_table));
  return SumVector_S32(c);
}

// Sum of squared elements.
inline uint32_t SquareSum_S16(const __m128i a) {
  const __m128i square_lo = Square_S32(_mm_cvtepi16_epi32(a));
  const __m128i square_hi =
      Square_S32(_mm_cvtepi16_epi32(_mm_srli_si128(a, 8)));
  const __m128i c = _mm_add_epi32(square_hi, square_lo);
  return SumVector_S32(c);
}

void CdefDirection_SSE4_1(const void* const source, ptrdiff_t stride,
                          int* const direction, int* const variance) {
  assert(direction != nullptr);
  assert(variance != nullptr);
  const auto* src = static_cast<const uint8_t*>(source);
  uint32_t cost[8];
  __m128i partial_lo[8], partial_hi[8];

  const __m128i zero = _mm_setzero_si128();
  for (int i = 0; i < 8; ++i) {
    partial_lo[i] = partial_hi[i] = zero;
  }

  AddPartial<0>(LoadLo8(src), partial_lo, partial_hi);
  src += stride;
  AddPartial<1>(LoadLo8(src), partial_lo, partial_hi);
  src += stride;
  AddPartial<2>(LoadLo8(src), partial_lo, partial_hi);
  src += stride;
  AddPartial<3>(LoadLo8(src), partial_lo, partial_hi);
  src += stride;
  AddPartial<4>(LoadLo8(src), partial_lo, partial_hi);
  src += stride;
  AddPartial<5>(LoadLo8(src), partial_lo, partial_hi);
  src += stride;
  AddPartial<6>(LoadLo8(src), partial_lo, partial_hi);
  src += stride;
  AddPartial<7>(LoadLo8(src), partial_lo, partial_hi);

  const __m128i signed_offset = _mm_set1_epi16(128 * 8);
  partial_lo[2] = _mm_sub_epi16(partial_lo[2], signed_offset);

  cost[2] = kDivisionTable[7] * SquareSum_S16(partial_lo[2]);
  cost[6] = kDivisionTable[7] * SquareSum_S16(partial_lo[6]);

  const __m128i division_table[4] = {LoadUnaligned16(kDivisionTable),
                                     LoadUnaligned16(kDivisionTable + 4),
                                     LoadUnaligned16(kDivisionTable + 8),
                                     LoadUnaligned16(kDivisionTable + 12)};

  cost[0] = Cost0Or4(partial_lo[0], partial_hi[0], division_table);
  cost[4] = Cost0Or4(partial_lo[4], partial_hi[4], division_table);

  const __m128i division_table_odd = LoadAligned16(kDivisionTableOdd);

  cost[1] = CostOdd(partial_lo[1], partial_hi[1], division_table_odd);
  cost[3] = CostOdd(partial_lo[3], partial_hi[3], division_table_odd);
  cost[5] = CostOdd(partial_lo[5], partial_hi[5], division_table_odd);
  cost[7] = CostOdd(partial_lo[7], partial_hi[7], division_table_odd);

  uint32_t best_cost = 0;
  *direction = 0;
  for (int i = 0; i < 8; ++i) {
    if (cost[i] > best_cost) {
      best_cost = cost[i];
      *direction = i;
    }
  }
  *variance = (best_cost - cost[(*direction + 4) & 7]) >> 10;
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
  dsp->cdef_direction = CdefDirection_SSE4_1;
}

}  // namespace
}  // namespace low_bitdepth

void CdefInit_SSE4_1() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1
#else   // !LIBGAV1_ENABLE_SSE4_1
namespace libgav1 {
namespace dsp {

void CdefInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
