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

#include "src/dsp/distance_weighted_blend.h"
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
namespace {

constexpr int kInterPostRoundBit = 4;

inline int16x8_t ComputeWeightedAverage8(const int16x8_t pred0,
                                         const int16x8_t pred1,
                                         const int16x4_t weights[2]) {
  // TODO(https://issuetracker.google.com/issues/150325685): Investigate range.
  const int32x4_t wpred0_lo = vmull_s16(weights[0], vget_low_s16(pred0));
  const int32x4_t wpred0_hi = vmull_s16(weights[0], vget_high_s16(pred0));
  const int32x4_t blended_lo =
      vmlal_s16(wpred0_lo, weights[1], vget_low_s16(pred1));
  const int32x4_t blended_hi =
      vmlal_s16(wpred0_hi, weights[1], vget_high_s16(pred1));

  return vcombine_s16(vqrshrn_n_s32(blended_lo, kInterPostRoundBit + 4),
                      vqrshrn_n_s32(blended_hi, kInterPostRoundBit + 4));
}

template <int width, int height>
inline void DistanceWeightedBlendSmall_NEON(const int16_t* prediction_0,
                                            const int16_t* prediction_1,
                                            const int16x4_t weights[2],
                                            void* const dest,
                                            const ptrdiff_t dest_stride) {
  auto* dst = static_cast<uint8_t*>(dest);
  constexpr int step = 16 / width;

  for (int y = 0; y < height; y += step) {
    const int16x8_t src_00 = vld1q_s16(prediction_0);
    const int16x8_t src_10 = vld1q_s16(prediction_1);
    prediction_0 += 8;
    prediction_1 += 8;
    const int16x8_t res0 = ComputeWeightedAverage8(src_00, src_10, weights);

    const int16x8_t src_01 = vld1q_s16(prediction_0);
    const int16x8_t src_11 = vld1q_s16(prediction_1);
    prediction_0 += 8;
    prediction_1 += 8;
    const int16x8_t res1 = ComputeWeightedAverage8(src_01, src_11, weights);

    const uint8x8_t result0 = vqmovun_s16(res0);
    const uint8x8_t result1 = vqmovun_s16(res1);
    if (width == 4) {
      StoreLo4(dst, result0);
      dst += dest_stride;
      StoreHi4(dst, result0);
      dst += dest_stride;
      StoreLo4(dst, result1);
      dst += dest_stride;
      StoreHi4(dst, result1);
      dst += dest_stride;
    } else {
      assert(width == 8);
      vst1_u8(dst, result0);
      dst += dest_stride;
      vst1_u8(dst, result1);
      dst += dest_stride;
    }
  }
}

inline void DistanceWeightedBlendLarge_NEON(const int16_t* prediction_0,
                                            const int16_t* prediction_1,
                                            const int16x4_t weights[2],
                                            const int width, const int height,
                                            void* const dest,
                                            const ptrdiff_t dest_stride) {
  auto* dst = static_cast<uint8_t*>(dest);

  int y = height;
  do {
    int x = 0;
    do {
      const int16x8_t src0_lo = vld1q_s16(prediction_0 + x);
      const int16x8_t src1_lo = vld1q_s16(prediction_1 + x);
      const int16x8_t res_lo =
          ComputeWeightedAverage8(src0_lo, src1_lo, weights);

      const int16x8_t src0_hi = vld1q_s16(prediction_0 + x + 8);
      const int16x8_t src1_hi = vld1q_s16(prediction_1 + x + 8);
      const int16x8_t res_hi =
          ComputeWeightedAverage8(src0_hi, src1_hi, weights);

      const uint8x16_t result =
          vcombine_u8(vqmovun_s16(res_lo), vqmovun_s16(res_hi));
      vst1q_u8(dst + x, result);
      x += 16;
    } while (x < width);
    dst += dest_stride;
    prediction_0 += width;
    prediction_1 += width;
  } while (--y != 0);
}

inline void DistanceWeightedBlend_NEON(const void* prediction_0,
                                       const void* prediction_1,
                                       const uint8_t weight_0,
                                       const uint8_t weight_1, const int width,
                                       const int height, void* const dest,
                                       const ptrdiff_t dest_stride) {
  const auto* pred_0 = static_cast<const int16_t*>(prediction_0);
  const auto* pred_1 = static_cast<const int16_t*>(prediction_1);
  int16x4_t weights[2] = {vdup_n_s16(weight_0), vdup_n_s16(weight_1)};
  // TODO(johannkoenig): Investigate the branching. May be fine to call with a
  // variable height.
  if (width == 4) {
    if (height == 4) {
      DistanceWeightedBlendSmall_NEON<4, 4>(pred_0, pred_1, weights, dest,
                                            dest_stride);
    } else if (height == 8) {
      DistanceWeightedBlendSmall_NEON<4, 8>(pred_0, pred_1, weights, dest,
                                            dest_stride);
    } else {
      assert(height == 16);
      DistanceWeightedBlendSmall_NEON<4, 16>(pred_0, pred_1, weights, dest,
                                             dest_stride);
    }
    return;
  }

  if (width == 8) {
    switch (height) {
      case 4:
        DistanceWeightedBlendSmall_NEON<8, 4>(pred_0, pred_1, weights, dest,
                                              dest_stride);
        return;
      case 8:
        DistanceWeightedBlendSmall_NEON<8, 8>(pred_0, pred_1, weights, dest,
                                              dest_stride);
        return;
      case 16:
        DistanceWeightedBlendSmall_NEON<8, 16>(pred_0, pred_1, weights, dest,
                                               dest_stride);
        return;
      default:
        assert(height == 32);
        DistanceWeightedBlendSmall_NEON<8, 32>(pred_0, pred_1, weights, dest,
                                               dest_stride);

        return;
    }
  }

  DistanceWeightedBlendLarge_NEON(pred_0, pred_1, weights, width, height, dest,
                                  dest_stride);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
  dsp->distance_weighted_blend = DistanceWeightedBlend_NEON;
}

}  // namespace

void DistanceWeightedBlendInit_NEON() { Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_NEON

namespace libgav1 {
namespace dsp {

void DistanceWeightedBlendInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON
