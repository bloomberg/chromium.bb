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

#if LIBGAV1_ENABLE_SSE4_1

#include <xmmintrin.h>

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/dsp/x86/common_sse4.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace {

constexpr int kInterPostRoundBit = 4;

inline void AverageBlend4Row(const int16_t* prediction_0,
                             const int16_t* prediction_1, uint8_t* dest) {
  const __m128i pred_0 = LoadLo8(prediction_0);
  const __m128i pred_1 = LoadLo8(prediction_1);
  __m128i res = _mm_add_epi16(pred_0, pred_1);
  res = RightShiftWithRounding_S16(res, kInterPostRoundBit + 1);
  Store4(dest, _mm_packus_epi16(res, res));
}

inline void AverageBlend8Row(const int16_t* prediction_0,
                             const int16_t* prediction_1, uint8_t* dest) {
  const __m128i pred_0 = LoadAligned16(prediction_0);
  const __m128i pred_1 = LoadAligned16(prediction_1);
  __m128i res = _mm_add_epi16(pred_0, pred_1);
  res = RightShiftWithRounding_S16(res, kInterPostRoundBit + 1);
  StoreLo8(dest, _mm_packus_epi16(res, res));
}

inline void AverageBlendLargeRow(const int16_t* prediction_0,
                                 const int16_t* prediction_1, const int width,
                                 uint8_t* dest) {
  int x = 0;
  do {
    const __m128i pred_00 = LoadAligned16(&prediction_0[x]);
    const __m128i pred_01 = LoadAligned16(&prediction_1[x]);
    __m128i res0 = _mm_add_epi16(pred_00, pred_01);
    res0 = RightShiftWithRounding_S16(res0, kInterPostRoundBit + 1);
    const __m128i pred_10 = LoadAligned16(&prediction_0[x + 8]);
    const __m128i pred_11 = LoadAligned16(&prediction_1[x + 8]);
    __m128i res1 = _mm_add_epi16(pred_10, pred_11);
    res1 = RightShiftWithRounding_S16(res1, kInterPostRoundBit + 1);
    StoreUnaligned16(dest + x, _mm_packus_epi16(res0, res1));
    x += 16;
  } while (x < width);
}

void AverageBlend_SSE4_1(const void* prediction_0, const void* prediction_1,
                         const int width, const int height, void* const dest,
                         const ptrdiff_t dest_stride) {
  auto* dst = static_cast<uint8_t*>(dest);
  const auto* pred_0 = static_cast<const int16_t*>(prediction_0);
  const auto* pred_1 = static_cast<const int16_t*>(prediction_1);
  int y = height;

  if (width == 4) {
    do {
      // TODO(b/150326556): |prediction_[01]| values are packed. It is possible
      // to load 8 values at a time.
      AverageBlend4Row(pred_0, pred_1, dst);
      dst += dest_stride;
      pred_0 += width;
      pred_1 += width;

      AverageBlend4Row(pred_0, pred_1, dst);
      dst += dest_stride;
      pred_0 += width;
      pred_1 += width;

      y -= 2;
    } while (y != 0);
    return;
  }

  if (width == 8) {
    do {
      AverageBlend8Row(pred_0, pred_1, dst);
      dst += dest_stride;
      pred_0 += width;
      pred_1 += width;

      AverageBlend8Row(pred_0, pred_1, dst);
      dst += dest_stride;
      pred_0 += width;
      pred_1 += width;

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
#if DSP_ENABLED_8BPP_SSE4_1(AverageBlend)
  dsp->average_blend = AverageBlend_SSE4_1;
#endif
}

}  // namespace

void AverageBlendInit_SSE4_1() { Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_SSE4_1

namespace libgav1 {
namespace dsp {

void AverageBlendInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
