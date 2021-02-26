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

#if LIBGAV1_ENABLE_NEON
#include <arm_neon.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "src/dsp/arm/common_neon.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

template <int bytes>
inline uint8x8_t VshrU128(const uint8x8x2_t src) {
  return vext_u8(src.val[0], src.val[1], bytes);
}

template <int bytes>
inline uint16x8_t VshrU128(const uint16x8x2_t src) {
  return vextq_u16(src.val[0], src.val[1], bytes / 2);
}

// Wiener

// Must make a local copy of coefficients to help compiler know that they have
// no overlap with other buffers. Using 'const' keyword is not enough. Actually
// compiler doesn't make a copy, since there is enough registers in this case.
inline void PopulateWienerCoefficients(
    const RestorationUnitInfo& restoration_info, const int direction,
    int16_t filter[4]) {
  // In order to keep the horizontal pass intermediate values within 16 bits we
  // offset |filter[3]| by 128. The 128 offset will be added back in the loop.
  for (int i = 0; i < 4; ++i) {
    filter[i] = restoration_info.wiener_info.filter[direction][i];
  }
  if (direction == WienerInfo::kHorizontal) {
    filter[3] -= 128;
  }
}

inline int16x8_t WienerHorizontal2(const uint8x8_t s0, const uint8x8_t s1,
                                   const int16_t filter, const int16x8_t sum) {
  const int16x8_t ss = vreinterpretq_s16_u16(vaddl_u8(s0, s1));
  return vmlaq_n_s16(sum, ss, filter);
}

inline int16x8x2_t WienerHorizontal2(const uint8x16_t s0, const uint8x16_t s1,
                                     const int16_t filter,
                                     const int16x8x2_t sum) {
  int16x8x2_t d;
  d.val[0] =
      WienerHorizontal2(vget_low_u8(s0), vget_low_u8(s1), filter, sum.val[0]);
  d.val[1] =
      WienerHorizontal2(vget_high_u8(s0), vget_high_u8(s1), filter, sum.val[1]);
  return d;
}

inline void WienerHorizontalSum(const uint8x8_t s[3], const int16_t filter[4],
                                int16x8_t sum, int16_t* const wiener_buffer) {
  constexpr int offset =
      1 << (8 + kWienerFilterBits - kInterRoundBitsHorizontal - 1);
  constexpr int limit = (offset << 2) - 1;
  const int16x8_t s_0_2 = vreinterpretq_s16_u16(vaddl_u8(s[0], s[2]));
  const int16x8_t s_1 = ZeroExtend(s[1]);
  sum = vmlaq_n_s16(sum, s_0_2, filter[2]);
  sum = vmlaq_n_s16(sum, s_1, filter[3]);
  // Calculate scaled down offset correction, and add to sum here to prevent
  // signed 16 bit outranging.
  sum = vrsraq_n_s16(vshlq_n_s16(s_1, 7 - kInterRoundBitsHorizontal), sum,
                     kInterRoundBitsHorizontal);
  sum = vmaxq_s16(sum, vdupq_n_s16(-offset));
  sum = vminq_s16(sum, vdupq_n_s16(limit - offset));
  vst1q_s16(wiener_buffer, sum);
}

inline void WienerHorizontalSum(const uint8x16_t src[3],
                                const int16_t filter[4], int16x8x2_t sum,
                                int16_t* const wiener_buffer) {
  uint8x8_t s[3];
  s[0] = vget_low_u8(src[0]);
  s[1] = vget_low_u8(src[1]);
  s[2] = vget_low_u8(src[2]);
  WienerHorizontalSum(s, filter, sum.val[0], wiener_buffer);
  s[0] = vget_high_u8(src[0]);
  s[1] = vget_high_u8(src[1]);
  s[2] = vget_high_u8(src[2]);
  WienerHorizontalSum(s, filter, sum.val[1], wiener_buffer + 8);
}

inline void WienerHorizontalTap7(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 const int16_t filter[4],
                                 int16_t** const wiener_buffer) {
  for (int y = height; y != 0; --y) {
    const uint8_t* src_ptr = src;
    uint8x16_t s[8];
    s[0] = vld1q_u8(src_ptr);
    ptrdiff_t x = width;
    do {
      src_ptr += 16;
      s[7] = vld1q_u8(src_ptr);
      s[1] = vextq_u8(s[0], s[7], 1);
      s[2] = vextq_u8(s[0], s[7], 2);
      s[3] = vextq_u8(s[0], s[7], 3);
      s[4] = vextq_u8(s[0], s[7], 4);
      s[5] = vextq_u8(s[0], s[7], 5);
      s[6] = vextq_u8(s[0], s[7], 6);
      int16x8x2_t sum;
      sum.val[0] = sum.val[1] = vdupq_n_s16(0);
      sum = WienerHorizontal2(s[0], s[6], filter[0], sum);
      sum = WienerHorizontal2(s[1], s[5], filter[1], sum);
      WienerHorizontalSum(s + 2, filter, sum, *wiener_buffer);
      s[0] = s[7];
      *wiener_buffer += 16;
      x -= 16;
    } while (x != 0);
    src += src_stride;
  }
}

inline void WienerHorizontalTap5(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 const int16_t filter[4],
                                 int16_t** const wiener_buffer) {
  for (int y = height; y != 0; --y) {
    const uint8_t* src_ptr = src;
    uint8x16_t s[6];
    s[0] = vld1q_u8(src_ptr);
    ptrdiff_t x = width;
    do {
      src_ptr += 16;
      s[5] = vld1q_u8(src_ptr);
      s[1] = vextq_u8(s[0], s[5], 1);
      s[2] = vextq_u8(s[0], s[5], 2);
      s[3] = vextq_u8(s[0], s[5], 3);
      s[4] = vextq_u8(s[0], s[5], 4);
      int16x8x2_t sum;
      sum.val[0] = sum.val[1] = vdupq_n_s16(0);
      sum = WienerHorizontal2(s[0], s[4], filter[1], sum);
      WienerHorizontalSum(s + 1, filter, sum, *wiener_buffer);
      s[0] = s[5];
      *wiener_buffer += 16;
      x -= 16;
    } while (x != 0);
    src += src_stride;
  }
}

inline void WienerHorizontalTap3(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 const int16_t filter[4],
                                 int16_t** const wiener_buffer) {
  for (int y = height; y != 0; --y) {
    const uint8_t* src_ptr = src;
    uint8x16_t s[4];
    s[0] = vld1q_u8(src_ptr);
    ptrdiff_t x = width;
    do {
      src_ptr += 16;
      s[3] = vld1q_u8(src_ptr);
      s[1] = vextq_u8(s[0], s[3], 1);
      s[2] = vextq_u8(s[0], s[3], 2);
      int16x8x2_t sum;
      sum.val[0] = sum.val[1] = vdupq_n_s16(0);
      WienerHorizontalSum(s, filter, sum, *wiener_buffer);
      s[0] = s[3];
      *wiener_buffer += 16;
      x -= 16;
    } while (x != 0);
    src += src_stride;
  }
}

inline void WienerHorizontalTap1(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 int16_t** const wiener_buffer) {
  for (int y = height; y != 0; --y) {
    const uint8_t* src_ptr = src;
    ptrdiff_t x = width;
    do {
      const uint8x16_t s = vld1q_u8(src_ptr);
      const uint8x8_t s0 = vget_low_u8(s);
      const uint8x8_t s1 = vget_high_u8(s);
      const int16x8_t d0 = vreinterpretq_s16_u16(vshll_n_u8(s0, 4));
      const int16x8_t d1 = vreinterpretq_s16_u16(vshll_n_u8(s1, 4));
      vst1q_s16(*wiener_buffer + 0, d0);
      vst1q_s16(*wiener_buffer + 8, d1);
      src_ptr += 16;
      *wiener_buffer += 16;
      x -= 16;
    } while (x != 0);
    src += src_stride;
  }
}

inline int32x4x2_t WienerVertical2(const int16x8_t a0, const int16x8_t a1,
                                   const int16_t filter,
                                   const int32x4x2_t sum) {
  const int16x8_t a = vaddq_s16(a0, a1);
  int32x4x2_t d;
  d.val[0] = vmlal_n_s16(sum.val[0], vget_low_s16(a), filter);
  d.val[1] = vmlal_n_s16(sum.val[1], vget_high_s16(a), filter);
  return d;
}

inline uint8x8_t WienerVertical(const int16x8_t a[3], const int16_t filter[4],
                                const int32x4x2_t sum) {
  int32x4x2_t d = WienerVertical2(a[0], a[2], filter[2], sum);
  d.val[0] = vmlal_n_s16(d.val[0], vget_low_s16(a[1]), filter[3]);
  d.val[1] = vmlal_n_s16(d.val[1], vget_high_s16(a[1]), filter[3]);
  const uint16x4_t sum_lo_16 = vqrshrun_n_s32(d.val[0], 11);
  const uint16x4_t sum_hi_16 = vqrshrun_n_s32(d.val[1], 11);
  return vqmovn_u16(vcombine_u16(sum_lo_16, sum_hi_16));
}

inline uint8x8_t WienerVerticalTap7Kernel(const int16_t* const wiener_buffer,
                                          const ptrdiff_t wiener_stride,
                                          const int16_t filter[4],
                                          int16x8_t a[7]) {
  int32x4x2_t sum;
  a[0] = vld1q_s16(wiener_buffer + 0 * wiener_stride);
  a[1] = vld1q_s16(wiener_buffer + 1 * wiener_stride);
  a[5] = vld1q_s16(wiener_buffer + 5 * wiener_stride);
  a[6] = vld1q_s16(wiener_buffer + 6 * wiener_stride);
  sum.val[0] = sum.val[1] = vdupq_n_s32(0);
  sum = WienerVertical2(a[0], a[6], filter[0], sum);
  sum = WienerVertical2(a[1], a[5], filter[1], sum);
  a[2] = vld1q_s16(wiener_buffer + 2 * wiener_stride);
  a[3] = vld1q_s16(wiener_buffer + 3 * wiener_stride);
  a[4] = vld1q_s16(wiener_buffer + 4 * wiener_stride);
  return WienerVertical(a + 2, filter, sum);
}

inline uint8x8x2_t WienerVerticalTap7Kernel2(const int16_t* const wiener_buffer,
                                             const ptrdiff_t wiener_stride,
                                             const int16_t filter[4]) {
  int16x8_t a[8];
  int32x4x2_t sum;
  uint8x8x2_t d;
  d.val[0] = WienerVerticalTap7Kernel(wiener_buffer, wiener_stride, filter, a);
  a[7] = vld1q_s16(wiener_buffer + 7 * wiener_stride);
  sum.val[0] = sum.val[1] = vdupq_n_s32(0);
  sum = WienerVertical2(a[1], a[7], filter[0], sum);
  sum = WienerVertical2(a[2], a[6], filter[1], sum);
  d.val[1] = WienerVertical(a + 3, filter, sum);
  return d;
}

inline void WienerVerticalTap7(const int16_t* wiener_buffer,
                               const ptrdiff_t width, const int height,
                               const int16_t filter[4], uint8_t* dst,
                               const ptrdiff_t dst_stride) {
  for (int y = height >> 1; y != 0; --y) {
    uint8_t* dst_ptr = dst;
    ptrdiff_t x = width;
    do {
      uint8x8x2_t d[2];
      d[0] = WienerVerticalTap7Kernel2(wiener_buffer + 0, width, filter);
      d[1] = WienerVerticalTap7Kernel2(wiener_buffer + 8, width, filter);
      vst1q_u8(dst_ptr, vcombine_u8(d[0].val[0], d[1].val[0]));
      vst1q_u8(dst_ptr + dst_stride, vcombine_u8(d[0].val[1], d[1].val[1]));
      wiener_buffer += 16;
      dst_ptr += 16;
      x -= 16;
    } while (x != 0);
    wiener_buffer += width;
    dst += 2 * dst_stride;
  }

  if ((height & 1) != 0) {
    ptrdiff_t x = width;
    do {
      int16x8_t a[7];
      const uint8x8_t d0 =
          WienerVerticalTap7Kernel(wiener_buffer + 0, width, filter, a);
      const uint8x8_t d1 =
          WienerVerticalTap7Kernel(wiener_buffer + 8, width, filter, a);
      vst1q_u8(dst, vcombine_u8(d0, d1));
      wiener_buffer += 16;
      dst += 16;
      x -= 16;
    } while (x != 0);
  }
}

inline uint8x8_t WienerVerticalTap5Kernel(const int16_t* const wiener_buffer,
                                          const ptrdiff_t wiener_stride,
                                          const int16_t filter[4],
                                          int16x8_t a[5]) {
  a[0] = vld1q_s16(wiener_buffer + 0 * wiener_stride);
  a[1] = vld1q_s16(wiener_buffer + 1 * wiener_stride);
  a[2] = vld1q_s16(wiener_buffer + 2 * wiener_stride);
  a[3] = vld1q_s16(wiener_buffer + 3 * wiener_stride);
  a[4] = vld1q_s16(wiener_buffer + 4 * wiener_stride);
  int32x4x2_t sum;
  sum.val[0] = sum.val[1] = vdupq_n_s32(0);
  sum = WienerVertical2(a[0], a[4], filter[1], sum);
  return WienerVertical(a + 1, filter, sum);
}

inline uint8x8x2_t WienerVerticalTap5Kernel2(const int16_t* const wiener_buffer,
                                             const ptrdiff_t wiener_stride,
                                             const int16_t filter[4]) {
  int16x8_t a[6];
  int32x4x2_t sum;
  uint8x8x2_t d;
  d.val[0] = WienerVerticalTap5Kernel(wiener_buffer, wiener_stride, filter, a);
  a[5] = vld1q_s16(wiener_buffer + 5 * wiener_stride);
  sum.val[0] = sum.val[1] = vdupq_n_s32(0);
  sum = WienerVertical2(a[1], a[5], filter[1], sum);
  d.val[1] = WienerVertical(a + 2, filter, sum);
  return d;
}

inline void WienerVerticalTap5(const int16_t* wiener_buffer,
                               const ptrdiff_t width, const int height,
                               const int16_t filter[4], uint8_t* dst,
                               const ptrdiff_t dst_stride) {
  for (int y = height >> 1; y != 0; --y) {
    uint8_t* dst_ptr = dst;
    ptrdiff_t x = width;
    do {
      uint8x8x2_t d[2];
      d[0] = WienerVerticalTap5Kernel2(wiener_buffer + 0, width, filter);
      d[1] = WienerVerticalTap5Kernel2(wiener_buffer + 8, width, filter);
      vst1q_u8(dst_ptr, vcombine_u8(d[0].val[0], d[1].val[0]));
      vst1q_u8(dst_ptr + dst_stride, vcombine_u8(d[0].val[1], d[1].val[1]));
      wiener_buffer += 16;
      dst_ptr += 16;
      x -= 16;
    } while (x != 0);
    wiener_buffer += width;
    dst += 2 * dst_stride;
  }

  if ((height & 1) != 0) {
    ptrdiff_t x = width;
    do {
      int16x8_t a[5];
      const uint8x8_t d0 =
          WienerVerticalTap5Kernel(wiener_buffer + 0, width, filter, a);
      const uint8x8_t d1 =
          WienerVerticalTap5Kernel(wiener_buffer + 8, width, filter, a);
      vst1q_u8(dst, vcombine_u8(d0, d1));
      wiener_buffer += 16;
      dst += 16;
      x -= 16;
    } while (x != 0);
  }
}

inline uint8x8_t WienerVerticalTap3Kernel(const int16_t* const wiener_buffer,
                                          const ptrdiff_t wiener_stride,
                                          const int16_t filter[4],
                                          int16x8_t a[3]) {
  a[0] = vld1q_s16(wiener_buffer + 0 * wiener_stride);
  a[1] = vld1q_s16(wiener_buffer + 1 * wiener_stride);
  a[2] = vld1q_s16(wiener_buffer + 2 * wiener_stride);
  int32x4x2_t sum;
  sum.val[0] = sum.val[1] = vdupq_n_s32(0);
  return WienerVertical(a, filter, sum);
}

inline uint8x8x2_t WienerVerticalTap3Kernel2(const int16_t* const wiener_buffer,
                                             const ptrdiff_t wiener_stride,
                                             const int16_t filter[4]) {
  int16x8_t a[4];
  int32x4x2_t sum;
  uint8x8x2_t d;
  d.val[0] = WienerVerticalTap3Kernel(wiener_buffer, wiener_stride, filter, a);
  a[3] = vld1q_s16(wiener_buffer + 3 * wiener_stride);
  sum.val[0] = sum.val[1] = vdupq_n_s32(0);
  d.val[1] = WienerVertical(a + 1, filter, sum);
  return d;
}

inline void WienerVerticalTap3(const int16_t* wiener_buffer,
                               const ptrdiff_t width, const int height,
                               const int16_t filter[4], uint8_t* dst,
                               const ptrdiff_t dst_stride) {
  for (int y = height >> 1; y != 0; --y) {
    uint8_t* dst_ptr = dst;
    ptrdiff_t x = width;
    do {
      uint8x8x2_t d[2];
      d[0] = WienerVerticalTap3Kernel2(wiener_buffer + 0, width, filter);
      d[1] = WienerVerticalTap3Kernel2(wiener_buffer + 8, width, filter);
      vst1q_u8(dst_ptr, vcombine_u8(d[0].val[0], d[1].val[0]));
      vst1q_u8(dst_ptr + dst_stride, vcombine_u8(d[0].val[1], d[1].val[1]));
      wiener_buffer += 16;
      dst_ptr += 16;
      x -= 16;
    } while (x != 0);
    wiener_buffer += width;
    dst += 2 * dst_stride;
  }

  if ((height & 1) != 0) {
    ptrdiff_t x = width;
    do {
      int16x8_t a[3];
      const uint8x8_t d0 =
          WienerVerticalTap3Kernel(wiener_buffer + 0, width, filter, a);
      const uint8x8_t d1 =
          WienerVerticalTap3Kernel(wiener_buffer + 8, width, filter, a);
      vst1q_u8(dst, vcombine_u8(d0, d1));
      wiener_buffer += 16;
      dst += 16;
      x -= 16;
    } while (x != 0);
  }
}

inline void WienerVerticalTap1Kernel(const int16_t* const wiener_buffer,
                                     uint8_t* const dst) {
  const int16x8_t a0 = vld1q_s16(wiener_buffer + 0);
  const int16x8_t a1 = vld1q_s16(wiener_buffer + 8);
  const uint8x8_t d0 = vqrshrun_n_s16(a0, 4);
  const uint8x8_t d1 = vqrshrun_n_s16(a1, 4);
  vst1q_u8(dst, vcombine_u8(d0, d1));
}

inline void WienerVerticalTap1(const int16_t* wiener_buffer,
                               const ptrdiff_t width, const int height,
                               uint8_t* dst, const ptrdiff_t dst_stride) {
  for (int y = height >> 1; y != 0; --y) {
    uint8_t* dst_ptr = dst;
    ptrdiff_t x = width;
    do {
      WienerVerticalTap1Kernel(wiener_buffer, dst_ptr);
      WienerVerticalTap1Kernel(wiener_buffer + width, dst_ptr + dst_stride);
      wiener_buffer += 16;
      dst_ptr += 16;
      x -= 16;
    } while (x != 0);
    wiener_buffer += width;
    dst += 2 * dst_stride;
  }

  if ((height & 1) != 0) {
    ptrdiff_t x = width;
    do {
      WienerVerticalTap1Kernel(wiener_buffer, dst);
      wiener_buffer += 16;
      dst += 16;
      x -= 16;
    } while (x != 0);
  }
}

// For width 16 and up, store the horizontal results, and then do the vertical
// filter row by row. This is faster than doing it column by column when
// considering cache issues.
void WienerFilter_NEON(const RestorationUnitInfo& restoration_info,
                       const void* const source, const void* const top_border,
                       const void* const bottom_border, const ptrdiff_t stride,
                       const int width, const int height,
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
  int16_t filter_horizontal[(kWienerFilterTaps + 1) / 2];
  int16_t filter_vertical[(kWienerFilterTaps + 1) / 2];
  PopulateWienerCoefficients(restoration_info, WienerInfo::kHorizontal,
                             filter_horizontal);
  PopulateWienerCoefficients(restoration_info, WienerInfo::kVertical,
                             filter_vertical);

  // horizontal filtering.
  // Over-reads up to 15 - |kRestorationHorizontalBorder| values.
  const int height_horizontal =
      height + kWienerFilterTaps - 1 - 2 * number_rows_to_skip;
  const int height_extra = (height_horizontal - height) >> 1;
  assert(height_extra <= 2);
  const auto* const src = static_cast<const uint8_t*>(source);
  const auto* const top = static_cast<const uint8_t*>(top_border);
  const auto* const bottom = static_cast<const uint8_t*>(bottom_border);
  if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 0) {
    WienerHorizontalTap7(top + (2 - height_extra) * stride - 3, stride,
                         wiener_stride, height_extra, filter_horizontal,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap7(src - 3, stride, wiener_stride, height,
                         filter_horizontal, &wiener_buffer_horizontal);
    WienerHorizontalTap7(bottom - 3, stride, wiener_stride, height_extra,
                         filter_horizontal, &wiener_buffer_horizontal);
  } else if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 1) {
    WienerHorizontalTap5(top + (2 - height_extra) * stride - 2, stride,
                         wiener_stride, height_extra, filter_horizontal,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap5(src - 2, stride, wiener_stride, height,
                         filter_horizontal, &wiener_buffer_horizontal);
    WienerHorizontalTap5(bottom - 2, stride, wiener_stride, height_extra,
                         filter_horizontal, &wiener_buffer_horizontal);
  } else if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 2) {
    // The maximum over-reads happen here.
    WienerHorizontalTap3(top + (2 - height_extra) * stride - 1, stride,
                         wiener_stride, height_extra, filter_horizontal,
                         &wiener_buffer_horizontal);
    WienerHorizontalTap3(src - 1, stride, wiener_stride, height,
                         filter_horizontal, &wiener_buffer_horizontal);
    WienerHorizontalTap3(bottom - 1, stride, wiener_stride, height_extra,
                         filter_horizontal, &wiener_buffer_horizontal);
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
                       height, filter_vertical, dst, stride);
  } else if (number_leading_zero_coefficients[WienerInfo::kVertical] == 2) {
    WienerVerticalTap3(wiener_buffer_vertical + 2 * wiener_stride,
                       wiener_stride, height, filter_vertical, dst, stride);
  } else {
    assert(number_leading_zero_coefficients[WienerInfo::kVertical] == 3);
    WienerVerticalTap1(wiener_buffer_vertical + 3 * wiener_stride,
                       wiener_stride, height, dst, stride);
  }
}

//------------------------------------------------------------------------------
// SGR

inline void Prepare3_8(const uint8x8x2_t src, uint8x8_t dst[3]) {
  dst[0] = VshrU128<0>(src);
  dst[1] = VshrU128<1>(src);
  dst[2] = VshrU128<2>(src);
}

inline void Prepare3_16(const uint16x8x2_t src, uint16x4_t low[3],
                        uint16x4_t high[3]) {
  uint16x8_t s[3];
  s[0] = VshrU128<0>(src);
  s[1] = VshrU128<2>(src);
  s[2] = VshrU128<4>(src);
  low[0] = vget_low_u16(s[0]);
  low[1] = vget_low_u16(s[1]);
  low[2] = vget_low_u16(s[2]);
  high[0] = vget_high_u16(s[0]);
  high[1] = vget_high_u16(s[1]);
  high[2] = vget_high_u16(s[2]);
}

inline void Prepare5_8(const uint8x8x2_t src, uint8x8_t dst[5]) {
  dst[0] = VshrU128<0>(src);
  dst[1] = VshrU128<1>(src);
  dst[2] = VshrU128<2>(src);
  dst[3] = VshrU128<3>(src);
  dst[4] = VshrU128<4>(src);
}

inline void Prepare5_16(const uint16x8x2_t src, uint16x4_t low[5],
                        uint16x4_t high[5]) {
  Prepare3_16(src, low, high);
  const uint16x8_t s3 = VshrU128<6>(src);
  const uint16x8_t s4 = VshrU128<8>(src);
  low[3] = vget_low_u16(s3);
  low[4] = vget_low_u16(s4);
  high[3] = vget_high_u16(s3);
  high[4] = vget_high_u16(s4);
}

inline uint16x8_t Sum3_16(const uint16x8_t src0, const uint16x8_t src1,
                          const uint16x8_t src2) {
  const uint16x8_t sum = vaddq_u16(src0, src1);
  return vaddq_u16(sum, src2);
}

inline uint16x8_t Sum3_16(const uint16x8_t src[3]) {
  return Sum3_16(src[0], src[1], src[2]);
}

inline uint32x4_t Sum3_32(const uint32x4_t src0, const uint32x4_t src1,
                          const uint32x4_t src2) {
  const uint32x4_t sum = vaddq_u32(src0, src1);
  return vaddq_u32(sum, src2);
}

inline uint32x4x2_t Sum3_32(const uint32x4x2_t src[3]) {
  uint32x4x2_t d;
  d.val[0] = Sum3_32(src[0].val[0], src[1].val[0], src[2].val[0]);
  d.val[1] = Sum3_32(src[0].val[1], src[1].val[1], src[2].val[1]);
  return d;
}

inline uint16x8_t Sum3W_16(const uint8x8_t src[3]) {
  const uint16x8_t sum = vaddl_u8(src[0], src[1]);
  return vaddw_u8(sum, src[2]);
}

inline uint32x4_t Sum3W_32(const uint16x4_t src[3]) {
  const uint32x4_t sum = vaddl_u16(src[0], src[1]);
  return vaddw_u16(sum, src[2]);
}

inline uint16x8_t Sum5_16(const uint16x8_t src[5]) {
  const uint16x8_t sum01 = vaddq_u16(src[0], src[1]);
  const uint16x8_t sum23 = vaddq_u16(src[2], src[3]);
  const uint16x8_t sum = vaddq_u16(sum01, sum23);
  return vaddq_u16(sum, src[4]);
}

inline uint32x4_t Sum5_32(const uint32x4_t src0, const uint32x4_t src1,
                          const uint32x4_t src2, const uint32x4_t src3,
                          const uint32x4_t src4) {
  const uint32x4_t sum01 = vaddq_u32(src0, src1);
  const uint32x4_t sum23 = vaddq_u32(src2, src3);
  const uint32x4_t sum = vaddq_u32(sum01, sum23);
  return vaddq_u32(sum, src4);
}

inline uint32x4x2_t Sum5_32(const uint32x4x2_t src[5]) {
  uint32x4x2_t d;
  d.val[0] = Sum5_32(src[0].val[0], src[1].val[0], src[2].val[0], src[3].val[0],
                     src[4].val[0]);
  d.val[1] = Sum5_32(src[0].val[1], src[1].val[1], src[2].val[1], src[3].val[1],
                     src[4].val[1]);
  return d;
}

inline uint32x4_t Sum5W_32(const uint16x4_t src[5]) {
  const uint32x4_t sum01 = vaddl_u16(src[0], src[1]);
  const uint32x4_t sum23 = vaddl_u16(src[2], src[3]);
  const uint32x4_t sum0123 = vaddq_u32(sum01, sum23);
  return vaddw_u16(sum0123, src[4]);
}

inline uint16x8_t Sum3Horizontal(const uint8x8x2_t src) {
  uint8x8_t s[3];
  Prepare3_8(src, s);
  return Sum3W_16(s);
}

inline uint32x4x2_t Sum3WHorizontal(const uint16x8x2_t src) {
  uint16x4_t low[3], high[3];
  uint32x4x2_t sum;
  Prepare3_16(src, low, high);
  sum.val[0] = Sum3W_32(low);
  sum.val[1] = Sum3W_32(high);
  return sum;
}

inline uint16x8_t Sum5Horizontal(const uint8x8x2_t src) {
  uint8x8_t s[5];
  Prepare5_8(src, s);
  const uint16x8_t sum01 = vaddl_u8(s[0], s[1]);
  const uint16x8_t sum23 = vaddl_u8(s[2], s[3]);
  const uint16x8_t sum0123 = vaddq_u16(sum01, sum23);
  return vaddw_u8(sum0123, s[4]);
}

inline uint32x4x2_t Sum5WHorizontal(const uint16x8x2_t src) {
  uint16x4_t low[5], high[5];
  Prepare5_16(src, low, high);
  uint32x4x2_t sum;
  sum.val[0] = Sum5W_32(low);
  sum.val[1] = Sum5W_32(high);
  return sum;
}

void SumHorizontal(const uint16x4_t src[5], uint32x4_t* const row_sq3,
                   uint32x4_t* const row_sq5) {
  const uint32x4_t sum04 = vaddl_u16(src[0], src[4]);
  const uint32x4_t sum12 = vaddl_u16(src[1], src[2]);
  *row_sq3 = vaddw_u16(sum12, src[3]);
  *row_sq5 = vaddq_u32(sum04, *row_sq3);
}

void SumHorizontal(const uint8x8x2_t src, const uint16x8x2_t sq,
                   uint16x8_t* const row3, uint16x8_t* const row5,
                   uint32x4x2_t* const row_sq3, uint32x4x2_t* const row_sq5) {
  uint8x8_t s[5];
  Prepare5_8(src, s);
  const uint16x8_t sum04 = vaddl_u8(s[0], s[4]);
  const uint16x8_t sum12 = vaddl_u8(s[1], s[2]);
  *row3 = vaddw_u8(sum12, s[3]);
  *row5 = vaddq_u16(sum04, *row3);
  uint16x4_t low[5], high[5];
  Prepare5_16(sq, low, high);
  SumHorizontal(low, &row_sq3->val[0], &row_sq5->val[0]);
  SumHorizontal(high, &row_sq3->val[1], &row_sq5->val[1]);
}

inline uint16x8_t Sum343(const uint8x8x2_t src) {
  uint8x8_t s[3];
  Prepare3_8(src, s);
  const uint16x8_t sum = Sum3W_16(s);
  const uint16x8_t sum3 = Sum3_16(sum, sum, sum);
  return vaddw_u8(sum3, s[1]);
}

inline uint32x4_t Sum343W(const uint16x4_t src[3]) {
  const uint32x4_t sum = Sum3W_32(src);
  const uint32x4_t sum3 = Sum3_32(sum, sum, sum);
  return vaddw_u16(sum3, src[1]);
}

inline uint32x4x2_t Sum343W(const uint16x8x2_t src) {
  uint16x4_t low[3], high[3];
  uint32x4x2_t d;
  Prepare3_16(src, low, high);
  d.val[0] = Sum343W(low);
  d.val[1] = Sum343W(high);
  return d;
}

inline uint16x8_t Sum565(const uint8x8x2_t src) {
  uint8x8_t s[3];
  Prepare3_8(src, s);
  const uint16x8_t sum = Sum3W_16(s);
  const uint16x8_t sum4 = vshlq_n_u16(sum, 2);
  const uint16x8_t sum5 = vaddq_u16(sum4, sum);
  return vaddw_u8(sum5, s[1]);
}

inline uint32x4_t Sum565W(const uint16x4_t src[3]) {
  const uint32x4_t sum = Sum3W_32(src);
  const uint32x4_t sum4 = vshlq_n_u32(sum, 2);
  const uint32x4_t sum5 = vaddq_u32(sum4, sum);
  return vaddw_u16(sum5, src[1]);
}

inline uint32x4x2_t Sum565W(const uint16x8x2_t src) {
  uint16x4_t low[3], high[3];
  uint32x4x2_t d;
  Prepare3_16(src, low, high);
  d.val[0] = Sum565W(low);
  d.val[1] = Sum565W(high);
  return d;
}

inline void BoxSum(const uint8_t* src, const ptrdiff_t src_stride,
                   const int height, const ptrdiff_t sum_stride, uint16_t* sum3,
                   uint16_t* sum5, uint32_t* square_sum3,
                   uint32_t* square_sum5) {
  int y = height;
  do {
    uint8x8x2_t s;
    uint16x8x2_t sq;
    s.val[0] = vld1_u8(src);
    sq.val[0] = vmull_u8(s.val[0], s.val[0]);
    ptrdiff_t x = 0;
    do {
      uint16x8_t row3, row5;
      uint32x4x2_t row_sq3, row_sq5;
      s.val[1] = vld1_u8(src + x + 8);
      sq.val[1] = vmull_u8(s.val[1], s.val[1]);
      SumHorizontal(s, sq, &row3, &row5, &row_sq3, &row_sq5);
      vst1q_u16(sum3, row3);
      vst1q_u16(sum5, row5);
      vst1q_u32(square_sum3 + 0, row_sq3.val[0]);
      vst1q_u32(square_sum3 + 4, row_sq3.val[1]);
      vst1q_u32(square_sum5 + 0, row_sq5.val[0]);
      vst1q_u32(square_sum5 + 4, row_sq5.val[1]);
      s.val[0] = s.val[1];
      sq.val[0] = sq.val[1];
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
                   const int height, const ptrdiff_t sum_stride, uint16_t* sums,
                   uint32_t* square_sums) {
  static_assert(size == 3 || size == 5, "");
  int y = height;
  do {
    uint8x8x2_t s;
    uint16x8x2_t sq;
    s.val[0] = vld1_u8(src);
    sq.val[0] = vmull_u8(s.val[0], s.val[0]);
    ptrdiff_t x = 0;
    do {
      uint16x8_t row;
      uint32x4x2_t row_sq;
      s.val[1] = vld1_u8(src + x + 8);
      sq.val[1] = vmull_u8(s.val[1], s.val[1]);
      if (size == 3) {
        row = Sum3Horizontal(s);
        row_sq = Sum3WHorizontal(sq);
      } else {
        row = Sum5Horizontal(s);
        row_sq = Sum5WHorizontal(sq);
      }
      vst1q_u16(sums, row);
      vst1q_u32(square_sums + 0, row_sq.val[0]);
      vst1q_u32(square_sums + 4, row_sq.val[1]);
      s.val[0] = s.val[1];
      sq.val[0] = sq.val[1];
      sums += 8;
      square_sums += 8;
      x += 8;
    } while (x < sum_stride);
    src += src_stride;
  } while (--y != 0);
}

template <int n>
inline uint16x4_t CalculateMa(const uint16x4_t sum, const uint32x4_t sum_sq,
                              const uint32_t scale) {
  // a = |sum_sq|
  // d = |sum|
  // p = (a * n < d * d) ? 0 : a * n - d * d;
  const uint32x4_t dxd = vmull_u16(sum, sum);
  const uint32x4_t axn = vmulq_n_u32(sum_sq, n);
  // Ensure |p| does not underflow by using saturating subtraction.
  const uint32x4_t p = vqsubq_u32(axn, dxd);
  const uint32x4_t pxs = vmulq_n_u32(p, scale);
  // vrshrn_n_u32() (narrowing shift) can only shift by 16 and kSgrProjScaleBits
  // is 20.
  const uint32x4_t shifted = vrshrq_n_u32(pxs, kSgrProjScaleBits);
  return vmovn_u32(shifted);
}

template <int n>
inline void CalculateIntermediate(const uint16x8_t sum,
                                  const uint32x4x2_t sum_sq,
                                  const uint32_t scale, uint8x8_t* const ma,
                                  uint16x8_t* const b) {
  constexpr uint32_t one_over_n =
      ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;
  const uint16x4_t z0 = CalculateMa<n>(vget_low_u16(sum), sum_sq.val[0], scale);
  const uint16x4_t z1 =
      CalculateMa<n>(vget_high_u16(sum), sum_sq.val[1], scale);
  const uint16x8_t z01 = vcombine_u16(z0, z1);
  // Using vqmovn_u16() needs an extra sign extension instruction.
  const uint16x8_t z = vminq_u16(z01, vdupq_n_u16(255));
  // Using vgetq_lane_s16() can save the sign extension instruction.
  const uint8_t lookup[8] = {
      kSgrMaLookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 0)],
      kSgrMaLookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 1)],
      kSgrMaLookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 2)],
      kSgrMaLookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 3)],
      kSgrMaLookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 4)],
      kSgrMaLookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 5)],
      kSgrMaLookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 6)],
      kSgrMaLookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 7)]};
  *ma = vld1_u8(lookup);
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
  const uint16x8_t maq = vmovl_u8(*ma);
  const uint32x4_t m0 = vmull_u16(vget_low_u16(maq), vget_low_u16(sum));
  const uint32x4_t m1 = vmull_u16(vget_high_u16(maq), vget_high_u16(sum));
  const uint32x4_t m2 = vmulq_n_u32(m0, one_over_n);
  const uint32x4_t m3 = vmulq_n_u32(m1, one_over_n);
  const uint16x4_t b_lo = vrshrn_n_u32(m2, kSgrProjReciprocalBits);
  const uint16x4_t b_hi = vrshrn_n_u32(m3, kSgrProjReciprocalBits);
  *b = vcombine_u16(b_lo, b_hi);
}

inline void CalculateIntermediate5(const uint16x8_t s5[5],
                                   const uint32x4x2_t sq5[5],
                                   const uint32_t scale, uint8x8_t* const ma,
                                   uint16x8_t* const b) {
  const uint16x8_t sum = Sum5_16(s5);
  const uint32x4x2_t sum_sq = Sum5_32(sq5);
  CalculateIntermediate<25>(sum, sum_sq, scale, ma, b);
}

inline void CalculateIntermediate3(const uint16x8_t s3[3],
                                   const uint32x4x2_t sq3[3],
                                   const uint32_t scale, uint8x8_t* const ma,
                                   uint16x8_t* const b) {
  const uint16x8_t sum = Sum3_16(s3);
  const uint32x4x2_t sum_sq = Sum3_32(sq3);
  CalculateIntermediate<9>(sum, sum_sq, scale, ma, b);
}

inline void Store343_444(const uint8x8x2_t ma3, const uint16x8x2_t b3,
                         const ptrdiff_t x, uint16x8_t* const sum_ma343,
                         uint16x8_t* const sum_ma444,
                         uint32x4x2_t* const sum_b343,
                         uint32x4x2_t* const sum_b444, uint16_t* const ma343,
                         uint16_t* const ma444, uint32_t* const b343,
                         uint32_t* const b444) {
  uint8x8_t s[3];
  Prepare3_8(ma3, s);
  const uint16x8_t sum_ma111 = Sum3W_16(s);
  *sum_ma444 = vshlq_n_u16(sum_ma111, 2);
  const uint16x8_t sum333 = vsubq_u16(*sum_ma444, sum_ma111);
  *sum_ma343 = vaddw_u8(sum333, s[1]);
  uint16x4_t low[3], high[3];
  uint32x4x2_t sum_b111;
  Prepare3_16(b3, low, high);
  sum_b111.val[0] = Sum3W_32(low);
  sum_b111.val[1] = Sum3W_32(high);
  sum_b444->val[0] = vshlq_n_u32(sum_b111.val[0], 2);
  sum_b444->val[1] = vshlq_n_u32(sum_b111.val[1], 2);
  sum_b343->val[0] = vsubq_u32(sum_b444->val[0], sum_b111.val[0]);
  sum_b343->val[1] = vsubq_u32(sum_b444->val[1], sum_b111.val[1]);
  sum_b343->val[0] = vaddw_u16(sum_b343->val[0], low[1]);
  sum_b343->val[1] = vaddw_u16(sum_b343->val[1], high[1]);
  vst1q_u16(ma343 + x, *sum_ma343);
  vst1q_u16(ma444 + x, *sum_ma444);
  vst1q_u32(b343 + x + 0, sum_b343->val[0]);
  vst1q_u32(b343 + x + 4, sum_b343->val[1]);
  vst1q_u32(b444 + x + 0, sum_b444->val[0]);
  vst1q_u32(b444 + x + 4, sum_b444->val[1]);
}

inline void Store343_444(const uint8x8x2_t ma3, const uint16x8x2_t b3,
                         const ptrdiff_t x, uint16x8_t* const sum_ma343,
                         uint32x4x2_t* const sum_b343, uint16_t* const ma343,
                         uint16_t* const ma444, uint32_t* const b343,
                         uint32_t* const b444) {
  uint16x8_t sum_ma444;
  uint32x4x2_t sum_b444;
  Store343_444(ma3, b3, x, sum_ma343, &sum_ma444, sum_b343, &sum_b444, ma343,
               ma444, b343, b444);
}

inline void Store343_444(const uint8x8x2_t ma3, const uint16x8x2_t b3,
                         const ptrdiff_t x, uint16_t* const ma343,
                         uint16_t* const ma444, uint32_t* const b343,
                         uint32_t* const b444) {
  uint16x8_t sum_ma343;
  uint32x4x2_t sum_b343;
  Store343_444(ma3, b3, x, &sum_ma343, &sum_b343, ma343, ma444, b343, b444);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess5(
    const uint8_t* const src0, const uint8_t* const src1, const ptrdiff_t x,
    const uint32_t scale, uint16_t* const sum5[5],
    uint32_t* const square_sum5[5], uint8x8x2_t s[2], uint16x8x2_t sq[2],
    uint8x8_t* const ma, uint16x8_t* const b) {
  uint16x8_t s5[5];
  uint32x4x2_t sq5[5];
  s[0].val[1] = vld1_u8(src0 + x + 8);
  s[1].val[1] = vld1_u8(src1 + x + 8);
  sq[0].val[1] = vmull_u8(s[0].val[1], s[0].val[1]);
  sq[1].val[1] = vmull_u8(s[1].val[1], s[1].val[1]);
  s5[3] = Sum5Horizontal(s[0]);
  s5[4] = Sum5Horizontal(s[1]);
  sq5[3] = Sum5WHorizontal(sq[0]);
  sq5[4] = Sum5WHorizontal(sq[1]);
  vst1q_u16(sum5[3] + x, s5[3]);
  vst1q_u16(sum5[4] + x, s5[4]);
  vst1q_u32(square_sum5[3] + x + 0, sq5[3].val[0]);
  vst1q_u32(square_sum5[3] + x + 4, sq5[3].val[1]);
  vst1q_u32(square_sum5[4] + x + 0, sq5[4].val[0]);
  vst1q_u32(square_sum5[4] + x + 4, sq5[4].val[1]);
  s5[0] = vld1q_u16(sum5[0] + x);
  s5[1] = vld1q_u16(sum5[1] + x);
  s5[2] = vld1q_u16(sum5[2] + x);
  sq5[0].val[0] = vld1q_u32(square_sum5[0] + x + 0);
  sq5[0].val[1] = vld1q_u32(square_sum5[0] + x + 4);
  sq5[1].val[0] = vld1q_u32(square_sum5[1] + x + 0);
  sq5[1].val[1] = vld1q_u32(square_sum5[1] + x + 4);
  sq5[2].val[0] = vld1q_u32(square_sum5[2] + x + 0);
  sq5[2].val[1] = vld1q_u32(square_sum5[2] + x + 4);
  CalculateIntermediate5(s5, sq5, scale, ma, b);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess5LastRow(
    const uint8_t* const src, const ptrdiff_t x, const uint32_t scale,
    const uint16_t* const sum5[5], const uint32_t* const square_sum5[5],
    uint8x8x2_t* const s, uint16x8x2_t* const sq, uint8x8_t* const ma,
    uint16x8_t* const b) {
  uint16x8_t s5[5];
  uint32x4x2_t sq5[5];
  s->val[1] = vld1_u8(src + x + 8);
  sq->val[1] = vmull_u8(s->val[1], s->val[1]);
  s5[3] = s5[4] = Sum5Horizontal(*s);
  sq5[3] = sq5[4] = Sum5WHorizontal(*sq);
  s5[0] = vld1q_u16(sum5[0] + x);
  s5[1] = vld1q_u16(sum5[1] + x);
  s5[2] = vld1q_u16(sum5[2] + x);
  sq5[0].val[0] = vld1q_u32(square_sum5[0] + x + 0);
  sq5[0].val[1] = vld1q_u32(square_sum5[0] + x + 4);
  sq5[1].val[0] = vld1q_u32(square_sum5[1] + x + 0);
  sq5[1].val[1] = vld1q_u32(square_sum5[1] + x + 4);
  sq5[2].val[0] = vld1q_u32(square_sum5[2] + x + 0);
  sq5[2].val[1] = vld1q_u32(square_sum5[2] + x + 4);
  CalculateIntermediate5(s5, sq5, scale, ma, b);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess3(
    const uint8_t* const src, const ptrdiff_t x, const uint32_t scale,
    uint16_t* const sum3[3], uint32_t* const square_sum3[3],
    uint8x8x2_t* const s, uint16x8x2_t* const sq, uint8x8_t* const ma,
    uint16x8_t* const b) {
  uint16x8_t s3[3];
  uint32x4x2_t sq3[3];
  s->val[1] = vld1_u8(src + x + 8);
  sq->val[1] = vmull_u8(s->val[1], s->val[1]);
  s3[2] = Sum3Horizontal(*s);
  sq3[2] = Sum3WHorizontal(*sq);
  vst1q_u16(sum3[2] + x, s3[2]);
  vst1q_u32(square_sum3[2] + x + 0, sq3[2].val[0]);
  vst1q_u32(square_sum3[2] + x + 4, sq3[2].val[1]);
  s3[0] = vld1q_u16(sum3[0] + x);
  s3[1] = vld1q_u16(sum3[1] + x);
  sq3[0].val[0] = vld1q_u32(square_sum3[0] + x + 0);
  sq3[0].val[1] = vld1q_u32(square_sum3[0] + x + 4);
  sq3[1].val[0] = vld1q_u32(square_sum3[1] + x + 0);
  sq3[1].val[1] = vld1q_u32(square_sum3[1] + x + 4);
  CalculateIntermediate3(s3, sq3, scale, ma, b);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess(
    const uint8_t* const src0, const uint8_t* const src1, const ptrdiff_t x,
    const uint16_t scales[2], uint16_t* const sum3[4], uint16_t* const sum5[5],
    uint32_t* const square_sum3[4], uint32_t* const square_sum5[5],
    uint8x8x2_t s[2], uint16x8x2_t sq[2], uint8x8_t* const ma3_0,
    uint8x8_t* const ma3_1, uint16x8_t* const b3_0, uint16x8_t* const b3_1,
    uint8x8_t* const ma5, uint16x8_t* const b5) {
  uint16x8_t s3[4], s5[5];
  uint32x4x2_t sq3[4], sq5[5];
  s[0].val[1] = vld1_u8(src0 + x + 8);
  s[1].val[1] = vld1_u8(src1 + x + 8);
  sq[0].val[1] = vmull_u8(s[0].val[1], s[0].val[1]);
  sq[1].val[1] = vmull_u8(s[1].val[1], s[1].val[1]);
  SumHorizontal(s[0], sq[0], &s3[2], &s5[3], &sq3[2], &sq5[3]);
  SumHorizontal(s[1], sq[1], &s3[3], &s5[4], &sq3[3], &sq5[4]);
  vst1q_u16(sum3[2] + x, s3[2]);
  vst1q_u16(sum3[3] + x, s3[3]);
  vst1q_u32(square_sum3[2] + x + 0, sq3[2].val[0]);
  vst1q_u32(square_sum3[2] + x + 4, sq3[2].val[1]);
  vst1q_u32(square_sum3[3] + x + 0, sq3[3].val[0]);
  vst1q_u32(square_sum3[3] + x + 4, sq3[3].val[1]);
  vst1q_u16(sum5[3] + x, s5[3]);
  vst1q_u16(sum5[4] + x, s5[4]);
  vst1q_u32(square_sum5[3] + x + 0, sq5[3].val[0]);
  vst1q_u32(square_sum5[3] + x + 4, sq5[3].val[1]);
  vst1q_u32(square_sum5[4] + x + 0, sq5[4].val[0]);
  vst1q_u32(square_sum5[4] + x + 4, sq5[4].val[1]);
  s3[0] = vld1q_u16(sum3[0] + x);
  s3[1] = vld1q_u16(sum3[1] + x);
  sq3[0].val[0] = vld1q_u32(square_sum3[0] + x + 0);
  sq3[0].val[1] = vld1q_u32(square_sum3[0] + x + 4);
  sq3[1].val[0] = vld1q_u32(square_sum3[1] + x + 0);
  sq3[1].val[1] = vld1q_u32(square_sum3[1] + x + 4);
  s5[0] = vld1q_u16(sum5[0] + x);
  s5[1] = vld1q_u16(sum5[1] + x);
  s5[2] = vld1q_u16(sum5[2] + x);
  sq5[0].val[0] = vld1q_u32(square_sum5[0] + x + 0);
  sq5[0].val[1] = vld1q_u32(square_sum5[0] + x + 4);
  sq5[1].val[0] = vld1q_u32(square_sum5[1] + x + 0);
  sq5[1].val[1] = vld1q_u32(square_sum5[1] + x + 4);
  sq5[2].val[0] = vld1q_u32(square_sum5[2] + x + 0);
  sq5[2].val[1] = vld1q_u32(square_sum5[2] + x + 4);
  CalculateIntermediate3(s3, sq3, scales[1], ma3_0, b3_0);
  CalculateIntermediate3(s3 + 1, sq3 + 1, scales[1], ma3_1, b3_1);
  CalculateIntermediate5(s5, sq5, scales[0], ma5, b5);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcessLastRow(
    const uint8_t* const src, const ptrdiff_t x, const uint16_t scales[2],
    const uint16_t* const sum3[4], const uint16_t* const sum5[5],
    const uint32_t* const square_sum3[4], const uint32_t* const square_sum5[5],
    uint8x8x2_t* const s, uint16x8x2_t* const sq, uint8x8_t* const ma3,
    uint8x8_t* const ma5, uint16x8_t* const b3, uint16x8_t* const b5) {
  uint16x8_t s3[3], s5[5];
  uint32x4x2_t sq3[3], sq5[5];
  s->val[1] = vld1_u8(src + x + 8);
  sq->val[1] = vmull_u8(s->val[1], s->val[1]);
  SumHorizontal(*s, *sq, &s3[2], &s5[3], &sq3[2], &sq5[3]);
  s5[0] = vld1q_u16(sum5[0] + x);
  s5[1] = vld1q_u16(sum5[1] + x);
  s5[2] = vld1q_u16(sum5[2] + x);
  s5[4] = s5[3];
  sq5[0].val[0] = vld1q_u32(square_sum5[0] + x + 0);
  sq5[0].val[1] = vld1q_u32(square_sum5[0] + x + 4);
  sq5[1].val[0] = vld1q_u32(square_sum5[1] + x + 0);
  sq5[1].val[1] = vld1q_u32(square_sum5[1] + x + 4);
  sq5[2].val[0] = vld1q_u32(square_sum5[2] + x + 0);
  sq5[2].val[1] = vld1q_u32(square_sum5[2] + x + 4);
  sq5[4] = sq5[3];
  CalculateIntermediate5(s5, sq5, scales[0], ma5, b5);
  s3[0] = vld1q_u16(sum3[0] + x);
  s3[1] = vld1q_u16(sum3[1] + x);
  sq3[0].val[0] = vld1q_u32(square_sum3[0] + x + 0);
  sq3[0].val[1] = vld1q_u32(square_sum3[0] + x + 4);
  sq3[1].val[0] = vld1q_u32(square_sum3[1] + x + 0);
  sq3[1].val[1] = vld1q_u32(square_sum3[1] + x + 4);
  CalculateIntermediate3(s3, sq3, scales[1], ma3, b3);
}

inline void BoxSumFilterPreProcess5(const uint8_t* const src0,
                                    const uint8_t* const src1, const int width,
                                    const uint32_t scale,
                                    uint16_t* const sum5[5],
                                    uint32_t* const square_sum5[5],
                                    uint16_t* ma565, uint32_t* b565) {
  uint8x8x2_t s[2], mas;
  uint16x8x2_t sq[2], bs;
  s[0].val[0] = vld1_u8(src0);
  s[1].val[0] = vld1_u8(src1);
  sq[0].val[0] = vmull_u8(s[0].val[0], s[0].val[0]);
  sq[1].val[0] = vmull_u8(s[1].val[0], s[1].val[0]);
  BoxFilterPreProcess5(src0, src1, 0, scale, sum5, square_sum5, s, sq,
                       &mas.val[0], &bs.val[0]);

  int x = 0;
  do {
    s[0].val[0] = s[0].val[1];
    s[1].val[0] = s[1].val[1];
    sq[0].val[0] = sq[0].val[1];
    sq[1].val[0] = sq[1].val[1];
    BoxFilterPreProcess5(src0, src1, x + 8, scale, sum5, square_sum5, s, sq,
                         &mas.val[1], &bs.val[1]);
    const uint16x8_t ma = Sum565(mas);
    const uint32x4x2_t b = Sum565W(bs);
    vst1q_u16(ma565, ma);
    vst1q_u32(b565 + 0, b.val[0]);
    vst1q_u32(b565 + 4, b.val[1]);
    mas.val[0] = mas.val[1];
    bs.val[0] = bs.val[1];
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
  uint8x8x2_t s, mas;
  uint16x8x2_t sq, bs;
  s.val[0] = vld1_u8(src);
  sq.val[0] = vmull_u8(s.val[0], s.val[0]);
  BoxFilterPreProcess3(src, 0, scale, sum3, square_sum3, &s, &sq, &mas.val[0],
                       &bs.val[0]);

  int x = 0;
  do {
    s.val[0] = s.val[1];
    sq.val[0] = sq.val[1];
    BoxFilterPreProcess3(src, x + 8, scale, sum3, square_sum3, &s, &sq,
                         &mas.val[1], &bs.val[1]);
    if (calculate444) {
      Store343_444(mas, bs, 0, ma343, ma444, b343, b444);
      ma444 += 8;
      b444 += 8;
    } else {
      const uint16x8_t ma = Sum343(mas);
      const uint32x4x2_t b = Sum343W(bs);
      vst1q_u16(ma343, ma);
      vst1q_u32(b343 + 0, b.val[0]);
      vst1q_u32(b343 + 4, b.val[1]);
    }
    mas.val[0] = mas.val[1];
    bs.val[0] = bs.val[1];
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
  uint8x8x2_t s[2];
  uint8x8x2_t ma3[2], ma5;
  uint16x8x2_t sq[2], b3[2], b5;
  s[0].val[0] = vld1_u8(src0);
  s[1].val[0] = vld1_u8(src1);
  sq[0].val[0] = vmull_u8(s[0].val[0], s[0].val[0]);
  sq[1].val[0] = vmull_u8(s[1].val[0], s[1].val[0]);
  BoxFilterPreProcess(src0, src1, 0, scales, sum3, sum5, square_sum3,
                      square_sum5, s, sq, &ma3[0].val[0], &ma3[1].val[0],
                      &b3[0].val[0], &b3[1].val[0], &ma5.val[0], &b5.val[0]);

  int x = 0;
  do {
    s[0].val[0] = s[0].val[1];
    s[1].val[0] = s[1].val[1];
    sq[0].val[0] = sq[0].val[1];
    sq[1].val[0] = sq[1].val[1];
    BoxFilterPreProcess(src0, src1, x + 8, scales, sum3, sum5, square_sum3,
                        square_sum5, s, sq, &ma3[0].val[1], &ma3[1].val[1],
                        &b3[0].val[1], &b3[1].val[1], &ma5.val[1], &b5.val[1]);
    uint16x8_t ma = Sum343(ma3[0]);
    uint32x4x2_t b = Sum343W(b3[0]);
    vst1q_u16(ma343[0] + x, ma);
    vst1q_u32(b343[0] + x, b.val[0]);
    vst1q_u32(b343[0] + x + 4, b.val[1]);
    Store343_444(ma3[1], b3[1], x, ma343[1], ma444[0], b343[1], b444[0]);
    ma = Sum565(ma5);
    b = Sum565W(b5);
    vst1q_u16(ma565, ma);
    vst1q_u32(b565 + 0, b.val[0]);
    vst1q_u32(b565 + 4, b.val[1]);
    ma3[0].val[0] = ma3[0].val[1];
    ma3[1].val[0] = ma3[1].val[1];
    b3[0].val[0] = b3[0].val[1];
    b3[1].val[0] = b3[1].val[1];
    ma5.val[0] = ma5.val[1];
    b5.val[0] = b5.val[1];
    ma565 += 8;
    b565 += 8;
    x += 8;
  } while (x < width);
}

template <int shift>
inline int16x4_t FilterOutput(const uint16x4_t src, const uint16x4_t ma,
                              const uint32x4_t b) {
  // ma: 255 * 32 = 8160 (13 bits)
  // b: 65088 * 32 = 2082816 (21 bits)
  // v: b - ma * 255 (22 bits)
  const int32x4_t v = vreinterpretq_s32_u32(vmlsl_u16(b, ma, src));
  // kSgrProjSgrBits = 8
  // kSgrProjRestoreBits = 4
  // shift = 4 or 5
  // v >> 8 or 9 (13 bits)
  return vrshrn_n_s32(v, kSgrProjSgrBits + shift - kSgrProjRestoreBits);
}

template <int shift>
inline int16x8_t CalculateFilteredOutput(const uint8x8_t src,
                                         const uint16x8_t ma,
                                         const uint32x4x2_t b) {
  const uint16x8_t src_u16 = vmovl_u8(src);
  const int16x4_t dst_lo =
      FilterOutput<shift>(vget_low_u16(src_u16), vget_low_u16(ma), b.val[0]);
  const int16x4_t dst_hi =
      FilterOutput<shift>(vget_high_u16(src_u16), vget_high_u16(ma), b.val[1]);
  return vcombine_s16(dst_lo, dst_hi);  // 13 bits
}

inline int16x8_t CalculateFilteredOutputPass1(const uint8x8_t s,
                                              uint16x8_t ma[2],
                                              uint32x4x2_t b[2]) {
  const uint16x8_t ma_sum = vaddq_u16(ma[0], ma[1]);
  uint32x4x2_t b_sum;
  b_sum.val[0] = vaddq_u32(b[0].val[0], b[1].val[0]);
  b_sum.val[1] = vaddq_u32(b[0].val[1], b[1].val[1]);
  return CalculateFilteredOutput<5>(s, ma_sum, b_sum);
}

inline int16x8_t CalculateFilteredOutputPass2(const uint8x8_t s,
                                              uint16x8_t ma[3],
                                              uint32x4x2_t b[3]) {
  const uint16x8_t ma_sum = Sum3_16(ma);
  const uint32x4x2_t b_sum = Sum3_32(b);
  return CalculateFilteredOutput<5>(s, ma_sum, b_sum);
}

inline void SelfGuidedFinal(const uint8x8_t src, const int32x4_t v[2],
                            uint8_t* const dst) {
  const int16x4_t v_lo =
      vrshrn_n_s32(v[0], kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const int16x4_t v_hi =
      vrshrn_n_s32(v[1], kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const int16x8_t vv = vcombine_s16(v_lo, v_hi);
  const int16x8_t s = ZeroExtend(src);
  const int16x8_t d = vaddq_s16(s, vv);
  vst1_u8(dst, vqmovun_s16(d));
}

inline void SelfGuidedDoubleMultiplier(const uint8x8_t src,
                                       const int16x8_t filter[2], const int w0,
                                       const int w2, uint8_t* const dst) {
  int32x4_t v[2];
  v[0] = vmull_n_s16(vget_low_s16(filter[0]), w0);
  v[1] = vmull_n_s16(vget_high_s16(filter[0]), w0);
  v[0] = vmlal_n_s16(v[0], vget_low_s16(filter[1]), w2);
  v[1] = vmlal_n_s16(v[1], vget_high_s16(filter[1]), w2);
  SelfGuidedFinal(src, v, dst);
}

inline void SelfGuidedSingleMultiplier(const uint8x8_t src,
                                       const int16x8_t filter, const int w0,
                                       uint8_t* const dst) {
  // weight: -96 to 96 (Sgrproj_Xqd_Min/Max)
  int32x4_t v[2];
  v[0] = vmull_n_s16(vget_low_s16(filter), w0);
  v[1] = vmull_n_s16(vget_high_s16(filter), w0);
  SelfGuidedFinal(src, v, dst);
}

LIBGAV1_ALWAYS_INLINE void BoxFilterPass1(
    const uint8_t* const src, const uint8_t* const src0,
    const uint8_t* const src1, const ptrdiff_t stride, uint16_t* const sum5[5],
    uint32_t* const square_sum5[5], const int width, const uint32_t scale,
    const int16_t w0, uint16_t* const ma565[2], uint32_t* const b565[2],
    uint8_t* const dst) {
  uint8x8x2_t s[2], mas;
  uint16x8x2_t sq[2], bs;
  s[0].val[0] = vld1_u8(src0);
  s[1].val[0] = vld1_u8(src1);
  sq[0].val[0] = vmull_u8(s[0].val[0], s[0].val[0]);
  sq[1].val[0] = vmull_u8(s[1].val[0], s[1].val[0]);
  BoxFilterPreProcess5(src0, src1, 0, scale, sum5, square_sum5, s, sq,
                       &mas.val[0], &bs.val[0]);

  int x = 0;
  do {
    s[0].val[0] = s[0].val[1];
    s[1].val[0] = s[1].val[1];
    sq[0].val[0] = sq[0].val[1];
    sq[1].val[0] = sq[1].val[1];
    BoxFilterPreProcess5(src0, src1, x + 8, scale, sum5, square_sum5, s, sq,
                         &mas.val[1], &bs.val[1]);
    uint16x8_t ma[2];
    uint32x4x2_t b[2];
    ma[1] = Sum565(mas);
    b[1] = Sum565W(bs);
    vst1q_u16(ma565[1] + x, ma[1]);
    vst1q_u32(b565[1] + x + 0, b[1].val[0]);
    vst1q_u32(b565[1] + x + 4, b[1].val[1]);
    const uint8x8_t sr0 = vld1_u8(src + x);
    const uint8x8_t sr1 = vld1_u8(src + stride + x);
    int16x8_t p0, p1;
    ma[0] = vld1q_u16(ma565[0] + x);
    b[0].val[0] = vld1q_u32(b565[0] + x + 0);
    b[0].val[1] = vld1q_u32(b565[0] + x + 4);
    p0 = CalculateFilteredOutputPass1(sr0, ma, b);
    p1 = CalculateFilteredOutput<4>(sr1, ma[1], b[1]);
    SelfGuidedSingleMultiplier(sr0, p0, w0, dst + x);
    SelfGuidedSingleMultiplier(sr1, p1, w0, dst + stride + x);
    mas.val[0] = mas.val[1];
    bs.val[0] = bs.val[1];
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
  uint8x8x2_t s, mas;
  uint16x8x2_t sq, bs;
  s.val[0] = vld1_u8(src0);
  sq.val[0] = vmull_u8(s.val[0], s.val[0]);
  BoxFilterPreProcess5LastRow(src0, 0, scale, sum5, square_sum5, &s, &sq,
                              &mas.val[0], &bs.val[0]);

  int x = 0;
  do {
    s.val[0] = s.val[1];
    sq.val[0] = sq.val[1];
    BoxFilterPreProcess5LastRow(src0, x + 8, scale, sum5, square_sum5, &s, &sq,
                                &mas.val[1], &bs.val[1]);
    uint16x8_t ma[2];
    uint32x4x2_t b[2];
    ma[1] = Sum565(mas);
    b[1] = Sum565W(bs);
    mas.val[0] = mas.val[1];
    bs.val[0] = bs.val[1];
    ma[0] = vld1q_u16(ma565);
    b[0].val[0] = vld1q_u32(b565 + 0);
    b[0].val[1] = vld1q_u32(b565 + 4);
    const uint8x8_t sr = vld1_u8(src + x);
    const int16x8_t p = CalculateFilteredOutputPass1(sr, ma, b);
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
  uint8x8x2_t s, mas;
  uint16x8x2_t sq, bs;
  s.val[0] = vld1_u8(src0);
  sq.val[0] = vmull_u8(s.val[0], s.val[0]);
  BoxFilterPreProcess3(src0, 0, scale, sum3, square_sum3, &s, &sq, &mas.val[0],
                       &bs.val[0]);

  int x = 0;
  do {
    s.val[0] = s.val[1];
    sq.val[0] = sq.val[1];
    BoxFilterPreProcess3(src0, x + 8, scale, sum3, square_sum3, &s, &sq,
                         &mas.val[1], &bs.val[1]);
    uint16x8_t ma[3];
    uint32x4x2_t b[3];
    Store343_444(mas, bs, x, &ma[2], &b[2], ma343[2], ma444[1], b343[2],
                 b444[1]);
    const uint8x8_t sr = vld1_u8(src + x);
    ma[0] = vld1q_u16(ma343[0] + x);
    ma[1] = vld1q_u16(ma444[0] + x);
    b[0].val[0] = vld1q_u32(b343[0] + x + 0);
    b[0].val[1] = vld1q_u32(b343[0] + x + 4);
    b[1].val[0] = vld1q_u32(b444[0] + x + 0);
    b[1].val[1] = vld1q_u32(b444[0] + x + 4);
    const int16x8_t p = CalculateFilteredOutputPass2(sr, ma, b);
    SelfGuidedSingleMultiplier(sr, p, w0, dst + x);
    mas.val[0] = mas.val[1];
    bs.val[0] = bs.val[1];
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
  uint8x8x2_t s[2], ma3[2], ma5;
  uint16x8x2_t sq[2], b3[2], b5;
  s[0].val[0] = vld1_u8(src0);
  s[1].val[0] = vld1_u8(src1);
  sq[0].val[0] = vmull_u8(s[0].val[0], s[0].val[0]);
  sq[1].val[0] = vmull_u8(s[1].val[0], s[1].val[0]);
  BoxFilterPreProcess(src0, src1, 0, scales, sum3, sum5, square_sum3,
                      square_sum5, s, sq, &ma3[0].val[0], &ma3[1].val[0],
                      &b3[0].val[0], &b3[1].val[0], &ma5.val[0], &b5.val[0]);

  int x = 0;
  do {
    s[0].val[0] = s[0].val[1];
    s[1].val[0] = s[1].val[1];
    sq[0].val[0] = sq[0].val[1];
    sq[1].val[0] = sq[1].val[1];
    BoxFilterPreProcess(src0, src1, x + 8, scales, sum3, sum5, square_sum3,
                        square_sum5, s, sq, &ma3[0].val[1], &ma3[1].val[1],
                        &b3[0].val[1], &b3[1].val[1], &ma5.val[1], &b5.val[1]);
    uint16x8_t ma[3][3];
    uint32x4x2_t b[3][3];
    Store343_444(ma3[0], b3[0], x, &ma[1][2], &ma[2][1], &b[1][2], &b[2][1],
                 ma343[2], ma444[1], b343[2], b444[1]);
    Store343_444(ma3[1], b3[1], x, &ma[2][2], &b[2][2], ma343[3], ma444[2],
                 b343[3], b444[2]);
    ma[0][1] = Sum565(ma5);
    b[0][1] = Sum565W(b5);
    vst1q_u16(ma565[1] + x, ma[0][1]);
    vst1q_u32(b565[1] + x, b[0][1].val[0]);
    vst1q_u32(b565[1] + x + 4, b[0][1].val[1]);
    ma3[0].val[0] = ma3[0].val[1];
    ma3[1].val[0] = ma3[1].val[1];
    b3[0].val[0] = b3[0].val[1];
    b3[1].val[0] = b3[1].val[1];
    ma5.val[0] = ma5.val[1];
    b5.val[0] = b5.val[1];
    int16x8_t p[2][2];
    const uint8x8_t sr0 = vld1_u8(src + x);
    const uint8x8_t sr1 = vld1_u8(src + stride + x);
    ma[0][0] = vld1q_u16(ma565[0] + x);
    b[0][0].val[0] = vld1q_u32(b565[0] + x);
    b[0][0].val[1] = vld1q_u32(b565[0] + x + 4);
    p[0][0] = CalculateFilteredOutputPass1(sr0, ma[0], b[0]);
    p[1][0] = CalculateFilteredOutput<4>(sr1, ma[0][1], b[0][1]);
    ma[1][0] = vld1q_u16(ma343[0] + x);
    ma[1][1] = vld1q_u16(ma444[0] + x);
    b[1][0].val[0] = vld1q_u32(b343[0] + x);
    b[1][0].val[1] = vld1q_u32(b343[0] + x + 4);
    b[1][1].val[0] = vld1q_u32(b444[0] + x);
    b[1][1].val[1] = vld1q_u32(b444[0] + x + 4);
    p[0][1] = CalculateFilteredOutputPass2(sr0, ma[1], b[1]);
    ma[2][0] = vld1q_u16(ma343[1] + x);
    b[2][0].val[0] = vld1q_u32(b343[1] + x);
    b[2][0].val[1] = vld1q_u32(b343[1] + x + 4);
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
  uint8x8x2_t s, ma3, ma5;
  uint16x8x2_t sq, b3, b5;
  uint16x8_t ma[3];
  uint32x4x2_t b[3];
  s.val[0] = vld1_u8(src0);
  sq.val[0] = vmull_u8(s.val[0], s.val[0]);
  BoxFilterPreProcessLastRow(src0, 0, scales, sum3, sum5, square_sum3,
                             square_sum5, &s, &sq, &ma3.val[0], &ma5.val[0],
                             &b3.val[0], &b5.val[0]);

  int x = 0;
  do {
    s.val[0] = s.val[1];
    sq.val[0] = sq.val[1];
    BoxFilterPreProcessLastRow(src0, x + 8, scales, sum3, sum5, square_sum3,
                               square_sum5, &s, &sq, &ma3.val[1], &ma5.val[1],
                               &b3.val[1], &b5.val[1]);
    ma[1] = Sum565(ma5);
    b[1] = Sum565W(b5);
    ma5.val[0] = ma5.val[1];
    b5.val[0] = b5.val[1];
    ma[2] = Sum343(ma3);
    b[2] = Sum343W(b3);
    ma3.val[0] = ma3.val[1];
    b3.val[0] = b3.val[1];
    const uint8x8_t sr = vld1_u8(src + x);
    int16x8_t p[2];
    ma[0] = vld1q_u16(ma565[0] + x);
    b[0].val[0] = vld1q_u32(b565[0] + x + 0);
    b[0].val[1] = vld1q_u32(b565[0] + x + 4);
    p[0] = CalculateFilteredOutputPass1(sr, ma, b);
    ma[0] = vld1q_u16(ma343[0] + x);
    ma[1] = vld1q_u16(ma444[0] + x);
    b[0].val[0] = vld1q_u32(b343[0] + x + 0);
    b[0].val[1] = vld1q_u32(b343[0] + x + 4);
    b[1].val[0] = vld1q_u32(b444[0] + x + 0);
    b[1].val[1] = vld1q_u32(b444[0] + x + 4);
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
  BoxSum(top_border, stride, 2, sum_stride, sum3[0], sum5[1], square_sum3[0],
         square_sum5[1]);
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
  BoxSum<5>(top_border, stride, 2, sum_stride, sum5[1], square_sum5[1]);
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
    if (height > 1) {
      src += 2 * stride;
      dst += 2 * stride;
      std::swap(ma565[0], ma565[1]);
      std::swap(b565[0], b565[1]);
      Circulate5PointersBy2<uint16_t>(sum5);
      Circulate5PointersBy2<uint32_t>(square_sum5);
    }
    BoxFilterPass1LastRow(src + 3, bottom_border + stride, width, scale, w0,
                          sum5, square_sum5, ma565[0], b565[0], dst);
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
  BoxSum<3>(top_border, stride, 2, sum_stride, sum3[0], square_sum3[0]);
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

  src += 2;
  int y = std::min(height, 2);
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
void SelfGuidedFilter_NEON(
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
  dsp->loop_restorations[0] = WienerFilter_NEON;
  dsp->loop_restorations[1] = SelfGuidedFilter_NEON;
}

}  // namespace
}  // namespace low_bitdepth

void LoopRestorationInit_NEON() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else  // !LIBGAV1_ENABLE_NEON
namespace libgav1 {
namespace dsp {

void LoopRestorationInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON
