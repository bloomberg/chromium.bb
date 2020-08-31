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

#if LIBGAV1_ENABLE_SSE4_1

#include <xmmintrin.h>

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/dsp/x86/common_sse4.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {
namespace {

#include "src/dsp/obmc.inc"

inline void OverlapBlendFromLeft2xH_SSE4_1(
    uint8_t* const prediction, const ptrdiff_t prediction_stride,
    const int height, const uint8_t* const obmc_prediction,
    const ptrdiff_t obmc_prediction_stride) {
  uint8_t* pred = prediction;
  const uint8_t* obmc_pred = obmc_prediction;
  const __m128i mask_inverter = _mm_cvtsi32_si128(0x40404040);
  const __m128i mask_val = _mm_shufflelo_epi16(Load4(kObmcMask), 0);
  // 64 - mask
  const __m128i obmc_mask_val = _mm_sub_epi8(mask_inverter, mask_val);
  const __m128i masks = _mm_unpacklo_epi8(mask_val, obmc_mask_val);
  int y = height;
  do {
    const __m128i pred_val = Load2x2(pred, pred + prediction_stride);
    const __m128i obmc_pred_val =
        Load2x2(obmc_pred, obmc_pred + obmc_prediction_stride);

    const __m128i terms = _mm_unpacklo_epi8(pred_val, obmc_pred_val);
    const __m128i result =
        RightShiftWithRounding_U16(_mm_maddubs_epi16(terms, masks), 6);
    const __m128i packed_result = _mm_packus_epi16(result, result);
    Store2(pred, packed_result);
    pred += prediction_stride;
    const int16_t second_row_result = _mm_extract_epi16(packed_result, 1);
    memcpy(pred, &second_row_result, sizeof(second_row_result));
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride << 1;
    y -= 2;
  } while (y != 0);
}

inline void OverlapBlendFromLeft4xH_SSE4_1(
    uint8_t* const prediction, const ptrdiff_t prediction_stride,
    const int height, const uint8_t* const obmc_prediction,
    const ptrdiff_t obmc_prediction_stride) {
  uint8_t* pred = prediction;
  const uint8_t* obmc_pred = obmc_prediction;
  const __m128i mask_inverter = _mm_cvtsi32_si128(0x40404040);
  const __m128i mask_val = Load4(kObmcMask + 2);
  // 64 - mask
  const __m128i obmc_mask_val = _mm_sub_epi8(mask_inverter, mask_val);
  // Duplicate first half of vector.
  const __m128i masks =
      _mm_shuffle_epi32(_mm_unpacklo_epi8(mask_val, obmc_mask_val), 0x44);
  int y = height;
  do {
    const __m128i pred_val0 = Load4(pred);
    const __m128i obmc_pred_val0 = Load4(obmc_pred);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;

    // Place the second row of each source in the second four bytes.
    const __m128i pred_val =
        _mm_alignr_epi8(Load4(pred), _mm_slli_si128(pred_val0, 12), 12);
    const __m128i obmc_pred_val = _mm_alignr_epi8(
        Load4(obmc_pred), _mm_slli_si128(obmc_pred_val0, 12), 12);
    const __m128i terms = _mm_unpacklo_epi8(pred_val, obmc_pred_val);
    const __m128i result =
        RightShiftWithRounding_U16(_mm_maddubs_epi16(terms, masks), 6);
    const __m128i packed_result = _mm_packus_epi16(result, result);
    Store4(pred - prediction_stride, packed_result);
    const int second_row_result = _mm_extract_epi32(packed_result, 1);
    memcpy(pred, &second_row_result, sizeof(second_row_result));
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;
    y -= 2;
  } while (y != 0);
}

inline void OverlapBlendFromLeft8xH_SSE4_1(
    uint8_t* const prediction, const ptrdiff_t prediction_stride,
    const int height, const uint8_t* const obmc_prediction,
    const ptrdiff_t obmc_prediction_stride) {
  uint8_t* pred = prediction;
  const uint8_t* obmc_pred = obmc_prediction;
  const __m128i mask_inverter = _mm_set1_epi8(64);
  const __m128i mask_val = LoadLo8(kObmcMask + 6);
  // 64 - mask
  const __m128i obmc_mask_val = _mm_sub_epi8(mask_inverter, mask_val);
  const __m128i masks = _mm_unpacklo_epi8(mask_val, obmc_mask_val);
  int y = height;
  do {
    const __m128i pred_val = LoadLo8(pred);
    const __m128i obmc_pred_val = LoadLo8(obmc_pred);
    const __m128i terms = _mm_unpacklo_epi8(pred_val, obmc_pred_val);
    const __m128i result =
        RightShiftWithRounding_U16(_mm_maddubs_epi16(terms, masks), 6);

    StoreLo8(pred, _mm_packus_epi16(result, result));
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;
  } while (--y != 0);
}

void OverlapBlendFromLeft_SSE4_1(void* const prediction,
                                 const ptrdiff_t prediction_stride,
                                 const int width, const int height,
                                 const void* const obmc_prediction,
                                 const ptrdiff_t obmc_prediction_stride) {
  auto* pred = static_cast<uint8_t*>(prediction);
  const auto* obmc_pred = static_cast<const uint8_t*>(obmc_prediction);

  if (width == 2) {
    OverlapBlendFromLeft2xH_SSE4_1(pred, prediction_stride, height, obmc_pred,
                                   obmc_prediction_stride);
    return;
  }
  if (width == 4) {
    OverlapBlendFromLeft4xH_SSE4_1(pred, prediction_stride, height, obmc_pred,
                                   obmc_prediction_stride);
    return;
  }
  if (width == 8) {
    OverlapBlendFromLeft8xH_SSE4_1(pred, prediction_stride, height, obmc_pred,
                                   obmc_prediction_stride);
    return;
  }
  const __m128i mask_inverter = _mm_set1_epi8(64);
  const uint8_t* mask = kObmcMask + width - 2;
  int x = 0;
  do {
    pred = static_cast<uint8_t*>(prediction) + x;
    obmc_pred = static_cast<const uint8_t*>(obmc_prediction) + x;
    const __m128i mask_val = LoadUnaligned16(mask + x);
    // 64 - mask
    const __m128i obmc_mask_val = _mm_sub_epi8(mask_inverter, mask_val);
    const __m128i masks_lo = _mm_unpacklo_epi8(mask_val, obmc_mask_val);
    const __m128i masks_hi = _mm_unpackhi_epi8(mask_val, obmc_mask_val);

    int y = 0;
    do {
      const __m128i pred_val = LoadUnaligned16(pred);
      const __m128i obmc_pred_val = LoadUnaligned16(obmc_pred);
      const __m128i terms_lo = _mm_unpacklo_epi8(pred_val, obmc_pred_val);
      const __m128i result_lo =
          RightShiftWithRounding_U16(_mm_maddubs_epi16(terms_lo, masks_lo), 6);
      const __m128i terms_hi = _mm_unpackhi_epi8(pred_val, obmc_pred_val);
      const __m128i result_hi =
          RightShiftWithRounding_U16(_mm_maddubs_epi16(terms_hi, masks_hi), 6);
      StoreUnaligned16(pred, _mm_packus_epi16(result_lo, result_hi));

      pred += prediction_stride;
      obmc_pred += obmc_prediction_stride;
    } while (++y < height);
    x += 16;
  } while (x < width);
}

inline void OverlapBlendFromTop4xH_SSE4_1(
    uint8_t* const prediction, const ptrdiff_t prediction_stride,
    const int height, const uint8_t* const obmc_prediction,
    const ptrdiff_t obmc_prediction_stride) {
  uint8_t* pred = prediction;
  const uint8_t* obmc_pred = obmc_prediction;
  const __m128i mask_inverter = _mm_set1_epi16(64);
  const __m128i mask_shuffler = _mm_set_epi32(0x01010101, 0x01010101, 0, 0);
  const __m128i mask_preinverter = _mm_set1_epi16(-256 | 1);

  const uint8_t* mask = kObmcMask + height - 2;
  const int compute_height = height - (height >> 2);
  int y = 0;
  do {
    // First mask in the first half, second mask in the second half.
    const __m128i mask_val = _mm_shuffle_epi8(
        _mm_cvtsi32_si128(*reinterpret_cast<const uint16_t*>(mask + y)),
        mask_shuffler);
    const __m128i masks =
        _mm_sub_epi8(mask_inverter, _mm_sign_epi8(mask_val, mask_preinverter));
    const __m128i pred_val0 = Load4(pred);

    const __m128i obmc_pred_val0 = Load4(obmc_pred);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;
    const __m128i pred_val =
        _mm_alignr_epi8(Load4(pred), _mm_slli_si128(pred_val0, 12), 12);
    const __m128i obmc_pred_val = _mm_alignr_epi8(
        Load4(obmc_pred), _mm_slli_si128(obmc_pred_val0, 12), 12);
    const __m128i terms = _mm_unpacklo_epi8(obmc_pred_val, pred_val);
    const __m128i result =
        RightShiftWithRounding_U16(_mm_maddubs_epi16(terms, masks), 6);

    const __m128i packed_result = _mm_packus_epi16(result, result);
    Store4(pred - prediction_stride, packed_result);
    Store4(pred, _mm_srli_si128(packed_result, 4));
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;
    y += 2;
  } while (y < compute_height);
}

inline void OverlapBlendFromTop8xH_SSE4_1(
    uint8_t* const prediction, const ptrdiff_t prediction_stride,
    const int height, const uint8_t* const obmc_prediction,
    const ptrdiff_t obmc_prediction_stride) {
  uint8_t* pred = prediction;
  const uint8_t* obmc_pred = obmc_prediction;
  const uint8_t* mask = kObmcMask + height - 2;
  const __m128i mask_inverter = _mm_set1_epi8(64);
  const int compute_height = height - (height >> 2);
  int y = compute_height;
  do {
    const __m128i mask_val = _mm_set1_epi8(mask[compute_height - y]);
    // 64 - mask
    const __m128i obmc_mask_val = _mm_sub_epi8(mask_inverter, mask_val);
    const __m128i masks = _mm_unpacklo_epi8(mask_val, obmc_mask_val);
    const __m128i pred_val = LoadLo8(pred);
    const __m128i obmc_pred_val = LoadLo8(obmc_pred);
    const __m128i terms = _mm_unpacklo_epi8(pred_val, obmc_pred_val);
    const __m128i result =
        RightShiftWithRounding_U16(_mm_maddubs_epi16(terms, masks), 6);

    StoreLo8(pred, _mm_packus_epi16(result, result));
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;
  } while (--y != 0);
}

void OverlapBlendFromTop_SSE4_1(void* const prediction,
                                const ptrdiff_t prediction_stride,
                                const int width, const int height,
                                const void* const obmc_prediction,
                                const ptrdiff_t obmc_prediction_stride) {
  auto* pred = static_cast<uint8_t*>(prediction);
  const auto* obmc_pred = static_cast<const uint8_t*>(obmc_prediction);

  if (width <= 4) {
    OverlapBlendFromTop4xH_SSE4_1(pred, prediction_stride, height, obmc_pred,
                                  obmc_prediction_stride);
    return;
  }
  if (width == 8) {
    OverlapBlendFromTop8xH_SSE4_1(pred, prediction_stride, height, obmc_pred,
                                  obmc_prediction_stride);
    return;
  }

  // Stop when mask value becomes 64.
  const int compute_height = height - (height >> 2);
  const __m128i mask_inverter = _mm_set1_epi8(64);
  int y = 0;
  const uint8_t* mask = kObmcMask + height - 2;
  do {
    const __m128i mask_val = _mm_set1_epi8(mask[y]);
    // 64 - mask
    const __m128i obmc_mask_val = _mm_sub_epi8(mask_inverter, mask_val);
    const __m128i masks = _mm_unpacklo_epi8(mask_val, obmc_mask_val);
    int x = 0;
    do {
      const __m128i pred_val = LoadUnaligned16(pred + x);
      const __m128i obmc_pred_val = LoadUnaligned16(obmc_pred + x);
      const __m128i terms_lo = _mm_unpacklo_epi8(pred_val, obmc_pred_val);
      const __m128i result_lo =
          RightShiftWithRounding_U16(_mm_maddubs_epi16(terms_lo, masks), 6);
      const __m128i terms_hi = _mm_unpackhi_epi8(pred_val, obmc_pred_val);
      const __m128i result_hi =
          RightShiftWithRounding_U16(_mm_maddubs_epi16(terms_hi, masks), 6);
      StoreUnaligned16(pred + x, _mm_packus_epi16(result_lo, result_hi));
      x += 16;
    } while (x < width);
    pred += prediction_stride;
    obmc_pred += obmc_prediction_stride;
  } while (++y < compute_height);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
#if DSP_ENABLED_8BPP_SSE4_1(ObmcVertical)
  dsp->obmc_blend[kObmcDirectionVertical] = OverlapBlendFromTop_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(ObmcHorizontal)
  dsp->obmc_blend[kObmcDirectionHorizontal] = OverlapBlendFromLeft_SSE4_1;
#endif
}

}  // namespace

void ObmcInit_SSE4_1() { Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_SSE4_1

namespace libgav1 {
namespace dsp {

void ObmcInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
