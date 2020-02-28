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

#include "src/dsp/mask_blend.h"
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_NEON

#include <arm_neon.h>

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/dsp/arm/common_neon.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

// TODO(b/150461164): Consider combining with GetInterIntraMask4x2().
// Compound predictors use int16_t values and need to multiply long because the
// Convolve range * 64 is 20 bits. Unfortunately there is no multiply int16_t by
// int8_t and accumulate into int32_t instruction.
template <int subsampling_x, int subsampling_y>
inline int16x8_t GetMask4x2(const uint8_t* mask, ptrdiff_t mask_stride) {
  if (subsampling_x == 1) {
    const int16x4_t mask_val0 = vreinterpret_s16_u16(vpaddl_u8(vld1_u8(mask)));
    const int16x4_t mask_val1 = vreinterpret_s16_u16(
        vpaddl_u8(vld1_u8(mask + (mask_stride << subsampling_y))));
    int16x8_t final_val;
    if (subsampling_y == 1) {
      const int16x4_t next_mask_val0 =
          vreinterpret_s16_u16(vpaddl_u8(vld1_u8(mask + mask_stride)));
      const int16x4_t next_mask_val1 =
          vreinterpret_s16_u16(vpaddl_u8(vld1_u8(mask + mask_stride * 3)));
      final_val = vaddq_s16(vcombine_s16(mask_val0, mask_val1),
                            vcombine_s16(next_mask_val0, next_mask_val1));
    } else {
      final_val = vreinterpretq_s16_u16(
          vpaddlq_u8(vreinterpretq_u8_s16(vcombine_s16(mask_val0, mask_val1))));
    }
    return vrshrq_n_s16(final_val, subsampling_y + 1);
  }
  assert(subsampling_y == 0 && subsampling_x == 0);
  const uint8x8_t mask_val0 = Load4(mask);
  const uint8x8_t mask_val =
      Load4<1>(mask + (mask_stride << subsampling_y), mask_val0);
  return vreinterpretq_s16_u16(vmovl_u8(mask_val));
}

template <int subsampling_x, int subsampling_y>
inline int16x8_t GetMask8(const uint8_t* mask, ptrdiff_t mask_stride) {
  if (subsampling_x == 1) {
    int16x8_t mask_val = vreinterpretq_s16_u16(vpaddlq_u8(vld1q_u8(mask)));
    if (subsampling_y == 1) {
      const int16x8_t next_mask_val =
          vreinterpretq_s16_u16(vpaddlq_u8(vld1q_u8(mask + mask_stride)));
      mask_val = vaddq_s16(mask_val, next_mask_val);
    }
    return vrshrq_n_s16(mask_val, 1 + subsampling_y);
  }
  assert(subsampling_y == 0 && subsampling_x == 0);
  const uint8x8_t mask_val = vld1_u8(mask);
  return vreinterpretq_s16_u16(vmovl_u8(mask_val));
}

inline void WriteMaskBlendLine4x2(const int16_t* const pred_0,
                                  const int16_t* const pred_1,
                                  const int16x8_t pred_mask_0,
                                  const int16x8_t pred_mask_1, uint8_t* dst,
                                  const ptrdiff_t dst_stride) {
  const int16x4_t pred_val_0_lo = vld1_s16(pred_0);
  const int16x4_t pred_val_0_hi = vld1_s16(pred_0 + 4);
  const int16x4_t pred_val_1_lo = vld1_s16(pred_1);
  const int16x4_t pred_val_1_hi = vld1_s16(pred_1 + 4);
  // int res = (mask_value * prediction_0[x] +
  //      (64 - mask_value) * prediction_1[x]) >> 6;
  const int32x4_t weighted_pred_0_lo =
      vmull_s16(vget_low_s16(pred_mask_0), pred_val_0_lo);
  const int32x4_t weighted_pred_0_hi =
      vmull_s16(vget_high_s16(pred_mask_0), pred_val_0_hi);
  const int32x4_t weighted_combo_lo =
      vmlal_s16(weighted_pred_0_lo, vget_low_s16(pred_mask_1), pred_val_1_lo);
  const int32x4_t weighted_combo_hi =
      vmlal_s16(weighted_pred_0_hi, vget_high_s16(pred_mask_1), pred_val_1_hi);
  // dst[x] = static_cast<Pixel>(
  //     Clip3(RightShiftWithRounding(res, inter_post_round_bits), 0,
  //         (1 << kBitdepth8) - 1));
  const uint8x8_t result =
      vqrshrun_n_s16(vcombine_s16(vshrn_n_s32(weighted_combo_lo, 6),
                                  vshrn_n_s32(weighted_combo_hi, 6)),
                     4);
  StoreLo4(dst, result);
  StoreHi4(dst + dst_stride, result);
}

template <int subsampling_x, int subsampling_y>
inline void MaskBlending4x4_NEON(const int16_t* pred_0, const int16_t* pred_1,
                                 const uint8_t* mask,
                                 const ptrdiff_t mask_stride, uint8_t* dst,
                                 const ptrdiff_t dst_stride) {
  const int16x8_t mask_inverter = vdupq_n_s16(64);
  int16x8_t pred_mask_0 =
      GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
  int16x8_t pred_mask_1 = vsubq_s16(mask_inverter, pred_mask_0);
  WriteMaskBlendLine4x2(pred_0, pred_1, pred_mask_0, pred_mask_1, dst,
                        dst_stride);
  // TODO(b/150461164): Arm tends to do better with load(val); val += stride
  // It may be possible to turn this into a loop with a templated height.
  pred_0 += 4 << 1;
  pred_1 += 4 << 1;
  mask += mask_stride << (1 + subsampling_y);
  dst += dst_stride << 1;

  pred_mask_0 = GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
  pred_mask_1 = vsubq_s16(mask_inverter, pred_mask_0);
  WriteMaskBlendLine4x2(pred_0, pred_1, pred_mask_0, pred_mask_1, dst,
                        dst_stride);
}

template <int subsampling_x, int subsampling_y>
inline void MaskBlending4xH_NEON(const int16_t* pred_0, const int16_t* pred_1,
                                 const uint8_t* const mask_ptr,
                                 const ptrdiff_t mask_stride, const int height,
                                 uint8_t* dst, const ptrdiff_t dst_stride) {
  const uint8_t* mask = mask_ptr;
  if (height == 4) {
    MaskBlending4x4_NEON<subsampling_x, subsampling_y>(
        pred_0, pred_1, mask, mask_stride, dst, dst_stride);
    return;
  }
  const int16x8_t mask_inverter = vdupq_n_s16(64);
  int y = 0;
  do {
    int16x8_t pred_mask_0 =
        GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
    int16x8_t pred_mask_1 = vsubq_s16(mask_inverter, pred_mask_0);

    WriteMaskBlendLine4x2(pred_0, pred_1, pred_mask_0, pred_mask_1, dst,
                          dst_stride);
    pred_0 += 4 << 1;
    pred_1 += 4 << 1;
    mask += mask_stride << (1 + subsampling_y);
    dst += dst_stride << 1;

    pred_mask_0 = GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
    pred_mask_1 = vsubq_s16(mask_inverter, pred_mask_0);
    WriteMaskBlendLine4x2(pred_0, pred_1, pred_mask_0, pred_mask_1, dst,
                          dst_stride);
    pred_0 += 4 << 1;
    pred_1 += 4 << 1;
    mask += mask_stride << (1 + subsampling_y);
    dst += dst_stride << 1;

    pred_mask_0 = GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
    pred_mask_1 = vsubq_s16(mask_inverter, pred_mask_0);
    WriteMaskBlendLine4x2(pred_0, pred_1, pred_mask_0, pred_mask_1, dst,
                          dst_stride);
    pred_0 += 4 << 1;
    pred_1 += 4 << 1;
    mask += mask_stride << (1 + subsampling_y);
    dst += dst_stride << 1;

    pred_mask_0 = GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
    pred_mask_1 = vsubq_s16(mask_inverter, pred_mask_0);
    WriteMaskBlendLine4x2(pred_0, pred_1, pred_mask_0, pred_mask_1, dst,
                          dst_stride);
    pred_0 += 4 << 1;
    pred_1 += 4 << 1;
    mask += mask_stride << (1 + subsampling_y);
    dst += dst_stride << 1;
    y += 8;
  } while (y < height);
}

template <int subsampling_x, int subsampling_y>
inline void MaskBlend_NEON(const void* prediction_0, const void* prediction_1,
                           const ptrdiff_t /*prediction_stride_1*/,
                           const uint8_t* const mask_ptr,
                           const ptrdiff_t mask_stride, const int width,
                           const int height, void* dest,
                           const ptrdiff_t dst_stride) {
  auto* dst = static_cast<uint8_t*>(dest);
  const auto* pred_0 = static_cast<const int16_t*>(prediction_0);
  const auto* pred_1 = static_cast<const int16_t*>(prediction_1);
  if (width == 4) {
    MaskBlending4xH_NEON<subsampling_x, subsampling_y>(
        pred_0, pred_1, mask_ptr, mask_stride, height, dst, dst_stride);
    return;
  }
  const uint8_t* mask = mask_ptr;
  const int16x8_t mask_inverter = vdupq_n_s16(64);
  int y = 0;
  do {
    int x = 0;
    do {
      const int16x8_t pred_mask_0 = GetMask8<subsampling_x, subsampling_y>(
          mask + (x << subsampling_x), mask_stride);
      // 64 - mask
      const int16x8_t pred_mask_1 = vsubq_s16(mask_inverter, pred_mask_0);
      const int16x8_t pred_val_0 = vld1q_s16(pred_0 + x);
      const int16x8_t pred_val_1 = vld1q_s16(pred_1 + x);
      uint8x8_t result;
      // int res = (mask_value * prediction_0[x] +
      //      (64 - mask_value) * prediction_1[x]) >> 6;
      const int32x4_t weighted_pred_0_lo =
          vmull_s16(vget_low_s16(pred_mask_0), vget_low_s16(pred_val_0));
      const int32x4_t weighted_pred_0_hi =
          vmull_s16(vget_high_s16(pred_mask_0), vget_high_s16(pred_val_0));
      const int32x4_t weighted_combo_lo =
          vmlal_s16(weighted_pred_0_lo, vget_low_s16(pred_mask_1),
                    vget_low_s16(pred_val_1));
      const int32x4_t weighted_combo_hi =
          vmlal_s16(weighted_pred_0_hi, vget_high_s16(pred_mask_1),
                    vget_high_s16(pred_val_1));

      // dst[x] = static_cast<Pixel>(
      //     Clip3(RightShiftWithRounding(res, inter_post_round_bits), 0,
      //           (1 << kBitdepth8) - 1));
      result = vqrshrun_n_s16(vcombine_s16(vshrn_n_s32(weighted_combo_lo, 6),
                                           vshrn_n_s32(weighted_combo_hi, 6)),
                              4);
      vst1_u8(dst + x, result);

      x += 8;
    } while (x < width);
    dst += dst_stride;
    pred_0 += width;
    pred_1 += width;
    mask += mask_stride << subsampling_y;
  } while (++y < height);
}

// TODO(b/150461164): This is much faster for inter_intra (input is Pixel
// values) but regresses compound versions (input is int16_t). Try to
// consolidate these.
template <int subsampling_x, int subsampling_y>
inline uint8x8_t GetInterIntraMask4x2(const uint8_t* mask,
                                      ptrdiff_t mask_stride) {
  if (subsampling_x == 1) {
    const uint8x8_t mask_val =
        vpadd_u8(vld1_u8(mask), vld1_u8(mask + (mask_stride << subsampling_y)));
    if (subsampling_y == 1) {
      const uint8x8_t next_mask_val = vpadd_u8(vld1_u8(mask + mask_stride),
                                               vld1_u8(mask + mask_stride * 3));

      // Use a saturating add to work around the case where all |mask| values
      // are 64. Together with the rounding shift this ensures the correct
      // result.
      const uint8x8_t sum = vqadd_u8(mask_val, next_mask_val);
      return vrshr_n_u8(sum, /*subsampling_x=*/1 + subsampling_y);
    }

    return vrshr_n_u8(mask_val, /*subsampling_x=*/1);
  }

  assert(subsampling_y == 0 && subsampling_x == 0);
  const uint8x8_t mask_val0 = Load4(mask);
  // TODO(b/150461164): Investigate the source of |mask| and see if the stride
  // can be removed.
  // TODO(b/150461164): The unit tests start at 8x8. Does this get run?
  return Load4<1>(mask + (mask_stride << subsampling_y), mask_val0);
}

template <int subsampling_x, int subsampling_y>
inline uint8x8_t GetInterIntraMask8(const uint8_t* mask,
                                    ptrdiff_t mask_stride) {
  if (subsampling_x == 1) {
    const uint8x16_t mask_val = vld1q_u8(mask);
    const uint8x8_t mask_paired =
        vpadd_u8(vget_low_u8(mask_val), vget_high_u8(mask_val));
    if (subsampling_y == 1) {
      const uint8x16_t next_mask_val = vld1q_u8(mask + mask_stride);
      const uint8x8_t next_mask_paired =
          vpadd_u8(vget_low_u8(next_mask_val), vget_high_u8(next_mask_val));

      // Use a saturating add to work around the case where all |mask| values
      // are 64. Together with the rounding shift this ensures the correct
      // result.
      const uint8x8_t sum = vqadd_u8(mask_paired, next_mask_paired);
      return vrshr_n_u8(sum, /*subsampling_x=*/1 + subsampling_y);
    }

    return vrshr_n_u8(mask_paired, /*subsampling_x=*/1);
  }

  assert(subsampling_y == 0 && subsampling_x == 0);
  return vld1_u8(mask);
}

inline void InterIntraWriteMaskBlendLine8bpp4x2(const uint8_t* const pred_0,
                                                uint8_t* const pred_1,
                                                const ptrdiff_t pred_stride_1,
                                                const uint8x8_t pred_mask_0,
                                                const uint8x8_t pred_mask_1) {
  const uint8x8_t pred_val_0 = vld1_u8(pred_0);
  uint8x8_t pred_val_1 = Load4(pred_1);
  pred_val_1 = Load4<1>(pred_1 + pred_stride_1, pred_val_1);

  const uint16x8_t weighted_pred_0 = vmull_u8(pred_mask_0, pred_val_0);
  const uint16x8_t weighted_combo =
      vmlal_u8(weighted_pred_0, pred_mask_1, pred_val_1);
  const uint8x8_t result = vrshrn_n_u16(weighted_combo, 6);
  StoreLo4(pred_1, result);
  StoreHi4(pred_1 + pred_stride_1, result);
}

template <int subsampling_x, int subsampling_y>
inline void InterIntraMaskBlending8bpp4x4_NEON(const uint8_t* pred_0,
                                               uint8_t* pred_1,
                                               const ptrdiff_t pred_stride_1,
                                               const uint8_t* mask,
                                               const ptrdiff_t mask_stride) {
  const uint8x8_t mask_inverter = vdup_n_u8(64);
  uint8x8_t pred_mask_1 =
      GetInterIntraMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
  uint8x8_t pred_mask_0 = vsub_u8(mask_inverter, pred_mask_1);
  InterIntraWriteMaskBlendLine8bpp4x2(pred_0, pred_1, pred_stride_1,
                                      pred_mask_0, pred_mask_1);
  pred_0 += 4 << 1;
  pred_1 += pred_stride_1 << 1;
  mask += mask_stride << (1 + subsampling_y);

  pred_mask_1 =
      GetInterIntraMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
  pred_mask_0 = vsub_u8(mask_inverter, pred_mask_1);
  InterIntraWriteMaskBlendLine8bpp4x2(pred_0, pred_1, pred_stride_1,
                                      pred_mask_0, pred_mask_1);
}

template <int subsampling_x, int subsampling_y>
inline void InterIntraMaskBlending8bpp4xH_NEON(
    const uint8_t* pred_0, uint8_t* pred_1, const ptrdiff_t pred_stride_1,
    const uint8_t* mask, const ptrdiff_t mask_stride, const int height) {
  if (height == 4) {
    InterIntraMaskBlending8bpp4x4_NEON<subsampling_x, subsampling_y>(
        pred_0, pred_1, pred_stride_1, mask, mask_stride);
    return;
  }
  int y = 0;
  do {
    InterIntraMaskBlending8bpp4x4_NEON<subsampling_x, subsampling_y>(
        pred_0, pred_1, pred_stride_1, mask, mask_stride);
    pred_0 += 4 << 2;
    pred_1 += pred_stride_1 << 2;
    mask += mask_stride << (2 + subsampling_y);

    InterIntraMaskBlending8bpp4x4_NEON<subsampling_x, subsampling_y>(
        pred_0, pred_1, pred_stride_1, mask, mask_stride);
    pred_0 += 4 << 2;
    pred_1 += pred_stride_1 << 2;
    mask += mask_stride << (2 + subsampling_y);
    y += 8;
  } while (y < height);
}

template <int subsampling_x, int subsampling_y>
inline void InterIntraMaskBlend8bpp_NEON(const uint8_t* prediction_0,
                                         uint8_t* prediction_1,
                                         const ptrdiff_t prediction_stride_1,
                                         const uint8_t* const mask_ptr,
                                         const ptrdiff_t mask_stride,
                                         const int width, const int height) {
  if (width == 4) {
    InterIntraMaskBlending8bpp4xH_NEON<subsampling_x, subsampling_y>(
        prediction_0, prediction_1, prediction_stride_1, mask_ptr, mask_stride,
        height);
    return;
  }
  const uint8_t* mask = mask_ptr;
  const uint8x8_t mask_inverter = vdup_n_u8(64);
  int y = 0;
  do {
    int x = 0;
    do {
      // TODO(b/150461164): Consider a 16 wide specialization (at least for the
      // unsampled version) to take advantage of vld1q_u8().
      const uint8x8_t pred_mask_1 =
          GetInterIntraMask8<subsampling_x, subsampling_y>(
              mask + (x << subsampling_x), mask_stride);
      // 64 - mask
      const uint8x8_t pred_mask_0 = vsub_u8(mask_inverter, pred_mask_1);
      const uint8x8_t pred_val_0 = vld1_u8(prediction_0);
      prediction_0 += 8;
      const uint8x8_t pred_val_1 = vld1_u8(prediction_1 + x);
      const uint16x8_t weighted_pred_0 = vmull_u8(pred_mask_0, pred_val_0);
      // weighted_pred0 + weighted_pred1
      const uint16x8_t weighted_combo =
          vmlal_u8(weighted_pred_0, pred_mask_1, pred_val_1);
      const uint8x8_t result = vrshrn_n_u16(weighted_combo, 6);
      vst1_u8(prediction_1 + x, result);

      x += 8;
    } while (x < width);
    prediction_1 += prediction_stride_1;
    mask += mask_stride << subsampling_y;
  } while (++y < height);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
  dsp->mask_blend[0][0] = MaskBlend_NEON<0, 0>;
  dsp->mask_blend[1][0] = MaskBlend_NEON<1, 0>;
  dsp->mask_blend[2][0] = MaskBlend_NEON<1, 1>;
  // The is_inter_intra index of mask_blend[][] is replaced by
  // inter_intra_mask_blend_8bpp[] in 8-bit.
  dsp->inter_intra_mask_blend_8bpp[0] = InterIntraMaskBlend8bpp_NEON<0, 0>;
  dsp->inter_intra_mask_blend_8bpp[1] = InterIntraMaskBlend8bpp_NEON<1, 0>;
  dsp->inter_intra_mask_blend_8bpp[2] = InterIntraMaskBlend8bpp_NEON<1, 1>;
}

}  // namespace
}  // namespace low_bitdepth

void MaskBlendInit_NEON() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_NEON

namespace libgav1 {
namespace dsp {

void MaskBlendInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON
