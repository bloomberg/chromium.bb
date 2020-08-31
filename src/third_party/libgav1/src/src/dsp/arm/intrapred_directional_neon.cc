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

#if LIBGAV1_ENABLE_NEON

#include <arm_neon.h>

#include <algorithm>  // std::min
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>  // memset

#include "src/dsp/arm/common_neon.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

// Blend two values based on a 32 bit weight.
inline uint8x8_t WeightedBlend(const uint8x8_t a, const uint8x8_t b,
                               const uint8x8_t a_weight,
                               const uint8x8_t b_weight) {
  const uint16x8_t a_product = vmull_u8(a, a_weight);
  const uint16x8_t b_product = vmull_u8(b, b_weight);

  return vrshrn_n_u16(vaddq_u16(a_product, b_product), 5);
}

// For vertical operations the weights are one constant value.
inline uint8x8_t WeightedBlend(const uint8x8_t a, const uint8x8_t b,
                               const uint8_t weight) {
  return WeightedBlend(a, b, vdup_n_u8(32 - weight), vdup_n_u8(weight));
}

// Fill |left| and |right| with the appropriate values for a given |base_step|.
inline void LoadStepwise(const uint8_t* const source, const uint8x8_t left_step,
                         const uint8x8_t right_step, uint8x8_t* left,
                         uint8x8_t* right) {
  const uint8x16_t mixed = vld1q_u8(source);
  *left = VQTbl1U8(mixed, left_step);
  *right = VQTbl1U8(mixed, right_step);
}

// Handle signed step arguments by ignoring the sign. Negative values are
// considered out of range and overwritten later.
inline void LoadStepwise(const uint8_t* const source, const int8x8_t left_step,
                         const int8x8_t right_step, uint8x8_t* left,
                         uint8x8_t* right) {
  LoadStepwise(source, vreinterpret_u8_s8(left_step),
               vreinterpret_u8_s8(right_step), left, right);
}

// Process 4 or 8 |width| by any |height|.
template <int width>
inline void DirectionalZone1_WxH(uint8_t* dst, const ptrdiff_t stride,
                                 const int height, const uint8_t* const top,
                                 const int xstep, const bool upsampled) {
  assert(width == 4 || width == 8);

  const int upsample_shift = static_cast<int>(upsampled);
  const int scale_bits = 6 - upsample_shift;

  const int max_base_x = (width + height - 1) << upsample_shift;
  const int8x8_t max_base = vdup_n_s8(max_base_x);
  const uint8x8_t top_max_base = vdup_n_u8(top[max_base_x]);

  const int8x8_t all = vcreate_s8(0x0706050403020100);
  const int8x8_t even = vcreate_s8(0x0e0c0a0806040200);
  const int8x8_t base_step = upsampled ? even : all;
  const int8x8_t right_step = vadd_s8(base_step, vdup_n_s8(1));

  int top_x = xstep;
  int y = 0;
  do {
    const int top_base_x = top_x >> scale_bits;

    if (top_base_x >= max_base_x) {
      for (int i = y; i < height; ++i) {
        memset(dst, top[max_base_x], 4 /* width */);
        dst += stride;
      }
      return;
    }

    const uint8_t shift = ((top_x << upsample_shift) & 0x3F) >> 1;

    // Zone2 uses negative values for xstep. Use signed values to compare
    // |top_base_x| to |max_base_x|.
    const int8x8_t base_v = vadd_s8(vdup_n_s8(top_base_x), base_step);

    const uint8x8_t max_base_mask = vclt_s8(base_v, max_base);

    // 4 wide subsamples the output. 8 wide subsamples the input.
    if (width == 4) {
      const uint8x8_t left_values = vld1_u8(top + top_base_x);
      const uint8x8_t right_values = RightShift<8>(left_values);
      const uint8x8_t value = WeightedBlend(left_values, right_values, shift);

      // If |upsampled| is true then extract every other value for output.
      const uint8x8_t value_stepped =
          vtbl1_u8(value, vreinterpret_u8_s8(base_step));
      const uint8x8_t masked_value =
          vbsl_u8(max_base_mask, value_stepped, top_max_base);

      StoreLo4(dst, masked_value);
    } else /* width == 8 */ {
      uint8x8_t left_values, right_values;
      // WeightedBlend() steps up to Q registers. Downsample the input to avoid
      // doing extra calculations.
      LoadStepwise(top + top_base_x, base_step, right_step, &left_values,
                   &right_values);

      const uint8x8_t value = WeightedBlend(left_values, right_values, shift);
      const uint8x8_t masked_value =
          vbsl_u8(max_base_mask, value, top_max_base);

      vst1_u8(dst, masked_value);
    }
    dst += stride;
    top_x += xstep;
  } while (++y < height);
}

// Process a multiple of 8 |width| by any |height|. Processes horizontally
// before vertically in the hopes of being a little more cache friendly.
inline void DirectionalZone1_WxH(uint8_t* dst, const ptrdiff_t stride,
                                 const int width, const int height,
                                 const uint8_t* const top, const int xstep,
                                 const bool upsampled) {
  assert(width % 8 == 0);
  const int upsample_shift = static_cast<int>(upsampled);
  const int scale_bits = 6 - upsample_shift;

  const int max_base_x = (width + height - 1) << upsample_shift;
  const int8x8_t max_base = vdup_n_s8(max_base_x);
  const uint8x8_t top_max_base = vdup_n_u8(top[max_base_x]);

  const int8x8_t all = vcreate_s8(0x0706050403020100);
  const int8x8_t even = vcreate_s8(0x0e0c0a0806040200);
  const int8x8_t base_step = upsampled ? even : all;
  const int8x8_t right_step = vadd_s8(base_step, vdup_n_s8(1));
  const int8x8_t block_step = vdup_n_s8(8 << upsample_shift);

  int top_x = xstep;
  int y = 0;
  do {
    const int top_base_x = top_x >> scale_bits;

    if (top_base_x >= max_base_x) {
      for (int i = y; i < height; ++i) {
        memset(dst, top[max_base_x], 4 /* width */);
        dst += stride;
      }
      return;
    }

    const uint8_t shift = ((top_x << upsample_shift) & 0x3F) >> 1;

    // Zone2 uses negative values for xstep. Use signed values to compare
    // |top_base_x| to |max_base_x|.
    int8x8_t base_v = vadd_s8(vdup_n_s8(top_base_x), base_step);

    int x = 0;
    do {
      const uint8x8_t max_base_mask = vclt_s8(base_v, max_base);

      // Extract the input values based on |upsampled| here to avoid doing twice
      // as many calculations.
      uint8x8_t left_values, right_values;
      LoadStepwise(top + top_base_x + x, base_step, right_step, &left_values,
                   &right_values);

      const uint8x8_t value = WeightedBlend(left_values, right_values, shift);
      const uint8x8_t masked_value =
          vbsl_u8(max_base_mask, value, top_max_base);

      vst1_u8(dst + x, masked_value);

      base_v = vadd_s8(base_v, block_step);
      x += 8;
    } while (x < width);
    top_x += xstep;
    dst += stride;
  } while (++y < height);
}

void DirectionalIntraPredictorZone1_NEON(void* const dest,
                                         const ptrdiff_t stride,
                                         const void* const top_row,
                                         const int width, const int height,
                                         const int xstep,
                                         const bool upsampled_top) {
  const uint8_t* const top = static_cast<const uint8_t*>(top_row);
  uint8_t* dst = static_cast<uint8_t*>(dest);

  assert(xstep > 0);

  const int upsample_shift = static_cast<int>(upsampled_top);

  const uint8x8_t all = vcreate_u8(0x0706050403020100);

  if (xstep == 64) {
    assert(!upsampled_top);
    const uint8_t* top_ptr = top + 1;
    int y = 0;
    do {
      memcpy(dst, top_ptr, width);
      memcpy(dst + stride, top_ptr + 1, width);
      memcpy(dst + 2 * stride, top_ptr + 2, width);
      memcpy(dst + 3 * stride, top_ptr + 3, width);
      dst += 4 * stride;
      top_ptr += 4;
      y += 4;
    } while (y < height);
  } else if (width == 4) {
    DirectionalZone1_WxH<4>(dst, stride, height, top, xstep, upsampled_top);
  } else if (xstep > 51) {
    // 7.11.2.10. Intra edge upsample selection process
    // if ( d <= 0 || d >= 40 ) useUpsample = 0
    // For |upsample_top| the delta is from vertical so |prediction_angle - 90|.
    // In |kDirectionalIntraPredictorDerivative[]| angles less than 51 will meet
    // this criteria. The |xstep| value for angle 51 happens to be 51 as well.
    // Shallower angles have greater xstep values.
    assert(!upsampled_top);
    const int max_base_x = ((width + height) - 1);
    const uint8x8_t max_base = vdup_n_u8(max_base_x);
    const uint8x8_t top_max_base = vdup_n_u8(top[max_base_x]);
    const uint8x8_t block_step = vdup_n_u8(8);

    int top_x = xstep;
    int y = 0;
    do {
      const int top_base_x = top_x >> 6;
      const uint8_t shift = ((top_x << upsample_shift) & 0x3F) >> 1;
      uint8x8_t base_v = vadd_u8(vdup_n_u8(top_base_x), all);
      int x = 0;
      // Only calculate a block of 8 when at least one of the output values is
      // within range. Otherwise it can read off the end of |top|.
      const int must_calculate_width =
          std::min(width, max_base_x - top_base_x + 7) & ~7;
      for (; x < must_calculate_width; x += 8) {
        const uint8x8_t max_base_mask = vclt_u8(base_v, max_base);

        // Since these |xstep| values can not be upsampled the load is
        // simplified.
        const uint8x8_t left_values = vld1_u8(top + top_base_x + x);
        const uint8x8_t right_values = vld1_u8(top + top_base_x + x + 1);
        const uint8x8_t value = WeightedBlend(left_values, right_values, shift);
        const uint8x8_t masked_value =
            vbsl_u8(max_base_mask, value, top_max_base);

        vst1_u8(dst + x, masked_value);
        base_v = vadd_u8(base_v, block_step);
      }
      memset(dst + x, top[max_base_x], width - x);
      dst += stride;
      top_x += xstep;
    } while (++y < height);
  } else {
    DirectionalZone1_WxH(dst, stride, width, height, top, xstep, upsampled_top);
  }
}

// Process 4 or 8 |width| by 4 or 8 |height|.
template <int width>
inline void DirectionalZone3_WxH(uint8_t* dest, const ptrdiff_t stride,
                                 const int height,
                                 const uint8_t* const left_column,
                                 const int base_left_y, const int ystep,
                                 const int upsample_shift) {
  assert(width == 4 || width == 8);
  assert(height == 4 || height == 8);
  const int scale_bits = 6 - upsample_shift;

  // Zone3 never runs out of left_column values.
  assert((width + height - 1) << upsample_shift >  // max_base_y
         ((ystep * width) >> scale_bits) +
             (/* base_step */ 1 << upsample_shift) *
                 (height - 1));  // left_base_y

  // Limited improvement for 8x8. ~20% faster for 64x64.
  const uint8x8_t all = vcreate_u8(0x0706050403020100);
  const uint8x8_t even = vcreate_u8(0x0e0c0a0806040200);
  const uint8x8_t base_step = upsample_shift ? even : all;
  const uint8x8_t right_step = vadd_u8(base_step, vdup_n_u8(1));

  uint8_t* dst = dest;
  uint8x8_t left_v[8], right_v[8], value_v[8];
  const uint8_t* const left = left_column;

  const int index_0 = base_left_y;
  LoadStepwise(left + (index_0 >> scale_bits), base_step, right_step,
               &left_v[0], &right_v[0]);
  value_v[0] = WeightedBlend(left_v[0], right_v[0],
                             ((index_0 << upsample_shift) & 0x3F) >> 1);

  const int index_1 = base_left_y + ystep;
  LoadStepwise(left + (index_1 >> scale_bits), base_step, right_step,
               &left_v[1], &right_v[1]);
  value_v[1] = WeightedBlend(left_v[1], right_v[1],
                             ((index_1 << upsample_shift) & 0x3F) >> 1);

  const int index_2 = base_left_y + ystep * 2;
  LoadStepwise(left + (index_2 >> scale_bits), base_step, right_step,
               &left_v[2], &right_v[2]);
  value_v[2] = WeightedBlend(left_v[2], right_v[2],
                             ((index_2 << upsample_shift) & 0x3F) >> 1);

  const int index_3 = base_left_y + ystep * 3;
  LoadStepwise(left + (index_3 >> scale_bits), base_step, right_step,
               &left_v[3], &right_v[3]);
  value_v[3] = WeightedBlend(left_v[3], right_v[3],
                             ((index_3 << upsample_shift) & 0x3F) >> 1);

  const int index_4 = base_left_y + ystep * 4;
  LoadStepwise(left + (index_4 >> scale_bits), base_step, right_step,
               &left_v[4], &right_v[4]);
  value_v[4] = WeightedBlend(left_v[4], right_v[4],
                             ((index_4 << upsample_shift) & 0x3F) >> 1);

  const int index_5 = base_left_y + ystep * 5;
  LoadStepwise(left + (index_5 >> scale_bits), base_step, right_step,
               &left_v[5], &right_v[5]);
  value_v[5] = WeightedBlend(left_v[5], right_v[5],
                             ((index_5 << upsample_shift) & 0x3F) >> 1);

  const int index_6 = base_left_y + ystep * 6;
  LoadStepwise(left + (index_6 >> scale_bits), base_step, right_step,
               &left_v[6], &right_v[6]);
  value_v[6] = WeightedBlend(left_v[6], right_v[6],
                             ((index_6 << upsample_shift) & 0x3F) >> 1);

  const int index_7 = base_left_y + ystep * 7;
  LoadStepwise(left + (index_7 >> scale_bits), base_step, right_step,
               &left_v[7], &right_v[7]);
  value_v[7] = WeightedBlend(left_v[7], right_v[7],
                             ((index_7 << upsample_shift) & 0x3F) >> 1);

  // 8x8 transpose.
  const uint8x16x2_t b0 = vtrnq_u8(vcombine_u8(value_v[0], value_v[4]),
                                   vcombine_u8(value_v[1], value_v[5]));
  const uint8x16x2_t b1 = vtrnq_u8(vcombine_u8(value_v[2], value_v[6]),
                                   vcombine_u8(value_v[3], value_v[7]));

  const uint16x8x2_t c0 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[0]),
                                    vreinterpretq_u16_u8(b1.val[0]));
  const uint16x8x2_t c1 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[1]),
                                    vreinterpretq_u16_u8(b1.val[1]));

  const uint32x4x2_t d0 = vuzpq_u32(vreinterpretq_u32_u16(c0.val[0]),
                                    vreinterpretq_u32_u16(c1.val[0]));
  const uint32x4x2_t d1 = vuzpq_u32(vreinterpretq_u32_u16(c0.val[1]),
                                    vreinterpretq_u32_u16(c1.val[1]));

  if (width == 4) {
    StoreLo4(dst, vreinterpret_u8_u32(vget_low_u32(d0.val[0])));
    dst += stride;
    StoreLo4(dst, vreinterpret_u8_u32(vget_high_u32(d0.val[0])));
    dst += stride;
    StoreLo4(dst, vreinterpret_u8_u32(vget_low_u32(d1.val[0])));
    dst += stride;
    StoreLo4(dst, vreinterpret_u8_u32(vget_high_u32(d1.val[0])));
    if (height == 4) return;
    dst += stride;
    StoreLo4(dst, vreinterpret_u8_u32(vget_low_u32(d0.val[1])));
    dst += stride;
    StoreLo4(dst, vreinterpret_u8_u32(vget_high_u32(d0.val[1])));
    dst += stride;
    StoreLo4(dst, vreinterpret_u8_u32(vget_low_u32(d1.val[1])));
    dst += stride;
    StoreLo4(dst, vreinterpret_u8_u32(vget_high_u32(d1.val[1])));
  } else {
    vst1_u8(dst, vreinterpret_u8_u32(vget_low_u32(d0.val[0])));
    dst += stride;
    vst1_u8(dst, vreinterpret_u8_u32(vget_high_u32(d0.val[0])));
    dst += stride;
    vst1_u8(dst, vreinterpret_u8_u32(vget_low_u32(d1.val[0])));
    dst += stride;
    vst1_u8(dst, vreinterpret_u8_u32(vget_high_u32(d1.val[0])));
    if (height == 4) return;
    dst += stride;
    vst1_u8(dst, vreinterpret_u8_u32(vget_low_u32(d0.val[1])));
    dst += stride;
    vst1_u8(dst, vreinterpret_u8_u32(vget_high_u32(d0.val[1])));
    dst += stride;
    vst1_u8(dst, vreinterpret_u8_u32(vget_low_u32(d1.val[1])));
    dst += stride;
    vst1_u8(dst, vreinterpret_u8_u32(vget_high_u32(d1.val[1])));
  }
}

// Because the source values "move backwards" as the row index increases, the
// indices derived from ystep are generally negative. This is accommodated by
// making sure the relative indices are within [-15, 0] when the function is
// called, and sliding them into the inclusive range [0, 15], relative to a
// lower base address.
constexpr int kPositiveIndexOffset = 15;

// Process 4 or 8 |width| by any |height|.
template <int width>
inline void DirectionalZone2FromLeftCol_WxH(uint8_t* dst,
                                            const ptrdiff_t stride,
                                            const int height,
                                            const uint8_t* const left_column,
                                            const int16x8_t left_y,
                                            const int upsample_shift) {
  assert(width == 4 || width == 8);

  // The shift argument must be a constant.
  int16x8_t offset_y, shift_upsampled = left_y;
  if (upsample_shift) {
    offset_y = vshrq_n_s16(left_y, 5);
    shift_upsampled = vshlq_n_s16(shift_upsampled, 1);
  } else {
    offset_y = vshrq_n_s16(left_y, 6);
  }

  // Select values to the left of the starting point.
  // The 15th element (and 16th) will be all the way at the end, to the right.
  // With a negative ystep everything else will be "left" of them.
  // This supports cumulative steps up to 15. We could support up to 16 by doing
  // separate loads for |left_values| and |right_values|. vtbl supports 2 Q
  // registers as input which would allow for cumulative offsets of 32.
  const int16x8_t sampler =
      vaddq_s16(offset_y, vdupq_n_s16(kPositiveIndexOffset));
  const uint8x8_t left_values = vqmovun_s16(sampler);
  const uint8x8_t right_values = vadd_u8(left_values, vdup_n_u8(1));

  const int16x8_t shift_masked = vandq_s16(shift_upsampled, vdupq_n_s16(0x3f));
  const uint8x8_t shift_mul = vreinterpret_u8_s8(vshrn_n_s16(shift_masked, 1));
  const uint8x8_t inv_shift_mul = vsub_u8(vdup_n_u8(32), shift_mul);

  int y = 0;
  do {
    uint8x8_t src_left, src_right;
    LoadStepwise(left_column - kPositiveIndexOffset + (y << upsample_shift),
                 left_values, right_values, &src_left, &src_right);
    const uint8x8_t val =
        WeightedBlend(src_left, src_right, inv_shift_mul, shift_mul);

    if (width == 4) {
      StoreLo4(dst, val);
    } else {
      vst1_u8(dst, val);
    }
    dst += stride;
  } while (++y < height);
}

// Process 4 or 8 |width| by any |height|.
template <int width>
inline void DirectionalZone1Blend_WxH(uint8_t* dest, const ptrdiff_t stride,
                                      const int height,
                                      const uint8_t* const top_row,
                                      int zone_bounds, int top_x,
                                      const int xstep,
                                      const int upsample_shift) {
  assert(width == 4 || width == 8);

  const int scale_bits_x = 6 - upsample_shift;

  const uint8x8_t all = vcreate_u8(0x0706050403020100);
  const uint8x8_t even = vcreate_u8(0x0e0c0a0806040200);
  const uint8x8_t base_step = upsample_shift ? even : all;
  const uint8x8_t right_step = vadd_u8(base_step, vdup_n_u8(1));

  int y = 0;
  do {
    const uint8_t* const src = top_row + (top_x >> scale_bits_x);
    uint8x8_t left, right;
    LoadStepwise(src, base_step, right_step, &left, &right);

    const uint8_t shift = ((top_x << upsample_shift) & 0x3f) >> 1;
    const uint8x8_t val = WeightedBlend(left, right, shift);

    uint8x8_t dst_blend = vld1_u8(dest);
    // |zone_bounds| values can be negative.
    uint8x8_t blend =
        vcge_s8(vreinterpret_s8_u8(all), vdup_n_s8((zone_bounds >> 6)));
    uint8x8_t output = vbsl_u8(blend, val, dst_blend);

    if (width == 4) {
      StoreLo4(dest, output);
    } else {
      vst1_u8(dest, output);
    }
    dest += stride;
    zone_bounds += xstep;
    top_x -= xstep;
  } while (++y < height);
}

// The height at which a load of 16 bytes will not contain enough source pixels
// from |left_column| to supply an accurate row when computing 8 pixels at a
// time. The values are found by inspection. By coincidence, all angles that
// satisfy (ystep >> 6) == 2 map to the same value, so it is enough to look up
// by ystep >> 6. The largest index for this lookup is 1023 >> 6 == 15.
constexpr int kDirectionalZone2ShuffleInvalidHeight[16] = {
    1024, 1024, 16, 16, 16, 16, 0, 0, 18, 0, 0, 0, 0, 0, 0, 40};

// 7.11.2.4 (8) 90 < angle > 180
// The strategy for these functions (4xH and 8+xH) is to know how many blocks
// can be processed with just pixels from |top_ptr|, then handle mixed blocks,
// then handle only blocks that take from |left_ptr|. Additionally, a fast
// index-shuffle approach is used for pred values from |left_column| in sections
// that permit it.
inline void DirectionalZone2_4xH(uint8_t* dst, const ptrdiff_t stride,
                                 const uint8_t* const top_row,
                                 const uint8_t* const left_column,
                                 const int height, const int xstep,
                                 const int ystep, const bool upsampled_top,
                                 const bool upsampled_left) {
  const int upsample_left_shift = static_cast<int>(upsampled_left);
  const int upsample_top_shift = static_cast<int>(upsampled_top);

  // Helper vector.
  const int16x8_t zero_to_seven = {0, 1, 2, 3, 4, 5, 6, 7};

  // Loop incrementers for moving by block (4xN). Vertical still steps by 8. If
  // it's only 4, it will be finished in the first iteration.
  const ptrdiff_t stride8 = stride << 3;
  const int xstep8 = xstep << 3;

  const int min_height = (height == 4) ? 4 : 8;

  // All columns from |min_top_only_x| to the right will only need |top_row| to
  // compute and can therefore call the Zone1 functions. This assumes |xstep| is
  // at least 3.
  assert(xstep >= 3);
  const int min_top_only_x = std::min((height * xstep) >> 6, /* width */ 4);

  // For steep angles, the source pixels from |left_column| may not fit in a
  // 16-byte load for shuffling.
  // TODO(petersonab): Find a more precise formula for this subject to x.
  // TODO(johannkoenig): Revisit this for |width| == 4.
  const int max_shuffle_height =
      std::min(kDirectionalZone2ShuffleInvalidHeight[ystep >> 6], height);

  // Offsets the original zone bound value to simplify x < (y+1)*xstep/64 -1
  int xstep_bounds_base = (xstep == 64) ? 0 : xstep - 1;

  const int left_base_increment = ystep >> 6;
  const int ystep_remainder = ystep & 0x3F;

  // If the 64 scaling is regarded as a decimal point, the first value of the
  // left_y vector omits the portion which is covered under the left_column
  // offset. The following values need the full ystep as a relative offset.
  int16x8_t left_y = vmulq_n_s16(zero_to_seven, -ystep);
  left_y = vaddq_s16(left_y, vdupq_n_s16(-ystep_remainder));

  // This loop treats each set of 4 columns in 3 stages with y-value boundaries.
  // The first stage, before the first y-loop, covers blocks that are only
  // computed from the top row. The second stage, comprising two y-loops, covers
  // blocks that have a mixture of values computed from top or left. The final
  // stage covers blocks that are only computed from the left.
  if (min_top_only_x > 0) {
    // Round down to the nearest multiple of 8.
    // TODO(johannkoenig): This never hits for Wx4 blocks but maybe it should.
    const int max_top_only_y = std::min((1 << 6) / xstep, height) & ~7;
    DirectionalZone1_WxH<4>(dst, stride, max_top_only_y, top_row, -xstep,
                            upsampled_top);

    if (max_top_only_y == height) return;

    int y = max_top_only_y;
    dst += stride * y;
    const int xstep_y = xstep * y;

    // All rows from |min_left_only_y| down for this set of columns only need
    // |left_column| to compute.
    const int min_left_only_y = std::min((4 << 6) / xstep, height);
    // At high angles such that min_left_only_y < 8, ystep is low and xstep is
    // high. This means that max_shuffle_height is unbounded and xstep_bounds
    // will overflow in 16 bits. This is prevented by stopping the first
    // blending loop at min_left_only_y for such cases, which means we skip over
    // the second blending loop as well.
    const int left_shuffle_stop_y =
        std::min(max_shuffle_height, min_left_only_y);
    int xstep_bounds = xstep_bounds_base + xstep_y;
    int top_x = -xstep - xstep_y;

    // +8 increment is OK because if height is 4 this only goes once.
    for (; y < left_shuffle_stop_y;
         y += 8, dst += stride8, xstep_bounds += xstep8, top_x -= xstep8) {
      DirectionalZone2FromLeftCol_WxH<4>(
          dst, stride, min_height,
          left_column + ((y - left_base_increment) << upsample_left_shift),
          left_y, upsample_left_shift);

      DirectionalZone1Blend_WxH<4>(dst, stride, min_height, top_row,
                                   xstep_bounds, top_x, xstep,
                                   upsample_top_shift);
    }

    // Pick up from the last y-value, using the slower but secure method for
    // left prediction.
    const int16_t base_left_y = vgetq_lane_s16(left_y, 0);
    for (; y < min_left_only_y;
         y += 8, dst += stride8, xstep_bounds += xstep8, top_x -= xstep8) {
      DirectionalZone3_WxH<4>(
          dst, stride, min_height,
          left_column + ((y - left_base_increment) << upsample_left_shift),
          base_left_y, -ystep, upsample_left_shift);

      DirectionalZone1Blend_WxH<4>(dst, stride, min_height, top_row,
                                   xstep_bounds, top_x, xstep,
                                   upsample_top_shift);
    }
    // Loop over y for left_only rows.
    for (; y < height; y += 8, dst += stride8) {
      DirectionalZone3_WxH<4>(
          dst, stride, min_height,
          left_column + ((y - left_base_increment) << upsample_left_shift),
          base_left_y, -ystep, upsample_left_shift);
    }
  } else {
    DirectionalZone1_WxH<4>(dst, stride, height, top_row, -xstep,
                            upsampled_top);
  }
}

// Process a multiple of 8 |width|.
inline void DirectionalZone2_8(uint8_t* const dst, const ptrdiff_t stride,
                               const uint8_t* const top_row,
                               const uint8_t* const left_column,
                               const int width, const int height,
                               const int xstep, const int ystep,
                               const bool upsampled_top,
                               const bool upsampled_left) {
  const int upsample_left_shift = static_cast<int>(upsampled_left);
  const int upsample_top_shift = static_cast<int>(upsampled_top);

  // Helper vector.
  const int16x8_t zero_to_seven = {0, 1, 2, 3, 4, 5, 6, 7};

  // Loop incrementers for moving by block (8x8). This function handles blocks
  // with height 4 as well. They are calculated in one pass so these variables
  // do not get used.
  const ptrdiff_t stride8 = stride << 3;
  const int xstep8 = xstep << 3;
  const int ystep8 = ystep << 3;

  // Process Wx4 blocks.
  const int min_height = (height == 4) ? 4 : 8;

  // All columns from |min_top_only_x| to the right will only need |top_row| to
  // compute and can therefore call the Zone1 functions. This assumes |xstep| is
  // at least 3.
  assert(xstep >= 3);
  const int min_top_only_x = std::min((height * xstep) >> 6, width);

  // For steep angles, the source pixels from |left_column| may not fit in a
  // 16-byte load for shuffling.
  // TODO(petersonab): Find a more precise formula for this subject to x.
  const int max_shuffle_height =
      std::min(kDirectionalZone2ShuffleInvalidHeight[ystep >> 6], height);

  // Offsets the original zone bound value to simplify x < (y+1)*xstep/64 -1
  int xstep_bounds_base = (xstep == 64) ? 0 : xstep - 1;

  const int left_base_increment = ystep >> 6;
  const int ystep_remainder = ystep & 0x3F;

  const int left_base_increment8 = ystep8 >> 6;
  const int ystep_remainder8 = ystep8 & 0x3F;
  const int16x8_t increment_left8 = vdupq_n_s16(ystep_remainder8);

  // If the 64 scaling is regarded as a decimal point, the first value of the
  // left_y vector omits the portion which is covered under the left_column
  // offset. Following values need the full ystep as a relative offset.
  int16x8_t left_y = vmulq_n_s16(zero_to_seven, -ystep);
  left_y = vaddq_s16(left_y, vdupq_n_s16(-ystep_remainder));

  // This loop treats each set of 4 columns in 3 stages with y-value boundaries.
  // The first stage, before the first y-loop, covers blocks that are only
  // computed from the top row. The second stage, comprising two y-loops, covers
  // blocks that have a mixture of values computed from top or left. The final
  // stage covers blocks that are only computed from the left.
  int x = 0;
  for (int left_offset = -left_base_increment; x < min_top_only_x; x += 8,
           xstep_bounds_base -= (8 << 6),
           left_y = vsubq_s16(left_y, increment_left8),
           left_offset -= left_base_increment8) {
    uint8_t* dst_x = dst + x;

    // Round down to the nearest multiple of 8.
    const int max_top_only_y = std::min(((x + 1) << 6) / xstep, height) & ~7;
    DirectionalZone1_WxH<8>(dst_x, stride, max_top_only_y,
                            top_row + (x << upsample_top_shift), -xstep,
                            upsampled_top);

    if (max_top_only_y == height) continue;

    int y = max_top_only_y;
    dst_x += stride * y;
    const int xstep_y = xstep * y;

    // All rows from |min_left_only_y| down for this set of columns only need
    // |left_column| to compute.
    const int min_left_only_y = std::min(((x + 8) << 6) / xstep, height);
    // At high angles such that min_left_only_y < 8, ystep is low and xstep is
    // high. This means that max_shuffle_height is unbounded and xstep_bounds
    // will overflow in 16 bits. This is prevented by stopping the first
    // blending loop at min_left_only_y for such cases, which means we skip over
    // the second blending loop as well.
    const int left_shuffle_stop_y =
        std::min(max_shuffle_height, min_left_only_y);
    int xstep_bounds = xstep_bounds_base + xstep_y;
    int top_x = -xstep - xstep_y;

    for (; y < left_shuffle_stop_y;
         y += 8, dst_x += stride8, xstep_bounds += xstep8, top_x -= xstep8) {
      DirectionalZone2FromLeftCol_WxH<8>(
          dst_x, stride, min_height,
          left_column + ((left_offset + y) << upsample_left_shift), left_y,
          upsample_left_shift);

      DirectionalZone1Blend_WxH<8>(
          dst_x, stride, min_height, top_row + (x << upsample_top_shift),
          xstep_bounds, top_x, xstep, upsample_top_shift);
    }

    // Pick up from the last y-value, using the slower but secure method for
    // left prediction.
    const int16_t base_left_y = vgetq_lane_s16(left_y, 0);
    for (; y < min_left_only_y;
         y += 8, dst_x += stride8, xstep_bounds += xstep8, top_x -= xstep8) {
      DirectionalZone3_WxH<8>(
          dst_x, stride, min_height,
          left_column + ((left_offset + y) << upsample_left_shift), base_left_y,
          -ystep, upsample_left_shift);

      DirectionalZone1Blend_WxH<8>(
          dst_x, stride, min_height, top_row + (x << upsample_top_shift),
          xstep_bounds, top_x, xstep, upsample_top_shift);
    }
    // Loop over y for left_only rows.
    for (; y < height; y += 8, dst_x += stride8) {
      DirectionalZone3_WxH<8>(
          dst_x, stride, min_height,
          left_column + ((left_offset + y) << upsample_left_shift), base_left_y,
          -ystep, upsample_left_shift);
    }
  }
  // TODO(johannkoenig): May be able to remove this branch.
  if (x < width) {
    DirectionalZone1_WxH(dst + x, stride, width - x, height,
                         top_row + (x << upsample_top_shift), -xstep,
                         upsampled_top);
  }
}

void DirectionalIntraPredictorZone2_NEON(
    void* const dest, const ptrdiff_t stride, const void* const top_row,
    const void* const left_column, const int width, const int height,
    const int xstep, const int ystep, const bool upsampled_top,
    const bool upsampled_left) {
  // Increasing the negative buffer for this function allows more rows to be
  // processed at a time without branching in an inner loop to check the base.
  uint8_t top_buffer[288];
  uint8_t left_buffer[288];
  memcpy(top_buffer + 128, static_cast<const uint8_t*>(top_row) - 16, 160);
  memcpy(left_buffer + 128, static_cast<const uint8_t*>(left_column) - 16, 160);
  const uint8_t* top_ptr = top_buffer + 144;
  const uint8_t* left_ptr = left_buffer + 144;
  auto* dst = static_cast<uint8_t*>(dest);

  if (width == 4) {
    DirectionalZone2_4xH(dst, stride, top_ptr, left_ptr, height, xstep, ystep,
                         upsampled_top, upsampled_left);
  } else {
    DirectionalZone2_8(dst, stride, top_ptr, left_ptr, width, height, xstep,
                       ystep, upsampled_top, upsampled_left);
  }
}

void DirectionalIntraPredictorZone3_NEON(void* const dest,
                                         const ptrdiff_t stride,
                                         const void* const left_column,
                                         const int width, const int height,
                                         const int ystep,
                                         const bool upsampled_left) {
  const auto* const left = static_cast<const uint8_t*>(left_column);

  assert(ystep > 0);

  const int upsample_shift = static_cast<int>(upsampled_left);
  const int scale_bits = 6 - upsample_shift;
  const int base_step = 1 << upsample_shift;

  if (width == 4 || height == 4) {
    // This block can handle all sizes but the specializations for other sizes
    // are faster.
    const uint8x8_t all = vcreate_u8(0x0706050403020100);
    const uint8x8_t even = vcreate_u8(0x0e0c0a0806040200);
    const uint8x8_t base_step_v = upsampled_left ? even : all;
    const uint8x8_t right_step = vadd_u8(base_step_v, vdup_n_u8(1));

    int y = 0;
    do {
      int x = 0;
      do {
        uint8_t* dst = static_cast<uint8_t*>(dest);
        dst += y * stride + x;
        uint8x8_t left_v[4], right_v[4], value_v[4];
        const int ystep_base = ystep * x;
        const int offset = y * base_step;

        const int index_0 = ystep_base + ystep * 1;
        LoadStepwise(left + offset + (index_0 >> scale_bits), base_step_v,
                     right_step, &left_v[0], &right_v[0]);
        value_v[0] = WeightedBlend(left_v[0], right_v[0],
                                   ((index_0 << upsample_shift) & 0x3F) >> 1);

        const int index_1 = ystep_base + ystep * 2;
        LoadStepwise(left + offset + (index_1 >> scale_bits), base_step_v,
                     right_step, &left_v[1], &right_v[1]);
        value_v[1] = WeightedBlend(left_v[1], right_v[1],
                                   ((index_1 << upsample_shift) & 0x3F) >> 1);

        const int index_2 = ystep_base + ystep * 3;
        LoadStepwise(left + offset + (index_2 >> scale_bits), base_step_v,
                     right_step, &left_v[2], &right_v[2]);
        value_v[2] = WeightedBlend(left_v[2], right_v[2],
                                   ((index_2 << upsample_shift) & 0x3F) >> 1);

        const int index_3 = ystep_base + ystep * 4;
        LoadStepwise(left + offset + (index_3 >> scale_bits), base_step_v,
                     right_step, &left_v[3], &right_v[3]);
        value_v[3] = WeightedBlend(left_v[3], right_v[3],
                                   ((index_3 << upsample_shift) & 0x3F) >> 1);

        // 8x4 transpose.
        const uint8x8x2_t b0 = vtrn_u8(value_v[0], value_v[1]);
        const uint8x8x2_t b1 = vtrn_u8(value_v[2], value_v[3]);

        const uint16x4x2_t c0 = vtrn_u16(vreinterpret_u16_u8(b0.val[0]),
                                         vreinterpret_u16_u8(b1.val[0]));
        const uint16x4x2_t c1 = vtrn_u16(vreinterpret_u16_u8(b0.val[1]),
                                         vreinterpret_u16_u8(b1.val[1]));

        StoreLo4(dst, vreinterpret_u8_u16(c0.val[0]));
        dst += stride;
        StoreLo4(dst, vreinterpret_u8_u16(c1.val[0]));
        dst += stride;
        StoreLo4(dst, vreinterpret_u8_u16(c0.val[1]));
        dst += stride;
        StoreLo4(dst, vreinterpret_u8_u16(c1.val[1]));

        if (height > 4) {
          dst += stride;
          StoreHi4(dst, vreinterpret_u8_u16(c0.val[0]));
          dst += stride;
          StoreHi4(dst, vreinterpret_u8_u16(c1.val[0]));
          dst += stride;
          StoreHi4(dst, vreinterpret_u8_u16(c0.val[1]));
          dst += stride;
          StoreHi4(dst, vreinterpret_u8_u16(c1.val[1]));
        }
        x += 4;
      } while (x < width);
      y += 8;
    } while (y < height);
  } else {  // 8x8 at a time.
    // Limited improvement for 8x8. ~20% faster for 64x64.
    int y = 0;
    do {
      int x = 0;
      do {
        uint8_t* dst = static_cast<uint8_t*>(dest);
        dst += y * stride + x;
        const int ystep_base = ystep * (x + 1);

        DirectionalZone3_WxH<8>(dst, stride, 8, left + (y << upsample_shift),
                                ystep_base, ystep, upsample_shift);
        x += 8;
      } while (x < width);
      y += 8;
    } while (y < height);
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
  dsp->directional_intra_predictor_zone1 = DirectionalIntraPredictorZone1_NEON;
  dsp->directional_intra_predictor_zone2 = DirectionalIntraPredictorZone2_NEON;
  dsp->directional_intra_predictor_zone3 = DirectionalIntraPredictorZone3_NEON;
}

}  // namespace
}  // namespace low_bitdepth

void IntraPredDirectionalInit_NEON() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_NEON
namespace libgav1 {
namespace dsp {

void IntraPredDirectionalInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON
