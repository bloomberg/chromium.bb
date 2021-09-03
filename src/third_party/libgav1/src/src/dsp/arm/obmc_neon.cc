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

#include "src/dsp/obmc.h"
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

namespace libgav1 {
namespace dsp {
namespace {

#include "src/dsp/obmc.inc"

inline void WriteObmcLine4(uint8_t* const pred, const uint8_t* const obmc_pred,
                           const uint8x8_t pred_mask,
                           const uint8x8_t obmc_pred_mask) {
  const uint8x8_t pred_val = Load4(pred);
  const uint8x8_t obmc_pred_val = Load4(obmc_pred);
  const uint16x8_t weighted_pred = vmull_u8(pred_mask, pred_val);
  const uint8x8_t result =
      vrshrn_n_u16(vmlal_u8(weighted_pred, obmc_pred_mask, obmc_pred_val), 6);
  StoreLo4(pred, result);
}

template <bool from_left>
inline void OverlapBlend2xH_NEON(uint8_t* const prediction,
                                 const ptrdiff_t prediction_stride,
                                 const int height,
                                 const uint8_t* const obmc_prediction,
                                 const ptrdiff_t obmc_prediction_stride) {
  uint8_t* pred = prediction;
  const uint8x8_t mask_inverter = vdup_n_u8(64);
  const uint8_t* obmc_pred = obmc_prediction;
  uint8x8_t pred_mask;
  uint8x8_t obmc_pred_mask;
  int compute_height;
  const int mask_offset = height - 2;
  if (from_left) {
    pred_mask = Load2(kObmcMask);
    obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
    compute_height = height;
  } else {
    // Weights for the last line are all 64, which is a no-op.
    compute_height = height - 1;
  }
  uint8x8_t pred_val = vdup_n_u8(0);
  uint8x8_t obmc_pred_val = vdup_n_u8(0);
  int y = 0;
  do {
    if (!from_left) {
      pred_mask = vdup_n_u8(kObmcMask[mask_offset + y]);
      obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
    }
    pred_val = Load2<0>(pred, pred_val);
    const uint16x8_t weighted_pred = vmull_u8(pred_mask, pred_val);
    obmc_pred_val = Load2<0>(obmc_pred, obmc_pred_val);
    const uint8x8_t result =
        vrshrn_n_u16(vmlal_u8(weighted_pred, obmc_pred_mask, obmc_pred_val), 6);
    Store2<0>(pred, result);

    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;
  } while (++y != compute_height);
}

inline void OverlapBlendFromLeft4xH_NEON(
    uint8_t* const prediction, const ptrdiff_t prediction_stride,
    const int height, const uint8_t* const obmc_prediction,
    const ptrdiff_t obmc_prediction_stride) {
  uint8_t* pred = prediction;
  const uint8_t* obmc_pred = obmc_prediction;

  const uint8x8_t mask_inverter = vdup_n_u8(64);
  const uint8x8_t pred_mask = Load4(kObmcMask + 2);
  // 64 - mask
  const uint8x8_t obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
  int y = 0;
  do {
    WriteObmcLine4(pred, obmc_pred, pred_mask, obmc_pred_mask);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;

    WriteObmcLine4(pred, obmc_pred, pred_mask, obmc_pred_mask);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;

    y += 2;
  } while (y != height);
}

inline void OverlapBlendFromLeft8xH_NEON(
    uint8_t* const prediction, const ptrdiff_t prediction_stride,
    const int height, const uint8_t* const obmc_prediction,
    const ptrdiff_t obmc_prediction_stride) {
  uint8_t* pred = prediction;
  const uint8_t* obmc_pred = obmc_prediction;
  const uint8x8_t mask_inverter = vdup_n_u8(64);
  const uint8x8_t pred_mask = vld1_u8(kObmcMask + 6);
  // 64 - mask
  const uint8x8_t obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
  int y = 0;
  do {
    const uint8x8_t pred_val = vld1_u8(pred);
    const uint16x8_t weighted_pred = vmull_u8(pred_mask, pred_val);
    const uint8x8_t obmc_pred_val = vld1_u8(obmc_pred);
    const uint8x8_t result =
        vrshrn_n_u16(vmlal_u8(weighted_pred, obmc_pred_mask, obmc_pred_val), 6);

    vst1_u8(pred, result);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;
  } while (++y != height);
}

void OverlapBlendFromLeft_NEON(void* const prediction,
                               const ptrdiff_t prediction_stride,
                               const int width, const int height,
                               const void* const obmc_prediction,
                               const ptrdiff_t obmc_prediction_stride) {
  auto* pred = static_cast<uint8_t*>(prediction);
  const auto* obmc_pred = static_cast<const uint8_t*>(obmc_prediction);

  if (width == 2) {
    OverlapBlend2xH_NEON<true>(pred, prediction_stride, height, obmc_pred,
                               obmc_prediction_stride);
    return;
  }
  if (width == 4) {
    OverlapBlendFromLeft4xH_NEON(pred, prediction_stride, height, obmc_pred,
                                 obmc_prediction_stride);
    return;
  }
  if (width == 8) {
    OverlapBlendFromLeft8xH_NEON(pred, prediction_stride, height, obmc_pred,
                                 obmc_prediction_stride);
    return;
  }
  const uint8x16_t mask_inverter = vdupq_n_u8(64);
  const uint8_t* mask = kObmcMask + width - 2;
  int x = 0;
  do {
    pred = static_cast<uint8_t*>(prediction) + x;
    obmc_pred = static_cast<const uint8_t*>(obmc_prediction) + x;
    const uint8x16_t pred_mask = vld1q_u8(mask + x);
    // 64 - mask
    const uint8x16_t obmc_pred_mask = vsubq_u8(mask_inverter, pred_mask);
    int y = 0;
    do {
      const uint8x16_t pred_val = vld1q_u8(pred);
      const uint8x16_t obmc_pred_val = vld1q_u8(obmc_pred);
      const uint16x8_t weighted_pred_lo =
          vmull_u8(vget_low_u8(pred_mask), vget_low_u8(pred_val));
      const uint8x8_t result_lo =
          vrshrn_n_u16(vmlal_u8(weighted_pred_lo, vget_low_u8(obmc_pred_mask),
                                vget_low_u8(obmc_pred_val)),
                       6);
      const uint16x8_t weighted_pred_hi =
          vmull_u8(vget_high_u8(pred_mask), vget_high_u8(pred_val));
      const uint8x8_t result_hi =
          vrshrn_n_u16(vmlal_u8(weighted_pred_hi, vget_high_u8(obmc_pred_mask),
                                vget_high_u8(obmc_pred_val)),
                       6);
      vst1q_u8(pred, vcombine_u8(result_lo, result_hi));

      pred += prediction_stride;
      obmc_pred += obmc_prediction_stride;
    } while (++y < height);
    x += 16;
  } while (x < width);
}

inline void OverlapBlendFromTop4x4_NEON(uint8_t* const prediction,
                                        const ptrdiff_t prediction_stride,
                                        const uint8_t* const obmc_prediction,
                                        const ptrdiff_t obmc_prediction_stride,
                                        const int height) {
  uint8_t* pred = prediction;
  const uint8_t* obmc_pred = obmc_prediction;
  uint8x8_t pred_mask = vdup_n_u8(kObmcMask[height - 2]);
  const uint8x8_t mask_inverter = vdup_n_u8(64);
  uint8x8_t obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
  WriteObmcLine4(pred, obmc_pred, pred_mask, obmc_pred_mask);
  pred += prediction_stride;
  obmc_pred += obmc_prediction_stride;

  if (height == 2) {
    return;
  }

  pred_mask = vdup_n_u8(kObmcMask[3]);
  obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
  WriteObmcLine4(pred, obmc_pred, pred_mask, obmc_pred_mask);
  pred += prediction_stride;
  obmc_pred += obmc_prediction_stride;

  pred_mask = vdup_n_u8(kObmcMask[4]);
  obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
  WriteObmcLine4(pred, obmc_pred, pred_mask, obmc_pred_mask);
}

inline void OverlapBlendFromTop4xH_NEON(
    uint8_t* const prediction, const ptrdiff_t prediction_stride,
    const int height, const uint8_t* const obmc_prediction,
    const ptrdiff_t obmc_prediction_stride) {
  if (height < 8) {
    OverlapBlendFromTop4x4_NEON(prediction, prediction_stride, obmc_prediction,
                                obmc_prediction_stride, height);
    return;
  }
  uint8_t* pred = prediction;
  const uint8_t* obmc_pred = obmc_prediction;
  const uint8_t* mask = kObmcMask + height - 2;
  const uint8x8_t mask_inverter = vdup_n_u8(64);
  int y = 0;
  // Compute 6 lines for height 8, or 12 lines for height 16. The remaining
  // lines are unchanged as the corresponding mask value is 64.
  do {
    uint8x8_t pred_mask = vdup_n_u8(mask[y]);
    uint8x8_t obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
    WriteObmcLine4(pred, obmc_pred, pred_mask, obmc_pred_mask);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;

    pred_mask = vdup_n_u8(mask[y + 1]);
    obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
    WriteObmcLine4(pred, obmc_pred, pred_mask, obmc_pred_mask);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;

    pred_mask = vdup_n_u8(mask[y + 2]);
    obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
    WriteObmcLine4(pred, obmc_pred, pred_mask, obmc_pred_mask);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;

    pred_mask = vdup_n_u8(mask[y + 3]);
    obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
    WriteObmcLine4(pred, obmc_pred, pred_mask, obmc_pred_mask);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;

    pred_mask = vdup_n_u8(mask[y + 4]);
    obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
    WriteObmcLine4(pred, obmc_pred, pred_mask, obmc_pred_mask);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;

    pred_mask = vdup_n_u8(mask[y + 5]);
    obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
    WriteObmcLine4(pred, obmc_pred, pred_mask, obmc_pred_mask);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;

    // Increment for the right mask index.
    y += 6;
  } while (y < height - 4);
}

inline void OverlapBlendFromTop8xH_NEON(
    uint8_t* const prediction, const ptrdiff_t prediction_stride,
    const int height, const uint8_t* const obmc_prediction,
    const ptrdiff_t obmc_prediction_stride) {
  uint8_t* pred = prediction;
  const uint8_t* obmc_pred = obmc_prediction;
  const uint8x8_t mask_inverter = vdup_n_u8(64);
  const uint8_t* mask = kObmcMask + height - 2;
  const int compute_height = height - (height >> 2);
  int y = 0;
  do {
    const uint8x8_t pred_mask = vdup_n_u8(mask[y]);
    // 64 - mask
    const uint8x8_t obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
    const uint8x8_t pred_val = vld1_u8(pred);
    const uint16x8_t weighted_pred = vmull_u8(pred_mask, pred_val);
    const uint8x8_t obmc_pred_val = vld1_u8(obmc_pred);
    const uint8x8_t result =
        vrshrn_n_u16(vmlal_u8(weighted_pred, obmc_pred_mask, obmc_pred_val), 6);

    vst1_u8(pred, result);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;
  } while (++y != compute_height);
}

void OverlapBlendFromTop_NEON(void* const prediction,
                              const ptrdiff_t prediction_stride,
                              const int width, const int height,
                              const void* const obmc_prediction,
                              const ptrdiff_t obmc_prediction_stride) {
  auto* pred = static_cast<uint8_t*>(prediction);
  const auto* obmc_pred = static_cast<const uint8_t*>(obmc_prediction);

  if (width == 2) {
    OverlapBlend2xH_NEON<false>(pred, prediction_stride, height, obmc_pred,
                                obmc_prediction_stride);
    return;
  }
  if (width == 4) {
    OverlapBlendFromTop4xH_NEON(pred, prediction_stride, height, obmc_pred,
                                obmc_prediction_stride);
    return;
  }

  if (width == 8) {
    OverlapBlendFromTop8xH_NEON(pred, prediction_stride, height, obmc_pred,
                                obmc_prediction_stride);
    return;
  }

  const uint8_t* mask = kObmcMask + height - 2;
  const uint8x8_t mask_inverter = vdup_n_u8(64);
  // Stop when mask value becomes 64. This is inferred for 4xH.
  const int compute_height = height - (height >> 2);
  int y = 0;
  do {
    const uint8x8_t pred_mask = vdup_n_u8(mask[y]);
    // 64 - mask
    const uint8x8_t obmc_pred_mask = vsub_u8(mask_inverter, pred_mask);
    int x = 0;
    do {
      const uint8x16_t pred_val = vld1q_u8(pred + x);
      const uint8x16_t obmc_pred_val = vld1q_u8(obmc_pred + x);
      const uint16x8_t weighted_pred_lo =
          vmull_u8(pred_mask, vget_low_u8(pred_val));
      const uint8x8_t result_lo =
          vrshrn_n_u16(vmlal_u8(weighted_pred_lo, obmc_pred_mask,
                                vget_low_u8(obmc_pred_val)),
                       6);
      const uint16x8_t weighted_pred_hi =
          vmull_u8(pred_mask, vget_high_u8(pred_val));
      const uint8x8_t result_hi =
          vrshrn_n_u16(vmlal_u8(weighted_pred_hi, obmc_pred_mask,
                                vget_high_u8(obmc_pred_val)),
                       6);
      vst1q_u8(pred + x, vcombine_u8(result_lo, result_hi));

      x += 16;
    } while (x < width);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;
  } while (++y < compute_height);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
  dsp->obmc_blend[kObmcDirectionVertical] = OverlapBlendFromTop_NEON;
  dsp->obmc_blend[kObmcDirectionHorizontal] = OverlapBlendFromLeft_NEON;
}

}  // namespace

void ObmcInit_NEON() { Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_NEON

namespace libgav1 {
namespace dsp {

void ObmcInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON
