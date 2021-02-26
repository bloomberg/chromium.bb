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

#include "src/dsp/loop_restoration.h"
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_SSE4_1
#include <smmintrin.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "src/dsp/common.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/dsp/x86/common_sse4.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

inline void WienerHorizontalTap7Kernel(const __m128i s[2],
                                       const __m128i filter[4],
                                       int16_t* const wiener_buffer) {
  const int limit =
      (1 << (8 + 1 + kWienerFilterBits - kInterRoundBitsHorizontal)) - 1;
  const int offset =
      1 << (8 + kWienerFilterBits - kInterRoundBitsHorizontal - 1);
  const __m128i offsets = _mm_set1_epi16(-offset);
  const __m128i limits = _mm_set1_epi16(limit - offset);
  const __m128i round = _mm_set1_epi16(1 << (kInterRoundBitsHorizontal - 1));
  const auto s01 = _mm_alignr_epi8(s[1], s[0], 1);
  const auto s23 = _mm_alignr_epi8(s[1], s[0], 5);
  const auto s45 = _mm_alignr_epi8(s[1], s[0], 9);
  const auto s67 = _mm_alignr_epi8(s[1], s[0], 13);
  const __m128i madd01 = _mm_maddubs_epi16(s01, filter[0]);
  const __m128i madd23 = _mm_maddubs_epi16(s23, filter[1]);
  const __m128i madd45 = _mm_maddubs_epi16(s45, filter[2]);
  const __m128i madd67 = _mm_maddubs_epi16(s67, filter[3]);
  const __m128i madd0123 = _mm_add_epi16(madd01, madd23);
  const __m128i madd4567 = _mm_add_epi16(madd45, madd67);
  // The sum range here is [-128 * 255, 90 * 255].
  const __m128i madd = _mm_add_epi16(madd0123, madd4567);
  const __m128i sum = _mm_add_epi16(madd, round);
  const __m128i rounded_sum0 = _mm_srai_epi16(sum, kInterRoundBitsHorizontal);
  // Calculate scaled down offset correction, and add to sum here to prevent
  // signed 16 bit outranging.
  const __m128i s_3x128 =
      _mm_slli_epi16(_mm_srli_epi16(s23, 8), 7 - kInterRoundBitsHorizontal);
  const __m128i rounded_sum1 = _mm_add_epi16(rounded_sum0, s_3x128);
  const __m128i d0 = _mm_max_epi16(rounded_sum1, offsets);
  const __m128i d1 = _mm_min_epi16(d0, limits);
  StoreAligned16(wiener_buffer, d1);
}

inline void WienerHorizontalTap5Kernel(const __m128i s[2],
                                       const __m128i filter[3],
                                       int16_t* const wiener_buffer) {
  const int limit =
      (1 << (8 + 1 + kWienerFilterBits - kInterRoundBitsHorizontal)) - 1;
  const int offset =
      1 << (8 + kWienerFilterBits - kInterRoundBitsHorizontal - 1);
  const __m128i offsets = _mm_set1_epi16(-offset);
  const __m128i limits = _mm_set1_epi16(limit - offset);
  const __m128i round = _mm_set1_epi16(1 << (kInterRoundBitsHorizontal - 1));
  const auto s01 = _mm_alignr_epi8(s[1], s[0], 1);
  const auto s23 = _mm_alignr_epi8(s[1], s[0], 5);
  const auto s45 = _mm_alignr_epi8(s[1], s[0], 9);
  const __m128i madd01 = _mm_maddubs_epi16(s01, filter[0]);
  const __m128i madd23 = _mm_maddubs_epi16(s23, filter[1]);
  const __m128i madd45 = _mm_maddubs_epi16(s45, filter[2]);
  const __m128i madd0123 = _mm_add_epi16(madd01, madd23);
  // The sum range here is [-128 * 255, 90 * 255].
  const __m128i madd = _mm_add_epi16(madd0123, madd45);
  const __m128i sum = _mm_add_epi16(madd, round);
  const __m128i rounded_sum0 = _mm_srai_epi16(sum, kInterRoundBitsHorizontal);
  // Calculate scaled down offset correction, and add to sum here to prevent
  // signed 16 bit outranging.
  const __m128i s_3x128 =
      _mm_srli_epi16(_mm_slli_epi16(s23, 8), kInterRoundBitsHorizontal + 1);
  const __m128i rounded_sum1 = _mm_add_epi16(rounded_sum0, s_3x128);
  const __m128i d0 = _mm_max_epi16(rounded_sum1, offsets);
  const __m128i d1 = _mm_min_epi16(d0, limits);
  StoreAligned16(wiener_buffer, d1);
}

inline void WienerHorizontalTap3Kernel(const __m128i s[2],
                                       const __m128i filter[2],
                                       int16_t* const wiener_buffer) {
  const int limit =
      (1 << (8 + 1 + kWienerFilterBits - kInterRoundBitsHorizontal)) - 1;
  const int offset =
      1 << (8 + kWienerFilterBits - kInterRoundBitsHorizontal - 1);
  const __m128i offsets = _mm_set1_epi16(-offset);
  const __m128i limits = _mm_set1_epi16(limit - offset);
  const __m128i round = _mm_set1_epi16(1 << (kInterRoundBitsHorizontal - 1));
  const auto s01 = _mm_alignr_epi8(s[1], s[0], 1);
  const auto s23 = _mm_alignr_epi8(s[1], s[0], 5);
  const __m128i madd01 = _mm_maddubs_epi16(s01, filter[0]);
  const __m128i madd23 = _mm_maddubs_epi16(s23, filter[1]);
  // The sum range here is [-128 * 255, 90 * 255].
  const __m128i madd = _mm_add_epi16(madd01, madd23);
  const __m128i sum = _mm_add_epi16(madd, round);
  const __m128i rounded_sum0 = _mm_srai_epi16(sum, kInterRoundBitsHorizontal);
  // Calculate scaled down offset correction, and add to sum here to prevent
  // signed 16 bit outranging.
  const __m128i s_3x128 =
      _mm_slli_epi16(_mm_srli_epi16(s01, 8), 7 - kInterRoundBitsHorizontal);
  const __m128i rounded_sum1 = _mm_add_epi16(rounded_sum0, s_3x128);
  const __m128i d0 = _mm_max_epi16(rounded_sum1, offsets);
  const __m128i d1 = _mm_min_epi16(d0, limits);
  StoreAligned16(wiener_buffer, d1);
}

inline void WienerHorizontalTap7(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 const __m128i coefficients,
                                 int16_t** const wiener_buffer) {
  __m128i filter[4];
  filter[0] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0200));
  filter[1] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0604));
  filter[2] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0204));
  filter[3] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x8000));
  for (int y = height; y != 0; --y) {
    __m128i s = LoadUnaligned16(src);
    __m128i ss[3];
    ss[0] = _mm_unpacklo_epi8(s, s);
    ptrdiff_t x = 0;
    do {
      ss[1] = _mm_unpackhi_epi8(s, s);
      s = LoadUnaligned16(src + x + 16);
      ss[2] = _mm_unpacklo_epi8(s, s);
      WienerHorizontalTap7Kernel(ss + 0, filter, *wiener_buffer + x + 0);
      WienerHorizontalTap7Kernel(ss + 1, filter, *wiener_buffer + x + 8);
      ss[0] = ss[2];
      x += 16;
    } while (x < width);
    src += src_stride;
    *wiener_buffer += width;
  }
}

inline void WienerHorizontalTap5(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 const __m128i coefficients,
                                 int16_t** const wiener_buffer) {
  __m128i filter[3];
  filter[0] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0402));
  filter[1] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0406));
  filter[2] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x8002));
  for (int y = height; y != 0; --y) {
    __m128i s = LoadUnaligned16(src);
    __m128i ss[3];
    ss[0] = _mm_unpacklo_epi8(s, s);
    ptrdiff_t x = 0;
    do {
      ss[1] = _mm_unpackhi_epi8(s, s);
      s = LoadUnaligned16(src + x + 16);
      ss[2] = _mm_unpacklo_epi8(s, s);
      WienerHorizontalTap5Kernel(ss + 0, filter, *wiener_buffer + x + 0);
      WienerHorizontalTap5Kernel(ss + 1, filter, *wiener_buffer + x + 8);
      ss[0] = ss[2];
      x += 16;
    } while (x < width);
    src += src_stride;
    *wiener_buffer += width;
  }
}

inline void WienerHorizontalTap3(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 const __m128i coefficients,
                                 int16_t** const wiener_buffer) {
  __m128i filter[2];
  filter[0] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0604));
  filter[1] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x8004));
  for (int y = height; y != 0; --y) {
    __m128i s = LoadUnaligned16(src);
    __m128i ss[3];
    ss[0] = _mm_unpacklo_epi8(s, s);
    ptrdiff_t x = 0;
    do {
      ss[1] = _mm_unpackhi_epi8(s, s);
      s = LoadUnaligned16(src + x + 16);
      ss[2] = _mm_unpacklo_epi8(s, s);
      WienerHorizontalTap3Kernel(ss + 0, filter, *wiener_buffer + x + 0);
      WienerHorizontalTap3Kernel(ss + 1, filter, *wiener_buffer + x + 8);
      ss[0] = ss[2];
      x += 16;
    } while (x < width);
    src += src_stride;
    *wiener_buffer += width;
  }
}

inline void WienerHorizontalTap1(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 int16_t** const wiener_buffer) {
  for (int y = height; y != 0; --y) {
    ptrdiff_t x = 0;
    do {
      const __m128i s = LoadUnaligned16(src + x);
      const __m128i s0 = _mm_unpacklo_epi8(s, _mm_setzero_si128());
      const __m128i s1 = _mm_unpackhi_epi8(s, _mm_setzero_si128());
      const __m128i d0 = _mm_slli_epi16(s0, 4);
      const __m128i d1 = _mm_slli_epi16(s1, 4);
      StoreAligned16(*wiener_buffer + x + 0, d0);
      StoreAligned16(*wiener_buffer + x + 8, d1);
      x += 16;
    } while (x < width);
    src += src_stride;
    *wiener_buffer += width;
  }
}

inline __m128i WienerVertical7(const __m128i a[2], const __m128i filter[2]) {
  const __m128i round = _mm_set1_epi32(1 << (kInterRoundBitsVertical - 1));
  const __m128i madd0 = _mm_madd_epi16(a[0], filter[0]);
  const __m128i madd1 = _mm_madd_epi16(a[1], filter[1]);
  const __m128i sum0 = _mm_add_epi32(round, madd0);
  const __m128i sum1 = _mm_add_epi32(sum0, madd1);
  return _mm_srai_epi32(sum1, kInterRoundBitsVertical);
}

inline __m128i WienerVertical5(const __m128i a[2], const __m128i filter[2]) {
  const __m128i madd0 = _mm_madd_epi16(a[0], filter[0]);
  const __m128i madd1 = _mm_madd_epi16(a[1], filter[1]);
  const __m128i sum = _mm_add_epi32(madd0, madd1);
  return _mm_srai_epi32(sum, kInterRoundBitsVertical);
}

inline __m128i WienerVertical3(const __m128i a, const __m128i filter) {
  const __m128i round = _mm_set1_epi32(1 << (kInterRoundBitsVertical - 1));
  const __m128i madd = _mm_madd_epi16(a, filter);
  const __m128i sum = _mm_add_epi32(round, madd);
  return _mm_srai_epi32(sum, kInterRoundBitsVertical);
}

inline __m128i WienerVerticalFilter7(const __m128i a[7],
                                     const __m128i filter[2]) {
  __m128i b[2];
  const __m128i a06 = _mm_add_epi16(a[0], a[6]);
  const __m128i a15 = _mm_add_epi16(a[1], a[5]);
  const __m128i a24 = _mm_add_epi16(a[2], a[4]);
  b[0] = _mm_unpacklo_epi16(a06, a15);
  b[1] = _mm_unpacklo_epi16(a24, a[3]);
  const __m128i sum0 = WienerVertical7(b, filter);
  b[0] = _mm_unpackhi_epi16(a06, a15);
  b[1] = _mm_unpackhi_epi16(a24, a[3]);
  const __m128i sum1 = WienerVertical7(b, filter);
  return _mm_packs_epi32(sum0, sum1);
}

inline __m128i WienerVerticalFilter5(const __m128i a[5],
                                     const __m128i filter[2]) {
  const __m128i round = _mm_set1_epi16(1 << (kInterRoundBitsVertical - 1));
  __m128i b[2];
  const __m128i a04 = _mm_add_epi16(a[0], a[4]);
  const __m128i a13 = _mm_add_epi16(a[1], a[3]);
  b[0] = _mm_unpacklo_epi16(a04, a13);
  b[1] = _mm_unpacklo_epi16(a[2], round);
  const __m128i sum0 = WienerVertical5(b, filter);
  b[0] = _mm_unpackhi_epi16(a04, a13);
  b[1] = _mm_unpackhi_epi16(a[2], round);
  const __m128i sum1 = WienerVertical5(b, filter);
  return _mm_packs_epi32(sum0, sum1);
}

inline __m128i WienerVerticalFilter3(const __m128i a[3], const __m128i filter) {
  __m128i b;
  const __m128i a02 = _mm_add_epi16(a[0], a[2]);
  b = _mm_unpacklo_epi16(a02, a[1]);
  const __m128i sum0 = WienerVertical3(b, filter);
  b = _mm_unpackhi_epi16(a02, a[1]);
  const __m128i sum1 = WienerVertical3(b, filter);
  return _mm_packs_epi32(sum0, sum1);
}

inline __m128i WienerVerticalTap7Kernel(const int16_t* wiener_buffer,
                                        const ptrdiff_t wiener_stride,
                                        const __m128i filter[2], __m128i a[7]) {
  a[0] = LoadAligned16(wiener_buffer + 0 * wiener_stride);
  a[1] = LoadAligned16(wiener_buffer + 1 * wiener_stride);
  a[2] = LoadAligned16(wiener_buffer + 2 * wiener_stride);
  a[3] = LoadAligned16(wiener_buffer + 3 * wiener_stride);
  a[4] = LoadAligned16(wiener_buffer + 4 * wiener_stride);
  a[5] = LoadAligned16(wiener_buffer + 5 * wiener_stride);
  a[6] = LoadAligned16(wiener_buffer + 6 * wiener_stride);
  return WienerVerticalFilter7(a, filter);
}

inline __m128i WienerVerticalTap5Kernel(const int16_t* wiener_buffer,
                                        const ptrdiff_t wiener_stride,
                                        const __m128i filter[2], __m128i a[5]) {
  a[0] = LoadAligned16(wiener_buffer + 0 * wiener_stride);
  a[1] = LoadAligned16(wiener_buffer + 1 * wiener_stride);
  a[2] = LoadAligned16(wiener_buffer + 2 * wiener_stride);
  a[3] = LoadAligned16(wiener_buffer + 3 * wiener_stride);
  a[4] = LoadAligned16(wiener_buffer + 4 * wiener_stride);
  return WienerVerticalFilter5(a, filter);
}

inline __m128i WienerVerticalTap3Kernel(const int16_t* wiener_buffer,
                                        const ptrdiff_t wiener_stride,
                                        const __m128i filter, __m128i a[3]) {
  a[0] = LoadAligned16(wiener_buffer + 0 * wiener_stride);
  a[1] = LoadAligned16(wiener_buffer + 1 * wiener_stride);
  a[2] = LoadAligned16(wiener_buffer + 2 * wiener_stride);
  return WienerVerticalFilter3(a, filter);
}

inline void WienerVerticalTap7Kernel2(const int16_t* wiener_buffer,
                                      const ptrdiff_t wiener_stride,
                                      const __m128i filter[2], __m128i d[2]) {
  __m128i a[8];
  d[0] = WienerVerticalTap7Kernel(wiener_buffer, wiener_stride, filter, a);
  a[7] = LoadAligned16(wiener_buffer + 7 * wiener_stride);
  d[1] = WienerVerticalFilter7(a + 1, filter);
}

inline void WienerVerticalTap5Kernel2(const int16_t* wiener_buffer,
                                      const ptrdiff_t wiener_stride,
                                      const __m128i filter[2], __m128i d[2]) {
  __m128i a[6];
  d[0] = WienerVerticalTap5Kernel(wiener_buffer, wiener_stride, filter, a);
  a[5] = LoadAligned16(wiener_buffer + 5 * wiener_stride);
  d[1] = WienerVerticalFilter5(a + 1, filter);
}

inline void WienerVerticalTap3Kernel2(const int16_t* wiener_buffer,
                                      const ptrdiff_t wiener_stride,
                                      const __m128i filter, __m128i d[2]) {
  __m128i a[4];
  d[0] = WienerVerticalTap3Kernel(wiener_buffer, wiener_stride, filter, a);
  a[3] = LoadAligned16(wiener_buffer + 3 * wiener_stride);
  d[1] = WienerVerticalFilter3(a + 1, filter);
}

inline void WienerVerticalTap7(const int16_t* wiener_buffer,
                               const ptrdiff_t width, const int height,
                               const int16_t coefficients[4], uint8_t* dst,
                               const ptrdiff_t dst_stride) {
  const __m128i c = LoadLo8(coefficients);
  __m128i filter[2];
  filter[0] = _mm_shuffle_epi32(c, 0x0);
  filter[1] = _mm_shuffle_epi32(c, 0x55);
  for (int y = height >> 1; y > 0; --y) {
    ptrdiff_t x = 0;
    do {
      __m128i d[2][2];
      WienerVerticalTap7Kernel2(wiener_buffer + x + 0, width, filter, d[0]);
      WienerVerticalTap7Kernel2(wiener_buffer + x + 8, width, filter, d[1]);
      StoreAligned16(dst + x, _mm_packus_epi16(d[0][0], d[1][0]));
      StoreAligned16(dst + dst_stride + x, _mm_packus_epi16(d[0][1], d[1][1]));
      x += 16;
    } while (x < width);
    dst += 2 * dst_stride;
    wiener_buffer += 2 * width;
  }

  if ((height & 1) != 0) {
    ptrdiff_t x = 0;
    do {
      __m128i a[7];
      const __m128i d0 =
          WienerVerticalTap7Kernel(wiener_buffer + x + 0, width, filter, a);
      const __m128i d1 =
          WienerVerticalTap7Kernel(wiener_buffer + x + 8, width, filter, a);
      StoreAligned16(dst + x, _mm_packus_epi16(d0, d1));
      x += 16;
    } while (x < width);
  }
}

inline void WienerVerticalTap5(const int16_t* wiener_buffer,
                               const ptrdiff_t width, const int height,
                               const int16_t coefficients[3], uint8_t* dst,
                               const ptrdiff_t dst_stride) {
  const __m128i c = Load4(coefficients);
  __m128i filter[2];
  filter[0] = _mm_shuffle_epi32(c, 0);
  filter[1] =
      _mm_set1_epi32((1 << 16) | static_cast<uint16_t>(coefficients[2]));
  for (int y = height >> 1; y > 0; --y) {
    ptrdiff_t x = 0;
    do {
      __m128i d[2][2];
      WienerVerticalTap5Kernel2(wiener_buffer + x + 0, width, filter, d[0]);
      WienerVerticalTap5Kernel2(wiener_buffer + x + 8, width, filter, d[1]);
      StoreAligned16(dst + x, _mm_packus_epi16(d[0][0], d[1][0]));
      StoreAligned16(dst + dst_stride + x, _mm_packus_epi16(d[0][1], d[1][1]));
      x += 16;
    } while (x < width);
    dst += 2 * dst_stride;
    wiener_buffer += 2 * width;
  }

  if ((height & 1) != 0) {
    ptrdiff_t x = 0;
    do {
      __m128i a[5];
      const __m128i d0 =
          WienerVerticalTap5Kernel(wiener_buffer + x + 0, width, filter, a);
      const __m128i d1 =
          WienerVerticalTap5Kernel(wiener_buffer + x + 8, width, filter, a);
      StoreAligned16(dst + x, _mm_packus_epi16(d0, d1));
      x += 16;
    } while (x < width);
  }
}

inline void WienerVerticalTap3(const int16_t* wiener_buffer,
                               const ptrdiff_t width, const int height,
                               const int16_t coefficients[2], uint8_t* dst,
                               const ptrdiff_t dst_stride) {
  const __m128i filter =
      _mm_set1_epi32(*reinterpret_cast<const int32_t*>(coefficients));
  for (int y = height >> 1; y > 0; --y) {
    ptrdiff_t x = 0;
    do {
      __m128i d[2][2];
      WienerVerticalTap3Kernel2(wiener_buffer + x + 0, width, filter, d[0]);
      WienerVerticalTap3Kernel2(wiener_buffer + x + 8, width, filter, d[1]);
      StoreAligned16(dst + x, _mm_packus_epi16(d[0][0], d[1][0]));
      StoreAligned16(dst + dst_stride + x, _mm_packus_epi16(d[0][1], d[1][1]));
      x += 16;
    } while (x < width);
    dst += 2 * dst_stride;
    wiener_buffer += 2 * width;
  }

  if ((height & 1) != 0) {
    ptrdiff_t x = 0;
    do {
      __m128i a[3];
      const __m128i d0 =
          WienerVerticalTap3Kernel(wiener_buffer + x + 0, width, filter, a);
      const __m128i d1 =
          WienerVerticalTap3Kernel(wiener_buffer + x + 8, width, filter, a);
      StoreAligned16(dst + x, _mm_packus_epi16(d0, d1));
      x += 16;
    } while (x < width);
  }
}

inline void WienerVerticalTap1Kernel(const int16_t* const wiener_buffer,
                                     uint8_t* const dst) {
  const __m128i a0 = LoadAligned16(wiener_buffer + 0);
  const __m128i a1 = LoadAligned16(wiener_buffer + 8);
  const __m128i b0 = _mm_add_epi16(a0, _mm_set1_epi16(8));
  const __m128i b1 = _mm_add_epi16(a1, _mm_set1_epi16(8));
  const __m128i c0 = _mm_srai_epi16(b0, 4);
  const __m128i c1 = _mm_srai_epi16(b1, 4);
  const __m128i d = _mm_packus_epi16(c0, c1);
  StoreAligned16(dst, d);
}

inline void WienerVerticalTap1(const int16_t* wiener_buffer,
                               const ptrdiff_t width, const int height,
                               uint8_t* dst, const ptrdiff_t dst_stride) {
  for (int y = height >> 1; y > 0; --y) {
    ptrdiff_t x = 0;
    do {
      WienerVerticalTap1Kernel(wiener_buffer + x, dst + x);
      WienerVerticalTap1Kernel(wiener_buffer + width + x, dst + dst_stride + x);
      x += 16;
    } while (x < width);
    dst += 2 * dst_stride;
    wiener_buffer += 2 * width;
  }

  if ((height & 1) != 0) {
    ptrdiff_t x = 0;
    do {
      WienerVerticalTap1Kernel(wiener_buffer + x, dst + x);
      x += 16;
    } while (x < width);
  }
}

void WienerFilter_SSE4_1(const RestorationUnitInfo& restoration_info,
                         const void* const source, const void* const top_border,
                         const void* const bottom_border,
                         const ptrdiff_t stride, const int width,
                         const int height,
                         RestorationBuffer* const restoration_buffer,
                         void* const dest) {
  const int16_t* const number_leading_zero_coefficients =
      restoration_info.wiener_info.number_leading_zero_coefficients;
  const int number_rows_to_skip = std::max(
      static_cast<int>(number_leading_zero_coefficients[WienerInfo::kVertical]),
      1);
  const ptrdiff_t wiener_stride = Align(width, 16);
  int16_t* const wiener_buffer_vertical = restoration_buffer->wiener_buffer;
  // The values are saturated to 13 bits before storing.
  int16_t* wiener_buffer_horizontal =
      wiener_buffer_vertical + number_rows_to_skip * wiener_stride;

  // horizontal filtering.
  // Over-reads up to 15 - |kRestorationHorizontalBorder| values.
  const int height_horizontal =
      height + kWienerFilterTaps - 1 - 2 * number_rows_to_skip;
  const int height_extra = (height_horizontal - height) >> 1;
  assert(height_extra <= 2);
  const auto* const src = static_cast<const uint8_t*>(source);
  const auto* const top = static_cast<const uint8_t*>(top_border);
  const auto* const bottom = static_cast<const uint8_t*>(bottom_border);
  const __m128i c =
      LoadLo8(restoration_info.wiener_info.filter[WienerInfo::kHorizontal]);
  // In order to keep the horizontal pass intermediate values within 16 bits we
  // offset |filter[3]| by 128. The 128 offset will be added back in the loop.
  const __m128i coefficients_horizontal =
      _mm_sub_epi16(c, _mm_setr_epi16(0, 0, 0, 128, 0, 0, 0, 0));
  if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 0) {
    WienerHorizontalTap7(top + (2 - height_extra) * stride - 3, stride,
                         wiener_stride, height_extra, coefficients_horizontal,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap7(src - 3, stride, wiener_stride, height,
                         coefficients_horizontal, &wiener_buffer_horizontal);
    WienerHorizontalTap7(bottom - 3, stride, wiener_stride, height_extra,
                         coefficients_horizontal, &wiener_buffer_horizontal);
  } else if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 1) {
    WienerHorizontalTap5(top + (2 - height_extra) * stride - 2, stride,
                         wiener_stride, height_extra, coefficients_horizontal,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap5(src - 2, stride, wiener_stride, height,
                         coefficients_horizontal, &wiener_buffer_horizontal);
    WienerHorizontalTap5(bottom - 2, stride, wiener_stride, height_extra,
                         coefficients_horizontal, &wiener_buffer_horizontal);
  } else if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 2) {
    // The maximum over-reads happen here.
    WienerHorizontalTap3(top + (2 - height_extra) * stride - 1, stride,
                         wiener_stride, height_extra, coefficients_horizontal,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap3(src - 1, stride, wiener_stride, height,
                         coefficients_horizontal, &wiener_buffer_horizontal);
    WienerHorizontalTap3(bottom - 1, stride, wiener_stride, height_extra,
                         coefficients_horizontal, &wiener_buffer_horizontal);
  } else {
    assert(number_leading_zero_coefficients[WienerInfo::kHorizontal] == 3);
    WienerHorizontalTap1(top + (2 - height_extra) * stride, stride,
                         wiener_stride, height_extra,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap1(src, stride, wiener_stride, height,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap1(bottom, stride, wiener_stride, height_extra,
                         &wiener_buffer_horizontal);
  }

  // vertical filtering.
  // Over-writes up to 15 values.
  const int16_t* const filter_vertical =
      restoration_info.wiener_info.filter[WienerInfo::kVertical];
  auto* dst = static_cast<uint8_t*>(dest);
  if (number_leading_zero_coefficients[WienerInfo::kVertical] == 0) {
    // Because the top row of |source| is a duplicate of the second row, and the
    // bottom row of |source| is a duplicate of its above row, we can duplicate
    // the top and bottom row of |wiener_buffer| accordingly.
    memcpy(wiener_buffer_horizontal, wiener_buffer_horizontal - wiener_stride,
           sizeof(*wiener_buffer_horizontal) * wiener_stride);
    memcpy(restoration_buffer->wiener_buffer,
           restoration_buffer->wiener_buffer + wiener_stride,
           sizeof(*restoration_buffer->wiener_buffer) * wiener_stride);
    WienerVerticalTap7(wiener_buffer_vertical, wiener_stride, height,
                       filter_vertical, dst, stride);
  } else if (number_leading_zero_coefficients[WienerInfo::kVertical] == 1) {
    WienerVerticalTap5(wiener_buffer_vertical + wiener_stride, wiener_stride,
                       height, filter_vertical + 1, dst, stride);
  } else if (number_leading_zero_coefficients[WienerInfo::kVertical] == 2) {
    WienerVerticalTap3(wiener_buffer_vertical + 2 * wiener_stride,
                       wiener_stride, height, filter_vertical + 2, dst, stride);
  } else {
    assert(number_leading_zero_coefficients[WienerInfo::kVertical] == 3);
    WienerVerticalTap1(wiener_buffer_vertical + 3 * wiener_stride,
                       wiener_stride, height, dst, stride);
  }
}

//------------------------------------------------------------------------------
// SGR

// SIMD overreads 8 - (width % 8) - 2 * padding pixels, where padding is 3 for
// Pass 1 and 2 for Pass 2.
constexpr int kOverreadInBytesPass1 = 2;
constexpr int kOverreadInBytesPass2 = 4;

// Don't use _mm_cvtepu8_epi16() or _mm_cvtepu16_epi32() in the following
// functions. Some compilers may generate super inefficient code and the whole
// decoder could be 15% slower.

inline __m128i VaddlLo8(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpacklo_epi8(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpacklo_epi8(src1, _mm_setzero_si128());
  return _mm_add_epi16(s0, s1);
}

inline __m128i VaddlLo16(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpacklo_epi16(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpacklo_epi16(src1, _mm_setzero_si128());
  return _mm_add_epi32(s0, s1);
}

inline __m128i VaddlHi16(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpackhi_epi16(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpackhi_epi16(src1, _mm_setzero_si128());
  return _mm_add_epi32(s0, s1);
}

inline __m128i VaddwLo8(const __m128i src0, const __m128i src1) {
  const __m128i s1 = _mm_unpacklo_epi8(src1, _mm_setzero_si128());
  return _mm_add_epi16(src0, s1);
}

inline __m128i VaddwLo16(const __m128i src0, const __m128i src1) {
  const __m128i s1 = _mm_unpacklo_epi16(src1, _mm_setzero_si128());
  return _mm_add_epi32(src0, s1);
}

inline __m128i VaddwHi16(const __m128i src0, const __m128i src1) {
  const __m128i s1 = _mm_unpackhi_epi16(src1, _mm_setzero_si128());
  return _mm_add_epi32(src0, s1);
}

// Using VgetLane16() can save a sign extension instruction.
template <int n>
inline int VgetLane16(const __m128i src) {
  return _mm_extract_epi16(src, n);
}

inline __m128i VmullLo8(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpacklo_epi8(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpacklo_epi8(src1, _mm_setzero_si128());
  return _mm_mullo_epi16(s0, s1);
}

inline __m128i VmullHi8(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpackhi_epi8(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpackhi_epi8(src1, _mm_setzero_si128());
  return _mm_mullo_epi16(s0, s1);
}

inline __m128i VmullNLo8(const __m128i src0, const int src1) {
  const __m128i s0 = _mm_unpacklo_epi16(src0, _mm_setzero_si128());
  return _mm_madd_epi16(s0, _mm_set1_epi32(src1));
}

inline __m128i VmullNHi8(const __m128i src0, const int src1) {
  const __m128i s0 = _mm_unpackhi_epi16(src0, _mm_setzero_si128());
  return _mm_madd_epi16(s0, _mm_set1_epi32(src1));
}

inline __m128i VmullLo16(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpacklo_epi16(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpacklo_epi16(src1, _mm_setzero_si128());
  return _mm_madd_epi16(s0, s1);
}

inline __m128i VmullHi16(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpackhi_epi16(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpackhi_epi16(src1, _mm_setzero_si128());
  return _mm_madd_epi16(s0, s1);
}

inline __m128i VrshrS32(const __m128i src0, const int src1) {
  const __m128i sum = _mm_add_epi32(src0, _mm_set1_epi32(1 << (src1 - 1)));
  return _mm_srai_epi32(sum, src1);
}

inline __m128i VrshrU32(const __m128i src0, const int src1) {
  const __m128i sum = _mm_add_epi32(src0, _mm_set1_epi32(1 << (src1 - 1)));
  return _mm_srli_epi32(sum, src1);
}

inline void Prepare3_8(const __m128i src, __m128i dst[3]) {
  dst[0] = src;
  dst[1] = _mm_srli_si128(src, 1);
  dst[2] = _mm_srli_si128(src, 2);
}

inline void Prepare3_16(const __m128i src[2], __m128i dst[3]) {
  dst[0] = src[0];
  dst[1] = _mm_alignr_epi8(src[1], src[0], 2);
  dst[2] = _mm_alignr_epi8(src[1], src[0], 4);
}

inline void Prepare5_8(const __m128i src, __m128i dst[5]) {
  dst[0] = src;
  dst[1] = _mm_srli_si128(src, 1);
  dst[2] = _mm_srli_si128(src, 2);
  dst[3] = _mm_srli_si128(src, 3);
  dst[4] = _mm_srli_si128(src, 4);
}

inline void Prepare5_16(const __m128i src[2], __m128i dst[5]) {
  Prepare3_16(src, dst);
  dst[3] = _mm_alignr_epi8(src[1], src[0], 6);
  dst[4] = _mm_alignr_epi8(src[1], src[0], 8);
}

inline __m128i Sum3_16(const __m128i src0, const __m128i src1,
                       const __m128i src2) {
  const __m128i sum = _mm_add_epi16(src0, src1);
  return _mm_add_epi16(sum, src2);
}

inline __m128i Sum3_16(const __m128i src[3]) {
  return Sum3_16(src[0], src[1], src[2]);
}

inline __m128i Sum3_32(const __m128i src0, const __m128i src1,
                       const __m128i src2) {
  const __m128i sum = _mm_add_epi32(src0, src1);
  return _mm_add_epi32(sum, src2);
}

inline void Sum3_32(const __m128i src[3][2], __m128i dst[2]) {
  dst[0] = Sum3_32(src[0][0], src[1][0], src[2][0]);
  dst[1] = Sum3_32(src[0][1], src[1][1], src[2][1]);
}

inline __m128i Sum3W_16(const __m128i src[3]) {
  const __m128i sum = VaddlLo8(src[0], src[1]);
  return VaddwLo8(sum, src[2]);
}

inline __m128i Sum3WLo_32(const __m128i src[3]) {
  const __m128i sum = VaddlLo16(src[0], src[1]);
  return VaddwLo16(sum, src[2]);
}

inline __m128i Sum3WHi_32(const __m128i src[3]) {
  const __m128i sum = VaddlHi16(src[0], src[1]);
  return VaddwHi16(sum, src[2]);
}

inline __m128i Sum5_16(const __m128i src[5]) {
  const __m128i sum01 = _mm_add_epi16(src[0], src[1]);
  const __m128i sum23 = _mm_add_epi16(src[2], src[3]);
  const __m128i sum = _mm_add_epi16(sum01, sum23);
  return _mm_add_epi16(sum, src[4]);
}

inline __m128i Sum5_32(const __m128i* const src0, const __m128i* const src1,
                       const __m128i* const src2, const __m128i* const src3,
                       const __m128i* const src4) {
  const __m128i sum01 = _mm_add_epi32(*src0, *src1);
  const __m128i sum23 = _mm_add_epi32(*src2, *src3);
  const __m128i sum = _mm_add_epi32(sum01, sum23);
  return _mm_add_epi32(sum, *src4);
}

inline void Sum5_32(const __m128i src[5][2], __m128i dst[2]) {
  dst[0] = Sum5_32(&src[0][0], &src[1][0], &src[2][0], &src[3][0], &src[4][0]);
  dst[1] = Sum5_32(&src[0][1], &src[1][1], &src[2][1], &src[3][1], &src[4][1]);
}

inline __m128i Sum5W_32(const __m128i src[5]) {
  const __m128i sum01 = VaddlLo16(src[0], src[1]);
  const __m128i sum23 = VaddlLo16(src[2], src[3]);
  const __m128i sum0123 = _mm_add_epi32(sum01, sum23);
  return VaddwLo16(sum0123, src[4]);
}

inline __m128i Sum5WHi_32(const __m128i src[5]) {
  const __m128i sum01 = VaddlHi16(src[0], src[1]);
  const __m128i sum23 = VaddlHi16(src[2], src[3]);
  const __m128i sum0123 = _mm_add_epi32(sum01, sum23);
  return VaddwHi16(sum0123, src[4]);
}

inline __m128i Sum3Horizontal(const __m128i src) {
  __m128i s[3];
  Prepare3_8(src, s);
  return Sum3W_16(s);
}

inline void Sum3WHorizontal(const __m128i src[2], __m128i dst[2]) {
  __m128i s[3];
  Prepare3_16(src, s);
  dst[0] = Sum3WLo_32(s);
  dst[1] = Sum3WHi_32(s);
}

inline __m128i Sum5Horizontal(const __m128i src) {
  __m128i s[5];
  Prepare5_8(src, s);
  const __m128i sum01 = VaddlLo8(s[0], s[1]);
  const __m128i sum23 = VaddlLo8(s[2], s[3]);
  const __m128i sum = _mm_add_epi16(sum01, sum23);
  return VaddwLo8(sum, s[4]);
}

inline void Sum5WHorizontal(const __m128i src[2], __m128i dst[2]) {
  __m128i s[5];
  Prepare5_16(src, s);
  dst[0] = Sum5W_32(s);
  dst[1] = Sum5WHi_32(s);
}

void SumHorizontalLo(const __m128i src[5], __m128i* const row_sq3,
                     __m128i* const row_sq5) {
  const __m128i sum04 = VaddlLo16(src[0], src[4]);
  const __m128i sum12 = VaddlLo16(src[1], src[2]);
  *row_sq3 = VaddwLo16(sum12, src[3]);
  *row_sq5 = _mm_add_epi32(sum04, *row_sq3);
}

void SumHorizontalHi(const __m128i src[5], __m128i* const row_sq3,
                     __m128i* const row_sq5) {
  const __m128i sum04 = VaddlHi16(src[0], src[4]);
  const __m128i sum12 = VaddlHi16(src[1], src[2]);
  *row_sq3 = VaddwHi16(sum12, src[3]);
  *row_sq5 = _mm_add_epi32(sum04, *row_sq3);
}

void SumHorizontal(const __m128i src, const __m128i sq[2], __m128i* const row3,
                   __m128i* const row5, __m128i row_sq3[2],
                   __m128i row_sq5[2]) {
  __m128i s[5];
  Prepare5_8(src, s);
  const __m128i sum04 = VaddlLo8(s[0], s[4]);
  const __m128i sum12 = VaddlLo8(s[1], s[2]);
  *row3 = VaddwLo8(sum12, s[3]);
  *row5 = _mm_add_epi16(sum04, *row3);
  Prepare5_16(sq, s);
  SumHorizontalLo(s, &row_sq3[0], &row_sq5[0]);
  SumHorizontalHi(s, &row_sq3[1], &row_sq5[1]);
}

inline __m128i Sum343(const __m128i src) {
  __m128i s[3];
  Prepare3_8(src, s);
  const __m128i sum = Sum3W_16(s);
  const __m128i sum3 = Sum3_16(sum, sum, sum);
  return VaddwLo8(sum3, s[1]);
}

inline __m128i Sum343WLo(const __m128i src[3]) {
  const __m128i sum = Sum3WLo_32(src);
  const __m128i sum3 = Sum3_32(sum, sum, sum);
  return VaddwLo16(sum3, src[1]);
}

inline __m128i Sum343WHi(const __m128i src[3]) {
  const __m128i sum = Sum3WHi_32(src);
  const __m128i sum3 = Sum3_32(sum, sum, sum);
  return VaddwHi16(sum3, src[1]);
}

inline void Sum343W(const __m128i src[2], __m128i dst[2]) {
  __m128i s[3];
  Prepare3_16(src, s);
  dst[0] = Sum343WLo(s);
  dst[1] = Sum343WHi(s);
}

inline __m128i Sum565(const __m128i src) {
  __m128i s[3];
  Prepare3_8(src, s);
  const __m128i sum = Sum3W_16(s);
  const __m128i sum4 = _mm_slli_epi16(sum, 2);
  const __m128i sum5 = _mm_add_epi16(sum4, sum);
  return VaddwLo8(sum5, s[1]);
}

inline __m128i Sum565WLo(const __m128i src[3]) {
  const __m128i sum = Sum3WLo_32(src);
  const __m128i sum4 = _mm_slli_epi32(sum, 2);
  const __m128i sum5 = _mm_add_epi32(sum4, sum);
  return VaddwLo16(sum5, src[1]);
}

inline __m128i Sum565WHi(const __m128i src[3]) {
  const __m128i sum = Sum3WHi_32(src);
  const __m128i sum4 = _mm_slli_epi32(sum, 2);
  const __m128i sum5 = _mm_add_epi32(sum4, sum);
  return VaddwHi16(sum5, src[1]);
}

inline void Sum565W(const __m128i src[2], __m128i dst[2]) {
  __m128i s[3];
  Prepare3_16(src, s);
  dst[0] = Sum565WLo(s);
  dst[1] = Sum565WHi(s);
}

inline void BoxSum(const uint8_t* src, const ptrdiff_t src_stride,
                   const int height, const ptrdiff_t sum_stride,
                   const ptrdiff_t width, uint16_t* sum3, uint16_t* sum5,
                   uint32_t* square_sum3, uint32_t* square_sum5) {
  int y = height;
  do {
    __m128i s, sq[2];
    s = LoadLo8Msan(src, kOverreadInBytesPass1 - width);
    sq[0] = VmullLo8(s, s);
    ptrdiff_t x = 0;
    do {
      __m128i row3, row5, row_sq3[2], row_sq5[2];
      s = LoadHi8Msan(s, src + x + 8, x + 8 + kOverreadInBytesPass1 - width);
      sq[1] = VmullHi8(s, s);
      SumHorizontal(s, sq, &row3, &row5, row_sq3, row_sq5);
      StoreAligned16(sum3, row3);
      StoreAligned16(sum5, row5);
      StoreAligned16(square_sum3 + 0, row_sq3[0]);
      StoreAligned16(square_sum3 + 4, row_sq3[1]);
      StoreAligned16(square_sum5 + 0, row_sq5[0]);
      StoreAligned16(square_sum5 + 4, row_sq5[1]);
      s = _mm_srli_si128(s, 8);
      sq[0] = sq[1];
      sum3 += 8;
      sum5 += 8;
      square_sum3 += 8;
      square_sum5 += 8;
      x += 8;
    } while (x < sum_stride);
    src += src_stride;
  } while (--y != 0);
}

template <int size>
inline void BoxSum(const uint8_t* src, const ptrdiff_t src_stride,
                   const int height, const ptrdiff_t sum_stride,
                   const ptrdiff_t width, uint16_t* sums,
                   uint32_t* square_sums) {
  static_assert(size == 3 || size == 5, "");
  constexpr int kOverreadInBytes =
      (size == 5) ? kOverreadInBytesPass1 : kOverreadInBytesPass2;
  int y = height;
  do {
    __m128i s, sq[2];
    s = LoadLo8Msan(src, kOverreadInBytes - width);
    sq[0] = VmullLo8(s, s);
    ptrdiff_t x = 0;
    do {
      __m128i row, row_sq[2];
      s = LoadHi8Msan(s, src + x + 8, x + 8 + kOverreadInBytes - width);
      sq[1] = VmullHi8(s, s);
      if (size == 3) {
        row = Sum3Horizontal(s);
        Sum3WHorizontal(sq, row_sq);
      } else {
        row = Sum5Horizontal(s);
        Sum5WHorizontal(sq, row_sq);
      }
      StoreAligned16(sums, row);
      StoreAligned16(square_sums + 0, row_sq[0]);
      StoreAligned16(square_sums + 4, row_sq[1]);
      s = _mm_srli_si128(s, 8);
      sq[0] = sq[1];
      sums += 8;
      square_sums += 8;
      x += 8;
    } while (x < sum_stride);
    src += src_stride;
  } while (--y != 0);
}

template <int n>
inline __m128i CalculateMa(const __m128i sum, const __m128i sum_sq,
                           const uint32_t scale) {
  // a = |sum_sq|
  // d = |sum|
  // p = (a * n < d * d) ? 0 : a * n - d * d;
  const __m128i dxd = _mm_madd_epi16(sum, sum);
  // _mm_mullo_epi32() has high latency. Using shifts and additions instead.
  // Some compilers could do this for us but we make this explicit.
  // return _mm_mullo_epi32(sum_sq, _mm_set1_epi32(n));
  __m128i axn = _mm_add_epi32(sum_sq, _mm_slli_epi32(sum_sq, 3));
  if (n == 25) axn = _mm_add_epi32(axn, _mm_slli_epi32(sum_sq, 4));
  const __m128i sub = _mm_sub_epi32(axn, dxd);
  const __m128i p = _mm_max_epi32(sub, _mm_setzero_si128());
  const __m128i pxs = _mm_mullo_epi32(p, _mm_set1_epi32(scale));
  return VrshrU32(pxs, kSgrProjScaleBits);
}

template <int n>
inline void CalculateIntermediate(const __m128i sum, const __m128i sum_sq[2],
                                  const uint32_t scale, __m128i* const ma,
                                  __m128i* const b) {
  constexpr uint32_t one_over_n =
      ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;
  const __m128i sum_lo = _mm_unpacklo_epi16(sum, _mm_setzero_si128());
  const __m128i sum_hi = _mm_unpackhi_epi16(sum, _mm_setzero_si128());
  const __m128i z0 = CalculateMa<n>(sum_lo, sum_sq[0], scale);
  const __m128i z1 = CalculateMa<n>(sum_hi, sum_sq[1], scale);
  const __m128i z01 = _mm_packus_epi32(z0, z1);
  const __m128i z = _mm_min_epu16(z01, _mm_set1_epi16(255));
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<0>(z)], 8);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<1>(z)], 9);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<2>(z)], 10);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<3>(z)], 11);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<4>(z)], 12);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<5>(z)], 13);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<6>(z)], 14);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<7>(z)], 15);
  // b = ma * b * one_over_n
  // |ma| = [0, 255]
  // |sum| is a box sum with radius 1 or 2.
  // For the first pass radius is 2. Maximum value is 5x5x255 = 6375.
  // For the second pass radius is 1. Maximum value is 3x3x255 = 2295.
  // |one_over_n| = ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n
  // When radius is 2 |n| is 25. |one_over_n| is 164.
  // When radius is 1 |n| is 9. |one_over_n| is 455.
  // |kSgrProjReciprocalBits| is 12.
  // Radius 2: 255 * 6375 * 164 >> 12 = 65088 (16 bits).
  // Radius 1: 255 * 2295 * 455 >> 12 = 65009 (16 bits).
  const __m128i maq = _mm_unpackhi_epi8(*ma, _mm_setzero_si128());
  const __m128i m0 = VmullLo16(maq, sum);
  const __m128i m1 = VmullHi16(maq, sum);
  const __m128i m2 = _mm_mullo_epi32(m0, _mm_set1_epi32(one_over_n));
  const __m128i m3 = _mm_mullo_epi32(m1, _mm_set1_epi32(one_over_n));
  const __m128i b_lo = VrshrU32(m2, kSgrProjReciprocalBits);
  const __m128i b_hi = VrshrU32(m3, kSgrProjReciprocalBits);
  *b = _mm_packus_epi32(b_lo, b_hi);
}

inline void CalculateIntermediate5(const __m128i s5[5], const __m128i sq5[5][2],
                                   const uint32_t scale, __m128i* const ma,
                                   __m128i* const b) {
  const __m128i sum = Sum5_16(s5);
  __m128i sum_sq[2];
  Sum5_32(sq5, sum_sq);
  CalculateIntermediate<25>(sum, sum_sq, scale, ma, b);
}

inline void CalculateIntermediate3(const __m128i s3[3], const __m128i sq3[3][2],
                                   const uint32_t scale, __m128i* const ma,
                                   __m128i* const b) {
  const __m128i sum = Sum3_16(s3);
  __m128i sum_sq[2];
  Sum3_32(sq3, sum_sq);
  CalculateIntermediate<9>(sum, sum_sq, scale, ma, b);
}

inline void Store343_444(const __m128i ma3, const __m128i b3[2],
                         const ptrdiff_t x, __m128i* const sum_ma343,
                         __m128i* const sum_ma444, __m128i sum_b343[2],
                         __m128i sum_b444[2], uint16_t* const ma343,
                         uint16_t* const ma444, uint32_t* const b343,
                         uint32_t* const b444) {
  __m128i s[3];
  Prepare3_8(ma3, s);
  const __m128i sum_ma111 = Sum3W_16(s);
  *sum_ma444 = _mm_slli_epi16(sum_ma111, 2);
  const __m128i sum333 = _mm_sub_epi16(*sum_ma444, sum_ma111);
  *sum_ma343 = VaddwLo8(sum333, s[1]);
  __m128i b[3], sum_b111[2];
  Prepare3_16(b3, b);
  sum_b111[0] = Sum3WLo_32(b);
  sum_b111[1] = Sum3WHi_32(b);
  sum_b444[0] = _mm_slli_epi32(sum_b111[0], 2);
  sum_b444[1] = _mm_slli_epi32(sum_b111[1], 2);
  sum_b343[0] = _mm_sub_epi32(sum_b444[0], sum_b111[0]);
  sum_b343[1] = _mm_sub_epi32(sum_b444[1], sum_b111[1]);
  sum_b343[0] = VaddwLo16(sum_b343[0], b[1]);
  sum_b343[1] = VaddwHi16(sum_b343[1], b[1]);
  StoreAligned16(ma343 + x, *sum_ma343);
  StoreAligned16(ma444 + x, *sum_ma444);
  StoreAligned16(b343 + x + 0, sum_b343[0]);
  StoreAligned16(b343 + x + 4, sum_b343[1]);
  StoreAligned16(b444 + x + 0, sum_b444[0]);
  StoreAligned16(b444 + x + 4, sum_b444[1]);
}

inline void Store343_444(const __m128i ma3, const __m128i b3[2],
                         const ptrdiff_t x, __m128i* const sum_ma343,
                         __m128i sum_b343[2], uint16_t* const ma343,
                         uint16_t* const ma444, uint32_t* const b343,
                         uint32_t* const b444) {
  __m128i sum_ma444, sum_b444[2];
  Store343_444(ma3, b3, x, sum_ma343, &sum_ma444, sum_b343, sum_b444, ma343,
               ma444, b343, b444);
}

inline void Store343_444(const __m128i ma3, const __m128i b3[2],
                         const ptrdiff_t x, uint16_t* const ma343,
                         uint16_t* const ma444, uint32_t* const b343,
                         uint32_t* const b444) {
  __m128i sum_ma343, sum_b343[2];
  Store343_444(ma3, b3, x, &sum_ma343, sum_b343, ma343, ma444, b343, b444);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess5(
    const uint8_t* const src0, const uint8_t* const src1, const ptrdiff_t width,
    const ptrdiff_t x, const uint32_t scale, uint16_t* const sum5[5],
    uint32_t* const square_sum5[5], __m128i s[2], __m128i sq[2][2],
    __m128i* const ma, __m128i* const b) {
  __m128i s5[5], sq5[5][2];
  s[0] = LoadHi8Msan(s[0], src0 + x + 8, x + 8 + kOverreadInBytesPass1 - width);
  s[1] = LoadHi8Msan(s[1], src1 + x + 8, x + 8 + kOverreadInBytesPass1 - width);
  sq[0][1] = VmullHi8(s[0], s[0]);
  sq[1][1] = VmullHi8(s[1], s[1]);
  s5[3] = Sum5Horizontal(s[0]);
  s5[4] = Sum5Horizontal(s[1]);
  Sum5WHorizontal(sq[0], sq5[3]);
  Sum5WHorizontal(sq[1], sq5[4]);
  StoreAligned16(sum5[3] + x, s5[3]);
  StoreAligned16(sum5[4] + x, s5[4]);
  StoreAligned16(square_sum5[3] + x + 0, sq5[3][0]);
  StoreAligned16(square_sum5[3] + x + 4, sq5[3][1]);
  StoreAligned16(square_sum5[4] + x + 0, sq5[4][0]);
  StoreAligned16(square_sum5[4] + x + 4, sq5[4][1]);
  s5[0] = LoadAligned16(sum5[0] + x);
  s5[1] = LoadAligned16(sum5[1] + x);
  s5[2] = LoadAligned16(sum5[2] + x);
  sq5[0][0] = LoadAligned16(square_sum5[0] + x + 0);
  sq5[0][1] = LoadAligned16(square_sum5[0] + x + 4);
  sq5[1][0] = LoadAligned16(square_sum5[1] + x + 0);
  sq5[1][1] = LoadAligned16(square_sum5[1] + x + 4);
  sq5[2][0] = LoadAligned16(square_sum5[2] + x + 0);
  sq5[2][1] = LoadAligned16(square_sum5[2] + x + 4);
  CalculateIntermediate5(s5, sq5, scale, ma, b);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess5LastRow(
    const uint8_t* const src, const ptrdiff_t width, const ptrdiff_t x,
    const uint32_t scale, const uint16_t* const sum5[5],
    const uint32_t* const square_sum5[5], __m128i s[2], __m128i sq[2],
    __m128i* const ma, __m128i* const b) {
  __m128i s5[5], sq5[5][2];
  *s = LoadHi8Msan(*s, src + x + 8, x + 8 + kOverreadInBytesPass1 - width);
  sq[1] = VmullHi8(*s, *s);
  s5[3] = s5[4] = Sum5Horizontal(*s);
  Sum5WHorizontal(sq, sq5[3]);
  sq5[4][0] = sq5[3][0];
  sq5[4][1] = sq5[3][1];
  s5[0] = LoadAligned16(sum5[0] + x);
  s5[1] = LoadAligned16(sum5[1] + x);
  s5[2] = LoadAligned16(sum5[2] + x);
  sq5[0][0] = LoadAligned16(square_sum5[0] + x + 0);
  sq5[0][1] = LoadAligned16(square_sum5[0] + x + 4);
  sq5[1][0] = LoadAligned16(square_sum5[1] + x + 0);
  sq5[1][1] = LoadAligned16(square_sum5[1] + x + 4);
  sq5[2][0] = LoadAligned16(square_sum5[2] + x + 0);
  sq5[2][1] = LoadAligned16(square_sum5[2] + x + 4);
  CalculateIntermediate5(s5, sq5, scale, ma, b);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess3(
    const uint8_t* const src, const ptrdiff_t width, const ptrdiff_t x,
    const uint32_t scale, uint16_t* const sum3[3],
    uint32_t* const square_sum3[3], __m128i* const s, __m128i sq[2],
    __m128i* const ma, __m128i* const b) {
  __m128i s3[3], sq3[3][2];
  *s = LoadHi8Msan(*s, src + x + 8, x + 8 + kOverreadInBytesPass2 - width);
  sq[1] = VmullHi8(*s, *s);
  s3[2] = Sum3Horizontal(*s);
  Sum3WHorizontal(sq, sq3[2]);
  StoreAligned16(sum3[2] + x, s3[2]);
  StoreAligned16(square_sum3[2] + x + 0, sq3[2][0]);
  StoreAligned16(square_sum3[2] + x + 4, sq3[2][1]);
  s3[0] = LoadAligned16(sum3[0] + x);
  s3[1] = LoadAligned16(sum3[1] + x);
  sq3[0][0] = LoadAligned16(square_sum3[0] + x + 0);
  sq3[0][1] = LoadAligned16(square_sum3[0] + x + 4);
  sq3[1][0] = LoadAligned16(square_sum3[1] + x + 0);
  sq3[1][1] = LoadAligned16(square_sum3[1] + x + 4);
  CalculateIntermediate3(s3, sq3, scale, ma, b);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess(
    const uint8_t* const src0, const uint8_t* const src1, const ptrdiff_t width,
    const ptrdiff_t x, const uint16_t scales[2], uint16_t* const sum3[4],
    uint16_t* const sum5[5], uint32_t* const square_sum3[4],
    uint32_t* const square_sum5[5], __m128i s[2], __m128i sq[2][2],
    __m128i* const ma3_0, __m128i* const ma3_1, __m128i* const b3_0,
    __m128i* const b3_1, __m128i* const ma5, __m128i* const b5) {
  __m128i s3[4], s5[5], sq3[4][2], sq5[5][2];
  s[0] = LoadHi8Msan(s[0], src0 + x + 8, x + 8 + kOverreadInBytesPass1 - width);
  s[1] = LoadHi8Msan(s[1], src1 + x + 8, x + 8 + kOverreadInBytesPass1 - width);
  sq[0][1] = VmullHi8(s[0], s[0]);
  sq[1][1] = VmullHi8(s[1], s[1]);
  SumHorizontal(s[0], sq[0], &s3[2], &s5[3], sq3[2], sq5[3]);
  SumHorizontal(s[1], sq[1], &s3[3], &s5[4], sq3[3], sq5[4]);
  StoreAligned16(sum3[2] + x, s3[2]);
  StoreAligned16(sum3[3] + x, s3[3]);
  StoreAligned16(square_sum3[2] + x + 0, sq3[2][0]);
  StoreAligned16(square_sum3[2] + x + 4, sq3[2][1]);
  StoreAligned16(square_sum3[3] + x + 0, sq3[3][0]);
  StoreAligned16(square_sum3[3] + x + 4, sq3[3][1]);
  StoreAligned16(sum5[3] + x, s5[3]);
  StoreAligned16(sum5[4] + x, s5[4]);
  StoreAligned16(square_sum5[3] + x + 0, sq5[3][0]);
  StoreAligned16(square_sum5[3] + x + 4, sq5[3][1]);
  StoreAligned16(square_sum5[4] + x + 0, sq5[4][0]);
  StoreAligned16(square_sum5[4] + x + 4, sq5[4][1]);
  s3[0] = LoadAligned16(sum3[0] + x);
  s3[1] = LoadAligned16(sum3[1] + x);
  sq3[0][0] = LoadAligned16(square_sum3[0] + x + 0);
  sq3[0][1] = LoadAligned16(square_sum3[0] + x + 4);
  sq3[1][0] = LoadAligned16(square_sum3[1] + x + 0);
  sq3[1][1] = LoadAligned16(square_sum3[1] + x + 4);
  s5[0] = LoadAligned16(sum5[0] + x);
  s5[1] = LoadAligned16(sum5[1] + x);
  s5[2] = LoadAligned16(sum5[2] + x);
  sq5[0][0] = LoadAligned16(square_sum5[0] + x + 0);
  sq5[0][1] = LoadAligned16(square_sum5[0] + x + 4);
  sq5[1][0] = LoadAligned16(square_sum5[1] + x + 0);
  sq5[1][1] = LoadAligned16(square_sum5[1] + x + 4);
  sq5[2][0] = LoadAligned16(square_sum5[2] + x + 0);
  sq5[2][1] = LoadAligned16(square_sum5[2] + x + 4);
  CalculateIntermediate3(s3, sq3, scales[1], ma3_0, b3_0);
  CalculateIntermediate3(s3 + 1, sq3 + 1, scales[1], ma3_1, b3_1);
  CalculateIntermediate5(s5, sq5, scales[0], ma5, b5);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcessLastRow(
    const uint8_t* const src, const ptrdiff_t width, const ptrdiff_t x,
    const uint16_t scales[2], const uint16_t* const sum3[4],
    const uint16_t* const sum5[5], const uint32_t* const square_sum3[4],
    const uint32_t* const square_sum5[5], __m128i* const s, __m128i sq[2],
    __m128i* const ma3, __m128i* const ma5, __m128i* const b3,
    __m128i* const b5) {
  __m128i s3[3], s5[5], sq3[3][2], sq5[5][2];
  *s = LoadHi8Msan(*s, src + x + 8, x + 8 + kOverreadInBytesPass1 - width);
  sq[1] = VmullHi8(*s, *s);
  SumHorizontal(*s, sq, &s3[2], &s5[3], sq3[2], sq5[3]);
  s5[0] = LoadAligned16(sum5[0] + x);
  s5[1] = LoadAligned16(sum5[1] + x);
  s5[2] = LoadAligned16(sum5[2] + x);
  s5[4] = s5[3];
  sq5[0][0] = LoadAligned16(square_sum5[0] + x + 0);
  sq5[0][1] = LoadAligned16(square_sum5[0] + x + 4);
  sq5[1][0] = LoadAligned16(square_sum5[1] + x + 0);
  sq5[1][1] = LoadAligned16(square_sum5[1] + x + 4);
  sq5[2][0] = LoadAligned16(square_sum5[2] + x + 0);
  sq5[2][1] = LoadAligned16(square_sum5[2] + x + 4);
  sq5[4][0] = sq5[3][0];
  sq5[4][1] = sq5[3][1];
  CalculateIntermediate5(s5, sq5, scales[0], ma5, b5);
  s3[0] = LoadAligned16(sum3[0] + x);
  s3[1] = LoadAligned16(sum3[1] + x);
  sq3[0][0] = LoadAligned16(square_sum3[0] + x + 0);
  sq3[0][1] = LoadAligned16(square_sum3[0] + x + 4);
  sq3[1][0] = LoadAligned16(square_sum3[1] + x + 0);
  sq3[1][1] = LoadAligned16(square_sum3[1] + x + 4);
  CalculateIntermediate3(s3, sq3, scales[1], ma3, b3);
}

inline void BoxSumFilterPreProcess5(const uint8_t* const src0,
                                    const uint8_t* const src1, const int width,
                                    const uint32_t scale,
                                    uint16_t* const sum5[5],
                                    uint32_t* const square_sum5[5],
                                    uint16_t* ma565, uint32_t* b565) {
  __m128i s[2], mas, sq[2][2], bs[2];
  s[0] = LoadLo8Msan(src0, kOverreadInBytesPass1 - width);
  s[1] = LoadLo8Msan(src1, kOverreadInBytesPass1 - width);
  sq[0][0] = VmullLo8(s[0], s[0]);
  sq[1][0] = VmullLo8(s[1], s[1]);
  BoxFilterPreProcess5(src0, src1, width, 0, scale, sum5, square_sum5, s, sq,
                       &mas, &bs[0]);

  int x = 0;
  do {
    s[0] = _mm_srli_si128(s[0], 8);
    s[1] = _mm_srli_si128(s[1], 8);
    sq[0][0] = sq[0][1];
    sq[1][0] = sq[1][1];
    mas = _mm_srli_si128(mas, 8);
    BoxFilterPreProcess5(src0, src1, width, x + 8, scale, sum5, square_sum5, s,
                         sq, &mas, &bs[1]);
    const __m128i ma = Sum565(mas);
    __m128i b[2];
    Sum565W(bs, b);
    StoreAligned16(ma565, ma);
    StoreAligned16(b565 + 0, b[0]);
    StoreAligned16(b565 + 4, b[1]);
    bs[0] = bs[1];
    ma565 += 8;
    b565 += 8;
    x += 8;
  } while (x < width);
}

template <bool calculate444>
LIBGAV1_ALWAYS_INLINE void BoxSumFilterPreProcess3(
    const uint8_t* const src, const int width, const uint32_t scale,
    uint16_t* const sum3[3], uint32_t* const square_sum3[3], uint16_t* ma343,
    uint16_t* ma444, uint32_t* b343, uint32_t* b444) {
  __m128i s, mas, sq[2], bs[2];
  s = LoadLo8Msan(src, kOverreadInBytesPass2 - width);
  sq[0] = VmullLo8(s, s);
  BoxFilterPreProcess3(src, width, 0, scale, sum3, square_sum3, &s, sq, &mas,
                       &bs[0]);

  int x = 0;
  do {
    s = _mm_srli_si128(s, 8);
    sq[0] = sq[1];
    mas = _mm_srli_si128(mas, 8);
    BoxFilterPreProcess3(src, width, x + 8, scale, sum3, square_sum3, &s, sq,
                         &mas, &bs[1]);
    if (calculate444) {  // NOLINT(readability-simplify-boolean-expr)
      Store343_444(mas, bs, 0, ma343, ma444, b343, b444);
      ma444 += 8;
      b444 += 8;
    } else {
      const __m128i ma = Sum343(mas);
      __m128i b[2];
      Sum343W(bs, b);
      StoreAligned16(ma343, ma);
      StoreAligned16(b343 + 0, b[0]);
      StoreAligned16(b343 + 4, b[1]);
    }
    bs[0] = bs[1];
    ma343 += 8;
    b343 += 8;
    x += 8;
  } while (x < width);
}

inline void BoxSumFilterPreProcess(
    const uint8_t* const src0, const uint8_t* const src1, const int width,
    const uint16_t scales[2], uint16_t* const sum3[4], uint16_t* const sum5[5],
    uint32_t* const square_sum3[4], uint32_t* const square_sum5[5],
    uint16_t* const ma343[4], uint16_t* const ma444[2], uint16_t* ma565,
    uint32_t* const b343[4], uint32_t* const b444[2], uint32_t* b565) {
  __m128i s[2], ma3[2], ma5, sq[2][2], b3[2][2], b5[2];
  s[0] = LoadLo8Msan(src0, kOverreadInBytesPass1 - width);
  s[1] = LoadLo8Msan(src1, kOverreadInBytesPass1 - width);
  sq[0][0] = VmullLo8(s[0], s[0]);
  sq[1][0] = VmullLo8(s[1], s[1]);
  BoxFilterPreProcess(src0, src1, width, 0, scales, sum3, sum5, square_sum3,
                      square_sum5, s, sq, &ma3[0], &ma3[1], &b3[0][0],
                      &b3[1][0], &ma5, &b5[0]);

  int x = 0;
  do {
    s[0] = _mm_srli_si128(s[0], 8);
    s[1] = _mm_srli_si128(s[1], 8);
    sq[0][0] = sq[0][1];
    sq[1][0] = sq[1][1];
    ma3[0] = _mm_srli_si128(ma3[0], 8);
    ma3[1] = _mm_srli_si128(ma3[1], 8);
    ma5 = _mm_srli_si128(ma5, 8);
    BoxFilterPreProcess(src0, src1, width, x + 8, scales, sum3, sum5,
                        square_sum3, square_sum5, s, sq, &ma3[0], &ma3[1],
                        &b3[0][1], &b3[1][1], &ma5, &b5[1]);
    __m128i ma = Sum343(ma3[0]);
    __m128i b[2];
    Sum343W(b3[0], b);
    StoreAligned16(ma343[0] + x, ma);
    StoreAligned16(b343[0] + x, b[0]);
    StoreAligned16(b343[0] + x + 4, b[1]);
    Store343_444(ma3[1], b3[1], x, ma343[1], ma444[0], b343[1], b444[0]);
    ma = Sum565(ma5);
    Sum565W(b5, b);
    StoreAligned16(ma565, ma);
    StoreAligned16(b565 + 0, b[0]);
    StoreAligned16(b565 + 4, b[1]);
    b3[0][0] = b3[0][1];
    b3[1][0] = b3[1][1];
    b5[0] = b5[1];
    ma565 += 8;
    b565 += 8;
    x += 8;
  } while (x < width);
}

template <int shift>
inline __m128i FilterOutput(const __m128i ma_x_src, const __m128i b) {
  // ma: 255 * 32 = 8160 (13 bits)
  // b: 65088 * 32 = 2082816 (21 bits)
  // v: b - ma * 255 (22 bits)
  const __m128i v = _mm_sub_epi32(b, ma_x_src);
  // kSgrProjSgrBits = 8
  // kSgrProjRestoreBits = 4
  // shift = 4 or 5
  // v >> 8 or 9 (13 bits)
  return VrshrS32(v, kSgrProjSgrBits + shift - kSgrProjRestoreBits);
}

template <int shift>
inline __m128i CalculateFilteredOutput(const __m128i src, const __m128i ma,
                                       const __m128i b[2]) {
  const __m128i src_u16 = _mm_unpacklo_epi8(src, _mm_setzero_si128());
  const __m128i ma_x_src_lo = VmullLo16(ma, src_u16);
  const __m128i ma_x_src_hi = VmullHi16(ma, src_u16);
  const __m128i dst_lo = FilterOutput<shift>(ma_x_src_lo, b[0]);
  const __m128i dst_hi = FilterOutput<shift>(ma_x_src_hi, b[1]);
  return _mm_packs_epi32(dst_lo, dst_hi);  // 13 bits
}

inline __m128i CalculateFilteredOutputPass1(const __m128i s, __m128i ma[2],
                                            __m128i b[2][2]) {
  const __m128i ma_sum = _mm_add_epi16(ma[0], ma[1]);
  __m128i b_sum[2];
  b_sum[0] = _mm_add_epi32(b[0][0], b[1][0]);
  b_sum[1] = _mm_add_epi32(b[0][1], b[1][1]);
  return CalculateFilteredOutput<5>(s, ma_sum, b_sum);
}

inline __m128i CalculateFilteredOutputPass2(const __m128i s, __m128i ma[3],
                                            __m128i b[3][2]) {
  const __m128i ma_sum = Sum3_16(ma);
  __m128i b_sum[2];
  Sum3_32(b, b_sum);
  return CalculateFilteredOutput<5>(s, ma_sum, b_sum);
}

inline void SelfGuidedFinal(const __m128i src, const __m128i v[2],
                            uint8_t* const dst) {
  const __m128i v_lo =
      VrshrS32(v[0], kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const __m128i v_hi =
      VrshrS32(v[1], kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const __m128i vv = _mm_packs_epi32(v_lo, v_hi);
  const __m128i s = _mm_unpacklo_epi8(src, _mm_setzero_si128());
  const __m128i d = _mm_add_epi16(s, vv);
  StoreLo8(dst, _mm_packus_epi16(d, d));
}

inline void SelfGuidedDoubleMultiplier(const __m128i src,
                                       const __m128i filter[2], const int w0,
                                       const int w2, uint8_t* const dst) {
  __m128i v[2];
  const __m128i w0_w2 = _mm_set1_epi32((w2 << 16) | static_cast<uint16_t>(w0));
  const __m128i f_lo = _mm_unpacklo_epi16(filter[0], filter[1]);
  const __m128i f_hi = _mm_unpackhi_epi16(filter[0], filter[1]);
  v[0] = _mm_madd_epi16(w0_w2, f_lo);
  v[1] = _mm_madd_epi16(w0_w2, f_hi);
  SelfGuidedFinal(src, v, dst);
}

inline void SelfGuidedSingleMultiplier(const __m128i src, const __m128i filter,
                                       const int w0, uint8_t* const dst) {
  // weight: -96 to 96 (Sgrproj_Xqd_Min/Max)
  __m128i v[2];
  v[0] = VmullNLo8(filter, w0);
  v[1] = VmullNHi8(filter, w0);
  SelfGuidedFinal(src, v, dst);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPass1(
    const uint8_t* const src, const uint8_t* const src0,
    const uint8_t* const src1, const ptrdiff_t stride, uint16_t* const sum5[5],
    uint32_t* const square_sum5[5], const int width, const uint32_t scale,
    const int16_t w0, uint16_t* const ma565[2], uint32_t* const b565[2],
    uint8_t* const dst) {
  __m128i s[2], mas, sq[2][2], bs[2];
  s[0] = LoadLo8Msan(src0, kOverreadInBytesPass1 - width);
  s[1] = LoadLo8Msan(src1, kOverreadInBytesPass1 - width);
  sq[0][0] = VmullLo8(s[0], s[0]);
  sq[1][0] = VmullLo8(s[1], s[1]);
  BoxFilterPreProcess5(src0, src1, width, 0, scale, sum5, square_sum5, s, sq,
                       &mas, &bs[0]);

  int x = 0;
  do {
    s[0] = _mm_srli_si128(s[0], 8);
    s[1] = _mm_srli_si128(s[1], 8);
    sq[0][0] = sq[0][1];
    sq[1][0] = sq[1][1];
    mas = _mm_srli_si128(mas, 8);
    BoxFilterPreProcess5(src0, src1, width, x + 8, scale, sum5, square_sum5, s,
                         sq, &mas, &bs[1]);
    __m128i ma[2], b[2][2];
    ma[1] = Sum565(mas);
    Sum565W(bs, b[1]);
    StoreAligned16(ma565[1] + x, ma[1]);
    StoreAligned16(b565[1] + x + 0, b[1][0]);
    StoreAligned16(b565[1] + x + 4, b[1][1]);
    const __m128i sr0 = LoadLo8(src + x);
    const __m128i sr1 = LoadLo8(src + stride + x);
    __m128i p0, p1;
    ma[0] = LoadAligned16(ma565[0] + x);
    b[0][0] = LoadAligned16(b565[0] + x + 0);
    b[0][1] = LoadAligned16(b565[0] + x + 4);
    p0 = CalculateFilteredOutputPass1(sr0, ma, b);
    p1 = CalculateFilteredOutput<4>(sr1, ma[1], b[1]);
    SelfGuidedSingleMultiplier(sr0, p0, w0, dst + x);
    SelfGuidedSingleMultiplier(sr1, p1, w0, dst + stride + x);
    bs[0] = bs[1];
    x += 8;
  } while (x < width);
}

inline void BoxFilterPass1LastRow(const uint8_t* const src,
                                  const uint8_t* const src0, const int width,
                                  const uint32_t scale, const int16_t w0,
                                  uint16_t* const sum5[5],
                                  uint32_t* const square_sum5[5],
                                  uint16_t* ma565, uint32_t* b565,
                                  uint8_t* const dst) {
  __m128i s, mas, sq[2], bs[2];
  s = LoadLo8Msan(src0, kOverreadInBytesPass1 - width);
  sq[0] = VmullLo8(s, s);
  BoxFilterPreProcess5LastRow(src0, width, 0, scale, sum5, square_sum5, &s, sq,
                              &mas, &bs[0]);

  int x = 0;
  do {
    s = _mm_srli_si128(s, 8);
    sq[0] = sq[1];
    mas = _mm_srli_si128(mas, 8);
    BoxFilterPreProcess5LastRow(src0, width, x + 8, scale, sum5, square_sum5,
                                &s, sq, &mas, &bs[1]);
    __m128i ma[2], b[2][2];
    ma[1] = Sum565(mas);
    Sum565W(bs, b[1]);
    bs[0] = bs[1];
    ma[0] = LoadAligned16(ma565);
    b[0][0] = LoadAligned16(b565 + 0);
    b[0][1] = LoadAligned16(b565 + 4);
    const __m128i sr = LoadLo8(src + x);
    const __m128i p = CalculateFilteredOutputPass1(sr, ma, b);
    SelfGuidedSingleMultiplier(sr, p, w0, dst + x);
    ma565 += 8;
    b565 += 8;
    x += 8;
  } while (x < width);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPass2(
    const uint8_t* const src, const uint8_t* const src0, const int width,
    const uint32_t scale, const int16_t w0, uint16_t* const sum3[3],
    uint32_t* const square_sum3[3], uint16_t* const ma343[3],
    uint16_t* const ma444[2], uint32_t* const b343[3], uint32_t* const b444[2],
    uint8_t* const dst) {
  __m128i s, mas, sq[2], bs[2];
  s = LoadLo8Msan(src0, kOverreadInBytesPass2 - width);
  sq[0] = VmullLo8(s, s);
  BoxFilterPreProcess3(src0, width, 0, scale, sum3, square_sum3, &s, sq, &mas,
                       &bs[0]);

  int x = 0;
  do {
    s = _mm_srli_si128(s, 8);
    sq[0] = sq[1];
    mas = _mm_srli_si128(mas, 8);
    BoxFilterPreProcess3(src0, width, x + 8, scale, sum3, square_sum3, &s, sq,
                         &mas, &bs[1]);
    __m128i ma[3], b[3][2];
    Store343_444(mas, bs, x, &ma[2], b[2], ma343[2], ma444[1], b343[2],
                 b444[1]);
    const __m128i sr = LoadLo8(src + x);
    ma[0] = LoadAligned16(ma343[0] + x);
    ma[1] = LoadAligned16(ma444[0] + x);
    b[0][0] = LoadAligned16(b343[0] + x + 0);
    b[0][1] = LoadAligned16(b343[0] + x + 4);
    b[1][0] = LoadAligned16(b444[0] + x + 0);
    b[1][1] = LoadAligned16(b444[0] + x + 4);
    const __m128i p = CalculateFilteredOutputPass2(sr, ma, b);
    SelfGuidedSingleMultiplier(sr, p, w0, dst + x);
    bs[0] = bs[1];
    x += 8;
  } while (x < width);
}

LIBGAV1_ALWAYS_INLINE void BoxFilter(
    const uint8_t* const src, const uint8_t* const src0,
    const uint8_t* const src1, const ptrdiff_t stride, const int width,
    const uint16_t scales[2], const int16_t w0, const int16_t w2,
    uint16_t* const sum3[4], uint16_t* const sum5[5],
    uint32_t* const square_sum3[4], uint32_t* const square_sum5[5],
    uint16_t* const ma343[4], uint16_t* const ma444[3],
    uint16_t* const ma565[2], uint32_t* const b343[4], uint32_t* const b444[3],
    uint32_t* const b565[2], uint8_t* const dst) {
  __m128i s[2], ma3[2], ma5, sq[2][2], b3[2][2], b5[2];
  s[0] = LoadLo8Msan(src0, kOverreadInBytesPass1 - width);
  s[1] = LoadLo8Msan(src1, kOverreadInBytesPass1 - width);
  sq[0][0] = VmullLo8(s[0], s[0]);
  sq[1][0] = VmullLo8(s[1], s[1]);
  BoxFilterPreProcess(src0, src1, width, 0, scales, sum3, sum5, square_sum3,
                      square_sum5, s, sq, &ma3[0], &ma3[1], &b3[0][0],
                      &b3[1][0], &ma5, &b5[0]);

  int x = 0;
  do {
    s[0] = _mm_srli_si128(s[0], 8);
    s[1] = _mm_srli_si128(s[1], 8);
    sq[0][0] = sq[0][1];
    sq[1][0] = sq[1][1];
    ma3[0] = _mm_srli_si128(ma3[0], 8);
    ma3[1] = _mm_srli_si128(ma3[1], 8);
    ma5 = _mm_srli_si128(ma5, 8);
    BoxFilterPreProcess(src0, src1, width, x + 8, scales, sum3, sum5,
                        square_sum3, square_sum5, s, sq, &ma3[0], &ma3[1],
                        &b3[0][1], &b3[1][1], &ma5, &b5[1]);
    __m128i ma[3][3], b[3][3][2];
    Store343_444(ma3[0], b3[0], x, &ma[1][2], &ma[2][1], b[1][2], b[2][1],
                 ma343[2], ma444[1], b343[2], b444[1]);
    Store343_444(ma3[1], b3[1], x, &ma[2][2], b[2][2], ma343[3], ma444[2],
                 b343[3], b444[2]);
    ma[0][1] = Sum565(ma5);
    Sum565W(b5, b[0][1]);
    StoreAligned16(ma565[1] + x, ma[0][1]);
    StoreAligned16(b565[1] + x, b[0][1][0]);
    StoreAligned16(b565[1] + x + 4, b[0][1][1]);
    b3[0][0] = b3[0][1];
    b3[1][0] = b3[1][1];
    b5[0] = b5[1];
    __m128i p[2][2];
    const __m128i sr0 = LoadLo8(src + x);
    const __m128i sr1 = LoadLo8(src + stride + x);
    ma[0][0] = LoadAligned16(ma565[0] + x);
    b[0][0][0] = LoadAligned16(b565[0] + x);
    b[0][0][1] = LoadAligned16(b565[0] + x + 4);
    p[0][0] = CalculateFilteredOutputPass1(sr0, ma[0], b[0]);
    p[1][0] = CalculateFilteredOutput<4>(sr1, ma[0][1], b[0][1]);
    ma[1][0] = LoadAligned16(ma343[0] + x);
    ma[1][1] = LoadAligned16(ma444[0] + x);
    b[1][0][0] = LoadAligned16(b343[0] + x);
    b[1][0][1] = LoadAligned16(b343[0] + x + 4);
    b[1][1][0] = LoadAligned16(b444[0] + x);
    b[1][1][1] = LoadAligned16(b444[0] + x + 4);
    p[0][1] = CalculateFilteredOutputPass2(sr0, ma[1], b[1]);
    ma[2][0] = LoadAligned16(ma343[1] + x);
    b[2][0][0] = LoadAligned16(b343[1] + x);
    b[2][0][1] = LoadAligned16(b343[1] + x + 4);
    p[1][1] = CalculateFilteredOutputPass2(sr1, ma[2], b[2]);
    SelfGuidedDoubleMultiplier(sr0, p[0], w0, w2, dst + x);
    SelfGuidedDoubleMultiplier(sr1, p[1], w0, w2, dst + stride + x);
    x += 8;
  } while (x < width);
}

inline void BoxFilterLastRow(
    const uint8_t* const src, const uint8_t* const src0, const int width,
    const uint16_t scales[2], const int16_t w0, const int16_t w2,
    uint16_t* const sum3[4], uint16_t* const sum5[5],
    uint32_t* const square_sum3[4], uint32_t* const square_sum5[5],
    uint16_t* const ma343[4], uint16_t* const ma444[3],
    uint16_t* const ma565[2], uint32_t* const b343[4], uint32_t* const b444[3],
    uint32_t* const b565[2], uint8_t* const dst) {
  __m128i s, ma3, ma5, sq[2], b3[2], b5[2], ma[3], b[3][2];
  s = LoadLo8Msan(src0, kOverreadInBytesPass1 - width);
  sq[0] = VmullLo8(s, s);
  BoxFilterPreProcessLastRow(src0, width, 0, scales, sum3, sum5, square_sum3,
                             square_sum5, &s, sq, &ma3, &ma5, &b3[0], &b5[0]);

  int x = 0;
  do {
    s = _mm_srli_si128(s, 8);
    sq[0] = sq[1];
    ma3 = _mm_srli_si128(ma3, 8);
    ma5 = _mm_srli_si128(ma5, 8);
    BoxFilterPreProcessLastRow(src0, width, x + 8, scales, sum3, sum5,
                               square_sum3, square_sum5, &s, sq, &ma3, &ma5,
                               &b3[1], &b5[1]);
    ma[1] = Sum565(ma5);
    Sum565W(b5, b[1]);
    b5[0] = b5[1];
    ma[2] = Sum343(ma3);
    Sum343W(b3, b[2]);
    b3[0] = b3[1];
    const __m128i sr = LoadLo8(src + x);
    __m128i p[2];
    ma[0] = LoadAligned16(ma565[0] + x);
    b[0][0] = LoadAligned16(b565[0] + x + 0);
    b[0][1] = LoadAligned16(b565[0] + x + 4);
    p[0] = CalculateFilteredOutputPass1(sr, ma, b);
    ma[0] = LoadAligned16(ma343[0] + x);
    ma[1] = LoadAligned16(ma444[0] + x);
    b[0][0] = LoadAligned16(b343[0] + x + 0);
    b[0][1] = LoadAligned16(b343[0] + x + 4);
    b[1][0] = LoadAligned16(b444[0] + x + 0);
    b[1][1] = LoadAligned16(b444[0] + x + 4);
    p[1] = CalculateFilteredOutputPass2(sr, ma, b);
    SelfGuidedDoubleMultiplier(sr, p, w0, w2, dst + x);
    x += 8;
  } while (x < width);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterProcess(
    const RestorationUnitInfo& restoration_info, const uint8_t* src,
    const uint8_t* const top_border, const uint8_t* bottom_border,
    const ptrdiff_t stride, const int width, const int height,
    SgrBuffer* const sgr_buffer, uint8_t* dst) {
  const auto temp_stride = Align<ptrdiff_t>(width, 8);
  const ptrdiff_t sum_stride = temp_stride + 8;
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  const uint16_t* const scales = kSgrScaleParameter[sgr_proj_index];  // < 2^12.
  const int16_t w0 = restoration_info.sgr_proj_info.multiplier[0];
  const int16_t w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int16_t w2 = (1 << kSgrProjPrecisionBits) - w0 - w1;
  uint16_t *sum3[4], *sum5[5], *ma343[4], *ma444[3], *ma565[2];
  uint32_t *square_sum3[4], *square_sum5[5], *b343[4], *b444[3], *b565[2];
  sum3[0] = sgr_buffer->sum3;
  square_sum3[0] = sgr_buffer->square_sum3;
  ma343[0] = sgr_buffer->ma343;
  b343[0] = sgr_buffer->b343;
  for (int i = 1; i <= 3; ++i) {
    sum3[i] = sum3[i - 1] + sum_stride;
    square_sum3[i] = square_sum3[i - 1] + sum_stride;
    ma343[i] = ma343[i - 1] + temp_stride;
    b343[i] = b343[i - 1] + temp_stride;
  }
  sum5[0] = sgr_buffer->sum5;
  square_sum5[0] = sgr_buffer->square_sum5;
  for (int i = 1; i <= 4; ++i) {
    sum5[i] = sum5[i - 1] + sum_stride;
    square_sum5[i] = square_sum5[i - 1] + sum_stride;
  }
  ma444[0] = sgr_buffer->ma444;
  b444[0] = sgr_buffer->b444;
  for (int i = 1; i <= 2; ++i) {
    ma444[i] = ma444[i - 1] + temp_stride;
    b444[i] = b444[i - 1] + temp_stride;
  }
  ma565[0] = sgr_buffer->ma565;
  ma565[1] = ma565[0] + temp_stride;
  b565[0] = sgr_buffer->b565;
  b565[1] = b565[0] + temp_stride;
  assert(scales[0] != 0);
  assert(scales[1] != 0);
  BoxSum(top_border, stride, 2, sum_stride, width, sum3[0], sum5[1],
         square_sum3[0], square_sum5[1]);
  sum5[0] = sum5[1];
  square_sum5[0] = square_sum5[1];
  const uint8_t* const s = (height > 1) ? src + stride : bottom_border;
  BoxSumFilterPreProcess(src, s, width, scales, sum3, sum5, square_sum3,
                         square_sum5, ma343, ma444, ma565[0], b343, b444,
                         b565[0]);
  sum5[0] = sgr_buffer->sum5;
  square_sum5[0] = sgr_buffer->square_sum5;

  for (int y = (height >> 1) - 1; y > 0; --y) {
    Circulate4PointersBy2<uint16_t>(sum3);
    Circulate4PointersBy2<uint32_t>(square_sum3);
    Circulate5PointersBy2<uint16_t>(sum5);
    Circulate5PointersBy2<uint32_t>(square_sum5);
    BoxFilter(src + 3, src + 2 * stride, src + 3 * stride, stride, width,
              scales, w0, w2, sum3, sum5, square_sum3, square_sum5, ma343,
              ma444, ma565, b343, b444, b565, dst);
    src += 2 * stride;
    dst += 2 * stride;
    Circulate4PointersBy2<uint16_t>(ma343);
    Circulate4PointersBy2<uint32_t>(b343);
    std::swap(ma444[0], ma444[2]);
    std::swap(b444[0], b444[2]);
    std::swap(ma565[0], ma565[1]);
    std::swap(b565[0], b565[1]);
  }

  Circulate4PointersBy2<uint16_t>(sum3);
  Circulate4PointersBy2<uint32_t>(square_sum3);
  Circulate5PointersBy2<uint16_t>(sum5);
  Circulate5PointersBy2<uint32_t>(square_sum5);
  if ((height & 1) == 0 || height > 1) {
    const uint8_t* sr[2];
    if ((height & 1) == 0) {
      sr[0] = bottom_border;
      sr[1] = bottom_border + stride;
    } else {
      sr[0] = src + 2 * stride;
      sr[1] = bottom_border;
    }
    BoxFilter(src + 3, sr[0], sr[1], stride, width, scales, w0, w2, sum3, sum5,
              square_sum3, square_sum5, ma343, ma444, ma565, b343, b444, b565,
              dst);
  }
  if ((height & 1) != 0) {
    if (height > 1) {
      src += 2 * stride;
      dst += 2 * stride;
      Circulate4PointersBy2<uint16_t>(sum3);
      Circulate4PointersBy2<uint32_t>(square_sum3);
      Circulate5PointersBy2<uint16_t>(sum5);
      Circulate5PointersBy2<uint32_t>(square_sum5);
      Circulate4PointersBy2<uint16_t>(ma343);
      Circulate4PointersBy2<uint32_t>(b343);
      std::swap(ma444[0], ma444[2]);
      std::swap(b444[0], b444[2]);
      std::swap(ma565[0], ma565[1]);
      std::swap(b565[0], b565[1]);
    }
    BoxFilterLastRow(src + 3, bottom_border + stride, width, scales, w0, w2,
                     sum3, sum5, square_sum3, square_sum5, ma343, ma444, ma565,
                     b343, b444, b565, dst);
  }
}

inline void BoxFilterProcessPass1(const RestorationUnitInfo& restoration_info,
                                  const uint8_t* src,
                                  const uint8_t* const top_border,
                                  const uint8_t* bottom_border,
                                  const ptrdiff_t stride, const int width,
                                  const int height, SgrBuffer* const sgr_buffer,
                                  uint8_t* dst) {
  const auto temp_stride = Align<ptrdiff_t>(width, 8);
  const ptrdiff_t sum_stride = temp_stride + 8;
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  const uint32_t scale = kSgrScaleParameter[sgr_proj_index][0];  // < 2^12.
  const int16_t w0 = restoration_info.sgr_proj_info.multiplier[0];
  uint16_t *sum5[5], *ma565[2];
  uint32_t *square_sum5[5], *b565[2];
  sum5[0] = sgr_buffer->sum5;
  square_sum5[0] = sgr_buffer->square_sum5;
  for (int i = 1; i <= 4; ++i) {
    sum5[i] = sum5[i - 1] + sum_stride;
    square_sum5[i] = square_sum5[i - 1] + sum_stride;
  }
  ma565[0] = sgr_buffer->ma565;
  ma565[1] = ma565[0] + temp_stride;
  b565[0] = sgr_buffer->b565;
  b565[1] = b565[0] + temp_stride;
  assert(scale != 0);
  BoxSum<5>(top_border, stride, 2, sum_stride, width, sum5[1], square_sum5[1]);
  sum5[0] = sum5[1];
  square_sum5[0] = square_sum5[1];
  const uint8_t* const s = (height > 1) ? src + stride : bottom_border;
  BoxSumFilterPreProcess5(src, s, width, scale, sum5, square_sum5, ma565[0],
                          b565[0]);
  sum5[0] = sgr_buffer->sum5;
  square_sum5[0] = sgr_buffer->square_sum5;

  for (int y = (height >> 1) - 1; y > 0; --y) {
    Circulate5PointersBy2<uint16_t>(sum5);
    Circulate5PointersBy2<uint32_t>(square_sum5);
    BoxFilterPass1(src + 3, src + 2 * stride, src + 3 * stride, stride, sum5,
                   square_sum5, width, scale, w0, ma565, b565, dst);
    src += 2 * stride;
    dst += 2 * stride;
    std::swap(ma565[0], ma565[1]);
    std::swap(b565[0], b565[1]);
  }

  Circulate5PointersBy2<uint16_t>(sum5);
  Circulate5PointersBy2<uint32_t>(square_sum5);
  if ((height & 1) == 0 || height > 1) {
    const uint8_t* sr[2];
    if ((height & 1) == 0) {
      sr[0] = bottom_border;
      sr[1] = bottom_border + stride;
    } else {
      sr[0] = src + 2 * stride;
      sr[1] = bottom_border;
    }
    BoxFilterPass1(src + 3, sr[0], sr[1], stride, sum5, square_sum5, width,
                   scale, w0, ma565, b565, dst);
  }
  if ((height & 1) != 0) {
    src += 3;
    if (height > 1) {
      src += 2 * stride;
      dst += 2 * stride;
      std::swap(ma565[0], ma565[1]);
      std::swap(b565[0], b565[1]);
      Circulate5PointersBy2<uint16_t>(sum5);
      Circulate5PointersBy2<uint32_t>(square_sum5);
    }
    BoxFilterPass1LastRow(src, bottom_border + stride, width, scale, w0, sum5,
                          square_sum5, ma565[0], b565[0], dst);
  }
}

inline void BoxFilterProcessPass2(const RestorationUnitInfo& restoration_info,
                                  const uint8_t* src,
                                  const uint8_t* const top_border,
                                  const uint8_t* bottom_border,
                                  const ptrdiff_t stride, const int width,
                                  const int height, SgrBuffer* const sgr_buffer,
                                  uint8_t* dst) {
  assert(restoration_info.sgr_proj_info.multiplier[0] == 0);
  const auto temp_stride = Align<ptrdiff_t>(width, 8);
  const ptrdiff_t sum_stride = temp_stride + 8;
  const int16_t w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int16_t w0 = (1 << kSgrProjPrecisionBits) - w1;
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  const uint32_t scale = kSgrScaleParameter[sgr_proj_index][1];  // < 2^12.
  uint16_t *sum3[3], *ma343[3], *ma444[2];
  uint32_t *square_sum3[3], *b343[3], *b444[2];
  sum3[0] = sgr_buffer->sum3;
  square_sum3[0] = sgr_buffer->square_sum3;
  ma343[0] = sgr_buffer->ma343;
  b343[0] = sgr_buffer->b343;
  for (int i = 1; i <= 2; ++i) {
    sum3[i] = sum3[i - 1] + sum_stride;
    square_sum3[i] = square_sum3[i - 1] + sum_stride;
    ma343[i] = ma343[i - 1] + temp_stride;
    b343[i] = b343[i - 1] + temp_stride;
  }
  ma444[0] = sgr_buffer->ma444;
  ma444[1] = ma444[0] + temp_stride;
  b444[0] = sgr_buffer->b444;
  b444[1] = b444[0] + temp_stride;
  assert(scale != 0);
  BoxSum<3>(top_border, stride, 2, sum_stride, width, sum3[0], square_sum3[0]);
  BoxSumFilterPreProcess3<false>(src, width, scale, sum3, square_sum3, ma343[0],
                                 nullptr, b343[0], nullptr);
  Circulate3PointersBy1<uint16_t>(sum3);
  Circulate3PointersBy1<uint32_t>(square_sum3);
  const uint8_t* s;
  if (height > 1) {
    s = src + stride;
  } else {
    s = bottom_border;
    bottom_border += stride;
  }
  BoxSumFilterPreProcess3<true>(s, width, scale, sum3, square_sum3, ma343[1],
                                ma444[0], b343[1], b444[0]);

  for (int y = height - 2; y > 0; --y) {
    Circulate3PointersBy1<uint16_t>(sum3);
    Circulate3PointersBy1<uint32_t>(square_sum3);
    BoxFilterPass2(src + 2, src + 2 * stride, width, scale, w0, sum3,
                   square_sum3, ma343, ma444, b343, b444, dst);
    src += stride;
    dst += stride;
    Circulate3PointersBy1<uint16_t>(ma343);
    Circulate3PointersBy1<uint32_t>(b343);
    std::swap(ma444[0], ma444[1]);
    std::swap(b444[0], b444[1]);
  }

  int y = std::min(height, 2);
  src += 2;
  do {
    Circulate3PointersBy1<uint16_t>(sum3);
    Circulate3PointersBy1<uint32_t>(square_sum3);
    BoxFilterPass2(src, bottom_border, width, scale, w0, sum3, square_sum3,
                   ma343, ma444, b343, b444, dst);
    src += stride;
    dst += stride;
    bottom_border += stride;
    Circulate3PointersBy1<uint16_t>(ma343);
    Circulate3PointersBy1<uint32_t>(b343);
    std::swap(ma444[0], ma444[1]);
    std::swap(b444[0], b444[1]);
  } while (--y != 0);
}

// If |width| is non-multiple of 8, up to 7 more pixels are written to |dest| in
// the end of each row. It is safe to overwrite the output as it will not be
// part of the visible frame.
void SelfGuidedFilter_SSE4_1(
    const RestorationUnitInfo& restoration_info, const void* const source,
    const void* const top_border, const void* const bottom_border,
    const ptrdiff_t stride, const int width, const int height,
    RestorationBuffer* const restoration_buffer, void* const dest) {
  const int index = restoration_info.sgr_proj_info.index;
  const int radius_pass_0 = kSgrProjParams[index][0];  // 2 or 0
  const int radius_pass_1 = kSgrProjParams[index][2];  // 1 or 0
  const auto* const src = static_cast<const uint8_t*>(source);
  const auto* top = static_cast<const uint8_t*>(top_border);
  const auto* bottom = static_cast<const uint8_t*>(bottom_border);
  auto* const dst = static_cast<uint8_t*>(dest);
  SgrBuffer* const sgr_buffer = &restoration_buffer->sgr_buffer;
  if (radius_pass_1 == 0) {
    // |radius_pass_0| and |radius_pass_1| cannot both be 0, so we have the
    // following assertion.
    assert(radius_pass_0 != 0);
    BoxFilterProcessPass1(restoration_info, src - 3, top - 3, bottom - 3,
                          stride, width, height, sgr_buffer, dst);
  } else if (radius_pass_0 == 0) {
    BoxFilterProcessPass2(restoration_info, src - 2, top - 2, bottom - 2,
                          stride, width, height, sgr_buffer, dst);
  } else {
    BoxFilterProcess(restoration_info, src - 3, top - 3, bottom - 3, stride,
                     width, height, sgr_buffer, dst);
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
#if DSP_ENABLED_8BPP_SSE4_1(WienerFilter)
  dsp->loop_restorations[0] = WienerFilter_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(SelfGuidedFilter)
  dsp->loop_restorations[1] = SelfGuidedFilter_SSE4_1;
#endif
}

}  // namespace
}  // namespace low_bitdepth

void LoopRestorationInit_SSE4_1() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else  // !LIBGAV1_ENABLE_SSE4_1
namespace libgav1 {
namespace dsp {

void LoopRestorationInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
