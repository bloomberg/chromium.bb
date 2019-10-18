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
#include "src/dsp/dsp.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

constexpr int kBitdepth8 = 8;
// An offset to cancel offsets used in compound predictor generation
// that make intermediate computations non negative.
const uint16x8_t kSingleRoundOffset8bpp =
    vdupq_n_u16((1 << kBitdepth8) + (1 << (kBitdepth8 - 1)));

template <int subsampling_x, int subsampling_y>
inline uint16x8_t GetMask4x2(const uint8_t* mask, ptrdiff_t mask_stride) {
  if (subsampling_x == 1) {
    const uint16x4_t mask_val0 = vpaddl_u8(vld1_u8(mask));
    const uint16x4_t mask_val1 =
        vpaddl_u8(vld1_u8(mask + (mask_stride << subsampling_y)));
    uint16x8_t final_val;
    if (subsampling_y == 1) {
      const uint16x4_t next_mask_val0 = vpaddl_u8(vld1_u8(mask + mask_stride));
      const uint16x4_t next_mask_val1 =
          vpaddl_u8(vld1_u8(mask + mask_stride * 3));
      final_val = vaddq_u16(vcombine_u16(mask_val0, mask_val1),
                            vcombine_u16(next_mask_val0, next_mask_val1));
    } else {
      final_val =
          vpaddlq_u8(vreinterpretq_u8_u16(vcombine_u16(mask_val0, mask_val1)));
    }
    return vrshrq_n_u16(final_val, subsampling_y + 1);
  }
  assert(subsampling_y == 0 && subsampling_x == 0);
  const uint8x8_t mask_val0 = LoadLo4(mask, vdup_n_u8(0));
  const uint8x8_t mask_val =
      LoadHi4(mask + (mask_stride << subsampling_y), mask_val0);
  return vmovl_u8(mask_val);
}

template <int subsampling_x, int subsampling_y>
inline uint16x8_t GetMask8(const uint8_t* mask, ptrdiff_t mask_stride) {
  if (subsampling_x == 1) {
    uint16x8_t mask_val = vpaddlq_u8(vld1q_u8(mask));
    if (subsampling_y == 1) {
      const uint16x8_t next_mask_val = vpaddlq_u8(vld1q_u8(mask + mask_stride));
      mask_val = vaddq_u16(mask_val, next_mask_val);
    }
    return vrshrq_n_u16(mask_val, 1 + subsampling_y);
  }
  assert(subsampling_y == 0 && subsampling_x == 0);
  const uint8x8_t mask_val = vld1_u8(mask);
  return vmovl_u8(mask_val);
}

template <bool is_inter_intra>
inline void WriteMaskBlendLine4x2(const uint16_t* const pred_0,
                                  const ptrdiff_t pred_stride_0,
                                  const uint16_t* const pred_1,
                                  const ptrdiff_t pred_stride_1,
                                  const uint16x8_t pred_mask_0,
                                  const uint16x8_t pred_mask_1, uint8_t* dst,
                                  const ptrdiff_t dst_stride) {
  const uint16x4_t pred_val_0_lo = vld1_u16(pred_0);
  const uint16x4_t pred_val_0_hi = vld1_u16(pred_0 + pred_stride_0);
  uint16x4_t pred_val_1_lo = vld1_u16(pred_1);
  uint16x4_t pred_val_1_hi = vld1_u16(pred_1 + pred_stride_1);
  uint8x8_t result;
  if (is_inter_intra) {
    // An offset to cancel offsets used in compound predictor generation
    // that make intermediate computations non negative.
    const uint16x8_t single_round_offset =
        vdupq_n_u16((1 << kBitdepth8) + (1 << (kBitdepth8 - 1)));
    // pred_0 and pred_1 are switched at the beginning with is_inter_intra.
    // Clip3(prediction_0[x] - single_round_offset, 0, (1 << kBitdepth8) - 1)
    const uint16x8_t pred_val_1 = vmovl_u8(vqmovn_u16(vqsubq_u16(
        vcombine_u16(pred_val_1_lo, pred_val_1_hi), single_round_offset)));

    const uint16x8_t pred_val_0 = vcombine_u16(pred_val_0_lo, pred_val_0_hi);
    const uint16x8_t weighted_pred_0 = vmulq_u16(pred_val_0, pred_mask_0);
    const uint16x8_t weighted_combo =
        vmlaq_u16(weighted_pred_0, pred_mask_1, pred_val_1);
    result = vrshrn_n_u16(weighted_combo, 6);
  } else {
    // int res = (mask_value * prediction_0[x] +
    //      (64 - mask_value) * prediction_1[x]) >> 6;
    const uint32x4_t weighted_pred_0_lo =
        vmull_u16(vget_low_u16(pred_mask_0), pred_val_0_lo);
    const uint32x4_t weighted_pred_0_hi =
        vmull_u16(vget_high_u16(pred_mask_0), pred_val_0_hi);
    const uint32x4_t weighted_combo_lo =
        vmlal_u16(weighted_pred_0_lo, vget_low_u16(pred_mask_1), pred_val_1_lo);
    const uint32x4_t weighted_combo_hi = vmlal_u16(
        weighted_pred_0_hi, vget_high_u16(pred_mask_1), pred_val_1_hi);
    // res -= compound_round_offset;
    // dst[x] = static_cast<Pixel>(
    //     Clip3(RightShiftWithRounding(res, inter_post_round_bits), 0,
    //         (1 << kBitdepth8) - 1));
    const int16x8_t compound_round_offset =
        vdupq_n_s16((1 << (kBitdepth8 + 4)) + (1 << (kBitdepth8 + 3)));
    result = vqrshrun_n_s16(vsubq_s16(vreinterpretq_s16_u16(vcombine_u16(
                                          vshrn_n_u32(weighted_combo_lo, 6),
                                          vshrn_n_u32(weighted_combo_hi, 6))),
                                      compound_round_offset),
                            4);
  }
  StoreLo4(dst, result);
  StoreHi4(dst + dst_stride, result);
}

template <bool is_inter_intra, int subsampling_x, int subsampling_y>
inline void MaskBlending4x4_NEON(const uint16_t* pred_0,
                                 const ptrdiff_t prediction_stride_0,
                                 const uint16_t* pred_1,
                                 const ptrdiff_t prediction_stride_1,
                                 const uint8_t* mask,
                                 const ptrdiff_t mask_stride, uint8_t* dst,
                                 const ptrdiff_t dst_stride) {
  const uint16x8_t mask_inverter = vdupq_n_u16(64);
  uint16x8_t pred_mask_0 =
      GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
  uint16x8_t pred_mask_1 = vsubq_u16(mask_inverter, pred_mask_0);
  WriteMaskBlendLine4x2<is_inter_intra>(pred_0, prediction_stride_0, pred_1,
                                        prediction_stride_1, pred_mask_0,
                                        pred_mask_1, dst, dst_stride);
  pred_0 += prediction_stride_0 << 1;
  pred_1 += prediction_stride_1 << 1;
  mask += mask_stride << (1 + subsampling_y);
  dst += dst_stride << 1;

  pred_mask_0 = GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
  pred_mask_1 = vsubq_u16(mask_inverter, pred_mask_0);
  WriteMaskBlendLine4x2<is_inter_intra>(pred_0, prediction_stride_0, pred_1,
                                        prediction_stride_1, pred_mask_0,
                                        pred_mask_1, dst, dst_stride);
}

template <bool is_inter_intra, int subsampling_x, int subsampling_y>
inline void MaskBlending4xH_NEON(const uint16_t* pred_0,
                                 const ptrdiff_t pred_stride_0,
                                 const int height, const uint16_t* pred_1,
                                 const ptrdiff_t pred_stride_1,
                                 const uint8_t* const mask_ptr,
                                 const ptrdiff_t mask_stride, uint8_t* dst,
                                 const ptrdiff_t dst_stride) {
  const uint8_t* mask = mask_ptr;
  if (height == 4) {
    MaskBlending4x4_NEON<is_inter_intra, subsampling_x, subsampling_y>(
        pred_0, pred_stride_0, pred_1, pred_stride_1, mask, mask_stride, dst,
        dst_stride);
    return;
  }
  const uint16x8_t mask_inverter = vdupq_n_u16(64);
  int y = 0;
  do {
    uint16x8_t pred_mask_0 =
        GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
    uint16x8_t pred_mask_1 = vsubq_u16(mask_inverter, pred_mask_0);

    WriteMaskBlendLine4x2<is_inter_intra>(pred_0, pred_stride_0, pred_1,
                                          pred_stride_1, pred_mask_0,
                                          pred_mask_1, dst, dst_stride);
    pred_0 += pred_stride_0 << 1;
    pred_1 += pred_stride_1 << 1;
    mask += mask_stride << (1 + subsampling_y);
    dst += dst_stride << 1;

    pred_mask_0 = GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
    pred_mask_1 = vsubq_u16(mask_inverter, pred_mask_0);
    WriteMaskBlendLine4x2<is_inter_intra>(pred_0, pred_stride_0, pred_1,
                                          pred_stride_1, pred_mask_0,
                                          pred_mask_1, dst, dst_stride);
    pred_0 += pred_stride_0 << 1;
    pred_1 += pred_stride_1 << 1;
    mask += mask_stride << (1 + subsampling_y);
    dst += dst_stride << 1;

    pred_mask_0 = GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
    pred_mask_1 = vsubq_u16(mask_inverter, pred_mask_0);
    WriteMaskBlendLine4x2<is_inter_intra>(pred_0, pred_stride_0, pred_1,
                                          pred_stride_1, pred_mask_0,
                                          pred_mask_1, dst, dst_stride);
    pred_0 += pred_stride_0 << 1;
    pred_1 += pred_stride_1 << 1;
    mask += mask_stride << (1 + subsampling_y);
    dst += dst_stride << 1;

    pred_mask_0 = GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
    pred_mask_1 = vsubq_u16(mask_inverter, pred_mask_0);
    WriteMaskBlendLine4x2<is_inter_intra>(pred_0, pred_stride_0, pred_1,
                                          pred_stride_1, pred_mask_0,
                                          pred_mask_1, dst, dst_stride);
    pred_0 += pred_stride_0 << 1;
    pred_1 += pred_stride_1 << 1;
    mask += mask_stride << (1 + subsampling_y);
    dst += dst_stride << 1;
    y += 8;
  } while (y < height);
}

template <bool is_inter_intra, int subsampling_x, int subsampling_y>
inline void MaskBlend_NEON(
    const uint16_t* prediction_0, const ptrdiff_t prediction_stride_0,
    const uint16_t* prediction_1, const ptrdiff_t prediction_stride_1,
    const uint8_t* const mask_ptr, const ptrdiff_t mask_stride, const int width,
    const int height, void* dest, const ptrdiff_t dst_stride) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(dest);
  const uint16_t* pred_0 = is_inter_intra ? prediction_1 : prediction_0;
  const uint16_t* pred_1 = is_inter_intra ? prediction_0 : prediction_1;
  const ptrdiff_t pred_stride_0 =
      is_inter_intra ? prediction_stride_1 : prediction_stride_0;
  const ptrdiff_t pred_stride_1 =
      is_inter_intra ? prediction_stride_0 : prediction_stride_1;
  if (width == 4) {
    MaskBlending4xH_NEON<is_inter_intra, subsampling_x, subsampling_y>(
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
      const uint16x8_t pred_mask_0 = GetMask8<subsampling_x, subsampling_y>(
          mask + (x << subsampling_x), mask_stride);
      // 64 - mask
      const uint16x8_t pred_mask_1 = vsubq_u16(mask_inverter, pred_mask_0);
      const uint16x8_t pred_val_0 = vld1q_u16(pred_0 + x);
      uint16x8_t pred_val_1 = vld1q_u16(pred_1 + x);
      if (is_inter_intra) {
        // An offset to cancel offsets used in compound predictor generation
        // that make intermediate computations non negative.
        const uint16x8_t single_round_offset =
            vdupq_n_u16((1 << kBitdepth8) + (1 << (kBitdepth8 - 1)));
        pred_val_1 =
            vmovl_u8(vqmovn_u16(vqsubq_u16(pred_val_1, single_round_offset)));
      }
      uint8x8_t result;
      if (is_inter_intra) {
        const uint16x8_t weighted_pred_0 = vmulq_u16(pred_mask_0, pred_val_0);
        // weighted_pred0 + weighted_pred1
        const uint16x8_t weighted_combo =
            vmlaq_u16(weighted_pred_0, pred_mask_1, pred_val_1);
        result = vrshrn_n_u16(weighted_combo, 6);
      } else {
        // int res = (mask_value * prediction_0[x] +
        //      (64 - mask_value) * prediction_1[x]) >> 6;
        const uint32x4_t weighted_pred_0_lo =
            vmull_u16(vget_low_u16(pred_mask_0), vget_low_u16(pred_val_0));
        const uint32x4_t weighted_pred_0_hi =
            vmull_u16(vget_high_u16(pred_mask_0), vget_high_u16(pred_val_0));
        const uint32x4_t weighted_combo_lo =
            vmlal_u16(weighted_pred_0_lo, vget_low_u16(pred_mask_1),
                      vget_low_u16(pred_val_1));
        const uint32x4_t weighted_combo_hi =
            vmlal_u16(weighted_pred_0_hi, vget_high_u16(pred_mask_1),
                      vget_high_u16(pred_val_1));

        const int16x8_t compound_round_offset =
            vdupq_n_s16((1 << (kBitdepth8 + 4)) + (1 << (kBitdepth8 + 3)));
        // res -= compound_round_offset;
        // dst[x] = static_cast<Pixel>(
        //     Clip3(RightShiftWithRounding(res, inter_post_round_bits), 0,
        //           (1 << kBitdepth8) - 1));
        result =
            vqrshrun_n_s16(vsubq_s16(vreinterpretq_s16_u16(vcombine_u16(
                                         vshrn_n_u32(weighted_combo_lo, 6),
                                         vshrn_n_u32(weighted_combo_hi, 6))),
                                     compound_round_offset),
                           4);
      }
      vst1_u8(dst + x, result);

      x += 8;
    } while (x < width);
    dst += dst_stride;
    pred_0 += pred_stride_0;
    pred_1 += pred_stride_1;
    mask += mask_stride << subsampling_y;
  } while (++y < height);
}

// At the higher level, the uint8_t* buffer is called prediction_1. However, the
// following implementation swaps the labels to make the "first" prediction
// correspond with the "unaltered" mask.
inline void InterIntraWriteMaskBlendLine8bpp4x2(
    const uint8_t* const pred_0, const ptrdiff_t pred_stride_0,
    const uint16_t* const pred_1, const ptrdiff_t pred_stride_1,
    const uint16x8_t pred_mask_0, const uint16x8_t pred_mask_1, uint8_t* dst,
    const ptrdiff_t dst_stride) {
  const uint8x8_t pred_val_0_lo = LoadLo4(pred_0, vdup_n_u8(0));
  const uint16x8_t pred_val_0 =
      vmovl_u8(LoadHi4(pred_0 + pred_stride_0, pred_val_0_lo));
  const uint16x4_t pred_val_1_lo = vld1_u16(pred_1);
  const uint16x4_t pred_val_1_hi = vld1_u16(pred_1 + pred_stride_1);
  // pred_0 and pred_1 are switched at the beginning with is_inter_intra.
  // Clip3(prediction_0[x] - single_round_offset, 0, (1 << kBitdepth8) - 1)
  const uint16x8_t pred_val_1 = vmovl_u8(vqmovn_u16(vqsubq_u16(
      vcombine_u16(pred_val_1_lo, pred_val_1_hi), kSingleRoundOffset8bpp)));

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
    const uint16_t* pred_1, const ptrdiff_t prediction_stride_1,
    const uint8_t* mask, const ptrdiff_t mask_stride, uint8_t* dst,
    const ptrdiff_t dst_stride) {
  const uint16x8_t mask_inverter = vdupq_n_u16(64);
  uint16x8_t pred_mask_0 =
      GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
  uint16x8_t pred_mask_1 = vsubq_u16(mask_inverter, pred_mask_0);
  InterIntraWriteMaskBlendLine8bpp4x2(pred_0, prediction_stride_0, pred_1,
                                      prediction_stride_1, pred_mask_0,
                                      pred_mask_1, dst, dst_stride);
  pred_0 += prediction_stride_0 << 1;
  pred_1 += prediction_stride_1 << 1;
  mask += mask_stride << (1 + subsampling_y);
  dst += dst_stride << 1;

  pred_mask_0 = GetMask4x2<subsampling_x, subsampling_y>(mask, mask_stride);
  pred_mask_1 = vsubq_u16(mask_inverter, pred_mask_0);
  InterIntraWriteMaskBlendLine8bpp4x2(pred_0, prediction_stride_0, pred_1,
                                      prediction_stride_1, pred_mask_0,
                                      pred_mask_1, dst, dst_stride);
}

template <int subsampling_x, int subsampling_y>
inline void InterIntraMaskBlending8bpp4xH_NEON(
    const uint8_t* pred_0, const ptrdiff_t pred_stride_0, const int height,
    const uint16_t* pred_1, const ptrdiff_t pred_stride_1,
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
    const uint16_t* prediction_0, const ptrdiff_t prediction_stride_0,
    const uint8_t* prediction_1, const ptrdiff_t prediction_stride_1,
    const uint8_t* const mask_ptr, const ptrdiff_t mask_stride, const int width,
    const int height, void* dest, const ptrdiff_t dst_stride) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(dest);
  const uint8_t* pred_0 = prediction_1;
  const uint16_t* pred_1 = prediction_0;
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
      const uint16x8_t pred_mask_0 = GetMask8<subsampling_x, subsampling_y>(
          mask + (x << subsampling_x), mask_stride);
      // 64 - mask
      const uint16x8_t pred_mask_1 = vsubq_u16(mask_inverter, pred_mask_0);
      const uint16x8_t pred_val_0 = vmovl_u8(vld1_u8(pred_0 + x));
      uint16x8_t pred_val_1 = vld1q_u16(pred_1 + x);
      pred_val_1 =
          vmovl_u8(vqmovn_u16(vqsubq_u16(pred_val_1, kSingleRoundOffset8bpp)));
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
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
  dsp->mask_blend[0][0] = MaskBlend_NEON<false, 0, 0>;
  dsp->mask_blend[1][0] = MaskBlend_NEON<false, 1, 0>;
  dsp->mask_blend[2][0] = MaskBlend_NEON<false, 1, 1>;
  dsp->mask_blend[0][1] = MaskBlend_NEON<true, 0, 0>;
  dsp->mask_blend[1][1] = MaskBlend_NEON<true, 1, 0>;
  dsp->mask_blend[2][1] = MaskBlend_NEON<true, 1, 1>;
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
