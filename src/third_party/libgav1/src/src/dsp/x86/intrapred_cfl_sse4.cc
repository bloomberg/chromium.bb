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

#if LIBGAV1_ENABLE_SSE4_1

#include <smmintrin.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/dsp/x86/common_sse4.h"
#include "src/utils/common.h"
#include "src/utils/compiler_attributes.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

//------------------------------------------------------------------------------
// CflIntraPredictor_SSE4_1

inline __m128i CflPredictUnclipped(const __m128i* input, __m128i alpha_q12,
                                   __m128i alpha_sign, __m128i dc_q0) {
  __m128i ac_q3 = LoadUnaligned16(input);
  __m128i ac_sign = _mm_sign_epi16(alpha_sign, ac_q3);
  __m128i scaled_luma_q0 = _mm_mulhrs_epi16(_mm_abs_epi16(ac_q3), alpha_q12);
  scaled_luma_q0 = _mm_sign_epi16(scaled_luma_q0, ac_sign);
  return _mm_add_epi16(scaled_luma_q0, dc_q0);
}

template <int width, int height>
void CflIntraPredictor_SSE4_1(
    void* const dest, ptrdiff_t stride,
    const int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int alpha) {
  auto* dst = static_cast<uint8_t*>(dest);
  const __m128i alpha_sign = _mm_set1_epi16(alpha);
  const __m128i alpha_q12 = _mm_slli_epi16(_mm_abs_epi16(alpha_sign), 9);
  auto* row = reinterpret_cast<const __m128i*>(luma);
  const int kCflLumaBufferStrideLog2_16i = 5;
  const int kCflLumaBufferStrideLog2_128i = kCflLumaBufferStrideLog2_16i - 3;
  const __m128i* row_end = row + (height << kCflLumaBufferStrideLog2_128i);
  const __m128i dc_val = _mm_set1_epi16(dst[0]);
  do {
    __m128i res = CflPredictUnclipped(row, alpha_q12, alpha_sign, dc_val);
    if (width < 16) {
      res = _mm_packus_epi16(res, res);
      if (width == 4) {
        Store4(dst, res);
      } else {
        StoreLo8(dst, res);
      }
    } else {
      __m128i next =
          CflPredictUnclipped(row + 1, alpha_q12, alpha_sign, dc_val);
      res = _mm_packus_epi16(res, next);
      StoreUnaligned16(dst, res);
      if (width == 32) {
        res = CflPredictUnclipped(row + 2, alpha_q12, alpha_sign, dc_val);
        next = CflPredictUnclipped(row + 3, alpha_q12, alpha_sign, dc_val);
        res = _mm_packus_epi16(res, next);
        StoreUnaligned16(dst + 16, res);
      }
    }
    dst += stride;
  } while ((row += (1 << kCflLumaBufferStrideLog2_128i)) < row_end);
}

template <int block_height_log2, bool is_inside>
void CflSubsampler444_4xH_SSE4_1(
    int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int /*max_luma_width*/, const int max_luma_height,
    const void* const source, ptrdiff_t stride) {
  static_assert(block_height_log2 <= 4, "");
  const int block_height = 1 << block_height_log2;
  const int visible_height = max_luma_height;
  const auto* src = static_cast<const uint8_t*>(source);
  __m128i sum = _mm_setzero_si128();
  int16_t* luma_ptr = luma[0];
  const __m128i zero = _mm_setzero_si128();
  __m128i samples;
  int y = 0;
  do {
    samples = Load4(src);
    src += stride;
    int src_bytes;
    memcpy(&src_bytes, src, 4);
    samples = _mm_insert_epi32(samples, src_bytes, 1);
    src += stride;
    samples = _mm_slli_epi16(_mm_cvtepu8_epi16(samples), 3);
    StoreLo8(luma_ptr, samples);
    luma_ptr += kCflLumaBufferStride;
    StoreHi8(luma_ptr, samples);
    luma_ptr += kCflLumaBufferStride;

    // The maximum value here is 2**bd * H * 2**shift. Since the maximum H for
    // 4XH is 16 = 2**4, we have 2**(8 + 4 + 3) = 2**15, which fits in 16 bits.
    sum = _mm_add_epi16(sum, samples);
    y += 2;
  } while (y < visible_height);

  if (!is_inside) {
    int y = visible_height;
    do {
      StoreHi8(luma_ptr, samples);
      luma_ptr += kCflLumaBufferStride;
      sum = _mm_add_epi16(sum, samples);
      ++y;
    } while (y < block_height);
  }

  __m128i sum_tmp = _mm_unpackhi_epi16(sum, zero);
  sum = _mm_cvtepu16_epi32(sum);
  sum = _mm_add_epi32(sum, sum_tmp);
  sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
  sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 4));

  __m128i averages = RightShiftWithRounding_U32(
      sum, block_height_log2 + 2 /* log2 of width 4 */);
  averages = _mm_shufflelo_epi16(averages, 0);
  luma_ptr = luma[0];
  for (int y = 0; y < block_height; ++y, luma_ptr += kCflLumaBufferStride) {
    const __m128i samples = LoadLo8(luma_ptr);
    StoreLo8(luma_ptr, _mm_sub_epi16(samples, averages));
  }
}

template <int block_height_log2>
void CflSubsampler444_4xH_SSE4_1(
    int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int max_luma_width, const int max_luma_height,
    const void* const source, ptrdiff_t stride) {
  static_assert(block_height_log2 <= 4, "");
  assert(max_luma_width >= 4);
  assert(max_luma_height >= 4);
  const int block_height = 1 << block_height_log2;
  const int block_width = 4;

  if (block_height <= max_luma_height && block_width <= max_luma_width) {
    CflSubsampler444_4xH_SSE4_1<block_height_log2, true>(
        luma, max_luma_width, max_luma_height, source, stride);
  } else {
    CflSubsampler444_4xH_SSE4_1<block_height_log2, false>(
        luma, max_luma_width, max_luma_height, source, stride);
  }
}

template <int block_height_log2, bool inside>
void CflSubsampler444_8xH_SSE4_1(
    int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int max_luma_width, const int max_luma_height,
    const void* const source, ptrdiff_t stride) {
  static_assert(block_height_log2 <= 5, "");
  const int block_height = 1 << block_height_log2, block_width = 8;
  const int visible_height = max_luma_height;
  const int invisible_width = inside ? 0 : block_width - max_luma_width;
  const int visible_width = max_luma_width;
  const __m128i blend_mask =
      inside ? _mm_setzero_si128() : MaskHighNBytes(8 + invisible_width);
  const __m128i dup16 = _mm_set1_epi32(0x01000100);
  const auto* src = static_cast<const uint8_t*>(source);
  int16_t* luma_ptr = luma[0];
  const __m128i zero = _mm_setzero_si128();
  // Since the maximum height is 32, if we split them by parity, each one only
  // needs to accumulate 16 rows. Just like the calculation done in 4XH, we can
  // store them in 16 bits without casting to 32 bits.
  __m128i sum_even = _mm_setzero_si128(), sum_odd = _mm_setzero_si128();
  __m128i sum;
  __m128i samples1;

  int y = 0;
  do {
    __m128i samples0 = LoadLo8(src);
    if (!inside) {
      const __m128i border0 =
          _mm_set1_epi8(static_cast<int8_t>(src[visible_width - 1]));
      samples0 = _mm_blendv_epi8(samples0, border0, blend_mask);
    }
    src += stride;
    samples0 = _mm_slli_epi16(_mm_cvtepu8_epi16(samples0), 3);
    StoreUnaligned16(luma_ptr, samples0);
    luma_ptr += kCflLumaBufferStride;

    sum_even = _mm_add_epi16(sum_even, samples0);

    samples1 = LoadLo8(src);
    if (!inside) {
      const __m128i border1 =
          _mm_set1_epi8(static_cast<int8_t>(src[visible_width - 1]));
      samples1 = _mm_blendv_epi8(samples1, border1, blend_mask);
    }
    src += stride;
    samples1 = _mm_slli_epi16(_mm_cvtepu8_epi16(samples1), 3);
    StoreUnaligned16(luma_ptr, samples1);
    luma_ptr += kCflLumaBufferStride;

    sum_odd = _mm_add_epi16(sum_odd, samples1);
    y += 2;
  } while (y < visible_height);

  if (!inside) {
    for (int y = visible_height; y < block_height; y += 2) {
      sum_even = _mm_add_epi16(sum_even, samples1);
      StoreUnaligned16(luma_ptr, samples1);
      luma_ptr += kCflLumaBufferStride;

      sum_odd = _mm_add_epi16(sum_odd, samples1);
      StoreUnaligned16(luma_ptr, samples1);
      luma_ptr += kCflLumaBufferStride;
    }
  }

  sum = _mm_add_epi32(_mm_unpackhi_epi16(sum_even, zero),
                      _mm_cvtepu16_epi32(sum_even));
  sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(sum_odd, zero));
  sum = _mm_add_epi32(sum, _mm_cvtepu16_epi32(sum_odd));

  sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
  sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 4));

  __m128i averages = RightShiftWithRounding_U32(
      sum, block_height_log2 + 3 /* log2 of width 8 */);
  averages = _mm_shuffle_epi8(averages, dup16);
  luma_ptr = luma[0];
  for (int y = 0; y < block_height; ++y, luma_ptr += kCflLumaBufferStride) {
    const __m128i samples = LoadUnaligned16(luma_ptr);
    StoreUnaligned16(luma_ptr, _mm_sub_epi16(samples, averages));
  }
}

template <int block_height_log2>
void CflSubsampler444_8xH_SSE4_1(
    int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int max_luma_width, const int max_luma_height,
    const void* const source, ptrdiff_t stride) {
  static_assert(block_height_log2 <= 5, "");
  assert(max_luma_width >= 4);
  assert(max_luma_height >= 4);
  const int block_height = 1 << block_height_log2;
  const int block_width = 8;

  const int horz_inside = block_width <= max_luma_width;
  const int vert_inside = block_height <= max_luma_height;
  if (horz_inside && vert_inside) {
    CflSubsampler444_8xH_SSE4_1<block_height_log2, true>(
        luma, max_luma_width, max_luma_height, source, stride);
  } else {
    CflSubsampler444_8xH_SSE4_1<block_height_log2, false>(
        luma, max_luma_width, max_luma_height, source, stride);
  }
}

// This function will only work for block_width 16 and 32.
template <int block_width_log2, int block_height_log2, bool inside>
void CflSubsampler444_SSE4_1(
    int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int max_luma_width, const int max_luma_height,
    const void* const source, ptrdiff_t stride) {
  static_assert(block_width_log2 == 4 || block_width_log2 == 5, "");
  static_assert(block_height_log2 <= 5, "");
  assert(max_luma_width >= 4);
  assert(max_luma_height >= 4);
  const int block_height = 1 << block_height_log2;
  const int block_width = 1 << block_width_log2;

  const int visible_height = max_luma_height;
  const int visible_width_16 = inside ? 16 : std::min(16, max_luma_width);
  const int invisible_width_16 = 16 - visible_width_16;
  const __m128i blend_mask_16 = MaskHighNBytes(invisible_width_16);
  const int visible_width_32 = inside ? 32 : max_luma_width;
  const int invisible_width_32 = 32 - visible_width_32;
  const __m128i blend_mask_32 =
      MaskHighNBytes(std::min(16, invisible_width_32));

  const __m128i dup16 = _mm_set1_epi32(0x01000100);
  const __m128i zero = _mm_setzero_si128();
  const auto* src = static_cast<const uint8_t*>(source);
  int16_t* luma_ptr = luma[0];
  __m128i sum = _mm_setzero_si128();

  __m128i samples0, samples1;
  __m128i samples2, samples3;
  __m128i inner_sum_lo, inner_sum_hi;
  int y = 0;
  do {
#if LIBGAV1_MSAN  // We can load uninitialized values here. Even though they are
                  // then masked off by blendv, MSAN isn't smart enough to
                  // understand that. So we switch to a C implementation here.
    uint16_t c_arr[16];
    for (int x = 0; x < 16; x++) {
      const int x_index = std::min(x, visible_width_16 - 1);
      c_arr[x] = src[x_index] << 3;
    }
    samples0 = LoadUnaligned16(c_arr);
    samples1 = LoadUnaligned16(c_arr + 8);
    static_cast<void>(blend_mask_16);
#else
    __m128i samples01 = LoadUnaligned16(src);

    if (!inside) {
      const __m128i border16 =
          _mm_set1_epi8(static_cast<int8_t>(src[visible_width_16 - 1]));
      samples01 = _mm_blendv_epi8(samples01, border16, blend_mask_16);
    }
    samples0 = _mm_slli_epi16(_mm_cvtepu8_epi16(samples01), 3);
    samples1 = _mm_slli_epi16(_mm_unpackhi_epi8(samples01, zero), 3);
#endif  // LIBGAV1_MSAN

    StoreUnaligned16(luma_ptr, samples0);
    StoreUnaligned16(luma_ptr + 8, samples1);
    __m128i inner_sum = _mm_add_epi16(samples0, samples1);

    if (block_width == 32) {
#if LIBGAV1_MSAN  // We can load uninitialized values here. Even though they are
                  // then masked off by blendv, MSAN isn't smart enough to
                  // understand that. So we switch to a C implementation here.
      uint16_t c_arr[16];
      for (int x = 16; x < 32; x++) {
        const int x_index = std::min(x, visible_width_32 - 1);
        c_arr[x - 16] = src[x_index] << 3;
      }
      samples2 = LoadUnaligned16(c_arr);
      samples3 = LoadUnaligned16(c_arr + 8);
      static_cast<void>(blend_mask_32);
#else
      __m128i samples23 = LoadUnaligned16(src + 16);
      if (!inside) {
        const __m128i border32 =
            _mm_set1_epi8(static_cast<int8_t>(src[visible_width_32 - 1]));
        samples23 = _mm_blendv_epi8(samples23, border32, blend_mask_32);
      }
      samples2 = _mm_slli_epi16(_mm_cvtepu8_epi16(samples23), 3);
      samples3 = _mm_slli_epi16(_mm_unpackhi_epi8(samples23, zero), 3);
#endif  // LIBGAV1_MSAN

      StoreUnaligned16(luma_ptr + 16, samples2);
      StoreUnaligned16(luma_ptr + 24, samples3);
      inner_sum = _mm_add_epi16(samples2, inner_sum);
      inner_sum = _mm_add_epi16(samples3, inner_sum);
    }

    inner_sum_lo = _mm_cvtepu16_epi32(inner_sum);
    inner_sum_hi = _mm_unpackhi_epi16(inner_sum, zero);
    sum = _mm_add_epi32(sum, inner_sum_lo);
    sum = _mm_add_epi32(sum, inner_sum_hi);
    luma_ptr += kCflLumaBufferStride;
    src += stride;
  } while (++y < visible_height);

  if (!inside) {
    for (int y = visible_height; y < block_height;
         luma_ptr += kCflLumaBufferStride, ++y) {
      sum = _mm_add_epi32(sum, inner_sum_lo);
      StoreUnaligned16(luma_ptr, samples0);
      sum = _mm_add_epi32(sum, inner_sum_hi);
      StoreUnaligned16(luma_ptr + 8, samples1);
      if (block_width == 32) {
        StoreUnaligned16(luma_ptr + 16, samples2);
        StoreUnaligned16(luma_ptr + 24, samples3);
      }
    }
  }

  sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
  sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 4));

  __m128i averages =
      RightShiftWithRounding_U32(sum, block_width_log2 + block_height_log2);
  averages = _mm_shuffle_epi8(averages, dup16);
  luma_ptr = luma[0];
  for (int y = 0; y < block_height; ++y, luma_ptr += kCflLumaBufferStride) {
    for (int x = 0; x < block_width; x += 8) {
      __m128i samples = LoadUnaligned16(&luma_ptr[x]);
      StoreUnaligned16(&luma_ptr[x], _mm_sub_epi16(samples, averages));
    }
  }
}

template <int block_width_log2, int block_height_log2>
void CflSubsampler444_SSE4_1(
    int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int max_luma_width, const int max_luma_height,
    const void* const source, ptrdiff_t stride) {
  static_assert(block_width_log2 == 4 || block_width_log2 == 5, "");
  static_assert(block_height_log2 <= 5, "");
  assert(max_luma_width >= 4);
  assert(max_luma_height >= 4);

  const int block_height = 1 << block_height_log2;
  const int block_width = 1 << block_width_log2;
  const int horz_inside = block_width <= max_luma_width;
  const int vert_inside = block_height <= max_luma_height;
  if (horz_inside && vert_inside) {
    CflSubsampler444_SSE4_1<block_width_log2, block_height_log2, true>(
        luma, max_luma_width, max_luma_height, source, stride);
  } else {
    CflSubsampler444_SSE4_1<block_width_log2, block_height_log2, false>(
        luma, max_luma_width, max_luma_height, source, stride);
  }
}

// Takes in two sums of input row pairs, and completes the computation for two
// output rows.
inline __m128i StoreLumaResults4_420(const __m128i vertical_sum0,
                                     const __m128i vertical_sum1,
                                     int16_t* luma_ptr) {
  __m128i result = _mm_hadd_epi16(vertical_sum0, vertical_sum1);
  result = _mm_slli_epi16(result, 1);
  StoreLo8(luma_ptr, result);
  StoreHi8(luma_ptr + kCflLumaBufferStride, result);
  return result;
}

// Takes two halves of a vertically added pair of rows and completes the
// computation for one output row.
inline __m128i StoreLumaResults8_420(const __m128i vertical_sum0,
                                     const __m128i vertical_sum1,
                                     int16_t* luma_ptr) {
  __m128i result = _mm_hadd_epi16(vertical_sum0, vertical_sum1);
  result = _mm_slli_epi16(result, 1);
  StoreUnaligned16(luma_ptr, result);
  return result;
}

template <int block_height_log2>
void CflSubsampler420_4xH_SSE4_1(
    int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int /*max_luma_width*/, const int max_luma_height,
    const void* const source, ptrdiff_t stride) {
  const int block_height = 1 << block_height_log2;
  const auto* src = static_cast<const uint8_t*>(source);
  int16_t* luma_ptr = luma[0];
  const __m128i zero = _mm_setzero_si128();
  __m128i final_sum = zero;
  const int luma_height = std::min(block_height, max_luma_height >> 1);
  int y = 0;
  do {
    // Note that double sampling and converting to 16bit makes a row fill the
    // vector.
    const __m128i samples_row0 = _mm_cvtepu8_epi16(LoadLo8(src));
    src += stride;
    const __m128i samples_row1 = _mm_cvtepu8_epi16(LoadLo8(src));
    src += stride;
    const __m128i luma_sum01 = _mm_add_epi16(samples_row0, samples_row1);

    const __m128i samples_row2 = _mm_cvtepu8_epi16(LoadLo8(src));
    src += stride;
    const __m128i samples_row3 = _mm_cvtepu8_epi16(LoadLo8(src));
    src += stride;
    const __m128i luma_sum23 = _mm_add_epi16(samples_row2, samples_row3);
    __m128i sum = StoreLumaResults4_420(luma_sum01, luma_sum23, luma_ptr);
    luma_ptr += kCflLumaBufferStride << 1;

    const __m128i samples_row4 = _mm_cvtepu8_epi16(LoadLo8(src));
    src += stride;
    const __m128i samples_row5 = _mm_cvtepu8_epi16(LoadLo8(src));
    src += stride;
    const __m128i luma_sum45 = _mm_add_epi16(samples_row4, samples_row5);

    const __m128i samples_row6 = _mm_cvtepu8_epi16(LoadLo8(src));
    src += stride;
    const __m128i samples_row7 = _mm_cvtepu8_epi16(LoadLo8(src));
    src += stride;
    const __m128i luma_sum67 = _mm_add_epi16(samples_row6, samples_row7);
    sum = _mm_add_epi16(
        sum, StoreLumaResults4_420(luma_sum45, luma_sum67, luma_ptr));
    luma_ptr += kCflLumaBufferStride << 1;

    final_sum = _mm_add_epi32(final_sum, _mm_cvtepu16_epi32(sum));
    final_sum = _mm_add_epi32(final_sum, _mm_unpackhi_epi16(sum, zero));
    y += 4;
  } while (y < luma_height);
  const __m128i final_fill = LoadLo8(luma_ptr - kCflLumaBufferStride);
  const __m128i final_fill_to_sum = _mm_cvtepu16_epi32(final_fill);
  for (; y < block_height; ++y) {
    StoreLo8(luma_ptr, final_fill);
    luma_ptr += kCflLumaBufferStride;

    final_sum = _mm_add_epi32(final_sum, final_fill_to_sum);
  }
  final_sum = _mm_add_epi32(final_sum, _mm_srli_si128(final_sum, 8));
  final_sum = _mm_add_epi32(final_sum, _mm_srli_si128(final_sum, 4));

  __m128i averages = RightShiftWithRounding_U32(
      final_sum, block_height_log2 + 2 /*log2 of width 4*/);

  averages = _mm_shufflelo_epi16(averages, 0);
  luma_ptr = luma[0];
  for (int y = 0; y < block_height; ++y, luma_ptr += kCflLumaBufferStride) {
    const __m128i samples = LoadLo8(luma_ptr);
    StoreLo8(luma_ptr, _mm_sub_epi16(samples, averages));
  }
}

// This duplicates the last two 16-bit values in |row|.
inline __m128i LastRowSamples(const __m128i row) {
  return _mm_shuffle_epi32(row, 0xFF);
}

// This duplicates the last 16-bit value in |row|.
inline __m128i LastRowResult(const __m128i row) {
  const __m128i dup_row = _mm_shufflehi_epi16(row, 0xFF);
  return _mm_shuffle_epi32(dup_row, 0xFF);
}

template <int block_height_log2, int max_luma_width>
inline void CflSubsampler420Impl_8xH_SSE4_1(
    int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int /*max_luma_width*/, const int max_luma_height,
    const void* const source, ptrdiff_t stride) {
  const int block_height = 1 << block_height_log2;
  const auto* src = static_cast<const uint8_t*>(source);
  const __m128i zero = _mm_setzero_si128();
  __m128i final_sum = zero;
  int16_t* luma_ptr = luma[0];
  const int luma_height = std::min(block_height, max_luma_height >> 1);
  int y = 0;

  do {
    const __m128i samples_row00 = _mm_cvtepu8_epi16(LoadLo8(src));
    const __m128i samples_row01 = (max_luma_width == 16)
                                      ? _mm_cvtepu8_epi16(LoadLo8(src + 8))
                                      : LastRowSamples(samples_row00);
    src += stride;
    const __m128i samples_row10 = _mm_cvtepu8_epi16(LoadLo8(src));
    const __m128i samples_row11 = (max_luma_width == 16)
                                      ? _mm_cvtepu8_epi16(LoadLo8(src + 8))
                                      : LastRowSamples(samples_row10);
    src += stride;
    const __m128i luma_sum00 = _mm_add_epi16(samples_row00, samples_row10);
    const __m128i luma_sum01 = _mm_add_epi16(samples_row01, samples_row11);
    __m128i sum = StoreLumaResults8_420(luma_sum00, luma_sum01, luma_ptr);
    luma_ptr += kCflLumaBufferStride;

    const __m128i samples_row20 = _mm_cvtepu8_epi16(LoadLo8(src));
    const __m128i samples_row21 = (max_luma_width == 16)
                                      ? _mm_cvtepu8_epi16(LoadLo8(src + 8))
                                      : LastRowSamples(samples_row20);
    src += stride;
    const __m128i samples_row30 = _mm_cvtepu8_epi16(LoadLo8(src));
    const __m128i samples_row31 = (max_luma_width == 16)
                                      ? _mm_cvtepu8_epi16(LoadLo8(src + 8))
                                      : LastRowSamples(samples_row30);
    src += stride;
    const __m128i luma_sum10 = _mm_add_epi16(samples_row20, samples_row30);
    const __m128i luma_sum11 = _mm_add_epi16(samples_row21, samples_row31);
    sum = _mm_add_epi16(
        sum, StoreLumaResults8_420(luma_sum10, luma_sum11, luma_ptr));
    luma_ptr += kCflLumaBufferStride;

    const __m128i samples_row40 = _mm_cvtepu8_epi16(LoadLo8(src));
    const __m128i samples_row41 = (max_luma_width == 16)
                                      ? _mm_cvtepu8_epi16(LoadLo8(src + 8))
                                      : LastRowSamples(samples_row40);
    src += stride;
    const __m128i samples_row50 = _mm_cvtepu8_epi16(LoadLo8(src));
    const __m128i samples_row51 = (max_luma_width == 16)
                                      ? _mm_cvtepu8_epi16(LoadLo8(src + 8))
                                      : LastRowSamples(samples_row50);
    src += stride;
    const __m128i luma_sum20 = _mm_add_epi16(samples_row40, samples_row50);
    const __m128i luma_sum21 = _mm_add_epi16(samples_row41, samples_row51);
    sum = _mm_add_epi16(
        sum, StoreLumaResults8_420(luma_sum20, luma_sum21, luma_ptr));
    luma_ptr += kCflLumaBufferStride;

    const __m128i samples_row60 = _mm_cvtepu8_epi16(LoadLo8(src));
    const __m128i samples_row61 = (max_luma_width == 16)
                                      ? _mm_cvtepu8_epi16(LoadLo8(src + 8))
                                      : LastRowSamples(samples_row60);
    src += stride;
    const __m128i samples_row70 = _mm_cvtepu8_epi16(LoadLo8(src));
    const __m128i samples_row71 = (max_luma_width == 16)
                                      ? _mm_cvtepu8_epi16(LoadLo8(src + 8))
                                      : LastRowSamples(samples_row70);
    src += stride;
    const __m128i luma_sum30 = _mm_add_epi16(samples_row60, samples_row70);
    const __m128i luma_sum31 = _mm_add_epi16(samples_row61, samples_row71);
    sum = _mm_add_epi16(
        sum, StoreLumaResults8_420(luma_sum30, luma_sum31, luma_ptr));
    luma_ptr += kCflLumaBufferStride;

    final_sum = _mm_add_epi32(final_sum, _mm_cvtepu16_epi32(sum));
    final_sum = _mm_add_epi32(final_sum, _mm_unpackhi_epi16(sum, zero));
    y += 4;
  } while (y < luma_height);
  // Duplicate the final row downward to the end after max_luma_height.
  const __m128i final_fill = LoadUnaligned16(luma_ptr - kCflLumaBufferStride);
  const __m128i final_fill_to_sum0 = _mm_cvtepi16_epi32(final_fill);
  const __m128i final_fill_to_sum1 =
      _mm_cvtepi16_epi32(_mm_srli_si128(final_fill, 8));
  const __m128i final_fill_to_sum =
      _mm_add_epi32(final_fill_to_sum0, final_fill_to_sum1);
  for (; y < block_height; ++y) {
    StoreUnaligned16(luma_ptr, final_fill);
    luma_ptr += kCflLumaBufferStride;

    final_sum = _mm_add_epi32(final_sum, final_fill_to_sum);
  }
  final_sum = _mm_add_epi32(final_sum, _mm_srli_si128(final_sum, 8));
  final_sum = _mm_add_epi32(final_sum, _mm_srli_si128(final_sum, 4));

  __m128i averages = RightShiftWithRounding_S32(
      final_sum, block_height_log2 + 3 /*log2 of width 8*/);

  averages = _mm_shufflelo_epi16(averages, 0);
  averages = _mm_shuffle_epi32(averages, 0);
  luma_ptr = luma[0];
  for (int y = 0; y < block_height; ++y, luma_ptr += kCflLumaBufferStride) {
    const __m128i samples = LoadUnaligned16(luma_ptr);
    StoreUnaligned16(luma_ptr, _mm_sub_epi16(samples, averages));
  }
}

template <int block_height_log2>
void CflSubsampler420_8xH_SSE4_1(
    int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int max_luma_width, const int max_luma_height,
    const void* const source, ptrdiff_t stride) {
  if (max_luma_width == 8) {
    CflSubsampler420Impl_8xH_SSE4_1<block_height_log2, 8>(
        luma, max_luma_width, max_luma_height, source, stride);
  } else {
    CflSubsampler420Impl_8xH_SSE4_1<block_height_log2, 16>(
        luma, max_luma_width, max_luma_height, source, stride);
  }
}

template <int block_width_log2, int block_height_log2, int max_luma_width>
inline void CflSubsampler420Impl_WxH_SSE4_1(
    int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int /*max_luma_width*/, const int max_luma_height,
    const void* const source, ptrdiff_t stride) {
  const auto* src = static_cast<const uint8_t*>(source);
  const __m128i zero = _mm_setzero_si128();
  __m128i final_sum = zero;
  const int block_height = 1 << block_height_log2;
  const int luma_height = std::min(block_height, max_luma_height >> 1);

  int16_t* luma_ptr = luma[0];
  __m128i final_row_result;
  // Begin first y section, covering width up to 16.
  int y = 0;
  do {
    const uint8_t* src_next = src + stride;
    const __m128i samples_row0_lo = LoadUnaligned16(src);
    const __m128i samples_row00 = _mm_cvtepu8_epi16(samples_row0_lo);
    const __m128i samples_row01 = (max_luma_width >= 16)
                                      ? _mm_unpackhi_epi8(samples_row0_lo, zero)
                                      : LastRowSamples(samples_row00);
    const __m128i samples_row0_hi = LoadUnaligned16(src + 16);
    const __m128i samples_row02 = (max_luma_width >= 24)
                                      ? _mm_cvtepu8_epi16(samples_row0_hi)
                                      : LastRowSamples(samples_row01);
    const __m128i samples_row03 = (max_luma_width == 32)
                                      ? _mm_unpackhi_epi8(samples_row0_hi, zero)
                                      : LastRowSamples(samples_row02);
    const __m128i samples_row1_lo = LoadUnaligned16(src_next);
    const __m128i samples_row10 = _mm_cvtepu8_epi16(samples_row1_lo);
    const __m128i samples_row11 = (max_luma_width >= 16)
                                      ? _mm_unpackhi_epi8(samples_row1_lo, zero)
                                      : LastRowSamples(samples_row10);
    const __m128i samples_row1_hi = LoadUnaligned16(src_next + 16);
    const __m128i samples_row12 = (max_luma_width >= 24)
                                      ? _mm_cvtepu8_epi16(samples_row1_hi)
                                      : LastRowSamples(samples_row11);
    const __m128i samples_row13 = (max_luma_width == 32)
                                      ? _mm_unpackhi_epi8(samples_row1_hi, zero)
                                      : LastRowSamples(samples_row12);
    const __m128i luma_sum0 = _mm_add_epi16(samples_row00, samples_row10);
    const __m128i luma_sum1 = _mm_add_epi16(samples_row01, samples_row11);
    const __m128i luma_sum2 = _mm_add_epi16(samples_row02, samples_row12);
    const __m128i luma_sum3 = _mm_add_epi16(samples_row03, samples_row13);
    __m128i sum = StoreLumaResults8_420(luma_sum0, luma_sum1, luma_ptr);
    final_row_result =
        StoreLumaResults8_420(luma_sum2, luma_sum3, luma_ptr + 8);
    sum = _mm_add_epi16(sum, final_row_result);
    final_sum = _mm_add_epi32(final_sum, _mm_cvtepu16_epi32(sum));
    final_sum = _mm_add_epi32(final_sum, _mm_unpackhi_epi16(sum, zero));
    src += stride << 1;
    luma_ptr += kCflLumaBufferStride;
  } while (++y < luma_height);

  // Because max_luma_width is at most 32, any values beyond x=16 will
  // necessarily be duplicated.
  if (block_width_log2 == 5) {
    const __m128i wide_fill = LastRowResult(final_row_result);
    // Multiply duplicated value by number of occurrences, height * 4, since
    // there are 16 in each row and the value appears in the vector 4 times.
    final_sum = _mm_add_epi32(
        final_sum,
        _mm_slli_epi32(_mm_cvtepi16_epi32(wide_fill), block_height_log2 + 2));
  }

  // Begin second y section.
  if (y < block_height) {
    const __m128i final_fill0 =
        LoadUnaligned16(luma_ptr - kCflLumaBufferStride);
    const __m128i final_fill1 =
        LoadUnaligned16(luma_ptr - kCflLumaBufferStride + 8);
    const __m128i final_inner_sum = _mm_add_epi16(final_fill0, final_fill1);
    const __m128i final_inner_sum0 = _mm_cvtepu16_epi32(final_inner_sum);
    const __m128i final_inner_sum1 = _mm_unpackhi_epi16(final_inner_sum, zero);
    const __m128i final_fill_to_sum =
        _mm_add_epi32(final_inner_sum0, final_inner_sum1);

    do {
      StoreUnaligned16(luma_ptr, final_fill0);
      StoreUnaligned16(luma_ptr + 8, final_fill1);
      luma_ptr += kCflLumaBufferStride;

      final_sum = _mm_add_epi32(final_sum, final_fill_to_sum);
    } while (++y < block_height);
  }  // End second y section.

  final_sum = _mm_add_epi32(final_sum, _mm_srli_si128(final_sum, 8));
  final_sum = _mm_add_epi32(final_sum, _mm_srli_si128(final_sum, 4));

  __m128i averages = RightShiftWithRounding_S32(
      final_sum, block_width_log2 + block_height_log2);
  averages = _mm_shufflelo_epi16(averages, 0);
  averages = _mm_shuffle_epi32(averages, 0);

  luma_ptr = luma[0];
  for (int y = 0; y < block_height; ++y, luma_ptr += kCflLumaBufferStride) {
    const __m128i samples0 = LoadUnaligned16(luma_ptr);
    StoreUnaligned16(luma_ptr, _mm_sub_epi16(samples0, averages));
    const __m128i samples1 = LoadUnaligned16(luma_ptr + 8);
    final_row_result = _mm_sub_epi16(samples1, averages);
    StoreUnaligned16(luma_ptr + 8, final_row_result);
  }
  if (block_width_log2 == 5) {
    int16_t* wide_luma_ptr = luma[0] + 16;
    const __m128i wide_fill = LastRowResult(final_row_result);
    for (int i = 0; i < block_height;
         ++i, wide_luma_ptr += kCflLumaBufferStride) {
      StoreUnaligned16(wide_luma_ptr, wide_fill);
      StoreUnaligned16(wide_luma_ptr + 8, wide_fill);
    }
  }
}

template <int block_width_log2, int block_height_log2>
void CflSubsampler420_WxH_SSE4_1(
    int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int max_luma_width, const int max_luma_height,
    const void* const source, ptrdiff_t stride) {
  switch (max_luma_width) {
    case 8:
      CflSubsampler420Impl_WxH_SSE4_1<block_width_log2, block_height_log2, 8>(
          luma, max_luma_width, max_luma_height, source, stride);
      return;
    case 16:
      CflSubsampler420Impl_WxH_SSE4_1<block_width_log2, block_height_log2, 16>(
          luma, max_luma_width, max_luma_height, source, stride);
      return;
    case 24:
      CflSubsampler420Impl_WxH_SSE4_1<block_width_log2, block_height_log2, 24>(
          luma, max_luma_width, max_luma_height, source, stride);
      return;
    default:
      assert(max_luma_width == 32);
      CflSubsampler420Impl_WxH_SSE4_1<block_width_log2, block_height_log2, 32>(
          luma, max_luma_width, max_luma_height, source, stride);
      return;
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x4_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize4x4][kSubsamplingType420] =
      CflSubsampler420_4xH_SSE4_1<2>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x8_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize4x8][kSubsamplingType420] =
      CflSubsampler420_4xH_SSE4_1<3>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x16_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize4x16][kSubsamplingType420] =
      CflSubsampler420_4xH_SSE4_1<4>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x4_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize8x4][kSubsamplingType420] =
      CflSubsampler420_8xH_SSE4_1<2>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x8_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize8x8][kSubsamplingType420] =
      CflSubsampler420_8xH_SSE4_1<3>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x16_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize8x16][kSubsamplingType420] =
      CflSubsampler420_8xH_SSE4_1<4>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x32_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize8x32][kSubsamplingType420] =
      CflSubsampler420_8xH_SSE4_1<5>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x4_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize16x4][kSubsamplingType420] =
      CflSubsampler420_WxH_SSE4_1<4, 2>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x8_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize16x8][kSubsamplingType420] =
      CflSubsampler420_WxH_SSE4_1<4, 3>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x16_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize16x16][kSubsamplingType420] =
      CflSubsampler420_WxH_SSE4_1<4, 4>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x32_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize16x32][kSubsamplingType420] =
      CflSubsampler420_WxH_SSE4_1<4, 5>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x8_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize32x8][kSubsamplingType420] =
      CflSubsampler420_WxH_SSE4_1<5, 3>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x16_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize32x16][kSubsamplingType420] =
      CflSubsampler420_WxH_SSE4_1<5, 4>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x32_CflSubsampler420)
  dsp->cfl_subsamplers[kTransformSize32x32][kSubsamplingType420] =
      CflSubsampler420_WxH_SSE4_1<5, 5>;
#endif

#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x4_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize4x4][kSubsamplingType444] =
      CflSubsampler444_4xH_SSE4_1<2>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x8_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize4x8][kSubsamplingType444] =
      CflSubsampler444_4xH_SSE4_1<3>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x16_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize4x16][kSubsamplingType444] =
      CflSubsampler444_4xH_SSE4_1<4>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x4_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize8x4][kSubsamplingType444] =
      CflSubsampler444_8xH_SSE4_1<2>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x8_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize8x8][kSubsamplingType444] =
      CflSubsampler444_8xH_SSE4_1<3>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x16_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize8x16][kSubsamplingType444] =
      CflSubsampler444_8xH_SSE4_1<4>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x32_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize8x32][kSubsamplingType444] =
      CflSubsampler444_8xH_SSE4_1<5>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x4_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize16x4][kSubsamplingType444] =
      CflSubsampler444_SSE4_1<4, 2>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x8_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize16x8][kSubsamplingType444] =
      CflSubsampler444_SSE4_1<4, 3>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x16_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize16x16][kSubsamplingType444] =
      CflSubsampler444_SSE4_1<4, 4>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x32_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize16x32][kSubsamplingType444] =
      CflSubsampler444_SSE4_1<4, 5>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x8_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize32x8][kSubsamplingType444] =
      CflSubsampler444_SSE4_1<5, 3>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x16_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize32x16][kSubsamplingType444] =
      CflSubsampler444_SSE4_1<5, 4>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x32_CflSubsampler444)
  dsp->cfl_subsamplers[kTransformSize32x32][kSubsamplingType444] =
      CflSubsampler444_SSE4_1<5, 5>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x4_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize4x4] = CflIntraPredictor_SSE4_1<4, 4>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x8_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize4x8] = CflIntraPredictor_SSE4_1<4, 8>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x16_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize4x16] =
      CflIntraPredictor_SSE4_1<4, 16>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x4_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize8x4] = CflIntraPredictor_SSE4_1<8, 4>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x8_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize8x8] = CflIntraPredictor_SSE4_1<8, 8>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x16_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize8x16] =
      CflIntraPredictor_SSE4_1<8, 16>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize8x32_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize8x32] =
      CflIntraPredictor_SSE4_1<8, 32>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x4_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize16x4] =
      CflIntraPredictor_SSE4_1<16, 4>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x8_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize16x8] =
      CflIntraPredictor_SSE4_1<16, 8>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x16_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize16x16] =
      CflIntraPredictor_SSE4_1<16, 16>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize16x32_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize16x32] =
      CflIntraPredictor_SSE4_1<16, 32>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x8_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize32x8] =
      CflIntraPredictor_SSE4_1<32, 8>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x16_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize32x16] =
      CflIntraPredictor_SSE4_1<32, 16>;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(TransformSize32x32_CflIntraPredictor)
  dsp->cfl_intra_predictors[kTransformSize32x32] =
      CflIntraPredictor_SSE4_1<32, 32>;
#endif
}

}  // namespace
}  // namespace low_bitdepth

void IntraPredCflInit_SSE4_1() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else  // !LIBGAV1_ENABLE_SSE4_1

namespace libgav1 {
namespace dsp {

void IntraPredCflInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1

#endif  // LIBGAV1_ENABLE_SSE4_1
