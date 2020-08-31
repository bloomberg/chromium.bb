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

#include "src/dsp/convolve.h"
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_SSE4_1
#include <smmintrin.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/dsp/x86/common_sse4.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

constexpr int kSubPixelMask = (1 << kSubPixelBits) - 1;
constexpr int kHorizontalOffset = 3;

// Multiply every entry in |src[]| by the corresponding entry in |taps[]| and
// sum. The filters in |taps[]| are pre-shifted by 1. This prevents the final
// sum from outranging int16_t.
template <int filter_index>
__m128i SumOnePassTaps(const __m128i* const src, const __m128i* const taps) {
  __m128i sum;
  if (filter_index < 2) {
    // 6 taps.
    const __m128i v_madd_21 = _mm_maddubs_epi16(src[0], taps[0]);  // k2k1
    const __m128i v_madd_43 = _mm_maddubs_epi16(src[1], taps[1]);  // k4k3
    const __m128i v_madd_65 = _mm_maddubs_epi16(src[2], taps[2]);  // k6k5
    sum = _mm_add_epi16(v_madd_21, v_madd_43);
    sum = _mm_add_epi16(sum, v_madd_65);
  } else if (filter_index == 2) {
    // 8 taps.
    const __m128i v_madd_10 = _mm_maddubs_epi16(src[0], taps[0]);  // k1k0
    const __m128i v_madd_32 = _mm_maddubs_epi16(src[1], taps[1]);  // k3k2
    const __m128i v_madd_54 = _mm_maddubs_epi16(src[2], taps[2]);  // k5k4
    const __m128i v_madd_76 = _mm_maddubs_epi16(src[3], taps[3]);  // k7k6
    const __m128i v_sum_3210 = _mm_add_epi16(v_madd_10, v_madd_32);
    const __m128i v_sum_7654 = _mm_add_epi16(v_madd_54, v_madd_76);
    sum = _mm_add_epi16(v_sum_7654, v_sum_3210);
  } else if (filter_index == 3) {
    // 2 taps.
    sum = _mm_maddubs_epi16(src[0], taps[0]);  // k4k3
  } else {
    // 4 taps.
    const __m128i v_madd_32 = _mm_maddubs_epi16(src[0], taps[0]);  // k3k2
    const __m128i v_madd_54 = _mm_maddubs_epi16(src[1], taps[1]);  // k5k4
    sum = _mm_add_epi16(v_madd_32, v_madd_54);
  }
  return sum;
}

template <int filter_index>
__m128i SumHorizontalTaps(const uint8_t* const src,
                          const __m128i* const v_tap) {
  __m128i v_src[4];
  const __m128i src_long = LoadUnaligned16(src);
  const __m128i src_long_dup_lo = _mm_unpacklo_epi8(src_long, src_long);
  const __m128i src_long_dup_hi = _mm_unpackhi_epi8(src_long, src_long);

  if (filter_index < 2) {
    // 6 taps.
    v_src[0] = _mm_alignr_epi8(src_long_dup_hi, src_long_dup_lo, 3);   // _21
    v_src[1] = _mm_alignr_epi8(src_long_dup_hi, src_long_dup_lo, 7);   // _43
    v_src[2] = _mm_alignr_epi8(src_long_dup_hi, src_long_dup_lo, 11);  // _65
  } else if (filter_index == 2) {
    // 8 taps.
    v_src[0] = _mm_alignr_epi8(src_long_dup_hi, src_long_dup_lo, 1);   // _10
    v_src[1] = _mm_alignr_epi8(src_long_dup_hi, src_long_dup_lo, 5);   // _32
    v_src[2] = _mm_alignr_epi8(src_long_dup_hi, src_long_dup_lo, 9);   // _54
    v_src[3] = _mm_alignr_epi8(src_long_dup_hi, src_long_dup_lo, 13);  // _76
  } else if (filter_index == 3) {
    // 2 taps.
    v_src[0] = _mm_alignr_epi8(src_long_dup_hi, src_long_dup_lo, 7);  // _43
  } else if (filter_index > 3) {
    // 4 taps.
    v_src[0] = _mm_alignr_epi8(src_long_dup_hi, src_long_dup_lo, 5);  // _32
    v_src[1] = _mm_alignr_epi8(src_long_dup_hi, src_long_dup_lo, 9);  // _54
  }
  const __m128i sum = SumOnePassTaps<filter_index>(v_src, v_tap);
  return sum;
}

template <int filter_index>
__m128i SimpleHorizontalTaps(const uint8_t* const src,
                             const __m128i* const v_tap) {
  __m128i sum = SumHorizontalTaps<filter_index>(src, v_tap);

  // Normally the Horizontal pass does the downshift in two passes:
  // kInterRoundBitsHorizontal - 1 and then (kFilterBits -
  // kInterRoundBitsHorizontal). Each one uses a rounding shift. Combining them
  // requires adding the rounding offset from the skipped shift.
  constexpr int first_shift_rounding_bit = 1 << (kInterRoundBitsHorizontal - 2);

  sum = _mm_add_epi16(sum, _mm_set1_epi16(first_shift_rounding_bit));
  sum = RightShiftWithRounding_S16(sum, kFilterBits - 1);
  return _mm_packus_epi16(sum, sum);
}

template <int filter_index>
__m128i HorizontalTaps8To16(const uint8_t* const src,
                            const __m128i* const v_tap) {
  const __m128i sum = SumHorizontalTaps<filter_index>(src, v_tap);

  return RightShiftWithRounding_S16(sum, kInterRoundBitsHorizontal - 1);
}

template <int filter_index>
__m128i SumHorizontalTaps2x2(const uint8_t* src, const ptrdiff_t src_stride,
                             const __m128i* const v_tap) {
  const __m128i input0 = LoadLo8(&src[2]);
  const __m128i input1 = LoadLo8(&src[2 + src_stride]);

  if (filter_index == 3) {
    // 03 04 04 05 05 06 06 07 ....
    const __m128i input0_dup =
        _mm_srli_si128(_mm_unpacklo_epi8(input0, input0), 3);
    // 13 14 14 15 15 16 16 17 ....
    const __m128i input1_dup =
        _mm_srli_si128(_mm_unpacklo_epi8(input1, input1), 3);
    const __m128i v_src_43 = _mm_unpacklo_epi64(input0_dup, input1_dup);
    const __m128i v_sum_43 = _mm_maddubs_epi16(v_src_43, v_tap[0]);  // k4k3
    return v_sum_43;
  }

  // 02 03 03 04 04 05 05 06 06 07 ....
  const __m128i input0_dup =
      _mm_srli_si128(_mm_unpacklo_epi8(input0, input0), 1);
  // 12 13 13 14 14 15 15 16 16 17 ....
  const __m128i input1_dup =
      _mm_srli_si128(_mm_unpacklo_epi8(input1, input1), 1);
  // 04 05 05 06 06 07 07 08 ...
  const __m128i input0_dup_54 = _mm_srli_si128(input0_dup, 4);
  // 14 15 15 16 16 17 17 18 ...
  const __m128i input1_dup_54 = _mm_srli_si128(input1_dup, 4);
  const __m128i v_src_32 = _mm_unpacklo_epi64(input0_dup, input1_dup);
  const __m128i v_src_54 = _mm_unpacklo_epi64(input0_dup_54, input1_dup_54);
  const __m128i v_madd_32 = _mm_maddubs_epi16(v_src_32, v_tap[0]);  // k3k2
  const __m128i v_madd_54 = _mm_maddubs_epi16(v_src_54, v_tap[1]);  // k5k4
  const __m128i v_sum_5432 = _mm_add_epi16(v_madd_54, v_madd_32);
  return v_sum_5432;
}

template <int filter_index>
__m128i SimpleHorizontalTaps2x2(const uint8_t* src, const ptrdiff_t src_stride,
                                const __m128i* const v_tap) {
  __m128i sum = SumHorizontalTaps2x2<filter_index>(src, src_stride, v_tap);

  // Normally the Horizontal pass does the downshift in two passes:
  // kInterRoundBitsHorizontal - 1 and then (kFilterBits -
  // kInterRoundBitsHorizontal). Each one uses a rounding shift. Combining them
  // requires adding the rounding offset from the skipped shift.
  constexpr int first_shift_rounding_bit = 1 << (kInterRoundBitsHorizontal - 2);

  sum = _mm_add_epi16(sum, _mm_set1_epi16(first_shift_rounding_bit));
  sum = RightShiftWithRounding_S16(sum, kFilterBits - 1);
  return _mm_packus_epi16(sum, sum);
}

template <int num_taps, int step, int filter_index, bool is_2d = false,
          bool is_compound = false>
void FilterHorizontal(const uint8_t* src, const ptrdiff_t src_stride,
                      void* const dest, const ptrdiff_t pred_stride,
                      const int width, const int height,
                      const __m128i* const v_tap) {
  auto* dest8 = static_cast<uint8_t*>(dest);
  auto* dest16 = static_cast<uint16_t*>(dest);

  // 4 tap filters are never used when width > 4.
  if (num_taps != 4 && width > 4) {
    int y = 0;
    do {
      int x = 0;
      do {
        if (is_2d || is_compound) {
          const __m128i v_sum =
              HorizontalTaps8To16<filter_index>(&src[x], v_tap);
          StoreUnaligned16(&dest16[x], v_sum);
        } else {
          const __m128i result =
              SimpleHorizontalTaps<filter_index>(&src[x], v_tap);
          StoreLo8(&dest8[x], result);
        }
        x += step;
      } while (x < width);
      src += src_stride;
      dest8 += pred_stride;
      dest16 += pred_stride;
    } while (++y < height);
    return;
  }

  // Horizontal passes only needs to account for |num_taps| 2 and 4 when
  // |width| <= 4.
  assert(width <= 4);
  assert(num_taps <= 4);
  if (num_taps <= 4) {
    if (width == 4) {
      int y = 0;
      do {
        if (is_2d || is_compound) {
          const __m128i v_sum = HorizontalTaps8To16<filter_index>(src, v_tap);
          StoreLo8(dest16, v_sum);
        } else {
          const __m128i result = SimpleHorizontalTaps<filter_index>(src, v_tap);
          Store4(&dest8[0], result);
        }
        src += src_stride;
        dest8 += pred_stride;
        dest16 += pred_stride;
      } while (++y < height);
      return;
    }

    if (!is_compound) {
      int y = 0;
      do {
        if (is_2d) {
          // TODO(slavarnway): Add 2d support
        } else {
          const __m128i sum =
              SimpleHorizontalTaps2x2<filter_index>(src, src_stride, v_tap);
          Store2(dest8, sum);
          dest8 += pred_stride;
          Store2(dest8, _mm_srli_si128(sum, 4));
          dest8 += pred_stride;
        }

        src += src_stride << 1;
        y += 2;
      } while (y < height - 1);

      // The 2d filters have an odd |height| because the horizontal pass
      // generates context for the vertical pass.
      if (is_2d) {
        assert(height % 2 == 1);
        // TODO(slavarnway): Add 2d support
      }
    }
  }
}

template <int num_taps>
LIBGAV1_ALWAYS_INLINE void SetupTaps(const __m128i* const filter,
                                     __m128i* v_tap) {
  if (num_taps == 8) {
    v_tap[0] = _mm_shufflelo_epi16(*filter, 0x0);   // k1k0
    v_tap[1] = _mm_shufflelo_epi16(*filter, 0x55);  // k3k2
    v_tap[2] = _mm_shufflelo_epi16(*filter, 0xaa);  // k5k4
    v_tap[3] = _mm_shufflelo_epi16(*filter, 0xff);  // k7k6
    v_tap[0] = _mm_unpacklo_epi64(v_tap[0], v_tap[0]);
    v_tap[1] = _mm_unpacklo_epi64(v_tap[1], v_tap[1]);
    v_tap[2] = _mm_unpacklo_epi64(v_tap[2], v_tap[2]);
    v_tap[3] = _mm_unpacklo_epi64(v_tap[3], v_tap[3]);
  } else if (num_taps == 6) {
    const __m128i adjusted_filter = _mm_srli_si128(*filter, 1);
    v_tap[0] = _mm_shufflelo_epi16(adjusted_filter, 0x0);   // k2k1
    v_tap[1] = _mm_shufflelo_epi16(adjusted_filter, 0x55);  // k4k3
    v_tap[2] = _mm_shufflelo_epi16(adjusted_filter, 0xaa);  // k6k5
    v_tap[0] = _mm_unpacklo_epi64(v_tap[0], v_tap[0]);
    v_tap[1] = _mm_unpacklo_epi64(v_tap[1], v_tap[1]);
    v_tap[2] = _mm_unpacklo_epi64(v_tap[2], v_tap[2]);
  } else if (num_taps == 4) {
    v_tap[0] = _mm_shufflelo_epi16(*filter, 0x55);  // k3k2
    v_tap[1] = _mm_shufflelo_epi16(*filter, 0xaa);  // k5k4
    v_tap[0] = _mm_unpacklo_epi64(v_tap[0], v_tap[0]);
    v_tap[1] = _mm_unpacklo_epi64(v_tap[1], v_tap[1]);
  } else {  // num_taps == 2
    const __m128i adjusted_filter = _mm_srli_si128(*filter, 1);
    v_tap[0] = _mm_shufflelo_epi16(adjusted_filter, 0x55);  // k4k3
    v_tap[0] = _mm_unpacklo_epi64(v_tap[0], v_tap[0]);
  }
}

template <bool is_2d = false, bool is_compound = false>
LIBGAV1_ALWAYS_INLINE void DoHorizontalPass(
    const uint8_t* const src, const ptrdiff_t src_stride, void* const dst,
    const ptrdiff_t dst_stride, const int width, const int height,
    const int subpixel, const int filter_index) {
  const int filter_id = (subpixel >> 6) & kSubPixelMask;
  assert(filter_id != 0);
  __m128i v_tap[4];
  const __m128i v_horizontal_filter =
      LoadLo8(kHalfSubPixelFilters[filter_index][filter_id]);

  if (filter_index == 2) {  // 8 tap.
    SetupTaps<8>(&v_horizontal_filter, v_tap);
    FilterHorizontal<8, 8, 2, is_2d, is_compound>(
        src, src_stride, dst, dst_stride, width, height, v_tap);
  } else if (filter_index == 1) {  // 6 tap.
    SetupTaps<6>(&v_horizontal_filter, v_tap);
    FilterHorizontal<6, 8, 1, is_2d, is_compound>(
        src, src_stride, dst, dst_stride, width, height, v_tap);
  } else if (filter_index == 0) {  // 6 tap.
    SetupTaps<6>(&v_horizontal_filter, v_tap);
    FilterHorizontal<6, 8, 0, is_2d, is_compound>(
        src, src_stride, dst, dst_stride, width, height, v_tap);
  } else if (filter_index == 4) {  // 4 tap.
    SetupTaps<4>(&v_horizontal_filter, v_tap);
    FilterHorizontal<4, 8, 4, is_2d, is_compound>(
        src, src_stride, dst, dst_stride, width, height, v_tap);
  } else if (filter_index == 5) {  // 4 tap.
    SetupTaps<4>(&v_horizontal_filter, v_tap);
    FilterHorizontal<4, 8, 5, is_2d, is_compound>(
        src, src_stride, dst, dst_stride, width, height, v_tap);
  } else {  // 2 tap.
    SetupTaps<2>(&v_horizontal_filter, v_tap);
    FilterHorizontal<2, 8, 3, is_2d, is_compound>(
        src, src_stride, dst, dst_stride, width, height, v_tap);
  }
}

void ConvolveCompoundCopy_SSE4(
    const void* const reference, const ptrdiff_t reference_stride,
    const int /*horizontal_filter_index*/, const int /*vertical_filter_index*/,
    const int /*subpixel_x*/, const int /*subpixel_y*/, const int width,
    const int height, void* prediction, const ptrdiff_t pred_stride) {
  const auto* src = static_cast<const uint8_t*>(reference);
  const ptrdiff_t src_stride = reference_stride;
  auto* dest = static_cast<uint16_t*>(prediction);
  constexpr int kRoundBitsVertical =
      kInterRoundBitsVertical - kInterRoundBitsCompoundVertical;
  if (width >= 16) {
    int y = height;
    do {
      int x = 0;
      do {
        const __m128i v_src = LoadUnaligned16(&src[x]);
        const __m128i v_src_ext_lo = _mm_cvtepu8_epi16(v_src);
        const __m128i v_src_ext_hi =
            _mm_cvtepu8_epi16(_mm_srli_si128(v_src, 8));
        const __m128i v_dest_lo =
            _mm_slli_epi16(v_src_ext_lo, kRoundBitsVertical);
        const __m128i v_dest_hi =
            _mm_slli_epi16(v_src_ext_hi, kRoundBitsVertical);
        // TODO(slavarnway): Investigate using aligned stores.
        StoreUnaligned16(&dest[x], v_dest_lo);
        StoreUnaligned16(&dest[x + 8], v_dest_hi);
        x += 16;
      } while (x < width);
      src += src_stride;
      dest += pred_stride;
    } while (--y != 0);
  } else if (width == 8) {
    int y = height;
    do {
      const __m128i v_src = LoadLo8(&src[0]);
      const __m128i v_src_ext = _mm_cvtepu8_epi16(v_src);
      const __m128i v_dest = _mm_slli_epi16(v_src_ext, kRoundBitsVertical);
      StoreUnaligned16(&dest[0], v_dest);
      src += src_stride;
      dest += pred_stride;
    } while (--y != 0);
  } else { /* width == 4 */
    int y = height;
    do {
      const __m128i v_src0 = Load4(&src[0]);
      const __m128i v_src1 = Load4(&src[src_stride]);
      const __m128i v_src = _mm_unpacklo_epi32(v_src0, v_src1);
      const __m128i v_src_ext = _mm_cvtepu8_epi16(v_src);
      const __m128i v_dest = _mm_slli_epi16(v_src_ext, kRoundBitsVertical);
      StoreLo8(&dest[0], v_dest);
      StoreHi8(&dest[pred_stride], v_dest);
      src += src_stride * 2;
      dest += pred_stride * 2;
      y -= 2;
    } while (y != 0);
  }
}

void ConvolveHorizontal_SSE4_1(const void* const reference,
                               const ptrdiff_t reference_stride,
                               const int horizontal_filter_index,
                               const int /*vertical_filter_index*/,
                               const int subpixel_x, const int /*subpixel_y*/,
                               const int width, const int height,
                               void* prediction, const ptrdiff_t pred_stride) {
  const int filter_index = GetFilterIndex(horizontal_filter_index, width);
  // Set |src| to the outermost tap.
  const auto* src = static_cast<const uint8_t*>(reference) - kHorizontalOffset;
  auto* dest = static_cast<uint8_t*>(prediction);

  DoHorizontalPass(src, reference_stride, dest, pred_stride, width, height,
                   subpixel_x, filter_index);
}

void ConvolveCompoundHorizontal_SSE4_1(
    const void* const reference, const ptrdiff_t reference_stride,
    const int horizontal_filter_index, const int /*vertical_filter_index*/,
    const int subpixel_x, const int /*subpixel_y*/, const int width,
    const int height, void* prediction, const ptrdiff_t /*pred_stride*/) {
  const int filter_index = GetFilterIndex(horizontal_filter_index, width);
  const auto* src = static_cast<const uint8_t*>(reference) - kHorizontalOffset;
  auto* dest = static_cast<uint16_t*>(prediction);

  DoHorizontalPass</*is_2d=*/false, /*is_compound=*/true>(
      src, reference_stride, dest, width, width, height, subpixel_x,
      filter_index);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
  dsp->convolve[0][0][0][1] = ConvolveHorizontal_SSE4_1;

  dsp->convolve[0][1][0][0] = ConvolveCompoundCopy_SSE4;
  dsp->convolve[0][1][0][1] = ConvolveCompoundHorizontal_SSE4_1;
}

}  // namespace
}  // namespace low_bitdepth

void ConvolveInit_SSE4_1() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_SSE4_1
namespace libgav1 {
namespace dsp {

void ConvolveInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
