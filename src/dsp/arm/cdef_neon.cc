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

#include "src/dsp/cdef.h"
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_NEON

#include <arm_neon.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "src/dsp/dsp.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

// TODO(johannkoenig): Dedup these constants.
constexpr int kBitdepth8 = 8;

constexpr uint16_t kCdefLargeValue = 0x4000;

constexpr uint8_t kPrimaryTaps[2][2] = {{4, 2}, {3, 3}};

constexpr uint8_t kSecondaryTaps[2] = {2, 1};

constexpr int8_t kCdefDirections[8][2][2] = {
    {{-1, 1}, {-2, 2}}, {{0, 1}, {-1, 2}}, {{0, 1}, {0, 2}}, {{0, 1}, {1, 2}},
    {{1, 1}, {2, 2}},   {{1, 0}, {2, 1}},  {{1, 0}, {2, 0}}, {{1, 0}, {2, -1}}};

int Constrain(int diff, int threshold, int damping) {
  if (threshold == 0) return 0;
  damping = std::max(0, damping - FloorLog2(threshold));
  const int sign = (diff < 0) ? -1 : 1;
  return sign *
         Clip3(threshold - (std::abs(diff) >> damping), 0, std::abs(diff));
}

// Load 4 vectors based on the given |direction|.
void LoadDirection(const uint16_t* const src, const ptrdiff_t stride,
                   uint16x8_t* output, const int direction) {
  // Each |direction| describes a different set of source values. Expand this
  // set by negating each set. For |direction| == 0 this gives a diagonal line
  // from top right to bottom left. The first value is y, the second x. Negative
  // y values move up.
  //    a       b         c       d
  // {-1, 1}, {1, -1}, {-2, 2}, {2, -2}
  //         c
  //       a
  //     0
  //   b
  // d
  const int y_0 = kCdefDirections[direction][0][0];
  const int x_0 = kCdefDirections[direction][0][1];
  const int y_1 = kCdefDirections[direction][1][0];
  const int x_1 = kCdefDirections[direction][1][1];
  output[0] = vld1q_u16(src + y_0 * stride + x_0);
  output[1] = vld1q_u16(src - y_0 * stride - x_0);
  output[2] = vld1q_u16(src + y_1 * stride + x_1);
  output[3] = vld1q_u16(src - y_1 * stride - x_1);
}

int16x8_t Constrain(const uint16x8_t pixel, const uint16x8_t reference,
                    const int16x8_t threshold, const int16x8_t damping) {
  const int16x8_t zero = vdupq_n_s16(0);
  const int16x8_t diff = vreinterpretq_s16_u16(vsubq_u16(pixel, reference));
  // Convert the sign bit to 0 or -1.
  const int16x8_t sign = vshrq_n_s16(diff, 15);
  int16x8_t abs_diff = vabsq_s16(diff);
  // Exclude |kCdefLargeValue|. Zeroing |abs_diff| here ensures the output of
  // Constrain() is 0. The alternative is masking out these values and replacing
  // them with |reference|.
  const uint16x8_t large_value_mask =
      vcltq_s16(abs_diff, vdupq_n_s16(kCdefLargeValue - 255));
  abs_diff = vandq_s16(abs_diff, vreinterpretq_s16_u16(large_value_mask));
  const int16x8_t shifted_diff = vshlq_s16(abs_diff, damping);
  const int16x8_t thresh_minus_shifted_diff =
      vsubq_s16(threshold, shifted_diff);
  const int16x8_t clamp_zero = vmaxq_s16(thresh_minus_shifted_diff, zero);
  const int16x8_t clamp_abs_diff = vminq_s16(clamp_zero, abs_diff);
  // Restore the sign.
  return vsubq_s16(veorq_s16(clamp_abs_diff, sign), sign);
}

// Filters the source block. It doesn't check whether the candidate pixel is
// inside the frame. However it requires the source input to be padded with a
// constant large value if at the boundary. The input must be uint16_t.
void CdefFilter_NEON(const void* const source, const ptrdiff_t source_stride,
                     const int rows4x4, const int columns4x4, const int curr_x,
                     const int curr_y, const int subsampling_x,
                     const int subsampling_y, const int primary_strength,
                     const int secondary_strength, const int damping,
                     const int direction, void* const dest,
                     const ptrdiff_t dest_stride) {
  const int plane_width = MultiplyBy4(columns4x4) >> subsampling_x;
  const int plane_height = MultiplyBy4(rows4x4) >> subsampling_y;
  const int block_width = std::min(8 >> subsampling_x, plane_width - curr_x);
  const int block_height = std::min(8 >> subsampling_y, plane_height - curr_y);
  const auto* src = static_cast<const uint16_t*>(source);
  auto* dst = static_cast<uint8_t*>(dest);

  if (block_width == 8) {
    const uint16x8_t cdef_large_value_mask =
        vdupq_n_u16(static_cast<uint16_t>(~kCdefLargeValue));
    const int16x8_t primary_threshold = vdupq_n_s16(primary_strength);
    const int16x8_t secondary_threshold = vdupq_n_s16(secondary_strength);

    int16x8_t primary_damping_shift, secondary_damping_shift;
    // FloorLog2() requires input to be > 0.
    if (primary_strength == 0) {
      primary_damping_shift = vdupq_n_s16(0);
    } else {
      primary_damping_shift =
          vdupq_n_s16(-std::max(0, damping - FloorLog2(primary_strength)));
    }

    if (secondary_strength == 0) {
      secondary_damping_shift = vdupq_n_s16(0);
    } else {
      secondary_damping_shift =
          vdupq_n_s16(-std::max(0, damping - FloorLog2(secondary_strength)));
    }

    const int primary_tap_0 = kPrimaryTaps[primary_strength & 1][0];
    const int primary_tap_1 = kPrimaryTaps[primary_strength & 1][1];
    const int secondary_tap_0 = kSecondaryTaps[0];
    const int secondary_tap_1 = kSecondaryTaps[1];

    int y = 0;
    do {
      const uint16x8_t pixel = vld1q_u16(src);
      uint16x8_t min = pixel;
      uint16x8_t max = pixel;

      // Primary |direction|.
      uint16x8_t primary_val[4];
      LoadDirection(src, source_stride, primary_val, direction);

      min = vminq_u16(min, primary_val[0]);
      min = vminq_u16(min, primary_val[1]);
      min = vminq_u16(min, primary_val[2]);
      min = vminq_u16(min, primary_val[3]);

      // Convert kCdefLargeValue to 0 before calculating max.
      max = vmaxq_u16(max, vandq_u16(primary_val[0], cdef_large_value_mask));
      max = vmaxq_u16(max, vandq_u16(primary_val[1], cdef_large_value_mask));
      max = vmaxq_u16(max, vandq_u16(primary_val[2], cdef_large_value_mask));
      max = vmaxq_u16(max, vandq_u16(primary_val[3], cdef_large_value_mask));

      int16x8_t sum = Constrain(primary_val[0], pixel, primary_threshold,
                                primary_damping_shift);
      sum = vmulq_n_s16(sum, primary_tap_0);
      sum = vmlaq_n_s16(sum,
                        Constrain(primary_val[1], pixel, primary_threshold,
                                  primary_damping_shift),
                        primary_tap_0);
      sum = vmlaq_n_s16(sum,
                        Constrain(primary_val[2], pixel, primary_threshold,
                                  primary_damping_shift),
                        primary_tap_1);
      sum = vmlaq_n_s16(sum,
                        Constrain(primary_val[3], pixel, primary_threshold,
                                  primary_damping_shift),
                        primary_tap_1);

      // Secondary |direction| values (+/- 2). Clamp |direction|.
      uint16x8_t secondary_val[8];
      LoadDirection(src, source_stride, secondary_val, (direction + 2) & 0x7);
      LoadDirection(src, source_stride, secondary_val + 4,
                    (direction - 2) & 0x7);

      min = vminq_u16(min, secondary_val[0]);
      min = vminq_u16(min, secondary_val[1]);
      min = vminq_u16(min, secondary_val[2]);
      min = vminq_u16(min, secondary_val[3]);
      min = vminq_u16(min, secondary_val[4]);
      min = vminq_u16(min, secondary_val[5]);
      min = vminq_u16(min, secondary_val[6]);
      min = vminq_u16(min, secondary_val[7]);

      max = vmaxq_u16(max, vandq_u16(secondary_val[0], cdef_large_value_mask));
      max = vmaxq_u16(max, vandq_u16(secondary_val[1], cdef_large_value_mask));
      max = vmaxq_u16(max, vandq_u16(secondary_val[2], cdef_large_value_mask));
      max = vmaxq_u16(max, vandq_u16(secondary_val[3], cdef_large_value_mask));
      max = vmaxq_u16(max, vandq_u16(secondary_val[4], cdef_large_value_mask));
      max = vmaxq_u16(max, vandq_u16(secondary_val[5], cdef_large_value_mask));
      max = vmaxq_u16(max, vandq_u16(secondary_val[6], cdef_large_value_mask));
      max = vmaxq_u16(max, vandq_u16(secondary_val[7], cdef_large_value_mask));

      sum = vmlaq_n_s16(sum,
                        Constrain(secondary_val[0], pixel, secondary_threshold,
                                  secondary_damping_shift),
                        secondary_tap_0);
      sum = vmlaq_n_s16(sum,
                        Constrain(secondary_val[1], pixel, secondary_threshold,
                                  secondary_damping_shift),
                        secondary_tap_0);
      sum = vmlaq_n_s16(sum,
                        Constrain(secondary_val[2], pixel, secondary_threshold,
                                  secondary_damping_shift),
                        secondary_tap_1);
      sum = vmlaq_n_s16(sum,
                        Constrain(secondary_val[3], pixel, secondary_threshold,
                                  secondary_damping_shift),
                        secondary_tap_1);
      sum = vmlaq_n_s16(sum,
                        Constrain(secondary_val[4], pixel, secondary_threshold,
                                  secondary_damping_shift),
                        secondary_tap_0);
      sum = vmlaq_n_s16(sum,
                        Constrain(secondary_val[5], pixel, secondary_threshold,
                                  secondary_damping_shift),
                        secondary_tap_0);
      sum = vmlaq_n_s16(sum,
                        Constrain(secondary_val[6], pixel, secondary_threshold,
                                  secondary_damping_shift),
                        secondary_tap_1);
      sum = vmlaq_n_s16(sum,
                        Constrain(secondary_val[7], pixel, secondary_threshold,
                                  secondary_damping_shift),
                        secondary_tap_1);

      // Clip3(pixel + ((8 + sum - (sum < 0)) >> 4), min, max))
      const int16x8_t sum_lt_0 = vshrq_n_s16(sum, 15);
      sum = vaddq_s16(sum, vdupq_n_s16(8));
      sum = vaddq_s16(sum, sum_lt_0);
      sum = vshrq_n_s16(sum, 4);
      sum = vaddq_s16(sum, vreinterpretq_s16_u16(pixel));
      sum = vminq_s16(sum, vreinterpretq_s16_u16(max));
      sum = vmaxq_s16(sum, vreinterpretq_s16_u16(min));

      vst1_u8(dst, vqmovun_s16(sum));

      src += source_stride;
      dst += dest_stride;
    } while (++y < block_height);
  } else {
    int y = 0;
    do {
      int x = 0;
      do {
        int16_t sum = 0;
        const uint16_t pixel_value = src[x];
        uint16_t max_value = pixel_value;
        uint16_t min_value = pixel_value;
        for (int k = 0; k < 2; ++k) {
          const int signs[] = {-1, 1};
          for (const int& sign : signs) {
            int dy = sign * kCdefDirections[direction][k][0];
            int dx = sign * kCdefDirections[direction][k][1];
            uint16_t value = src[dy * source_stride + dx + x];
            // Note: the summation can ignore the condition check in SIMD
            // implementation, because Constrain() will return 0 when
            // value == kCdefLargeValue.
            if (value != kCdefLargeValue) {
              sum += Constrain(value - pixel_value, primary_strength, damping) *
                     kPrimaryTaps[primary_strength & 1][k];
              max_value = std::max(value, max_value);
              min_value = std::min(value, min_value);
            }
            const int offsets[] = {-2, 2};
            for (const int& offset : offsets) {
              dy = sign * kCdefDirections[(direction + offset) & 7][k][0];
              dx = sign * kCdefDirections[(direction + offset) & 7][k][1];
              value = src[dy * source_stride + dx + x];
              // Note: the summation can ignore the condition check in SIMD
              // implementation.
              if (value != kCdefLargeValue) {
                sum += Constrain(value - pixel_value, secondary_strength,
                                 damping) *
                       kSecondaryTaps[k];
                max_value = std::max(value, max_value);
                min_value = std::min(value, min_value);
              }
            }
          }
        }

        dst[x] = static_cast<uint8_t>(Clip3(
            pixel_value + ((8 + sum - (sum < 0)) >> 4), min_value, max_value));
      } while (++x < block_width);

      src += source_stride;
      dst += dest_stride;
    } while (++y < block_height);
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
  dsp->cdef_filter = CdefFilter_NEON;
}

}  // namespace
}  // namespace low_bitdepth

void CdefInit_NEON() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1
#else   // !LIBGAV1_ENABLE_NEON
namespace libgav1 {
namespace dsp {

void CdefInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON
