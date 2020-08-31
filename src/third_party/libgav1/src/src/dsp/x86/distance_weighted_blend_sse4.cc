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

inline __m128i ComputeWeightedAverage8(const __m128i& pred0,
                                       const __m128i& pred1,
                                       const __m128i& weights) {
  // TODO(https://issuetracker.google.com/issues/150325685): Investigate range.
  const __m128i preds_lo = _mm_unpacklo_epi16(pred0, pred1);
  const __m128i mult_lo = _mm_madd_epi16(preds_lo, weights);
  const __m128i result_lo =
      RightShiftWithRounding_S32(mult_lo, kInterPostRoundBit + 4);

  const __m128i preds_hi = _mm_unpackhi_epi16(pred0, pred1);
  const __m128i mult_hi = _mm_madd_epi16(preds_hi, weights);
  const __m128i result_hi =
      RightShiftWithRounding_S32(mult_hi, kInterPostRoundBit + 4);

  return _mm_packs_epi32(result_lo, result_hi);
}

template <int height>
inline void DistanceWeightedBlend4xH_SSE4_1(
    const int16_t* pred_0, const int16_t* pred_1, const uint8_t weight_0,
    const uint8_t weight_1, void* const dest, const ptrdiff_t dest_stride) {
  auto* dst = static_cast<uint8_t*>(dest);
  const __m128i weights = _mm_set1_epi32(weight_0 | (weight_1 << 16));

  for (int y = 0; y < height; y += 4) {
    // TODO(b/150326556): Use larger loads.
    const __m128i src_00 = LoadLo8(pred_0);
    const __m128i src_10 = LoadLo8(pred_1);
    pred_0 += 4;
    pred_1 += 4;
    __m128i src_0 = LoadHi8(src_00, pred_0);
    __m128i src_1 = LoadHi8(src_10, pred_1);
    pred_0 += 4;
    pred_1 += 4;
    const __m128i res0 = ComputeWeightedAverage8(src_0, src_1, weights);

    const __m128i src_01 = LoadLo8(pred_0);
    const __m128i src_11 = LoadLo8(pred_1);
    pred_0 += 4;
    pred_1 += 4;
    src_0 = LoadHi8(src_01, pred_0);
    src_1 = LoadHi8(src_11, pred_1);
    pred_0 += 4;
    pred_1 += 4;
    const __m128i res1 = ComputeWeightedAverage8(src_0, src_1, weights);

    const __m128i result_pixels = _mm_packus_epi16(res0, res1);
    Store4(dst, result_pixels);
    dst += dest_stride;
    const int result_1 = _mm_extract_epi32(result_pixels, 1);
    memcpy(dst, &result_1, sizeof(result_1));
    dst += dest_stride;
    const int result_2 = _mm_extract_epi32(result_pixels, 2);
    memcpy(dst, &result_2, sizeof(result_2));
    dst += dest_stride;
    const int result_3 = _mm_extract_epi32(result_pixels, 3);
    memcpy(dst, &result_3, sizeof(result_3));
    dst += dest_stride;
  }
}

template <int height>
inline void DistanceWeightedBlend8xH_SSE4_1(
    const int16_t* pred_0, const int16_t* pred_1, const uint8_t weight_0,
    const uint8_t weight_1, void* const dest, const ptrdiff_t dest_stride) {
  auto* dst = static_cast<uint8_t*>(dest);
  const __m128i weights = _mm_set1_epi32(weight_0 | (weight_1 << 16));

  for (int y = 0; y < height; y += 2) {
    const __m128i src_00 = LoadAligned16(pred_0);
    const __m128i src_10 = LoadAligned16(pred_1);
    pred_0 += 8;
    pred_1 += 8;
    const __m128i res0 = ComputeWeightedAverage8(src_00, src_10, weights);

    const __m128i src_01 = LoadAligned16(pred_0);
    const __m128i src_11 = LoadAligned16(pred_1);
    pred_0 += 8;
    pred_1 += 8;
    const __m128i res1 = ComputeWeightedAverage8(src_01, src_11, weights);

    const __m128i result_pixels = _mm_packus_epi16(res0, res1);
    StoreLo8(dst, result_pixels);
    dst += dest_stride;
    StoreHi8(dst, result_pixels);
    dst += dest_stride;
  }
}

inline void DistanceWeightedBlendLarge_SSE4_1(
    const int16_t* pred_0, const int16_t* pred_1, const uint8_t weight_0,
    const uint8_t weight_1, const int width, const int height, void* const dest,
    const ptrdiff_t dest_stride) {
  auto* dst = static_cast<uint8_t*>(dest);
  const __m128i weights = _mm_set1_epi32(weight_0 | (weight_1 << 16));

  int y = height;
  do {
    int x = 0;
    do {
      const __m128i src_0_lo = LoadAligned16(pred_0 + x);
      const __m128i src_1_lo = LoadAligned16(pred_1 + x);
      const __m128i res_lo =
          ComputeWeightedAverage8(src_0_lo, src_1_lo, weights);

      const __m128i src_0_hi = LoadAligned16(pred_0 + x + 8);
      const __m128i src_1_hi = LoadAligned16(pred_1 + x + 8);
      const __m128i res_hi =
          ComputeWeightedAverage8(src_0_hi, src_1_hi, weights);

      StoreUnaligned16(dst + x, _mm_packus_epi16(res_lo, res_hi));
      x += 16;
    } while (x < width);
    dst += dest_stride;
    pred_0 += width;
    pred_1 += width;
  } while (--y != 0);
}

void DistanceWeightedBlend_SSE4_1(const void* prediction_0,
                                  const void* prediction_1,
                                  const uint8_t weight_0,
                                  const uint8_t weight_1, const int width,
                                  const int height, void* const dest,
                                  const ptrdiff_t dest_stride) {
  const auto* pred_0 = static_cast<const int16_t*>(prediction_0);
  const auto* pred_1 = static_cast<const int16_t*>(prediction_1);
  if (width == 4) {
    if (height == 4) {
      DistanceWeightedBlend4xH_SSE4_1<4>(pred_0, pred_1, weight_0, weight_1,
                                         dest, dest_stride);
    } else if (height == 8) {
      DistanceWeightedBlend4xH_SSE4_1<8>(pred_0, pred_1, weight_0, weight_1,
                                         dest, dest_stride);
    } else {
      assert(height == 16);
      DistanceWeightedBlend4xH_SSE4_1<16>(pred_0, pred_1, weight_0, weight_1,
                                          dest, dest_stride);
    }
    return;
  }

  if (width == 8) {
    switch (height) {
      case 4:
        DistanceWeightedBlend8xH_SSE4_1<4>(pred_0, pred_1, weight_0, weight_1,
                                           dest, dest_stride);
        return;
      case 8:
        DistanceWeightedBlend8xH_SSE4_1<8>(pred_0, pred_1, weight_0, weight_1,
                                           dest, dest_stride);
        return;
      case 16:
        DistanceWeightedBlend8xH_SSE4_1<16>(pred_0, pred_1, weight_0, weight_1,
                                            dest, dest_stride);
        return;
      default:
        assert(height == 32);
        DistanceWeightedBlend8xH_SSE4_1<32>(pred_0, pred_1, weight_0, weight_1,
                                            dest, dest_stride);

        return;
    }
  }

  DistanceWeightedBlendLarge_SSE4_1(pred_0, pred_1, weight_0, weight_1, width,
                                    height, dest, dest_stride);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
#if DSP_ENABLED_8BPP_SSE4_1(DistanceWeightedBlend)
  dsp->distance_weighted_blend = DistanceWeightedBlend_SSE4_1;
#endif
}

}  // namespace

void DistanceWeightedBlendInit_SSE4_1() { Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_SSE4_1

namespace libgav1 {
namespace dsp {

void DistanceWeightedBlendInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
