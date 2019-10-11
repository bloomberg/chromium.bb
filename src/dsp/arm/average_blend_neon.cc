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

#include "src/dsp/average_blend.h"
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
// An offset to cancel offsets used in compound predictor generation that
// make intermediate computations non negative.
const int16x8_t kCompoundRoundOffset =
    vdupq_n_s16((2 << (kBitdepth8 + 4)) + (2 << (kBitdepth8 + 3)));

inline void AverageBlend4Row(const uint16_t* prediction_0,
                             const uint16_t* prediction_1, uint8_t* dest) {
  const int16x4_t pred0 = vreinterpret_s16_u16(vld1_u16(prediction_0));
  const int16x4_t pred1 = vreinterpret_s16_u16(vld1_u16(prediction_1));
  int16x4_t res = vadd_s16(pred0, pred1);
  res = vsub_s16(res, vget_low_s16(kCompoundRoundOffset));
  StoreLo4(dest,
           vqrshrun_n_s16(vcombine_s16(res, res), kInterPostRoundBit + 1));
}

inline void AverageBlend8Row(const uint16_t* prediction_0,
                             const uint16_t* prediction_1, uint8_t* dest) {
  const int16x8_t pred0 = vreinterpretq_s16_u16(vld1q_u16(prediction_0));
  const int16x8_t pred1 = vreinterpretq_s16_u16(vld1q_u16(prediction_1));
  int16x8_t res = vaddq_s16(pred0, pred1);
  res = vsubq_s16(res, kCompoundRoundOffset);
  vst1_u8(dest, vqrshrun_n_s16(res, kInterPostRoundBit + 1));
}

inline void AverageBlendLargeRow(const uint16_t* prediction_0,
                                 const uint16_t* prediction_1, const int width,
                                 uint8_t* dest) {
  int x = 0;
  do {
    const int16x8_t pred_00 =
        vreinterpretq_s16_u16(vld1q_u16(&prediction_0[x]));
    const int16x8_t pred_01 =
        vreinterpretq_s16_u16(vld1q_u16(&prediction_1[x]));
    int16x8_t res0 = vaddq_s16(pred_00, pred_01);
    res0 = vsubq_s16(res0, kCompoundRoundOffset);
    const uint8x8_t res_out0 = vqrshrun_n_s16(res0, kInterPostRoundBit + 1);
    const int16x8_t pred_10 =
        vreinterpretq_s16_u16(vld1q_u16(&prediction_0[x + 8]));
    const int16x8_t pred_11 =
        vreinterpretq_s16_u16(vld1q_u16(&prediction_1[x + 8]));
    int16x8_t res1 = vaddq_s16(pred_10, pred_11);
    res1 = vsubq_s16(res1, kCompoundRoundOffset);
    const uint8x8_t res_out1 = vqrshrun_n_s16(res1, kInterPostRoundBit + 1);
    vst1q_u8(dest + x, vcombine_u8(res_out0, res_out1));
    x += 16;
  } while (x < width);
}

void AverageBlend_NEON(const uint16_t* prediction_0,
                       const ptrdiff_t prediction_stride_0,
                       const uint16_t* prediction_1,
                       const ptrdiff_t prediction_stride_1, const int width,
                       const int height, void* const dest,
                       const ptrdiff_t dest_stride) {
  auto* dst = static_cast<uint8_t*>(dest);
  int y = height;

  if (width == 4) {
    do {
      AverageBlend4Row(prediction_0, prediction_1, dst);
      dst += dest_stride;
      prediction_0 += prediction_stride_0;
      prediction_1 += prediction_stride_1;

      AverageBlend4Row(prediction_0, prediction_1, dst);
      dst += dest_stride;
      prediction_0 += prediction_stride_0;
      prediction_1 += prediction_stride_1;

      y -= 2;
    } while (y != 0);
    return;
  }

  if (width == 8) {
    do {
      AverageBlend8Row(prediction_0, prediction_1, dst);
      dst += dest_stride;
      prediction_0 += prediction_stride_0;
      prediction_1 += prediction_stride_1;

      AverageBlend8Row(prediction_0, prediction_1, dst);
      dst += dest_stride;
      prediction_0 += prediction_stride_0;
      prediction_1 += prediction_stride_1;

      y -= 2;
    } while (y != 0);
    return;
  }

  do {
    AverageBlendLargeRow(prediction_0, prediction_1, width, dst);
    dst += dest_stride;
    prediction_0 += prediction_stride_0;
    prediction_1 += prediction_stride_1;

    AverageBlendLargeRow(prediction_0, prediction_1, width, dst);
    dst += dest_stride;
    prediction_0 += prediction_stride_0;
    prediction_1 += prediction_stride_1;

    y -= 2;
  } while (y != 0);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
  dsp->average_blend = AverageBlend_NEON;
}

}  // namespace

void AverageBlendInit_NEON() { Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_NEON

namespace libgav1 {
namespace dsp {

void AverageBlendInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON
