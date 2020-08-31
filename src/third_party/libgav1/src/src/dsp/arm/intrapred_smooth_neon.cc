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

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/dsp/arm/common_neon.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"

namespace libgav1 {
namespace dsp {

namespace low_bitdepth {
namespace {

// Note these constants are duplicated from intrapred.cc to allow the compiler
// to have visibility of the values. This helps reduce loads and in the
// creation of the inverse weights.
constexpr uint8_t kSmoothWeights[] = {
    // block dimension = 4
    255, 149, 85, 64,
    // block dimension = 8
    255, 197, 146, 105, 73, 50, 37, 32,
    // block dimension = 16
    255, 225, 196, 170, 145, 123, 102, 84, 68, 54, 43, 33, 26, 20, 17, 16,
    // block dimension = 32
    255, 240, 225, 210, 196, 182, 169, 157, 145, 133, 122, 111, 101, 92, 83, 74,
    66, 59, 52, 45, 39, 34, 29, 25, 21, 17, 14, 12, 10, 9, 8, 8,
    // block dimension = 64
    255, 248, 240, 233, 225, 218, 210, 203, 196, 189, 182, 176, 169, 163, 156,
    150, 144, 138, 133, 127, 121, 116, 111, 106, 101, 96, 91, 86, 82, 77, 73,
    69, 65, 61, 57, 54, 50, 47, 44, 41, 38, 35, 32, 29, 27, 25, 22, 20, 18, 16,
    15, 13, 12, 10, 9, 8, 7, 6, 6, 5, 5, 4, 4, 4};

// TODO(b/150459137): Keeping the intermediate values in uint16_t would allow
// processing more values at once. At the high end, it could do 4x4 or 8x2 at a
// time.
inline uint16x4_t CalculatePred(const uint16x4_t weighted_top,
                                const uint16x4_t weighted_left,
                                const uint16x4_t weighted_bl,
                                const uint16x4_t weighted_tr) {
  const uint32x4_t pred_0 = vaddl_u16(weighted_top, weighted_left);
  const uint32x4_t pred_1 = vaddl_u16(weighted_bl, weighted_tr);
  const uint32x4_t pred_2 = vaddq_u32(pred_0, pred_1);
  return vrshrn_n_u32(pred_2, kSmoothWeightScale + 1);
}

template <int width, int height>
inline void Smooth4Or8xN_NEON(void* const dest, ptrdiff_t stride,
                              const void* const top_row,
                              const void* const left_column) {
  const uint8_t* const top = static_cast<const uint8_t*>(top_row);
  const uint8_t* const left = static_cast<const uint8_t*>(left_column);
  const uint8_t top_right = top[width - 1];
  const uint8_t bottom_left = left[height - 1];
  const uint8_t* const weights_y = kSmoothWeights + height - 4;
  uint8_t* dst = static_cast<uint8_t*>(dest);

  uint8x8_t top_v;
  if (width == 4) {
    top_v = Load4(top);
  } else {  // width == 8
    top_v = vld1_u8(top);
  }
  const uint8x8_t top_right_v = vdup_n_u8(top_right);
  const uint8x8_t bottom_left_v = vdup_n_u8(bottom_left);
  // Over-reads for 4xN but still within the array.
  const uint8x8_t weights_x_v = vld1_u8(kSmoothWeights + width - 4);
  // 256 - weights = vneg_s8(weights)
  const uint8x8_t scaled_weights_x =
      vreinterpret_u8_s8(vneg_s8(vreinterpret_s8_u8(weights_x_v)));

  for (int y = 0; y < height; ++y) {
    const uint8x8_t left_v = vdup_n_u8(left[y]);
    const uint8x8_t weights_y_v = vdup_n_u8(weights_y[y]);
    const uint8x8_t scaled_weights_y = vdup_n_u8(256 - weights_y[y]);
    const uint16x8_t weighted_bl = vmull_u8(scaled_weights_y, bottom_left_v);

    const uint16x8_t weighted_top = vmull_u8(weights_y_v, top_v);
    const uint16x8_t weighted_left = vmull_u8(weights_x_v, left_v);
    const uint16x8_t weighted_tr = vmull_u8(scaled_weights_x, top_right_v);
    const uint16x4_t dest_0 =
        CalculatePred(vget_low_u16(weighted_top), vget_low_u16(weighted_left),
                      vget_low_u16(weighted_tr), vget_low_u16(weighted_bl));

    if (width == 4) {
      StoreLo4(dst, vmovn_u16(vcombine_u16(dest_0, dest_0)));
    } else {  // width == 8
      const uint16x4_t dest_1 = CalculatePred(
          vget_high_u16(weighted_top), vget_high_u16(weighted_left),
          vget_high_u16(weighted_tr), vget_high_u16(weighted_bl));
      vst1_u8(dst, vmovn_u16(vcombine_u16(dest_0, dest_1)));
    }
    dst += stride;
  }
}

inline uint8x16_t CalculateWeightsAndPred(
    const uint8x16_t top, const uint8x8_t left, const uint8x8_t top_right,
    const uint8x8_t weights_y, const uint8x16_t weights_x,
    const uint8x16_t scaled_weights_x, const uint16x8_t weighted_bl) {
  const uint16x8_t weighted_top_low = vmull_u8(weights_y, vget_low_u8(top));
  const uint16x8_t weighted_left_low = vmull_u8(vget_low_u8(weights_x), left);
  const uint16x8_t weighted_tr_low =
      vmull_u8(vget_low_u8(scaled_weights_x), top_right);
  const uint16x4_t dest_0 = CalculatePred(
      vget_low_u16(weighted_top_low), vget_low_u16(weighted_left_low),
      vget_low_u16(weighted_tr_low), vget_low_u16(weighted_bl));
  const uint16x4_t dest_1 = CalculatePred(
      vget_high_u16(weighted_top_low), vget_high_u16(weighted_left_low),
      vget_high_u16(weighted_tr_low), vget_high_u16(weighted_bl));
  const uint8x8_t dest_0_u8 = vmovn_u16(vcombine_u16(dest_0, dest_1));

  const uint16x8_t weighted_top_high = vmull_u8(weights_y, vget_high_u8(top));
  const uint16x8_t weighted_left_high = vmull_u8(vget_high_u8(weights_x), left);
  const uint16x8_t weighted_tr_high =
      vmull_u8(vget_high_u8(scaled_weights_x), top_right);
  const uint16x4_t dest_2 = CalculatePred(
      vget_low_u16(weighted_top_high), vget_low_u16(weighted_left_high),
      vget_low_u16(weighted_tr_high), vget_low_u16(weighted_bl));
  const uint16x4_t dest_3 = CalculatePred(
      vget_high_u16(weighted_top_high), vget_high_u16(weighted_left_high),
      vget_high_u16(weighted_tr_high), vget_high_u16(weighted_bl));
  const uint8x8_t dest_1_u8 = vmovn_u16(vcombine_u16(dest_2, dest_3));

  return vcombine_u8(dest_0_u8, dest_1_u8);
}

template <int width, int height>
inline void Smooth16PlusxN_NEON(void* const dest, ptrdiff_t stride,
                                const void* const top_row,
                                const void* const left_column) {
  const uint8_t* const top = static_cast<const uint8_t*>(top_row);
  const uint8_t* const left = static_cast<const uint8_t*>(left_column);
  const uint8_t top_right = top[width - 1];
  const uint8_t bottom_left = left[height - 1];
  const uint8_t* const weights_y = kSmoothWeights + height - 4;
  uint8_t* dst = static_cast<uint8_t*>(dest);

  uint8x16_t top_v[4];
  top_v[0] = vld1q_u8(top);
  if (width > 16) {
    top_v[1] = vld1q_u8(top + 16);
    if (width == 64) {
      top_v[2] = vld1q_u8(top + 32);
      top_v[3] = vld1q_u8(top + 48);
    }
  }

  const uint8x8_t top_right_v = vdup_n_u8(top_right);
  const uint8x8_t bottom_left_v = vdup_n_u8(bottom_left);

  // TODO(johannkoenig): Consider re-reading top_v and weights_x_v in the loop.
  // This currently has a performance slope similar to Paeth so it does not
  // appear to be register bound for arm64.
  uint8x16_t weights_x_v[4];
  weights_x_v[0] = vld1q_u8(kSmoothWeights + width - 4);
  if (width > 16) {
    weights_x_v[1] = vld1q_u8(kSmoothWeights + width + 16 - 4);
    if (width == 64) {
      weights_x_v[2] = vld1q_u8(kSmoothWeights + width + 32 - 4);
      weights_x_v[3] = vld1q_u8(kSmoothWeights + width + 48 - 4);
    }
  }

  uint8x16_t scaled_weights_x[4];
  scaled_weights_x[0] =
      vreinterpretq_u8_s8(vnegq_s8(vreinterpretq_s8_u8(weights_x_v[0])));
  if (width > 16) {
    scaled_weights_x[1] =
        vreinterpretq_u8_s8(vnegq_s8(vreinterpretq_s8_u8(weights_x_v[1])));
    if (width == 64) {
      scaled_weights_x[2] =
          vreinterpretq_u8_s8(vnegq_s8(vreinterpretq_s8_u8(weights_x_v[2])));
      scaled_weights_x[3] =
          vreinterpretq_u8_s8(vnegq_s8(vreinterpretq_s8_u8(weights_x_v[3])));
    }
  }

  for (int y = 0; y < height; ++y) {
    const uint8x8_t left_v = vdup_n_u8(left[y]);
    const uint8x8_t weights_y_v = vdup_n_u8(weights_y[y]);
    const uint8x8_t scaled_weights_y = vdup_n_u8(256 - weights_y[y]);
    const uint16x8_t weighted_bl = vmull_u8(scaled_weights_y, bottom_left_v);

    vst1q_u8(dst, CalculateWeightsAndPred(top_v[0], left_v, top_right_v,
                                          weights_y_v, weights_x_v[0],
                                          scaled_weights_x[0], weighted_bl));

    if (width > 16) {
      vst1q_u8(dst + 16, CalculateWeightsAndPred(
                             top_v[1], left_v, top_right_v, weights_y_v,
                             weights_x_v[1], scaled_weights_x[1], weighted_bl));
      if (width == 64) {
        vst1q_u8(dst + 32,
                 CalculateWeightsAndPred(top_v[2], left_v, top_right_v,
                                         weights_y_v, weights_x_v[2],
                                         scaled_weights_x[2], weighted_bl));
        vst1q_u8(dst + 48,
                 CalculateWeightsAndPred(top_v[3], left_v, top_right_v,
                                         weights_y_v, weights_x_v[3],
                                         scaled_weights_x[3], weighted_bl));
      }
    }

    dst += stride;
  }
}

template <int width, int height>
inline void SmoothVertical4Or8xN_NEON(void* const dest, ptrdiff_t stride,
                                      const void* const top_row,
                                      const void* const left_column) {
  const uint8_t* const top = static_cast<const uint8_t*>(top_row);
  const uint8_t* const left = static_cast<const uint8_t*>(left_column);
  const uint8_t bottom_left = left[height - 1];
  const uint8_t* const weights_y = kSmoothWeights + height - 4;
  uint8_t* dst = static_cast<uint8_t*>(dest);

  uint8x8_t top_v;
  if (width == 4) {
    top_v = Load4(top);
  } else {  // width == 8
    top_v = vld1_u8(top);
  }

  const uint8x8_t bottom_left_v = vdup_n_u8(bottom_left);

  for (int y = 0; y < height; ++y) {
    const uint8x8_t weights_y_v = vdup_n_u8(weights_y[y]);
    const uint8x8_t scaled_weights_y = vdup_n_u8(256 - weights_y[y]);

    const uint16x8_t weighted_top = vmull_u8(weights_y_v, top_v);
    const uint16x8_t weighted_bl = vmull_u8(scaled_weights_y, bottom_left_v);
    const uint16x8_t pred = vaddq_u16(weighted_top, weighted_bl);
    const uint8x8_t pred_scaled = vrshrn_n_u16(pred, kSmoothWeightScale);

    if (width == 4) {
      StoreLo4(dst, pred_scaled);
    } else {  // width == 8
      vst1_u8(dst, pred_scaled);
    }
    dst += stride;
  }
}

inline uint8x16_t CalculateVerticalWeightsAndPred(
    const uint8x16_t top, const uint8x8_t weights_y,
    const uint16x8_t weighted_bl) {
  const uint16x8_t weighted_top_low = vmull_u8(weights_y, vget_low_u8(top));
  const uint16x8_t weighted_top_high = vmull_u8(weights_y, vget_high_u8(top));
  const uint16x8_t pred_low = vaddq_u16(weighted_top_low, weighted_bl);
  const uint16x8_t pred_high = vaddq_u16(weighted_top_high, weighted_bl);
  const uint8x8_t pred_scaled_low = vrshrn_n_u16(pred_low, kSmoothWeightScale);
  const uint8x8_t pred_scaled_high =
      vrshrn_n_u16(pred_high, kSmoothWeightScale);
  return vcombine_u8(pred_scaled_low, pred_scaled_high);
}

template <int width, int height>
inline void SmoothVertical16PlusxN_NEON(void* const dest, ptrdiff_t stride,
                                        const void* const top_row,
                                        const void* const left_column) {
  const uint8_t* const top = static_cast<const uint8_t*>(top_row);
  const uint8_t* const left = static_cast<const uint8_t*>(left_column);
  const uint8_t bottom_left = left[height - 1];
  const uint8_t* const weights_y = kSmoothWeights + height - 4;
  uint8_t* dst = static_cast<uint8_t*>(dest);

  uint8x16_t top_v[4];
  top_v[0] = vld1q_u8(top);
  if (width > 16) {
    top_v[1] = vld1q_u8(top + 16);
    if (width == 64) {
      top_v[2] = vld1q_u8(top + 32);
      top_v[3] = vld1q_u8(top + 48);
    }
  }

  const uint8x8_t bottom_left_v = vdup_n_u8(bottom_left);

  for (int y = 0; y < height; ++y) {
    const uint8x8_t weights_y_v = vdup_n_u8(weights_y[y]);
    const uint8x8_t scaled_weights_y = vdup_n_u8(256 - weights_y[y]);
    const uint16x8_t weighted_bl = vmull_u8(scaled_weights_y, bottom_left_v);

    const uint8x16_t pred_0 =
        CalculateVerticalWeightsAndPred(top_v[0], weights_y_v, weighted_bl);
    vst1q_u8(dst, pred_0);

    if (width > 16) {
      const uint8x16_t pred_1 =
          CalculateVerticalWeightsAndPred(top_v[1], weights_y_v, weighted_bl);
      vst1q_u8(dst + 16, pred_1);

      if (width == 64) {
        const uint8x16_t pred_2 =
            CalculateVerticalWeightsAndPred(top_v[2], weights_y_v, weighted_bl);
        vst1q_u8(dst + 32, pred_2);

        const uint8x16_t pred_3 =
            CalculateVerticalWeightsAndPred(top_v[3], weights_y_v, weighted_bl);
        vst1q_u8(dst + 48, pred_3);
      }
    }

    dst += stride;
  }
}

template <int width, int height>
inline void SmoothHorizontal4Or8xN_NEON(void* const dest, ptrdiff_t stride,
                                        const void* const top_row,
                                        const void* const left_column) {
  const uint8_t* const top = static_cast<const uint8_t*>(top_row);
  const uint8_t* const left = static_cast<const uint8_t*>(left_column);
  const uint8_t top_right = top[width - 1];
  uint8_t* dst = static_cast<uint8_t*>(dest);

  const uint8x8_t top_right_v = vdup_n_u8(top_right);
  // Over-reads for 4xN but still within the array.
  const uint8x8_t weights_x = vld1_u8(kSmoothWeights + width - 4);
  // 256 - weights = vneg_s8(weights)
  const uint8x8_t scaled_weights_x =
      vreinterpret_u8_s8(vneg_s8(vreinterpret_s8_u8(weights_x)));

  for (int y = 0; y < height; ++y) {
    const uint8x8_t left_v = vdup_n_u8(left[y]);

    const uint16x8_t weighted_left = vmull_u8(weights_x, left_v);
    const uint16x8_t weighted_tr = vmull_u8(scaled_weights_x, top_right_v);
    const uint16x8_t pred = vaddq_u16(weighted_left, weighted_tr);
    const uint8x8_t pred_scaled = vrshrn_n_u16(pred, kSmoothWeightScale);

    if (width == 4) {
      StoreLo4(dst, pred_scaled);
    } else {  // width == 8
      vst1_u8(dst, pred_scaled);
    }
    dst += stride;
  }
}

inline uint8x16_t CalculateHorizontalWeightsAndPred(
    const uint8x8_t left, const uint8x8_t top_right, const uint8x16_t weights_x,
    const uint8x16_t scaled_weights_x) {
  const uint16x8_t weighted_left_low = vmull_u8(vget_low_u8(weights_x), left);
  const uint16x8_t weighted_tr_low =
      vmull_u8(vget_low_u8(scaled_weights_x), top_right);
  const uint16x8_t pred_low = vaddq_u16(weighted_left_low, weighted_tr_low);
  const uint8x8_t pred_scaled_low = vrshrn_n_u16(pred_low, kSmoothWeightScale);

  const uint16x8_t weighted_left_high = vmull_u8(vget_high_u8(weights_x), left);
  const uint16x8_t weighted_tr_high =
      vmull_u8(vget_high_u8(scaled_weights_x), top_right);
  const uint16x8_t pred_high = vaddq_u16(weighted_left_high, weighted_tr_high);
  const uint8x8_t pred_scaled_high =
      vrshrn_n_u16(pred_high, kSmoothWeightScale);

  return vcombine_u8(pred_scaled_low, pred_scaled_high);
}

template <int width, int height>
inline void SmoothHorizontal16PlusxN_NEON(void* const dest, ptrdiff_t stride,
                                          const void* const top_row,
                                          const void* const left_column) {
  const uint8_t* const top = static_cast<const uint8_t*>(top_row);
  const uint8_t* const left = static_cast<const uint8_t*>(left_column);
  const uint8_t top_right = top[width - 1];
  uint8_t* dst = static_cast<uint8_t*>(dest);

  const uint8x8_t top_right_v = vdup_n_u8(top_right);

  uint8x16_t weights_x[4];
  weights_x[0] = vld1q_u8(kSmoothWeights + width - 4);
  if (width > 16) {
    weights_x[1] = vld1q_u8(kSmoothWeights + width + 16 - 4);
    if (width == 64) {
      weights_x[2] = vld1q_u8(kSmoothWeights + width + 32 - 4);
      weights_x[3] = vld1q_u8(kSmoothWeights + width + 48 - 4);
    }
  }

  uint8x16_t scaled_weights_x[4];
  scaled_weights_x[0] =
      vreinterpretq_u8_s8(vnegq_s8(vreinterpretq_s8_u8(weights_x[0])));
  if (width > 16) {
    scaled_weights_x[1] =
        vreinterpretq_u8_s8(vnegq_s8(vreinterpretq_s8_u8(weights_x[1])));
    if (width == 64) {
      scaled_weights_x[2] =
          vreinterpretq_u8_s8(vnegq_s8(vreinterpretq_s8_u8(weights_x[2])));
      scaled_weights_x[3] =
          vreinterpretq_u8_s8(vnegq_s8(vreinterpretq_s8_u8(weights_x[3])));
    }
  }

  for (int y = 0; y < height; ++y) {
    const uint8x8_t left_v = vdup_n_u8(left[y]);

    const uint8x16_t pred_0 = CalculateHorizontalWeightsAndPred(
        left_v, top_right_v, weights_x[0], scaled_weights_x[0]);
    vst1q_u8(dst, pred_0);

    if (width > 16) {
      const uint8x16_t pred_1 = CalculateHorizontalWeightsAndPred(
          left_v, top_right_v, weights_x[1], scaled_weights_x[1]);
      vst1q_u8(dst + 16, pred_1);

      if (width == 64) {
        const uint8x16_t pred_2 = CalculateHorizontalWeightsAndPred(
            left_v, top_right_v, weights_x[2], scaled_weights_x[2]);
        vst1q_u8(dst + 32, pred_2);

        const uint8x16_t pred_3 = CalculateHorizontalWeightsAndPred(
            left_v, top_right_v, weights_x[3], scaled_weights_x[3]);
        vst1q_u8(dst + 48, pred_3);
      }
    }
    dst += stride;
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
  // 4x4
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorSmooth] =
      Smooth4Or8xN_NEON<4, 4>;
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorSmoothVertical] =
      SmoothVertical4Or8xN_NEON<4, 4>;
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal4Or8xN_NEON<4, 4>;

  // 4x8
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorSmooth] =
      Smooth4Or8xN_NEON<4, 8>;
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorSmoothVertical] =
      SmoothVertical4Or8xN_NEON<4, 8>;
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal4Or8xN_NEON<4, 8>;

  // 4x16
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorSmooth] =
      Smooth4Or8xN_NEON<4, 16>;
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorSmoothVertical] =
      SmoothVertical4Or8xN_NEON<4, 16>;
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal4Or8xN_NEON<4, 16>;

  // 8x4
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorSmooth] =
      Smooth4Or8xN_NEON<8, 4>;
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorSmoothVertical] =
      SmoothVertical4Or8xN_NEON<8, 4>;
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal4Or8xN_NEON<8, 4>;

  // 8x8
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorSmooth] =
      Smooth4Or8xN_NEON<8, 8>;
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorSmoothVertical] =
      SmoothVertical4Or8xN_NEON<8, 8>;
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal4Or8xN_NEON<8, 8>;

  // 8x16
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorSmooth] =
      Smooth4Or8xN_NEON<8, 16>;
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorSmoothVertical] =
      SmoothVertical4Or8xN_NEON<8, 16>;
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal4Or8xN_NEON<8, 16>;

  // 8x32
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorSmooth] =
      Smooth4Or8xN_NEON<8, 32>;
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorSmoothVertical] =
      SmoothVertical4Or8xN_NEON<8, 32>;
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal4Or8xN_NEON<8, 32>;

  // 16x4
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorSmooth] =
      Smooth16PlusxN_NEON<16, 4>;
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorSmoothVertical] =
      SmoothVertical16PlusxN_NEON<16, 4>;
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal16PlusxN_NEON<16, 4>;

  // 16x8
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorSmooth] =
      Smooth16PlusxN_NEON<16, 8>;
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorSmoothVertical] =
      SmoothVertical16PlusxN_NEON<16, 8>;
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal16PlusxN_NEON<16, 8>;

  // 16x16
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorSmooth] =
      Smooth16PlusxN_NEON<16, 16>;
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorSmoothVertical] =
      SmoothVertical16PlusxN_NEON<16, 16>;
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal16PlusxN_NEON<16, 16>;

  // 16x32
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorSmooth] =
      Smooth16PlusxN_NEON<16, 32>;
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorSmoothVertical] =
      SmoothVertical16PlusxN_NEON<16, 32>;
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal16PlusxN_NEON<16, 32>;

  // 16x64
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorSmooth] =
      Smooth16PlusxN_NEON<16, 64>;
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorSmoothVertical] =
      SmoothVertical16PlusxN_NEON<16, 64>;
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal16PlusxN_NEON<16, 64>;

  // 32x8
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorSmooth] =
      Smooth16PlusxN_NEON<32, 8>;
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorSmoothVertical] =
      SmoothVertical16PlusxN_NEON<32, 8>;
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal16PlusxN_NEON<32, 8>;

  // 32x16
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorSmooth] =
      Smooth16PlusxN_NEON<32, 16>;
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorSmoothVertical] =
      SmoothVertical16PlusxN_NEON<32, 16>;
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal16PlusxN_NEON<32, 16>;

  // 32x32
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorSmooth] =
      Smooth16PlusxN_NEON<32, 32>;
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorSmoothVertical] =
      SmoothVertical16PlusxN_NEON<32, 32>;
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal16PlusxN_NEON<32, 32>;

  // 32x64
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorSmooth] =
      Smooth16PlusxN_NEON<32, 64>;
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorSmoothVertical] =
      SmoothVertical16PlusxN_NEON<32, 64>;
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal16PlusxN_NEON<32, 64>;

  // 64x16
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorSmooth] =
      Smooth16PlusxN_NEON<64, 16>;
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorSmoothVertical] =
      SmoothVertical16PlusxN_NEON<64, 16>;
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal16PlusxN_NEON<64, 16>;

  // 64x32
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorSmooth] =
      Smooth16PlusxN_NEON<64, 32>;
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorSmoothVertical] =
      SmoothVertical16PlusxN_NEON<64, 32>;
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal16PlusxN_NEON<64, 32>;

  // 64x64
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorSmooth] =
      Smooth16PlusxN_NEON<64, 64>;
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorSmoothVertical] =
      SmoothVertical16PlusxN_NEON<64, 64>;
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorSmoothHorizontal] =
      SmoothHorizontal16PlusxN_NEON<64, 64>;
}

}  // namespace
}  // namespace low_bitdepth

void IntraPredSmoothInit_NEON() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_NEON
namespace libgav1 {
namespace dsp {

void IntraPredSmoothInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON
