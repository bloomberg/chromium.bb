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

// TODO(johannkoenig): The final mask value is [0, 64]. Evaluate use of int16_t
// intermediates.
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
  // TODO(johannkoenig): Remove vmovl().
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
                                  const ptrdiff_t pred_stride_0,
                                  const int16_t* const pred_1,
                                  const ptrdiff_t pred_stride_1,
                                  const int16x8_t pred_mask_0,
                                  const int16x8_t pred_mask_1, uint8_t* dst,
                                  const ptrdiff_t dst_stride) {
  const int16x4_t pred_val_0_lo = vld1_s16(pred_0);
  const int16x4_t pred_val_0_hi = vld1_s16(pred_0 + pred_stride_0);
  const int16x4_t pred_val_1_lo = vld1_s16(pred_1);
  const int16x4_t pred_val_1_hi = vld1_s16(pred_1 + pred_stride_1);
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
inline void MaskBlending4x4_NEON(const int16_t* pred_0,
                                 const ptrdiff_t prediction_stride_0,
                                 const int16_t* pred_1,
                                 const ptrdiff_t prediction_stride_1,
                                 const uint8_t* mask,
                                 const ptrdiff_t mask_stride, uint8_t* dst,
                                 const ptrdiff_t dst_stride) {
  const int16x8_t mask_inverter = vdupq_n_s16(64);
  int16x8_t pred_mask_0 =
      GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
  int16x8_t pred_mask_1 = vsubq_s16(mask_inverter, pred_mask_0);
  WriteMaskBlendLine4x2(pred_0, prediction_stride_0, pred_1,
                        prediction_stride_1, pred_mask_0, pred_mask_1, dst,
                        dst_stride);
  pred_0 += prediction_stride_0 << 1;
  pred_1 += prediction_stride_1 << 1;
  mask += mask_stride << (1 + subsampling_y);
  dst += dst_stride << 1;

  pred_mask_0 = GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
  pred_mask_1 = vsubq_s16(mask_inverter, pred_mask_0);
  WriteMaskBlendLine4x2(pred_0, prediction_stride_0, pred_1,
                        prediction_stride_1, pred_mask_0, pred_mask_1, dst,
                        dst_stride);
}

template <int subsampling_x, int subsampling_y>
inline void MaskBlending4xH_NEON(const int16_t* pred_0,
                                 const ptrdiff_t pred_stride_0,
                                 const int height, const int16_t* pred_1,
                                 const ptrdiff_t pred_stride_1,
                                 const uint8_t* const mask_ptr,
                                 const ptrdiff_t mask_stride, uint8_t* dst,
                                 const ptrdiff_t dst_stride) {
  const uint8_t* mask = mask_ptr;
  if (height == 4) {
    MaskBlending4x4_NEON<subsampling_x, subsampling_y>(
        pred_0, pred_stride_0, pred_1, pred_stride_1, mask, mask_stride, dst,
        dst_stride);
    return;
  }
  const int16x8_t mask_inverter = vdupq_n_s16(64);
  int y = 0;
  do {
    int16x8_t pred_mask_0 =
        GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
    int16x8_t pred_mask_1 = vsubq_s16(mask_inverter, pred_mask_0);

    WriteMaskBlendLine4x2(pred_0, pred_stride_0, pred_1, pred_stride_1,
                          pred_mask_0, pred_mask_1, dst, dst_stride);
    pred_0 += pred_stride_0 << 1;
    pred_1 += pred_stride_1 << 1;
    mask += mask_stride << (1 + subsampling_y);
    dst += dst_stride << 1;

    pred_mask_0 = GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
    pred_mask_1 = vsubq_s16(mask_inverter, pred_mask_0);
    WriteMaskBlendLine4x2(pred_0, pred_stride_0, pred_1, pred_stride_1,
                          pred_mask_0, pred_mask_1, dst, dst_stride);
    pred_0 += pred_stride_0 << 1;
    pred_1 += pred_stride_1 << 1;
    mask += mask_stride << (1 + subsampling_y);
    dst += dst_stride << 1;

    pred_mask_0 = GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
    pred_mask_1 = vsubq_s16(mask_inverter, pred_mask_0);
    WriteMaskBlendLine4x2(pred_0, pred_stride_0, pred_1, pred_stride_1,
                          pred_mask_0, pred_mask_1, dst, dst_stride);
    pred_0 += pred_stride_0 << 1;
    pred_1 += pred_stride_1 << 1;
    mask += mask_stride << (1 + subsampling_y);
    dst += dst_stride << 1;

    pred_mask_0 = GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
    pred_mask_1 = vsubq_s16(mask_inverter, pred_mask_0);
    WriteMaskBlendLine4x2(pred_0, pred_stride_0, pred_1, pred_stride_1,
                          pred_mask_0, pred_mask_1, dst, dst_stride);
    pred_0 += pred_stride_0 << 1;
    pred_1 += pred_stride_1 << 1;
    mask += mask_stride << (1 + subsampling_y);
    dst += dst_stride << 1;
    y += 8;
  } while (y < height);
}

template <int subsampling_x, int subsampling_y>
inline void MaskBlend_NEON(
    const void* prediction_0, const ptrdiff_t prediction_stride_0,
    const void* prediction_1, const ptrdiff_t prediction_stride_1,
    const uint8_t* const mask_ptr, const ptrdiff_t mask_stride, const int width,
    const int height, void* dest, const ptrdiff_t dst_stride) {
  auto* dst = reinterpret_cast<uint8_t*>(dest);
  auto* pred_0 = reinterpret_cast<const int16_t*>(prediction_0);
  auto* pred_1 = reinterpret_cast<const int16_t*>(prediction_1);
  const ptrdiff_t pred_stride_0 = prediction_stride_0;
  const ptrdiff_t pred_stride_1 = prediction_stride_1;
  if (width == 4) {
    MaskBlending4xH_NEON<subsampling_x, subsampling_y>(
        pred_0, pred_stride_0, height, pred_1, pred_stride_1, mask_ptr,
        mask_stride, dst, dst_stride);
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
    pred_0 += pred_stride_0;
    pred_1 += pred_stride_1;
    mask += mask_stride << subsampling_y;
  } while (++y < height);
}

inline void InterIntraWriteMaskBlendLine8bpp4x2(
    const uint8_t* const pred_0, const ptrdiff_t pred_stride_0,
    const uint8_t* const pred_1, const ptrdiff_t pred_stride_1,
    const uint16x8_t pred_mask_0, const uint16x8_t pred_mask_1, uint8_t* dst,
    const ptrdiff_t dst_stride) {
  // TODO(johannkoenig): Use uint8x8_t for |pred_val| and |pred_mask|. Only
  // expand during the calculation.
  const uint8x8_t pred_val_0_lo = Load4(pred_0);
  const uint16x8_t pred_val_0 =
      vmovl_u8(Load4<1>(pred_0 + pred_stride_0, pred_val_0_lo));
  const uint8x8_t pred_val_1_lo = Load4(pred_1);
  const uint16x8_t pred_val_1 =
      vmovl_u8(Load4<1>(pred_1 + pred_stride_1, pred_val_1_lo));

  const uint16x8_t weighted_pred_0 = vmulq_u16(pred_mask_0, pred_val_0);
  const uint16x8_t weighted_combo =
      vmlaq_u16(weighted_pred_0, pred_mask_1, pred_val_1);
  const uint8x8_t result = vrshrn_n_u16(weighted_combo, 6);
  StoreLo4(dst, result);
  StoreHi4(dst + dst_stride, result);
}

template <int subsampling_x, int subsampling_y>
inline void InterIntraMaskBlending8bpp4x4_NEON(
    const uint8_t* pred_0, const ptrdiff_t prediction_stride_0,
    const uint8_t* pred_1, const ptrdiff_t prediction_stride_1,
    const uint8_t* mask, const ptrdiff_t mask_stride, uint8_t* dst,
    const ptrdiff_t dst_stride) {
  const uint16x8_t mask_inverter = vdupq_n_u16(64);
  uint16x8_t pred_mask_0 = vreinterpretq_u16_s16(
      GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride));
  uint16x8_t pred_mask_1 = vsubq_u16(mask_inverter, pred_mask_0);
  InterIntraWriteMaskBlendLine8bpp4x2(pred_0, prediction_stride_0, pred_1,
                                      prediction_stride_1, pred_mask_0,
                                      pred_mask_1, dst, dst_stride);
  pred_0 += prediction_stride_0 << 1;
  pred_1 += prediction_stride_1 << 1;
  mask += mask_stride << (1 + subsampling_y);
  dst += dst_stride << 1;

  pred_mask_0 = vreinterpretq_u16_s16(
      GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride));
  pred_mask_1 = vsubq_u16(mask_inverter, pred_mask_0);
  InterIntraWriteMaskBlendLine8bpp4x2(pred_0, prediction_stride_0, pred_1,
                                      prediction_stride_1, pred_mask_0,
                                      pred_mask_1, dst, dst_stride);
}

template <int subsampling_x, int subsampling_y>
inline void InterIntraMaskBlending8bpp4xH_NEON(
    const uint8_t* pred_0, const ptrdiff_t pred_stride_0, const int height,
    const uint8_t* pred_1, const ptrdiff_t pred_stride_1,
    const uint8_t* const mask_ptr, const ptrdiff_t mask_stride, uint8_t* dst,
    const ptrdiff_t dst_stride) {
  const uint8_t* mask = mask_ptr;
  if (height == 4) {
    InterIntraMaskBlending8bpp4x4_NEON<subsampling_x, subsampling_y>(
        pred_0, pred_stride_0, pred_1, pred_stride_1, mask, mask_stride, dst,
        dst_stride);
    return;
  }
  int y = 0;
  do {
    InterIntraMaskBlending8bpp4x4_NEON<subsampling_x, subsampling_y>(
        pred_0, pred_stride_0, pred_1, pred_stride_1, mask, mask_stride, dst,
        dst_stride);
    pred_0 += pred_stride_0 << 2;
    pred_1 += pred_stride_1 << 2;
    mask += mask_stride << (2 + subsampling_y);
    dst += dst_stride << 2;

    InterIntraMaskBlending8bpp4x4_NEON<subsampling_x, subsampling_y>(
        pred_0, pred_stride_0, pred_1, pred_stride_1, mask, mask_stride, dst,
        dst_stride);
    pred_0 += pred_stride_0 << 2;
    pred_1 += pred_stride_1 << 2;
    mask += mask_stride << (2 + subsampling_y);
    dst += dst_stride << 2;
    y += 8;
  } while (y < height);
}

template <int subsampling_x, int subsampling_y>
inline void InterIntraMaskBlend8bpp_NEON(
    const uint8_t* prediction_0, const ptrdiff_t prediction_stride_0,
    const uint8_t* prediction_1, const ptrdiff_t prediction_stride_1,
    const uint8_t* const mask_ptr, const ptrdiff_t mask_stride, const int width,
    const int height, void* dest, const ptrdiff_t dst_stride) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(dest);
  const uint8_t* pred_0 = prediction_1;
  const uint8_t* pred_1 = prediction_0;
  const ptrdiff_t pred_stride_0 = prediction_stride_1;
  const ptrdiff_t pred_stride_1 = prediction_stride_0;
  if (width == 4) {
    InterIntraMaskBlending8bpp4xH_NEON<subsampling_x, subsampling_y>(
        pred_0, pred_stride_0, height, pred_1, pred_stride_1, mask_ptr,
        mask_stride, dst, dst_stride);
    return;
  }
  const uint8_t* mask = mask_ptr;
  const uint16x8_t mask_inverter = vdupq_n_u16(64);
  int y = 0;
  do {
    int x = 0;
    do {
      const uint16x8_t pred_mask_0 =
          vreinterpretq_u16_s16(GetMask8<subsampling_x, subsampling_y>(
              mask + (x << subsampling_x), mask_stride));
      // 64 - mask
      // TODO(johannkoenig): Don't use vmovl().
      const uint16x8_t pred_mask_1 = vsubq_u16(mask_inverter, pred_mask_0);
      const uint16x8_t pred_val_0 = vmovl_u8(vld1_u8(pred_0 + x));
      const uint16x8_t pred_val_1 = vmovl_u8(vld1_u8(pred_1 + x));
      uint8x8_t result;
      const uint16x8_t weighted_pred_0 = vmulq_u16(pred_mask_0, pred_val_0);
      // weighted_pred0 + weighted_pred1
      const uint16x8_t weighted_combo =
          vmlaq_u16(weighted_pred_0, pred_mask_1, pred_val_1);
      result = vrshrn_n_u16(weighted_combo, 6);
      vst1_u8(dst + x, result);

      x += 8;
    } while (x < width);
    dst += dst_stride;
    pred_0 += pred_stride_0;
    pred_1 += pred_stride_1;
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
