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
#include "src/dsp/dsp.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace {

constexpr int kBitdepth8 = 8;
constexpr int kInterPostRoundBit = 4;

const int16x8_t kCompoundRoundOffset =
    vdupq_n_s16((1 << kBitdepth8) + (1 << (kBitdepth8 - 1)));

inline int16x8_t ComputeWeightedAverage8(const uint16x8_t pred0,
                                         const uint16x8_t pred1,
                                         const uint16x4_t weights[2]) {
  const uint32x4_t wpred0_lo = vmull_u16(weights[0], vget_low_u16(pred0));
  const uint32x4_t wpred0_hi = vmull_u16(weights[0], vget_high_u16(pred0));
  const uint32x4_t blended_lo =
      vmlal_u16(wpred0_lo, weights[1], vget_low_u16(pred1));
  const uint32x4_t blended_hi =
      vmlal_u16(wpred0_hi, weights[1], vget_high_u16(pred1));

  const uint16x4_t result_lo =
      vqrshrn_n_u32(blended_lo, kInterPostRoundBit + 4);
  const uint16x4_t result_hi =
      vqrshrn_n_u32(blended_hi, kInterPostRoundBit + 4);
  return vsubq_s16(vreinterpretq_s16_u16(vcombine_u16(result_lo, result_hi)),
                   kCompoundRoundOffset);
}

template <int height>
inline void DistanceWeightedBlend4xH_NEON(const uint16_t* prediction_0,
                                          const ptrdiff_t prediction_stride_0,
                                          const uint16_t* prediction_1,
                                          const ptrdiff_t prediction_stride_1,
                                          const uint16x4_t weights[2],
                                          void* const dest,
                                          const ptrdiff_t dest_stride) {
  auto* dst = static_cast<uint8_t*>(dest);
  const uint16_t* pred_0 = prediction_0;
  const uint16_t* pred_1 = prediction_1;

  for (int y = 0; y < height; y += 4) {
    const uint16x4_t src_00 = vld1_u16(pred_0);
    const uint16x4_t src_10 = vld1_u16(pred_1);
    pred_0 += prediction_stride_0;
    pred_1 += prediction_stride_1;
    const uint16x4_t src_01 = vld1_u16(pred_0);
    const uint16x4_t src_11 = vld1_u16(pred_1);
    pred_0 += prediction_stride_0;
    pred_1 += prediction_stride_1;
    const int16x8_t res01 = ComputeWeightedAverage8(
        vcombine_u16(src_00, src_01), vcombine_u16(src_10, src_11), weights);

    const uint16x4_t src_02 = vld1_u16(pred_0);
    const uint16x4_t src_12 = vld1_u16(pred_1);
    pred_0 += prediction_stride_0;
    pred_1 += prediction_stride_1;
    const uint16x4_t src_03 = vld1_u16(pred_0);
    const uint16x4_t src_13 = vld1_u16(pred_1);
    pred_0 += prediction_stride_0;
    pred_1 += prediction_stride_1;
    const int16x8_t res23 = ComputeWeightedAverage8(
        vcombine_u16(src_02, src_03), vcombine_u16(src_12, src_13), weights);

    const uint8x8_t result_01 = vqmovun_s16(res01);
    const uint8x8_t result_23 = vqmovun_s16(res23);
    StoreLo4(dst, result_01);
    dst += dest_stride;
    StoreHi4(dst, result_01);
    dst += dest_stride;
    StoreLo4(dst, result_23);
    dst += dest_stride;
    StoreHi4(dst, result_23);
    dst += dest_stride;
  }
}

template <int height>
inline void DistanceWeightedBlend8xH_NEON(const uint16_t* prediction_0,
                                          const ptrdiff_t prediction_stride_0,
                                          const uint16_t* prediction_1,
                                          const ptrdiff_t prediction_stride_1,
                                          const uint16x4_t weights[2],
                                          void* const dest,
                                          const ptrdiff_t dest_stride) {
  auto* dst = static_cast<uint8_t*>(dest);
  const uint16_t* pred_0 = prediction_0;
  const uint16_t* pred_1 = prediction_1;

  for (int y = 0; y < height; y += 2) {
    const uint16x8_t src_00 = vld1q_u16(pred_0);
    const uint16x8_t src_10 = vld1q_u16(pred_1);
    pred_0 += prediction_stride_0;
    pred_1 += prediction_stride_1;
    const int16x8_t res0 = ComputeWeightedAverage8(src_00, src_10, weights);

    const uint16x8_t src_01 = vld1q_u16(pred_0);
    const uint16x8_t src_11 = vld1q_u16(pred_1);
    pred_0 += prediction_stride_0;
    pred_1 += prediction_stride_1;
    const int16x8_t res1 = ComputeWeightedAverage8(src_01, src_11, weights);

    const uint8x8_t result0 = vqmovun_s16(res0);
    const uint8x8_t result1 = vqmovun_s16(res1);
    vst1_u8(dst, result0);
    dst += dest_stride;
    vst1_u8(dst, result1);
    dst += dest_stride;
  }
}

inline void DistanceWeightedBlendLarge_NEON(
    const uint16_t* prediction_0, const ptrdiff_t prediction_stride_0,
    const uint16_t* prediction_1, const ptrdiff_t prediction_stride_1,
    const uint16x4_t weights[2], const int width, const int height,
    void* const dest, const ptrdiff_t dest_stride) {
  auto* dst = static_cast<uint8_t*>(dest);
  const uint16_t* pred_0 = prediction_0;
  const uint16_t* pred_1 = prediction_1;

  int y = height;
  do {
    int x = 0;
    do {
      const uint16x8_t src0_lo = vld1q_u16(pred_0 + x);
      const uint16x8_t src1_lo = vld1q_u16(pred_1 + x);
      const int16x8_t res_lo =
          ComputeWeightedAverage8(src0_lo, src1_lo, weights);

      const uint16x8_t src0_hi = vld1q_u16(pred_0 + x + 8);
      const uint16x8_t src1_hi = vld1q_u16(pred_1 + x + 8);
      const int16x8_t res_hi =
          ComputeWeightedAverage8(src0_hi, src1_hi, weights);

      const uint8x16_t result =
          vcombine_u8(vqmovun_s16(res_lo), vqmovun_s16(res_hi));
      vst1q_u8(dst + x, result);
      x += 16;
    } while (x < width);
    dst += dest_stride;
    pred_0 += prediction_stride_0;
    pred_1 += prediction_stride_1;
  } while (--y != 0);
}

inline void DistanceWeightedBlend_NEON(
    const uint16_t* prediction_0, const ptrdiff_t prediction_stride_0,
    const uint16_t* prediction_1, const ptrdiff_t prediction_stride_1,
    const uint8_t weight_0, const uint8_t weight_1, const int width,
    const int height, void* const dest, const ptrdiff_t dest_stride) {
  uint16x4_t weights[2] = {vdup_n_u16(weight_0), vdup_n_u16(weight_1)};
  if (width == 4) {
    if (height == 4) {
      DistanceWeightedBlend4xH_NEON<4>(prediction_0, prediction_stride_0,
                                       prediction_1, prediction_stride_1,
                                       weights, dest, dest_stride);
    } else if (height == 8) {
      DistanceWeightedBlend4xH_NEON<8>(prediction_0, prediction_stride_0,
                                       prediction_1, prediction_stride_1,
                                       weights, dest, dest_stride);
    } else {
      assert(height == 16);
      DistanceWeightedBlend4xH_NEON<16>(prediction_0, prediction_stride_0,
                                        prediction_1, prediction_stride_1,
                                        weights, dest, dest_stride);
    }
    return;
  }

  if (width == 8) {
    switch (height) {
      case 4:
        DistanceWeightedBlend8xH_NEON<4>(prediction_0, prediction_stride_0,
                                         prediction_1, prediction_stride_1,
                                         weights, dest, dest_stride);
        return;
      case 8:
        DistanceWeightedBlend8xH_NEON<8>(prediction_0, prediction_stride_0,
                                         prediction_1, prediction_stride_1,
                                         weights, dest, dest_stride);
        return;
      case 16:
        DistanceWeightedBlend8xH_NEON<16>(prediction_0, prediction_stride_0,
                                          prediction_1, prediction_stride_1,
                                          weights, dest, dest_stride);
        return;
      default:
        assert(height == 32);
        DistanceWeightedBlend8xH_NEON<32>(prediction_0, prediction_stride_0,
                                          prediction_1, prediction_stride_1,
                                          weights, dest, dest_stride);

        return;
    }
  }

  DistanceWeightedBlendLarge_NEON(prediction_0, prediction_stride_0,
                                  prediction_1, prediction_stride_1, weights,
                                  width, height, dest, dest_stride);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
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
