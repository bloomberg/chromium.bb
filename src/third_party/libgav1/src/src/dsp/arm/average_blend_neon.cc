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
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace {

constexpr int kInterPostRoundBit =
    kInterRoundBitsVertical - kInterRoundBitsCompoundVertical;

inline uint8x8_t AverageBlend8Row(const int16_t* prediction_0,
                                  const int16_t* prediction_1) {
  const int16x8_t pred0 = vld1q_s16(prediction_0);
  const int16x8_t pred1 = vld1q_s16(prediction_1);
  const int16x8_t res = vaddq_s16(pred0, pred1);
  return vqrshrun_n_s16(res, kInterPostRoundBit + 1);
}

inline void AverageBlendLargeRow(const int16_t* prediction_0,
                                 const int16_t* prediction_1, const int width,
                                 uint8_t* dest) {
  int x = 0;
  do {
    const int16x8_t pred_00 = vld1q_s16(&prediction_0[x]);
    const int16x8_t pred_01 = vld1q_s16(&prediction_1[x]);
    const int16x8_t res0 = vaddq_s16(pred_00, pred_01);
    const uint8x8_t res_out0 = vqrshrun_n_s16(res0, kInterPostRoundBit + 1);
    const int16x8_t pred_10 = vld1q_s16(&prediction_0[x + 8]);
    const int16x8_t pred_11 = vld1q_s16(&prediction_1[x + 8]);
    const int16x8_t res1 = vaddq_s16(pred_10, pred_11);
    const uint8x8_t res_out1 = vqrshrun_n_s16(res1, kInterPostRoundBit + 1);
    vst1q_u8(dest + x, vcombine_u8(res_out0, res_out1));
    x += 16;
  } while (x < width);
}

void AverageBlend_NEON(const void* prediction_0, const void* prediction_1,
                       const int width, const int height, void* const dest,
                       const ptrdiff_t dest_stride) {
  auto* dst = static_cast<uint8_t*>(dest);
  const auto* pred_0 = static_cast<const int16_t*>(prediction_0);
  const auto* pred_1 = static_cast<const int16_t*>(prediction_1);
  int y = height;

  if (width == 4) {
    do {
      const uint8x8_t result = AverageBlend8Row(pred_0, pred_1);
      pred_0 += 8;
      pred_1 += 8;

      StoreLo4(dst, result);
      dst += dest_stride;
      StoreHi4(dst, result);
      dst += dest_stride;
      y -= 2;
    } while (y != 0);
    return;
  }

  if (width == 8) {
    do {
      vst1_u8(dst, AverageBlend8Row(pred_0, pred_1));
      dst += dest_stride;
      pred_0 += 8;
      pred_1 += 8;

      vst1_u8(dst, AverageBlend8Row(pred_0, pred_1));
      dst += dest_stride;
      pred_0 += 8;
      pred_1 += 8;

      y -= 2;
    } while (y != 0);
    return;
  }

  do {
    AverageBlendLargeRow(pred_0, pred_1, width, dst);
    dst += dest_stride;
    pred_0 += width;
    pred_1 += width;

    AverageBlendLargeRow(pred_0, pred_1, width, dst);
    dst += dest_stride;
    pred_0 += width;
    pred_1 += width;

    y -= 2;
  } while (y != 0);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
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
