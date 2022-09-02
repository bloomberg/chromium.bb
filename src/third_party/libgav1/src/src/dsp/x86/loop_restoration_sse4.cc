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

#if LIBGAV1_TARGETING_SSE4_1
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

inline void WienerHorizontalClip(const __m128i s[2], const __m128i s_3x128,
                                 int16_t* const wiener_buffer) {
  constexpr int offset =
      1 << (8 + kWienerFilterBits - kInterRoundBitsHorizontal - 1);
  constexpr int limit =
      (1 << (8 + 1 + kWienerFilterBits - kInterRoundBitsHorizontal)) - 1;
  const __m128i offsets = _mm_set1_epi16(-offset);
  const __m128i limits = _mm_set1_epi16(limit - offset);
  // The sum range here is [-128 * 255 + 4, 90 * 255 + 4].
  const __m128i sum = _mm_add_epi16(s[0], s[1]);
  const __m128i rounded_sum0 = _mm_srai_epi16(sum, kInterRoundBitsHorizontal);
  // Add back scaled down offset correction.
  const __m128i rounded_sum1 = _mm_add_epi16(rounded_sum0, s_3x128);
  const __m128i d0 = _mm_max_epi16(rounded_sum1, offsets);
  const __m128i d1 = _mm_min_epi16(d0, limits);
  StoreAligned16(wiener_buffer, d1);
}

inline void WienerHorizontalTap7Kernel(const __m128i s[4],
                                       const __m128i filter[4],
                                       int16_t* const wiener_buffer) {
  __m128i madds[4];
  madds[0] = _mm_maddubs_epi16(s[0], filter[0]);
  madds[1] = _mm_maddubs_epi16(s[1], filter[1]);
  madds[2] = _mm_maddubs_epi16(s[2], filter[2]);
  madds[3] = _mm_maddubs_epi16(s[3], filter[3]);
  madds[0] = _mm_add_epi16(madds[0], madds[2]);
  madds[1] = _mm_add_epi16(madds[1], madds[3]);
  const __m128i s_3x128 =
      _mm_slli_epi16(_mm_srli_epi16(s[1], 8), 7 - kInterRoundBitsHorizontal);
  WienerHorizontalClip(madds, s_3x128, wiener_buffer);
}

inline void WienerHorizontalTap5Kernel(const __m128i s[5],
                                       const __m128i filter[3],
                                       int16_t* const wiener_buffer) {
  __m128i madds[3];
  madds[0] = _mm_maddubs_epi16(s[0], filter[0]);
  madds[1] = _mm_maddubs_epi16(s[1], filter[1]);
  madds[2] = _mm_maddubs_epi16(s[2], filter[2]);
  madds[0] = _mm_add_epi16(madds[0], madds[2]);
  const __m128i s_3x128 =
      _mm_srli_epi16(_mm_slli_epi16(s[1], 8), kInterRoundBitsHorizontal + 1);
  WienerHorizontalClip(madds, s_3x128, wiener_buffer);
}

inline void WienerHorizontalTap3Kernel(const __m128i s[2],
                                       const __m128i filter[2],
                                       int16_t* const wiener_buffer) {
  __m128i madds[2];
  madds[0] = _mm_maddubs_epi16(s[0], filter[0]);
  madds[1] = _mm_maddubs_epi16(s[1], filter[1]);
  const __m128i s_3x128 =
      _mm_slli_epi16(_mm_srli_epi16(s[0], 8), 7 - kInterRoundBitsHorizontal);
  WienerHorizontalClip(madds, s_3x128, wiener_buffer);
}

// loading all and unpacking is about 7% faster than using _mm_alignr_epi8().
inline void WienerHorizontalTap7(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 const int coefficient0,
                                 const __m128i coefficients,
                                 int16_t** const wiener_buffer) {
  const __m128i round = _mm_set1_epi8(1 << (kInterRoundBitsHorizontal - 1));
  __m128i filter[4];
  filter[0] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0200));
  filter[1] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0604));
  filter[2] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0204));
  filter[3] = _mm_set1_epi16((1 << 8) | static_cast<uint8_t>(coefficient0));
  for (int y = height; y != 0; --y) {
    ptrdiff_t x = 0;
    do {
      __m128i s[7], ss[4];
      s[0] = LoadUnaligned16(src + x + 0);
      s[1] = LoadUnaligned16(src + x + 1);
      s[2] = LoadUnaligned16(src + x + 2);
      s[3] = LoadUnaligned16(src + x + 3);
      s[4] = LoadUnaligned16(src + x + 4);
      s[5] = LoadUnaligned16(src + x + 5);
      s[6] = LoadUnaligned16(src + x + 6);
      ss[0] = _mm_unpacklo_epi8(s[0], s[1]);
      ss[1] = _mm_unpacklo_epi8(s[2], s[3]);
      ss[2] = _mm_unpacklo_epi8(s[4], s[5]);
      ss[3] = _mm_unpacklo_epi8(s[6], round);
      WienerHorizontalTap7Kernel(ss, filter, *wiener_buffer + x + 0);
      ss[0] = _mm_unpackhi_epi8(s[0], s[1]);
      ss[1] = _mm_unpackhi_epi8(s[2], s[3]);
      ss[2] = _mm_unpackhi_epi8(s[4], s[5]);
      ss[3] = _mm_unpackhi_epi8(s[6], round);
      WienerHorizontalTap7Kernel(ss, filter, *wiener_buffer + x + 8);
      x += 16;
    } while (x < width);
    src += src_stride;
    *wiener_buffer += width;
  }
}

inline void WienerHorizontalTap5(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 const int coefficient1,
                                 const __m128i coefficients,
                                 int16_t** const wiener_buffer) {
  const __m128i round = _mm_set1_epi8(1 << (kInterRoundBitsHorizontal - 1));
  __m128i filter[3];
  filter[0] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0402));
  filter[1] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0406));
  filter[2] = _mm_set1_epi16((1 << 8) | static_cast<uint8_t>(coefficient1));
  for (int y = height; y != 0; --y) {
    ptrdiff_t x = 0;
    do {
      __m128i s[5], ss[3];
      s[0] = LoadUnaligned16(src + x + 0);
      s[1] = LoadUnaligned16(src + x + 1);
      s[2] = LoadUnaligned16(src + x + 2);
      s[3] = LoadUnaligned16(src + x + 3);
      s[4] = LoadUnaligned16(src + x + 4);
      ss[0] = _mm_unpacklo_epi8(s[0], s[1]);
      ss[1] = _mm_unpacklo_epi8(s[2], s[3]);
      ss[2] = _mm_unpacklo_epi8(s[4], round);
      WienerHorizontalTap5Kernel(ss, filter, *wiener_buffer + x + 0);
      ss[0] = _mm_unpackhi_epi8(s[0], s[1]);
      ss[1] = _mm_unpackhi_epi8(s[2], s[3]);
      ss[2] = _mm_unpackhi_epi8(s[4], round);
      WienerHorizontalTap5Kernel(ss, filter, *wiener_buffer + x + 8);
      x += 16;
    } while (x < width);
    src += src_stride;
    *wiener_buffer += width;
  }
}

inline void WienerHorizontalTap3(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 const int coefficient2,
                                 const __m128i coefficients,
                                 int16_t** const wiener_buffer) {
  const __m128i round = _mm_set1_epi8(1 << (kInterRoundBitsHorizontal - 1));
  __m128i filter[2];
  filter[0] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0604));
  filter[1] = _mm_set1_epi16((1 << 8) | static_cast<uint8_t>(coefficient2));
  for (int y = height; y != 0; --y) {
    ptrdiff_t x = 0;
    do {
      __m128i s[3], ss[2];
      s[0] = LoadUnaligned16(src + x + 0);
      s[1] = LoadUnaligned16(src + x + 1);
      s[2] = LoadUnaligned16(src + x + 2);
      ss[0] = _mm_unpacklo_epi8(s[0], s[1]);
      ss[1] = _mm_unpacklo_epi8(s[2], round);
      WienerHorizontalTap3Kernel(ss, filter, *wiener_buffer + x + 0);
      ss[0] = _mm_unpackhi_epi8(s[0], s[1]);
      ss[1] = _mm_unpackhi_epi8(s[2], round);
      WienerHorizontalTap3Kernel(ss, filter, *wiener_buffer + x + 8);
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

void WienerFilter_SSE4_1(
    const RestorationUnitInfo& LIBGAV1_RESTRICT restoration_info,
    const void* LIBGAV1_RESTRICT const source, const ptrdiff_t stride,
    const void* LIBGAV1_RESTRICT const top_border,
    const ptrdiff_t top_border_stride,
    const void* LIBGAV1_RESTRICT const bottom_border,
    const ptrdiff_t bottom_border_stride, const int width, const int height,
    RestorationBuffer* LIBGAV1_RESTRICT const restoration_buffer,
    void* LIBGAV1_RESTRICT const dest) {
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
  const int16_t* const filter_horizontal =
      restoration_info.wiener_info.filter[WienerInfo::kHorizontal];
  const __m128i c = LoadLo8(filter_horizontal);
  // In order to keep the horizontal pass intermediate values within 16 bits we
  // offset |filter[3]| by 128. The 128 offset will be added back in the loop.
  const __m128i coefficients_horizontal =
      _mm_sub_epi16(c, _mm_setr_epi16(0, 0, 0, 128, 0, 0, 0, 0));
  if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 0) {
    WienerHorizontalTap7(top + (2 - height_extra) * top_border_stride - 3,
                         top_border_stride, wiener_stride, height_extra,
                         filter_horizontal[0], coefficients_horizontal,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap7(src - 3, stride, wiener_stride, height,
                         filter_horizontal[0], coefficients_horizontal,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap7(bottom - 3, bottom_border_stride, wiener_stride,
                         height_extra, filter_horizontal[0],
                         coefficients_horizontal, &wiener_buffer_horizontal);
  } else if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 1) {
    WienerHorizontalTap5(top + (2 - height_extra) * top_border_stride - 2,
                         top_border_stride, wiener_stride, height_extra,
                         filter_horizontal[1], coefficients_horizontal,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap5(src - 2, stride, wiener_stride, height,
                         filter_horizontal[1], coefficients_horizontal,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap5(bottom - 2, bottom_border_stride, wiener_stride,
                         height_extra, filter_horizontal[1],
                         coefficients_horizontal, &wiener_buffer_horizontal);
  } else if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 2) {
    // The maximum over-reads happen here.
    WienerHorizontalTap3(top + (2 - height_extra) * top_border_stride - 1,
                         top_border_stride, wiener_stride, height_extra,
                         filter_horizontal[2], coefficients_horizontal,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap3(src - 1, stride, wiener_stride, height,
                         filter_horizontal[2], coefficients_horizontal,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap3(bottom - 1, bottom_border_stride, wiener_stride,
                         height_extra, filter_horizontal[2],
                         coefficients_horizontal, &wiener_buffer_horizontal);
  } else {
    assert(number_leading_zero_coefficients[WienerInfo::kHorizontal] == 3);
    WienerHorizontalTap1(top + (2 - height_extra) * top_border_stride,
                         top_border_stride, wiener_stride, height_extra,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap1(src, stride, wiener_stride, height,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap1(bottom, bottom_border_stride, wiener_stride,
                         height_extra, &wiener_buffer_horizontal);
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

// SIMD overreads 16 - (width % 16) - 2 * padding pixels, where padding is 3 for
// Pass 1 and 2 for Pass 2.
constexpr int kOverreadInBytesPass1 = 10;
constexpr int kOverreadInBytesPass2 = 12;

inline void LoadAligned16x2U16(const uint16_t* const src[2], const ptrdiff_t x,
                               __m128i dst[2]) {
  dst[0] = LoadAligned16(src[0] + x);
  dst[1] = LoadAligned16(src[1] + x);
}

inline void LoadAligned16x2U16Msan(const uint16_t* const src[2],
                                   const ptrdiff_t x, const ptrdiff_t border,
                                   __m128i dst[2]) {
  dst[0] = LoadAligned16Msan(src[0] + x, sizeof(**src) * (x + 8 - border));
  dst[1] = LoadAligned16Msan(src[1] + x, sizeof(**src) * (x + 8 - border));
}

inline void LoadAligned16x3U16(const uint16_t* const src[3], const ptrdiff_t x,
                               __m128i dst[3]) {
  dst[0] = LoadAligned16(src[0] + x);
  dst[1] = LoadAligned16(src[1] + x);
  dst[2] = LoadAligned16(src[2] + x);
}

inline void LoadAligned16x3U16Msan(const uint16_t* const src[3],
                                   const ptrdiff_t x, const ptrdiff_t border,
                                   __m128i dst[3]) {
  dst[0] = LoadAligned16Msan(src[0] + x, sizeof(**src) * (x + 8 - border));
  dst[1] = LoadAligned16Msan(src[1] + x, sizeof(**src) * (x + 8 - border));
  dst[2] = LoadAligned16Msan(src[2] + x, sizeof(**src) * (x + 8 - border));
}

inline void LoadAligned32U32(const uint32_t* const src, __m128i dst[2]) {
  dst[0] = LoadAligned16(src + 0);
  dst[1] = LoadAligned16(src + 4);
}

inline void LoadAligned32U32Msan(const uint32_t* const src, const ptrdiff_t x,
                                 const ptrdiff_t border, __m128i dst[2]) {
  dst[0] = LoadAligned16Msan(src + x + 0, sizeof(*src) * (x + 4 - border));
  dst[1] = LoadAligned16Msan(src + x + 4, sizeof(*src) * (x + 8 - border));
}

inline void LoadAligned32x2U32(const uint32_t* const src[2], const ptrdiff_t x,
                               __m128i dst[2][2]) {
  LoadAligned32U32(src[0] + x, dst[0]);
  LoadAligned32U32(src[1] + x, dst[1]);
}

inline void LoadAligned32x2U32Msan(const uint32_t* const src[2],
                                   const ptrdiff_t x, const ptrdiff_t border,
                                   __m128i dst[2][2]) {
  LoadAligned32U32Msan(src[0], x, border, dst[0]);
  LoadAligned32U32Msan(src[1], x, border, dst[1]);
}

inline void LoadAligned32x3U32(const uint32_t* const src[3], const ptrdiff_t x,
                               __m128i dst[3][2]) {
  LoadAligned32U32(src[0] + x, dst[0]);
  LoadAligned32U32(src[1] + x, dst[1]);
  LoadAligned32U32(src[2] + x, dst[2]);
}

inline void LoadAligned32x3U32Msan(const uint32_t* const src[3],
                                   const ptrdiff_t x, const ptrdiff_t border,
                                   __m128i dst[3][2]) {
  LoadAligned32U32Msan(src[0], x, border, dst[0]);
  LoadAligned32U32Msan(src[1], x, border, dst[1]);
  LoadAligned32U32Msan(src[2], x, border, dst[2]);
}

inline void StoreAligned32U16(uint16_t* const dst, const __m128i src[2]) {
  StoreAligned16(dst + 0, src[0]);
  StoreAligned16(dst + 8, src[1]);
}

inline void StoreAligned32U32(uint32_t* const dst, const __m128i src[2]) {
  StoreAligned16(dst + 0, src[0]);
  StoreAligned16(dst + 4, src[1]);
}

inline void StoreAligned64U32(uint32_t* const dst, const __m128i src[4]) {
  StoreAligned32U32(dst + 0, src + 0);
  StoreAligned32U32(dst + 8, src + 2);
}

// Don't use _mm_cvtepu8_epi16() or _mm_cvtepu16_epi32() in the following
// functions. Some compilers may generate super inefficient code and the whole
// decoder could be 15% slower.

inline __m128i VaddlLo8(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpacklo_epi8(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpacklo_epi8(src1, _mm_setzero_si128());
  return _mm_add_epi16(s0, s1);
}

inline __m128i VaddlHi8(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpackhi_epi8(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpackhi_epi8(src1, _mm_setzero_si128());
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

inline __m128i VaddwHi8(const __m128i src0, const __m128i src1) {
  const __m128i s1 = _mm_unpackhi_epi8(src1, _mm_setzero_si128());
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

inline __m128i SquareLo8(const __m128i src) {
  const __m128i s = _mm_unpacklo_epi8(src, _mm_setzero_si128());
  return _mm_mullo_epi16(s, s);
}

inline __m128i SquareHi8(const __m128i src) {
  const __m128i s = _mm_unpackhi_epi8(src, _mm_setzero_si128());
  return _mm_mullo_epi16(s, s);
}

inline void Prepare3Lo8(const __m128i src, __m128i dst[3]) {
  dst[0] = src;
  dst[1] = _mm_srli_si128(src, 1);
  dst[2] = _mm_srli_si128(src, 2);
}

template <int offset>
inline void Prepare3_8(const __m128i src[2], __m128i dst[3]) {
  dst[0] = _mm_alignr_epi8(src[1], src[0], offset + 0);
  dst[1] = _mm_alignr_epi8(src[1], src[0], offset + 1);
  dst[2] = _mm_alignr_epi8(src[1], src[0], offset + 2);
}

inline void Prepare3_16(const __m128i src[2], __m128i dst[3]) {
  dst[0] = src[0];
  dst[1] = _mm_alignr_epi8(src[1], src[0], 2);
  dst[2] = _mm_alignr_epi8(src[1], src[0], 4);
}

inline void Prepare5Lo8(const __m128i src, __m128i dst[5]) {
  dst[0] = src;
  dst[1] = _mm_srli_si128(src, 1);
  dst[2] = _mm_srli_si128(src, 2);
  dst[3] = _mm_srli_si128(src, 3);
  dst[4] = _mm_srli_si128(src, 4);
}

template <int offset>
inline void Prepare5_8(const __m128i src[2], __m128i dst[5]) {
  dst[0] = _mm_alignr_epi8(src[1], src[0], offset + 0);
  dst[1] = _mm_alignr_epi8(src[1], src[0], offset + 1);
  dst[2] = _mm_alignr_epi8(src[1], src[0], offset + 2);
  dst[3] = _mm_alignr_epi8(src[1], src[0], offset + 3);
  dst[4] = _mm_alignr_epi8(src[1], src[0], offset + 4);
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

inline __m128i Sum3WLo16(const __m128i src[3]) {
  const __m128i sum = VaddlLo8(src[0], src[1]);
  return VaddwLo8(sum, src[2]);
}

inline __m128i Sum3WHi16(const __m128i src[3]) {
  const __m128i sum = VaddlHi8(src[0], src[1]);
  return VaddwHi8(sum, src[2]);
}

inline __m128i Sum3WLo32(const __m128i src[3]) {
  const __m128i sum = VaddlLo16(src[0], src[1]);
  return VaddwLo16(sum, src[2]);
}

inline __m128i Sum3WHi32(const __m128i src[3]) {
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

inline __m128i Sum5WLo16(const __m128i src[5]) {
  const __m128i sum01 = VaddlLo8(src[0], src[1]);
  const __m128i sum23 = VaddlLo8(src[2], src[3]);
  const __m128i sum = _mm_add_epi16(sum01, sum23);
  return VaddwLo8(sum, src[4]);
}

inline __m128i Sum5WHi16(const __m128i src[5]) {
  const __m128i sum01 = VaddlHi8(src[0], src[1]);
  const __m128i sum23 = VaddlHi8(src[2], src[3]);
  const __m128i sum = _mm_add_epi16(sum01, sum23);
  return VaddwHi8(sum, src[4]);
}

inline __m128i Sum3Horizontal(const __m128i src) {
  __m128i s[3];
  Prepare3Lo8(src, s);
  return Sum3WLo16(s);
}

template <int offset>
inline void Sum3Horizontal(const __m128i src[2], __m128i dst[2]) {
  __m128i s[3];
  Prepare3_8<offset>(src, s);
  dst[0] = Sum3WLo16(s);
  dst[1] = Sum3WHi16(s);
}

inline void Sum3WHorizontal(const __m128i src[2], __m128i dst[2]) {
  __m128i s[3];
  Prepare3_16(src, s);
  dst[0] = Sum3WLo32(s);
  dst[1] = Sum3WHi32(s);
}

inline __m128i Sum5Horizontal(const __m128i src) {
  __m128i s[5];
  Prepare5Lo8(src, s);
  return Sum5WLo16(s);
}

template <int offset>
inline void Sum5Horizontal(const __m128i src[2], __m128i* const dst0,
                           __m128i* const dst1) {
  __m128i s[5];
  Prepare5_8<offset>(src, s);
  *dst0 = Sum5WLo16(s);
  *dst1 = Sum5WHi16(s);
}

inline void Sum5WHorizontal(const __m128i src[2], __m128i dst[2]) {
  __m128i s[5];
  Prepare5_16(src, s);
  const __m128i sum01_lo = VaddlLo16(s[0], s[1]);
  const __m128i sum23_lo = VaddlLo16(s[2], s[3]);
  const __m128i sum0123_lo = _mm_add_epi32(sum01_lo, sum23_lo);
  dst[0] = VaddwLo16(sum0123_lo, s[4]);
  const __m128i sum01_hi = VaddlHi16(s[0], s[1]);
  const __m128i sum23_hi = VaddlHi16(s[2], s[3]);
  const __m128i sum0123_hi = _mm_add_epi32(sum01_hi, sum23_hi);
  dst[1] = VaddwHi16(sum0123_hi, s[4]);
}

void SumHorizontalLo(const __m128i src[5], __m128i* const row_sq3,
                     __m128i* const row_sq5) {
  const __m128i sum04 = VaddlLo16(src[0], src[4]);
  *row_sq3 = Sum3WLo32(src + 1);
  *row_sq5 = _mm_add_epi32(sum04, *row_sq3);
}

void SumHorizontalHi(const __m128i src[5], __m128i* const row_sq3,
                     __m128i* const row_sq5) {
  const __m128i sum04 = VaddlHi16(src[0], src[4]);
  *row_sq3 = Sum3WHi32(src + 1);
  *row_sq5 = _mm_add_epi32(sum04, *row_sq3);
}

void SumHorizontalLo(const __m128i src, __m128i* const row3,
                     __m128i* const row5) {
  __m128i s[5];
  Prepare5Lo8(src, s);
  const __m128i sum04 = VaddlLo8(s[0], s[4]);
  *row3 = Sum3WLo16(s + 1);
  *row5 = _mm_add_epi16(sum04, *row3);
}

template <int offset>
void SumHorizontal(const __m128i src[2], __m128i* const row3_0,
                   __m128i* const row3_1, __m128i* const row5_0,
                   __m128i* const row5_1) {
  __m128i s[5];
  Prepare5_8<offset>(src, s);
  const __m128i sum04_lo = VaddlLo8(s[0], s[4]);
  const __m128i sum04_hi = VaddlHi8(s[0], s[4]);
  *row3_0 = Sum3WLo16(s + 1);
  *row3_1 = Sum3WHi16(s + 1);
  *row5_0 = _mm_add_epi16(sum04_lo, *row3_0);
  *row5_1 = _mm_add_epi16(sum04_hi, *row3_1);
}

inline void SumHorizontal(const __m128i src[2], __m128i* const row_sq3_0,
                          __m128i* const row_sq3_1, __m128i* const row_sq5_0,
                          __m128i* const row_sq5_1) {
  __m128i s[5];
  Prepare5_16(src, s);
  SumHorizontalLo(s, row_sq3_0, row_sq5_0);
  SumHorizontalHi(s, row_sq3_1, row_sq5_1);
}

inline __m128i Sum343Lo(const __m128i ma3[3]) {
  const __m128i sum = Sum3WLo16(ma3);
  const __m128i sum3 = Sum3_16(sum, sum, sum);
  return VaddwLo8(sum3, ma3[1]);
}

inline __m128i Sum343Hi(const __m128i ma3[3]) {
  const __m128i sum = Sum3WHi16(ma3);
  const __m128i sum3 = Sum3_16(sum, sum, sum);
  return VaddwHi8(sum3, ma3[1]);
}

inline __m128i Sum343WLo(const __m128i src[3]) {
  const __m128i sum = Sum3WLo32(src);
  const __m128i sum3 = Sum3_32(sum, sum, sum);
  return VaddwLo16(sum3, src[1]);
}

inline __m128i Sum343WHi(const __m128i src[3]) {
  const __m128i sum = Sum3WHi32(src);
  const __m128i sum3 = Sum3_32(sum, sum, sum);
  return VaddwHi16(sum3, src[1]);
}

inline void Sum343W(const __m128i src[2], __m128i dst[2]) {
  __m128i s[3];
  Prepare3_16(src, s);
  dst[0] = Sum343WLo(s);
  dst[1] = Sum343WHi(s);
}

inline __m128i Sum565Lo(const __m128i src[3]) {
  const __m128i sum = Sum3WLo16(src);
  const __m128i sum4 = _mm_slli_epi16(sum, 2);
  const __m128i sum5 = _mm_add_epi16(sum4, sum);
  return VaddwLo8(sum5, src[1]);
}

inline __m128i Sum565Hi(const __m128i src[3]) {
  const __m128i sum = Sum3WHi16(src);
  const __m128i sum4 = _mm_slli_epi16(sum, 2);
  const __m128i sum5 = _mm_add_epi16(sum4, sum);
  return VaddwHi8(sum5, src[1]);
}

inline __m128i Sum565WLo(const __m128i src[3]) {
  const __m128i sum = Sum3WLo32(src);
  const __m128i sum4 = _mm_slli_epi32(sum, 2);
  const __m128i sum5 = _mm_add_epi32(sum4, sum);
  return VaddwLo16(sum5, src[1]);
}

inline __m128i Sum565WHi(const __m128i src[3]) {
  const __m128i sum = Sum3WHi32(src);
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
                   const ptrdiff_t width, const ptrdiff_t sum_stride,
                   const ptrdiff_t sum_width, uint16_t* sum3, uint16_t* sum5,
                   uint32_t* square_sum3, uint32_t* square_sum5) {
  int y = 2;
  do {
    __m128i s[2], sq[3];
    s[0] = LoadUnaligned16Msan(src, kOverreadInBytesPass1 - width);
    sq[0] = SquareLo8(s[0]);
    ptrdiff_t x = sum_width;
    do {
      __m128i row3[2], row5[2], row_sq3[2], row_sq5[2];
      x -= 16;
      src += 16;
      s[1] = LoadUnaligned16Msan(src,
                                 sum_width - x + kOverreadInBytesPass1 - width);
      sq[1] = SquareHi8(s[0]);
      sq[2] = SquareLo8(s[1]);
      SumHorizontal<0>(s, &row3[0], &row3[1], &row5[0], &row5[1]);
      StoreAligned32U16(sum3, row3);
      StoreAligned32U16(sum5, row5);
      SumHorizontal(sq + 0, &row_sq3[0], &row_sq3[1], &row_sq5[0], &row_sq5[1]);
      StoreAligned32U32(square_sum3 + 0, row_sq3);
      StoreAligned32U32(square_sum5 + 0, row_sq5);
      SumHorizontal(sq + 1, &row_sq3[0], &row_sq3[1], &row_sq5[0], &row_sq5[1]);
      StoreAligned32U32(square_sum3 + 8, row_sq3);
      StoreAligned32U32(square_sum5 + 8, row_sq5);
      s[0] = s[1];
      sq[0] = sq[2];
      sum3 += 16;
      sum5 += 16;
      square_sum3 += 16;
      square_sum5 += 16;
    } while (x != 0);
    src += src_stride - sum_width;
    sum3 += sum_stride - sum_width;
    sum5 += sum_stride - sum_width;
    square_sum3 += sum_stride - sum_width;
    square_sum5 += sum_stride - sum_width;
  } while (--y != 0);
}

template <int size>
inline void BoxSum(const uint8_t* src, const ptrdiff_t src_stride,
                   const ptrdiff_t width, const ptrdiff_t sum_stride,
                   const ptrdiff_t sum_width, uint16_t* sums,
                   uint32_t* square_sums) {
  static_assert(size == 3 || size == 5, "");
  constexpr int kOverreadInBytes =
      (size == 5) ? kOverreadInBytesPass1 : kOverreadInBytesPass2;
  int y = 2;
  do {
    __m128i s[2], sq[3];
    s[0] = LoadUnaligned16Msan(src, kOverreadInBytes - width);
    sq[0] = SquareLo8(s[0]);
    ptrdiff_t x = sum_width;
    do {
      __m128i row[2], row_sq[4];
      x -= 16;
      src += 16;
      s[1] = LoadUnaligned16Msan(src, sum_width - x + kOverreadInBytes - width);
      sq[1] = SquareHi8(s[0]);
      sq[2] = SquareLo8(s[1]);
      if (size == 3) {
        Sum3Horizontal<0>(s, row);
        Sum3WHorizontal(sq + 0, row_sq + 0);
        Sum3WHorizontal(sq + 1, row_sq + 2);
      } else {
        Sum5Horizontal<0>(s, &row[0], &row[1]);
        Sum5WHorizontal(sq + 0, row_sq + 0);
        Sum5WHorizontal(sq + 1, row_sq + 2);
      }
      StoreAligned32U16(sums, row);
      StoreAligned64U32(square_sums, row_sq);
      s[0] = s[1];
      sq[0] = sq[2];
      sums += 16;
      square_sums += 16;
    } while (x != 0);
    src += src_stride - sum_width;
    sums += sum_stride - sum_width;
    square_sums += sum_stride - sum_width;
  } while (--y != 0);
}

template <int n>
inline __m128i CalculateMa(const __m128i sum, const __m128i sum_sq,
                           const uint32_t scale) {
  static_assert(n == 9 || n == 25, "");
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
inline __m128i CalculateMa(const __m128i sum, const __m128i sum_sq[2],
                           const uint32_t scale) {
  static_assert(n == 9 || n == 25, "");
  const __m128i sum_lo = _mm_unpacklo_epi16(sum, _mm_setzero_si128());
  const __m128i sum_hi = _mm_unpackhi_epi16(sum, _mm_setzero_si128());
  const __m128i z0 = CalculateMa<n>(sum_lo, sum_sq[0], scale);
  const __m128i z1 = CalculateMa<n>(sum_hi, sum_sq[1], scale);
  return _mm_packus_epi32(z0, z1);
}

inline __m128i CalculateB5(const __m128i sum, const __m128i ma) {
  // one_over_n == 164.
  constexpr uint32_t one_over_n =
      ((1 << kSgrProjReciprocalBits) + (25 >> 1)) / 25;
  // one_over_n_quarter == 41.
  constexpr uint32_t one_over_n_quarter = one_over_n >> 2;
  static_assert(one_over_n == one_over_n_quarter << 2, "");
  // |ma| is in range [0, 255].
  const __m128i m = _mm_maddubs_epi16(ma, _mm_set1_epi16(one_over_n_quarter));
  const __m128i m0 = VmullLo16(m, sum);
  const __m128i m1 = VmullHi16(m, sum);
  const __m128i b_lo = VrshrU32(m0, kSgrProjReciprocalBits - 2);
  const __m128i b_hi = VrshrU32(m1, kSgrProjReciprocalBits - 2);
  return _mm_packus_epi32(b_lo, b_hi);
}

inline __m128i CalculateB3(const __m128i sum, const __m128i ma) {
  // one_over_n == 455.
  constexpr uint32_t one_over_n =
      ((1 << kSgrProjReciprocalBits) + (9 >> 1)) / 9;
  const __m128i m0 = VmullLo16(ma, sum);
  const __m128i m1 = VmullHi16(ma, sum);
  const __m128i m2 = _mm_mullo_epi32(m0, _mm_set1_epi32(one_over_n));
  const __m128i m3 = _mm_mullo_epi32(m1, _mm_set1_epi32(one_over_n));
  const __m128i b_lo = VrshrU32(m2, kSgrProjReciprocalBits);
  const __m128i b_hi = VrshrU32(m3, kSgrProjReciprocalBits);
  return _mm_packus_epi32(b_lo, b_hi);
}

inline void CalculateSumAndIndex5(const __m128i s5[5], const __m128i sq5[5][2],
                                  const uint32_t scale, __m128i* const sum,
                                  __m128i* const index) {
  __m128i sum_sq[2];
  *sum = Sum5_16(s5);
  Sum5_32(sq5, sum_sq);
  *index = CalculateMa<25>(*sum, sum_sq, scale);
}

inline void CalculateSumAndIndex3(const __m128i s3[3], const __m128i sq3[3][2],
                                  const uint32_t scale, __m128i* const sum,
                                  __m128i* const index) {
  __m128i sum_sq[2];
  *sum = Sum3_16(s3);
  Sum3_32(sq3, sum_sq);
  *index = CalculateMa<9>(*sum, sum_sq, scale);
}

template <int n, int offset>
inline void LookupIntermediate(const __m128i sum, const __m128i index,
                               __m128i* const ma, __m128i* const b) {
  static_assert(n == 9 || n == 25, "");
  static_assert(offset == 0 || offset == 8, "");
  const __m128i idx = _mm_packus_epi16(index, index);
  // Actually it's not stored and loaded. The compiler will use a 64-bit
  // general-purpose register to process. Faster than using _mm_extract_epi8().
  uint8_t temp[8];
  StoreLo8(temp, idx);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[temp[0]], offset + 0);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[temp[1]], offset + 1);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[temp[2]], offset + 2);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[temp[3]], offset + 3);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[temp[4]], offset + 4);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[temp[5]], offset + 5);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[temp[6]], offset + 6);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[temp[7]], offset + 7);
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
  __m128i maq;
  if (offset == 0) {
    maq = _mm_unpacklo_epi8(*ma, _mm_setzero_si128());
  } else {
    maq = _mm_unpackhi_epi8(*ma, _mm_setzero_si128());
  }
  *b = (n == 9) ? CalculateB3(sum, maq) : CalculateB5(sum, maq);
}

// Set the shuffle control mask of indices out of range [0, 15] to (1xxxxxxx)b
// to get value 0 as the shuffle result. The most significiant bit 1 comes
// either from the comparison instruction, or from the sign bit of the index.
inline __m128i ShuffleIndex(const __m128i table, const __m128i index) {
  __m128i mask;
  mask = _mm_cmpgt_epi8(index, _mm_set1_epi8(15));
  mask = _mm_or_si128(mask, index);
  return _mm_shuffle_epi8(table, mask);
}

inline __m128i AdjustValue(const __m128i value, const __m128i index,
                           const int threshold) {
  const __m128i thresholds = _mm_set1_epi8(threshold - 128);
  const __m128i offset = _mm_cmpgt_epi8(index, thresholds);
  return _mm_add_epi8(value, offset);
}

inline void CalculateIntermediate(const __m128i sum[2], const __m128i index[2],
                                  __m128i* const ma, __m128i* const b0,
                                  __m128i* const b1) {
  // Use table lookup to read elements whose indices are less than 48.
  const __m128i c0 = LoadAligned16(kSgrMaLookup + 0 * 16);
  const __m128i c1 = LoadAligned16(kSgrMaLookup + 1 * 16);
  const __m128i c2 = LoadAligned16(kSgrMaLookup + 2 * 16);
  const __m128i indices = _mm_packus_epi16(index[0], index[1]);
  __m128i idx;
  // Clip idx to 127 to apply signed comparison instructions.
  idx = _mm_min_epu8(indices, _mm_set1_epi8(127));
  // All elements whose indices are less than 48 are set to 0.
  // Get shuffle results for indices in range [0, 15].
  *ma = ShuffleIndex(c0, idx);
  // Get shuffle results for indices in range [16, 31].
  // Subtract 16 to utilize the sign bit of the index.
  idx = _mm_sub_epi8(idx, _mm_set1_epi8(16));
  const __m128i res1 = ShuffleIndex(c1, idx);
  // Use OR instruction to combine shuffle results together.
  *ma = _mm_or_si128(*ma, res1);
  // Get shuffle results for indices in range [32, 47].
  // Subtract 16 to utilize the sign bit of the index.
  idx = _mm_sub_epi8(idx, _mm_set1_epi8(16));
  const __m128i res2 = ShuffleIndex(c2, idx);
  *ma = _mm_or_si128(*ma, res2);

  // For elements whose indices are larger than 47, since they seldom change
  // values with the increase of the index, we use comparison and arithmetic
  // operations to calculate their values.
  // Add -128 to apply signed comparison instructions.
  idx = _mm_add_epi8(indices, _mm_set1_epi8(-128));
  // Elements whose indices are larger than 47 (with value 0) are set to 5.
  *ma = _mm_max_epu8(*ma, _mm_set1_epi8(5));
  *ma = AdjustValue(*ma, idx, 55);   // 55 is the last index which value is 5.
  *ma = AdjustValue(*ma, idx, 72);   // 72 is the last index which value is 4.
  *ma = AdjustValue(*ma, idx, 101);  // 101 is the last index which value is 3.
  *ma = AdjustValue(*ma, idx, 169);  // 169 is the last index which value is 2.
  *ma = AdjustValue(*ma, idx, 254);  // 254 is the last index which value is 1.

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
  const __m128i maq0 = _mm_unpacklo_epi8(*ma, _mm_setzero_si128());
  *b0 = CalculateB3(sum[0], maq0);
  const __m128i maq1 = _mm_unpackhi_epi8(*ma, _mm_setzero_si128());
  *b1 = CalculateB3(sum[1], maq1);
}

inline void CalculateIntermediate(const __m128i sum[2], const __m128i index[2],
                                  __m128i ma[2], __m128i b[2]) {
  __m128i mas;
  CalculateIntermediate(sum, index, &mas, &b[0], &b[1]);
  ma[0] = _mm_unpacklo_epi64(ma[0], mas);
  ma[1] = _mm_srli_si128(mas, 8);
}

// Note: It has been tried to call CalculateIntermediate() to replace the slow
// LookupIntermediate() when calculating 16 intermediate data points. However,
// the compiler generates even slower code.
template <int offset>
inline void CalculateIntermediate5(const __m128i s5[5], const __m128i sq5[5][2],
                                   const uint32_t scale, __m128i* const ma,
                                   __m128i* const b) {
  static_assert(offset == 0 || offset == 8, "");
  __m128i sum, index;
  CalculateSumAndIndex5(s5, sq5, scale, &sum, &index);
  LookupIntermediate<25, offset>(sum, index, ma, b);
}

inline void CalculateIntermediate3(const __m128i s3[3], const __m128i sq3[3][2],
                                   const uint32_t scale, __m128i* const ma,
                                   __m128i* const b) {
  __m128i sum, index;
  CalculateSumAndIndex3(s3, sq3, scale, &sum, &index);
  LookupIntermediate<9, 0>(sum, index, ma, b);
}

inline void Store343_444(const __m128i b3[2], const ptrdiff_t x,
                         __m128i sum_b343[2], __m128i sum_b444[2],
                         uint32_t* const b343, uint32_t* const b444) {
  __m128i b[3], sum_b111[2];
  Prepare3_16(b3, b);
  sum_b111[0] = Sum3WLo32(b);
  sum_b111[1] = Sum3WHi32(b);
  sum_b444[0] = _mm_slli_epi32(sum_b111[0], 2);
  sum_b444[1] = _mm_slli_epi32(sum_b111[1], 2);
  StoreAligned32U32(b444 + x, sum_b444);
  sum_b343[0] = _mm_sub_epi32(sum_b444[0], sum_b111[0]);
  sum_b343[1] = _mm_sub_epi32(sum_b444[1], sum_b111[1]);
  sum_b343[0] = VaddwLo16(sum_b343[0], b[1]);
  sum_b343[1] = VaddwHi16(sum_b343[1], b[1]);
  StoreAligned32U32(b343 + x, sum_b343);
}

inline void Store343_444Lo(const __m128i ma3[3], const __m128i b3[2],
                           const ptrdiff_t x, __m128i* const sum_ma343,
                           __m128i* const sum_ma444, __m128i sum_b343[2],
                           __m128i sum_b444[2], uint16_t* const ma343,
                           uint16_t* const ma444, uint32_t* const b343,
                           uint32_t* const b444) {
  const __m128i sum_ma111 = Sum3WLo16(ma3);
  *sum_ma444 = _mm_slli_epi16(sum_ma111, 2);
  StoreAligned16(ma444 + x, *sum_ma444);
  const __m128i sum333 = _mm_sub_epi16(*sum_ma444, sum_ma111);
  *sum_ma343 = VaddwLo8(sum333, ma3[1]);
  StoreAligned16(ma343 + x, *sum_ma343);
  Store343_444(b3, x, sum_b343, sum_b444, b343, b444);
}

inline void Store343_444Hi(const __m128i ma3[3], const __m128i b3[2],
                           const ptrdiff_t x, __m128i* const sum_ma343,
                           __m128i* const sum_ma444, __m128i sum_b343[2],
                           __m128i sum_b444[2], uint16_t* const ma343,
                           uint16_t* const ma444, uint32_t* const b343,
                           uint32_t* const b444) {
  const __m128i sum_ma111 = Sum3WHi16(ma3);
  *sum_ma444 = _mm_slli_epi16(sum_ma111, 2);
  StoreAligned16(ma444 + x, *sum_ma444);
  const __m128i sum333 = _mm_sub_epi16(*sum_ma444, sum_ma111);
  *sum_ma343 = VaddwHi8(sum333, ma3[1]);
  StoreAligned16(ma343 + x, *sum_ma343);
  Store343_444(b3, x, sum_b343, sum_b444, b343, b444);
}

inline void Store343_444Lo(const __m128i ma3[3], const __m128i b3[2],
                           const ptrdiff_t x, __m128i* const sum_ma343,
                           __m128i sum_b343[2], uint16_t* const ma343,
                           uint16_t* const ma444, uint32_t* const b343,
                           uint32_t* const b444) {
  __m128i sum_ma444, sum_b444[2];
  Store343_444Lo(ma3, b3, x, sum_ma343, &sum_ma444, sum_b343, sum_b444, ma343,
                 ma444, b343, b444);
}

inline void Store343_444Hi(const __m128i ma3[3], const __m128i b3[2],
                           const ptrdiff_t x, __m128i* const sum_ma343,
                           __m128i sum_b343[2], uint16_t* const ma343,
                           uint16_t* const ma444, uint32_t* const b343,
                           uint32_t* const b444) {
  __m128i sum_ma444, sum_b444[2];
  Store343_444Hi(ma3, b3, x, sum_ma343, &sum_ma444, sum_b343, sum_b444, ma343,
                 ma444, b343, b444);
}

inline void Store343_444Lo(const __m128i ma3[3], const __m128i b3[2],
                           const ptrdiff_t x, uint16_t* const ma343,
                           uint16_t* const ma444, uint32_t* const b343,
                           uint32_t* const b444) {
  __m128i sum_ma343, sum_b343[2];
  Store343_444Lo(ma3, b3, x, &sum_ma343, sum_b343, ma343, ma444, b343, b444);
}

inline void Store343_444Hi(const __m128i ma3[3], const __m128i b3[2],
                           const ptrdiff_t x, uint16_t* const ma343,
                           uint16_t* const ma444, uint32_t* const b343,
                           uint32_t* const b444) {
  __m128i sum_ma343, sum_b343[2];
  Store343_444Hi(ma3, b3, x, &sum_ma343, sum_b343, ma343, ma444, b343, b444);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess5Lo(
    const __m128i s[2][2], const uint32_t scale, uint16_t* const sum5[5],
    uint32_t* const square_sum5[5], __m128i sq[2][4], __m128i* const ma,
    __m128i* const b) {
  __m128i s5[2][5], sq5[5][2];
  sq[0][1] = SquareHi8(s[0][0]);
  sq[1][1] = SquareHi8(s[1][0]);
  s5[0][3] = Sum5Horizontal(s[0][0]);
  StoreAligned16(sum5[3], s5[0][3]);
  s5[0][4] = Sum5Horizontal(s[1][0]);
  StoreAligned16(sum5[4], s5[0][4]);
  Sum5WHorizontal(sq[0], sq5[3]);
  StoreAligned32U32(square_sum5[3], sq5[3]);
  Sum5WHorizontal(sq[1], sq5[4]);
  StoreAligned32U32(square_sum5[4], sq5[4]);
  LoadAligned16x3U16(sum5, 0, s5[0]);
  LoadAligned32x3U32(square_sum5, 0, sq5);
  CalculateIntermediate5<0>(s5[0], sq5, scale, ma, b);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess5(
    const __m128i s[2][2], const ptrdiff_t sum_width, const ptrdiff_t x,
    const uint32_t scale, uint16_t* const sum5[5],
    uint32_t* const square_sum5[5], __m128i sq[2][4], __m128i ma[2],
    __m128i b[3]) {
  __m128i s5[2][5], sq5[5][2];
  sq[0][2] = SquareLo8(s[0][1]);
  sq[1][2] = SquareLo8(s[1][1]);
  Sum5Horizontal<8>(s[0], &s5[0][3], &s5[1][3]);
  StoreAligned16(sum5[3] + x + 0, s5[0][3]);
  StoreAligned16(sum5[3] + x + 8, s5[1][3]);
  Sum5Horizontal<8>(s[1], &s5[0][4], &s5[1][4]);
  StoreAligned16(sum5[4] + x + 0, s5[0][4]);
  StoreAligned16(sum5[4] + x + 8, s5[1][4]);
  Sum5WHorizontal(sq[0] + 1, sq5[3]);
  StoreAligned32U32(square_sum5[3] + x, sq5[3]);
  Sum5WHorizontal(sq[1] + 1, sq5[4]);
  StoreAligned32U32(square_sum5[4] + x, sq5[4]);
  LoadAligned16x3U16(sum5, x, s5[0]);
  LoadAligned32x3U32(square_sum5, x, sq5);
  CalculateIntermediate5<8>(s5[0], sq5, scale, &ma[0], &b[1]);

  sq[0][3] = SquareHi8(s[0][1]);
  sq[1][3] = SquareHi8(s[1][1]);
  Sum5WHorizontal(sq[0] + 2, sq5[3]);
  StoreAligned32U32(square_sum5[3] + x + 8, sq5[3]);
  Sum5WHorizontal(sq[1] + 2, sq5[4]);
  StoreAligned32U32(square_sum5[4] + x + 8, sq5[4]);
  LoadAligned16x3U16Msan(sum5, x + 8, sum_width, s5[1]);
  LoadAligned32x3U32Msan(square_sum5, x + 8, sum_width, sq5);
  CalculateIntermediate5<0>(s5[1], sq5, scale, &ma[1], &b[2]);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess5LastRowLo(
    const __m128i s, const uint32_t scale, const uint16_t* const sum5[5],
    const uint32_t* const square_sum5[5], __m128i sq[2], __m128i* const ma,
    __m128i* const b) {
  __m128i s5[5], sq5[5][2];
  sq[1] = SquareHi8(s);
  s5[3] = s5[4] = Sum5Horizontal(s);
  Sum5WHorizontal(sq, sq5[3]);
  sq5[4][0] = sq5[3][0];
  sq5[4][1] = sq5[3][1];
  LoadAligned16x3U16(sum5, 0, s5);
  LoadAligned32x3U32(square_sum5, 0, sq5);
  CalculateIntermediate5<0>(s5, sq5, scale, ma, b);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess5LastRow(
    const __m128i s[2], const ptrdiff_t sum_width, const ptrdiff_t x,
    const uint32_t scale, const uint16_t* const sum5[5],
    const uint32_t* const square_sum5[5], __m128i sq[4], __m128i ma[2],
    __m128i b[3]) {
  __m128i s5[2][5], sq5[5][2];
  sq[2] = SquareLo8(s[1]);
  Sum5Horizontal<8>(s, &s5[0][3], &s5[1][3]);
  s5[0][4] = s5[0][3];
  s5[1][4] = s5[1][3];
  Sum5WHorizontal(sq + 1, sq5[3]);
  sq5[4][0] = sq5[3][0];
  sq5[4][1] = sq5[3][1];
  LoadAligned16x3U16(sum5, x, s5[0]);
  LoadAligned32x3U32(square_sum5, x, sq5);
  CalculateIntermediate5<8>(s5[0], sq5, scale, &ma[0], &b[1]);

  sq[3] = SquareHi8(s[1]);
  Sum5WHorizontal(sq + 2, sq5[3]);
  sq5[4][0] = sq5[3][0];
  sq5[4][1] = sq5[3][1];
  LoadAligned16x3U16Msan(sum5, x + 8, sum_width, s5[1]);
  LoadAligned32x3U32Msan(square_sum5, x + 8, sum_width, sq5);
  CalculateIntermediate5<0>(s5[1], sq5, scale, &ma[1], &b[2]);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess3Lo(
    const __m128i s, const uint32_t scale, uint16_t* const sum3[3],
    uint32_t* const square_sum3[3], __m128i sq[2], __m128i* const ma,
    __m128i* const b) {
  __m128i s3[3], sq3[3][2];
  sq[1] = SquareHi8(s);
  s3[2] = Sum3Horizontal(s);
  StoreAligned16(sum3[2], s3[2]);
  Sum3WHorizontal(sq, sq3[2]);
  StoreAligned32U32(square_sum3[2], sq3[2]);
  LoadAligned16x2U16(sum3, 0, s3);
  LoadAligned32x2U32(square_sum3, 0, sq3);
  CalculateIntermediate3(s3, sq3, scale, ma, b);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess3(
    const __m128i s[2], const ptrdiff_t x, const ptrdiff_t sum_width,
    const uint32_t scale, uint16_t* const sum3[3],
    uint32_t* const square_sum3[3], __m128i sq[4], __m128i ma[2],
    __m128i b[3]) {
  __m128i s3[4], sq3[3][2], sum[2], index[2];
  sq[2] = SquareLo8(s[1]);
  Sum3Horizontal<8>(s, s3 + 2);
  StoreAligned32U16(sum3[2] + x, s3 + 2);
  Sum3WHorizontal(sq + 1, sq3[2]);
  StoreAligned32U32(square_sum3[2] + x + 0, sq3[2]);
  LoadAligned16x2U16(sum3, x, s3);
  LoadAligned32x2U32(square_sum3, x, sq3);
  CalculateSumAndIndex3(s3, sq3, scale, &sum[0], &index[0]);

  sq[3] = SquareHi8(s[1]);
  Sum3WHorizontal(sq + 2, sq3[2]);
  StoreAligned32U32(square_sum3[2] + x + 8, sq3[2]);
  LoadAligned16x2U16Msan(sum3, x + 8, sum_width, s3 + 1);
  LoadAligned32x2U32Msan(square_sum3, x + 8, sum_width, sq3);
  CalculateSumAndIndex3(s3 + 1, sq3, scale, &sum[1], &index[1]);
  CalculateIntermediate(sum, index, ma, b + 1);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcessLo(
    const __m128i s[2][2], const uint16_t scales[2], uint16_t* const sum3[4],
    uint16_t* const sum5[5], uint32_t* const square_sum3[4],
    uint32_t* const square_sum5[5], __m128i sq[2][4], __m128i ma3[2][2],
    __m128i b3[2][3], __m128i* const ma5, __m128i* const b5) {
  __m128i s3[4], s5[5], sq3[4][2], sq5[5][2], sum[2], index[2];
  sq[0][1] = SquareHi8(s[0][0]);
  sq[1][1] = SquareHi8(s[1][0]);
  SumHorizontalLo(s[0][0], &s3[2], &s5[3]);
  SumHorizontalLo(s[1][0], &s3[3], &s5[4]);
  StoreAligned16(sum3[2], s3[2]);
  StoreAligned16(sum3[3], s3[3]);
  StoreAligned16(sum5[3], s5[3]);
  StoreAligned16(sum5[4], s5[4]);
  SumHorizontal(sq[0], &sq3[2][0], &sq3[2][1], &sq5[3][0], &sq5[3][1]);
  StoreAligned32U32(square_sum3[2], sq3[2]);
  StoreAligned32U32(square_sum5[3], sq5[3]);
  SumHorizontal(sq[1], &sq3[3][0], &sq3[3][1], &sq5[4][0], &sq5[4][1]);
  StoreAligned32U32(square_sum3[3], sq3[3]);
  StoreAligned32U32(square_sum5[4], sq5[4]);
  LoadAligned16x2U16(sum3, 0, s3);
  LoadAligned32x2U32(square_sum3, 0, sq3);
  LoadAligned16x3U16(sum5, 0, s5);
  LoadAligned32x3U32(square_sum5, 0, sq5);
  CalculateSumAndIndex3(s3 + 0, sq3 + 0, scales[1], &sum[0], &index[0]);
  CalculateSumAndIndex3(s3 + 1, sq3 + 1, scales[1], &sum[1], &index[1]);
  CalculateIntermediate(sum, index, &ma3[0][0], &b3[0][0], &b3[1][0]);
  ma3[1][0] = _mm_srli_si128(ma3[0][0], 8);
  CalculateIntermediate5<0>(s5, sq5, scales[0], ma5, b5);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess(
    const __m128i s[2][2], const ptrdiff_t x, const uint16_t scales[2],
    uint16_t* const sum3[4], uint16_t* const sum5[5],
    uint32_t* const square_sum3[4], uint32_t* const square_sum5[5],
    const ptrdiff_t sum_width, __m128i sq[2][4], __m128i ma3[2][2],
    __m128i b3[2][3], __m128i ma5[2], __m128i b5[3]) {
  __m128i s3[2][4], s5[2][5], sq3[4][2], sq5[5][2], sum[2][2], index[2][2];
  SumHorizontal<8>(s[0], &s3[0][2], &s3[1][2], &s5[0][3], &s5[1][3]);
  StoreAligned16(sum3[2] + x + 0, s3[0][2]);
  StoreAligned16(sum3[2] + x + 8, s3[1][2]);
  StoreAligned16(sum5[3] + x + 0, s5[0][3]);
  StoreAligned16(sum5[3] + x + 8, s5[1][3]);
  SumHorizontal<8>(s[1], &s3[0][3], &s3[1][3], &s5[0][4], &s5[1][4]);
  StoreAligned16(sum3[3] + x + 0, s3[0][3]);
  StoreAligned16(sum3[3] + x + 8, s3[1][3]);
  StoreAligned16(sum5[4] + x + 0, s5[0][4]);
  StoreAligned16(sum5[4] + x + 8, s5[1][4]);
  sq[0][2] = SquareLo8(s[0][1]);
  sq[1][2] = SquareLo8(s[1][1]);
  SumHorizontal(sq[0] + 1, &sq3[2][0], &sq3[2][1], &sq5[3][0], &sq5[3][1]);
  StoreAligned32U32(square_sum3[2] + x, sq3[2]);
  StoreAligned32U32(square_sum5[3] + x, sq5[3]);
  SumHorizontal(sq[1] + 1, &sq3[3][0], &sq3[3][1], &sq5[4][0], &sq5[4][1]);
  StoreAligned32U32(square_sum3[3] + x, sq3[3]);
  StoreAligned32U32(square_sum5[4] + x, sq5[4]);
  LoadAligned16x2U16(sum3, x, s3[0]);
  LoadAligned32x2U32(square_sum3, x, sq3);
  CalculateSumAndIndex3(s3[0], sq3, scales[1], &sum[0][0], &index[0][0]);
  CalculateSumAndIndex3(s3[0] + 1, sq3 + 1, scales[1], &sum[1][0],
                        &index[1][0]);
  LoadAligned16x3U16(sum5, x, s5[0]);
  LoadAligned32x3U32(square_sum5, x, sq5);
  CalculateIntermediate5<8>(s5[0], sq5, scales[0], &ma5[0], &b5[1]);

  sq[0][3] = SquareHi8(s[0][1]);
  sq[1][3] = SquareHi8(s[1][1]);
  SumHorizontal(sq[0] + 2, &sq3[2][0], &sq3[2][1], &sq5[3][0], &sq5[3][1]);
  StoreAligned32U32(square_sum3[2] + x + 8, sq3[2]);
  StoreAligned32U32(square_sum5[3] + x + 8, sq5[3]);
  SumHorizontal(sq[1] + 2, &sq3[3][0], &sq3[3][1], &sq5[4][0], &sq5[4][1]);
  StoreAligned32U32(square_sum3[3] + x + 8, sq3[3]);
  StoreAligned32U32(square_sum5[4] + x + 8, sq5[4]);
  LoadAligned16x2U16Msan(sum3, x + 8, sum_width, s3[1]);
  LoadAligned32x2U32Msan(square_sum3, x + 8, sum_width, sq3);
  CalculateSumAndIndex3(s3[1], sq3, scales[1], &sum[0][1], &index[0][1]);
  CalculateSumAndIndex3(s3[1] + 1, sq3 + 1, scales[1], &sum[1][1],
                        &index[1][1]);
  CalculateIntermediate(sum[0], index[0], ma3[0], b3[0] + 1);
  CalculateIntermediate(sum[1], index[1], ma3[1], b3[1] + 1);
  LoadAligned16x3U16Msan(sum5, x + 8, sum_width, s5[1]);
  LoadAligned32x3U32Msan(square_sum5, x + 8, sum_width, sq5);
  CalculateIntermediate5<0>(s5[1], sq5, scales[0], &ma5[1], &b5[2]);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcessLastRowLo(
    const __m128i s, const uint16_t scales[2], const uint16_t* const sum3[4],
    const uint16_t* const sum5[5], const uint32_t* const square_sum3[4],
    const uint32_t* const square_sum5[5], __m128i sq[2], __m128i* const ma3,
    __m128i* const ma5, __m128i* const b3, __m128i* const b5) {
  __m128i s3[3], s5[5], sq3[3][2], sq5[5][2];
  sq[1] = SquareHi8(s);
  SumHorizontalLo(s, &s3[2], &s5[3]);
  SumHorizontal(sq, &sq3[2][0], &sq3[2][1], &sq5[3][0], &sq5[3][1]);
  LoadAligned16x3U16(sum5, 0, s5);
  s5[4] = s5[3];
  LoadAligned32x3U32(square_sum5, 0, sq5);
  sq5[4][0] = sq5[3][0];
  sq5[4][1] = sq5[3][1];
  CalculateIntermediate5<0>(s5, sq5, scales[0], ma5, b5);
  LoadAligned16x2U16(sum3, 0, s3);
  LoadAligned32x2U32(square_sum3, 0, sq3);
  CalculateIntermediate3(s3, sq3, scales[1], ma3, b3);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcessLastRow(
    const __m128i s[2], const ptrdiff_t sum_width, const ptrdiff_t x,
    const uint16_t scales[2], const uint16_t* const sum3[4],
    const uint16_t* const sum5[5], const uint32_t* const square_sum3[4],
    const uint32_t* const square_sum5[5], __m128i sq[4], __m128i ma3[2],
    __m128i ma5[2], __m128i b3[3], __m128i b5[3]) {
  __m128i s3[2][3], s5[2][5], sq3[3][2], sq5[5][2], sum[2], index[2];
  sq[2] = SquareLo8(s[1]);
  SumHorizontal<8>(s, &s3[0][2], &s3[1][2], &s5[0][3], &s5[1][3]);
  SumHorizontal(sq + 1, &sq3[2][0], &sq3[2][1], &sq5[3][0], &sq5[3][1]);
  LoadAligned16x3U16(sum5, x, s5[0]);
  s5[0][4] = s5[0][3];
  LoadAligned32x3U32(square_sum5, x, sq5);
  sq5[4][0] = sq5[3][0];
  sq5[4][1] = sq5[3][1];
  CalculateIntermediate5<8>(s5[0], sq5, scales[0], ma5, b5 + 1);
  LoadAligned16x2U16(sum3, x, s3[0]);
  LoadAligned32x2U32(square_sum3, x, sq3);
  CalculateSumAndIndex3(s3[0], sq3, scales[1], &sum[0], &index[0]);

  sq[3] = SquareHi8(s[1]);
  SumHorizontal(sq + 2, &sq3[2][0], &sq3[2][1], &sq5[3][0], &sq5[3][1]);
  LoadAligned16x3U16Msan(sum5, x + 8, sum_width, s5[1]);
  s5[1][4] = s5[1][3];
  LoadAligned32x3U32Msan(square_sum5, x + 8, sum_width, sq5);
  sq5[4][0] = sq5[3][0];
  sq5[4][1] = sq5[3][1];
  CalculateIntermediate5<0>(s5[1], sq5, scales[0], ma5 + 1, b5 + 2);
  LoadAligned16x2U16Msan(sum3, x + 8, sum_width, s3[1]);
  LoadAligned32x2U32Msan(square_sum3, x + 8, sum_width, sq3);
  CalculateSumAndIndex3(s3[1], sq3, scales[1], &sum[1], &index[1]);
  CalculateIntermediate(sum, index, ma3, b3 + 1);
}

inline void BoxSumFilterPreProcess5(const uint8_t* const src0,
                                    const uint8_t* const src1, const int width,
                                    const uint32_t scale,
                                    uint16_t* const sum5[5],
                                    uint32_t* const square_sum5[5],
                                    const ptrdiff_t sum_width, uint16_t* ma565,
                                    uint32_t* b565) {
  __m128i s[2][2], mas[2], sq[2][4], bs[3];
  s[0][0] = LoadUnaligned16Msan(src0, kOverreadInBytesPass1 - width);
  s[1][0] = LoadUnaligned16Msan(src1, kOverreadInBytesPass1 - width);
  sq[0][0] = SquareLo8(s[0][0]);
  sq[1][0] = SquareLo8(s[1][0]);
  BoxFilterPreProcess5Lo(s, scale, sum5, square_sum5, sq, &mas[0], &bs[0]);

  int x = 0;
  do {
    __m128i ma5[3], ma[2], b[4];
    s[0][1] = LoadUnaligned16Msan(src0 + x + 16,
                                  x + 16 + kOverreadInBytesPass1 - width);
    s[1][1] = LoadUnaligned16Msan(src1 + x + 16,
                                  x + 16 + kOverreadInBytesPass1 - width);
    BoxFilterPreProcess5(s, sum_width, x + 8, scale, sum5, square_sum5, sq, mas,
                         bs);
    Prepare3_8<0>(mas, ma5);
    ma[0] = Sum565Lo(ma5);
    ma[1] = Sum565Hi(ma5);
    StoreAligned32U16(ma565, ma);
    Sum565W(bs + 0, b + 0);
    Sum565W(bs + 1, b + 2);
    StoreAligned64U32(b565, b);
    s[0][0] = s[0][1];
    s[1][0] = s[1][1];
    sq[0][1] = sq[0][3];
    sq[1][1] = sq[1][3];
    mas[0] = mas[1];
    bs[0] = bs[2];
    ma565 += 16;
    b565 += 16;
    x += 16;
  } while (x < width);
}

template <bool calculate444>
LIBGAV1_ALWAYS_INLINE void BoxSumFilterPreProcess3(
    const uint8_t* const src, const int width, const uint32_t scale,
    uint16_t* const sum3[3], uint32_t* const square_sum3[3],
    const ptrdiff_t sum_width, uint16_t* ma343, uint16_t* ma444, uint32_t* b343,
    uint32_t* b444) {
  __m128i s[2], mas[2], sq[4], bs[3];
  s[0] = LoadUnaligned16Msan(src, kOverreadInBytesPass2 - width);
  sq[0] = SquareLo8(s[0]);
  BoxFilterPreProcess3Lo(s[0], scale, sum3, square_sum3, sq, &mas[0], &bs[0]);

  int x = 0;
  do {
    s[1] = LoadUnaligned16Msan(src + x + 16,
                               x + 16 + kOverreadInBytesPass2 - width);
    BoxFilterPreProcess3(s, x + 8, sum_width, scale, sum3, square_sum3, sq, mas,
                         bs);
    __m128i ma3[3];
    Prepare3_8<0>(mas, ma3);
    if (calculate444) {  // NOLINT(readability-simplify-boolean-expr)
      Store343_444Lo(ma3, bs + 0, 0, ma343, ma444, b343, b444);
      Store343_444Hi(ma3, bs + 1, 8, ma343, ma444, b343, b444);
      ma444 += 16;
      b444 += 16;
    } else {
      __m128i ma[2], b[4];
      ma[0] = Sum343Lo(ma3);
      ma[1] = Sum343Hi(ma3);
      StoreAligned32U16(ma343, ma);
      Sum343W(bs + 0, b + 0);
      Sum343W(bs + 1, b + 2);
      StoreAligned64U32(b343, b);
    }
    s[0] = s[1];
    sq[1] = sq[3];
    mas[0] = mas[1];
    bs[0] = bs[2];
    ma343 += 16;
    b343 += 16;
    x += 16;
  } while (x < width);
}

inline void BoxSumFilterPreProcess(
    const uint8_t* const src0, const uint8_t* const src1, const int width,
    const uint16_t scales[2], uint16_t* const sum3[4], uint16_t* const sum5[5],
    uint32_t* const square_sum3[4], uint32_t* const square_sum5[5],
    const ptrdiff_t sum_width, uint16_t* const ma343[4], uint16_t* const ma444,
    uint16_t* ma565, uint32_t* const b343[4], uint32_t* const b444,
    uint32_t* b565) {
  __m128i s[2][2], ma3[2][2], ma5[2], sq[2][4], b3[2][3], b5[3];
  s[0][0] = LoadUnaligned16Msan(src0, kOverreadInBytesPass1 - width);
  s[1][0] = LoadUnaligned16Msan(src1, kOverreadInBytesPass1 - width);
  sq[0][0] = SquareLo8(s[0][0]);
  sq[1][0] = SquareLo8(s[1][0]);
  BoxFilterPreProcessLo(s, scales, sum3, sum5, square_sum3, square_sum5, sq,
                        ma3, b3, &ma5[0], &b5[0]);

  int x = 0;
  do {
    __m128i ma[2], b[4], ma3x[3], ma5x[3];
    s[0][1] = LoadUnaligned16Msan(src0 + x + 16,
                                  x + 16 + kOverreadInBytesPass1 - width);
    s[1][1] = LoadUnaligned16Msan(src1 + x + 16,
                                  x + 16 + kOverreadInBytesPass1 - width);
    BoxFilterPreProcess(s, x + 8, scales, sum3, sum5, square_sum3, square_sum5,
                        sum_width, sq, ma3, b3, ma5, b5);

    Prepare3_8<0>(ma3[0], ma3x);
    ma[0] = Sum343Lo(ma3x);
    ma[1] = Sum343Hi(ma3x);
    StoreAligned32U16(ma343[0] + x, ma);
    Sum343W(b3[0] + 0, b + 0);
    Sum343W(b3[0] + 1, b + 2);
    StoreAligned64U32(b343[0] + x, b);
    Sum565W(b5 + 0, b + 0);
    Sum565W(b5 + 1, b + 2);
    StoreAligned64U32(b565, b);
    Prepare3_8<0>(ma3[1], ma3x);
    Store343_444Lo(ma3x, b3[1], x, ma343[1], ma444, b343[1], b444);
    Store343_444Hi(ma3x, b3[1] + 1, x + 8, ma343[1], ma444, b343[1], b444);
    Prepare3_8<0>(ma5, ma5x);
    ma[0] = Sum565Lo(ma5x);
    ma[1] = Sum565Hi(ma5x);
    StoreAligned32U16(ma565, ma);
    s[0][0] = s[0][1];
    s[1][0] = s[1][1];
    sq[0][1] = sq[0][3];
    sq[1][1] = sq[1][3];
    ma3[0][0] = ma3[0][1];
    ma3[1][0] = ma3[1][1];
    ma5[0] = ma5[1];
    b3[0][0] = b3[0][2];
    b3[1][0] = b3[1][2];
    b5[0] = b5[2];
    ma565 += 16;
    b565 += 16;
    x += 16;
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
  const __m128i ma_x_src_lo = VmullLo16(ma, src);
  const __m128i ma_x_src_hi = VmullHi16(ma, src);
  const __m128i dst_lo = FilterOutput<shift>(ma_x_src_lo, b[0]);
  const __m128i dst_hi = FilterOutput<shift>(ma_x_src_hi, b[1]);
  return _mm_packs_epi32(dst_lo, dst_hi);  // 13 bits
}

inline __m128i CalculateFilteredOutputPass1(const __m128i src,
                                            const __m128i ma[2],
                                            const __m128i b[2][2]) {
  const __m128i ma_sum = _mm_add_epi16(ma[0], ma[1]);
  __m128i b_sum[2];
  b_sum[0] = _mm_add_epi32(b[0][0], b[1][0]);
  b_sum[1] = _mm_add_epi32(b[0][1], b[1][1]);
  return CalculateFilteredOutput<5>(src, ma_sum, b_sum);
}

inline __m128i CalculateFilteredOutputPass2(const __m128i src,
                                            const __m128i ma[3],
                                            const __m128i b[3][2]) {
  const __m128i ma_sum = Sum3_16(ma);
  __m128i b_sum[2];
  Sum3_32(b, b_sum);
  return CalculateFilteredOutput<5>(src, ma_sum, b_sum);
}

inline __m128i SelfGuidedFinal(const __m128i src, const __m128i v[2]) {
  const __m128i v_lo =
      VrshrS32(v[0], kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const __m128i v_hi =
      VrshrS32(v[1], kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const __m128i vv = _mm_packs_epi32(v_lo, v_hi);
  return _mm_add_epi16(src, vv);
}

inline __m128i SelfGuidedDoubleMultiplier(const __m128i src,
                                          const __m128i filter[2], const int w0,
                                          const int w2) {
  __m128i v[2];
  const __m128i w0_w2 = _mm_set1_epi32((w2 << 16) | static_cast<uint16_t>(w0));
  const __m128i f_lo = _mm_unpacklo_epi16(filter[0], filter[1]);
  const __m128i f_hi = _mm_unpackhi_epi16(filter[0], filter[1]);
  v[0] = _mm_madd_epi16(w0_w2, f_lo);
  v[1] = _mm_madd_epi16(w0_w2, f_hi);
  return SelfGuidedFinal(src, v);
}

inline __m128i SelfGuidedSingleMultiplier(const __m128i src,
                                          const __m128i filter, const int w0) {
  // weight: -96 to 96 (Sgrproj_Xqd_Min/Max)
  __m128i v[2];
  v[0] = VmullNLo8(filter, w0);
  v[1] = VmullNHi8(filter, w0);
  return SelfGuidedFinal(src, v);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPass1(
    const uint8_t* const src, const uint8_t* const src0,
    const uint8_t* const src1, const ptrdiff_t stride, uint16_t* const sum5[5],
    uint32_t* const square_sum5[5], const int width, const ptrdiff_t sum_width,
    const uint32_t scale, const int16_t w0, uint16_t* const ma565[2],
    uint32_t* const b565[2], uint8_t* const dst) {
  __m128i s[2][2], mas[2], sq[2][4], bs[3];
  s[0][0] = LoadUnaligned16Msan(src0, kOverreadInBytesPass1 - width);
  s[1][0] = LoadUnaligned16Msan(src1, kOverreadInBytesPass1 - width);
  sq[0][0] = SquareLo8(s[0][0]);
  sq[1][0] = SquareLo8(s[1][0]);
  BoxFilterPreProcess5Lo(s, scale, sum5, square_sum5, sq, &mas[0], &bs[0]);

  int x = 0;
  do {
    __m128i ma[2], ma5[3], b[2][2], sr[2], p[2];
    s[0][1] = LoadUnaligned16Msan(src0 + x + 16,
                                  x + 16 + kOverreadInBytesPass1 - width);
    s[1][1] = LoadUnaligned16Msan(src1 + x + 16,
                                  x + 16 + kOverreadInBytesPass1 - width);
    BoxFilterPreProcess5(s, sum_width, x + 8, scale, sum5, square_sum5, sq, mas,
                         bs);
    Prepare3_8<0>(mas, ma5);
    ma[1] = Sum565Lo(ma5);
    StoreAligned16(ma565[1] + x, ma[1]);
    Sum565W(bs, b[1]);
    StoreAligned32U32(b565[1] + x, b[1]);
    sr[0] = LoadAligned16(src + x);
    sr[1] = LoadAligned16(src + stride + x);
    const __m128i sr0_lo = _mm_unpacklo_epi8(sr[0], _mm_setzero_si128());
    const __m128i sr1_lo = _mm_unpacklo_epi8(sr[1], _mm_setzero_si128());
    ma[0] = LoadAligned16(ma565[0] + x);
    LoadAligned32U32(b565[0] + x, b[0]);
    p[0] = CalculateFilteredOutputPass1(sr0_lo, ma, b);
    p[1] = CalculateFilteredOutput<4>(sr1_lo, ma[1], b[1]);
    const __m128i d00 = SelfGuidedSingleMultiplier(sr0_lo, p[0], w0);
    const __m128i d10 = SelfGuidedSingleMultiplier(sr1_lo, p[1], w0);

    ma[1] = Sum565Hi(ma5);
    StoreAligned16(ma565[1] + x + 8, ma[1]);
    Sum565W(bs + 1, b[1]);
    StoreAligned32U32(b565[1] + x + 8, b[1]);
    const __m128i sr0_hi = _mm_unpackhi_epi8(sr[0], _mm_setzero_si128());
    const __m128i sr1_hi = _mm_unpackhi_epi8(sr[1], _mm_setzero_si128());
    ma[0] = LoadAligned16(ma565[0] + x + 8);
    LoadAligned32U32(b565[0] + x + 8, b[0]);
    p[0] = CalculateFilteredOutputPass1(sr0_hi, ma, b);
    p[1] = CalculateFilteredOutput<4>(sr1_hi, ma[1], b[1]);
    const __m128i d01 = SelfGuidedSingleMultiplier(sr0_hi, p[0], w0);
    StoreAligned16(dst + x, _mm_packus_epi16(d00, d01));
    const __m128i d11 = SelfGuidedSingleMultiplier(sr1_hi, p[1], w0);
    StoreAligned16(dst + stride + x, _mm_packus_epi16(d10, d11));
    s[0][0] = s[0][1];
    s[1][0] = s[1][1];
    sq[0][1] = sq[0][3];
    sq[1][1] = sq[1][3];
    mas[0] = mas[1];
    bs[0] = bs[2];
    x += 16;
  } while (x < width);
}

inline void BoxFilterPass1LastRow(
    const uint8_t* const src, const uint8_t* const src0, const int width,
    const ptrdiff_t sum_width, const uint32_t scale, const int16_t w0,
    uint16_t* const sum5[5], uint32_t* const square_sum5[5], uint16_t* ma565,
    uint32_t* b565, uint8_t* const dst) {
  __m128i s[2], mas[2], sq[4], bs[3];
  s[0] = LoadUnaligned16Msan(src0, kOverreadInBytesPass1 - width);
  sq[0] = SquareLo8(s[0]);
  BoxFilterPreProcess5LastRowLo(s[0], scale, sum5, square_sum5, sq, &mas[0],
                                &bs[0]);

  int x = 0;
  do {
    __m128i ma[2], ma5[3], b[2][2];
    s[1] = LoadUnaligned16Msan(src0 + x + 16,
                               x + 16 + kOverreadInBytesPass1 - width);
    BoxFilterPreProcess5LastRow(s, sum_width, x + 8, scale, sum5, square_sum5,
                                sq, mas, bs);
    Prepare3_8<0>(mas, ma5);
    ma[1] = Sum565Lo(ma5);
    Sum565W(bs, b[1]);
    ma[0] = LoadAligned16(ma565);
    LoadAligned32U32(b565, b[0]);
    const __m128i sr = LoadAligned16(src + x);
    const __m128i sr_lo = _mm_unpacklo_epi8(sr, _mm_setzero_si128());
    __m128i p = CalculateFilteredOutputPass1(sr_lo, ma, b);
    const __m128i d0 = SelfGuidedSingleMultiplier(sr_lo, p, w0);

    ma[1] = Sum565Hi(ma5);
    Sum565W(bs + 1, b[1]);
    ma[0] = LoadAligned16(ma565 + 8);
    LoadAligned32U32(b565 + 8, b[0]);
    const __m128i sr_hi = _mm_unpackhi_epi8(sr, _mm_setzero_si128());
    p = CalculateFilteredOutputPass1(sr_hi, ma, b);
    const __m128i d1 = SelfGuidedSingleMultiplier(sr_hi, p, w0);
    StoreAligned16(dst + x, _mm_packus_epi16(d0, d1));
    s[0] = s[1];
    sq[1] = sq[3];
    mas[0] = mas[1];
    bs[0] = bs[2];
    ma565 += 16;
    b565 += 16;
    x += 16;
  } while (x < width);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPass2(
    const uint8_t* const src, const uint8_t* const src0, const int width,
    const ptrdiff_t sum_width, const uint32_t scale, const int16_t w0,
    uint16_t* const sum3[3], uint32_t* const square_sum3[3],
    uint16_t* const ma343[3], uint16_t* const ma444[2], uint32_t* const b343[3],
    uint32_t* const b444[2], uint8_t* const dst) {
  __m128i s[2], mas[2], sq[4], bs[3];
  s[0] = LoadUnaligned16Msan(src0, kOverreadInBytesPass2 - width);
  sq[0] = SquareLo8(s[0]);
  BoxFilterPreProcess3Lo(s[0], scale, sum3, square_sum3, sq, &mas[0], &bs[0]);

  int x = 0;
  do {
    s[1] = LoadUnaligned16Msan(src0 + x + 16,
                               x + 16 + kOverreadInBytesPass2 - width);
    BoxFilterPreProcess3(s, x + 8, sum_width, scale, sum3, square_sum3, sq, mas,
                         bs);
    __m128i ma[3], b[3][2], ma3[3];
    Prepare3_8<0>(mas, ma3);
    Store343_444Lo(ma3, bs + 0, x, &ma[2], b[2], ma343[2], ma444[1], b343[2],
                   b444[1]);
    const __m128i sr = LoadAligned16(src + x);
    const __m128i sr_lo = _mm_unpacklo_epi8(sr, _mm_setzero_si128());
    ma[0] = LoadAligned16(ma343[0] + x);
    ma[1] = LoadAligned16(ma444[0] + x);
    LoadAligned32U32(b343[0] + x, b[0]);
    LoadAligned32U32(b444[0] + x, b[1]);
    const __m128i p0 = CalculateFilteredOutputPass2(sr_lo, ma, b);

    Store343_444Hi(ma3, bs + 1, x + 8, &ma[2], b[2], ma343[2], ma444[1],
                   b343[2], b444[1]);
    const __m128i sr_hi = _mm_unpackhi_epi8(sr, _mm_setzero_si128());
    ma[0] = LoadAligned16(ma343[0] + x + 8);
    ma[1] = LoadAligned16(ma444[0] + x + 8);
    LoadAligned32U32(b343[0] + x + 8, b[0]);
    LoadAligned32U32(b444[0] + x + 8, b[1]);
    const __m128i p1 = CalculateFilteredOutputPass2(sr_hi, ma, b);
    const __m128i d0 = SelfGuidedSingleMultiplier(sr_lo, p0, w0);
    const __m128i d1 = SelfGuidedSingleMultiplier(sr_hi, p1, w0);
    StoreAligned16(dst + x, _mm_packus_epi16(d0, d1));
    s[0] = s[1];
    sq[1] = sq[3];
    mas[0] = mas[1];
    bs[0] = bs[2];
    x += 16;
  } while (x < width);
}

LIBGAV1_ALWAYS_INLINE void BoxFilter(
    const uint8_t* const src, const uint8_t* const src0,
    const uint8_t* const src1, const ptrdiff_t stride, const int width,
    const uint16_t scales[2], const int16_t w0, const int16_t w2,
    uint16_t* const sum3[4], uint16_t* const sum5[5],
    uint32_t* const square_sum3[4], uint32_t* const square_sum5[5],
    const ptrdiff_t sum_width, uint16_t* const ma343[4],
    uint16_t* const ma444[3], uint16_t* const ma565[2], uint32_t* const b343[4],
    uint32_t* const b444[3], uint32_t* const b565[2], uint8_t* const dst) {
  __m128i s[2][2], ma3[2][2], ma5[2], sq[2][4], b3[2][3], b5[3];
  ma5[1] = _mm_setzero_si128();  // Quiets -Wmaybe-unintialized with gcc.
  s[0][0] = LoadUnaligned16Msan(src0, kOverreadInBytesPass1 - width);
  s[1][0] = LoadUnaligned16Msan(src1, kOverreadInBytesPass1 - width);
  sq[0][0] = SquareLo8(s[0][0]);
  sq[1][0] = SquareLo8(s[1][0]);
  BoxFilterPreProcessLo(s, scales, sum3, sum5, square_sum3, square_sum5, sq,
                        ma3, b3, &ma5[0], &b5[0]);

  int x = 0;
  do {
    __m128i ma[3][3], b[3][3][2], p[2][2], ma3x[2][3], ma5x[3];
    s[0][1] = LoadUnaligned16Msan(src0 + x + 16,
                                  x + 16 + kOverreadInBytesPass1 - width);
    s[1][1] = LoadUnaligned16Msan(src1 + x + 16,
                                  x + 16 + kOverreadInBytesPass1 - width);
    BoxFilterPreProcess(s, x + 8, scales, sum3, sum5, square_sum3, square_sum5,
                        sum_width, sq, ma3, b3, ma5, b5);
    Prepare3_8<0>(ma3[0], ma3x[0]);
    Prepare3_8<0>(ma3[1], ma3x[1]);
    Prepare3_8<0>(ma5, ma5x);
    Store343_444Lo(ma3x[0], b3[0], x, &ma[1][2], &ma[2][1], b[1][2], b[2][1],
                   ma343[2], ma444[1], b343[2], b444[1]);
    Store343_444Lo(ma3x[1], b3[1], x, &ma[2][2], b[2][2], ma343[3], ma444[2],
                   b343[3], b444[2]);
    ma[0][1] = Sum565Lo(ma5x);
    StoreAligned16(ma565[1] + x, ma[0][1]);
    Sum565W(b5, b[0][1]);
    StoreAligned32U32(b565[1] + x, b[0][1]);
    const __m128i sr0 = LoadAligned16(src + x);
    const __m128i sr1 = LoadAligned16(src + stride + x);
    const __m128i sr0_lo = _mm_unpacklo_epi8(sr0, _mm_setzero_si128());
    const __m128i sr1_lo = _mm_unpacklo_epi8(sr1, _mm_setzero_si128());
    ma[0][0] = LoadAligned16(ma565[0] + x);
    LoadAligned32U32(b565[0] + x, b[0][0]);
    p[0][0] = CalculateFilteredOutputPass1(sr0_lo, ma[0], b[0]);
    p[1][0] = CalculateFilteredOutput<4>(sr1_lo, ma[0][1], b[0][1]);
    ma[1][0] = LoadAligned16(ma343[0] + x);
    ma[1][1] = LoadAligned16(ma444[0] + x);
    LoadAligned32U32(b343[0] + x, b[1][0]);
    LoadAligned32U32(b444[0] + x, b[1][1]);
    p[0][1] = CalculateFilteredOutputPass2(sr0_lo, ma[1], b[1]);
    const __m128i d00 = SelfGuidedDoubleMultiplier(sr0_lo, p[0], w0, w2);
    ma[2][0] = LoadAligned16(ma343[1] + x);
    LoadAligned32U32(b343[1] + x, b[2][0]);
    p[1][1] = CalculateFilteredOutputPass2(sr1_lo, ma[2], b[2]);
    const __m128i d10 = SelfGuidedDoubleMultiplier(sr1_lo, p[1], w0, w2);

    Store343_444Hi(ma3x[0], b3[0] + 1, x + 8, &ma[1][2], &ma[2][1], b[1][2],
                   b[2][1], ma343[2], ma444[1], b343[2], b444[1]);
    Store343_444Hi(ma3x[1], b3[1] + 1, x + 8, &ma[2][2], b[2][2], ma343[3],
                   ma444[2], b343[3], b444[2]);
    ma[0][1] = Sum565Hi(ma5x);
    StoreAligned16(ma565[1] + x + 8, ma[0][1]);
    Sum565W(b5 + 1, b[0][1]);
    StoreAligned32U32(b565[1] + x + 8, b[0][1]);
    const __m128i sr0_hi = _mm_unpackhi_epi8(sr0, _mm_setzero_si128());
    const __m128i sr1_hi = _mm_unpackhi_epi8(sr1, _mm_setzero_si128());
    ma[0][0] = LoadAligned16(ma565[0] + x + 8);
    LoadAligned32U32(b565[0] + x + 8, b[0][0]);
    p[0][0] = CalculateFilteredOutputPass1(sr0_hi, ma[0], b[0]);
    p[1][0] = CalculateFilteredOutput<4>(sr1_hi, ma[0][1], b[0][1]);
    ma[1][0] = LoadAligned16(ma343[0] + x + 8);
    ma[1][1] = LoadAligned16(ma444[0] + x + 8);
    LoadAligned32U32(b343[0] + x + 8, b[1][0]);
    LoadAligned32U32(b444[0] + x + 8, b[1][1]);
    p[0][1] = CalculateFilteredOutputPass2(sr0_hi, ma[1], b[1]);
    const __m128i d01 = SelfGuidedDoubleMultiplier(sr0_hi, p[0], w0, w2);
    StoreAligned16(dst + x, _mm_packus_epi16(d00, d01));
    ma[2][0] = LoadAligned16(ma343[1] + x + 8);
    LoadAligned32U32(b343[1] + x + 8, b[2][0]);
    p[1][1] = CalculateFilteredOutputPass2(sr1_hi, ma[2], b[2]);
    const __m128i d11 = SelfGuidedDoubleMultiplier(sr1_hi, p[1], w0, w2);
    StoreAligned16(dst + stride + x, _mm_packus_epi16(d10, d11));
    s[0][0] = s[0][1];
    s[1][0] = s[1][1];
    sq[0][1] = sq[0][3];
    sq[1][1] = sq[1][3];
    ma3[0][0] = ma3[0][1];
    ma3[1][0] = ma3[1][1];
    ma5[0] = ma5[1];
    b3[0][0] = b3[0][2];
    b3[1][0] = b3[1][2];
    b5[0] = b5[2];
    x += 16;
  } while (x < width);
}

inline void BoxFilterLastRow(
    const uint8_t* const src, const uint8_t* const src0, const int width,
    const ptrdiff_t sum_width, const uint16_t scales[2], const int16_t w0,
    const int16_t w2, uint16_t* const sum3[4], uint16_t* const sum5[5],
    uint32_t* const square_sum3[4], uint32_t* const square_sum5[5],
    uint16_t* const ma343, uint16_t* const ma444, uint16_t* const ma565,
    uint32_t* const b343, uint32_t* const b444, uint32_t* const b565,
    uint8_t* const dst) {
  __m128i s[2], ma3[2], ma5[2], sq[4], b3[3], b5[3], ma[3], b[3][2];
  s[0] = LoadUnaligned16Msan(src0, kOverreadInBytesPass1 - width);
  sq[0] = SquareLo8(s[0]);
  BoxFilterPreProcessLastRowLo(s[0], scales, sum3, sum5, square_sum3,
                               square_sum5, sq, &ma3[0], &ma5[0], &b3[0],
                               &b5[0]);

  int x = 0;
  do {
    __m128i ma3x[3], ma5x[3], p[2];
    s[1] = LoadUnaligned16Msan(src0 + x + 16,
                               x + 16 + kOverreadInBytesPass1 - width);
    BoxFilterPreProcessLastRow(s, sum_width, x + 8, scales, sum3, sum5,
                               square_sum3, square_sum5, sq, ma3, ma5, b3, b5);
    Prepare3_8<0>(ma3, ma3x);
    Prepare3_8<0>(ma5, ma5x);
    ma[1] = Sum565Lo(ma5x);
    Sum565W(b5, b[1]);
    ma[2] = Sum343Lo(ma3x);
    Sum343W(b3, b[2]);
    const __m128i sr = LoadAligned16(src + x);
    const __m128i sr_lo = _mm_unpacklo_epi8(sr, _mm_setzero_si128());
    ma[0] = LoadAligned16(ma565 + x);
    LoadAligned32U32(b565 + x, b[0]);
    p[0] = CalculateFilteredOutputPass1(sr_lo, ma, b);
    ma[0] = LoadAligned16(ma343 + x);
    ma[1] = LoadAligned16(ma444 + x);
    LoadAligned32U32(b343 + x, b[0]);
    LoadAligned32U32(b444 + x, b[1]);
    p[1] = CalculateFilteredOutputPass2(sr_lo, ma, b);
    const __m128i d0 = SelfGuidedDoubleMultiplier(sr_lo, p, w0, w2);

    ma[1] = Sum565Hi(ma5x);
    Sum565W(b5 + 1, b[1]);
    ma[2] = Sum343Hi(ma3x);
    Sum343W(b3 + 1, b[2]);
    const __m128i sr_hi = _mm_unpackhi_epi8(sr, _mm_setzero_si128());
    ma[0] = LoadAligned16(ma565 + x + 8);
    LoadAligned32U32(b565 + x + 8, b[0]);
    p[0] = CalculateFilteredOutputPass1(sr_hi, ma, b);
    ma[0] = LoadAligned16(ma343 + x + 8);
    ma[1] = LoadAligned16(ma444 + x + 8);
    LoadAligned32U32(b343 + x + 8, b[0]);
    LoadAligned32U32(b444 + x + 8, b[1]);
    p[1] = CalculateFilteredOutputPass2(sr_hi, ma, b);
    const __m128i d1 = SelfGuidedDoubleMultiplier(sr_hi, p, w0, w2);
    StoreAligned16(dst + x, _mm_packus_epi16(d0, d1));
    s[0] = s[1];
    sq[1] = sq[3];
    ma3[0] = ma3[1];
    ma5[0] = ma5[1];
    b3[0] = b3[2];
    b5[0] = b5[2];
    x += 16;
  } while (x < width);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterProcess(
    const RestorationUnitInfo& restoration_info, const uint8_t* src,
    const ptrdiff_t stride, const uint8_t* const top_border,
    const ptrdiff_t top_border_stride, const uint8_t* bottom_border,
    const ptrdiff_t bottom_border_stride, const int width, const int height,
    SgrBuffer* const sgr_buffer, uint8_t* dst) {
  const auto temp_stride = Align<ptrdiff_t>(width, 16);
  const auto sum_width = Align<ptrdiff_t>(width + 8, 16);
  const auto sum_stride = temp_stride + 16;
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
  BoxSum(top_border, top_border_stride, width, sum_stride, sum_width, sum3[0],
         sum5[1], square_sum3[0], square_sum5[1]);
  sum5[0] = sum5[1];
  square_sum5[0] = square_sum5[1];
  const uint8_t* const s = (height > 1) ? src + stride : bottom_border;
  BoxSumFilterPreProcess(src, s, width, scales, sum3, sum5, square_sum3,
                         square_sum5, sum_width, ma343, ma444[0], ma565[0],
                         b343, b444[0], b565[0]);
  sum5[0] = sgr_buffer->sum5;
  square_sum5[0] = sgr_buffer->square_sum5;

  for (int y = (height >> 1) - 1; y > 0; --y) {
    Circulate4PointersBy2<uint16_t>(sum3);
    Circulate4PointersBy2<uint32_t>(square_sum3);
    Circulate5PointersBy2<uint16_t>(sum5);
    Circulate5PointersBy2<uint32_t>(square_sum5);
    BoxFilter(src + 3, src + 2 * stride, src + 3 * stride, stride, width,
              scales, w0, w2, sum3, sum5, square_sum3, square_sum5, sum_width,
              ma343, ma444, ma565, b343, b444, b565, dst);
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
      sr[1] = bottom_border + bottom_border_stride;
    } else {
      sr[0] = src + 2 * stride;
      sr[1] = bottom_border;
    }
    BoxFilter(src + 3, sr[0], sr[1], stride, width, scales, w0, w2, sum3, sum5,
              square_sum3, square_sum5, sum_width, ma343, ma444, ma565, b343,
              b444, b565, dst);
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
    BoxFilterLastRow(src + 3, bottom_border + bottom_border_stride, width,
                     sum_width, scales, w0, w2, sum3, sum5, square_sum3,
                     square_sum5, ma343[0], ma444[0], ma565[0], b343[0],
                     b444[0], b565[0], dst);
  }
}

inline void BoxFilterProcessPass1(const RestorationUnitInfo& restoration_info,
                                  const uint8_t* src, const ptrdiff_t stride,
                                  const uint8_t* const top_border,
                                  const ptrdiff_t top_border_stride,
                                  const uint8_t* bottom_border,
                                  const ptrdiff_t bottom_border_stride,
                                  const int width, const int height,
                                  SgrBuffer* const sgr_buffer, uint8_t* dst) {
  const auto temp_stride = Align<ptrdiff_t>(width, 16);
  const auto sum_width = Align<ptrdiff_t>(width + 8, 16);
  const auto sum_stride = temp_stride + 16;
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
  BoxSum<5>(top_border, top_border_stride, width, sum_stride, sum_width,
            sum5[1], square_sum5[1]);
  sum5[0] = sum5[1];
  square_sum5[0] = square_sum5[1];
  const uint8_t* const s = (height > 1) ? src + stride : bottom_border;
  BoxSumFilterPreProcess5(src, s, width, scale, sum5, square_sum5, sum_width,
                          ma565[0], b565[0]);
  sum5[0] = sgr_buffer->sum5;
  square_sum5[0] = sgr_buffer->square_sum5;

  for (int y = (height >> 1) - 1; y > 0; --y) {
    Circulate5PointersBy2<uint16_t>(sum5);
    Circulate5PointersBy2<uint32_t>(square_sum5);
    BoxFilterPass1(src + 3, src + 2 * stride, src + 3 * stride, stride, sum5,
                   square_sum5, width, sum_width, scale, w0, ma565, b565, dst);
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
      sr[1] = bottom_border + bottom_border_stride;
    } else {
      sr[0] = src + 2 * stride;
      sr[1] = bottom_border;
    }
    BoxFilterPass1(src + 3, sr[0], sr[1], stride, sum5, square_sum5, width,
                   sum_width, scale, w0, ma565, b565, dst);
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
    BoxFilterPass1LastRow(src, bottom_border + bottom_border_stride, width,
                          sum_width, scale, w0, sum5, square_sum5, ma565[0],
                          b565[0], dst);
  }
}

inline void BoxFilterProcessPass2(const RestorationUnitInfo& restoration_info,
                                  const uint8_t* src, const ptrdiff_t stride,
                                  const uint8_t* const top_border,
                                  const ptrdiff_t top_border_stride,
                                  const uint8_t* bottom_border,
                                  const ptrdiff_t bottom_border_stride,
                                  const int width, const int height,
                                  SgrBuffer* const sgr_buffer, uint8_t* dst) {
  assert(restoration_info.sgr_proj_info.multiplier[0] == 0);
  const auto temp_stride = Align<ptrdiff_t>(width, 16);
  const auto sum_width = Align<ptrdiff_t>(width + 8, 16);
  const auto sum_stride = temp_stride + 16;
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
  BoxSum<3>(top_border, top_border_stride, width, sum_stride, sum_width,
            sum3[0], square_sum3[0]);
  BoxSumFilterPreProcess3<false>(src, width, scale, sum3, square_sum3,
                                 sum_width, ma343[0], nullptr, b343[0],
                                 nullptr);
  Circulate3PointersBy1<uint16_t>(sum3);
  Circulate3PointersBy1<uint32_t>(square_sum3);
  const uint8_t* s;
  if (height > 1) {
    s = src + stride;
  } else {
    s = bottom_border;
    bottom_border += bottom_border_stride;
  }
  BoxSumFilterPreProcess3<true>(s, width, scale, sum3, square_sum3, sum_width,
                                ma343[1], ma444[0], b343[1], b444[0]);

  for (int y = height - 2; y > 0; --y) {
    Circulate3PointersBy1<uint16_t>(sum3);
    Circulate3PointersBy1<uint32_t>(square_sum3);
    BoxFilterPass2(src + 2, src + 2 * stride, width, sum_width, scale, w0, sum3,
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
    BoxFilterPass2(src, bottom_border, width, sum_width, scale, w0, sum3,
                   square_sum3, ma343, ma444, b343, b444, dst);
    src += stride;
    dst += stride;
    bottom_border += bottom_border_stride;
    Circulate3PointersBy1<uint16_t>(ma343);
    Circulate3PointersBy1<uint32_t>(b343);
    std::swap(ma444[0], ma444[1]);
    std::swap(b444[0], b444[1]);
  } while (--y != 0);
}

// If |width| is non-multiple of 16, up to 15 more pixels are written to |dest|
// in the end of each row. It is safe to overwrite the output as it will not be
// part of the visible frame.
void SelfGuidedFilter_SSE4_1(
    const RestorationUnitInfo& LIBGAV1_RESTRICT restoration_info,
    const void* LIBGAV1_RESTRICT const source, const ptrdiff_t stride,
    const void* LIBGAV1_RESTRICT const top_border,
    const ptrdiff_t top_border_stride,
    const void* LIBGAV1_RESTRICT const bottom_border,
    const ptrdiff_t bottom_border_stride, const int width, const int height,
    RestorationBuffer* LIBGAV1_RESTRICT const restoration_buffer,
    void* LIBGAV1_RESTRICT const dest) {
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
    BoxFilterProcessPass1(restoration_info, src - 3, stride, top - 3,
                          top_border_stride, bottom - 3, bottom_border_stride,
                          width, height, sgr_buffer, dst);
  } else if (radius_pass_0 == 0) {
    BoxFilterProcessPass2(restoration_info, src - 2, stride, top - 2,
                          top_border_stride, bottom - 2, bottom_border_stride,
                          width, height, sgr_buffer, dst);
  } else {
    BoxFilterProcess(restoration_info, src - 3, stride, top - 3,
                     top_border_stride, bottom - 3, bottom_border_stride, width,
                     height, sgr_buffer, dst);
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
  static_cast<void>(dsp);
#if DSP_ENABLED_8BPP_SSE4_1(WienerFilter)
  dsp->loop_restorations[0] = WienerFilter_SSE4_1;
#else
  static_cast<void>(WienerFilter_SSE4_1);
#endif
#if DSP_ENABLED_8BPP_SSE4_1(SelfGuidedFilter)
  dsp->loop_restorations[1] = SelfGuidedFilter_SSE4_1;
#else
  static_cast<void>(SelfGuidedFilter_SSE4_1);
#endif
}

}  // namespace
}  // namespace low_bitdepth

void LoopRestorationInit_SSE4_1() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_TARGETING_SSE4_1
namespace libgav1 {
namespace dsp {

void LoopRestorationInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_TARGETING_SSE4_1
