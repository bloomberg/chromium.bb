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

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/dsp/arm/common_neon.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

// Wiener

// Must make a local copy of coefficients to help compiler know that they have
// no overlap with other buffers. Using 'const' keyword is not enough. Actually
// compiler doesn't make a copy, since there is enough registers in this case.
inline void PopulateWienerCoefficients(
    const RestorationUnitInfo& restoration_info, const int direction,
    int16_t filter[4]) {
  // In order to keep the horizontal pass intermediate values within 16 bits we
  // initialize |filter[3]| to 0 instead of 128.
  int filter_3;
  if (direction == WienerInfo::kHorizontal) {
    filter_3 = 0;
  } else {
    assert(direction == WienerInfo::kVertical);
    filter_3 = 128;
  }

  for (int i = 0; i < 3; ++i) {
    const int16_t coeff = restoration_info.wiener_info.filter[direction][i];
    filter[i] = coeff;
    filter_3 -= coeff * 2;
  }
  filter[3] = filter_3;
}

inline int CountZeroCoefficients(const int16_t filter[2][kSubPixelTaps]) {
  int number_zero_coefficients = 0;
  if ((filter[WienerInfo::kHorizontal][0] | filter[WienerInfo::kVertical][0]) ==
      0) {
    number_zero_coefficients++;
    if ((filter[WienerInfo::kHorizontal][1] |
         filter[WienerInfo::kVertical][1]) == 0) {
      number_zero_coefficients++;
      if ((filter[WienerInfo::kHorizontal][2] |
           filter[WienerInfo::kVertical][2]) == 0) {
        number_zero_coefficients++;
      }
    }
  }
  return number_zero_coefficients;
}

inline void LoadHorizontal4Tap3(const uint8_t* source, uint8x8_t s[3]) {
  s[0] = vld1_u8(source);
  // Faster than using vshr_n_u64().
  s[1] = vext_u8(s[0], s[0], 1);
  s[2] = vext_u8(s[0], s[0], 2);
}

inline void LoadHorizontal4Tap5(const uint8_t* source, uint8x8_t s[5]) {
  s[0] = vld1_u8(source);
  // Faster than using vshr_n_u64().
  s[1] = vext_u8(s[0], s[0], 1);
  s[2] = vext_u8(s[0], s[0], 2);
  s[3] = vext_u8(s[0], s[0], 3);
  s[4] = vext_u8(s[0], s[0], 4);
}

inline void LoadHorizontal8Tap3(const uint8_t* source, uint8x8_t s[3]) {
  const uint8x16_t r = vld1q_u8(source);
  s[0] = vget_low_u8(r);
  s[1] = vext_u8(s[0], vget_high_u8(r), 1);
  s[2] = vext_u8(s[0], vget_high_u8(r), 2);
}

inline void LoadHorizontal8Tap5(const uint8_t* source, uint8x8_t s[5]) {
  const uint8x16_t r = vld1q_u8(source);
  s[0] = vget_low_u8(r);
  s[1] = vext_u8(s[0], vget_high_u8(r), 1);
  s[2] = vext_u8(s[0], vget_high_u8(r), 2);
  s[3] = vext_u8(s[0], vget_high_u8(r), 3);
  s[4] = vext_u8(s[0], vget_high_u8(r), 4);
}

inline void LoadHorizontalTap7(const uint8_t* source, uint8x8_t s[7]) {
  // This is just as fast as an 8x8 transpose but avoids over-reading
  // extra rows. It always over-reads by at least 1 value. On small widths
  // (4xH) it over-reads by 9 values.
  const uint8x16_t r = vld1q_u8(source);
  s[0] = vget_low_u8(r);
  s[1] = vext_u8(s[0], vget_high_u8(r), 1);
  s[2] = vext_u8(s[0], vget_high_u8(r), 2);
  s[3] = vext_u8(s[0], vget_high_u8(r), 3);
  s[4] = vext_u8(s[0], vget_high_u8(r), 4);
  s[5] = vext_u8(s[0], vget_high_u8(r), 5);
  s[6] = vext_u8(s[0], vget_high_u8(r), 6);
}

inline int16x8_t HorizontalSum(const uint8x8_t a[3], const int16_t filter[2],
                               int16x8_t sum) {
  const int16x8_t a_0_2 = vreinterpretq_s16_u16(vaddl_u8(a[0], a[2]));
  sum = vmlaq_n_s16(sum, a_0_2, filter[0]);
  sum = vmlaq_n_s16(sum, vreinterpretq_s16_u16(vmovl_u8(a[1])), filter[1]);

  sum = vrshrq_n_s16(sum, kInterRoundBitsHorizontal);

  // Delaying |horizontal_rounding| until after down shifting allows the sum to
  // stay in 16 bits.
  // |horizontal_rounding| = 1 << (bitdepth + kWienerFilterBits - 1)
  //                         1 << (       8 +                 7 - 1)
  // Plus |kInterRoundBitsHorizontal| and it works out to 1 << 11.
  sum = vaddq_s16(sum, vdupq_n_s16(1 << 11));

  // Just like |horizontal_rounding|, adding |filter[3]| at this point allows
  // the sum to stay in 16 bits.
  // But wait! We *did* calculate |filter[3]| and used it in the sum! But it was
  // offset by 128. Fix that here:
  // |src[3]| * 128 >> 3 == |src[3]| << 4
  sum = vaddq_s16(sum, vreinterpretq_s16_u16(vshll_n_u8(a[1], 4)));

  // Saturate to
  // [0,
  // (1 << (bitdepth + 1 + kWienerFilterBits - kInterRoundBitsHorizontal)) - 1)]
  // (1 << (       8 + 1 +                 7 -                         3)) - 1)
  sum = vminq_s16(sum, vdupq_n_s16((1 << 13) - 1));
  sum = vmaxq_s16(sum, vdupq_n_s16(0));
  return sum;
}

inline int16x8_t HorizontalSumTap3(const uint8x8_t a[3],
                                   const int16_t filter[2]) {
  return HorizontalSum(a, filter, vdupq_n_s16(0));
}

inline int16x8_t HorizontalSumTap5(const uint8x8_t a[5],
                                   const int16_t filter[3]) {
  const int16x8_t a_0_4 = vreinterpretq_s16_u16(vaddl_u8(a[0], a[4]));
  const int16x8_t sum = vmulq_n_s16(a_0_4, filter[0]);
  return HorizontalSum(a + 1, filter + 1, sum);
}

inline int16x8_t HorizontalSumTap7(const uint8x8_t a[7],
                                   const int16_t filter[4]) {
  const int16x8_t a_0_6 = vreinterpretq_s16_u16(vaddl_u8(a[0], a[6]));
  const int16x8_t a_1_5 = vreinterpretq_s16_u16(vaddl_u8(a[1], a[5]));
  int16x8_t sum = vmulq_n_s16(a_0_6, filter[0]);
  sum = vmlaq_n_s16(sum, a_1_5, filter[1]);
  return HorizontalSum(a + 2, filter + 2, sum);
}

inline int16x8_t WienerHorizontal4Tap3(const uint8_t* source,
                                       const int16_t filter[2]) {
  uint8x8_t s[5];
  LoadHorizontal4Tap3(source, s);
  return HorizontalSumTap3(s, filter);
}

inline int16x8_t WienerHorizontal4Tap5(const uint8_t* source,
                                       const int16_t filter[3]) {
  uint8x8_t s[5];
  LoadHorizontal4Tap5(source, s);
  return HorizontalSumTap5(s, filter);
}

inline int16x8_t WienerHorizontal4Tap7(const uint8_t* source,
                                       const int16_t filter[4]) {
  uint8x8_t s[7];
  LoadHorizontalTap7(source, s);
  return HorizontalSumTap7(s, filter);
}

inline int16x8_t WienerHorizontal4x2Tap3(const uint8_t* source,
                                         const ptrdiff_t stride,
                                         const int16_t filter[2]) {
  uint8x8_t s0[5], s1[5], s[5];
  LoadHorizontal4Tap3(source + 0 * stride, s0);
  LoadHorizontal4Tap3(source + 1 * stride, s1);
  s[0] = InterleaveLow32(s0[0], s1[0]);
  s[1] = InterleaveLow32(s0[1], s1[1]);
  s[2] = InterleaveLow32(s0[2], s1[2]);
  return HorizontalSumTap3(s, filter);
}

inline int16x8_t WienerHorizontal4x2Tap5(const uint8_t* source,
                                         const ptrdiff_t stride,
                                         const int16_t filter[3]) {
  uint8x8_t s0[5], s1[5], s[5];
  LoadHorizontal4Tap5(source + 0 * stride, s0);
  LoadHorizontal4Tap5(source + 1 * stride, s1);
  s[0] = InterleaveLow32(s0[0], s1[0]);
  s[1] = InterleaveLow32(s0[1], s1[1]);
  s[2] = InterleaveLow32(s0[2], s1[2]);
  s[3] = InterleaveLow32(s0[3], s1[3]);
  s[4] = InterleaveLow32(s0[4], s1[4]);
  return HorizontalSumTap5(s, filter);
}

inline int16x8_t WienerHorizontal4x2Tap7(const uint8_t* source,
                                         const ptrdiff_t stride,
                                         const int16_t filter[4]) {
  uint8x8_t s0[7], s1[7], s[7];
  LoadHorizontalTap7(source + 0 * stride, s0);
  LoadHorizontalTap7(source + 1 * stride, s1);
  s[0] = InterleaveLow32(s0[0], s1[0]);
  s[1] = InterleaveLow32(s0[1], s1[1]);
  s[2] = InterleaveLow32(s0[2], s1[2]);
  s[3] = InterleaveLow32(s0[3], s1[3]);
  s[4] = InterleaveLow32(s0[4], s1[4]);
  s[5] = InterleaveLow32(s0[5], s1[5]);
  s[6] = InterleaveLow32(s0[6], s1[6]);
  return HorizontalSumTap7(s, filter);
}

inline int16x8_t WienerHorizontal8Tap3(const uint8_t* source,
                                       const int16_t filter[2]) {
  uint8x8_t s[3];
  LoadHorizontal8Tap3(source, s);
  return HorizontalSumTap3(s, filter);
}

inline int16x8_t WienerHorizontal8Tap5(const uint8_t* source,
                                       const int16_t filter[3]) {
  uint8x8_t s[5];
  LoadHorizontal8Tap5(source, s);
  return HorizontalSumTap5(s, filter);
}

inline int16x8_t WienerHorizontal8Tap7(const uint8_t* source,
                                       const int16_t filter[4]) {
  uint8x8_t s[7];
  LoadHorizontalTap7(source, s);
  return HorizontalSumTap7(s, filter);
}

inline uint8x8_t WienerVertical(const int16x8_t a[3], const int16_t filter[2],
                                int32x4_t sum[2]) {
  // -(1 << (bitdepth + kInterRoundBitsVertical - 1))
  // -(1 << (       8 +                      11 - 1))
  constexpr int vertical_rounding = -(1 << 18);
  const int32x4_t rounding = vdupq_n_s32(vertical_rounding);
  const int16x8_t a_0_2 = vaddq_s16(a[0], a[2]);

  sum[0] = vaddq_s32(sum[0], rounding);
  sum[1] = vaddq_s32(sum[1], rounding);
  sum[0] = vmlal_n_s16(sum[0], vget_low_s16(a_0_2), filter[0]);
  sum[1] = vmlal_n_s16(sum[1], vget_high_s16(a_0_2), filter[0]);
  sum[0] = vmlal_n_s16(sum[0], vget_low_s16(a[1]), filter[1]);
  sum[1] = vmlal_n_s16(sum[1], vget_high_s16(a[1]), filter[1]);
  const uint16x4_t sum_lo_16 = vqrshrun_n_s32(sum[0], 11);
  const uint16x4_t sum_hi_16 = vqrshrun_n_s32(sum[1], 11);

  return vqmovn_u16(vcombine_u16(sum_lo_16, sum_hi_16));
}

inline uint8x8_t WienerVerticalTap3(const int16x8_t a[3],
                                    const int16_t filter[2]) {
  int32x4_t sum[2];
  sum[0] = sum[1] = vdupq_n_s32(0);
  return WienerVertical(a, filter, sum);
}

inline uint8x8_t WienerVerticalTap5(const int16x8_t a[5],
                                    const int16_t filter[3]) {
  const int16x8_t a_0_4 = vaddq_s16(a[0], a[4]);
  int32x4_t sum[2];

  sum[0] = sum[1] = vdupq_n_s32(0);
  sum[0] = vmlal_n_s16(sum[0], vget_low_s16(a_0_4), filter[0]);
  sum[1] = vmlal_n_s16(sum[1], vget_high_s16(a_0_4), filter[0]);

  return WienerVertical(a + 1, filter + 1, sum);
}

inline uint8x8_t WienerVerticalTap7(const int16x8_t a[7],
                                    const int16_t filter[4]) {
  const int16x8_t a_0_6 = vaddq_s16(a[0], a[6]);
  const int16x8_t a_1_5 = vaddq_s16(a[1], a[5]);
  int32x4_t sum[2];

  sum[0] = sum[1] = vdupq_n_s32(0);
  sum[0] = vmlal_n_s16(sum[0], vget_low_s16(a_0_6), filter[0]);
  sum[1] = vmlal_n_s16(sum[1], vget_high_s16(a_0_6), filter[0]);
  sum[0] = vmlal_n_s16(sum[0], vget_low_s16(a_1_5), filter[1]);
  sum[1] = vmlal_n_s16(sum[1], vget_high_s16(a_1_5), filter[1]);

  return WienerVertical(a + 2, filter + 2, sum);
}

// For width 16 and up, store the horizontal results, and then do the vertical
// filter row by row. This is faster than doing it column by column when
// considering cache issues.
void WienerFilter_NEON(const void* const source, void* const dest,
                       const RestorationUnitInfo& restoration_info,
                       const ptrdiff_t source_stride,
                       const ptrdiff_t dest_stride, const int width,
                       const int height, RestorationBuffer* const buffer) {
  constexpr int kCenterTap = (kSubPixelTaps - 1) / 2;
  const int number_zero_coefficients =
      CountZeroCoefficients(restoration_info.wiener_info.filter);
  const auto* src = static_cast<const uint8_t*>(source);
  auto* dst = static_cast<uint8_t*>(dest);
  // Casting once here saves a lot of vreinterpret() calls. The values are
  // saturated to 13 bits before storing.
  int16_t* wiener_buffer = reinterpret_cast<int16_t*>(buffer->wiener_buffer);
  int16_t filter_horizontal[kSubPixelTaps / 2];
  int16_t filter_vertical[kSubPixelTaps / 2];
  int16x8_t a[7];

  PopulateWienerCoefficients(restoration_info, WienerInfo::kHorizontal,
                             filter_horizontal);
  PopulateWienerCoefficients(restoration_info, WienerInfo::kVertical,
                             filter_vertical);

  if (number_zero_coefficients == 0) {
    // 7-tap
    src -= kCenterTap * source_stride + kCenterTap;

    if (width > 8) {
      int y = height + kSubPixelTaps - 2;
      do {
        int x = 0;
        do {
          const int16x8_t a = WienerHorizontal8Tap7(src + x, filter_horizontal);
          vst1q_s16(wiener_buffer + x, a);
          x += 8;
        } while (x < width);
        src += source_stride;
        wiener_buffer += width;
      } while (--y != 0);

      wiener_buffer = reinterpret_cast<int16_t*>(buffer->wiener_buffer);

      y = height;
      do {
        int x = 0;
        do {
          a[0] = vld1q_s16(wiener_buffer + x + 0 * width);
          a[1] = vld1q_s16(wiener_buffer + x + 1 * width);
          a[2] = vld1q_s16(wiener_buffer + x + 2 * width);
          a[3] = vld1q_s16(wiener_buffer + x + 3 * width);
          a[4] = vld1q_s16(wiener_buffer + x + 4 * width);
          a[5] = vld1q_s16(wiener_buffer + x + 5 * width);
          a[6] = vld1q_s16(wiener_buffer + x + 6 * width);

          const uint8x8_t r = WienerVerticalTap7(a, filter_vertical);
          vst1_u8(dst + x, r);
          x += 8;
        } while (x < width);
        wiener_buffer += width;
        dst += dest_stride;
      } while (--y != 0);
    } else if (width > 4) {
      a[0] = WienerHorizontal8Tap7(src, filter_horizontal);
      src += source_stride;
      a[1] = WienerHorizontal8Tap7(src, filter_horizontal);
      src += source_stride;
      a[2] = WienerHorizontal8Tap7(src, filter_horizontal);
      src += source_stride;
      a[3] = WienerHorizontal8Tap7(src, filter_horizontal);
      src += source_stride;
      a[4] = WienerHorizontal8Tap7(src, filter_horizontal);
      src += source_stride;
      a[5] = WienerHorizontal8Tap7(src, filter_horizontal);
      src += source_stride;

      int y = height;
      do {
        a[6] = WienerHorizontal8Tap7(src, filter_horizontal);
        src += source_stride;

        const uint8x8_t r = WienerVerticalTap7(a, filter_vertical);
        vst1_u8(dst, r);
        dst += dest_stride;

        a[0] = a[1];
        a[1] = a[2];
        a[2] = a[3];
        a[3] = a[4];
        a[4] = a[5];
        a[5] = a[6];
      } while (--y != 0);
    } else {
      int y = height;

      if ((y & 1) != 0) {
        --y;
        a[0] = WienerHorizontal4x2Tap7(src, source_stride, filter_horizontal);
        src += source_stride;
        a[2] = WienerHorizontal4x2Tap7(src + source_stride, source_stride,
                                       filter_horizontal);
        a[4] = WienerHorizontal4x2Tap7(src + 3 * source_stride, source_stride,
                                       filter_horizontal);
        a[1] = vcombine_s16(vget_high_s16(a[0]), vget_low_s16(a[2]));
        a[3] = vcombine_s16(vget_high_s16(a[2]), vget_low_s16(a[4]));
        a[6] =
            WienerHorizontal4Tap7(src + 5 * source_stride, filter_horizontal);
        a[5] = vcombine_s16(vget_high_s16(a[4]), vget_low_s16(a[6]));
        const uint8x8_t r = WienerVerticalTap7(a, filter_vertical);
        StoreLo4(dst, r);
        dst += dest_stride;
      }

      if (y != 0) {
        a[0] = WienerHorizontal4x2Tap7(src, source_stride, filter_horizontal);
        src += 2 * source_stride;
        a[2] = WienerHorizontal4x2Tap7(src, source_stride, filter_horizontal);
        src += 2 * source_stride;
        a[4] = WienerHorizontal4x2Tap7(src, source_stride, filter_horizontal);
        src += 2 * source_stride;
        a[1] = vcombine_s16(vget_high_s16(a[0]), vget_low_s16(a[2]));
        a[3] = vcombine_s16(vget_high_s16(a[2]), vget_low_s16(a[4]));

        do {
          a[6] = WienerHorizontal4x2Tap7(src, source_stride, filter_horizontal);
          src += 2 * source_stride;
          a[5] = vcombine_s16(vget_high_s16(a[4]), vget_low_s16(a[6]));

          const uint8x8_t r = WienerVerticalTap7(a, filter_vertical);
          StoreLo4(dst, r);
          dst += dest_stride;
          StoreHi4(dst, r);
          dst += dest_stride;

          a[0] = a[2];
          a[1] = a[3];
          a[2] = a[4];
          a[3] = a[5];
          a[4] = a[6];
          y -= 2;
        } while (y != 0);
      }
    }
  } else if (number_zero_coefficients == 1) {
    // 5-tap
    src -= (kCenterTap - 1) * source_stride + kCenterTap - 1;

    if (width > 8) {
      int y = height + kSubPixelTaps - 4;
      do {
        int x = 0;
        do {
          const int16x8_t a =
              WienerHorizontal8Tap5(src + x, filter_horizontal + 1);
          vst1q_s16(wiener_buffer + x, a);
          x += 8;
        } while (x < width);
        src += source_stride;
        wiener_buffer += width;
      } while (--y != 0);

      wiener_buffer = reinterpret_cast<int16_t*>(buffer->wiener_buffer);

      y = height;
      do {
        int x = 0;
        do {
          a[0] = vld1q_s16(wiener_buffer + x + 0 * width);
          a[1] = vld1q_s16(wiener_buffer + x + 1 * width);
          a[2] = vld1q_s16(wiener_buffer + x + 2 * width);
          a[3] = vld1q_s16(wiener_buffer + x + 3 * width);
          a[4] = vld1q_s16(wiener_buffer + x + 4 * width);

          const uint8x8_t r = WienerVerticalTap5(a, filter_vertical + 1);
          vst1_u8(dst + x, r);
          x += 8;
        } while (x < width);
        wiener_buffer += width;
        dst += dest_stride;
      } while (--y != 0);
    } else if (width > 4) {
      a[0] = WienerHorizontal8Tap5(src, filter_horizontal + 1);
      src += source_stride;
      a[1] = WienerHorizontal8Tap5(src, filter_horizontal + 1);
      src += source_stride;
      a[2] = WienerHorizontal8Tap5(src, filter_horizontal + 1);
      src += source_stride;
      a[3] = WienerHorizontal8Tap5(src, filter_horizontal + 1);
      src += source_stride;

      int y = height;
      do {
        a[4] = WienerHorizontal8Tap5(src, filter_horizontal + 1);
        src += source_stride;

        const uint8x8_t r = WienerVerticalTap5(a, filter_vertical + 1);
        vst1_u8(dst, r);
        dst += dest_stride;

        a[0] = a[1];
        a[1] = a[2];
        a[2] = a[3];
        a[3] = a[4];
      } while (--y != 0);
    } else {
      int y = height;

      if ((y & 1) != 0) {
        --y;
        a[0] =
            WienerHorizontal4x2Tap5(src, source_stride, filter_horizontal + 1);
        src += source_stride;
        a[2] = WienerHorizontal4x2Tap5(src + source_stride, source_stride,
                                       filter_horizontal + 1);
        a[1] = vcombine_s16(vget_high_s16(a[0]), vget_low_s16(a[2]));
        a[4] = WienerHorizontal4Tap5(src + 3 * source_stride,
                                     filter_horizontal + 1);
        a[3] = vcombine_s16(vget_high_s16(a[2]), vget_low_s16(a[4]));
        const uint8x8_t r = WienerVerticalTap5(a, filter_vertical + 1);
        StoreLo4(dst, r);
        dst += dest_stride;
      }

      if (y != 0) {
        a[0] =
            WienerHorizontal4x2Tap5(src, source_stride, filter_horizontal + 1);
        src += 2 * source_stride;
        a[2] =
            WienerHorizontal4x2Tap5(src, source_stride, filter_horizontal + 1);
        src += 2 * source_stride;
        a[1] = vcombine_s16(vget_high_s16(a[0]), vget_low_s16(a[2]));

        do {
          a[4] = WienerHorizontal4x2Tap5(src, source_stride,
                                         filter_horizontal + 1);
          src += 2 * source_stride;
          a[3] = vcombine_s16(vget_high_s16(a[2]), vget_low_s16(a[4]));

          const uint8x8_t r = WienerVerticalTap5(a, filter_vertical + 1);
          StoreLo4(dst, r);
          dst += dest_stride;
          StoreHi4(dst, r);
          dst += dest_stride;

          a[0] = a[2];
          a[1] = a[3];
          a[2] = a[4];
          y -= 2;
        } while (y != 0);
      }
    }
  } else {
    // 3-tap
    src -= (kCenterTap - 2) * source_stride + kCenterTap - 2;

    if (width > 8) {
      int y = height + kSubPixelTaps - 6;
      do {
        int x = 0;
        do {
          const int16x8_t a =
              WienerHorizontal8Tap3(src + x, filter_horizontal + 2);
          vst1q_s16(wiener_buffer + x, a);
          x += 8;
        } while (x < width);
        src += source_stride;
        wiener_buffer += width;
      } while (--y != 0);

      wiener_buffer = reinterpret_cast<int16_t*>(buffer->wiener_buffer);

      y = height;
      do {
        int x = 0;
        do {
          a[0] = vld1q_s16(wiener_buffer + x + 0 * width);
          a[1] = vld1q_s16(wiener_buffer + x + 1 * width);
          a[2] = vld1q_s16(wiener_buffer + x + 2 * width);

          const uint8x8_t r = WienerVerticalTap3(a, filter_vertical + 2);
          vst1_u8(dst + x, r);
          x += 8;
        } while (x < width);
        wiener_buffer += width;
        dst += dest_stride;
      } while (--y != 0);
    } else if (width > 4) {
      a[0] = WienerHorizontal8Tap3(src, filter_horizontal + 2);
      src += source_stride;
      a[1] = WienerHorizontal8Tap3(src, filter_horizontal + 2);
      src += source_stride;

      int y = height;
      do {
        a[2] = WienerHorizontal8Tap3(src, filter_horizontal + 2);
        src += source_stride;

        const uint8x8_t r = WienerVerticalTap3(a, filter_vertical + 2);
        vst1_u8(dst, r);
        dst += dest_stride;

        a[0] = a[1];
        a[1] = a[2];
      } while (--y != 0);
    } else {
      int y = height;

      if ((y & 1) != 0) {
        --y;
        a[0] =
            WienerHorizontal4x2Tap3(src, source_stride, filter_horizontal + 2);
        src += source_stride;
        a[2] =
            WienerHorizontal4Tap3(src + source_stride, filter_horizontal + 2);
        a[1] = vcombine_s16(vget_high_s16(a[0]), vget_low_s16(a[2]));
        const uint8x8_t r = WienerVerticalTap3(a, filter_vertical + 2);
        StoreLo4(dst, r);
        dst += dest_stride;
      }

      if (y != 0) {
        a[0] =
            WienerHorizontal4x2Tap3(src, source_stride, filter_horizontal + 2);
        src += 2 * source_stride;

        do {
          a[2] = WienerHorizontal4x2Tap3(src, source_stride,
                                         filter_horizontal + 2);
          src += 2 * source_stride;
          a[1] = vcombine_s16(vget_high_s16(a[0]), vget_low_s16(a[2]));

          const uint8x8_t r = WienerVerticalTap3(a, filter_vertical + 2);
          StoreLo4(dst, r);
          dst += dest_stride;
          StoreHi4(dst, r);
          dst += dest_stride;

          a[0] = a[2];
          y -= 2;
        } while (y != 0);
      }
    }
  }
}

// SGR

constexpr int kSgrProjScaleBits = 20;
constexpr int kSgrProjRestoreBits = 4;
constexpr int kSgrProjSgrBits = 8;
constexpr int kSgrProjReciprocalBits = 12;

// a2 = ((z << kSgrProjSgrBits) + (z >> 1)) / (z + 1);
// sgr_ma2 = 256 - a2
constexpr uint8_t kSgrMa2Lookup[256] = {
    255, 128, 85, 64, 51, 43, 37, 32, 28, 26, 23, 21, 20, 18, 17, 16, 15, 14,
    13,  13,  12, 12, 11, 11, 10, 10, 9,  9,  9,  9,  8,  8,  8,  8,  7,  7,
    7,   7,   7,  6,  6,  6,  6,  6,  6,  6,  5,  5,  5,  5,  5,  5,  5,  5,
    5,   5,   4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
    4,   3,   3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,   3,   3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  2,  2,  2,  2,  2,  2,
    2,   2,   2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,   2,   2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,   2,   2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,   2,   2,  2,  2,  2,  2,  2,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,   1,   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,   1,   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,   1,   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,   1,   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,   1,   1,  0};

template <int n>
inline uint16x4_t CalculateSgrMA2(const uint32x4_t sum_sq, const uint16x4_t sum,
                                  const uint32_t s) {
  // a = |sum_sq|
  // d = |sum|
  // p = (a * n < d * d) ? 0 : a * n - d * d;
  const uint32x4_t dxd = vmull_u16(sum, sum);
  const uint32x4_t axn = vmulq_n_u32(sum_sq, n);
  // Ensure |p| does not underflow by using saturating subtraction.
  const uint32x4_t p = vqsubq_u32(axn, dxd);

  // z = RightShiftWithRounding(p * s, kSgrProjScaleBits);
  const uint32x4_t pxs = vmulq_n_u32(p, s);
  // For some reason vrshrn_n_u32() (narrowing shift) can only shift by 16
  // and kSgrProjScaleBits is 20.
  const uint32x4_t shifted = vrshrq_n_u32(pxs, kSgrProjScaleBits);
  return vmovn_u32(shifted);
}

inline uint16x4_t CalculateB2Shifted(const uint8x8_t sgr_ma2,
                                     const uint16x4_t sum,
                                     const uint32_t one_over_n) {
  // b2 = ((1 << kSgrProjSgrBits) - a2) * b * one_over_n
  // 1 << kSgrProjSgrBits = 256
  // |a2| = [1, 256]
  // |sgr_ma2| max value = 255
  // |sum| is a box sum with radius 1 or 2.
  // For the first pass radius is 2. Maximum value is 5x5x255 = 6375.
  // For the second pass radius is 1. Maximum value is 3x3x255 = 2295.
  // |one_over_n| = ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n
  // When radius is 2 |n| is 25. |one_over_n| is 164.
  // When radius is 1 |n| is 9. |one_over_n| is 455.
  const uint16x8_t sgr_ma2q = vmovl_u8(sgr_ma2);
  const uint32x4_t m = vmull_u16(vget_high_u16(sgr_ma2q), sum);
  const uint32x4_t b2 = vmulq_n_u32(m, one_over_n);
  // static_cast<int>(RightShiftWithRounding(b2, kSgrProjReciprocalBits));
  // |kSgrProjReciprocalBits| is 12.
  // Radius 2: 255 * 6375 * 164 >> 12 = 65088 (16 bits).
  // Radius 1: 255 * 2295 * 455 >> 12 = 65009 (16 bits).
  return vrshrn_n_u32(b2, kSgrProjReciprocalBits);
}

inline uint16x8_t CalculateB2Shifted(const uint8x8_t sgr_ma2,
                                     const uint16x8_t sum,
                                     const uint32_t one_over_n) {
  // b2 = ((1 << kSgrProjSgrBits) - a2) * b * one_over_n
  // 1 << kSgrProjSgrBits = 256
  // |a2| = [1, 256]
  // |sgr_ma2| max value = 255
  // |sum| is a box sum with radius 1 or 2.
  // For the first pass radius is 2. Maximum value is 5x5x255 = 6375.
  // For the second pass radius is 1. Maximum value is 3x3x255 = 2295.
  // |one_over_n| = ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n
  // When radius is 2 |n| is 25. |one_over_n| is 164.
  // When radius is 1 |n| is 9. |one_over_n| is 455.
  const uint16x8_t sgr_ma2q = vmovl_u8(sgr_ma2);
  const uint32x4_t m0 = vmull_u16(vget_low_u16(sgr_ma2q), vget_low_u16(sum));
  const uint32x4_t m1 = vmull_u16(vget_high_u16(sgr_ma2q), vget_high_u16(sum));
  const uint32x4_t m2 = vmulq_n_u32(m0, one_over_n);
  const uint32x4_t m3 = vmulq_n_u32(m1, one_over_n);
  // static_cast<int>(RightShiftWithRounding(b2, kSgrProjReciprocalBits));
  // |kSgrProjReciprocalBits| is 12.
  // Radius 2: 255 * 6375 * 164 >> 12 = 65088 (16 bits).
  // Radius 1: 255 * 2295 * 455 >> 12 = 65009 (16 bits).
  const uint16x4_t b2_lo = vrshrn_n_u32(m2, kSgrProjReciprocalBits);
  const uint16x4_t b2_hi = vrshrn_n_u32(m3, kSgrProjReciprocalBits);
  return vcombine_u16(b2_lo, b2_hi);
}

inline uint16x8_t Sum3(const uint16x8_t left, const uint16x8_t middle,
                       const uint16x8_t right) {
  const uint16x8_t sum = vaddq_u16(left, middle);
  return vaddq_u16(sum, right);
}

inline uint32x4_t Sum3(const uint32x4_t left, const uint32x4_t middle,
                       const uint32x4_t right) {
  const uint32x4_t sum = vaddq_u32(left, middle);
  return vaddq_u32(sum, right);
}

inline uint16x8_t Sum3W(const uint8x8_t left, const uint8x8_t middle,
                        const uint8x8_t right) {
  const uint16x8_t sum = vaddl_u8(left, middle);
  return vaddw_u8(sum, right);
}

inline uint32x4_t Sum3W(const uint16x4_t left, const uint16x4_t middle,
                        const uint16x4_t right) {
  const uint32x4_t sum = vaddl_u16(left, middle);
  return vaddw_u16(sum, right);
}

inline uint16x4_t Sum3(const uint16x4_t left, const uint16x4_t middle,
                       const uint16x4_t right) {
  const uint16x4_t sum = vadd_u16(left, middle);
  return vadd_u16(sum, right);
}

inline uint16x8_t Sum3W(const uint8x8_t a[3]) {
  return Sum3W(a[0], a[1], a[2]);
}

inline uint16x8x2_t Sum3W(const uint8x16_t a[3]) {
  const uint8x8_t low0 = vget_low_u8(a[0]);
  const uint8x8_t low1 = vget_low_u8(a[1]);
  const uint8x8_t low2 = vget_low_u8(a[2]);
  const uint8x8_t high0 = vget_high_u8(a[0]);
  const uint8x8_t high1 = vget_high_u8(a[1]);
  const uint8x8_t high2 = vget_high_u8(a[2]);
  uint16x8x2_t sum;
  sum.val[0] = Sum3W(low0, low1, low2);
  sum.val[1] = Sum3W(high0, high1, high2);
  return sum;
}

inline uint32x4x2_t Sum3W(const uint16x8_t a[3]) {
  const uint16x4_t low0 = vget_low_u16(a[0]);
  const uint16x4_t low1 = vget_low_u16(a[1]);
  const uint16x4_t low2 = vget_low_u16(a[2]);
  const uint16x4_t high0 = vget_high_u16(a[0]);
  const uint16x4_t high1 = vget_high_u16(a[1]);
  const uint16x4_t high2 = vget_high_u16(a[2]);
  uint32x4x2_t sum;
  sum.val[0] = Sum3W(low0, low1, low2);
  sum.val[1] = Sum3W(high0, high1, high2);
  return sum;
}

template <int index>
inline uint32x4_t Sum3WLow(const uint16x8x2_t a[3]) {
  const uint16x4_t low0 = vget_low_u16(a[0].val[index]);
  const uint16x4_t low1 = vget_low_u16(a[1].val[index]);
  const uint16x4_t low2 = vget_low_u16(a[2].val[index]);
  return Sum3W(low0, low1, low2);
}

template <int index>
inline uint32x4_t Sum3WHigh(const uint16x8x2_t a[3]) {
  const uint16x4_t high0 = vget_high_u16(a[0].val[index]);
  const uint16x4_t high1 = vget_high_u16(a[1].val[index]);
  const uint16x4_t high2 = vget_high_u16(a[2].val[index]);
  return Sum3W(high0, high1, high2);
}

inline uint32x4x3_t Sum3W(const uint16x8x2_t a[3]) {
  uint32x4x3_t sum;
  sum.val[0] = Sum3WLow<0>(a);
  sum.val[1] = Sum3WHigh<0>(a);
  sum.val[2] = Sum3WLow<1>(a);
  return sum;
}

inline uint16x4_t Sum5(const uint16x4_t a[5]) {
  const uint16x4_t sum01 = vadd_u16(a[0], a[1]);
  const uint16x4_t sum23 = vadd_u16(a[2], a[3]);
  const uint16x4_t sum = vadd_u16(sum01, sum23);
  return vadd_u16(sum, a[4]);
}

inline uint16x8_t Sum5(const uint16x8_t a[5]) {
  const uint16x8_t sum01 = vaddq_u16(a[0], a[1]);
  const uint16x8_t sum23 = vaddq_u16(a[2], a[3]);
  const uint16x8_t sum = vaddq_u16(sum01, sum23);
  return vaddq_u16(sum, a[4]);
}

inline uint32x4_t Sum5(const uint32x4_t a[5]) {
  const uint32x4_t sum01 = vaddq_u32(a[0], a[1]);
  const uint32x4_t sum23 = vaddq_u32(a[2], a[3]);
  const uint32x4_t sum = vaddq_u32(sum01, sum23);
  return vaddq_u32(sum, a[4]);
}

inline uint16x8_t Sum5W(const uint8x8_t a[5]) {
  const uint16x8_t sum01 = vaddl_u8(a[0], a[1]);
  const uint16x8_t sum23 = vaddl_u8(a[2], a[3]);
  const uint16x8_t sum = vaddq_u16(sum01, sum23);
  return vaddw_u8(sum, a[4]);
}

inline uint32x4_t Sum5W(const uint16x4_t a[5]) {
  const uint32x4_t sum01 = vaddl_u16(a[0], a[1]);
  const uint32x4_t sum23 = vaddl_u16(a[2], a[3]);
  const uint32x4_t sum0123 = vaddq_u32(sum01, sum23);
  return vaddw_u16(sum0123, a[4]);
}

inline uint16x8x2_t Sum5W(const uint8x16_t a[5]) {
  uint16x8x2_t sum;
  uint8x8_t low[5], high[5];
  low[0] = vget_low_u8(a[0]);
  low[1] = vget_low_u8(a[1]);
  low[2] = vget_low_u8(a[2]);
  low[3] = vget_low_u8(a[3]);
  low[4] = vget_low_u8(a[4]);
  high[0] = vget_high_u8(a[0]);
  high[1] = vget_high_u8(a[1]);
  high[2] = vget_high_u8(a[2]);
  high[3] = vget_high_u8(a[3]);
  high[4] = vget_high_u8(a[4]);
  sum.val[0] = Sum5W(low);
  sum.val[1] = Sum5W(high);
  return sum;
}

inline uint32x4x2_t Sum5W(const uint16x8_t a[5]) {
  uint32x4x2_t sum;
  uint16x4_t low[5], high[5];
  low[0] = vget_low_u16(a[0]);
  low[1] = vget_low_u16(a[1]);
  low[2] = vget_low_u16(a[2]);
  low[3] = vget_low_u16(a[3]);
  low[4] = vget_low_u16(a[4]);
  high[0] = vget_high_u16(a[0]);
  high[1] = vget_high_u16(a[1]);
  high[2] = vget_high_u16(a[2]);
  high[3] = vget_high_u16(a[3]);
  high[4] = vget_high_u16(a[4]);
  sum.val[0] = Sum5W(low);
  sum.val[1] = Sum5W(high);
  return sum;
}

template <int index>
inline uint32x4_t Sum5WLow(const uint16x8x2_t a[5]) {
  uint16x4_t low[5];
  low[0] = vget_low_u16(a[0].val[index]);
  low[1] = vget_low_u16(a[1].val[index]);
  low[2] = vget_low_u16(a[2].val[index]);
  low[3] = vget_low_u16(a[3].val[index]);
  low[4] = vget_low_u16(a[4].val[index]);
  return Sum5W(low);
}

template <int index>
inline uint32x4_t Sum5WHigh(const uint16x8x2_t a[5]) {
  uint16x4_t high[5];
  high[0] = vget_high_u16(a[0].val[index]);
  high[1] = vget_high_u16(a[1].val[index]);
  high[2] = vget_high_u16(a[2].val[index]);
  high[3] = vget_high_u16(a[3].val[index]);
  high[4] = vget_high_u16(a[4].val[index]);
  return Sum5W(high);
}

inline uint32x4x3_t Sum5W(const uint16x8x2_t a[5]) {
  uint32x4x3_t sum;
  sum.val[0] = Sum5WLow<0>(a);
  sum.val[1] = Sum5WHigh<0>(a);
  sum.val[2] = Sum5WLow<1>(a);
  return sum;
}

inline uint16x4_t Sum3Horizontal(const uint16x8_t a) {
  const uint16x4_t left = vget_low_u16(a);
  const uint16x4_t middle = vext_u16(vget_low_u16(a), vget_high_u16(a), 1);
  const uint16x4_t right = vext_u16(vget_low_u16(a), vget_high_u16(a), 2);
  return Sum3(left, middle, right);
}

inline uint16x8_t Sum3Horizontal(const uint16x8x2_t a) {
  const uint16x8_t left = a.val[0];
  const uint16x8_t middle = vextq_u16(a.val[0], a.val[1], 1);
  const uint16x8_t right = vextq_u16(a.val[0], a.val[1], 2);
  return Sum3(left, middle, right);
}

inline uint32x4_t Sum3Horizontal(const uint32x4x2_t a) {
  const uint32x4_t left = a.val[0];
  const uint32x4_t middle = vextq_u32(a.val[0], a.val[1], 1);
  const uint32x4_t right = vextq_u32(a.val[0], a.val[1], 2);
  return Sum3(left, middle, right);
}

inline uint32x4x2_t Sum3Horizontal(const uint32x4x3_t a) {
  uint32x4x2_t sum;
  {
    const uint32x4_t left = a.val[0];
    const uint32x4_t middle = vextq_u32(a.val[0], a.val[1], 1);
    const uint32x4_t right = vextq_u32(a.val[0], a.val[1], 2);
    sum.val[0] = Sum3(left, middle, right);
  }
  {
    const uint32x4_t left = a.val[1];
    const uint32x4_t middle = vextq_u32(a.val[1], a.val[2], 1);
    const uint32x4_t right = vextq_u32(a.val[1], a.val[2], 2);
    sum.val[1] = Sum3(left, middle, right);
  }
  return sum;
}

inline uint16x4_t Sum3HorizontalOffset1(const uint16x8_t a) {
  const uint16x4_t left = vext_u16(vget_low_u16(a), vget_high_u16(a), 1);
  const uint16x4_t middle = vext_u16(vget_low_u16(a), vget_high_u16(a), 2);
  const uint16x4_t right = vext_u16(vget_low_u16(a), vget_high_u16(a), 3);
  return Sum3(left, middle, right);
}

inline uint16x8_t Sum3HorizontalOffset1(const uint16x8x2_t a) {
  const uint16x8_t left = vextq_u16(a.val[0], a.val[1], 1);
  const uint16x8_t middle = vextq_u16(a.val[0], a.val[1], 2);
  const uint16x8_t right = vextq_u16(a.val[0], a.val[1], 3);
  return Sum3(left, middle, right);
}

inline uint32x4_t Sum3HorizontalOffset1(const uint32x4x2_t a) {
  const uint32x4_t left = vextq_u32(a.val[0], a.val[1], 1);
  const uint32x4_t middle = vextq_u32(a.val[0], a.val[1], 2);
  const uint32x4_t right = vextq_u32(a.val[0], a.val[1], 3);
  return Sum3(left, middle, right);
}

inline uint32x4x2_t Sum3HorizontalOffset1(const uint32x4x3_t a) {
  uint32x4x2_t sum;
  {
    const uint32x4_t left = vextq_u32(a.val[0], a.val[1], 1);
    const uint32x4_t middle = vextq_u32(a.val[0], a.val[1], 2);
    const uint32x4_t right = vextq_u32(a.val[0], a.val[1], 3);
    sum.val[0] = Sum3(left, middle, right);
  }
  {
    const uint32x4_t left = vextq_u32(a.val[1], a.val[2], 1);
    const uint32x4_t middle = vextq_u32(a.val[1], a.val[2], 2);
    const uint32x4_t right = vextq_u32(a.val[1], a.val[2], 3);
    sum.val[1] = Sum3(left, middle, right);
  }
  return sum;
}

inline uint16x4_t Sum5Horizontal(const uint16x8_t a) {
  uint16x4_t s[5];
  s[0] = vget_low_u16(a);
  s[1] = vext_u16(vget_low_u16(a), vget_high_u16(a), 1);
  s[2] = vext_u16(vget_low_u16(a), vget_high_u16(a), 2);
  s[3] = vext_u16(vget_low_u16(a), vget_high_u16(a), 3);
  s[4] = vget_high_u16(a);
  return Sum5(s);
}

inline uint16x8_t Sum5Horizontal(const uint16x8x2_t a) {
  uint16x8_t s[5];
  s[0] = a.val[0];
  s[1] = vextq_u16(a.val[0], a.val[1], 1);
  s[2] = vextq_u16(a.val[0], a.val[1], 2);
  s[3] = vextq_u16(a.val[0], a.val[1], 3);
  s[4] = vcombine_u16(vget_high_u16(a.val[0]), vget_low_u16(a.val[1]));
  return Sum5(s);
}

inline uint32x4_t Sum5Horizontal(const uint32x4x2_t a) {
  uint32x4_t s[5];
  s[0] = a.val[0];
  s[1] = vextq_u32(a.val[0], a.val[1], 1);
  s[2] = vextq_u32(a.val[0], a.val[1], 2);
  s[3] = vextq_u32(a.val[0], a.val[1], 3);
  s[4] = a.val[1];
  return Sum5(s);
}

inline uint32x4x2_t Sum5Horizontal(const uint32x4x3_t a) {
  uint32x4x2_t sum;
  uint32x4_t s[5];
  s[0] = a.val[0];
  s[1] = vextq_u32(a.val[0], a.val[1], 1);
  s[2] = vextq_u32(a.val[0], a.val[1], 2);
  s[3] = vextq_u32(a.val[0], a.val[1], 3);
  s[4] = a.val[1];
  sum.val[0] = Sum5(s);
  s[0] = a.val[1];
  s[1] = vextq_u32(a.val[1], a.val[2], 1);
  s[2] = vextq_u32(a.val[1], a.val[2], 2);
  s[3] = vextq_u32(a.val[1], a.val[2], 3);
  s[4] = a.val[2];
  sum.val[1] = Sum5(s);
  return sum;
}

template <int size, int offset>
inline void PreProcess4(const uint8x8_t* const row,
                        const uint16x8_t* const row_sq, const uint32_t s,
                        uint16_t* const dst) {
  static_assert(offset == 0 || offset == 1, "");
  // Number of elements in the box being summed.
  constexpr uint32_t n = size * size;
  constexpr uint32_t one_over_n =
      ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;
  const uint16x4_t v_255 = vdup_n_u16(255);
  uint16x4_t sum;
  uint32x4_t sum_sq;
  if (size == 3) {
    if (offset == 0) {
      sum = Sum3Horizontal(Sum3W(row));
      sum_sq = Sum3Horizontal(Sum3W(row_sq));
    } else {
      sum = Sum3HorizontalOffset1(Sum3W(row));
      sum_sq = Sum3HorizontalOffset1(Sum3W(row_sq));
    }
  }
  if (size == 5) {
    sum = Sum5Horizontal(Sum5W(row));
    sum_sq = Sum5Horizontal(Sum5W(row_sq));
  }
  const uint16x4_t z0 = CalculateSgrMA2<n>(sum_sq, sum, s);
  const uint16x4_t z = vmin_u16(v_255, z0);
  // Using vget_lane_s16() can save a sign extension instruction.
  // Add 4 0s for memory initialization purpose only.
  const uint8_t lookup[8] = {
      0,
      0,
      0,
      0,
      kSgrMa2Lookup[vget_lane_s16(vreinterpret_s16_u16(z), 0)],
      kSgrMa2Lookup[vget_lane_s16(vreinterpret_s16_u16(z), 1)],
      kSgrMa2Lookup[vget_lane_s16(vreinterpret_s16_u16(z), 2)],
      kSgrMa2Lookup[vget_lane_s16(vreinterpret_s16_u16(z), 3)]};
  const uint8x8_t sgr_ma2 = vld1_u8(lookup);
  const uint16x4_t b2 = CalculateB2Shifted(sgr_ma2, sum, one_over_n);
  const uint16x8_t sgr_ma2_b2 = vcombine_u16(vreinterpret_u16_u8(sgr_ma2), b2);
  vst1q_u16(dst, sgr_ma2_b2);
}

template <int size, int offset>
inline void PreProcess8(const uint8x16_t* const row,
                        const uint16x8x2_t* const row_sq, const uint32_t s,
                        uint8x8_t* const sgr_ma2, uint16x8_t* const b2,
                        uint16_t* const dst) {
  // Number of elements in the box being summed.
  constexpr uint32_t n = size * size;
  constexpr uint32_t one_over_n =
      ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;
  const uint16x8_t v_255 = vdupq_n_u16(255);
  uint16x8_t sum;
  uint32x4x2_t sum_sq;
  if (size == 3) {
    if (offset == 0) {
      sum = Sum3Horizontal(Sum3W(row));
      sum_sq = Sum3Horizontal(Sum3W(row_sq));
    } else /* if (offset == 1) */ {
      sum = Sum3HorizontalOffset1(Sum3W(row));
      sum_sq = Sum3HorizontalOffset1(Sum3W(row_sq));
    }
  }
  if (size == 5) {
    sum = Sum5Horizontal(Sum5W(row));
    sum_sq = Sum5Horizontal(Sum5W(row_sq));
  }
  const uint16x4_t z0 = CalculateSgrMA2<n>(sum_sq.val[0], vget_low_u16(sum), s);
  const uint16x4_t z1 =
      CalculateSgrMA2<n>(sum_sq.val[1], vget_high_u16(sum), s);
  const uint16x8_t z01 = vcombine_u16(z0, z1);
  // Using vqmovn_u16() needs an extra sign extension instruction.
  const uint16x8_t z = vminq_u16(v_255, z01);
  // Using vgetq_lane_s16() can save the sign extension instruction.
  const uint8_t lookup[8] = {
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 0)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 1)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 2)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 3)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 4)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 5)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 6)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 7)]};
  *sgr_ma2 = vld1_u8(lookup);
  *b2 = CalculateB2Shifted(*sgr_ma2, sum, one_over_n);
  const uint16x8_t sgr_ma2_b2 =
      vcombine_u16(vreinterpret_u16_u8(*sgr_ma2), vget_high_u16(*b2));
  vst1q_u16(dst, sgr_ma2_b2);
}

inline void Prepare3(const uint8x8_t a[2], uint8x8_t* const left,
                     uint8x8_t* const middle, uint8x8_t* const right) {
  *left = vext_u8(a[0], a[1], 4);
  *middle = vext_u8(a[0], a[1], 5);
  *right = vext_u8(a[0], a[1], 6);
}

inline void Prepare3(const uint16x8_t a[2], uint16x8_t* const left,
                     uint16x8_t* const middle, uint16x8_t* const right) {
  *left = vcombine_u16(vget_high_u16(a[0]), vget_low_u16(a[1]));
  *middle = vextq_u16(a[0], a[1], 5);
  *right = vextq_u16(a[0], a[1], 6);
}

inline uint16x8_t Sum343(const uint8x8_t a[2]) {
  uint8x8_t left, middle, right;
  Prepare3(a, &left, &middle, &right);
  const uint16x8_t sum = Sum3W(left, middle, right);
  const uint16x8_t sum3 = Sum3(sum, sum, sum);
  return vaddw_u8(sum3, middle);
}

inline void Sum343_444(const uint8x8_t a[2], uint16x8_t* const sum343,
                       uint16x8_t* const sum444) {
  uint8x8_t left, middle, right;
  Prepare3(a, &left, &middle, &right);
  const uint16x8_t sum = Sum3W(left, middle, right);
  const uint16x8_t sum3 = Sum3(sum, sum, sum);
  *sum343 = vaddw_u8(sum3, middle);
  *sum444 = vshlq_n_u16(sum, 2);
}

inline uint32x4x2_t Sum343W(const uint16x8_t a[2]) {
  uint16x8_t left, middle, right;
  uint32x4x2_t d;
  Prepare3(a, &left, &middle, &right);
  d.val[0] =
      Sum3W(vget_low_u16(left), vget_low_u16(middle), vget_low_u16(right));
  d.val[1] =
      Sum3W(vget_high_u16(left), vget_high_u16(middle), vget_high_u16(right));
  d.val[0] = Sum3(d.val[0], d.val[0], d.val[0]);
  d.val[1] = Sum3(d.val[1], d.val[1], d.val[1]);
  d.val[0] = vaddw_u16(d.val[0], vget_low_u16(middle));
  d.val[1] = vaddw_u16(d.val[1], vget_high_u16(middle));
  return d;
}

inline void Sum343_444W(const uint16x8_t a[2], uint32x4x2_t* const sum343,
                        uint32x4x2_t* const sum444) {
  uint16x8_t left, middle, right;
  Prepare3(a, &left, &middle, &right);
  sum444->val[0] =
      Sum3W(vget_low_u16(left), vget_low_u16(middle), vget_low_u16(right));
  sum444->val[1] =
      Sum3W(vget_high_u16(left), vget_high_u16(middle), vget_high_u16(right));
  sum343->val[0] = Sum3(sum444->val[0], sum444->val[0], sum444->val[0]);
  sum343->val[1] = Sum3(sum444->val[1], sum444->val[1], sum444->val[1]);
  sum343->val[0] = vaddw_u16(sum343->val[0], vget_low_u16(middle));
  sum343->val[1] = vaddw_u16(sum343->val[1], vget_high_u16(middle));
  sum444->val[0] = vshlq_n_u32(sum444->val[0], 2);
  sum444->val[1] = vshlq_n_u32(sum444->val[1], 2);
}

inline uint16x8_t Sum565(const uint8x8_t a[2]) {
  uint8x8_t left, middle, right;
  Prepare3(a, &left, &middle, &right);
  const uint16x8_t sum = Sum3W(left, middle, right);
  const uint16x8_t sum4 = vshlq_n_u16(sum, 2);
  const uint16x8_t sum5 = vaddq_u16(sum4, sum);
  return vaddw_u8(sum5, middle);
}

inline uint32x4_t Sum565W(const uint16x8_t a) {
  const uint16x4_t left = vget_low_u16(a);
  const uint16x4_t middle = vext_u16(left, vget_high_u16(a), 1);
  const uint16x4_t right = vext_u16(left, vget_high_u16(a), 2);
  const uint32x4_t sum = Sum3W(left, middle, right);
  const uint32x4_t sum4 = vshlq_n_u32(sum, 2);
  const uint32x4_t sum5 = vaddq_u32(sum4, sum);
  return vaddw_u16(sum5, middle);
}

// RightShiftWithRounding(
//   (a * src_ptr[x] + b), kSgrProjSgrBits + shift - kSgrProjRestoreBits);
template <int shift>
inline uint16x4_t FilterOutput(const uint16x4_t src, const uint16x4_t a,
                               const uint32x4_t b) {
  // a: 256 * 32 = 8192 (14 bits)
  // b: 65088 * 32 = 2082816 (21 bits)
  const uint32x4_t axsrc = vmull_u16(a, src);
  // v: 8192 * 255 + 2082816 = 4171876 (22 bits)
  const uint32x4_t v = vaddq_u32(axsrc, b);

  // kSgrProjSgrBits = 8
  // kSgrProjRestoreBits = 4
  // shift = 4 or 5
  // v >> 8 or 9
  // 22 bits >> 8 = 14 bits
  return vrshrn_n_u32(v, kSgrProjSgrBits + shift - kSgrProjRestoreBits);
}

template <int shift>
inline void CalculateFilteredOutput(const uint8x8_t src, const uint16x8_t a,
                                    const uint32x4x2_t b, uint16_t* const dst) {
  const uint16x8_t src_u16 = vmovl_u8(src);
  const uint16x4_t dst_lo =
      FilterOutput<shift>(vget_low_u16(src_u16), vget_low_u16(a), b.val[0]);
  const uint16x4_t dst_hi =
      FilterOutput<shift>(vget_high_u16(src_u16), vget_high_u16(a), b.val[1]);
  const uint16x8_t d = vcombine_u16(dst_lo, dst_hi);
  vst1q_u16(dst, d);
}

inline void BoxFilter1(const uint8x8_t src_u8, const uint8x8_t a2[2],
                       const uint16x8_t b2[2], uint16x8_t sum565_a[2],
                       uint32x4x2_t sum565_b[2], uint16_t* const out_buf) {
  uint32x4x2_t b_v;
  sum565_a[1] = Sum565(a2);
  sum565_a[1] = vsubq_u16(vdupq_n_u16((5 + 6 + 5) * 256), sum565_a[1]);
  sum565_b[1].val[0] =
      Sum565W(vcombine_u16(vget_high_u16(b2[0]), vget_low_u16(b2[1])));
  sum565_b[1].val[1] = Sum565W(b2[1]);

  uint16x8_t a_v = vaddq_u16(sum565_a[0], sum565_a[1]);
  b_v.val[0] = vaddq_u32(sum565_b[0].val[0], sum565_b[1].val[0]);
  b_v.val[1] = vaddq_u32(sum565_b[0].val[1], sum565_b[1].val[1]);
  CalculateFilteredOutput<5>(src_u8, a_v, b_v, out_buf);
}

inline void BoxFilter2(const uint8x8_t src_u8, const uint8x8_t a2[2],
                       const uint16x8_t b2[2], uint16x8_t sum343_a[4],
                       uint16x8_t sum444_a[3], uint32x4x2_t sum343_b[4],
                       uint32x4x2_t sum444_b[3], uint16_t* const out_buf) {
  uint32x4x2_t b_v;
  Sum343_444(a2, &sum343_a[2], &sum444_a[1]);
  sum343_a[2] = vsubq_u16(vdupq_n_u16((3 + 4 + 3) * 256), sum343_a[2]);
  sum444_a[1] = vsubq_u16(vdupq_n_u16((4 + 4 + 4) * 256), sum444_a[1]);
  uint16x8_t a_v = Sum3(sum343_a[0], sum444_a[0], sum343_a[2]);
  Sum343_444W(b2, &sum343_b[2], &sum444_b[1]);
  b_v.val[0] = Sum3(sum343_b[0].val[0], sum444_b[0].val[0], sum343_b[2].val[0]);
  b_v.val[1] = Sum3(sum343_b[0].val[1], sum444_b[0].val[1], sum343_b[2].val[1]);
  CalculateFilteredOutput<5>(src_u8, a_v, b_v, out_buf);
}

inline void BoxFilterProcess(const uint8_t* const src, const ptrdiff_t stride,
                             const int width, const int height,
                             const uint16_t s[2],
                             uint16_t* const box_filter_process_output,
                             uint16_t* const temp) {
  // We have combined PreProcess and Process for the first pass by storing
  // intermediate values in the |a2| region. The values stored are one vertical
  // column of interleaved |a2| and |b2| values and consume 8 * |height| values.
  // This is |height| and not |height| * 2 because PreProcess only generates
  // output for every other row. When processing the next column we write the
  // new scratch values right after reading the previously saved ones.

  // The PreProcess phase calculates a 5x5 box sum for every other row
  //
  // PreProcess and Process have been combined into the same step. We need 12
  // input values to generate 8 output values for PreProcess:
  // 0 1 2 3 4 5 6 7 8 9 10 11
  // 2 = 0 + 1 + 2 +  3 +  4
  // 3 = 1 + 2 + 3 +  4 +  5
  // 4 = 2 + 3 + 4 +  5 +  6
  // 5 = 3 + 4 + 5 +  6 +  7
  // 6 = 4 + 5 + 6 +  7 +  8
  // 7 = 5 + 6 + 7 +  8 +  9
  // 8 = 6 + 7 + 8 +  9 + 10
  // 9 = 7 + 8 + 9 + 10 + 11
  //
  // and then we need 10 input values to generate 8 output values for Process:
  // 0 1 2 3 4 5 6 7 8 9
  // 1 = 0 + 1 + 2
  // 2 = 1 + 2 + 3
  // 3 = 2 + 3 + 4
  // 4 = 3 + 4 + 5
  // 5 = 4 + 5 + 6
  // 6 = 5 + 6 + 7
  // 7 = 6 + 7 + 8
  // 8 = 7 + 8 + 9
  //
  // To avoid re-calculating PreProcess values over and over again we will do a
  // single column of 8 output values and store the second half of them
  // interleaved in |temp|. The first half is not stored, since it is used
  // immediately and becomes useless for the next column. Next we will start the
  // second column. When 2 rows have been calculated we can calculate Process
  // and output those into the top of |box_filter_process_output|.

  // Calculate and store a single column. Scope so we can re-use the variable
  // names for the next step.
  uint16_t* ab_ptr = temp;

  // The first phase needs a radius of 2 context values. The second phase
  // needs a context of radius 1 values. This means we start at (-3, -3).
  const uint8_t* const src_pre_process = src - 3 - 3 * stride;
  // Calculate intermediate results, including two-pixel border, for example,
  // if unit size is 64x64, we calculate 68x68 pixels.
  {
    const uint8_t* column = src_pre_process;
    uint8x8_t row[5];
    uint16x8_t row_sq[5];

    row[0] = vld1_u8(column);
    column += stride;
    row[1] = vld1_u8(column);
    column += stride;
    row[2] = vld1_u8(column);

    row_sq[0] = vmull_u8(row[0], row[0]);
    row_sq[1] = vmull_u8(row[1], row[1]);
    row_sq[2] = vmull_u8(row[2], row[2]);

    int y = 0;
    do {
      column += stride;
      row[3] = vld1_u8(column);
      column += stride;
      row[4] = vld1_u8(column);

      row_sq[3] = vmull_u8(row[3], row[3]);
      row_sq[4] = vmull_u8(row[4], row[4]);

      PreProcess4<5, 0>(row + 0, row_sq + 0, s[0], ab_ptr + 0);
      PreProcess4<3, 1>(row + 1, row_sq + 1, s[1], ab_ptr + 8);
      PreProcess4<3, 1>(row + 2, row_sq + 2, s[1], ab_ptr + 16);

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0] = row_sq[2];
      row_sq[1] = row_sq[3];
      row_sq[2] = row_sq[4];
      ab_ptr += 24;
      y += 2;
    } while (y < height + 2);
  }

  int x = 0;
  do {
    // |src_pre_process| is X but we already processed the first column of 4
    // values so we want to start at Y and increment from there.
    // X s s s Y s s
    // s s s s s s s
    // s s i i i i i
    // s s i o o o o
    // s s i o o o o

    // Seed the loop with one line of output. Then, inside the loop, for each
    // iteration we can output one even row and one odd row and carry the new
    // line to the next iteration. In the diagram below 'i' values are
    // intermediary values from the first step and '-' values are empty.
    // iiii
    // ---- > even row
    // iiii - odd row
    // ---- > even row
    // iiii
    uint8x8_t a2[2][2];
    uint16x8_t b2[2][2], sum565_a[2], sum343_a[4], sum444_a[3];
    uint32x4x2_t sum565_b[2], sum343_b[4], sum444_b[3];
    ab_ptr = temp;
    b2[0][0] = vld1q_u16(ab_ptr);
    a2[0][0] = vget_low_u8(vreinterpretq_u8_u16(b2[0][0]));
    b2[1][0] = vld1q_u16(ab_ptr + 8);
    a2[1][0] = vget_low_u8(vreinterpretq_u8_u16(b2[1][0]));

    const uint8_t* column = src_pre_process + x + 4;
    uint8x16_t row[5];
    uint16x8x2_t row_sq[5];

    row[0] = vld1q_u8(column);
    column += stride;
    row[1] = vld1q_u8(column);
    column += stride;
    row[2] = vld1q_u8(column);
    column += stride;
    row[3] = vld1q_u8(column);
    column += stride;
    row[4] = vld1q_u8(column);

    row_sq[0].val[0] = vmull_u8(vget_low_u8(row[0]), vget_low_u8(row[0]));
    row_sq[0].val[1] = vmull_u8(vget_high_u8(row[0]), vget_high_u8(row[0]));
    row_sq[1].val[0] = vmull_u8(vget_low_u8(row[1]), vget_low_u8(row[1]));
    row_sq[1].val[1] = vmull_u8(vget_high_u8(row[1]), vget_high_u8(row[1]));
    row_sq[2].val[0] = vmull_u8(vget_low_u8(row[2]), vget_low_u8(row[2]));
    row_sq[2].val[1] = vmull_u8(vget_high_u8(row[2]), vget_high_u8(row[2]));
    row_sq[3].val[0] = vmull_u8(vget_low_u8(row[3]), vget_low_u8(row[3]));
    row_sq[3].val[1] = vmull_u8(vget_high_u8(row[3]), vget_high_u8(row[3]));
    row_sq[4].val[0] = vmull_u8(vget_low_u8(row[4]), vget_low_u8(row[4]));
    row_sq[4].val[1] = vmull_u8(vget_high_u8(row[4]), vget_high_u8(row[4]));

    PreProcess8<5, 0>(row, row_sq, s[0], &a2[0][1], &b2[0][1], ab_ptr);
    PreProcess8<3, 1>(row + 1, row_sq + 1, s[1], &a2[1][1], &b2[1][1],
                      ab_ptr + 8);

    // Pass 1 Process. These are the only values we need to propagate between
    // rows.
    sum565_a[0] = Sum565(a2[0]);
    sum565_a[0] = vsubq_u16(vdupq_n_u16((5 + 6 + 5) * 256), sum565_a[0]);
    sum565_b[0].val[0] =
        Sum565W(vcombine_u16(vget_high_u16(b2[0][0]), vget_low_u16(b2[0][1])));
    sum565_b[0].val[1] = Sum565W(b2[0][1]);

    const uint8_t* src_ptr = src + x;
    uint16_t* out_buf = box_filter_process_output + 2 * x;

    sum343_a[0] = Sum343(a2[1]);
    sum343_a[0] = vsubq_u16(vdupq_n_u16((3 + 4 + 3) * 256), sum343_a[0]);
    sum343_b[0] = Sum343W(b2[1]);

    b2[1][0] = vld1q_u16(ab_ptr + 16);
    a2[1][0] = vget_low_u8(vreinterpretq_u8_u16(b2[1][0]));

    PreProcess8<3, 1>(row + 2, row_sq + 2, s[1], &a2[1][1], &b2[1][1],
                      ab_ptr + 16);

    Sum343_444(a2[1], &sum343_a[1], &sum444_a[0]);
    sum343_a[1] = vsubq_u16(vdupq_n_u16((3 + 4 + 3) * 256), sum343_a[1]);
    sum444_a[0] = vsubq_u16(vdupq_n_u16((4 + 4 + 4) * 256), sum444_a[0]);
    Sum343_444W(b2[1], &sum343_b[1], &sum444_b[0]);

    // Calculate one output line. Add in the line from the previous pass and
    // output one even row. Sum the new line and output the odd row. Carry the
    // new row into the next pass.
    int y = 0;
    do {
      ab_ptr += 24;
      b2[0][0] = vld1q_u16(ab_ptr);
      a2[0][0] = vget_low_u8(vreinterpretq_u8_u16(b2[0][0]));
      b2[1][0] = vld1q_u16(ab_ptr + 8);
      a2[1][0] = vget_low_u8(vreinterpretq_u8_u16(b2[1][0]));

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0] = row_sq[2];
      row_sq[1] = row_sq[3];
      row_sq[2] = row_sq[4];

      column += stride;
      row[3] = vld1q_u8(column);
      column += stride;
      row[4] = vld1q_u8(column);

      row_sq[3].val[0] = vmull_u8(vget_low_u8(row[3]), vget_low_u8(row[3]));
      row_sq[3].val[1] = vmull_u8(vget_high_u8(row[3]), vget_high_u8(row[3]));
      row_sq[4].val[0] = vmull_u8(vget_low_u8(row[4]), vget_low_u8(row[4]));
      row_sq[4].val[1] = vmull_u8(vget_high_u8(row[4]), vget_high_u8(row[4]));

      PreProcess8<5, 0>(row, row_sq, s[0], &a2[0][1], &b2[0][1], ab_ptr);
      PreProcess8<3, 1>(row + 1, row_sq + 1, s[1], &a2[1][1], &b2[1][1],
                        ab_ptr + 8);

      uint8x8_t src_u8 = vld1_u8(src_ptr);
      BoxFilter1(src_u8, a2[0], b2[0], sum565_a, sum565_b, out_buf);
      BoxFilter2(src_u8, a2[1], b2[1], sum343_a, sum444_a, sum343_b, sum444_b,
                 out_buf + 8);
      src_ptr += stride;
      out_buf += 2 * kRestorationProcessingUnitSize;

      src_u8 = vld1_u8(src_ptr);
      CalculateFilteredOutput<4>(src_u8, sum565_a[1], sum565_b[1], out_buf);
      b2[1][0] = vld1q_u16(ab_ptr + 16);
      a2[1][0] = vget_low_u8(vreinterpretq_u8_u16(b2[1][0]));
      PreProcess8<3, 1>(row + 2, row_sq + 2, s[1], &a2[1][1], &b2[1][1],
                        ab_ptr + 16);

      BoxFilter2(src_u8, a2[1], b2[1], sum343_a + 1, sum444_a + 1, sum343_b + 1,
                 sum444_b + 1, out_buf + 8);
      src_ptr += stride;
      out_buf += 2 * kRestorationProcessingUnitSize;

      sum565_a[0] = sum565_a[1];
      sum565_b[0] = sum565_b[1];
      sum343_a[0] = sum343_a[2];
      sum343_a[1] = sum343_a[3];
      sum444_a[0] = sum444_a[2];
      sum343_b[0] = sum343_b[2];
      sum343_b[1] = sum343_b[3];
      sum444_b[0] = sum444_b[2];

      y += 2;
    } while (y < height);
    x += 8;
  } while (x < width);
}

inline void BoxFilterProcess_FirstPass(
    const uint8_t* const src, const ptrdiff_t stride, const int width,
    const int height, const uint32_t s,
    uint16_t* const box_filter_process_output, uint16_t* const temp) {
  // We have combined PreProcess and Process for the first pass by storing
  // intermediate values in the |a2| region. The values stored are one vertical
  // column of interleaved |a2| and |b2| values and consume 8 * |height| values.
  // This is |height| and not |height| * 2 because PreProcess only generates
  // output for every other row. When processing the next column we write the
  // new scratch values right after reading the previously saved ones.

  // The PreProcess phase calculates a 5x5 box sum for every other row
  //
  // PreProcess and Process have been combined into the same step. We need 12
  // input values to generate 8 output values for PreProcess:
  // 0 1 2 3 4 5 6 7 8 9 10 11
  // 2 = 0 + 1 + 2 +  3 +  4
  // 3 = 1 + 2 + 3 +  4 +  5
  // 4 = 2 + 3 + 4 +  5 +  6
  // 5 = 3 + 4 + 5 +  6 +  7
  // 6 = 4 + 5 + 6 +  7 +  8
  // 7 = 5 + 6 + 7 +  8 +  9
  // 8 = 6 + 7 + 8 +  9 + 10
  // 9 = 7 + 8 + 9 + 10 + 11
  //
  // and then we need 10 input values to generate 8 output values for Process:
  // 0 1 2 3 4 5 6 7 8 9
  // 1 = 0 + 1 + 2
  // 2 = 1 + 2 + 3
  // 3 = 2 + 3 + 4
  // 4 = 3 + 4 + 5
  // 5 = 4 + 5 + 6
  // 6 = 5 + 6 + 7
  // 7 = 6 + 7 + 8
  // 8 = 7 + 8 + 9
  //
  // To avoid re-calculating PreProcess values over and over again we will do a
  // single column of 8 output values and store the second half of them
  // interleaved in |temp|. The first half is not stored, since it is used
  // immediately and becomes useless for the next column. Next we will start the
  // second column. When 2 rows have been calculated we can calculate Process
  // and output those into the top of |box_filter_process_output|.

  // Calculate and store a single column. Scope so we can re-use the variable
  // names for the next step.
  uint16_t* ab_ptr = temp;

  // The first phase needs a radius of 2 context values. The second phase
  // needs a context of radius 1 values. This means we start at (-3, -3).
  const uint8_t* const src_pre_process = src - 3 - 3 * stride;
  // Calculate intermediate results, including two-pixel border, for example,
  // if unit size is 64x64, we calculate 68x68 pixels.
  {
    const uint8_t* column = src_pre_process;
    uint8x8_t row[5];
    uint16x8_t row_sq[5];

    row[0] = vld1_u8(column);
    column += stride;
    row[1] = vld1_u8(column);
    column += stride;
    row[2] = vld1_u8(column);

    row_sq[0] = vmull_u8(row[0], row[0]);
    row_sq[1] = vmull_u8(row[1], row[1]);
    row_sq[2] = vmull_u8(row[2], row[2]);

    int y = 0;
    do {
      column += stride;
      row[3] = vld1_u8(column);
      column += stride;
      row[4] = vld1_u8(column);

      row_sq[3] = vmull_u8(row[3], row[3]);
      row_sq[4] = vmull_u8(row[4], row[4]);

      PreProcess4<5, 0>(row, row_sq, s, ab_ptr);

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0] = row_sq[2];
      row_sq[1] = row_sq[3];
      row_sq[2] = row_sq[4];
      ab_ptr += 8;
      y += 2;
    } while (y < height + 2);
  }

  int x = 0;
  do {
    // |src_pre_process| is X but we already processed the first column of 4
    // values so we want to start at Y and increment from there.
    // X s s s Y s s
    // s s s s s s s
    // s s i i i i i
    // s s i o o o o
    // s s i o o o o

    // Seed the loop with one line of output. Then, inside the loop, for each
    // iteration we can output one even row and one odd row and carry the new
    // line to the next iteration. In the diagram below 'i' values are
    // intermediary values from the first step and '-' values are empty.
    // iiii
    // ---- > even row
    // iiii - odd row
    // ---- > even row
    // iiii
    uint8x8_t a2[2];
    uint16x8_t b2[2], sum565_a[2];
    uint32x4x2_t sum565_b[2];
    ab_ptr = temp;
    b2[0] = vld1q_u16(ab_ptr);
    a2[0] = vget_low_u8(vreinterpretq_u8_u16(b2[0]));

    const uint8_t* column = src_pre_process + x + 4;
    uint8x16_t row[5];
    uint16x8x2_t row_sq[5];

    row[0] = vld1q_u8(column);
    column += stride;
    row[1] = vld1q_u8(column);
    column += stride;
    row[2] = vld1q_u8(column);
    column += stride;
    row[3] = vld1q_u8(column);
    column += stride;
    row[4] = vld1q_u8(column);

    row_sq[0].val[0] = vmull_u8(vget_low_u8(row[0]), vget_low_u8(row[0]));
    row_sq[0].val[1] = vmull_u8(vget_high_u8(row[0]), vget_high_u8(row[0]));
    row_sq[1].val[0] = vmull_u8(vget_low_u8(row[1]), vget_low_u8(row[1]));
    row_sq[1].val[1] = vmull_u8(vget_high_u8(row[1]), vget_high_u8(row[1]));
    row_sq[2].val[0] = vmull_u8(vget_low_u8(row[2]), vget_low_u8(row[2]));
    row_sq[2].val[1] = vmull_u8(vget_high_u8(row[2]), vget_high_u8(row[2]));
    row_sq[3].val[0] = vmull_u8(vget_low_u8(row[3]), vget_low_u8(row[3]));
    row_sq[3].val[1] = vmull_u8(vget_high_u8(row[3]), vget_high_u8(row[3]));
    row_sq[4].val[0] = vmull_u8(vget_low_u8(row[4]), vget_low_u8(row[4]));
    row_sq[4].val[1] = vmull_u8(vget_high_u8(row[4]), vget_high_u8(row[4]));

    PreProcess8<5, 0>(row, row_sq, s, &a2[1], &b2[1], ab_ptr);

    // Pass 1 Process. These are the only values we need to propagate between
    // rows.
    sum565_a[0] = Sum565(a2);
    sum565_a[0] = vsubq_u16(vdupq_n_u16((5 + 6 + 5) * 256), sum565_a[0]);
    sum565_b[0].val[0] =
        Sum565W(vcombine_u16(vget_high_u16(b2[0]), vget_low_u16(b2[1])));
    sum565_b[0].val[1] = Sum565W(b2[1]);

    const uint8_t* src_ptr = src + x;
    uint16_t* out_buf = box_filter_process_output + x;

    // Calculate one output line. Add in the line from the previous pass and
    // output one even row. Sum the new line and output the odd row. Carry the
    // new row into the next pass.
    int y = 0;
    do {
      ab_ptr += 8;
      b2[0] = vld1q_u16(ab_ptr);
      a2[0] = vget_low_u8(vreinterpretq_u8_u16(b2[0]));

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0] = row_sq[2];
      row_sq[1] = row_sq[3];
      row_sq[2] = row_sq[4];

      column += stride;
      row[3] = vld1q_u8(column);
      column += stride;
      row[4] = vld1q_u8(column);

      row_sq[3].val[0] = vmull_u8(vget_low_u8(row[3]), vget_low_u8(row[3]));
      row_sq[3].val[1] = vmull_u8(vget_high_u8(row[3]), vget_high_u8(row[3]));
      row_sq[4].val[0] = vmull_u8(vget_low_u8(row[4]), vget_low_u8(row[4]));
      row_sq[4].val[1] = vmull_u8(vget_high_u8(row[4]), vget_high_u8(row[4]));

      PreProcess8<5, 0>(row, row_sq, s, &a2[1], &b2[1], ab_ptr);

      uint8x8_t src_u8 = vld1_u8(src_ptr);
      BoxFilter1(src_u8, a2, b2, sum565_a, sum565_b, out_buf);
      src_ptr += stride;
      out_buf += kRestorationProcessingUnitSize;

      src_u8 = vld1_u8(src_ptr);
      CalculateFilteredOutput<4>(src_u8, sum565_a[1], sum565_b[1], out_buf);
      src_ptr += stride;
      out_buf += kRestorationProcessingUnitSize;

      sum565_a[0] = sum565_a[1];
      sum565_b[0] = sum565_b[1];
      y += 2;
    } while (y < height);
    x += 8;
  } while (x < width);
}

inline void BoxFilterProcess_SecondPass(
    const uint8_t* src, const ptrdiff_t stride, const int width,
    const int height, const uint32_t s,
    uint16_t* const box_filter_process_output, uint16_t* const temp) {
  uint16_t* ab_ptr = temp;

  // Calculate intermediate results, including one-pixel border, for example,
  // if unit size is 64x64, we calculate 66x66 pixels.
  // Because of the vectors this calculates start in blocks of 4 so we actually
  // get 68 values.
  const uint8_t* const src_top_left_corner = src - 2 - 2 * stride;
  {
    const uint8_t* column = src_top_left_corner;
    uint8x8_t row[3];
    uint16x8_t row_sq[3];

    row[0] = vld1_u8(column);
    column += stride;
    row[1] = vld1_u8(column);
    row_sq[0] = vmull_u8(row[0], row[0]);
    row_sq[1] = vmull_u8(row[1], row[1]);

    int y = height + 2;
    do {
      column += stride;
      row[2] = vld1_u8(column);
      row_sq[2] = vmull_u8(row[2], row[2]);

      PreProcess4<3, 0>(row, row_sq, s, ab_ptr);

      row[0] = row[1];
      row[1] = row[2];

      row_sq[0] = row_sq[1];
      row_sq[1] = row_sq[2];
      ab_ptr += 8;
    } while (--y);
  }

  int x = 0;
  do {
    const uint8_t* src_ptr = src + x;
    uint16_t* out_buf = box_filter_process_output + x;
    ab_ptr = temp;

    uint8x8_t a2[2];
    uint16x8_t b2[2], sum343_a[3], sum444_a[2];
    uint32x4x2_t sum343_b[3], sum444_b[2];
    b2[0] = vld1q_u16(ab_ptr);
    a2[0] = vget_low_u8(vreinterpretq_u8_u16(b2[0]));

    const uint8_t* column = src_top_left_corner + x + 4;
    uint8x16_t row[3];
    uint16x8x2_t row_sq[3];
    row[0] = vld1q_u8(column);
    column += stride;
    row[1] = vld1q_u8(column);
    column += stride;
    row[2] = vld1q_u8(column);

    row_sq[0].val[0] = vmull_u8(vget_low_u8(row[0]), vget_low_u8(row[0]));
    row_sq[0].val[1] = vmull_u8(vget_high_u8(row[0]), vget_high_u8(row[0]));
    row_sq[1].val[0] = vmull_u8(vget_low_u8(row[1]), vget_low_u8(row[1]));
    row_sq[1].val[1] = vmull_u8(vget_high_u8(row[1]), vget_high_u8(row[1]));
    row_sq[2].val[0] = vmull_u8(vget_low_u8(row[2]), vget_low_u8(row[2]));
    row_sq[2].val[1] = vmull_u8(vget_high_u8(row[2]), vget_high_u8(row[2]));

    PreProcess8<3, 0>(row, row_sq, s, &a2[1], &b2[1], ab_ptr);

    sum343_a[0] = Sum343(a2);
    sum343_a[0] = vsubq_u16(vdupq_n_u16((3 + 4 + 3) * 256), sum343_a[0]);
    sum343_b[0] = Sum343W(b2);

    ab_ptr += 8;
    b2[0] = vld1q_u16(ab_ptr);
    a2[0] = vget_low_u8(vreinterpretq_u8_u16(b2[0]));

    row[0] = row[1];
    row[1] = row[2];

    row_sq[0] = row_sq[1];
    row_sq[1] = row_sq[2];
    column += stride;
    row[2] = vld1q_u8(column);

    row_sq[2].val[0] = vmull_u8(vget_low_u8(row[2]), vget_low_u8(row[2]));
    row_sq[2].val[1] = vmull_u8(vget_high_u8(row[2]), vget_high_u8(row[2]));

    PreProcess8<3, 0>(row, row_sq, s, &a2[1], &b2[1], ab_ptr);

    Sum343_444(a2, &sum343_a[1], &sum444_a[0]);
    sum343_a[1] = vsubq_u16(vdupq_n_u16((3 + 4 + 3) * 256), sum343_a[1]);
    sum444_a[0] = vsubq_u16(vdupq_n_u16((4 + 4 + 4) * 256), sum444_a[0]);
    Sum343_444W(b2, &sum343_b[1], &sum444_b[0]);

    int y = height;
    do {
      ab_ptr += 8;
      b2[0] = vld1q_u16(ab_ptr);
      a2[0] = vget_low_u8(vreinterpretq_u8_u16(b2[0]));

      row[0] = row[1];
      row[1] = row[2];

      row_sq[0] = row_sq[1];
      row_sq[1] = row_sq[2];
      column += stride;
      row[2] = vld1q_u8(column);

      row_sq[2].val[0] = vmull_u8(vget_low_u8(row[2]), vget_low_u8(row[2]));
      row_sq[2].val[1] = vmull_u8(vget_high_u8(row[2]), vget_high_u8(row[2]));

      PreProcess8<3, 0>(row, row_sq, s, &a2[1], &b2[1], ab_ptr);

      uint8x8_t src_u8 = vld1_u8(src_ptr);
      BoxFilter2(src_u8, a2, b2, sum343_a, sum444_a, sum343_b, sum444_b,
                 out_buf);
      sum343_a[0] = sum343_a[1];
      sum343_a[1] = sum343_a[2];
      sum444_a[0] = sum444_a[1];
      sum343_b[0] = sum343_b[1];
      sum343_b[1] = sum343_b[2];
      sum444_b[0] = sum444_b[1];
      src_ptr += stride;
      out_buf += kRestorationProcessingUnitSize;
    } while (--y);
    x += 8;
  } while (x < width);
}

inline void SelfGuidedSingleMultiplier(
    const uint8_t* src, const ptrdiff_t src_stride,
    const uint16_t* const box_filter_process_output, uint8_t* dst,
    const ptrdiff_t dst_stride, const int width, const int height,
    const int16_t w_single) {
  const int16_t w_combo = (1 << kSgrProjPrecisionBits) - w_single;
  const auto* box_filter =
      reinterpret_cast<const int16_t*>(box_filter_process_output);
  int w = width;

  if (w & 4) {
    w -= 4;
    const uint8_t* src_ptr = src + w;
    uint8_t* dst_ptr = dst + w;
    const int16_t* box_filter_w = box_filter + w;
    int y = height;
    do {
      const int16x8_t u = vreinterpretq_s16_u16(
          vshll_n_u8(vld1_u8(src_ptr), kSgrProjRestoreBits));
      const int16x4_t p = vld1_s16(box_filter_w);
      // u * w1 + u * wN == u * (w1 + wN)
      int32x4_t v_lo = vmull_n_s16(vget_low_s16(u), w_combo);
      v_lo = vmlal_n_s16(v_lo, p, w_single);
      const int16x4_t s_lo =
          vrshrn_n_s32(v_lo, kSgrProjRestoreBits + kSgrProjPrecisionBits);
      StoreLo4(dst_ptr, vqmovun_s16(vcombine_s16(s_lo, s_lo)));
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      box_filter_w += kRestorationProcessingUnitSize;
    } while (--y);

    if (!w) return;
  }

  int y = height;
  do {
    int x = 0;
    do {
      const int16x8_t u = vreinterpretq_s16_u16(
          vshll_n_u8(vld1_u8(src + x), kSgrProjRestoreBits));
      const int16x8_t p = vld1q_s16(box_filter + x);
      // u * w1 + u * wN == u * (w1 + wN)
      int32x4_t v_lo = vmull_n_s16(vget_low_s16(u), w_combo);
      v_lo = vmlal_n_s16(v_lo, vget_low_s16(p), w_single);
      int32x4_t v_hi = vmull_n_s16(vget_high_s16(u), w_combo);
      v_hi = vmlal_n_s16(v_hi, vget_high_s16(p), w_single);
      const int16x4_t s_lo =
          vrshrn_n_s32(v_lo, kSgrProjRestoreBits + kSgrProjPrecisionBits);
      const int16x4_t s_hi =
          vrshrn_n_s32(v_hi, kSgrProjRestoreBits + kSgrProjPrecisionBits);
      vst1_u8(dst + x, vqmovun_s16(vcombine_s16(s_lo, s_hi)));
      x += 8;
    } while (x < w);
    src += src_stride;
    dst += dst_stride;
    box_filter += kRestorationProcessingUnitSize;
  } while (--y);
}

inline void SelfGuidedDoubleMultiplier(
    const uint8_t* src, const ptrdiff_t src_stride,
    const uint16_t* const box_filter_process_output, uint8_t* dst,
    const ptrdiff_t dst_stride, const int width, const int height, const int w0,
    const int w1, const int w2) {
  const auto* box_filter =
      reinterpret_cast<const int16_t*>(box_filter_process_output);
  const int16x4_t w0_v = vdup_n_s16(w0);
  const int16x4_t w1_v = vdup_n_s16(w1);
  const int16x4_t w2_v = vdup_n_s16(w2);
  int w = width;

  if (w & 4) {
    w -= 4;
    const uint8_t* src_ptr = src + w;
    uint8_t* dst_ptr = dst + w;
    const int16_t* box_filter_w = box_filter + 2 * w;
    int y = height;
    do {
      // |wN| values are signed. |src| values can be treated as int16_t.
      // Load 8 values but ignore 4.
      const int16x4_t u = vget_low_s16(vreinterpretq_s16_u16(
          vshll_n_u8(vld1_u8(src_ptr), kSgrProjRestoreBits)));
      // |box_filter_process_output| is 14 bits, also safe to treat as int16_t.
      const int16x4_t p0 = vld1_s16(box_filter_w + 0);
      const int16x4_t p1 = vld1_s16(box_filter_w + 8);
      int32x4_t v = vmull_s16(u, w1_v);
      v = vmlal_s16(v, p0, w0_v);
      v = vmlal_s16(v, p1, w2_v);
      // |s| is saturated to uint8_t.
      const int16x4_t s =
          vrshrn_n_s32(v, kSgrProjRestoreBits + kSgrProjPrecisionBits);
      StoreLo4(dst_ptr, vqmovun_s16(vcombine_s16(s, s)));
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      box_filter_w += 2 * kRestorationProcessingUnitSize;
    } while (--y);

    if (!w) return;
  }

  int y = height;
  do {
    int x = 0;
    do {
      // |wN| values are signed. |src| values can be treated as int16_t.
      const int16x8_t u = vreinterpretq_s16_u16(
          vshll_n_u8(vld1_u8(src + x), kSgrProjRestoreBits));
      // |box_filter_process_output| is 14 bits, also safe to treat as int16_t.
      const int16x8_t p0 = vld1q_s16(box_filter + 2 * x + 0);
      const int16x8_t p1 = vld1q_s16(box_filter + 2 * x + 8);
      int32x4_t v_lo = vmull_s16(vget_low_s16(u), w1_v);
      v_lo = vmlal_s16(v_lo, vget_low_s16(p0), w0_v);
      v_lo = vmlal_s16(v_lo, vget_low_s16(p1), w2_v);
      int32x4_t v_hi = vmull_s16(vget_high_s16(u), w1_v);
      v_hi = vmlal_s16(v_hi, vget_high_s16(p0), w0_v);
      v_hi = vmlal_s16(v_hi, vget_high_s16(p1), w2_v);
      // |s| is saturated to uint8_t.
      const int16x4_t s_lo =
          vrshrn_n_s32(v_lo, kSgrProjRestoreBits + kSgrProjPrecisionBits);
      const int16x4_t s_hi =
          vrshrn_n_s32(v_hi, kSgrProjRestoreBits + kSgrProjPrecisionBits);
      vst1_u8(dst + x, vqmovun_s16(vcombine_s16(s_lo, s_hi)));
      x += 8;
    } while (x < w);
    src += src_stride;
    dst += dst_stride;
    box_filter += 2 * kRestorationProcessingUnitSize;
  } while (--y);
}

void SelfGuidedFilter_NEON(const void* const source, void* const dest,
                           const RestorationUnitInfo& restoration_info,
                           ptrdiff_t source_stride, ptrdiff_t dest_stride,
                           const int width, const int height,
                           RestorationBuffer* const /*buffer*/) {
  const auto* src = static_cast<const uint8_t*>(source);

  // The output frame is broken into blocks of 64x64 (32x32 if U/V are
  // subsampled). If either dimension is less than 32/64 it indicates it is at
  // the right or bottom edge of the frame. It is safe to overwrite the output
  // as it will not be part of the visible frame. This saves us from having to
  // handle non-multiple-of-8 widths.
  // We could round here, but the for loop with += 8 does the same thing.

  // width = (width + 7) & ~0x7;

  // -96 to 96 (Sgrproj_Xqd_Min/Max)
  const int index = restoration_info.sgr_proj_info.index;
  const int radius_pass_0 = kSgrProjParams[index][0];
  const int radius_pass_1 = kSgrProjParams[index][2];
  alignas(kMaxAlignment)
      uint16_t box_filter_process_output[2 * kMaxBoxFilterProcessOutputPixels];
  alignas(kMaxAlignment)
      uint16_t temp[12 * (kRestorationProcessingUnitSize + 2)];

  // If |radius| is 0 then there is nothing to do. If |radius| is not 0, it is
  // always 2 for the first pass and 1 for the second pass.
  const int w0 = restoration_info.sgr_proj_info.multiplier[0];
  const int w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int w2 = (1 << kSgrProjPrecisionBits) - w0 - w1;
  auto* dst = static_cast<uint8_t*>(dest);
  // Note: Combining box filter process with the final multipliers has no speed
  // gain. There are not enough neon registers to hold those weights.
  if (radius_pass_0 != 0 && radius_pass_1 != 0) {
    BoxFilterProcess(src, source_stride, width, height,
                     kSgrScaleParameter[index], box_filter_process_output,
                     temp);
    SelfGuidedDoubleMultiplier(src, source_stride, box_filter_process_output,
                               dst, dest_stride, width, height, w0, w1, w2);
  } else {
    int16_t w_single;
    if (radius_pass_0 != 0) {
      BoxFilterProcess_FirstPass(src, source_stride, width, height,
                                 kSgrScaleParameter[index][0],
                                 box_filter_process_output, temp);
      w_single = w0;
    } else /* if (radius_pass_1 != 0) */ {
      BoxFilterProcess_SecondPass(src, source_stride, width, height,
                                  kSgrScaleParameter[index][1],
                                  box_filter_process_output, temp);
      w_single = w2;
    }
    SelfGuidedSingleMultiplier(src, source_stride, box_filter_process_output,
                               dst, dest_stride, width, height, w_single);
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

#else   // !LIBGAV1_ENABLE_NEON
namespace libgav1 {
namespace dsp {

void LoopRestorationInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON
