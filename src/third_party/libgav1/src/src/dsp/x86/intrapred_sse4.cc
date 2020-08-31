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

#include "src/dsp/intrapred.h"
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_SSE4_1

#include <xmmintrin.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>  // memcpy

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/dsp/x86/common_sse4.h"
#include "src/dsp/x86/transpose_sse4.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace {

//------------------------------------------------------------------------------
// Utility Functions

// This is a fast way to divide by a number of the form 2^n + 2^k, n > k.
// Divide by 2^k by right shifting by k, leaving the denominator 2^m + 1. In the
// block size cases, n - k is 1 or 2 (block is proportional to 1x2 or 1x4), so
// we use a multiplier that reflects division by 2+1=3 or 4+1=5 in the high
// bits.
constexpr int kThreeInverse = 0x5556;
constexpr int kFiveInverse = 0x3334;
template <int shiftk, int multiplier>
inline __m128i DivideByMultiplyShift_U32(const __m128i dividend) {
  const __m128i interm = _mm_srli_epi32(dividend, shiftk);
  return _mm_mulhi_epi16(interm, _mm_cvtsi32_si128(multiplier));
}

// This shuffle mask selects 32-bit blocks in the order 0, 1, 0, 1, which
// duplicates the first 8 bytes of a 128-bit vector into the second 8 bytes.
constexpr int kDuplicateFirstHalf = 0x44;

//------------------------------------------------------------------------------
// DcPredFuncs_SSE4_1

using DcSumFunc = __m128i (*)(const void* ref);
using DcStoreFunc = void (*)(void* dest, ptrdiff_t stride, const __m128i dc);
using WriteDuplicateFunc = void (*)(void* dest, ptrdiff_t stride,
                                    const __m128i column);
// For copying an entire column across a block.
using ColumnStoreFunc = void (*)(void* dest, ptrdiff_t stride,
                                 const void* column);

// DC intra-predictors for non-square blocks.
template <int width_log2, int height_log2, DcSumFunc top_sumfn,
          DcSumFunc left_sumfn, DcStoreFunc storefn, int shiftk, int dc_mult>
struct DcPredFuncs_SSE4_1 {
  DcPredFuncs_SSE4_1() = delete;

  static void DcTop(void* dest, ptrdiff_t stride, const void* top_row,
                    const void* left_column);
  static void DcLeft(void* dest, ptrdiff_t stride, const void* top_row,
                     const void* left_column);
  static void Dc(void* dest, ptrdiff_t stride, const void* top_row,
                 const void* left_column);
};

// Directional intra-predictors for square blocks.
template <ColumnStoreFunc col_storefn>
struct DirectionalPredFuncs_SSE4_1 {
  DirectionalPredFuncs_SSE4_1() = delete;

  static void Vertical(void* dest, ptrdiff_t stride, const void* top_row,
                       const void* left_column);
  static void Horizontal(void* dest, ptrdiff_t stride, const void* top_row,
                         const void* left_column);
};

template <int width_log2, int height_log2, DcSumFunc top_sumfn,
          DcSumFunc left_sumfn, DcStoreFunc storefn, int shiftk, int dc_mult>
void DcPredFuncs_SSE4_1<width_log2, height_log2, top_sumfn, left_sumfn, storefn,
                        shiftk, dc_mult>::DcTop(void* const dest,
                                                ptrdiff_t stride,
                                                const void* const top_row,
                                                const void* /*left_column*/) {
  const __m128i rounder = _mm_set1_epi32(1 << (width_log2 - 1));
  const __m128i sum = top_sumfn(top_row);
  const __m128i dc = _mm_srli_epi32(_mm_add_epi32(sum, rounder), width_log2);
  storefn(dest, stride, dc);
}

template <int width_log2, int height_log2, DcSumFunc top_sumfn,
          DcSumFunc left_sumfn, DcStoreFunc storefn, int shiftk, int dc_mult>
void DcPredFuncs_SSE4_1<width_log2, height_log2, top_sumfn, left_sumfn, storefn,
                        shiftk,
                        dc_mult>::DcLeft(void* const dest, ptrdiff_t stride,
                                         const void* /*top_row*/,
                                         const void* const left_column) {
  const __m128i rounder = _mm_set1_epi32(1 << (height_log2 - 1));
  const __m128i sum = left_sumfn(left_column);
  const __m128i dc = _mm_srli_epi32(_mm_add_epi32(sum, rounder), height_log2);
  storefn(dest, stride, dc);
}

template <int width_log2, int height_log2, DcSumFunc top_sumfn,
          DcSumFunc left_sumfn, DcStoreFunc storefn, int shiftk, int dc_mult>
void DcPredFuncs_SSE4_1<width_log2, height_log2, top_sumfn, left_sumfn, storefn,
                        shiftk, dc_mult>::Dc(void* const dest, ptrdiff_t stride,
                                             const void* const top_row,
                                             const void* const left_column) {
  const __m128i rounder =
      _mm_set1_epi32((1 << (width_log2 - 1)) + (1 << (height_log2 - 1)));
  const __m128i sum_top = top_sumfn(top_row);
  const __m128i sum_left = left_sumfn(left_column);
  const __m128i sum = _mm_add_epi32(sum_top, sum_left);
  if (width_log2 == height_log2) {
    const __m128i dc =
        _mm_srli_epi32(_mm_add_epi32(sum, rounder), width_log2 + 1);
    storefn(dest, stride, dc);
  } else {
    const __m128i dc =
        DivideByMultiplyShift_U32<shiftk, dc_mult>(_mm_add_epi32(sum, rounder));
    storefn(dest, stride, dc);
  }
}

//------------------------------------------------------------------------------
// DcPredFuncs_SSE4_1 directional predictors

template <ColumnStoreFunc col_storefn>
void DirectionalPredFuncs_SSE4_1<col_storefn>::Horizontal(
    void* const dest, ptrdiff_t stride, const void* /*top_row*/,
    const void* const left_column) {
  col_storefn(dest, stride, left_column);
}

}  // namespace

//------------------------------------------------------------------------------
namespace low_bitdepth {
namespace {

// |ref| points to 4 bytes containing 4 packed ints.
inline __m128i DcSum4_SSE4_1(const void* const ref) {
  const __m128i vals = Load4(ref);
  const __m128i zero = _mm_setzero_si128();
  return _mm_sad_epu8(vals, zero);
}

inline __m128i DcSum8_SSE4_1(const void* const ref) {
  const __m128i vals = LoadLo8(ref);
  const __m128i zero = _mm_setzero_si128();
  return _mm_sad_epu8(vals, zero);
}

inline __m128i DcSum16_SSE4_1(const void* const ref) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i vals = LoadUnaligned16(ref);
  const __m128i partial_sum = _mm_sad_epu8(vals, zero);
  return _mm_add_epi16(partial_sum, _mm_srli_si128(partial_sum, 8));
}

inline __m128i DcSum32_SSE4_1(const void* const ref) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i vals1 = LoadUnaligned16(ref);
  const __m128i vals2 = LoadUnaligned16(static_cast<const uint8_t*>(ref) + 16);
  const __m128i partial_sum1 = _mm_sad_epu8(vals1, zero);
  const __m128i partial_sum2 = _mm_sad_epu8(vals2, zero);
  const __m128i partial_sum = _mm_add_epi16(partial_sum1, partial_sum2);
  return _mm_add_epi16(partial_sum, _mm_srli_si128(partial_sum, 8));
}

inline __m128i DcSum64_SSE4_1(const void* const ref) {
  const auto* const ref_ptr = static_cast<const uint8_t*>(ref);
  const __m128i zero = _mm_setzero_si128();
  const __m128i vals1 = LoadUnaligned16(ref_ptr);
  const __m128i vals2 = LoadUnaligned16(ref_ptr + 16);
  const __m128i vals3 = LoadUnaligned16(ref_ptr + 32);
  const __m128i vals4 = LoadUnaligned16(ref_ptr + 48);
  const __m128i partial_sum1 = _mm_sad_epu8(vals1, zero);
  const __m128i partial_sum2 = _mm_sad_epu8(vals2, zero);
  __m128i partial_sum = _mm_add_epi16(partial_sum1, partial_sum2);
  const __m128i partial_sum3 = _mm_sad_epu8(vals3, zero);
  partial_sum = _mm_add_epi16(partial_sum, partial_sum3);
  const __m128i partial_sum4 = _mm_sad_epu8(vals4, zero);
  partial_sum = _mm_add_epi16(partial_sum, partial_sum4);
  return _mm_add_epi16(partial_sum, _mm_srli_si128(partial_sum, 8));
}

template <int height>
inline void DcStore4xH_SSE4_1(void* const dest, ptrdiff_t stride,
                              const __m128i dc) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i dc_dup = _mm_shuffle_epi8(dc, zero);
  int y = height - 1;
  auto* dst = static_cast<uint8_t*>(dest);
  do {
    Store4(dst, dc_dup);
    dst += stride;
  } while (--y != 0);
  Store4(dst, dc_dup);
}

template <int height>
inline void DcStore8xH_SSE4_1(void* const dest, ptrdiff_t stride,
                              const __m128i dc) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i dc_dup = _mm_shuffle_epi8(dc, zero);
  int y = height - 1;
  auto* dst = static_cast<uint8_t*>(dest);
  do {
    StoreLo8(dst, dc_dup);
    dst += stride;
  } while (--y != 0);
  StoreLo8(dst, dc_dup);
}

template <int height>
inline void DcStore16xH_SSE4_1(void* const dest, ptrdiff_t stride,
                               const __m128i dc) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i dc_dup = _mm_shuffle_epi8(dc, zero);
  int y = height - 1;
  auto* dst = static_cast<uint8_t*>(dest);
  do {
    StoreUnaligned16(dst, dc_dup);
    dst += stride;
  } while (--y != 0);
  StoreUnaligned16(dst, dc_dup);
}

template <int height>
inline void DcStore32xH_SSE4_1(void* const dest, ptrdiff_t stride,
                               const __m128i dc) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i dc_dup = _mm_shuffle_epi8(dc, zero);
  int y = height - 1;
  auto* dst = static_cast<uint8_t*>(dest);
  do {
    StoreUnaligned16(dst, dc_dup);
    StoreUnaligned16(dst + 16, dc_dup);
    dst += stride;
  } while (--y != 0);
  StoreUnaligned16(dst, dc_dup);
  StoreUnaligned16(dst + 16, dc_dup);
}

template <int height>
inline void DcStore64xH_SSE4_1(void* const dest, ptrdiff_t stride,
                               const __m128i dc) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i dc_dup = _mm_shuffle_epi8(dc, zero);
  int y = height - 1;
  auto* dst = static_cast<uint8_t*>(dest);
  do {
    StoreUnaligned16(dst, dc_dup);
    StoreUnaligned16(dst + 16, dc_dup);
    StoreUnaligned16(dst + 32, dc_dup);
    StoreUnaligned16(dst + 48, dc_dup);
    dst += stride;
  } while (--y != 0);
  StoreUnaligned16(dst, dc_dup);
  StoreUnaligned16(dst + 16, dc_dup);
  StoreUnaligned16(dst + 32, dc_dup);
  StoreUnaligned16(dst + 48, dc_dup);
}

// WriteDuplicateN assumes dup has 4 sets of 4 identical bytes that are meant to
// be copied for width N into dest.
inline void WriteDuplicate4x4(void* const dest, ptrdiff_t stride,
                              const __m128i dup32) {
  auto* dst = static_cast<uint8_t*>(dest);
  Store4(dst, dup32);
  dst += stride;
  const int row1 = _mm_extract_epi32(dup32, 1);
  memcpy(dst, &row1, 4);
  dst += stride;
  const int row2 = _mm_extract_epi32(dup32, 2);
  memcpy(dst, &row2, 4);
  dst += stride;
  const int row3 = _mm_extract_epi32(dup32, 3);
  memcpy(dst, &row3, 4);
}

inline void WriteDuplicate8x4(void* const dest, ptrdiff_t stride,
                              const __m128i dup32) {
  const __m128i dup64_lo = _mm_unpacklo_epi32(dup32, dup32);
  const __m128i dup64_hi = _mm_unpackhi_epi32(dup32, dup32);
  auto* dst = static_cast<uint8_t*>(dest);
  _mm_storel_epi64(reinterpret_cast<__m128i*>(dst), dup64_lo);
  dst += stride;
  _mm_storeh_pi(reinterpret_cast<__m64*>(dst), _mm_castsi128_ps(dup64_lo));
  dst += stride;
  _mm_storel_epi64(reinterpret_cast<__m128i*>(dst), dup64_hi);
  dst += stride;
  _mm_storeh_pi(reinterpret_cast<__m64*>(dst), _mm_castsi128_ps(dup64_hi));
}

inline void WriteDuplicate16x4(void* const dest, ptrdiff_t stride,
                               const __m128i dup32) {
  const __m128i dup64_lo = _mm_unpacklo_epi32(dup32, dup32);
  const __m128i dup64_hi = _mm_unpackhi_epi32(dup32, dup32);

  auto* dst = static_cast<uint8_t*>(dest);
  const __m128i dup128_0 = _mm_unpacklo_epi64(dup64_lo, dup64_lo);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_0);
  dst += stride;
  const __m128i dup128_1 = _mm_unpackhi_epi64(dup64_lo, dup64_lo);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_1);
  dst += stride;
  const __m128i dup128_2 = _mm_unpacklo_epi64(dup64_hi, dup64_hi);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_2);
  dst += stride;
  const __m128i dup128_3 = _mm_unpackhi_epi64(dup64_hi, dup64_hi);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_3);
}

inline void WriteDuplicate32x4(void* const dest, ptrdiff_t stride,
                               const __m128i dup32) {
  const __m128i dup64_lo = _mm_unpacklo_epi32(dup32, dup32);
  const __m128i dup64_hi = _mm_unpackhi_epi32(dup32, dup32);

  auto* dst = static_cast<uint8_t*>(dest);
  const __m128i dup128_0 = _mm_unpacklo_epi64(dup64_lo, dup64_lo);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_0);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_0);
  dst += stride;
  const __m128i dup128_1 = _mm_unpackhi_epi64(dup64_lo, dup64_lo);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_1);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_1);
  dst += stride;
  const __m128i dup128_2 = _mm_unpacklo_epi64(dup64_hi, dup64_hi);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_2);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_2);
  dst += stride;
  const __m128i dup128_3 = _mm_unpackhi_epi64(dup64_hi, dup64_hi);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_3);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_3);
}

inline void WriteDuplicate64x4(void* const dest, ptrdiff_t stride,
                               const __m128i dup32) {
  const __m128i dup64_lo = _mm_unpacklo_epi32(dup32, dup32);
  const __m128i dup64_hi = _mm_unpackhi_epi32(dup32, dup32);

  auto* dst = static_cast<uint8_t*>(dest);
  const __m128i dup128_0 = _mm_unpacklo_epi64(dup64_lo, dup64_lo);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_0);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_0);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), dup128_0);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), dup128_0);
  dst += stride;
  const __m128i dup128_1 = _mm_unpackhi_epi64(dup64_lo, dup64_lo);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_1);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_1);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), dup128_1);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), dup128_1);
  dst += stride;
  const __m128i dup128_2 = _mm_unpacklo_epi64(dup64_hi, dup64_hi);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_2);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_2);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), dup128_2);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), dup128_2);
  dst += stride;
  const __m128i dup128_3 = _mm_unpackhi_epi64(dup64_hi, dup64_hi);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_3);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_3);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), dup128_3);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), dup128_3);
}

// ColStoreN<height> copies each of the |height| values in |column| across its
// corresponding in dest.
template <WriteDuplicateFunc writefn>
inline void ColStore4_SSE4_1(void* const dest, ptrdiff_t stride,
                             const void* const column) {
  const __m128i col_data = Load4(column);
  const __m128i col_dup16 = _mm_unpacklo_epi8(col_data, col_data);
  const __m128i col_dup32 = _mm_unpacklo_epi16(col_dup16, col_dup16);
  writefn(dest, stride, col_dup32);
}

template <WriteDuplicateFunc writefn>
inline void ColStore8_SSE4_1(void* const dest, ptrdiff_t stride,
                             const void* const column) {
  const ptrdiff_t stride4 = stride << 2;
  const __m128i col_data = LoadLo8(column);
  const __m128i col_dup16 = _mm_unpacklo_epi8(col_data, col_data);
  const __m128i col_dup32_lo = _mm_unpacklo_epi16(col_dup16, col_dup16);
  auto* dst = static_cast<uint8_t*>(dest);
  writefn(dst, stride, col_dup32_lo);
  dst += stride4;
  const __m128i col_dup32_hi = _mm_unpackhi_epi16(col_dup16, col_dup16);
  writefn(dst, stride, col_dup32_hi);
}

template <WriteDuplicateFunc writefn>
inline void ColStore16_SSE4_1(void* const dest, ptrdiff_t stride,
                              const void* const column) {
  const ptrdiff_t stride4 = stride << 2;
  const __m128i col_data = _mm_loadu_si128(static_cast<const __m128i*>(column));
  const __m128i col_dup16_lo = _mm_unpacklo_epi8(col_data, col_data);
  const __m128i col_dup16_hi = _mm_unpackhi_epi8(col_data, col_data);
  const __m128i col_dup32_lolo = _mm_unpacklo_epi16(col_dup16_lo, col_dup16_lo);
  auto* dst = static_cast<uint8_t*>(dest);
  writefn(dst, stride, col_dup32_lolo);
  dst += stride4;
  const __m128i col_dup32_lohi = _mm_unpackhi_epi16(col_dup16_lo, col_dup16_lo);
  writefn(dst, stride, col_dup32_lohi);
  dst += stride4;
  const __m128i col_dup32_hilo = _mm_unpacklo_epi16(col_dup16_hi, col_dup16_hi);
  writefn(dst, stride, col_dup32_hilo);
  dst += stride4;
  const __m128i col_dup32_hihi = _mm_unpackhi_epi16(col_dup16_hi, col_dup16_hi);
  writefn(dst, stride, col_dup32_hihi);
}

template <WriteDuplicateFunc writefn>
inline void ColStore32_SSE4_1(void* const dest, ptrdiff_t stride,
                              const void* const column) {
  const ptrdiff_t stride4 = stride << 2;
  auto* dst = static_cast<uint8_t*>(dest);
  for (int y = 0; y < 32; y += 16) {
    const __m128i col_data =
        LoadUnaligned16(static_cast<const uint8_t*>(column) + y);
    const __m128i col_dup16_lo = _mm_unpacklo_epi8(col_data, col_data);
    const __m128i col_dup16_hi = _mm_unpackhi_epi8(col_data, col_data);
    const __m128i col_dup32_lolo =
        _mm_unpacklo_epi16(col_dup16_lo, col_dup16_lo);
    writefn(dst, stride, col_dup32_lolo);
    dst += stride4;
    const __m128i col_dup32_lohi =
        _mm_unpackhi_epi16(col_dup16_lo, col_dup16_lo);
    writefn(dst, stride, col_dup32_lohi);
    dst += stride4;
    const __m128i col_dup32_hilo =
        _mm_unpacklo_epi16(col_dup16_hi, col_dup16_hi);
    writefn(dst, stride, col_dup32_hilo);
    dst += stride4;
    const __m128i col_dup32_hihi =
        _mm_unpackhi_epi16(col_dup16_hi, col_dup16_hi);
    writefn(dst, stride, col_dup32_hihi);
    dst += stride4;
  }
}

template <WriteDuplicateFunc writefn>
inline void ColStore64_SSE4_1(void* const dest, ptrdiff_t stride,
                              const void* const column) {
  const ptrdiff_t stride4 = stride << 2;
  auto* dst = static_cast<uint8_t*>(dest);
  for (int y = 0; y < 64; y += 16) {
    const __m128i col_data =
        LoadUnaligned16(static_cast<const uint8_t*>(column) + y);
    const __m128i col_dup16_lo = _mm_unpacklo_epi8(col_data, col_data);
    const __m128i col_dup16_hi = _mm_unpackhi_epi8(col_data, col_data);
    const __m128i col_dup32_lolo =
        _mm_unpacklo_epi16(col_dup16_lo, col_dup16_lo);
    writefn(dst, stride, col_dup32_lolo);
    dst += stride4;
    const __m128i col_dup32_lohi =
        _mm_unpackhi_epi16(col_dup16_lo, col_dup16_lo);
    writefn(dst, stride, col_dup32_lohi);
    dst += stride4;
    const __m128i col_dup32_hilo =
        _mm_unpacklo_epi16(col_dup16_hi, col_dup16_hi);
    writefn(dst, stride, col_dup32_hilo);
    dst += stride4;
    const __m128i col_dup32_hihi =
        _mm_unpackhi_epi16(col_dup16_hi, col_dup16_hi);
    writefn(dst, stride, col_dup32_hihi);
    dst += stride4;
  }
}

struct DcDefs {
  DcDefs() = delete;

  using _4x4 = DcPredFuncs_SSE4_1<2, 2, DcSum4_SSE4_1, DcSum4_SSE4_1,
                                  DcStore4xH_SSE4_1<4>, 0, 0>;
  // shiftk is the smaller of width_log2 and height_log2.
  // dc_mult corresponds to the ratio of the smaller block size to the larger.
  using _4x8 = DcPredFuncs_SSE4_1<2, 3, DcSum4_SSE4_1, DcSum8_SSE4_1,
                                  DcStore4xH_SSE4_1<8>, 2, kThreeInverse>;
  using _4x16 = DcPredFuncs_SSE4_1<2, 4, DcSum4_SSE4_1, DcSum16_SSE4_1,
                                   DcStore4xH_SSE4_1<16>, 2, kFiveInverse>;

  using _8x4 = DcPredFuncs_SSE4_1<3, 2, DcSum8_SSE4_1, DcSum4_SSE4_1,
                                  DcStore8xH_SSE4_1<4>, 2, kThreeInverse>;
  using _8x8 = DcPredFuncs_SSE4_1<3, 3, DcSum8_SSE4_1, DcSum8_SSE4_1,
                                  DcStore8xH_SSE4_1<8>, 0, 0>;
  using _8x16 = DcPredFuncs_SSE4_1<3, 4, DcSum8_SSE4_1, DcSum16_SSE4_1,
                                   DcStore8xH_SSE4_1<16>, 3, kThreeInverse>;
  using _8x32 = DcPredFuncs_SSE4_1<3, 5, DcSum8_SSE4_1, DcSum32_SSE4_1,
                                   DcStore8xH_SSE4_1<32>, 3, kFiveInverse>;

  using _16x4 = DcPredFuncs_SSE4_1<4, 2, DcSum16_SSE4_1, DcSum4_SSE4_1,
                                   DcStore16xH_SSE4_1<4>, 2, kFiveInverse>;
  using _16x8 = DcPredFuncs_SSE4_1<4, 3, DcSum16_SSE4_1, DcSum8_SSE4_1,
                                   DcStore16xH_SSE4_1<8>, 3, kThreeInverse>;
  using _16x16 = DcPredFuncs_SSE4_1<4, 4, DcSum16_SSE4_1, DcSum16_SSE4_1,
                                    DcStore16xH_SSE4_1<16>, 0, 0>;
  using _16x32 = DcPredFuncs_SSE4_1<4, 5, DcSum16_SSE4_1, DcSum32_SSE4_1,
                                    DcStore16xH_SSE4_1<32>, 4, kThreeInverse>;
  using _16x64 = DcPredFuncs_SSE4_1<4, 6, DcSum16_SSE4_1, DcSum64_SSE4_1,
                                    DcStore16xH_SSE4_1<64>, 4, kFiveInverse>;

  using _32x8 = DcPredFuncs_SSE4_1<5, 3, DcSum32_SSE4_1, DcSum8_SSE4_1,
                                   DcStore32xH_SSE4_1<8>, 3, kFiveInverse>;
  using _32x16 = DcPredFuncs_SSE4_1<5, 4, DcSum32_SSE4_1, DcSum16_SSE4_1,
                                    DcStore32xH_SSE4_1<16>, 4, kThreeInverse>;
  using _32x32 = DcPredFuncs_SSE4_1<5, 5, DcSum32_SSE4_1, DcSum32_SSE4_1,
                                    DcStore32xH_SSE4_1<32>, 0, 0>;
  using _32x64 = DcPredFuncs_SSE4_1<5, 6, DcSum32_SSE4_1, DcSum64_SSE4_1,
                                    DcStore32xH_SSE4_1<64>, 5, kThreeInverse>;

  using _64x16 = DcPredFuncs_SSE4_1<6, 4, DcSum64_SSE4_1, DcSum16_SSE4_1,
                                    DcStore64xH_SSE4_1<16>, 4, kFiveInverse>;
  using _64x32 = DcPredFuncs_SSE4_1<6, 5, DcSum64_SSE4_1, DcSum32_SSE4_1,
                                    DcStore64xH_SSE4_1<32>, 5, kThreeInverse>;
  using _64x64 = DcPredFuncs_SSE4_1<6, 6, DcSum64_SSE4_1, DcSum64_SSE4_1,
                                    DcStore64xH_SSE4_1<64>, 0, 0>;
};

struct DirDefs {
  DirDefs() = delete;

  using _4x4 = DirectionalPredFuncs_SSE4_1<ColStore4_SSE4_1<WriteDuplicate4x4>>;
  using _4x8 = DirectionalPredFuncs_SSE4_1<ColStore8_SSE4_1<WriteDuplicate4x4>>;
  using _4x16 =
      DirectionalPredFuncs_SSE4_1<ColStore16_SSE4_1<WriteDuplicate4x4>>;
  using _8x4 = DirectionalPredFuncs_SSE4_1<ColStore4_SSE4_1<WriteDuplicate8x4>>;
  using _8x8 = DirectionalPredFuncs_SSE4_1<ColStore8_SSE4_1<WriteDuplicate8x4>>;
  using _8x16 =
      DirectionalPredFuncs_SSE4_1<ColStore16_SSE4_1<WriteDuplicate8x4>>;
  using _8x32 =
      DirectionalPredFuncs_SSE4_1<ColStore32_SSE4_1<WriteDuplicate8x4>>;
  using _16x4 =
      DirectionalPredFuncs_SSE4_1<ColStore4_SSE4_1<WriteDuplicate16x4>>;
  using _16x8 =
      DirectionalPredFuncs_SSE4_1<ColStore8_SSE4_1<WriteDuplicate16x4>>;
  using _16x16 =
      DirectionalPredFuncs_SSE4_1<ColStore16_SSE4_1<WriteDuplicate16x4>>;
  using _16x32 =
      DirectionalPredFuncs_SSE4_1<ColStore32_SSE4_1<WriteDuplicate16x4>>;
  using _16x64 =
      DirectionalPredFuncs_SSE4_1<ColStore64_SSE4_1<WriteDuplicate16x4>>;
  using _32x8 =
      DirectionalPredFuncs_SSE4_1<ColStore8_SSE4_1<WriteDuplicate32x4>>;
  using _32x16 =
      DirectionalPredFuncs_SSE4_1<ColStore16_SSE4_1<WriteDuplicate32x4>>;
  using _32x32 =
      DirectionalPredFuncs_SSE4_1<ColStore32_SSE4_1<WriteDuplicate32x4>>;
  using _32x64 =
      DirectionalPredFuncs_SSE4_1<ColStore64_SSE4_1<WriteDuplicate32x4>>;
  using _64x16 =
      DirectionalPredFuncs_SSE4_1<ColStore16_SSE4_1<WriteDuplicate64x4>>;
  using _64x32 =
      DirectionalPredFuncs_SSE4_1<ColStore32_SSE4_1<WriteDuplicate64x4>>;
  using _64x64 =
      DirectionalPredFuncs_SSE4_1<ColStore64_SSE4_1<WriteDuplicate64x4>>;
};

template <int y_mask>
inline void WritePaethLine4(uint8_t* dst, const __m128i& top,
                            const __m128i& left, const __m128i& top_lefts,
                            const __m128i& top_dists, const __m128i& left_dists,
                            const __m128i& top_left_diffs) {
  const __m128i top_dists_y = _mm_shuffle_epi32(top_dists, y_mask);

  const __m128i lefts_y = _mm_shuffle_epi32(left, y_mask);
  const __m128i top_left_dists =
      _mm_abs_epi32(_mm_add_epi32(lefts_y, top_left_diffs));

  // Section 7.11.2.2 specifies the logic and terms here. The less-or-equal
  // operation is unavailable, so the logic for selecting top, left, or
  // top_left is inverted.
  __m128i not_select_left = _mm_cmpgt_epi32(left_dists, top_left_dists);
  not_select_left =
      _mm_or_si128(not_select_left, _mm_cmpgt_epi32(left_dists, top_dists_y));
  const __m128i not_select_top = _mm_cmpgt_epi32(top_dists_y, top_left_dists);

  const __m128i left_out = _mm_andnot_si128(not_select_left, lefts_y);

  const __m128i top_left_out = _mm_and_si128(not_select_top, top_lefts);
  __m128i top_or_top_left_out = _mm_andnot_si128(not_select_top, top);
  top_or_top_left_out = _mm_or_si128(top_or_top_left_out, top_left_out);
  top_or_top_left_out = _mm_and_si128(not_select_left, top_or_top_left_out);

  // The sequence of 32-bit packed operations was found (see CL via blame) to
  // outperform 16-bit operations, despite the availability of the packus
  // function, when tested on a Xeon E7 v3.
  const __m128i cvtepi32_epi8 = _mm_set1_epi32(0x0C080400);
  const __m128i pred = _mm_shuffle_epi8(
      _mm_or_si128(left_out, top_or_top_left_out), cvtepi32_epi8);
  Store4(dst, pred);
}

// top_left_diffs is the only variable whose ints may exceed 8 bits. Otherwise
// we would be able to do all of these operations as epi8 for a 16-pixel version
// of this function. Still, since lefts_y is just a vector of duplicates, it
// could pay off to accommodate top_left_dists for cmpgt, and repack into epi8
// for the blends.
template <int y_mask>
inline void WritePaethLine8(uint8_t* dst, const __m128i& top,
                            const __m128i& left, const __m128i& top_lefts,
                            const __m128i& top_dists, const __m128i& left_dists,
                            const __m128i& top_left_diffs) {
  const __m128i select_y = _mm_set1_epi32(y_mask);
  const __m128i top_dists_y = _mm_shuffle_epi8(top_dists, select_y);

  const __m128i lefts_y = _mm_shuffle_epi8(left, select_y);
  const __m128i top_left_dists =
      _mm_abs_epi16(_mm_add_epi16(lefts_y, top_left_diffs));

  // Section 7.11.2.2 specifies the logic and terms here. The less-or-equal
  // operation is unavailable, so the logic for selecting top, left, or
  // top_left is inverted.
  __m128i not_select_left = _mm_cmpgt_epi16(left_dists, top_left_dists);
  not_select_left =
      _mm_or_si128(not_select_left, _mm_cmpgt_epi16(left_dists, top_dists_y));
  const __m128i not_select_top = _mm_cmpgt_epi16(top_dists_y, top_left_dists);

  const __m128i left_out = _mm_andnot_si128(not_select_left, lefts_y);

  const __m128i top_left_out = _mm_and_si128(not_select_top, top_lefts);
  __m128i top_or_top_left_out = _mm_andnot_si128(not_select_top, top);
  top_or_top_left_out = _mm_or_si128(top_or_top_left_out, top_left_out);
  top_or_top_left_out = _mm_and_si128(not_select_left, top_or_top_left_out);

  const __m128i pred = _mm_packus_epi16(
      _mm_or_si128(left_out, top_or_top_left_out), /* unused */ left_out);
  _mm_storel_epi64(reinterpret_cast<__m128i*>(dst), pred);
}

// |top| is an epi8 of length 16
// |left| is epi8 of unknown length, as y_mask specifies access
// |top_lefts| is an epi8 of 16 duplicates
// |top_dists| is an epi8 of unknown length, as y_mask specifies access
// |left_dists| is an epi8 of length 16
// |left_dists_lo| is an epi16 of length 8
// |left_dists_hi| is an epi16 of length 8
// |top_left_diffs_lo| is an epi16 of length 8
// |top_left_diffs_hi| is an epi16 of length 8
// The latter two vectors are epi16 because their values may reach -510.
// |left_dists| is provided alongside its spread out version because it doesn't
// change between calls and interacts with both kinds of packing.
template <int y_mask>
inline void WritePaethLine16(uint8_t* dst, const __m128i& top,
                             const __m128i& left, const __m128i& top_lefts,
                             const __m128i& top_dists,
                             const __m128i& left_dists,
                             const __m128i& left_dists_lo,
                             const __m128i& left_dists_hi,
                             const __m128i& top_left_diffs_lo,
                             const __m128i& top_left_diffs_hi) {
  const __m128i select_y = _mm_set1_epi32(y_mask);
  const __m128i top_dists_y8 = _mm_shuffle_epi8(top_dists, select_y);
  const __m128i top_dists_y16 = _mm_cvtepu8_epi16(top_dists_y8);
  const __m128i lefts_y8 = _mm_shuffle_epi8(left, select_y);
  const __m128i lefts_y16 = _mm_cvtepu8_epi16(lefts_y8);

  const __m128i top_left_dists_lo =
      _mm_abs_epi16(_mm_add_epi16(lefts_y16, top_left_diffs_lo));
  const __m128i top_left_dists_hi =
      _mm_abs_epi16(_mm_add_epi16(lefts_y16, top_left_diffs_hi));

  const __m128i left_gt_top_left_lo = _mm_packs_epi16(
      _mm_cmpgt_epi16(left_dists_lo, top_left_dists_lo), left_dists_lo);
  const __m128i left_gt_top_left_hi =
      _mm_packs_epi16(_mm_cmpgt_epi16(left_dists_hi, top_left_dists_hi),
                      /* unused second arg for pack */ left_dists_hi);
  const __m128i left_gt_top_left = _mm_alignr_epi8(
      left_gt_top_left_hi, _mm_slli_si128(left_gt_top_left_lo, 8), 8);

  const __m128i not_select_top_lo =
      _mm_packs_epi16(_mm_cmpgt_epi16(top_dists_y16, top_left_dists_lo),
                      /* unused second arg for pack */ top_dists_y16);
  const __m128i not_select_top_hi =
      _mm_packs_epi16(_mm_cmpgt_epi16(top_dists_y16, top_left_dists_hi),
                      /* unused second arg for pack */ top_dists_y16);
  const __m128i not_select_top = _mm_alignr_epi8(
      not_select_top_hi, _mm_slli_si128(not_select_top_lo, 8), 8);

  const __m128i left_leq_top =
      _mm_cmpeq_epi8(left_dists, _mm_min_epu8(top_dists_y8, left_dists));
  const __m128i select_left = _mm_andnot_si128(left_gt_top_left, left_leq_top);

  // Section 7.11.2.2 specifies the logic and terms here. The less-or-equal
  // operation is unavailable, so the logic for selecting top, left, or
  // top_left is inverted.
  const __m128i left_out = _mm_and_si128(select_left, lefts_y8);

  const __m128i top_left_out = _mm_and_si128(not_select_top, top_lefts);
  __m128i top_or_top_left_out = _mm_andnot_si128(not_select_top, top);
  top_or_top_left_out = _mm_or_si128(top_or_top_left_out, top_left_out);
  top_or_top_left_out = _mm_andnot_si128(select_left, top_or_top_left_out);
  const __m128i pred = _mm_or_si128(left_out, top_or_top_left_out);

  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), pred);
}

void Paeth4x4_SSE4_1(void* const dest, ptrdiff_t stride,
                     const void* const top_row, const void* const left_column) {
  const __m128i left = _mm_cvtepu8_epi32(Load4(left_column));
  const __m128i top = _mm_cvtepu8_epi32(Load4(top_row));

  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_lefts = _mm_set1_epi32(top_ptr[-1]);

  // Given that the spec defines "base" as top[x] + left[y] - top[-1],
  // pLeft = abs(base - left[y]) = abs(top[x] - top[-1])
  // pTop = abs(base - top[x]) = abs(left[y] - top[-1])
  const __m128i left_dists = _mm_abs_epi32(_mm_sub_epi32(top, top_lefts));
  const __m128i top_dists = _mm_abs_epi32(_mm_sub_epi32(left, top_lefts));

  const __m128i top_left_x2 = _mm_add_epi32(top_lefts, top_lefts);
  const __m128i top_left_diff = _mm_sub_epi32(top, top_left_x2);
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaethLine4<0>(dst, top, left, top_lefts, top_dists, left_dists,
                     top_left_diff);
  dst += stride;
  WritePaethLine4<0x55>(dst, top, left, top_lefts, top_dists, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xAA>(dst, top, left, top_lefts, top_dists, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xFF>(dst, top, left, top_lefts, top_dists, left_dists,
                        top_left_diff);
}

void Paeth4x8_SSE4_1(void* const dest, ptrdiff_t stride,
                     const void* const top_row, const void* const left_column) {
  const __m128i left = LoadLo8(left_column);
  const __m128i left_lo = _mm_cvtepu8_epi32(left);
  const __m128i left_hi = _mm_cvtepu8_epi32(_mm_srli_si128(left, 4));

  const __m128i top = _mm_cvtepu8_epi32(Load4(top_row));
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_lefts = _mm_set1_epi32(top_ptr[-1]);

  // Given that the spec defines "base" as top[x] + left[y] - top[-1],
  // pLeft = abs(base - left[y]) = abs(top[x] - top[-1])
  // pTop = abs(base - top[x]) = abs(left[y] - top[-1])
  const __m128i left_dists = _mm_abs_epi32(_mm_sub_epi32(top, top_lefts));
  const __m128i top_dists_lo = _mm_abs_epi32(_mm_sub_epi32(left_lo, top_lefts));
  const __m128i top_dists_hi = _mm_abs_epi32(_mm_sub_epi32(left_hi, top_lefts));

  const __m128i top_left_x2 = _mm_add_epi32(top_lefts, top_lefts);
  const __m128i top_left_diff = _mm_sub_epi32(top, top_left_x2);
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaethLine4<0>(dst, top, left_lo, top_lefts, top_dists_lo, left_dists,
                     top_left_diff);
  dst += stride;
  WritePaethLine4<0x55>(dst, top, left_lo, top_lefts, top_dists_lo, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xAA>(dst, top, left_lo, top_lefts, top_dists_lo, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xFF>(dst, top, left_lo, top_lefts, top_dists_lo, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0>(dst, top, left_hi, top_lefts, top_dists_hi, left_dists,
                     top_left_diff);
  dst += stride;
  WritePaethLine4<0x55>(dst, top, left_hi, top_lefts, top_dists_hi, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xAA>(dst, top, left_hi, top_lefts, top_dists_hi, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xFF>(dst, top, left_hi, top_lefts, top_dists_hi, left_dists,
                        top_left_diff);
}

void Paeth4x16_SSE4_1(void* const dest, ptrdiff_t stride,
                      const void* const top_row,
                      const void* const left_column) {
  const __m128i left = LoadUnaligned16(left_column);
  const __m128i left_0 = _mm_cvtepu8_epi32(left);
  const __m128i left_1 = _mm_cvtepu8_epi32(_mm_srli_si128(left, 4));
  const __m128i left_2 = _mm_cvtepu8_epi32(_mm_srli_si128(left, 8));
  const __m128i left_3 = _mm_cvtepu8_epi32(_mm_srli_si128(left, 12));

  const __m128i top = _mm_cvtepu8_epi32(Load4(top_row));
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_lefts = _mm_set1_epi32(top_ptr[-1]);

  // Given that the spec defines "base" as top[x] + left[y] - top[-1],
  // pLeft = abs(base - left[y]) = abs(top[x] - top[-1])
  // pTop = abs(base - top[x]) = abs(left[y] - top[-1])
  const __m128i left_dists = _mm_abs_epi32(_mm_sub_epi32(top, top_lefts));
  const __m128i top_dists_0 = _mm_abs_epi32(_mm_sub_epi32(left_0, top_lefts));
  const __m128i top_dists_1 = _mm_abs_epi32(_mm_sub_epi32(left_1, top_lefts));
  const __m128i top_dists_2 = _mm_abs_epi32(_mm_sub_epi32(left_2, top_lefts));
  const __m128i top_dists_3 = _mm_abs_epi32(_mm_sub_epi32(left_3, top_lefts));

  const __m128i top_left_x2 = _mm_add_epi32(top_lefts, top_lefts);
  const __m128i top_left_diff = _mm_sub_epi32(top, top_left_x2);

  auto* dst = static_cast<uint8_t*>(dest);
  WritePaethLine4<0>(dst, top, left_0, top_lefts, top_dists_0, left_dists,
                     top_left_diff);
  dst += stride;
  WritePaethLine4<0x55>(dst, top, left_0, top_lefts, top_dists_0, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xAA>(dst, top, left_0, top_lefts, top_dists_0, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xFF>(dst, top, left_0, top_lefts, top_dists_0, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0>(dst, top, left_1, top_lefts, top_dists_1, left_dists,
                     top_left_diff);
  dst += stride;
  WritePaethLine4<0x55>(dst, top, left_1, top_lefts, top_dists_1, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xAA>(dst, top, left_1, top_lefts, top_dists_1, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xFF>(dst, top, left_1, top_lefts, top_dists_1, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0>(dst, top, left_2, top_lefts, top_dists_2, left_dists,
                     top_left_diff);
  dst += stride;
  WritePaethLine4<0x55>(dst, top, left_2, top_lefts, top_dists_2, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xAA>(dst, top, left_2, top_lefts, top_dists_2, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xFF>(dst, top, left_2, top_lefts, top_dists_2, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0>(dst, top, left_3, top_lefts, top_dists_3, left_dists,
                     top_left_diff);
  dst += stride;
  WritePaethLine4<0x55>(dst, top, left_3, top_lefts, top_dists_3, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xAA>(dst, top, left_3, top_lefts, top_dists_3, left_dists,
                        top_left_diff);
  dst += stride;
  WritePaethLine4<0xFF>(dst, top, left_3, top_lefts, top_dists_3, left_dists,
                        top_left_diff);
}

void Paeth8x4_SSE4_1(void* const dest, ptrdiff_t stride,
                     const void* const top_row, const void* const left_column) {
  const __m128i left = _mm_cvtepu8_epi16(Load4(left_column));
  const __m128i top = _mm_cvtepu8_epi16(LoadLo8(top_row));
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_lefts = _mm_set1_epi16(top_ptr[-1]);

  // Given that the spec defines "base" as top[x] + left[y] - top[-1],
  // pLeft = abs(base - left[y]) = abs(top[x] - top[-1])
  // pTop = abs(base - top[x]) = abs(left[y] - top[-1])
  const __m128i left_dists = _mm_abs_epi16(_mm_sub_epi16(top, top_lefts));
  const __m128i top_dists = _mm_abs_epi16(_mm_sub_epi16(left, top_lefts));

  const __m128i top_left_x2 = _mm_add_epi16(top_lefts, top_lefts);
  const __m128i top_left_diff = _mm_sub_epi16(top, top_left_x2);
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaethLine8<0x01000100>(dst, top, left, top_lefts, top_dists, left_dists,
                              top_left_diff);
  dst += stride;
  WritePaethLine8<0x03020302>(dst, top, left, top_lefts, top_dists, left_dists,
                              top_left_diff);
  dst += stride;
  WritePaethLine8<0x05040504>(dst, top, left, top_lefts, top_dists, left_dists,
                              top_left_diff);
  dst += stride;
  WritePaethLine8<0x07060706>(dst, top, left, top_lefts, top_dists, left_dists,
                              top_left_diff);
}

void Paeth8x8_SSE4_1(void* const dest, ptrdiff_t stride,
                     const void* const top_row, const void* const left_column) {
  const __m128i left = _mm_cvtepu8_epi16(LoadLo8(left_column));
  const __m128i top = _mm_cvtepu8_epi16(LoadLo8(top_row));
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_lefts = _mm_set1_epi16(top_ptr[-1]);

  // Given that the spec defines "base" as top[x] + left[y] - top[-1],
  // pLeft = abs(base - left[y]) = abs(top[x] - top[-1])
  // pTop = abs(base - top[x]) = abs(left[y] - top[-1])
  const __m128i left_dists = _mm_abs_epi16(_mm_sub_epi16(top, top_lefts));
  const __m128i top_dists = _mm_abs_epi16(_mm_sub_epi16(left, top_lefts));

  const __m128i top_left_x2 = _mm_add_epi16(top_lefts, top_lefts);
  const __m128i top_left_diff = _mm_sub_epi16(top, top_left_x2);
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaethLine8<0x01000100>(dst, top, left, top_lefts, top_dists, left_dists,
                              top_left_diff);
  dst += stride;
  WritePaethLine8<0x03020302>(dst, top, left, top_lefts, top_dists, left_dists,
                              top_left_diff);
  dst += stride;
  WritePaethLine8<0x05040504>(dst, top, left, top_lefts, top_dists, left_dists,
                              top_left_diff);
  dst += stride;
  WritePaethLine8<0x07060706>(dst, top, left, top_lefts, top_dists, left_dists,
                              top_left_diff);
  dst += stride;
  WritePaethLine8<0x09080908>(dst, top, left, top_lefts, top_dists, left_dists,
                              top_left_diff);
  dst += stride;
  WritePaethLine8<0x0B0A0B0A>(dst, top, left, top_lefts, top_dists, left_dists,
                              top_left_diff);
  dst += stride;
  WritePaethLine8<0x0D0C0D0C>(dst, top, left, top_lefts, top_dists, left_dists,
                              top_left_diff);
  dst += stride;
  WritePaethLine8<0x0F0E0F0E>(dst, top, left, top_lefts, top_dists, left_dists,
                              top_left_diff);
}

void Paeth8x16_SSE4_1(void* const dest, ptrdiff_t stride,
                      const void* const top_row,
                      const void* const left_column) {
  const __m128i left = LoadUnaligned16(left_column);
  const __m128i left_lo = _mm_cvtepu8_epi16(left);
  const __m128i left_hi = _mm_cvtepu8_epi16(_mm_srli_si128(left, 8));
  const __m128i top = _mm_cvtepu8_epi16(LoadLo8(top_row));
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_lefts = _mm_set1_epi16(top_ptr[-1]);

  // Given that the spec defines "base" as top[x] + left[y] - top[-1],
  // pLeft = abs(base - left[y]) = abs(top[x] - top[-1])
  // pTop = abs(base - top[x]) = abs(left[y] - top[-1])
  const __m128i left_dists = _mm_abs_epi16(_mm_sub_epi16(top, top_lefts));
  const __m128i top_dists_lo = _mm_abs_epi16(_mm_sub_epi16(left_lo, top_lefts));
  const __m128i top_dists_hi = _mm_abs_epi16(_mm_sub_epi16(left_hi, top_lefts));

  const __m128i top_left_x2 = _mm_add_epi16(top_lefts, top_lefts);
  const __m128i top_left_diff = _mm_sub_epi16(top, top_left_x2);
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaethLine8<0x01000100>(dst, top, left_lo, top_lefts, top_dists_lo,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x03020302>(dst, top, left_lo, top_lefts, top_dists_lo,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x05040504>(dst, top, left_lo, top_lefts, top_dists_lo,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x07060706>(dst, top, left_lo, top_lefts, top_dists_lo,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x09080908>(dst, top, left_lo, top_lefts, top_dists_lo,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x0B0A0B0A>(dst, top, left_lo, top_lefts, top_dists_lo,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x0D0C0D0C>(dst, top, left_lo, top_lefts, top_dists_lo,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x0F0E0F0E>(dst, top, left_lo, top_lefts, top_dists_lo,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x01000100>(dst, top, left_hi, top_lefts, top_dists_hi,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x03020302>(dst, top, left_hi, top_lefts, top_dists_hi,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x05040504>(dst, top, left_hi, top_lefts, top_dists_hi,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x07060706>(dst, top, left_hi, top_lefts, top_dists_hi,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x09080908>(dst, top, left_hi, top_lefts, top_dists_hi,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x0B0A0B0A>(dst, top, left_hi, top_lefts, top_dists_hi,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x0D0C0D0C>(dst, top, left_hi, top_lefts, top_dists_hi,
                              left_dists, top_left_diff);
  dst += stride;
  WritePaethLine8<0x0F0E0F0E>(dst, top, left_hi, top_lefts, top_dists_hi,
                              left_dists, top_left_diff);
}

void Paeth8x32_SSE4_1(void* const dest, ptrdiff_t stride,
                      const void* const top_row,
                      const void* const left_column) {
  const auto* const left_ptr = static_cast<const uint8_t*>(left_column);
  auto* const dst = static_cast<uint8_t*>(dest);
  Paeth8x16_SSE4_1(dst, stride, top_row, left_column);
  Paeth8x16_SSE4_1(dst + (stride << 4), stride, top_row, left_ptr + 16);
}

void Paeth16x4_SSE4_1(void* const dest, ptrdiff_t stride,
                      const void* const top_row,
                      const void* const left_column) {
  const __m128i left = Load4(left_column);
  const __m128i top = LoadUnaligned16(top_row);
  const __m128i top_lo = _mm_cvtepu8_epi16(top);
  const __m128i top_hi = _mm_cvtepu8_epi16(_mm_srli_si128(top, 8));

  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_lefts16 = _mm_set1_epi16(top_ptr[-1]);
  const __m128i top_lefts8 = _mm_set1_epi8(static_cast<int8_t>(top_ptr[-1]));

  // Given that the spec defines "base" as top[x] + left[y] - top[-1],
  // pLeft = abs(base - left[y]) = abs(top[x] - top[-1])
  // pTop = abs(base - top[x]) = abs(left[y] - top[-1])

  const __m128i left_dists = _mm_or_si128(_mm_subs_epu8(top, top_lefts8),
                                          _mm_subs_epu8(top_lefts8, top));
  const __m128i left_dists_lo = _mm_cvtepu8_epi16(left_dists);
  const __m128i left_dists_hi =
      _mm_cvtepu8_epi16(_mm_srli_si128(left_dists, 8));
  const __m128i top_dists = _mm_or_si128(_mm_subs_epu8(left, top_lefts8),
                                         _mm_subs_epu8(top_lefts8, left));

  const __m128i top_left_x2 = _mm_add_epi16(top_lefts16, top_lefts16);
  const __m128i top_left_diff_lo = _mm_sub_epi16(top_lo, top_left_x2);
  const __m128i top_left_diff_hi = _mm_sub_epi16(top_hi, top_left_x2);
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaethLine16<0>(dst, top, left, top_lefts8, top_dists, left_dists,
                      left_dists_lo, left_dists_hi, top_left_diff_lo,
                      top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x01010101>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x02020202>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x03030303>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
}

// Inlined for calling with offsets in larger transform sizes, mainly to
// preserve top_left.
inline void WritePaeth16x8(void* const dest, ptrdiff_t stride,
                           const uint8_t top_left, const __m128i top,
                           const __m128i left) {
  const __m128i top_lo = _mm_cvtepu8_epi16(top);
  const __m128i top_hi = _mm_cvtepu8_epi16(_mm_srli_si128(top, 8));

  const __m128i top_lefts16 = _mm_set1_epi16(top_left);
  const __m128i top_lefts8 = _mm_set1_epi8(static_cast<int8_t>(top_left));

  // Given that the spec defines "base" as top[x] + left[y] - top_left,
  // pLeft = abs(base - left[y]) = abs(top[x] - top[-1])
  // pTop = abs(base - top[x]) = abs(left[y] - top[-1])

  const __m128i left_dists = _mm_or_si128(_mm_subs_epu8(top, top_lefts8),
                                          _mm_subs_epu8(top_lefts8, top));
  const __m128i left_dists_lo = _mm_cvtepu8_epi16(left_dists);
  const __m128i left_dists_hi =
      _mm_cvtepu8_epi16(_mm_srli_si128(left_dists, 8));
  const __m128i top_dists = _mm_or_si128(_mm_subs_epu8(left, top_lefts8),
                                         _mm_subs_epu8(top_lefts8, left));

  const __m128i top_left_x2 = _mm_add_epi16(top_lefts16, top_lefts16);
  const __m128i top_left_diff_lo = _mm_sub_epi16(top_lo, top_left_x2);
  const __m128i top_left_diff_hi = _mm_sub_epi16(top_hi, top_left_x2);
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaethLine16<0>(dst, top, left, top_lefts8, top_dists, left_dists,
                      left_dists_lo, left_dists_hi, top_left_diff_lo,
                      top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x01010101>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x02020202>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x03030303>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x04040404>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x05050505>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x06060606>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x07070707>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
}

void Paeth16x8_SSE4_1(void* const dest, ptrdiff_t stride,
                      const void* const top_row,
                      const void* const left_column) {
  const __m128i top = LoadUnaligned16(top_row);
  const __m128i left = LoadLo8(left_column);
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  WritePaeth16x8(static_cast<uint8_t*>(dest), stride, top_ptr[-1], top, left);
}

void WritePaeth16x16(void* const dest, ptrdiff_t stride, const uint8_t top_left,
                     const __m128i top, const __m128i left) {
  const __m128i top_lo = _mm_cvtepu8_epi16(top);
  const __m128i top_hi = _mm_cvtepu8_epi16(_mm_srli_si128(top, 8));

  const __m128i top_lefts16 = _mm_set1_epi16(top_left);
  const __m128i top_lefts8 = _mm_set1_epi8(static_cast<int8_t>(top_left));

  // Given that the spec defines "base" as top[x] + left[y] - top[-1],
  // pLeft = abs(base - left[y]) = abs(top[x] - top[-1])
  // pTop = abs(base - top[x]) = abs(left[y] - top[-1])

  const __m128i left_dists = _mm_or_si128(_mm_subs_epu8(top, top_lefts8),
                                          _mm_subs_epu8(top_lefts8, top));
  const __m128i left_dists_lo = _mm_cvtepu8_epi16(left_dists);
  const __m128i left_dists_hi =
      _mm_cvtepu8_epi16(_mm_srli_si128(left_dists, 8));
  const __m128i top_dists = _mm_or_si128(_mm_subs_epu8(left, top_lefts8),
                                         _mm_subs_epu8(top_lefts8, left));

  const __m128i top_left_x2 = _mm_add_epi16(top_lefts16, top_lefts16);
  const __m128i top_left_diff_lo = _mm_sub_epi16(top_lo, top_left_x2);
  const __m128i top_left_diff_hi = _mm_sub_epi16(top_hi, top_left_x2);
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaethLine16<0>(dst, top, left, top_lefts8, top_dists, left_dists,
                      left_dists_lo, left_dists_hi, top_left_diff_lo,
                      top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x01010101>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x02020202>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x03030303>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x04040404>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x05050505>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x06060606>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x07070707>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x08080808>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x09090909>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x0A0A0A0A>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x0B0B0B0B>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x0C0C0C0C>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x0D0D0D0D>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x0E0E0E0E>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
  dst += stride;
  WritePaethLine16<0x0F0F0F0F>(dst, top, left, top_lefts8, top_dists,
                               left_dists, left_dists_lo, left_dists_hi,
                               top_left_diff_lo, top_left_diff_hi);
}

void Paeth16x16_SSE4_1(void* const dest, ptrdiff_t stride,
                       const void* const top_row,
                       const void* const left_column) {
  const __m128i left = LoadUnaligned16(left_column);
  const __m128i top = LoadUnaligned16(top_row);
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  WritePaeth16x16(static_cast<uint8_t*>(dest), stride, top_ptr[-1], top, left);
}

void Paeth16x32_SSE4_1(void* const dest, ptrdiff_t stride,
                       const void* const top_row,
                       const void* const left_column) {
  const __m128i left_0 = LoadUnaligned16(left_column);
  const __m128i top = LoadUnaligned16(top_row);
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const uint8_t top_left = top_ptr[-1];
  auto* const dst = static_cast<uint8_t*>(dest);
  WritePaeth16x16(dst, stride, top_left, top, left_0);
  const auto* const left_ptr = static_cast<const uint8_t*>(left_column);
  const __m128i left_1 = LoadUnaligned16(left_ptr + 16);
  WritePaeth16x16(dst + (stride << 4), stride, top_left, top, left_1);
}

void Paeth16x64_SSE4_1(void* const dest, ptrdiff_t stride,
                       const void* const top_row,
                       const void* const left_column) {
  const ptrdiff_t stride16 = stride << 4;
  const __m128i left_0 = LoadUnaligned16(left_column);
  const __m128i top = LoadUnaligned16(top_row);
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const uint8_t top_left = top_ptr[-1];
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaeth16x16(dst, stride, top_left, top, left_0);
  dst += stride16;
  const auto* const left_ptr = static_cast<const uint8_t*>(left_column);
  const __m128i left_1 = LoadUnaligned16(left_ptr + 16);
  WritePaeth16x16(dst, stride, top_left, top, left_1);
  dst += stride16;
  const __m128i left_2 = LoadUnaligned16(left_ptr + 32);
  WritePaeth16x16(dst, stride, top_left, top, left_2);
  dst += stride16;
  const __m128i left_3 = LoadUnaligned16(left_ptr + 48);
  WritePaeth16x16(dst, stride, top_left, top, left_3);
}

void Paeth32x8_SSE4_1(void* const dest, ptrdiff_t stride,
                      const void* const top_row,
                      const void* const left_column) {
  const __m128i left = LoadLo8(left_column);
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_0 = LoadUnaligned16(top_row);
  const uint8_t top_left = top_ptr[-1];
  auto* const dst = static_cast<uint8_t*>(dest);
  WritePaeth16x8(dst, stride, top_left, top_0, left);
  const __m128i top_1 = LoadUnaligned16(top_ptr + 16);
  WritePaeth16x8(dst + 16, stride, top_left, top_1, left);
}

void Paeth32x16_SSE4_1(void* const dest, ptrdiff_t stride,
                       const void* const top_row,
                       const void* const left_column) {
  const __m128i left = LoadUnaligned16(left_column);
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_0 = LoadUnaligned16(top_row);
  const uint8_t top_left = top_ptr[-1];
  auto* const dst = static_cast<uint8_t*>(dest);
  WritePaeth16x16(dst, stride, top_left, top_0, left);
  const __m128i top_1 = LoadUnaligned16(top_ptr + 16);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left);
}

void Paeth32x32_SSE4_1(void* const dest, ptrdiff_t stride,
                       const void* const top_row,
                       const void* const left_column) {
  const auto* const left_ptr = static_cast<const uint8_t*>(left_column);
  const __m128i left_0 = LoadUnaligned16(left_ptr);
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_0 = LoadUnaligned16(top_ptr);
  const __m128i left_1 = LoadUnaligned16(left_ptr + 16);
  const __m128i top_1 = LoadUnaligned16(top_ptr + 16);
  const uint8_t top_left = top_ptr[-1];
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaeth16x16(dst, stride, top_left, top_0, left_0);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left_0);
  dst += (stride << 4);
  WritePaeth16x16(dst, stride, top_left, top_0, left_1);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left_1);
}

void Paeth32x64_SSE4_1(void* const dest, ptrdiff_t stride,
                       const void* const top_row,
                       const void* const left_column) {
  const auto* const left_ptr = static_cast<const uint8_t*>(left_column);
  const __m128i left_0 = LoadUnaligned16(left_ptr);
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_0 = LoadUnaligned16(top_ptr);
  const __m128i left_1 = LoadUnaligned16(left_ptr + 16);
  const __m128i top_1 = LoadUnaligned16(top_ptr + 16);
  const __m128i left_2 = LoadUnaligned16(left_ptr + 32);
  const __m128i left_3 = LoadUnaligned16(left_ptr + 48);
  const uint8_t top_left = top_ptr[-1];
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaeth16x16(dst, stride, top_left, top_0, left_0);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left_0);
  dst += (stride << 4);
  WritePaeth16x16(dst, stride, top_left, top_0, left_1);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left_1);
  dst += (stride << 4);
  WritePaeth16x16(dst, stride, top_left, top_0, left_2);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left_2);
  dst += (stride << 4);
  WritePaeth16x16(dst, stride, top_left, top_0, left_3);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left_3);
}

void Paeth64x16_SSE4_1(void* const dest, ptrdiff_t stride,
                       const void* const top_row,
                       const void* const left_column) {
  const __m128i left = LoadUnaligned16(left_column);
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_0 = LoadUnaligned16(top_ptr);
  const __m128i top_1 = LoadUnaligned16(top_ptr + 16);
  const __m128i top_2 = LoadUnaligned16(top_ptr + 32);
  const __m128i top_3 = LoadUnaligned16(top_ptr + 48);
  const uint8_t top_left = top_ptr[-1];
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaeth16x16(dst, stride, top_left, top_0, left);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left);
  WritePaeth16x16(dst + 32, stride, top_left, top_2, left);
  WritePaeth16x16(dst + 48, stride, top_left, top_3, left);
}

void Paeth64x32_SSE4_1(void* const dest, ptrdiff_t stride,
                       const void* const top_row,
                       const void* const left_column) {
  const auto* const left_ptr = static_cast<const uint8_t*>(left_column);
  const __m128i left_0 = LoadUnaligned16(left_ptr);
  const __m128i left_1 = LoadUnaligned16(left_ptr + 16);
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_0 = LoadUnaligned16(top_ptr);
  const __m128i top_1 = LoadUnaligned16(top_ptr + 16);
  const __m128i top_2 = LoadUnaligned16(top_ptr + 32);
  const __m128i top_3 = LoadUnaligned16(top_ptr + 48);
  const uint8_t top_left = top_ptr[-1];
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaeth16x16(dst, stride, top_left, top_0, left_0);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left_0);
  WritePaeth16x16(dst + 32, stride, top_left, top_2, left_0);
  WritePaeth16x16(dst + 48, stride, top_left, top_3, left_0);
  dst += (stride << 4);
  WritePaeth16x16(dst, stride, top_left, top_0, left_1);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left_1);
  WritePaeth16x16(dst + 32, stride, top_left, top_2, left_1);
  WritePaeth16x16(dst + 48, stride, top_left, top_3, left_1);
}

void Paeth64x64_SSE4_1(void* const dest, ptrdiff_t stride,
                       const void* const top_row,
                       const void* const left_column) {
  const auto* const left_ptr = static_cast<const uint8_t*>(left_column);
  const __m128i left_0 = LoadUnaligned16(left_ptr);
  const __m128i left_1 = LoadUnaligned16(left_ptr + 16);
  const __m128i left_2 = LoadUnaligned16(left_ptr + 32);
  const __m128i left_3 = LoadUnaligned16(left_ptr + 48);
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const __m128i top_0 = LoadUnaligned16(top_ptr);
  const __m128i top_1 = LoadUnaligned16(top_ptr + 16);
  const __m128i top_2 = LoadUnaligned16(top_ptr + 32);
  const __m128i top_3 = LoadUnaligned16(top_ptr + 48);
  const uint8_t top_left = top_ptr[-1];
  auto* dst = static_cast<uint8_t*>(dest);
  WritePaeth16x16(dst, stride, top_left, top_0, left_0);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left_0);
  WritePaeth16x16(dst + 32, stride, top_left, top_2, left_0);
  WritePaeth16x16(dst + 48, stride, top_left, top_3, left_0);
  dst += (stride << 4);
  WritePaeth16x16(dst, stride, top_left, top_0, left_1);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left_1);
  WritePaeth16x16(dst + 32, stride, top_left, top_2, left_1);
  WritePaeth16x16(dst + 48, stride, top_left, top_3, left_1);
  dst += (stride << 4);
  WritePaeth16x16(dst, stride, top_left, top_0, left_2);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left_2);
  WritePaeth16x16(dst + 32, stride, top_left, top_2, left_2);
  WritePaeth16x16(dst + 48, stride, top_left, top_3, left_2);
  dst += (stride << 4);
  WritePaeth16x16(dst, stride, top_left, top_0, left_3);
  WritePaeth16x16(dst + 16, stride, top_left, top_1, left_3);
  WritePaeth16x16(dst + 32, stride, top_left, top_2, left_3);
  WritePaeth16x16(dst + 48, stride, top_left, top_3, left_3);
}

//------------------------------------------------------------------------------
// 7.11.2.4. Directional intra prediction process

// Special case: An |xstep| of 64 corresponds to an angle delta of 45, meaning
// upsampling is ruled out. In addition, the bits masked by 0x3F for
// |shift_val| are 0 for all multiples of 64, so the formula
// val = top[top_base_x]*shift + top[top_base_x+1]*(32-shift), reduces to
// val = top[top_base_x+1] << 5, meaning only the second set of pixels is
// involved in the output. Hence |top| is offset by 1.
inline void DirectionalZone1_Step64(uint8_t* dst, ptrdiff_t stride,
                                    const uint8_t* const top, const int width,
                                    const int height) {
  ptrdiff_t offset = 1;
  if (height == 4) {
    memcpy(dst, top + offset, width);
    dst += stride;
    memcpy(dst, top + offset + 1, width);
    dst += stride;
    memcpy(dst, top + offset + 2, width);
    dst += stride;
    memcpy(dst, top + offset + 3, width);
    return;
  }
  int y = 0;
  do {
    memcpy(dst, top + offset, width);
    dst += stride;
    memcpy(dst, top + offset + 1, width);
    dst += stride;
    memcpy(dst, top + offset + 2, width);
    dst += stride;
    memcpy(dst, top + offset + 3, width);
    dst += stride;
    memcpy(dst, top + offset + 4, width);
    dst += stride;
    memcpy(dst, top + offset + 5, width);
    dst += stride;
    memcpy(dst, top + offset + 6, width);
    dst += stride;
    memcpy(dst, top + offset + 7, width);
    dst += stride;

    offset += 8;
    y += 8;
  } while (y < height);
}

inline void DirectionalZone1_4xH(uint8_t* dst, ptrdiff_t stride,
                                 const uint8_t* const top, const int height,
                                 const int xstep, const bool upsampled) {
  const int upsample_shift = static_cast<int>(upsampled);
  const int scale_bits = 6 - upsample_shift;
  const int rounding_bits = 5;
  const int max_base_x = (height + 3 /* width - 1 */) << upsample_shift;
  const __m128i final_top_val = _mm_set1_epi16(top[max_base_x]);
  const __m128i sampler = upsampled ? _mm_set_epi64x(0, 0x0706050403020100)
                                    : _mm_set_epi64x(0, 0x0403030202010100);
  // Each 16-bit value here corresponds to a position that may exceed
  // |max_base_x|. When added to the top_base_x, it is used to mask values
  // that pass the end of |top|. Starting from 1 to simulate "cmpge" which is
  // not supported for packed integers.
  const __m128i offsets =
      _mm_set_epi32(0x00080007, 0x00060005, 0x00040003, 0x00020001);

  // All rows from |min_corner_only_y| down will simply use memcpy. |max_base_x|
  // is always greater than |height|, so clipping to 1 is enough to make the
  // logic work.
  const int xstep_units = std::max(xstep >> scale_bits, 1);
  const int min_corner_only_y = std::min(max_base_x / xstep_units, height);

  // Rows up to this y-value can be computed without checking for bounds.
  int y = 0;
  int top_x = xstep;

  for (; y < min_corner_only_y; ++y, dst += stride, top_x += xstep) {
    const int top_base_x = top_x >> scale_bits;

    // Permit negative values of |top_x|.
    const int shift_val = (LeftShift(top_x, upsample_shift) & 0x3F) >> 1;
    const __m128i shift = _mm_set1_epi8(shift_val);
    const __m128i max_shift = _mm_set1_epi8(32);
    const __m128i opposite_shift = _mm_sub_epi8(max_shift, shift);
    const __m128i shifts = _mm_unpacklo_epi8(opposite_shift, shift);
    __m128i top_index_vect = _mm_set1_epi16(top_base_x);
    top_index_vect = _mm_add_epi16(top_index_vect, offsets);
    const __m128i max_base_x_vect = _mm_set1_epi16(max_base_x);

    // Load 8 values because we will select the sampled values based on
    // |upsampled|.
    const __m128i values = LoadLo8(top + top_base_x);
    const __m128i sampled_values = _mm_shuffle_epi8(values, sampler);
    const __m128i past_max = _mm_cmpgt_epi16(top_index_vect, max_base_x_vect);
    __m128i prod = _mm_maddubs_epi16(sampled_values, shifts);
    prod = RightShiftWithRounding_U16(prod, rounding_bits);
    // Replace pixels from invalid range with top-right corner.
    prod = _mm_blendv_epi8(prod, final_top_val, past_max);
    Store4(dst, _mm_packus_epi16(prod, prod));
  }

  // Fill in corner-only rows.
  for (; y < height; ++y) {
    memset(dst, top[max_base_x], /* width */ 4);
    dst += stride;
  }
}

// 7.11.2.4 (7) angle < 90
inline void DirectionalZone1_Large(uint8_t* dest, ptrdiff_t stride,
                                   const uint8_t* const top_row,
                                   const int width, const int height,
                                   const int xstep, const bool upsampled) {
  const int upsample_shift = static_cast<int>(upsampled);
  const __m128i sampler =
      upsampled ? _mm_set_epi32(0x0F0E0D0C, 0x0B0A0908, 0x07060504, 0x03020100)
                : _mm_set_epi32(0x08070706, 0x06050504, 0x04030302, 0x02010100);
  const int scale_bits = 6 - upsample_shift;
  const int max_base_x = ((width + height) - 1) << upsample_shift;

  const __m128i max_shift = _mm_set1_epi8(32);
  const int rounding_bits = 5;
  const int base_step = 1 << upsample_shift;
  const int base_step8 = base_step << 3;

  // All rows from |min_corner_only_y| down will simply use memcpy. |max_base_x|
  // is always greater than |height|, so clipping to 1 is enough to make the
  // logic work.
  const int xstep_units = std::max(xstep >> scale_bits, 1);
  const int min_corner_only_y = std::min(max_base_x / xstep_units, height);

  // Rows up to this y-value can be computed without checking for bounds.
  const int max_no_corner_y = std::min(
      LeftShift((max_base_x - (base_step * width)), scale_bits) / xstep,
      height);
  // No need to check for exceeding |max_base_x| in the first loop.
  int y = 0;
  int top_x = xstep;
  for (; y < max_no_corner_y; ++y, dest += stride, top_x += xstep) {
    int top_base_x = top_x >> scale_bits;
    // Permit negative values of |top_x|.
    const int shift_val = (LeftShift(top_x, upsample_shift) & 0x3F) >> 1;
    const __m128i shift = _mm_set1_epi8(shift_val);
    const __m128i opposite_shift = _mm_sub_epi8(max_shift, shift);
    const __m128i shifts = _mm_unpacklo_epi8(opposite_shift, shift);
    int x = 0;
    do {
      const __m128i top_vals = LoadUnaligned16(top_row + top_base_x);
      __m128i vals = _mm_shuffle_epi8(top_vals, sampler);
      vals = _mm_maddubs_epi16(vals, shifts);
      vals = RightShiftWithRounding_U16(vals, rounding_bits);
      StoreLo8(dest + x, _mm_packus_epi16(vals, vals));
      top_base_x += base_step8;
      x += 8;
    } while (x < width);
  }

  // Each 16-bit value here corresponds to a position that may exceed
  // |max_base_x|. When added to the top_base_x, it is used to mask values
  // that pass the end of |top|. Starting from 1 to simulate "cmpge" which is
  // not supported for packed integers.
  const __m128i offsets =
      _mm_set_epi32(0x00080007, 0x00060005, 0x00040003, 0x00020001);

  const __m128i max_base_x_vect = _mm_set1_epi16(max_base_x);
  const __m128i final_top_val = _mm_set1_epi16(top_row[max_base_x]);
  const __m128i base_step8_vect = _mm_set1_epi16(base_step8);
  for (; y < min_corner_only_y; ++y, dest += stride, top_x += xstep) {
    int top_base_x = top_x >> scale_bits;

    const int shift_val = (LeftShift(top_x, upsample_shift) & 0x3F) >> 1;
    const __m128i shift = _mm_set1_epi8(shift_val);
    const __m128i opposite_shift = _mm_sub_epi8(max_shift, shift);
    const __m128i shifts = _mm_unpacklo_epi8(opposite_shift, shift);
    __m128i top_index_vect = _mm_set1_epi16(top_base_x);
    top_index_vect = _mm_add_epi16(top_index_vect, offsets);

    int x = 0;
    const int min_corner_only_x =
        std::min(width, ((max_base_x - top_base_x) >> upsample_shift) + 7) & ~7;
    for (; x < min_corner_only_x;
         x += 8, top_base_x += base_step8,
         top_index_vect = _mm_add_epi16(top_index_vect, base_step8_vect)) {
      const __m128i past_max = _mm_cmpgt_epi16(top_index_vect, max_base_x_vect);
      // Assuming a buffer zone of 8 bytes at the end of top_row, this prevents
      // reading out of bounds. If all indices are past max and we don't need to
      // use the loaded bytes at all, |top_base_x| becomes 0. |top_base_x| will
      // reset for the next |y|.
      top_base_x &= ~_mm_cvtsi128_si32(past_max);
      const __m128i top_vals = LoadUnaligned16(top_row + top_base_x);
      __m128i vals = _mm_shuffle_epi8(top_vals, sampler);
      vals = _mm_maddubs_epi16(vals, shifts);
      vals = RightShiftWithRounding_U16(vals, rounding_bits);
      vals = _mm_blendv_epi8(vals, final_top_val, past_max);
      StoreLo8(dest + x, _mm_packus_epi16(vals, vals));
    }
    // Corner-only section of the row.
    memset(dest + x, top_row[max_base_x], width - x);
  }
  // Fill in corner-only rows.
  for (; y < height; ++y) {
    memset(dest, top_row[max_base_x], width);
    dest += stride;
  }
}

// 7.11.2.4 (7) angle < 90
inline void DirectionalZone1_SSE4_1(uint8_t* dest, ptrdiff_t stride,
                                    const uint8_t* const top_row,
                                    const int width, const int height,
                                    const int xstep, const bool upsampled) {
  const int upsample_shift = static_cast<int>(upsampled);
  if (xstep == 64) {
    DirectionalZone1_Step64(dest, stride, top_row, width, height);
    return;
  }
  if (width == 4) {
    DirectionalZone1_4xH(dest, stride, top_row, height, xstep, upsampled);
    return;
  }
  if (width >= 32) {
    DirectionalZone1_Large(dest, stride, top_row, width, height, xstep,
                           upsampled);
    return;
  }
  const __m128i sampler =
      upsampled ? _mm_set_epi32(0x0F0E0D0C, 0x0B0A0908, 0x07060504, 0x03020100)
                : _mm_set_epi32(0x08070706, 0x06050504, 0x04030302, 0x02010100);
  const int scale_bits = 6 - upsample_shift;
  const int max_base_x = ((width + height) - 1) << upsample_shift;

  const __m128i max_shift = _mm_set1_epi8(32);
  const int rounding_bits = 5;
  const int base_step = 1 << upsample_shift;
  const int base_step8 = base_step << 3;

  // No need to check for exceeding |max_base_x| in the loops.
  if (((xstep * height) >> scale_bits) + base_step * width < max_base_x) {
    int top_x = xstep;
    int y = 0;
    do {
      int top_base_x = top_x >> scale_bits;
      // Permit negative values of |top_x|.
      const int shift_val = (LeftShift(top_x, upsample_shift) & 0x3F) >> 1;
      const __m128i shift = _mm_set1_epi8(shift_val);
      const __m128i opposite_shift = _mm_sub_epi8(max_shift, shift);
      const __m128i shifts = _mm_unpacklo_epi8(opposite_shift, shift);
      int x = 0;
      do {
        const __m128i top_vals = LoadUnaligned16(top_row + top_base_x);
        __m128i vals = _mm_shuffle_epi8(top_vals, sampler);
        vals = _mm_maddubs_epi16(vals, shifts);
        vals = RightShiftWithRounding_U16(vals, rounding_bits);
        StoreLo8(dest + x, _mm_packus_epi16(vals, vals));
        top_base_x += base_step8;
        x += 8;
      } while (x < width);
      dest += stride;
      top_x += xstep;
    } while (++y < height);
    return;
  }

  // Each 16-bit value here corresponds to a position that may exceed
  // |max_base_x|. When added to the top_base_x, it is used to mask values
  // that pass the end of |top|. Starting from 1 to simulate "cmpge" which is
  // not supported for packed integers.
  const __m128i offsets =
      _mm_set_epi32(0x00080007, 0x00060005, 0x00040003, 0x00020001);

  const __m128i max_base_x_vect = _mm_set1_epi16(max_base_x);
  const __m128i final_top_val = _mm_set1_epi16(top_row[max_base_x]);
  const __m128i base_step8_vect = _mm_set1_epi16(base_step8);
  int top_x = xstep;
  int y = 0;
  do {
    int top_base_x = top_x >> scale_bits;

    if (top_base_x >= max_base_x) {
      for (int i = y; i < height; ++i) {
        memset(dest, top_row[max_base_x], width);
        dest += stride;
      }
      return;
    }

    const int shift_val = (LeftShift(top_x, upsample_shift) & 0x3F) >> 1;
    const __m128i shift = _mm_set1_epi8(shift_val);
    const __m128i opposite_shift = _mm_sub_epi8(max_shift, shift);
    const __m128i shifts = _mm_unpacklo_epi8(opposite_shift, shift);
    __m128i top_index_vect = _mm_set1_epi16(top_base_x);
    top_index_vect = _mm_add_epi16(top_index_vect, offsets);

    int x = 0;
    for (; x < width - 8;
         x += 8, top_base_x += base_step8,
         top_index_vect = _mm_add_epi16(top_index_vect, base_step8_vect)) {
      const __m128i past_max = _mm_cmpgt_epi16(top_index_vect, max_base_x_vect);
      // Assuming a buffer zone of 8 bytes at the end of top_row, this prevents
      // reading out of bounds. If all indices are past max and we don't need to
      // use the loaded bytes at all, |top_base_x| becomes 0. |top_base_x| will
      // reset for the next |y|.
      top_base_x &= ~_mm_cvtsi128_si32(past_max);
      const __m128i top_vals = LoadUnaligned16(top_row + top_base_x);
      __m128i vals = _mm_shuffle_epi8(top_vals, sampler);
      vals = _mm_maddubs_epi16(vals, shifts);
      vals = RightShiftWithRounding_U16(vals, rounding_bits);
      vals = _mm_blendv_epi8(vals, final_top_val, past_max);
      StoreLo8(dest + x, _mm_packus_epi16(vals, vals));
    }
    const __m128i past_max = _mm_cmpgt_epi16(top_index_vect, max_base_x_vect);
    __m128i vals;
    if (upsampled) {
      vals = LoadUnaligned16(top_row + top_base_x);
    } else {
      const __m128i top_vals = LoadLo8(top_row + top_base_x);
      vals = _mm_shuffle_epi8(top_vals, sampler);
      vals = _mm_insert_epi8(vals, top_row[top_base_x + 8], 15);
    }
    vals = _mm_maddubs_epi16(vals, shifts);
    vals = RightShiftWithRounding_U16(vals, rounding_bits);
    vals = _mm_blendv_epi8(vals, final_top_val, past_max);
    StoreLo8(dest + x, _mm_packus_epi16(vals, vals));
    dest += stride;
    top_x += xstep;
  } while (++y < height);
}

void DirectionalIntraPredictorZone1_SSE4_1(void* const dest, ptrdiff_t stride,
                                           const void* const top_row,
                                           const int width, const int height,
                                           const int xstep,
                                           const bool upsampled_top) {
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  auto* dst = static_cast<uint8_t*>(dest);
  DirectionalZone1_SSE4_1(dst, stride, top_ptr, width, height, xstep,
                          upsampled_top);
}

template <bool upsampled>
inline void DirectionalZone3_4x4(uint8_t* dest, ptrdiff_t stride,
                                 const uint8_t* const left_column,
                                 const int base_left_y, const int ystep) {
  // For use in the non-upsampled case.
  const __m128i sampler = _mm_set_epi64x(0, 0x0403030202010100);
  const int upsample_shift = static_cast<int>(upsampled);
  const int scale_bits = 6 - upsample_shift;
  const __m128i max_shift = _mm_set1_epi8(32);
  const int rounding_bits = 5;

  __m128i result_block[4];
  for (int x = 0, left_y = base_left_y; x < 4; x++, left_y += ystep) {
    const int left_base_y = left_y >> scale_bits;
    const int shift_val = ((left_y << upsample_shift) & 0x3F) >> 1;
    const __m128i shift = _mm_set1_epi8(shift_val);
    const __m128i opposite_shift = _mm_sub_epi8(max_shift, shift);
    const __m128i shifts = _mm_unpacklo_epi8(opposite_shift, shift);
    __m128i vals;
    if (upsampled) {
      vals = LoadLo8(left_column + left_base_y);
    } else {
      const __m128i top_vals = LoadLo8(left_column + left_base_y);
      vals = _mm_shuffle_epi8(top_vals, sampler);
    }
    vals = _mm_maddubs_epi16(vals, shifts);
    vals = RightShiftWithRounding_U16(vals, rounding_bits);
    result_block[x] = _mm_packus_epi16(vals, vals);
  }
  const __m128i result = Transpose4x4_U8(result_block);
  // This is result_row0.
  Store4(dest, result);
  dest += stride;
  const int result_row1 = _mm_extract_epi32(result, 1);
  memcpy(dest, &result_row1, sizeof(result_row1));
  dest += stride;
  const int result_row2 = _mm_extract_epi32(result, 2);
  memcpy(dest, &result_row2, sizeof(result_row2));
  dest += stride;
  const int result_row3 = _mm_extract_epi32(result, 3);
  memcpy(dest, &result_row3, sizeof(result_row3));
}

template <bool upsampled, int height>
inline void DirectionalZone3_8xH(uint8_t* dest, ptrdiff_t stride,
                                 const uint8_t* const left_column,
                                 const int base_left_y, const int ystep) {
  // For use in the non-upsampled case.
  const __m128i sampler =
      _mm_set_epi64x(0x0807070606050504, 0x0403030202010100);
  const int upsample_shift = static_cast<int>(upsampled);
  const int scale_bits = 6 - upsample_shift;
  const __m128i max_shift = _mm_set1_epi8(32);
  const int rounding_bits = 5;

  __m128i result_block[8];
  for (int x = 0, left_y = base_left_y; x < 8; x++, left_y += ystep) {
    const int left_base_y = left_y >> scale_bits;
    const int shift_val = (LeftShift(left_y, upsample_shift) & 0x3F) >> 1;
    const __m128i shift = _mm_set1_epi8(shift_val);
    const __m128i opposite_shift = _mm_sub_epi8(max_shift, shift);
    const __m128i shifts = _mm_unpacklo_epi8(opposite_shift, shift);
    __m128i vals;
    if (upsampled) {
      vals = LoadUnaligned16(left_column + left_base_y);
    } else {
      const __m128i top_vals = LoadUnaligned16(left_column + left_base_y);
      vals = _mm_shuffle_epi8(top_vals, sampler);
    }
    vals = _mm_maddubs_epi16(vals, shifts);
    result_block[x] = RightShiftWithRounding_U16(vals, rounding_bits);
  }
  Transpose8x8_U16(result_block, result_block);
  for (int y = 0; y < height; ++y) {
    StoreLo8(dest, _mm_packus_epi16(result_block[y], result_block[y]));
    dest += stride;
  }
}

// 7.11.2.4 (9) angle > 180
void DirectionalIntraPredictorZone3_SSE4_1(void* dest, ptrdiff_t stride,
                                           const void* const left_column,
                                           const int width, const int height,
                                           const int ystep,
                                           const bool upsampled) {
  const auto* const left_ptr = static_cast<const uint8_t*>(left_column);
  auto* dst = static_cast<uint8_t*>(dest);
  const int upsample_shift = static_cast<int>(upsampled);
  if (width == 4 || height == 4) {
    const ptrdiff_t stride4 = stride << 2;
    if (upsampled) {
      int left_y = ystep;
      int x = 0;
      do {
        uint8_t* dst_x = dst + x;
        int y = 0;
        do {
          DirectionalZone3_4x4<true>(
              dst_x, stride, left_ptr + (y << upsample_shift), left_y, ystep);
          dst_x += stride4;
          y += 4;
        } while (y < height);
        left_y += ystep << 2;
        x += 4;
      } while (x < width);
    } else {
      int left_y = ystep;
      int x = 0;
      do {
        uint8_t* dst_x = dst + x;
        int y = 0;
        do {
          DirectionalZone3_4x4<false>(dst_x, stride, left_ptr + y, left_y,
                                      ystep);
          dst_x += stride4;
          y += 4;
        } while (y < height);
        left_y += ystep << 2;
        x += 4;
      } while (x < width);
    }
    return;
  }

  const ptrdiff_t stride8 = stride << 3;
  if (upsampled) {
    int left_y = ystep;
    int x = 0;
    do {
      uint8_t* dst_x = dst + x;
      int y = 0;
      do {
        DirectionalZone3_8xH<true, 8>(
            dst_x, stride, left_ptr + (y << upsample_shift), left_y, ystep);
        dst_x += stride8;
        y += 8;
      } while (y < height);
      left_y += ystep << 3;
      x += 8;
    } while (x < width);
  } else {
    int left_y = ystep;
    int x = 0;
    do {
      uint8_t* dst_x = dst + x;
      int y = 0;
      do {
        DirectionalZone3_8xH<false, 8>(
            dst_x, stride, left_ptr + (y << upsample_shift), left_y, ystep);
        dst_x += stride8;
        y += 8;
      } while (y < height);
      left_y += ystep << 3;
      x += 8;
    } while (x < width);
  }
}

//------------------------------------------------------------------------------
// Directional Zone 2 Functions
// 7.11.2.4 (8)

// DirectionalBlend* selectively overwrites the values written by
// DirectionalZone2FromLeftCol*. |zone_bounds| has one 16-bit index for each
// row.
template <int y_selector>
inline void DirectionalBlend4_SSE4_1(uint8_t* dest,
                                     const __m128i& dest_index_vect,
                                     const __m128i& vals,
                                     const __m128i& zone_bounds) {
  const __m128i max_dest_x_vect = _mm_shufflelo_epi16(zone_bounds, y_selector);
  const __m128i use_left = _mm_cmplt_epi16(dest_index_vect, max_dest_x_vect);
  const __m128i original_vals = _mm_cvtepu8_epi16(Load4(dest));
  const __m128i blended_vals = _mm_blendv_epi8(vals, original_vals, use_left);
  Store4(dest, _mm_packus_epi16(blended_vals, blended_vals));
}

inline void DirectionalBlend8_SSE4_1(uint8_t* dest,
                                     const __m128i& dest_index_vect,
                                     const __m128i& vals,
                                     const __m128i& zone_bounds,
                                     const __m128i& bounds_selector) {
  const __m128i max_dest_x_vect =
      _mm_shuffle_epi8(zone_bounds, bounds_selector);
  const __m128i use_left = _mm_cmplt_epi16(dest_index_vect, max_dest_x_vect);
  const __m128i original_vals = _mm_cvtepu8_epi16(LoadLo8(dest));
  const __m128i blended_vals = _mm_blendv_epi8(vals, original_vals, use_left);
  StoreLo8(dest, _mm_packus_epi16(blended_vals, blended_vals));
}

constexpr int kDirectionalWeightBits = 5;
// |source| is packed with 4 or 8 pairs of 8-bit values from left or top.
// |shifts| is named to match the specification, with 4 or 8 pairs of (32 -
// shift) and shift. Shift is guaranteed to be between 0 and 32.
inline __m128i DirectionalZone2FromSource_SSE4_1(const uint8_t* const source,
                                                 const __m128i& shifts,
                                                 const __m128i& sampler) {
  const __m128i src_vals = LoadUnaligned16(source);
  __m128i vals = _mm_shuffle_epi8(src_vals, sampler);
  vals = _mm_maddubs_epi16(vals, shifts);
  return RightShiftWithRounding_U16(vals, kDirectionalWeightBits);
}

// Because the source values "move backwards" as the row index increases, the
// indices derived from ystep are generally negative. This is accommodated by
// making sure the relative indices are within [-15, 0] when the function is
// called, and sliding them into the inclusive range [0, 15], relative to a
// lower base address.
constexpr int kPositiveIndexOffset = 15;

template <bool upsampled>
inline void DirectionalZone2FromLeftCol_4x4_SSE4_1(
    uint8_t* dst, ptrdiff_t stride, const uint8_t* const left_column_base,
    __m128i left_y) {
  const int upsample_shift = static_cast<int>(upsampled);
  const int scale_bits = 6 - upsample_shift;
  const __m128i max_shifts = _mm_set1_epi8(32);
  const __m128i shift_mask = _mm_set1_epi32(0x003F003F);
  const __m128i index_increment = _mm_cvtsi32_si128(0x01010101);
  const __m128i positive_offset = _mm_set1_epi8(kPositiveIndexOffset);
  // Left_column and sampler are both offset by 15 so the indices are always
  // positive.
  const uint8_t* left_column = left_column_base - kPositiveIndexOffset;
  for (int y = 0; y < 4; dst += stride, ++y) {
    __m128i offset_y = _mm_srai_epi16(left_y, scale_bits);
    offset_y = _mm_packs_epi16(offset_y, offset_y);

    const __m128i adjacent = _mm_add_epi8(offset_y, index_increment);
    __m128i sampler = _mm_unpacklo_epi8(offset_y, adjacent);
    // Slide valid |offset_y| indices from range [-15, 0] to [0, 15] so they
    // can work as shuffle indices. Some values may be out of bounds, but their
    // pred results will be masked over by top prediction.
    sampler = _mm_add_epi8(sampler, positive_offset);

    __m128i shifts = _mm_srli_epi16(
        _mm_and_si128(_mm_slli_epi16(left_y, upsample_shift), shift_mask), 1);
    shifts = _mm_packus_epi16(shifts, shifts);
    const __m128i opposite_shifts = _mm_sub_epi8(max_shifts, shifts);
    shifts = _mm_unpacklo_epi8(opposite_shifts, shifts);
    const __m128i vals = DirectionalZone2FromSource_SSE4_1(
        left_column + (y << upsample_shift), shifts, sampler);
    Store4(dst, _mm_packus_epi16(vals, vals));
  }
}

// The height at which a load of 16 bytes will not contain enough source pixels
// from |left_column| to supply an accurate row when computing 8 pixels at a
// time. The values are found by inspection. By coincidence, all angles that
// satisfy (ystep >> 6) == 2 map to the same value, so it is enough to look up
// by ystep >> 6. The largest index for this lookup is 1023 >> 6 == 15.
constexpr int kDirectionalZone2ShuffleInvalidHeight[16] = {
    1024, 1024, 16, 16, 16, 16, 0, 0, 18, 0, 0, 0, 0, 0, 0, 40};

template <bool upsampled>
inline void DirectionalZone2FromLeftCol_8x8_SSE4_1(
    uint8_t* dst, ptrdiff_t stride, const uint8_t* const left_column,
    __m128i left_y) {
  const int upsample_shift = static_cast<int>(upsampled);
  const int scale_bits = 6 - upsample_shift;
  const __m128i max_shifts = _mm_set1_epi8(32);
  const __m128i shift_mask = _mm_set1_epi32(0x003F003F);
  const __m128i index_increment = _mm_set1_epi8(1);
  const __m128i denegation = _mm_set1_epi8(kPositiveIndexOffset);
  for (int y = 0; y < 8; dst += stride, ++y) {
    __m128i offset_y = _mm_srai_epi16(left_y, scale_bits);
    offset_y = _mm_packs_epi16(offset_y, offset_y);
    const __m128i adjacent = _mm_add_epi8(offset_y, index_increment);

    // Offset the relative index because ystep is negative in Zone 2 and shuffle
    // indices must be nonnegative.
    __m128i sampler = _mm_unpacklo_epi8(offset_y, adjacent);
    sampler = _mm_add_epi8(sampler, denegation);

    __m128i shifts = _mm_srli_epi16(
        _mm_and_si128(_mm_slli_epi16(left_y, upsample_shift), shift_mask), 1);
    shifts = _mm_packus_epi16(shifts, shifts);
    const __m128i opposite_shifts = _mm_sub_epi8(max_shifts, shifts);
    shifts = _mm_unpacklo_epi8(opposite_shifts, shifts);

    // The specification adds (y << 6) to left_y, which is subject to
    // upsampling, but this puts sampler indices out of the 0-15 range. It is
    // equivalent to offset the source address by (y << upsample_shift) instead.
    const __m128i vals = DirectionalZone2FromSource_SSE4_1(
        left_column - kPositiveIndexOffset + (y << upsample_shift), shifts,
        sampler);
    StoreLo8(dst, _mm_packus_epi16(vals, vals));
  }
}

// |zone_bounds| is an epi16 of the relative x index at which base >= -(1 <<
// upsampled_top), for each row. When there are 4 values, they can be duplicated
// with a non-register shuffle mask.
// |shifts| is one pair of weights that applies throughout a given row.
template <bool upsampled_top>
inline void DirectionalZone1Blend_4x4(
    uint8_t* dest, const uint8_t* const top_row, ptrdiff_t stride,
    __m128i sampler, const __m128i& zone_bounds, const __m128i& shifts,
    const __m128i& dest_index_x, int top_x, const int xstep) {
  const int upsample_shift = static_cast<int>(upsampled_top);
  const int scale_bits_x = 6 - upsample_shift;
  top_x -= xstep;

  int top_base_x = (top_x >> scale_bits_x);
  const __m128i vals0 = DirectionalZone2FromSource_SSE4_1(
      top_row + top_base_x, _mm_shufflelo_epi16(shifts, 0x00), sampler);
  DirectionalBlend4_SSE4_1<0x00>(dest, dest_index_x, vals0, zone_bounds);
  top_x -= xstep;
  dest += stride;

  top_base_x = (top_x >> scale_bits_x);
  const __m128i vals1 = DirectionalZone2FromSource_SSE4_1(
      top_row + top_base_x, _mm_shufflelo_epi16(shifts, 0x55), sampler);
  DirectionalBlend4_SSE4_1<0x55>(dest, dest_index_x, vals1, zone_bounds);
  top_x -= xstep;
  dest += stride;

  top_base_x = (top_x >> scale_bits_x);
  const __m128i vals2 = DirectionalZone2FromSource_SSE4_1(
      top_row + top_base_x, _mm_shufflelo_epi16(shifts, 0xAA), sampler);
  DirectionalBlend4_SSE4_1<0xAA>(dest, dest_index_x, vals2, zone_bounds);
  top_x -= xstep;
  dest += stride;

  top_base_x = (top_x >> scale_bits_x);
  const __m128i vals3 = DirectionalZone2FromSource_SSE4_1(
      top_row + top_base_x, _mm_shufflelo_epi16(shifts, 0xFF), sampler);
  DirectionalBlend4_SSE4_1<0xFF>(dest, dest_index_x, vals3, zone_bounds);
}

template <bool upsampled_top, int height>
inline void DirectionalZone1Blend_8xH(
    uint8_t* dest, const uint8_t* const top_row, ptrdiff_t stride,
    __m128i sampler, const __m128i& zone_bounds, const __m128i& shifts,
    const __m128i& dest_index_x, int top_x, const int xstep) {
  const int upsample_shift = static_cast<int>(upsampled_top);
  const int scale_bits_x = 6 - upsample_shift;

  __m128i y_selector = _mm_set1_epi32(0x01000100);
  const __m128i index_increment = _mm_set1_epi32(0x02020202);
  for (int y = 0; y < height; ++y,
           y_selector = _mm_add_epi8(y_selector, index_increment),
           dest += stride) {
    top_x -= xstep;
    const int top_base_x = top_x >> scale_bits_x;
    const __m128i vals = DirectionalZone2FromSource_SSE4_1(
        top_row + top_base_x, _mm_shuffle_epi8(shifts, y_selector), sampler);
    DirectionalBlend8_SSE4_1(dest, dest_index_x, vals, zone_bounds, y_selector);
  }
}

// 7.11.2.4 (8) 90 < angle > 180
// The strategy for this function is to know how many blocks can be processed
// with just pixels from |top_ptr|, then handle mixed blocks, then handle only
// blocks that take from |left_ptr|. Additionally, a fast index-shuffle
// approach is used for pred values from |left_column| in sections that permit
// it.
template <bool upsampled_left, bool upsampled_top>
inline void DirectionalZone2_SSE4_1(void* dest, ptrdiff_t stride,
                                    const uint8_t* const top_row,
                                    const uint8_t* const left_column,
                                    const int width, const int height,
                                    const int xstep, const int ystep) {
  auto* dst = static_cast<uint8_t*>(dest);
  const int upsample_left_shift = static_cast<int>(upsampled_left);
  const int upsample_top_shift = static_cast<int>(upsampled_top);
  const __m128i max_shift = _mm_set1_epi8(32);
  const ptrdiff_t stride8 = stride << 3;
  const __m128i dest_index_x =
      _mm_set_epi32(0x00070006, 0x00050004, 0x00030002, 0x00010000);
  const __m128i sampler_top =
      upsampled_top
          ? _mm_set_epi32(0x0F0E0D0C, 0x0B0A0908, 0x07060504, 0x03020100)
          : _mm_set_epi32(0x08070706, 0x06050504, 0x04030302, 0x02010100);
  const __m128i shift_mask = _mm_set1_epi32(0x003F003F);
  // All columns from |min_top_only_x| to the right will only need |top_row| to
  // compute. This assumes minimum |xstep| is 3.
  const int min_top_only_x = std::min((height * xstep) >> 6, width);

  // For steep angles, the source pixels from left_column may not fit in a
  // 16-byte load for shuffling.
  // TODO(petersonab): Find a more precise formula for this subject to x.
  const int max_shuffle_height =
      std::min(height, kDirectionalZone2ShuffleInvalidHeight[ystep >> 6]);

  const int xstep8 = xstep << 3;
  const __m128i xstep8_vect = _mm_set1_epi16(xstep8);
  // Accumulate xstep across 8 rows.
  const __m128i xstep_dup = _mm_set1_epi16(-xstep);
  const __m128i increments = _mm_set_epi16(8, 7, 6, 5, 4, 3, 2, 1);
  const __m128i xstep_for_shift = _mm_mullo_epi16(xstep_dup, increments);
  // Offsets the original zone bound value to simplify x < (y+1)*xstep/64 -1
  const __m128i scaled_one = _mm_set1_epi16(-64);
  __m128i xstep_bounds_base =
      (xstep == 64) ? _mm_sub_epi16(scaled_one, xstep_for_shift)
                    : _mm_sub_epi16(_mm_set1_epi16(-1), xstep_for_shift);

  const int left_base_increment = ystep >> 6;
  const int ystep_remainder = ystep & 0x3F;
  const int ystep8 = ystep << 3;
  const int left_base_increment8 = ystep8 >> 6;
  const int ystep_remainder8 = ystep8 & 0x3F;
  const __m128i increment_left8 = _mm_set1_epi16(-ystep_remainder8);

  // If the 64 scaling is regarded as a decimal point, the first value of the
  // left_y vector omits the portion which is covered under the left_column
  // offset. Following values need the full ystep as a relative offset.
  const __m128i ystep_init = _mm_set1_epi16(-ystep_remainder);
  const __m128i ystep_dup = _mm_set1_epi16(-ystep);
  __m128i left_y = _mm_mullo_epi16(ystep_dup, dest_index_x);
  left_y = _mm_add_epi16(ystep_init, left_y);

  const __m128i increment_top8 = _mm_set1_epi16(8 << 6);
  int x = 0;

  // This loop treats each set of 4 columns in 3 stages with y-value boundaries.
  // The first stage, before the first y-loop, covers blocks that are only
  // computed from the top row. The second stage, comprising two y-loops, covers
  // blocks that have a mixture of values computed from top or left. The final
  // stage covers blocks that are only computed from the left.
  for (int left_offset = -left_base_increment; x < min_top_only_x;
       x += 8,
           xstep_bounds_base = _mm_sub_epi16(xstep_bounds_base, increment_top8),
           // Watch left_y because it can still get big.
       left_y = _mm_add_epi16(left_y, increment_left8),
           left_offset -= left_base_increment8) {
    uint8_t* dst_x = dst + x;

    // Round down to the nearest multiple of 8.
    const int max_top_only_y = std::min(((x + 1) << 6) / xstep, height) & ~7;
    DirectionalZone1_4xH(dst_x, stride, top_row + (x << upsample_top_shift),
                         max_top_only_y, -xstep, upsampled_top);
    DirectionalZone1_4xH(dst_x + 4, stride,
                         top_row + ((x + 4) << upsample_top_shift),
                         max_top_only_y, -xstep, upsampled_top);

    int y = max_top_only_y;
    dst_x += stride * y;
    const int xstep_y = xstep * y;
    const __m128i xstep_y_vect = _mm_set1_epi16(xstep_y);
    // All rows from |min_left_only_y| down for this set of columns, only need
    // |left_column| to compute.
    const int min_left_only_y = std::min(((x + 8) << 6) / xstep, height);
    // At high angles such that min_left_only_y < 8, ystep is low and xstep is
    // high. This means that max_shuffle_height is unbounded and xstep_bounds
    // will overflow in 16 bits. This is prevented by stopping the first
    // blending loop at min_left_only_y for such cases, which means we skip over
    // the second blending loop as well.
    const int left_shuffle_stop_y =
        std::min(max_shuffle_height, min_left_only_y);
    __m128i xstep_bounds = _mm_add_epi16(xstep_bounds_base, xstep_y_vect);
    __m128i xstep_for_shift_y = _mm_sub_epi16(xstep_for_shift, xstep_y_vect);
    int top_x = -xstep_y;

    for (; y < left_shuffle_stop_y;
         y += 8, dst_x += stride8,
         xstep_bounds = _mm_add_epi16(xstep_bounds, xstep8_vect),
         xstep_for_shift_y = _mm_sub_epi16(xstep_for_shift_y, xstep8_vect),
         top_x -= xstep8) {
      DirectionalZone2FromLeftCol_8x8_SSE4_1<upsampled_left>(
          dst_x, stride,
          left_column + ((left_offset + y) << upsample_left_shift), left_y);

      __m128i shifts = _mm_srli_epi16(
          _mm_and_si128(_mm_slli_epi16(xstep_for_shift_y, upsample_top_shift),
                        shift_mask),
          1);
      shifts = _mm_packus_epi16(shifts, shifts);
      __m128i opposite_shifts = _mm_sub_epi8(max_shift, shifts);
      shifts = _mm_unpacklo_epi8(opposite_shifts, shifts);
      __m128i xstep_bounds_off = _mm_srai_epi16(xstep_bounds, 6);
      DirectionalZone1Blend_8xH<upsampled_top, 8>(
          dst_x, top_row + (x << upsample_top_shift), stride, sampler_top,
          xstep_bounds_off, shifts, dest_index_x, top_x, xstep);
    }
    // Pick up from the last y-value, using the 10% slower but secure method for
    // left prediction.
    const auto base_left_y = static_cast<int16_t>(_mm_extract_epi16(left_y, 0));
    for (; y < min_left_only_y;
         y += 8, dst_x += stride8,
         xstep_bounds = _mm_add_epi16(xstep_bounds, xstep8_vect),
         xstep_for_shift_y = _mm_sub_epi16(xstep_for_shift_y, xstep8_vect),
         top_x -= xstep8) {
      const __m128i xstep_bounds_off = _mm_srai_epi16(xstep_bounds, 6);

      DirectionalZone3_8xH<upsampled_left, 8>(
          dst_x, stride,
          left_column + ((left_offset + y) << upsample_left_shift), base_left_y,
          -ystep);

      __m128i shifts = _mm_srli_epi16(
          _mm_and_si128(_mm_slli_epi16(xstep_for_shift_y, upsample_top_shift),
                        shift_mask),
          1);
      shifts = _mm_packus_epi16(shifts, shifts);
      __m128i opposite_shifts = _mm_sub_epi8(max_shift, shifts);
      shifts = _mm_unpacklo_epi8(opposite_shifts, shifts);
      DirectionalZone1Blend_8xH<upsampled_top, 8>(
          dst_x, top_row + (x << upsample_top_shift), stride, sampler_top,
          xstep_bounds_off, shifts, dest_index_x, top_x, xstep);
    }
    // Loop over y for left_only rows.
    for (; y < height; y += 8, dst_x += stride8) {
      DirectionalZone3_8xH<upsampled_left, 8>(
          dst_x, stride,
          left_column + ((left_offset + y) << upsample_left_shift), base_left_y,
          -ystep);
    }
  }
  for (; x < width; x += 4) {
    DirectionalZone1_4xH(dst + x, stride, top_row + (x << upsample_top_shift),
                         height, -xstep, upsampled_top);
  }
}

template <bool upsampled_left, bool upsampled_top>
inline void DirectionalZone2_4_SSE4_1(void* dest, ptrdiff_t stride,
                                      const uint8_t* const top_row,
                                      const uint8_t* const left_column,
                                      const int width, const int height,
                                      const int xstep, const int ystep) {
  auto* dst = static_cast<uint8_t*>(dest);
  const int upsample_left_shift = static_cast<int>(upsampled_left);
  const int upsample_top_shift = static_cast<int>(upsampled_top);
  const __m128i max_shift = _mm_set1_epi8(32);
  const ptrdiff_t stride4 = stride << 2;
  const __m128i dest_index_x = _mm_set_epi32(0, 0, 0x00030002, 0x00010000);
  const __m128i sampler_top =
      upsampled_top
          ? _mm_set_epi32(0x0F0E0D0C, 0x0B0A0908, 0x07060504, 0x03020100)
          : _mm_set_epi32(0x08070706, 0x06050504, 0x04030302, 0x02010100);
  // All columns from |min_top_only_x| to the right will only need |top_row| to
  // compute.
  assert(xstep >= 3);
  const int min_top_only_x = std::min((height * xstep) >> 6, width);

  const int xstep4 = xstep << 2;
  const __m128i xstep4_vect = _mm_set1_epi16(xstep4);
  const __m128i xstep_dup = _mm_set1_epi16(-xstep);
  const __m128i increments = _mm_set_epi32(0, 0, 0x00040003, 0x00020001);
  __m128i xstep_for_shift = _mm_mullo_epi16(xstep_dup, increments);
  const __m128i scaled_one = _mm_set1_epi16(-64);
  // Offsets the original zone bound value to simplify x < (y+1)*xstep/64 -1
  __m128i xstep_bounds_base =
      (xstep == 64) ? _mm_sub_epi16(scaled_one, xstep_for_shift)
                    : _mm_sub_epi16(_mm_set1_epi16(-1), xstep_for_shift);

  const int left_base_increment = ystep >> 6;
  const int ystep_remainder = ystep & 0x3F;
  const int ystep4 = ystep << 2;
  const int left_base_increment4 = ystep4 >> 6;
  // This is guaranteed to be less than 64, but accumulation may bring it past
  // 64 for higher x values.
  const int ystep_remainder4 = ystep4 & 0x3F;
  const __m128i increment_left4 = _mm_set1_epi16(-ystep_remainder4);
  const __m128i increment_top4 = _mm_set1_epi16(4 << 6);

  // If the 64 scaling is regarded as a decimal point, the first value of the
  // left_y vector omits the portion which will go into the left_column offset.
  // Following values need the full ystep as a relative offset.
  const __m128i ystep_init = _mm_set1_epi16(-ystep_remainder);
  const __m128i ystep_dup = _mm_set1_epi16(-ystep);
  __m128i left_y = _mm_mullo_epi16(ystep_dup, dest_index_x);
  left_y = _mm_add_epi16(ystep_init, left_y);
  const __m128i shift_mask = _mm_set1_epi32(0x003F003F);

  int x = 0;
  // Loop over x for columns with a mixture of sources.
  for (int left_offset = -left_base_increment; x < min_top_only_x; x += 4,
           xstep_bounds_base = _mm_sub_epi16(xstep_bounds_base, increment_top4),
           left_y = _mm_add_epi16(left_y, increment_left4),
           left_offset -= left_base_increment4) {
    uint8_t* dst_x = dst + x;

    // Round down to the nearest multiple of 8.
    const int max_top_only_y = std::min((x << 6) / xstep, height) & 0xFFFFFFF4;
    DirectionalZone1_4xH(dst_x, stride, top_row + (x << upsample_top_shift),
                         max_top_only_y, -xstep, upsampled_top);
    int y = max_top_only_y;
    dst_x += stride * y;
    const int xstep_y = xstep * y;
    const __m128i xstep_y_vect = _mm_set1_epi16(xstep_y);
    // All rows from |min_left_only_y| down for this set of columns, only need
    // |left_column| to compute. Rounded up to the nearest multiple of 4.
    const int min_left_only_y = std::min(((x + 4) << 6) / xstep, height);

    __m128i xstep_bounds = _mm_add_epi16(xstep_bounds_base, xstep_y_vect);
    __m128i xstep_for_shift_y = _mm_sub_epi16(xstep_for_shift, xstep_y_vect);
    int top_x = -xstep_y;

    // Loop over y for mixed rows.
    for (; y < min_left_only_y;
         y += 4, dst_x += stride4,
         xstep_bounds = _mm_add_epi16(xstep_bounds, xstep4_vect),
         xstep_for_shift_y = _mm_sub_epi16(xstep_for_shift_y, xstep4_vect),
         top_x -= xstep4) {
      DirectionalZone2FromLeftCol_4x4_SSE4_1<upsampled_left>(
          dst_x, stride,
          left_column + ((left_offset + y) * (1 << upsample_left_shift)),
          left_y);

      __m128i shifts = _mm_srli_epi16(
          _mm_and_si128(_mm_slli_epi16(xstep_for_shift_y, upsample_top_shift),
                        shift_mask),
          1);
      shifts = _mm_packus_epi16(shifts, shifts);
      const __m128i opposite_shifts = _mm_sub_epi8(max_shift, shifts);
      shifts = _mm_unpacklo_epi8(opposite_shifts, shifts);
      const __m128i xstep_bounds_off = _mm_srai_epi16(xstep_bounds, 6);
      DirectionalZone1Blend_4x4<upsampled_top>(
          dst_x, top_row + (x << upsample_top_shift), stride, sampler_top,
          xstep_bounds_off, shifts, dest_index_x, top_x, xstep);
    }
    // Loop over y for left-only rows, if any.
    for (; y < height; y += 4, dst_x += stride4) {
      DirectionalZone2FromLeftCol_4x4_SSE4_1<upsampled_left>(
          dst_x, stride,
          left_column + ((left_offset + y) << upsample_left_shift), left_y);
    }
  }
  // Loop over top-only columns, if any.
  for (; x < width; x += 4) {
    DirectionalZone1_4xH(dst + x, stride, top_row + (x << upsample_top_shift),
                         height, -xstep, upsampled_top);
  }
}

void DirectionalIntraPredictorZone2_SSE4_1(void* const dest, ptrdiff_t stride,
                                           const void* const top_row,
                                           const void* const left_column,
                                           const int width, const int height,
                                           const int xstep, const int ystep,
                                           const bool upsampled_top,
                                           const bool upsampled_left) {
  // Increasing the negative buffer for this function allows more rows to be
  // processed at a time without branching in an inner loop to check the base.
  uint8_t top_buffer[288];
  uint8_t left_buffer[288];
  memcpy(top_buffer + 128, static_cast<const uint8_t*>(top_row) - 16, 160);
  memcpy(left_buffer + 128, static_cast<const uint8_t*>(left_column) - 16, 160);
  const uint8_t* top_ptr = top_buffer + 144;
  const uint8_t* left_ptr = left_buffer + 144;
  if (width == 4 || height == 4) {
    if (upsampled_left) {
      if (upsampled_top) {
        DirectionalZone2_4_SSE4_1<true, true>(dest, stride, top_ptr, left_ptr,
                                              width, height, xstep, ystep);
      } else {
        DirectionalZone2_4_SSE4_1<true, false>(dest, stride, top_ptr, left_ptr,
                                               width, height, xstep, ystep);
      }
    } else {
      if (upsampled_top) {
        DirectionalZone2_4_SSE4_1<false, true>(dest, stride, top_ptr, left_ptr,
                                               width, height, xstep, ystep);
      } else {
        DirectionalZone2_4_SSE4_1<false, false>(dest, stride, top_ptr, left_ptr,
                                                width, height, xstep, ystep);
      }
    }
    return;
  }
  if (upsampled_left) {
    if (upsampled_top) {
      DirectionalZone2_SSE4_1<true, true>(dest, stride, top_ptr, left_ptr,
                                          width, height, xstep, ystep);
    } else {
      DirectionalZone2_SSE4_1<true, false>(dest, stride, top_ptr, left_ptr,
                                           width, height, xstep, ystep);
    }
  } else {
    if (upsampled_top) {
      DirectionalZone2_SSE4_1<false, true>(dest, stride, top_ptr, left_ptr,
                                           width, height, xstep, ystep);
    } else {
      DirectionalZone2_SSE4_1<false, false>(dest, stride, top_ptr, left_ptr,
                                            width, height, xstep, ystep);
    }
  }
}

//------------------------------------------------------------------------------
// FilterIntraPredictor_SSE4_1

// Apply all filter taps to the given 7 packed 16-bit values, keeping the 8th
// at zero to preserve the sum.
inline void Filter4x2_SSE4_1(uint8_t* dst, const ptrdiff_t stride,
                             const __m128i& pixels, const __m128i& taps_0_1,
                             const __m128i& taps_2_3, const __m128i& taps_4_5,
                             const __m128i& taps_6_7) {
  const __m128i mul_0_01 = _mm_maddubs_epi16(pixels, taps_0_1);
  const __m128i mul_0_23 = _mm_maddubs_epi16(pixels, taps_2_3);
  // |output_half| contains 8 partial sums.
  __m128i output_half = _mm_hadd_epi16(mul_0_01, mul_0_23);
  __m128i output = _mm_hadd_epi16(output_half, output_half);
  const __m128i output_row0 =
      _mm_packus_epi16(RightShiftWithRounding_S16(output, 4),
                       /* arbitrary pack arg */ output);
  Store4(dst, output_row0);
  const __m128i mul_1_01 = _mm_maddubs_epi16(pixels, taps_4_5);
  const __m128i mul_1_23 = _mm_maddubs_epi16(pixels, taps_6_7);
  output_half = _mm_hadd_epi16(mul_1_01, mul_1_23);
  output = _mm_hadd_epi16(output_half, output_half);
  const __m128i output_row1 =
      _mm_packus_epi16(RightShiftWithRounding_S16(output, 4),
                       /* arbitrary pack arg */ output);
  Store4(dst + stride, output_row1);
}

// 4xH transform sizes are given special treatment because LoadLo8 goes out
// of bounds and every block involves the left column. This implementation
// loads TL from the top row for the first block, so it is not
inline void Filter4xH(uint8_t* dest, ptrdiff_t stride,
                      const uint8_t* const top_ptr,
                      const uint8_t* const left_ptr, FilterIntraPredictor pred,
                      const int height) {
  const __m128i taps_0_1 = LoadUnaligned16(kFilterIntraTaps[pred][0]);
  const __m128i taps_2_3 = LoadUnaligned16(kFilterIntraTaps[pred][2]);
  const __m128i taps_4_5 = LoadUnaligned16(kFilterIntraTaps[pred][4]);
  const __m128i taps_6_7 = LoadUnaligned16(kFilterIntraTaps[pred][6]);
  __m128i top = Load4(top_ptr - 1);
  __m128i pixels = _mm_insert_epi8(top, top_ptr[3], 4);
  __m128i left = (height == 4 ? Load4(left_ptr) : LoadLo8(left_ptr));
  left = _mm_slli_si128(left, 5);

  // Relative pixels: top[-1], top[0], top[1], top[2], top[3], left[0], left[1],
  // left[2], left[3], left[4], left[5], left[6], left[7]
  pixels = _mm_or_si128(left, pixels);

  // Duplicate first 8 bytes.
  pixels = _mm_shuffle_epi32(pixels, kDuplicateFirstHalf);
  Filter4x2_SSE4_1(dest, stride, pixels, taps_0_1, taps_2_3, taps_4_5,
                   taps_6_7);
  dest += stride;  // Move to y = 1.
  pixels = Load4(dest);

  // Relative pixels: top[0], top[1], top[2], top[3], empty, left[-2], left[-1],
  // left[0], left[1], ...
  pixels = _mm_or_si128(left, pixels);

  // This mask rearranges bytes in the order: 6, 0, 1, 2, 3, 7, 8, 15. The last
  // byte is an unused value, which shall be multiplied by 0 when we apply the
  // filter.
  constexpr int64_t kInsertTopLeftFirstMask = 0x0F08070302010006;

  // Insert left[-1] in front as TL and put left[0] and left[1] at the end.
  const __m128i pixel_order1 = _mm_set1_epi64x(kInsertTopLeftFirstMask);
  pixels = _mm_shuffle_epi8(pixels, pixel_order1);
  dest += stride;  // Move to y = 2.
  Filter4x2_SSE4_1(dest, stride, pixels, taps_0_1, taps_2_3, taps_4_5,
                   taps_6_7);
  dest += stride;  // Move to y = 3.

  // Compute the middle 8 rows before using common code for the final 4 rows.
  // Because the common code below this block assumes that
  if (height == 16) {
    // This shift allows us to use pixel_order2 twice after shifting by 2 later.
    left = _mm_slli_si128(left, 1);
    pixels = Load4(dest);

    // Relative pixels: top[0], top[1], top[2], top[3], empty, empty, left[-4],
    // left[-3], left[-2], left[-1], left[0], left[1], left[2], left[3]
    pixels = _mm_or_si128(left, pixels);

    // This mask rearranges bytes in the order: 9, 0, 1, 2, 3, 7, 8, 15. The
    // last byte is an unused value, as above. The top-left was shifted to
    // position nine to keep two empty spaces after the top pixels.
    constexpr int64_t kInsertTopLeftSecondMask = 0x0F0B0A0302010009;

    // Insert (relative) left[-1] in front as TL and put left[0] and left[1] at
    // the end.
    const __m128i pixel_order2 = _mm_set1_epi64x(kInsertTopLeftSecondMask);
    pixels = _mm_shuffle_epi8(pixels, pixel_order2);
    dest += stride;  // Move to y = 4.

    // First 4x2 in the if body.
    Filter4x2_SSE4_1(dest, stride, pixels, taps_0_1, taps_2_3, taps_4_5,
                     taps_6_7);

    // Clear all but final pixel in the first 8 of left column.
    __m128i keep_top_left = _mm_srli_si128(left, 13);
    dest += stride;  // Move to y = 5.
    pixels = Load4(dest);
    left = _mm_srli_si128(left, 2);

    // Relative pixels: top[0], top[1], top[2], top[3], left[-6],
    // left[-5], left[-4], left[-3], left[-2], left[-1], left[0], left[1]
    pixels = _mm_or_si128(left, pixels);
    left = LoadLo8(left_ptr + 8);

    pixels = _mm_shuffle_epi8(pixels, pixel_order2);
    dest += stride;  // Move to y = 6.

    // Second 4x2 in the if body.
    Filter4x2_SSE4_1(dest, stride, pixels, taps_0_1, taps_2_3, taps_4_5,
                     taps_6_7);

    // Position TL value so we can use pixel_order1.
    keep_top_left = _mm_slli_si128(keep_top_left, 6);
    dest += stride;  // Move to y = 7.
    pixels = Load4(dest);
    left = _mm_slli_si128(left, 7);
    left = _mm_or_si128(left, keep_top_left);

    // Relative pixels: top[0], top[1], top[2], top[3], empty, empty,
    // left[-1], left[0], left[1], left[2], left[3], ...
    pixels = _mm_or_si128(left, pixels);
    pixels = _mm_shuffle_epi8(pixels, pixel_order1);
    dest += stride;  // Move to y = 8.

    // Third 4x2 in the if body.
    Filter4x2_SSE4_1(dest, stride, pixels, taps_0_1, taps_2_3, taps_4_5,
                     taps_6_7);
    dest += stride;  // Move to y = 9.

    // Prepare final inputs.
    pixels = Load4(dest);
    left = _mm_srli_si128(left, 2);

    // Relative pixels: top[0], top[1], top[2], top[3], left[-3], left[-2]
    // left[-1], left[0], left[1], left[2], left[3], ...
    pixels = _mm_or_si128(left, pixels);
    pixels = _mm_shuffle_epi8(pixels, pixel_order1);
    dest += stride;  // Move to y = 10.

    // Fourth 4x2 in the if body.
    Filter4x2_SSE4_1(dest, stride, pixels, taps_0_1, taps_2_3, taps_4_5,
                     taps_6_7);
    dest += stride;  // Move to y = 11.
  }

  // In both the 8 and 16 case, we assume that the left vector has the next TL
  // at position 8.
  if (height > 4) {
    // Erase prior left pixels by shifting TL to position 0.
    left = _mm_srli_si128(left, 8);
    left = _mm_slli_si128(left, 6);
    pixels = Load4(dest);

    // Relative pixels: top[0], top[1], top[2], top[3], empty, empty,
    // left[-1], left[0], left[1], left[2], left[3], ...
    pixels = _mm_or_si128(left, pixels);
    pixels = _mm_shuffle_epi8(pixels, pixel_order1);
    dest += stride;  // Move to y = 12 or 4.

    // First of final two 4x2 blocks.
    Filter4x2_SSE4_1(dest, stride, pixels, taps_0_1, taps_2_3, taps_4_5,
                     taps_6_7);
    dest += stride;  // Move to y = 13 or 5.
    pixels = Load4(dest);
    left = _mm_srli_si128(left, 2);

    // Relative pixels: top[0], top[1], top[2], top[3], left[-3], left[-2]
    // left[-1], left[0], left[1], left[2], left[3], ...
    pixels = _mm_or_si128(left, pixels);
    pixels = _mm_shuffle_epi8(pixels, pixel_order1);
    dest += stride;  // Move to y = 14 or 6.

    // Last of final two 4x2 blocks.
    Filter4x2_SSE4_1(dest, stride, pixels, taps_0_1, taps_2_3, taps_4_5,
                     taps_6_7);
  }
}

void FilterIntraPredictor_SSE4_1(void* const dest, ptrdiff_t stride,
                                 const void* const top_row,
                                 const void* const left_column,
                                 FilterIntraPredictor pred, const int width,
                                 const int height) {
  const auto* const top_ptr = static_cast<const uint8_t*>(top_row);
  const auto* const left_ptr = static_cast<const uint8_t*>(left_column);
  auto* dst = static_cast<uint8_t*>(dest);
  if (width == 4) {
    Filter4xH(dst, stride, top_ptr, left_ptr, pred, height);
    return;
  }

  // There is one set of 7 taps for each of the 4x2 output pixels.
  const __m128i taps_0_1 = LoadUnaligned16(kFilterIntraTaps[pred][0]);
  const __m128i taps_2_3 = LoadUnaligned16(kFilterIntraTaps[pred][2]);
  const __m128i taps_4_5 = LoadUnaligned16(kFilterIntraTaps[pred][4]);
  const __m128i taps_6_7 = LoadUnaligned16(kFilterIntraTaps[pred][6]);

  // This mask rearranges bytes in the order: 0, 1, 2, 3, 4, 8, 9, 15. The 15 at
  // the end is an unused value, which shall be multiplied by 0 when we apply
  // the filter.
  constexpr int64_t kCondenseLeftMask = 0x0F09080403020100;

  // Takes the "left section" and puts it right after p0-p4.
  const __m128i pixel_order1 = _mm_set1_epi64x(kCondenseLeftMask);

  // This mask rearranges bytes in the order: 8, 0, 1, 2, 3, 9, 10, 15. The last
  // byte is unused as above.
  constexpr int64_t kInsertTopLeftMask = 0x0F0A090302010008;

  // Shuffles the "top left" from the left section, to the front. Used when
  // grabbing data from left_column and not top_row.
  const __m128i pixel_order2 = _mm_set1_epi64x(kInsertTopLeftMask);

  // This first pass takes care of the cases where the top left pixel comes from
  // top_row.
  __m128i pixels = LoadLo8(top_ptr - 1);
  __m128i left = _mm_slli_si128(Load4(left_column), 8);
  pixels = _mm_or_si128(pixels, left);

  // Two sets of the same pixels to multiply with two sets of taps.
  pixels = _mm_shuffle_epi8(pixels, pixel_order1);
  Filter4x2_SSE4_1(dst, stride, pixels, taps_0_1, taps_2_3, taps_4_5, taps_6_7);
  left = _mm_srli_si128(left, 1);

  // Load
  pixels = Load4(dst + stride);

  // Because of the above shift, this OR 'invades' the final of the first 8
  // bytes of |pixels|. This is acceptable because the 8th filter tap is always
  // a padded 0.
  pixels = _mm_or_si128(pixels, left);
  pixels = _mm_shuffle_epi8(pixels, pixel_order2);
  const ptrdiff_t stride2 = stride << 1;
  const ptrdiff_t stride4 = stride << 2;
  Filter4x2_SSE4_1(dst + stride2, stride, pixels, taps_0_1, taps_2_3, taps_4_5,
                   taps_6_7);
  dst += 4;
  for (int x = 3; x < width - 4; x += 4) {
    pixels = Load4(top_ptr + x);
    pixels = _mm_insert_epi8(pixels, top_ptr[x + 4], 4);
    pixels = _mm_insert_epi8(pixels, dst[-1], 5);
    pixels = _mm_insert_epi8(pixels, dst[stride - 1], 6);

    // Duplicate bottom half into upper half.
    pixels = _mm_shuffle_epi32(pixels, kDuplicateFirstHalf);
    Filter4x2_SSE4_1(dst, stride, pixels, taps_0_1, taps_2_3, taps_4_5,
                     taps_6_7);
    pixels = Load4(dst + stride - 1);
    pixels = _mm_insert_epi8(pixels, dst[stride + 3], 4);
    pixels = _mm_insert_epi8(pixels, dst[stride2 - 1], 5);
    pixels = _mm_insert_epi8(pixels, dst[stride + stride2 - 1], 6);

    // Duplicate bottom half into upper half.
    pixels = _mm_shuffle_epi32(pixels, kDuplicateFirstHalf);
    Filter4x2_SSE4_1(dst + stride2, stride, pixels, taps_0_1, taps_2_3,
                     taps_4_5, taps_6_7);
    dst += 4;
  }

  // Now we handle heights that reference previous blocks rather than top_row.
  for (int y = 4; y < height; y += 4) {
    // Leftmost 4x4 block for this height.
    dst -= width;
    dst += stride4;

    // Top Left is not available by offset in these leftmost blocks.
    pixels = Load4(dst - stride);
    left = _mm_slli_si128(Load4(left_ptr + y - 1), 8);
    left = _mm_insert_epi8(left, left_ptr[y + 3], 12);
    pixels = _mm_or_si128(pixels, left);
    pixels = _mm_shuffle_epi8(pixels, pixel_order2);
    Filter4x2_SSE4_1(dst, stride, pixels, taps_0_1, taps_2_3, taps_4_5,
                     taps_6_7);

    // The bytes shifted into positions 6 and 7 will be ignored by the shuffle.
    left = _mm_srli_si128(left, 2);
    pixels = Load4(dst + stride);
    pixels = _mm_or_si128(pixels, left);
    pixels = _mm_shuffle_epi8(pixels, pixel_order2);
    Filter4x2_SSE4_1(dst + stride2, stride, pixels, taps_0_1, taps_2_3,
                     taps_4_5, taps_6_7);

    dst += 4;

    // Remaining 4x4 blocks for this height.
    for (int x = 4; x < width; x += 4) {
      pixels = Load4(dst - stride - 1);
      pixels = _mm_insert_epi8(pixels, dst[-stride + 3], 4);
      pixels = _mm_insert_epi8(pixels, dst[-1], 5);
      pixels = _mm_insert_epi8(pixels, dst[stride - 1], 6);

      // Duplicate bottom half into upper half.
      pixels = _mm_shuffle_epi32(pixels, kDuplicateFirstHalf);
      Filter4x2_SSE4_1(dst, stride, pixels, taps_0_1, taps_2_3, taps_4_5,
                       taps_6_7);
      pixels = Load4(dst + stride - 1);
      pixels = _mm_insert_epi8(pixels, dst[stride + 3], 4);
      pixels = _mm_insert_epi8(pixels, dst[stride2 - 1], 5);
      pixels = _mm_insert_epi8(pixels, dst[stride2 + stride - 1], 6);

      // Duplicate bottom half into upper half.
      pixels = _mm_shuffle_epi32(pixels, kDuplicateFirstHalf);
      Filter4x2_SSE4_1(dst + stride2, stride, pixels, taps_0_1, taps_2_3,
                       taps_4_5, taps_6_7);
      dst += 4;
    }
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
  static_cast<void>(dsp);
// These guards check if this version of the function was not superseded by
// a higher optimization level, such as AVX. The corresponding #define also
// prevents the C version from being added to the table.
#if DSP_ENABLED_8BPP_SSE4_1(FilterIntraPredictor)
  dsp->filter_intra_predictor = FilterIntraPredictor_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(DirectionalIntraPredictorZone1)
  dsp->directional_intra_predictor_zone1 =
      DirectionalIntraPredictorZone1_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(DirectionalIntraPredictorZone2)
  dsp->directional_intra_predictor_zone2 =
      DirectionalIntraPredictorZone2_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(DirectionalIntraPredictorZone3)
  dsp->directional_intra_predictor_zone3 =
      DirectionalIntraPredictorZone3_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x4_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDcTop] =
      DcDefs::_4x4::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x8_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorDcTop] =
      DcDefs::_4x8::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x16_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorDcTop] =
      DcDefs::_4x16::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x4_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorDcTop] =
      DcDefs::_8x4::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x8_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorDcTop] =
      DcDefs::_8x8::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x16_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorDcTop] =
      DcDefs::_8x16::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x32_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorDcTop] =
      DcDefs::_8x32::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x4_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorDcTop] =
      DcDefs::_16x4::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x8_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorDcTop] =
      DcDefs::_16x8::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x16_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorDcTop] =
      DcDefs::_16x16::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x32_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorDcTop] =
      DcDefs::_16x32::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x64_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorDcTop] =
      DcDefs::_16x64::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x8_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorDcTop] =
      DcDefs::_32x8::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x16_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorDcTop] =
      DcDefs::_32x16::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x32_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorDcTop] =
      DcDefs::_32x32::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x64_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorDcTop] =
      DcDefs::_32x64::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x16_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorDcTop] =
      DcDefs::_64x16::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x32_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorDcTop] =
      DcDefs::_64x32::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x64_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorDcTop] =
      DcDefs::_64x64::DcTop;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x4_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDcLeft] =
      DcDefs::_4x4::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x8_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorDcLeft] =
      DcDefs::_4x8::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x16_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorDcLeft] =
      DcDefs::_4x16::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x4_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorDcLeft] =
      DcDefs::_8x4::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x8_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorDcLeft] =
      DcDefs::_8x8::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x16_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorDcLeft] =
      DcDefs::_8x16::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x32_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorDcLeft] =
      DcDefs::_8x32::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x4_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorDcLeft] =
      DcDefs::_16x4::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x8_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorDcLeft] =
      DcDefs::_16x8::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x16_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorDcLeft] =
      DcDefs::_16x16::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x32_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorDcLeft] =
      DcDefs::_16x32::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x64_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorDcLeft] =
      DcDefs::_16x64::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x8_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorDcLeft] =
      DcDefs::_32x8::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x16_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorDcLeft] =
      DcDefs::_32x16::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x32_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorDcLeft] =
      DcDefs::_32x32::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x64_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorDcLeft] =
      DcDefs::_32x64::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x16_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorDcLeft] =
      DcDefs::_64x16::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x32_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorDcLeft] =
      DcDefs::_64x32::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x64_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorDcLeft] =
      DcDefs::_64x64::DcLeft;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x4_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDc] =
      DcDefs::_4x4::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x8_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorDc] =
      DcDefs::_4x8::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x16_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorDc] =
      DcDefs::_4x16::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x4_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorDc] =
      DcDefs::_8x4::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x8_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorDc] =
      DcDefs::_8x8::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x16_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorDc] =
      DcDefs::_8x16::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x32_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorDc] =
      DcDefs::_8x32::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x4_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorDc] =
      DcDefs::_16x4::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x8_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorDc] =
      DcDefs::_16x8::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x16_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorDc] =
      DcDefs::_16x16::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x32_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorDc] =
      DcDefs::_16x32::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x64_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorDc] =
      DcDefs::_16x64::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x8_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorDc] =
      DcDefs::_32x8::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x16_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorDc] =
      DcDefs::_32x16::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x32_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorDc] =
      DcDefs::_32x32::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x64_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorDc] =
      DcDefs::_32x64::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x16_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorDc] =
      DcDefs::_64x16::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x32_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorDc] =
      DcDefs::_64x32::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x64_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorDc] =
      DcDefs::_64x64::Dc;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x4_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorPaeth] =
      Paeth4x4_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x8_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorPaeth] =
      Paeth4x8_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x16_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorPaeth] =
      Paeth4x16_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x4_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorPaeth] =
      Paeth8x4_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x8_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorPaeth] =
      Paeth8x8_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x16_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorPaeth] =
      Paeth8x16_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x32_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorPaeth] =
      Paeth8x32_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x4_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorPaeth] =
      Paeth16x4_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x8_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorPaeth] =
      Paeth16x8_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x16_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorPaeth] =
      Paeth16x16_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x32_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorPaeth] =
      Paeth16x32_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x64_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorPaeth] =
      Paeth16x64_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x8_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorPaeth] =
      Paeth32x8_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x16_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorPaeth] =
      Paeth32x16_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x32_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorPaeth] =
      Paeth32x32_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x64_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorPaeth] =
      Paeth32x64_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x16_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorPaeth] =
      Paeth64x16_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x32_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorPaeth] =
      Paeth64x32_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x64_IntraPredictorPaeth)
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorPaeth] =
      Paeth64x64_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x4_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorHorizontal] =
      DirDefs::_4x4::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x8_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorHorizontal] =
      DirDefs::_4x8::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x16_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorHorizontal] =
      DirDefs::_4x16::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x4_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorHorizontal] =
      DirDefs::_8x4::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x8_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorHorizontal] =
      DirDefs::_8x8::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x16_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorHorizontal] =
      DirDefs::_8x16::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x32_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorHorizontal] =
      DirDefs::_8x32::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x4_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorHorizontal] =
      DirDefs::_16x4::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x8_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorHorizontal] =
      DirDefs::_16x8::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x16_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorHorizontal] =
      DirDefs::_16x16::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x32_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorHorizontal] =
      DirDefs::_16x32::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x64_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorHorizontal] =
      DirDefs::_16x64::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x8_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorHorizontal] =
      DirDefs::_32x8::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x16_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorHorizontal] =
      DirDefs::_32x16::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x32_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorHorizontal] =
      DirDefs::_32x32::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x64_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorHorizontal] =
      DirDefs::_32x64::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x16_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorHorizontal] =
      DirDefs::_64x16::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x32_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorHorizontal] =
      DirDefs::_64x32::Horizontal;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize64x64_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorHorizontal] =
      DirDefs::_64x64::Horizontal;
#endif
}  // NOLINT(readability/fn_size)
// TODO(petersonab): Split Init8bpp function into family-specific files.

}  // namespace
}  // namespace low_bitdepth

//------------------------------------------------------------------------------
#if LIBGAV1_MAX_BITDEPTH >= 10
namespace high_bitdepth {
namespace {

template <int height>
inline void DcStore4xH_SSE4_1(void* const dest, ptrdiff_t stride,
                              const __m128i dc) {
  const __m128i dc_dup = _mm_shufflelo_epi16(dc, 0);
  int y = height - 1;
  auto* dst = static_cast<uint8_t*>(dest);
  do {
    StoreLo8(dst, dc_dup);
    dst += stride;
  } while (--y != 0);
  StoreLo8(dst, dc_dup);
}

// WriteDuplicateN assumes dup has 4 32-bit "units," each of which comprises 2
// identical shorts that need N total copies written into dest. The unpacking
// works the same as in the 8bpp case, except that each 32-bit unit needs twice
// as many copies.
inline void WriteDuplicate4x4(void* const dest, ptrdiff_t stride,
                              const __m128i dup32) {
  const __m128i dup64_lo = _mm_unpacklo_epi32(dup32, dup32);
  auto* dst = static_cast<uint8_t*>(dest);
  _mm_storel_epi64(reinterpret_cast<__m128i*>(dst), dup64_lo);
  dst += stride;
  _mm_storeh_pi(reinterpret_cast<__m64*>(dst), _mm_castsi128_ps(dup64_lo));
  dst += stride;
  const __m128i dup64_hi = _mm_unpackhi_epi32(dup32, dup32);
  _mm_storel_epi64(reinterpret_cast<__m128i*>(dst), dup64_hi);
  dst += stride;
  _mm_storeh_pi(reinterpret_cast<__m64*>(dst), _mm_castsi128_ps(dup64_hi));
}

inline void WriteDuplicate8x4(void* const dest, ptrdiff_t stride,
                              const __m128i dup32) {
  const __m128i dup64_lo = _mm_unpacklo_epi32(dup32, dup32);
  const __m128i dup64_hi = _mm_unpackhi_epi32(dup32, dup32);

  auto* dst = static_cast<uint8_t*>(dest);
  const __m128i dup128_0 = _mm_unpacklo_epi64(dup64_lo, dup64_lo);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_0);
  dst += stride;
  const __m128i dup128_1 = _mm_unpackhi_epi64(dup64_lo, dup64_lo);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_1);
  dst += stride;
  const __m128i dup128_2 = _mm_unpacklo_epi64(dup64_hi, dup64_hi);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_2);
  dst += stride;
  const __m128i dup128_3 = _mm_unpackhi_epi64(dup64_hi, dup64_hi);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_3);
}

inline void WriteDuplicate16x4(void* const dest, ptrdiff_t stride,
                               const __m128i dup32) {
  const __m128i dup64_lo = _mm_unpacklo_epi32(dup32, dup32);
  const __m128i dup64_hi = _mm_unpackhi_epi32(dup32, dup32);

  auto* dst = static_cast<uint8_t*>(dest);
  const __m128i dup128_0 = _mm_unpacklo_epi64(dup64_lo, dup64_lo);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_0);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_0);
  dst += stride;
  const __m128i dup128_1 = _mm_unpackhi_epi64(dup64_lo, dup64_lo);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_1);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_1);
  dst += stride;
  const __m128i dup128_2 = _mm_unpacklo_epi64(dup64_hi, dup64_hi);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_2);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_2);
  dst += stride;
  const __m128i dup128_3 = _mm_unpackhi_epi64(dup64_hi, dup64_hi);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_3);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_3);
}

inline void WriteDuplicate32x4(void* const dest, ptrdiff_t stride,
                               const __m128i dup32) {
  const __m128i dup64_lo = _mm_unpacklo_epi32(dup32, dup32);
  const __m128i dup64_hi = _mm_unpackhi_epi32(dup32, dup32);

  auto* dst = static_cast<uint8_t*>(dest);
  const __m128i dup128_0 = _mm_unpacklo_epi64(dup64_lo, dup64_lo);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_0);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_0);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), dup128_0);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), dup128_0);
  dst += stride;
  const __m128i dup128_1 = _mm_unpackhi_epi64(dup64_lo, dup64_lo);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_1);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_1);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), dup128_1);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), dup128_1);
  dst += stride;
  const __m128i dup128_2 = _mm_unpacklo_epi64(dup64_hi, dup64_hi);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_2);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_2);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), dup128_2);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), dup128_2);
  dst += stride;
  const __m128i dup128_3 = _mm_unpackhi_epi64(dup64_hi, dup64_hi);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), dup128_3);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), dup128_3);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), dup128_3);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), dup128_3);
}

inline void WriteDuplicate64x4(void* const dest, ptrdiff_t stride,
                               const __m128i dup32) {
  const __m128i dup64_lo = _mm_unpacklo_epi32(dup32, dup32);
  const __m128i dup64_hi = _mm_unpackhi_epi32(dup32, dup32);

  auto* dst = static_cast<uint8_t*>(dest);
  const __m128i dup128_0 = _mm_unpacklo_epi64(dup64_lo, dup64_lo);
  for (int x = 0; x < 128; x += 16) {
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + x), dup128_0);
  }
  dst += stride;
  const __m128i dup128_1 = _mm_unpackhi_epi64(dup64_lo, dup64_lo);
  for (int x = 0; x < 128; x += 16) {
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + x), dup128_1);
  }
  dst += stride;
  const __m128i dup128_2 = _mm_unpacklo_epi64(dup64_hi, dup64_hi);
  for (int x = 0; x < 128; x += 16) {
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + x), dup128_2);
  }
  dst += stride;
  const __m128i dup128_3 = _mm_unpackhi_epi64(dup64_hi, dup64_hi);
  for (int x = 0; x < 128; x += 16) {
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + x), dup128_3);
  }
}

// ColStoreN<height> copies each of the |height| values in |column| across its
// corresponding row in dest.
template <WriteDuplicateFunc writefn>
inline void ColStore4_SSE4_1(void* const dest, ptrdiff_t stride,
                             const void* const column) {
  const __m128i col_data = LoadLo8(column);
  const __m128i col_dup32 = _mm_unpacklo_epi16(col_data, col_data);
  writefn(dest, stride, col_dup32);
}

template <WriteDuplicateFunc writefn>
inline void ColStore8_SSE4_1(void* const dest, ptrdiff_t stride,
                             const void* const column) {
  const __m128i col_data = LoadUnaligned16(column);
  const __m128i col_dup32_lo = _mm_unpacklo_epi16(col_data, col_data);
  const __m128i col_dup32_hi = _mm_unpackhi_epi16(col_data, col_data);
  auto* dst = static_cast<uint8_t*>(dest);
  writefn(dst, stride, col_dup32_lo);
  const ptrdiff_t stride4 = stride << 2;
  dst += stride4;
  writefn(dst, stride, col_dup32_hi);
}

template <WriteDuplicateFunc writefn>
inline void ColStore16_SSE4_1(void* const dest, ptrdiff_t stride,
                              const void* const column) {
  const ptrdiff_t stride4 = stride << 2;
  auto* dst = static_cast<uint8_t*>(dest);
  for (int y = 0; y < 32; y += 16) {
    const __m128i col_data =
        LoadUnaligned16(static_cast<const uint8_t*>(column) + y);
    const __m128i col_dup32_lo = _mm_unpacklo_epi16(col_data, col_data);
    const __m128i col_dup32_hi = _mm_unpackhi_epi16(col_data, col_data);
    writefn(dst, stride, col_dup32_lo);
    dst += stride4;
    writefn(dst, stride, col_dup32_hi);
    dst += stride4;
  }
}

template <WriteDuplicateFunc writefn>
inline void ColStore32_SSE4_1(void* const dest, ptrdiff_t stride,
                              const void* const column) {
  const ptrdiff_t stride4 = stride << 2;
  auto* dst = static_cast<uint8_t*>(dest);
  for (int y = 0; y < 64; y += 16) {
    const __m128i col_data =
        LoadUnaligned16(static_cast<const uint8_t*>(column) + y);
    const __m128i col_dup32_lo = _mm_unpacklo_epi16(col_data, col_data);
    const __m128i col_dup32_hi = _mm_unpackhi_epi16(col_data, col_data);
    writefn(dst, stride, col_dup32_lo);
    dst += stride4;
    writefn(dst, stride, col_dup32_hi);
    dst += stride4;
  }
}

template <WriteDuplicateFunc writefn>
inline void ColStore64_SSE4_1(void* const dest, ptrdiff_t stride,
                              const void* const column) {
  const ptrdiff_t stride4 = stride << 2;
  auto* dst = static_cast<uint8_t*>(dest);
  for (int y = 0; y < 128; y += 16) {
    const __m128i col_data =
        LoadUnaligned16(static_cast<const uint8_t*>(column) + y);
    const __m128i col_dup32_lo = _mm_unpacklo_epi16(col_data, col_data);
    const __m128i col_dup32_hi = _mm_unpackhi_epi16(col_data, col_data);
    writefn(dst, stride, col_dup32_lo);
    dst += stride4;
    writefn(dst, stride, col_dup32_hi);
    dst += stride4;
  }
}

// |ref| points to 8 bytes containing 4 packed int16 values.
inline __m128i DcSum4_SSE4_1(const void* ref) {
  const __m128i vals = _mm_loadl_epi64(static_cast<const __m128i*>(ref));
  const __m128i ones = _mm_set1_epi16(1);

  // half_sum[31:0]  = a1+a2
  // half_sum[63:32] = a3+a4
  const __m128i half_sum = _mm_madd_epi16(vals, ones);
  // Place half_sum[63:32] in shift_sum[31:0].
  const __m128i shift_sum = _mm_srli_si128(half_sum, 4);
  return _mm_add_epi32(half_sum, shift_sum);
}

struct DcDefs {
  DcDefs() = delete;

  using _4x4 = DcPredFuncs_SSE4_1<2, 2, DcSum4_SSE4_1, DcSum4_SSE4_1,
                                  DcStore4xH_SSE4_1<4>, 0, 0>;
};

struct DirDefs {
  DirDefs() = delete;

  using _4x4 = DirectionalPredFuncs_SSE4_1<ColStore4_SSE4_1<WriteDuplicate4x4>>;
  using _4x8 = DirectionalPredFuncs_SSE4_1<ColStore8_SSE4_1<WriteDuplicate4x4>>;
  using _4x16 =
      DirectionalPredFuncs_SSE4_1<ColStore16_SSE4_1<WriteDuplicate4x4>>;
  using _8x4 = DirectionalPredFuncs_SSE4_1<ColStore4_SSE4_1<WriteDuplicate8x4>>;
  using _8x8 = DirectionalPredFuncs_SSE4_1<ColStore8_SSE4_1<WriteDuplicate8x4>>;
  using _8x16 =
      DirectionalPredFuncs_SSE4_1<ColStore16_SSE4_1<WriteDuplicate8x4>>;
  using _8x32 =
      DirectionalPredFuncs_SSE4_1<ColStore32_SSE4_1<WriteDuplicate8x4>>;
  using _16x4 =
      DirectionalPredFuncs_SSE4_1<ColStore4_SSE4_1<WriteDuplicate16x4>>;
  using _16x8 =
      DirectionalPredFuncs_SSE4_1<ColStore8_SSE4_1<WriteDuplicate16x4>>;
  using _16x16 =
      DirectionalPredFuncs_SSE4_1<ColStore16_SSE4_1<WriteDuplicate16x4>>;
  using _16x32 =
      DirectionalPredFuncs_SSE4_1<ColStore32_SSE4_1<WriteDuplicate16x4>>;
  using _16x64 =
      DirectionalPredFuncs_SSE4_1<ColStore64_SSE4_1<WriteDuplicate16x4>>;
  using _32x8 =
      DirectionalPredFuncs_SSE4_1<ColStore8_SSE4_1<WriteDuplicate32x4>>;
  using _32x16 =
      DirectionalPredFuncs_SSE4_1<ColStore16_SSE4_1<WriteDuplicate32x4>>;
  using _32x32 =
      DirectionalPredFuncs_SSE4_1<ColStore32_SSE4_1<WriteDuplicate32x4>>;
  using _32x64 =
      DirectionalPredFuncs_SSE4_1<ColStore64_SSE4_1<WriteDuplicate32x4>>;
  using _64x16 =
      DirectionalPredFuncs_SSE4_1<ColStore16_SSE4_1<WriteDuplicate64x4>>;
  using _64x32 =
      DirectionalPredFuncs_SSE4_1<ColStore32_SSE4_1<WriteDuplicate64x4>>;
  using _64x64 =
      DirectionalPredFuncs_SSE4_1<ColStore64_SSE4_1<WriteDuplicate64x4>>;
};

void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
  static_cast<void>(dsp);
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize4x4_IntraPredictorDcTop)
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDcTop] =
      DcDefs::_4x4::DcTop;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize4x4_IntraPredictorDcLeft)
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDcLeft] =
      DcDefs::_4x4::DcLeft;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize4x4_IntraPredictorDc)
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDc] =
      DcDefs::_4x4::Dc;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize4x4_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorHorizontal] =
      DirDefs::_4x4::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize4x8_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorHorizontal] =
      DirDefs::_4x8::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize4x16_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorHorizontal] =
      DirDefs::_4x16::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize8x4_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorHorizontal] =
      DirDefs::_8x4::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize8x8_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorHorizontal] =
      DirDefs::_8x8::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize8x16_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorHorizontal] =
      DirDefs::_8x16::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize8x32_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorHorizontal] =
      DirDefs::_8x32::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize16x4_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorHorizontal] =
      DirDefs::_16x4::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize16x8_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorHorizontal] =
      DirDefs::_16x8::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize16x16_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorHorizontal] =
      DirDefs::_16x16::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize16x32_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorHorizontal] =
      DirDefs::_16x32::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize16x64_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorHorizontal] =
      DirDefs::_16x64::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize32x8_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorHorizontal] =
      DirDefs::_32x8::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize32x16_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorHorizontal] =
      DirDefs::_32x16::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize32x32_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorHorizontal] =
      DirDefs::_32x32::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize32x64_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorHorizontal] =
      DirDefs::_32x64::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize64x16_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorHorizontal] =
      DirDefs::_64x16::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize64x32_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorHorizontal] =
      DirDefs::_64x32::Horizontal;
#endif
#if DSP_ENABLED_10BPP_SSE4_1(TransformSize64x64_IntraPredictorHorizontal)
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorHorizontal] =
      DirDefs::_64x64::Horizontal;
#endif
}

}  // namespace
}  // namespace high_bitdepth
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

void IntraPredInit_SSE4_1() {
  low_bitdepth::Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  high_bitdepth::Init10bpp();
#endif
}

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_SSE4_1
namespace libgav1 {
namespace dsp {

void IntraPredInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
