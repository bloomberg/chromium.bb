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
#include "src/dsp/dsp.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

// Wiener
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

inline int16x8_t HorizontalSum(const uint8x8_t a[7], int16_t filter[4]) {
  int16x8_t sum = vmulq_n_s16(vreinterpretq_s16_u16(vmovl_u8(a[0])), filter[0]);
  sum = vmlaq_n_s16(sum, vreinterpretq_s16_u16(vmovl_u8(a[1])), filter[1]);
  sum = vmlaq_n_s16(sum, vreinterpretq_s16_u16(vmovl_u8(a[2])), filter[2]);
  sum = vmlaq_n_s16(sum, vreinterpretq_s16_u16(vmovl_u8(a[3])), filter[3]);
  sum = vmlaq_n_s16(sum, vreinterpretq_s16_u16(vmovl_u8(a[4])), filter[2]);
  sum = vmlaq_n_s16(sum, vreinterpretq_s16_u16(vmovl_u8(a[5])), filter[1]);
  sum = vmlaq_n_s16(sum, vreinterpretq_s16_u16(vmovl_u8(a[6])), filter[0]);

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
  sum = vaddq_s16(sum, vreinterpretq_s16_u16(vshll_n_u8(a[3], 4)));

  // Saturate to
  // [0,
  // (1 << (bitdepth + 1 + kWienerFilterBits - kInterRoundBitsHorizontal)) - 1)]
  // (1 << (       8 + 1 +                 7 -                         3)) - 1)
  sum = vminq_s16(sum, vdupq_n_s16((1 << 13) - 1));
  sum = vmaxq_s16(sum, vdupq_n_s16(0));
  return sum;
}

template <int min_width>
inline void VerticalSum(const int16_t* src_base, const ptrdiff_t src_stride,
                        uint8_t* dst_base, const ptrdiff_t dst_stride,
                        const int16_t filter[4], const int width,
                        const int height) {
  static_assert(min_width == 4 || min_width == 8, "");
  // -(1 << (bitdepth + kInterRoundBitsVertical - 1))
  // -(1 << (       8 +                      11 - 1))
  constexpr int vertical_rounding = -(1 << 18);
  if (min_width == 8) {
    int x = 0;
    do {
      const int16_t* src = src_base + x;
      uint8_t* dst = dst_base + x;
      int16x8_t a[7];
      a[0] = vld1q_s16(src);
      src += src_stride;
      a[1] = vld1q_s16(src);
      src += src_stride;
      a[2] = vld1q_s16(src);
      src += src_stride;
      a[3] = vld1q_s16(src);
      src += src_stride;
      a[4] = vld1q_s16(src);
      src += src_stride;
      a[5] = vld1q_s16(src);
      src += src_stride;

      int y = 0;
      do {
        a[6] = vld1q_s16(src);
        src += src_stride;

        int32x4_t sum_lo = vdupq_n_s32(vertical_rounding);
        sum_lo = vmlal_n_s16(sum_lo, vget_low_s16(a[0]), filter[0]);
        sum_lo = vmlal_n_s16(sum_lo, vget_low_s16(a[1]), filter[1]);
        sum_lo = vmlal_n_s16(sum_lo, vget_low_s16(a[2]), filter[2]);
        sum_lo = vmlal_n_s16(sum_lo, vget_low_s16(a[3]), filter[3]);
        sum_lo = vmlal_n_s16(sum_lo, vget_low_s16(a[4]), filter[2]);
        sum_lo = vmlal_n_s16(sum_lo, vget_low_s16(a[5]), filter[1]);
        sum_lo = vmlal_n_s16(sum_lo, vget_low_s16(a[6]), filter[0]);
        uint16x4_t sum_lo_16 = vqrshrun_n_s32(sum_lo, 11);

        int32x4_t sum_hi = vdupq_n_s32(vertical_rounding);
        sum_hi = vmlal_n_s16(sum_hi, vget_high_s16(a[0]), filter[0]);
        sum_hi = vmlal_n_s16(sum_hi, vget_high_s16(a[1]), filter[1]);
        sum_hi = vmlal_n_s16(sum_hi, vget_high_s16(a[2]), filter[2]);
        sum_hi = vmlal_n_s16(sum_hi, vget_high_s16(a[3]), filter[3]);
        sum_hi = vmlal_n_s16(sum_hi, vget_high_s16(a[4]), filter[2]);
        sum_hi = vmlal_n_s16(sum_hi, vget_high_s16(a[5]), filter[1]);
        sum_hi = vmlal_n_s16(sum_hi, vget_high_s16(a[6]), filter[0]);
        uint16x4_t sum_hi_16 = vqrshrun_n_s32(sum_hi, 11);

        vst1_u8(dst, vqmovn_u16(vcombine_u16(sum_lo_16, sum_hi_16)));
        dst += dst_stride;

        a[0] = a[1];
        a[1] = a[2];
        a[2] = a[3];
        a[3] = a[4];
        a[4] = a[5];
        a[5] = a[6];
      } while (++y < height);
      x += 8;
    } while (x < width);
  } else if (min_width == 4) {
    const int16_t* src = src_base;
    uint8_t* dst = dst_base;
    int16x4_t a[7];
    a[0] = vld1_s16(src);
    src += src_stride;
    a[1] = vld1_s16(src);
    src += src_stride;
    a[2] = vld1_s16(src);
    src += src_stride;
    a[3] = vld1_s16(src);
    src += src_stride;
    a[4] = vld1_s16(src);
    src += src_stride;
    a[5] = vld1_s16(src);
    src += src_stride;

    int y = 0;
    do {
      a[6] = vld1_s16(src);
      src += src_stride;

      int32x4_t sum = vdupq_n_s32(vertical_rounding);
      sum = vmlal_n_s16(sum, a[0], filter[0]);
      sum = vmlal_n_s16(sum, a[1], filter[1]);
      sum = vmlal_n_s16(sum, a[2], filter[2]);
      sum = vmlal_n_s16(sum, a[3], filter[3]);
      sum = vmlal_n_s16(sum, a[4], filter[2]);
      sum = vmlal_n_s16(sum, a[5], filter[1]);
      sum = vmlal_n_s16(sum, a[6], filter[0]);
      uint16x4_t sum_16 = vqrshrun_n_s32(sum, 11);

      StoreLo4(dst, vqmovn_u16(vcombine_u16(sum_16, sum_16)));
      dst += dst_stride;

      a[0] = a[1];
      a[1] = a[2];
      a[2] = a[3];
      a[3] = a[4];
      a[4] = a[5];
      a[5] = a[6];
    } while (++y < height);
  }
}

void WienerFilter_NEON(const void* const source, void* const dest,
                       const RestorationUnitInfo& restoration_info,
                       const ptrdiff_t source_stride,
                       const ptrdiff_t dest_stride, const int width,
                       const int height, RestorationBuffer* const buffer) {
  const auto* src = static_cast<const uint8_t*>(source);
  auto* dst = static_cast<uint8_t*>(dest);
  // It should be possible to set this to |width|.
  ptrdiff_t buffer_stride = buffer->wiener_buffer_stride;
  // Casting once here saves a lot of vreinterpret() calls. The values are
  // saturated to 13 bits before storing.
  int16_t* wiener_buffer = reinterpret_cast<int16_t*>(buffer->wiener_buffer);

  // Horizontal filtering.
  int16_t filter[4];
  PopulateWienerCoefficients(restoration_info, WienerInfo::kHorizontal, filter);
  // The taps have a radius of 3. Adjust |src| so we start reading with the top
  // left value.
  const int center_tap = 3;
  src -= center_tap * source_stride + center_tap;
  int y = 0;
  do {
    int x = 0;
    do {
      // This is just as fast as an 8x8 transpose but avoids over-reading extra
      // rows. It always over-reads by at least 1 value. On small widths (4xH)
      // it over-reads by 9 values.
      const uint8x16_t src_v = vld1q_u8(src + x);
      uint8x8_t b[7];
      b[0] = vget_low_u8(src_v);
      b[1] = vget_low_u8(vextq_u8(src_v, src_v, 1));
      b[2] = vget_low_u8(vextq_u8(src_v, src_v, 2));
      b[3] = vget_low_u8(vextq_u8(src_v, src_v, 3));
      b[4] = vget_low_u8(vextq_u8(src_v, src_v, 4));
      b[5] = vget_low_u8(vextq_u8(src_v, src_v, 5));
      b[6] = vget_low_u8(vextq_u8(src_v, src_v, 6));

      int16x8_t sum = HorizontalSum(b, filter);

      vst1q_s16(wiener_buffer + x, sum);
      x += 8;
    } while (x < width);
    src += source_stride;
    wiener_buffer += buffer_stride;
  } while (++y < height + kSubPixelTaps - 2);

  // Vertical filtering.
  wiener_buffer = reinterpret_cast<int16_t*>(buffer->wiener_buffer);
  PopulateWienerCoefficients(restoration_info, WienerInfo::kVertical, filter);

  if (width == 4) {
    VerticalSum<4>(wiener_buffer, buffer_stride, dst, dest_stride, filter,
                   width, height);
  } else {
    VerticalSum<8>(wiener_buffer, buffer_stride, dst, dest_stride, filter,
                   width, height);
  }
}

// SGR
inline uint16x4_t Sum3Horizontal(const uint16x8_t a) {
  const uint16x4_t sum =
      vadd_u16(vget_low_u16(a), vext_u16(vget_low_u16(a), vget_high_u16(a), 1));
  return vadd_u16(sum, vext_u16(vget_low_u16(a), vget_high_u16(a), 2));
}

inline uint32x4_t Sum3Horizontal(const uint32x4x2_t a) {
  const uint32x4_t sum = vaddq_u32(a.val[0], vextq_u32(a.val[0], a.val[1], 1));
  return vaddq_u32(sum, vextq_u32(a.val[0], a.val[1], 2));
}

inline uint16x8_t Sum3Vertical(const uint8x8_t a[3]) {
  const uint16x8_t sum = vaddl_u8(a[0], a[1]);
  return vaddw_u8(sum, a[2]);
}

inline uint32x4x2_t Sum3Vertical(const uint16x8_t a[3]) {
  uint32x4_t sum_a = vaddl_u16(vget_low_u16(a[0]), vget_low_u16(a[1]));
  sum_a = vaddw_u16(sum_a, vget_low_u16(a[2]));
  uint32x4_t sum_b = vaddl_u16(vget_high_u16(a[0]), vget_high_u16(a[1]));
  sum_b = vaddw_u16(sum_b, vget_high_u16(a[2]));
  return {sum_a, sum_b};
}

inline uint16x4_t Sum5Horizontal(const uint16x8_t a) {
  uint16x4_t sum =
      vadd_u16(vget_low_u16(a), vext_u16(vget_low_u16(a), vget_high_u16(a), 1));
  sum = vadd_u16(sum, vext_u16(vget_low_u16(a), vget_high_u16(a), 2));
  sum = vadd_u16(sum, vext_u16(vget_low_u16(a), vget_high_u16(a), 3));
  return vadd_u16(sum, vget_high_u16(a));
}

inline uint32x4_t Sum5Horizontal(const uint32x4x2_t a) {
  uint32x4_t sum = vaddq_u32(a.val[0], vextq_u32(a.val[0], a.val[1], 1));
  sum = vaddq_u32(sum, vextq_u32(a.val[0], a.val[1], 2));
  sum = vaddq_u32(sum, vextq_u32(a.val[0], a.val[1], 3));
  return vaddq_u32(sum, a.val[1]);
}

inline uint16x8_t Sum5Vertical(const uint8x8_t a[5]) {
  uint16x8_t sum = vaddl_u8(a[0], a[1]);
  sum = vaddq_u16(sum, vaddl_u8(a[2], a[3]));
  return vaddw_u8(sum, a[4]);
}

inline uint32x4x2_t Sum5Vertical(const uint16x8_t a[5]) {
  uint32x4_t sum_a = vaddl_u16(vget_low_u16(a[0]), vget_low_u16(a[1]));
  sum_a = vaddq_u32(sum_a, vaddl_u16(vget_low_u16(a[2]), vget_low_u16(a[3])));
  sum_a = vaddw_u16(sum_a, vget_low_u16(a[4]));
  uint32x4_t sum_b = vaddl_u16(vget_high_u16(a[0]), vget_high_u16(a[1]));
  sum_b = vaddq_u32(sum_b, vaddl_u16(vget_high_u16(a[2]), vget_high_u16(a[3])));
  sum_b = vaddw_u16(sum_b, vget_high_u16(a[4]));
  return {sum_a, sum_b};
}

constexpr int kSgrProjScaleBits = 20;
constexpr int kSgrProjRestoreBits = 4;
constexpr int kSgrProjSgrBits = 8;
constexpr int kSgrProjReciprocalBits = 12;

constexpr int kIntermediateStride = 68;
constexpr int kIntermediateHeight = 66;

// a2 = ((z << kSgrProjSgrBits) + (z >> 1)) / (z + 1);
constexpr uint16_t kA2Lookup[256] = {
    1,   128, 171, 192, 205, 213, 219, 224, 228, 230, 233, 235, 236, 238, 239,
    240, 241, 242, 243, 243, 244, 244, 245, 245, 246, 246, 247, 247, 247, 247,
    248, 248, 248, 248, 249, 249, 249, 249, 249, 250, 250, 250, 250, 250, 250,
    250, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 252, 252, 252, 252,
    252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 253, 253,
    253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
    253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    256};

inline uint16x4_t Sum565(const uint16x8_t a) {
  // Multiply everything by 4.
  const uint16x8_t x4 = vshlq_n_u16(a, 2);
  // Everything * 5
  const uint16x8_t x5 = vaddq_u16(x4, a);
  // The middle elements get added once more
  const uint16x4_t middle = vext_u16(vget_low_u16(a), vget_high_u16(a), 1);
  return vadd_u16(middle, Sum3Horizontal(x5));
}

inline uint32x4_t Sum3HorizontalW(const uint32x4_t a, const uint32x4_t b) {
  const uint32x4_t sum = vaddq_u32(a, vextq_u32(a, b, 1));
  return vaddq_u32(sum, vcombine_u32(vget_high_u32(a), vget_low_u32(b)));
}

inline uint32x4_t Sum565W(const uint16x8_t a) {
  // Multiply everything by 4. |b2| values can be up to 65088 which means we
  // have to step up to 32 bits immediately.
  const uint32x4_t x4_lo = vshll_n_u16(vget_low_u16(a), 2);
  const uint32x4_t x4_hi = vshll_n_u16(vget_high_u16(a), 2);
  // Everything * 5
  const uint32x4_t x5_lo = vaddw_u16(x4_lo, vget_low_u16(a));
  const uint32x4_t x5_hi = vaddw_u16(x4_hi, vget_high_u16(a));
  // The middle elements get added once more
  const uint16x4_t middle = vext_u16(vget_low_u16(a), vget_high_u16(a), 1);
  return vaddw_u16(Sum3HorizontalW(x5_lo, x5_hi), middle);
}

template <int n>
inline uint16x4_t CalculateA2(const uint32x4_t sum_sq, const uint16x4_t sum,
                              const uint32_t s, const uint16x4_t v_255) {
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
  // Narrow |shifted| so we can use a D register for v_255.
  const uint16x4_t z = vmin_u16(v_255, vmovn_u32(shifted));

  // a2 = ((z << kSgrProjSgrBits) + (z >> 1)) / (z + 1);
  const uint16_t lookup[4] = {
      kA2Lookup[vget_lane_u16(z, 0)], kA2Lookup[vget_lane_u16(z, 1)],
      kA2Lookup[vget_lane_u16(z, 2)], kA2Lookup[vget_lane_u16(z, 3)]};
  return vld1_u16(lookup);
}

inline uint16x4_t CalculateB2Shifted(const uint16x4_t a2, const uint16x4_t sum,
                                     const uint32_t one_over_n) {
  // b2 = ((1 << kSgrProjSgrBits) - a2) * b * one_over_n
  // 1 << kSgrProjSgrBits = 256
  // |a2| = [1, 256]
  // |sgrMa2| max value = 255
  const uint16x4_t sgrMa2 = vsub_u16(vdup_n_u16(1 << kSgrProjSgrBits), a2);
  // |sum| is a box sum with radius 1 or 2.
  // For the first pass radius is 2. Maximum value is 5x5x255 = 6375.
  // For the second pass radius is 1. Maximum value is 3x3x255 = 2295.
  // |one_over_n| = ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n
  // When radius is 2 |n| is 25. |one_over_n| is 164.
  // When radius is 1 |n| is 9. |one_over_n| is 455.
  const uint32x4_t b2 = vmulq_n_u32(vmull_u16(sgrMa2, sum), one_over_n);
  // static_cast<int>(RightShiftWithRounding(b2, kSgrProjReciprocalBits));
  // |kSgrProjReciprocalBits| is 12.
  // Radius 2: 255 * 6375 * 164 >> 12 = 65088 (16 bits).
  // Radius 1: 255 * 2295 * 455 >> 12 = 65009 (16 bits).
  return vrshrn_n_u32(b2, kSgrProjReciprocalBits);
}

// RightShiftWithRounding(
//   (a * src_ptr[x] + b), kSgrProjSgrBits + shift - kSgrProjRestoreBits);
template <int shift>
inline uint16x4_t CalculateFilteredOutput(const uint16x4_t a,
                                          const uint32x4_t b,
                                          const uint16x4_t src) {
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

inline void BoxFilterProcess_FirstPass(const uint8_t* const src,
                                       const ptrdiff_t stride, const int width,
                                       const int height, const uint32_t s,
                                       uint16_t* const buf) {
  // Number of elements in the box being summed.
  const uint32_t n = 25;
  const uint32_t one_over_n = ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;

  const uint16x4_t v_255 = vdup_n_u16(255);

  // We have combined PreProcess and Process for the first pass by storing
  // intermediate values in the |a2| region. The values stored are one vertical
  // column of interleaved |a2| and |b2| values and consume 4 * |height| values.
  // This is |height| and not |height| * 2 because PreProcess only generates
  // output for every other row. When processing the next column we write the
  // new scratch values right after reading the previously saved ones.
  uint16_t* const temp = buf + kIntermediateStride * kIntermediateHeight;

  // The PreProcess phase calculates a 5x5 box sum for every other row
  //
  // PreProcess and Process have been combined into the same step. We need 8
  // input values to generate 4 output values for PreProcess:
  // 0 1 2 3 4 5 6 7
  // 2 = 0 + 1 + 2 + 3 + 4
  // 3 = 1 + 2 + 3 + 4 + 5
  // 4 = 2 + 3 + 4 + 5 + 6
  // 5 = 3 + 4 + 5 + 6 + 7
  //
  // and then we need 6 input values to generate 4 output values for Process:
  // 0 1 2 3 4 5
  // 1 = 0 + 1 + 2
  // 2 = 1 + 2 + 3
  // 3 = 2 + 3 + 4
  // 4 = 3 + 4 + 5
  //
  // To avoid re-calculating PreProcess values over and over again we will do a
  // single column of 4 output values and store them interleaved in |temp|. Next
  // we will start the second column. When 2 rows have been calculated we can
  // calculate Process and output those into the top of |buf|.

  // The first phase needs a radius of 2 context values. The second phase needs
  // a context of radius 1 values. This means we start at (-3, -3).
  const uint8_t* const src_pre_process = src - 3 - 3 * stride;

  // Calculate and store a single column. Scope so we can re-use the variable
  // names for the next step.
  {
    const uint8_t* column = src_pre_process;
    uint16_t* temp_column = temp;

    uint8x8_t row[5];
    row[0] = vld1_u8(column);
    column += stride;
    row[1] = vld1_u8(column);
    column += stride;
    row[2] = vld1_u8(column);
    column += stride;

    uint16x8_t row_sq[5];
    row_sq[0] = vmull_u8(row[0], row[0]);
    row_sq[1] = vmull_u8(row[1], row[1]);
    row_sq[2] = vmull_u8(row[2], row[2]);

    int y = -1;
    do {
      row[3] = vld1_u8(column);
      column += stride;
      row[4] = vld1_u8(column);
      column += stride;

      row_sq[3] = vmull_u8(row[3], row[3]);
      row_sq[4] = vmull_u8(row[4], row[4]);

      const uint16x4_t sum = Sum5Horizontal(Sum5Vertical(row));
      const uint32x4_t sum_sq = Sum5Horizontal(Sum5Vertical(row_sq));

      const uint16x4_t a2 = CalculateA2<n>(sum_sq, sum, s, v_255);
      const uint16x4_t b2 = CalculateB2Shifted(a2, sum, one_over_n);

      vst1q_u16(temp_column, vcombine_u16(a2, b2));
      temp_column += 8;

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0] = row_sq[2];
      row_sq[1] = row_sq[3];
      row_sq[2] = row_sq[4];
      y += 2;
    } while (y < height + 1);
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
    const uint8_t* column = src_pre_process + x + 4;

    uint8x8_t row[5];
    row[0] = vld1_u8(column);
    column += stride;
    row[1] = vld1_u8(column);
    column += stride;
    row[2] = vld1_u8(column);
    column += stride;
    row[3] = vld1_u8(column);
    column += stride;
    row[4] = vld1_u8(column);
    column += stride;

    uint16x8_t row_sq[5];
    row_sq[0] = vmull_u8(row[0], row[0]);
    row_sq[1] = vmull_u8(row[1], row[1]);
    row_sq[2] = vmull_u8(row[2], row[2]);
    row_sq[3] = vmull_u8(row[3], row[3]);
    row_sq[4] = vmull_u8(row[4], row[4]);

    // Seed the loop with one line of output. Then, inside the loop, for each
    // iteration we can output one even row and one odd row and carry the new
    // line to the next iteration. In the diagram below 'i' values are
    // intermediary values from the first step and '-' values are empty.
    // iiii
    // ---- > even row
    // iiii - odd row
    // ---- > even row
    // iiii
    uint16_t* temp_column = temp;
    uint16x4_t sum565_a0;
    uint32x4_t sum565_b0;
    {
      const uint16x4_t sum = Sum5Horizontal(Sum5Vertical(row));
      const uint32x4_t sum_sq = Sum5Horizontal(Sum5Vertical(row_sq));

      const uint16x4_t a2 = CalculateA2<n>(sum_sq, sum, s, v_255);
      const uint16x4_t b2 = CalculateB2Shifted(a2, sum, one_over_n);

      // Exchange the previously calculated |a2| and |b2| values.
      const uint16x8_t a2_b2 = vld1q_u16(temp_column);
      vst1q_u16(temp_column, vcombine_u16(a2, b2));
      temp_column += 8;

      // Pass 1 Process. These are the only values we need to propagate between
      // rows.
      sum565_a0 = Sum565(vcombine_u16(vget_low_u16(a2_b2), a2));
      sum565_b0 = Sum565W(vcombine_u16(vget_high_u16(a2_b2), b2));
    }

    row[0] = row[2];
    row[1] = row[3];
    row[2] = row[4];

    row_sq[0] = row_sq[2];
    row_sq[1] = row_sq[3];
    row_sq[2] = row_sq[4];

    const uint8_t* src_ptr = src + x;
    uint16_t* out_buf = buf + x;

    // Calculate one output line. Add in the line from the previous pass and
    // output one even row. Sum the new line and output the odd row. Carry the
    // new row into the next pass.
    int y = 0;
    do {
      row[3] = vld1_u8(column);
      column += stride;
      row[4] = vld1_u8(column);
      column += stride;

      row_sq[3] = vmull_u8(row[3], row[3]);
      row_sq[4] = vmull_u8(row[4], row[4]);

      const uint16x4_t sum = Sum5Horizontal(Sum5Vertical(row));
      const uint32x4_t sum_sq = Sum5Horizontal(Sum5Vertical(row_sq));

      const uint16x4_t a2 = CalculateA2<n>(sum_sq, sum, s, v_255);
      const uint16x4_t b2 = CalculateB2Shifted(a2, sum, one_over_n);

      const uint16x8_t a2_b2 = vld1q_u16(temp_column);
      vst1q_u16(temp_column, vcombine_u16(a2, b2));
      temp_column += 8;

      uint16x4_t sum565_a1 = Sum565(vcombine_u16(vget_low_u16(a2_b2), a2));
      uint32x4_t sum565_b1 = Sum565W(vcombine_u16(vget_high_u16(a2_b2), b2));

      const uint8x8_t src_u8 = vld1_u8(src_ptr);
      src_ptr += stride;
      const uint16x4_t src_u16 = vget_low_u16(vmovl_u8(src_u8));

      const uint16x4_t output =
          CalculateFilteredOutput<5>(vadd_u16(sum565_a0, sum565_a1),
                                     vaddq_u32(sum565_b0, sum565_b1), src_u16);

      vst1_u16(out_buf, output);
      out_buf += kIntermediateStride;

      const uint8x8_t src0_u8 = vld1_u8(src_ptr);
      src_ptr += stride;
      const uint16x4_t src0_u16 = vget_low_u16(vmovl_u8(src0_u8));

      const uint16x4_t output1 =
          CalculateFilteredOutput<4>(sum565_a1, sum565_b1, src0_u16);
      vst1_u16(out_buf, output1);
      out_buf += kIntermediateStride;

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0] = row_sq[2];
      row_sq[1] = row_sq[3];
      row_sq[2] = row_sq[4];

      sum565_a0 = sum565_a1;
      sum565_b0 = sum565_b1;
      y += 2;
    } while (y < height);
    x += 4;
  } while (x < width);
}

inline void BoxFilterPreProcess_SecondPass(const uint8_t* const src,
                                           const ptrdiff_t stride,
                                           const int width, const int height,
                                           const uint32_t s,
                                           uint16_t* const a2) {
  // Number of elements in the box being summed.
  const uint32_t n = 9;
  const uint32_t one_over_n = ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;

  const uint16x4_t v_255 = vdup_n_u16(255);

  // Calculate intermediate results, including one-pixel border, for example,
  // if unit size is 64x64, we calculate 66x66 pixels.
  // Because of the vectors this calculates in blocks of 4 so we actually
  // get 68 values. This doesn't appear to be causing problems yet but it
  // might.
  const uint8_t* const src_top_left_corner = src - 1 - 2 * stride;
  int x = -1;
  do {
    const uint8_t* column = src_top_left_corner + x;
    uint16_t* a2_column = a2 + (x + 1);
    uint8x8_t row[3];
    row[0] = vld1_u8(column);
    column += stride;
    row[1] = vld1_u8(column);
    column += stride;

    uint16x8_t row_sq[3];
    row_sq[0] = vmull_u8(row[0], row[0]);
    row_sq[1] = vmull_u8(row[1], row[1]);

    int y = -1;
    do {
      row[2] = vld1_u8(column);
      column += stride;

      row_sq[2] = vmull_u8(row[2], row[2]);

      const uint16x4_t sum = Sum3Horizontal(Sum3Vertical(row));
      const uint32x4_t sum_sq = Sum3Horizontal(Sum3Vertical(row_sq));

      const uint16x4_t a2_v = CalculateA2<n>(sum_sq, sum, s, v_255);

      vst1_u16(a2_column, a2_v);
      a2_column += kIntermediateStride;

      vst1_u16(a2_column, CalculateB2Shifted(a2_v, sum, one_over_n));
      a2_column += kIntermediateStride;

      row[0] = row[1];
      row[1] = row[2];

      row_sq[0] = row_sq[1];
      row_sq[1] = row_sq[2];
    } while (++y < height + 1);
    x += 4;
  } while (x < width + 1);
}

inline uint16x4_t Sum444(const uint16x8_t a) {
  return Sum3Horizontal(vshlq_n_u16(a, 2));
}

inline uint32x4_t Sum444W(const uint16x8_t a) {
  return Sum3HorizontalW(vshll_n_u16(vget_low_u16(a), 2),
                         vshll_n_u16(vget_high_u16(a), 2));
}

inline uint16x4_t Sum343(const uint16x8_t a) {
  const uint16x4_t middle = vext_u16(vget_low_u16(a), vget_high_u16(a), 1);
  const uint16x4_t sum = Sum3Horizontal(a);
  return vadd_u16(vadd_u16(vadd_u16(sum, sum), sum), middle);
}

inline uint32x4_t Sum343W(const uint16x8_t a) {
  const uint16x4_t middle = vext_u16(vget_low_u16(a), vget_high_u16(a), 1);
  const uint32x4_t sum =
      Sum3HorizontalW(vmovl_u16(vget_low_u16(a)), vmovl_u16(vget_high_u16(a)));
  return vaddw_u16(vaddq_u32(vaddq_u32(sum, sum), sum), middle);
}

inline void BoxFilterProcess_SecondPass(const uint8_t* src,
                                        const ptrdiff_t stride, const int width,
                                        const int height, const uint32_t s,
                                        uint16_t* const intermediate_buffer) {
  uint16_t* const a2 =
      intermediate_buffer + kIntermediateStride * kIntermediateHeight;

  BoxFilterPreProcess_SecondPass(src, stride, width, height, s, a2);

  int x = 0;
  do {
    uint16_t* a2_ptr = a2 + x;
    const uint8_t* src_ptr = src + x;
    // |filtered_output| must match how |a2| values are read since they are
    // written out over the |a2| values which have already been used.
    uint16_t* filtered_output = a2_ptr;

    uint16x4_t sum343_a[2], sum444_a;
    uint32x4_t sum343_b[2], sum444_b;

    sum343_a[0] = Sum343(vld1q_u16(a2_ptr));
    a2_ptr += kIntermediateStride;

    sum343_b[0] = Sum343W(vld1q_u16(a2_ptr));
    a2_ptr += kIntermediateStride;

    const uint16x8_t a_1 = vld1q_u16(a2_ptr);
    a2_ptr += kIntermediateStride;
    sum343_a[1] = Sum343(a_1);
    sum444_a = Sum444(a_1);

    const uint16x8_t b_1 = vld1q_u16(a2_ptr);
    a2_ptr += kIntermediateStride;
    sum343_b[1] = Sum343W(b_1);
    sum444_b = Sum444W(b_1);

    int y = 0;
    do {
      const uint16x8_t a_2 = vld1q_u16(a2_ptr);
      a2_ptr += kIntermediateStride;

      const uint16x4_t sum343_a2 = Sum343(a_2);
      const uint16x4_t a_v =
          vadd_u16(vadd_u16(sum343_a[0], sum444_a), sum343_a2);
      sum444_a = Sum444(a_2);
      sum343_a[0] = sum343_a[1];
      sum343_a[1] = sum343_a2;

      const uint16x8_t b_2 = vld1q_u16(a2_ptr);
      a2_ptr += kIntermediateStride;

      const uint32x4_t sum343_b2 = Sum343W(b_2);
      const uint32x4_t b_v =
          vaddq_u32(vaddq_u32(sum343_b[0], sum444_b), sum343_b2);
      sum444_b = Sum444W(b_2);
      sum343_b[0] = sum343_b[1];
      sum343_b[1] = sum343_b2;

      // Load 8 values and discard 4.
      const uint8x8_t src_u8 = vld1_u8(src_ptr);
      const uint16x4_t src_u16 = vget_low_u16(vmovl_u8(src_u8));

      vst1_u16(filtered_output, CalculateFilteredOutput<5>(a_v, b_v, src_u16));

      src_ptr += stride;
      filtered_output += kIntermediateStride;
    } while (++y < height);
    x += 4;
  } while (x < width);
}

template <int min_width>
inline void SelfGuidedSingleMultiplier(const uint8_t* src,
                                       const ptrdiff_t src_stride,
                                       uint16_t* box_filter_process_output,
                                       uint8_t* dst, const ptrdiff_t dst_stride,
                                       const int width, const int height,
                                       const int16_t w_combo,
                                       const int16x4_t w_single) {
  static_assert(min_width == 4 || min_width == 8, "");

  int y = 0;
  do {
    if (min_width == 8) {
      int x = 0;
      do {
        const int16x8_t u = vreinterpretq_s16_u16(
            vshll_n_u8(vld1_u8(src + x), kSgrProjRestoreBits));
        const int16x8_t p =
            vreinterpretq_s16_u16(vld1q_u16(box_filter_process_output + x));

        // u * w1 + u * wN == u * (w1 + wN)
        int32x4_t v_lo = vmull_n_s16(vget_low_s16(u), w_combo);
        v_lo = vmlal_s16(v_lo, vget_low_s16(p), w_single);

        int32x4_t v_hi = vmull_n_s16(vget_high_s16(u), w_combo);
        v_hi = vmlal_s16(v_hi, vget_high_s16(p), w_single);

        const int16x4_t s_lo =
            vrshrn_n_s32(v_lo, kSgrProjRestoreBits + kSgrProjPrecisionBits);
        const int16x4_t s_hi =
            vrshrn_n_s32(v_hi, kSgrProjRestoreBits + kSgrProjPrecisionBits);
        vst1_u8(dst + x, vqmovun_s16(vcombine_s16(s_lo, s_hi)));
        x += 8;
      } while (x < width);
    } else if (min_width == 4) {
      const int16x8_t u =
          vreinterpretq_s16_u16(vshll_n_u8(vld1_u8(src), kSgrProjRestoreBits));
      const int16x8_t p =
          vreinterpretq_s16_u16(vld1q_u16(box_filter_process_output));

      // u * w1 + u * wN == u * (w1 + wN)
      int32x4_t v_lo = vmull_n_s16(vget_low_s16(u), w_combo);
      v_lo = vmlal_s16(v_lo, vget_low_s16(p), w_single);

      int32x4_t v_hi = vmull_n_s16(vget_high_s16(u), w_combo);
      v_hi = vmlal_s16(v_hi, vget_high_s16(p), w_single);

      const int16x4_t s_lo =
          vrshrn_n_s32(v_lo, kSgrProjRestoreBits + kSgrProjPrecisionBits);
      const int16x4_t s_hi =
          vrshrn_n_s32(v_hi, kSgrProjRestoreBits + kSgrProjPrecisionBits);
      StoreLo4(dst, vqmovun_s16(vcombine_s16(s_lo, s_hi)));
    }
    src += src_stride;
    dst += dst_stride;
    box_filter_process_output += kIntermediateStride;
  } while (++y < height);
}

template <int min_width>
inline void SelfGuidedDoubleMultiplier(const uint8_t* src,
                                       const ptrdiff_t src_stride,
                                       uint16_t* box_filter_process_output[2],
                                       uint8_t* dst, const ptrdiff_t dst_stride,
                                       const int width, const int height,
                                       const int16x4_t w0, const int w1,
                                       const int16x4_t w2) {
  static_assert(min_width == 4 || min_width == 8, "");

  int y = 0;
  do {
    if (min_width == 8) {
      int x = 0;
      do {
        // |wN| values are signed. |src| values can be treated as int16_t.
        const int16x8_t u = vreinterpretq_s16_u16(
            vshll_n_u8(vld1_u8(src + x), kSgrProjRestoreBits));
        // |box_filter_process_output| is 14 bits, also safe to treat as
        // int16_t.
        const int16x8_t p0 =
            vreinterpretq_s16_u16(vld1q_u16(box_filter_process_output[0] + x));
        const int16x8_t p1 =
            vreinterpretq_s16_u16(vld1q_u16(box_filter_process_output[1] + x));

        int32x4_t v_lo = vmull_n_s16(vget_low_s16(u), w1);
        v_lo = vmlal_s16(v_lo, vget_low_s16(p0), w0);
        v_lo = vmlal_s16(v_lo, vget_low_s16(p1), w2);

        int32x4_t v_hi = vmull_n_s16(vget_high_s16(u), w1);
        v_hi = vmlal_s16(v_hi, vget_high_s16(p0), w0);
        v_hi = vmlal_s16(v_hi, vget_high_s16(p1), w2);

        // |s| is saturated to uint8_t.
        const int16x4_t s_lo =
            vrshrn_n_s32(v_lo, kSgrProjRestoreBits + kSgrProjPrecisionBits);
        const int16x4_t s_hi =
            vrshrn_n_s32(v_hi, kSgrProjRestoreBits + kSgrProjPrecisionBits);
        vst1_u8(dst + x, vqmovun_s16(vcombine_s16(s_lo, s_hi)));
        x += 8;
      } while (x < width);
    } else if (min_width == 4) {
      // |wN| values are signed. |src| values can be treated as int16_t.
      // Load 8 values but ignore 4.
      const int16x4_t u = vget_low_s16(
          vreinterpretq_s16_u16(vshll_n_u8(vld1_u8(src), kSgrProjRestoreBits)));
      // |box_filter_process_output| is 14 bits, also safe to treat as
      // int16_t.
      const int16x4_t p0 =
          vreinterpret_s16_u16(vld1_u16(box_filter_process_output[0]));
      const int16x4_t p1 =
          vreinterpret_s16_u16(vld1_u16(box_filter_process_output[1]));

      int32x4_t v = vmull_n_s16(u, w1);
      v = vmlal_s16(v, p0, w0);
      v = vmlal_s16(v, p1, w2);

      // |s| is saturated to uint8_t.
      const int16x4_t s =
          vrshrn_n_s32(v, kSgrProjRestoreBits + kSgrProjPrecisionBits);
      StoreLo4(dst, vqmovun_s16(vcombine_s16(s, s)));
    }
    src += src_stride;
    dst += dst_stride;
    box_filter_process_output[0] += kIntermediateStride;
    box_filter_process_output[1] += kIntermediateStride;
  } while (++y < height);
}

// Assume box_filter_process_output[2] are allocated before calling
// this function. Their sizes are width * height, stride equals width.
void SelfGuidedFilter_NEON(const void* const source, void* const dest,
                           const RestorationUnitInfo& restoration_info,
                           ptrdiff_t source_stride, ptrdiff_t dest_stride,
                           const int width, const int height,
                           RestorationBuffer* const /*buffer*/) {
  // The output frame is broken into blocks of 64x64 (32x32 if U/V are
  // subsampled). If either dimension is less than 32/64 it indicates it is at
  // the right or bottom edge of the frame. It is safe to overwrite the output
  // as it will not be part of the visible frame. This saves us from having to
  // handle non-multiple-of-8 widths.
  // We could round here, but the for loop with += 8 does the same thing.

  // width = (width + 7) & ~0x7;

  // -96 to 96 (Sgrproj_Xqd_Min/Max)
  const int8_t w0 = restoration_info.sgr_proj_info.multiplier[0];
  const int8_t w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int16_t w2 = (1 << kSgrProjPrecisionBits) - w0 - w1;
  const int index = restoration_info.sgr_proj_info.index;
  const uint8_t radius_pass_0 = kSgrProjParams[index][0];
  const uint8_t radius_pass_1 = kSgrProjParams[index][2];
  const auto* src = static_cast<const uint8_t*>(source);
  auto* dst = static_cast<uint8_t*>(dest);

  // |intermediate_buffer| is broken down into three distinct regions, each with
  // size |kIntermediateStride| * |kIntermediateHeight|.
  // The first is for final output of the first pass of PreProcess/Process. It
  // can be stored in |width| * |height| (at most 64x64).
  // The second and third are scratch space for |a2| and |b2| values from
  // PreProcess.
  //
  // At the end of BoxFilterProcess_SecondPass() the output is stored over |a2|.

  uint16_t intermediate_buffer[3 * kIntermediateStride * kIntermediateHeight];
  uint16_t* box_filter_process_output[2] = {
      intermediate_buffer,
      intermediate_buffer + kIntermediateStride * kIntermediateHeight};

  // If |radius| is 0 then there is nothing to do. If |radius| is not 0, it is
  // always 2 for the first pass and 1 for the second pass.
  if (radius_pass_0 != 0) {
    BoxFilterProcess_FirstPass(src, source_stride, width, height,
                               kSgrScaleParameter[index][0],
                               intermediate_buffer);
  }

  if (radius_pass_1 != 0) {
    BoxFilterProcess_SecondPass(src, source_stride, width, height,
                                kSgrScaleParameter[index][1],
                                intermediate_buffer);
  }

  // Put |w[02]| in vectors because we can use vmull_n_s16() for |w1| but there
  // is no vmlal_n_s16().
  const int16x4_t w0_v = vdup_n_s16(w0);
  const int16x4_t w2_v = vdup_n_s16(w2);
  if (radius_pass_0 != 0 && radius_pass_1 != 0) {
    if (width > 4) {
      SelfGuidedDoubleMultiplier<8>(src, source_stride,
                                    box_filter_process_output, dst, dest_stride,
                                    width, height, w0_v, w1, w2_v);
    } else /* if (width == 4) */ {
      SelfGuidedDoubleMultiplier<4>(src, source_stride,
                                    box_filter_process_output, dst, dest_stride,
                                    width, height, w0_v, w1, w2_v);
    }
  } else {
    int16_t w_combo;
    int16x4_t w_single;
    uint16_t* box_filter_process_output_n;
    if (radius_pass_0 != 0) {
      w_combo = w1 + w2;
      w_single = w0_v;
      box_filter_process_output_n = box_filter_process_output[0];
    } else /* if (radius_pass_1 != 0) */ {
      w_combo = w1 + w0;
      w_single = w2_v;
      box_filter_process_output_n = box_filter_process_output[1];
    }

    if (width > 4) {
      SelfGuidedSingleMultiplier<8>(
          src, source_stride, box_filter_process_output_n, dst, dest_stride,
          width, height, w_combo, w_single);
    } else /* if (width == 4) */ {
      SelfGuidedSingleMultiplier<4>(
          src, source_stride, box_filter_process_output_n, dst, dest_stride,
          width, height, w_combo, w_single);
    }
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
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
