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
#include "src/utils/constants.h"
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

// TODO(slavarnway): Move to common neon/sse4 file.
int GetNumTapsInFilter(const int filter_index) {
  if (filter_index < 2) {
    // Despite the names these only use 6 taps.
    // kInterpolationFilterEightTap
    // kInterpolationFilterEightTapSmooth
    return 6;
  }

  if (filter_index == 2) {
    // kInterpolationFilterEightTapSharp
    return 8;
  }

  if (filter_index == 3) {
    // kInterpolationFilterBilinear
    return 2;
  }

  assert(filter_index > 3);
  // For small sizes (width/height <= 4) the large filters are replaced with 4
  // tap options.
  // If the original filters were |kInterpolationFilterEightTap| or
  // |kInterpolationFilterEightTapSharp| then it becomes
  // |kInterpolationFilterSwitchable|.
  // If it was |kInterpolationFilterEightTapSmooth| then it becomes an unnamed 4
  // tap filter.
  return 4;
}

constexpr int kIntermediateStride = kMaxSuperBlockSizeInPixels;
constexpr int kHorizontalOffset = 3;
constexpr int kFilterIndexShift = 6;

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

template <int filter_index>
__m128i HorizontalTaps8To16_2x2(const uint8_t* src, const ptrdiff_t src_stride,
                                const __m128i* const v_tap) {
  const __m128i sum =
      SumHorizontalTaps2x2<filter_index>(src, src_stride, v_tap);

  return RightShiftWithRounding_S16(sum, kInterRoundBitsHorizontal - 1);
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
          if (is_2d) {
            StoreAligned16(&dest16[x], v_sum);
          } else {
            StoreUnaligned16(&dest16[x], v_sum);
          }
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
          const __m128i sum =
              HorizontalTaps8To16_2x2<filter_index>(src, src_stride, v_tap);
          Store4(&dest16[0], sum);
          dest16 += pred_stride;
          Store4(&dest16[0], _mm_srli_si128(sum, 8));
          dest16 += pred_stride;
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
        __m128i sum;
        const __m128i input = LoadLo8(&src[2]);
        if (filter_index == 3) {
          // 03 04 04 05 05 06 06 07 ....
          const __m128i v_src_43 =
              _mm_srli_si128(_mm_unpacklo_epi8(input, input), 3);
          sum = _mm_maddubs_epi16(v_src_43, v_tap[0]);  // k4k3
        } else {
          // 02 03 03 04 04 05 05 06 06 07 ....
          const __m128i v_src_32 =
              _mm_srli_si128(_mm_unpacklo_epi8(input, input), 1);
          // 04 05 05 06 06 07 07 08 ...
          const __m128i v_src_54 = _mm_srli_si128(v_src_32, 4);
          const __m128i v_madd_32 =
              _mm_maddubs_epi16(v_src_32, v_tap[0]);  // k3k2
          const __m128i v_madd_54 =
              _mm_maddubs_epi16(v_src_54, v_tap[1]);  // k5k4
          sum = _mm_add_epi16(v_madd_54, v_madd_32);
        }
        sum = RightShiftWithRounding_S16(sum, kInterRoundBitsHorizontal - 1);
        Store4(dest16, sum);
      }
    }
  }
}

template <int num_taps, bool is_2d_vertical = false>
LIBGAV1_ALWAYS_INLINE void SetupTaps(const __m128i* const filter,
                                     __m128i* v_tap) {
  if (num_taps == 8) {
    v_tap[0] = _mm_shufflelo_epi16(*filter, 0x0);   // k1k0
    v_tap[1] = _mm_shufflelo_epi16(*filter, 0x55);  // k3k2
    v_tap[2] = _mm_shufflelo_epi16(*filter, 0xaa);  // k5k4
    v_tap[3] = _mm_shufflelo_epi16(*filter, 0xff);  // k7k6
    if (is_2d_vertical) {
      v_tap[0] = _mm_cvtepi8_epi16(v_tap[0]);
      v_tap[1] = _mm_cvtepi8_epi16(v_tap[1]);
      v_tap[2] = _mm_cvtepi8_epi16(v_tap[2]);
      v_tap[3] = _mm_cvtepi8_epi16(v_tap[3]);
    } else {
      v_tap[0] = _mm_unpacklo_epi64(v_tap[0], v_tap[0]);
      v_tap[1] = _mm_unpacklo_epi64(v_tap[1], v_tap[1]);
      v_tap[2] = _mm_unpacklo_epi64(v_tap[2], v_tap[2]);
      v_tap[3] = _mm_unpacklo_epi64(v_tap[3], v_tap[3]);
    }
  } else if (num_taps == 6) {
    const __m128i adjusted_filter = _mm_srli_si128(*filter, 1);
    v_tap[0] = _mm_shufflelo_epi16(adjusted_filter, 0x0);   // k2k1
    v_tap[1] = _mm_shufflelo_epi16(adjusted_filter, 0x55);  // k4k3
    v_tap[2] = _mm_shufflelo_epi16(adjusted_filter, 0xaa);  // k6k5
    if (is_2d_vertical) {
      v_tap[0] = _mm_cvtepi8_epi16(v_tap[0]);
      v_tap[1] = _mm_cvtepi8_epi16(v_tap[1]);
      v_tap[2] = _mm_cvtepi8_epi16(v_tap[2]);
    } else {
      v_tap[0] = _mm_unpacklo_epi64(v_tap[0], v_tap[0]);
      v_tap[1] = _mm_unpacklo_epi64(v_tap[1], v_tap[1]);
      v_tap[2] = _mm_unpacklo_epi64(v_tap[2], v_tap[2]);
    }
  } else if (num_taps == 4) {
    v_tap[0] = _mm_shufflelo_epi16(*filter, 0x55);  // k3k2
    v_tap[1] = _mm_shufflelo_epi16(*filter, 0xaa);  // k5k4
    if (is_2d_vertical) {
      v_tap[0] = _mm_cvtepi8_epi16(v_tap[0]);
      v_tap[1] = _mm_cvtepi8_epi16(v_tap[1]);
    } else {
      v_tap[0] = _mm_unpacklo_epi64(v_tap[0], v_tap[0]);
      v_tap[1] = _mm_unpacklo_epi64(v_tap[1], v_tap[1]);
    }
  } else {  // num_taps == 2
    const __m128i adjusted_filter = _mm_srli_si128(*filter, 1);
    v_tap[0] = _mm_shufflelo_epi16(adjusted_filter, 0x55);  // k4k3
    if (is_2d_vertical) {
      v_tap[0] = _mm_cvtepi8_epi16(v_tap[0]);
    } else {
      v_tap[0] = _mm_unpacklo_epi64(v_tap[0], v_tap[0]);
    }
  }
}

template <int num_taps, bool is_compound>
__m128i SimpleSum2DVerticalTaps(const __m128i* const src,
                                const __m128i* const taps) {
  __m128i sum_lo = _mm_madd_epi16(_mm_unpacklo_epi16(src[0], src[1]), taps[0]);
  __m128i sum_hi = _mm_madd_epi16(_mm_unpackhi_epi16(src[0], src[1]), taps[0]);
  if (num_taps >= 4) {
    __m128i madd_lo =
        _mm_madd_epi16(_mm_unpacklo_epi16(src[2], src[3]), taps[1]);
    __m128i madd_hi =
        _mm_madd_epi16(_mm_unpackhi_epi16(src[2], src[3]), taps[1]);
    sum_lo = _mm_add_epi32(sum_lo, madd_lo);
    sum_hi = _mm_add_epi32(sum_hi, madd_hi);
    if (num_taps >= 6) {
      madd_lo = _mm_madd_epi16(_mm_unpacklo_epi16(src[4], src[5]), taps[2]);
      madd_hi = _mm_madd_epi16(_mm_unpackhi_epi16(src[4], src[5]), taps[2]);
      sum_lo = _mm_add_epi32(sum_lo, madd_lo);
      sum_hi = _mm_add_epi32(sum_hi, madd_hi);
      if (num_taps == 8) {
        madd_lo = _mm_madd_epi16(_mm_unpacklo_epi16(src[6], src[7]), taps[3]);
        madd_hi = _mm_madd_epi16(_mm_unpackhi_epi16(src[6], src[7]), taps[3]);
        sum_lo = _mm_add_epi32(sum_lo, madd_lo);
        sum_hi = _mm_add_epi32(sum_hi, madd_hi);
      }
    }
  }

  if (is_compound) {
    return _mm_packs_epi32(
        RightShiftWithRounding_S32(sum_lo, kInterRoundBitsCompoundVertical - 1),
        RightShiftWithRounding_S32(sum_hi,
                                   kInterRoundBitsCompoundVertical - 1));
  }

  return _mm_packs_epi32(
      RightShiftWithRounding_S32(sum_lo, kInterRoundBitsVertical - 1),
      RightShiftWithRounding_S32(sum_hi, kInterRoundBitsVertical - 1));
}

template <int num_taps, bool is_compound = false>
void Filter2DVertical(const uint16_t* src, void* const dst,
                      const ptrdiff_t dst_stride, const int width,
                      const int height, const __m128i* const taps) {
  assert(width >= 8);
  constexpr int next_row = num_taps - 1;
  // The Horizontal pass uses |width| as |stride| for the intermediate buffer.
  const ptrdiff_t src_stride = width;

  auto* dst8 = static_cast<uint8_t*>(dst);
  auto* dst16 = static_cast<uint16_t*>(dst);

  int x = 0;
  do {
    __m128i srcs[8];
    const uint16_t* src_x = src + x;
    srcs[0] = LoadAligned16(src_x);
    src_x += src_stride;
    if (num_taps >= 4) {
      srcs[1] = LoadAligned16(src_x);
      src_x += src_stride;
      srcs[2] = LoadAligned16(src_x);
      src_x += src_stride;
      if (num_taps >= 6) {
        srcs[3] = LoadAligned16(src_x);
        src_x += src_stride;
        srcs[4] = LoadAligned16(src_x);
        src_x += src_stride;
        if (num_taps == 8) {
          srcs[5] = LoadAligned16(src_x);
          src_x += src_stride;
          srcs[6] = LoadAligned16(src_x);
          src_x += src_stride;
        }
      }
    }

    int y = 0;
    do {
      srcs[next_row] = LoadAligned16(src_x);
      src_x += src_stride;

      const __m128i sum =
          SimpleSum2DVerticalTaps<num_taps, is_compound>(srcs, taps);
      if (is_compound) {
        StoreUnaligned16(dst16 + x + y * dst_stride, sum);
      } else {
        StoreLo8(dst8 + x + y * dst_stride, _mm_packus_epi16(sum, sum));
      }

      srcs[0] = srcs[1];
      if (num_taps >= 4) {
        srcs[1] = srcs[2];
        srcs[2] = srcs[3];
        if (num_taps >= 6) {
          srcs[3] = srcs[4];
          srcs[4] = srcs[5];
          if (num_taps == 8) {
            srcs[5] = srcs[6];
            srcs[6] = srcs[7];
          }
        }
      }
    } while (++y < height);
    x += 8;
  } while (x < width);
}

// Take advantage of |src_stride| == |width| to process two rows at a time.
template <int num_taps, bool is_compound = false>
void Filter2DVertical4xH(const uint16_t* src, void* const dst,
                         const ptrdiff_t dst_stride, const int height,
                         const __m128i* const taps) {
  auto* dst8 = static_cast<uint8_t*>(dst);
  auto* dst16 = static_cast<uint16_t*>(dst);

  __m128i srcs[9];
  srcs[0] = LoadAligned16(src);
  src += 8;
  if (num_taps >= 4) {
    srcs[2] = LoadAligned16(src);
    src += 8;
    srcs[1] = _mm_unpacklo_epi64(_mm_srli_si128(srcs[0], 8), srcs[2]);
    if (num_taps >= 6) {
      srcs[4] = LoadAligned16(src);
      src += 8;
      srcs[3] = _mm_unpacklo_epi64(_mm_srli_si128(srcs[2], 8), srcs[4]);
      if (num_taps == 8) {
        srcs[6] = LoadAligned16(src);
        src += 8;
        srcs[5] = _mm_unpacklo_epi64(_mm_srli_si128(srcs[4], 8), srcs[6]);
      }
    }
  }

  int y = 0;
  do {
    srcs[num_taps] = LoadAligned16(src);
    src += 8;
    srcs[num_taps - 1] = _mm_unpacklo_epi64(
        _mm_srli_si128(srcs[num_taps - 2], 8), srcs[num_taps]);

    const __m128i sum =
        SimpleSum2DVerticalTaps<num_taps, is_compound>(srcs, taps);
    if (is_compound) {
      StoreUnaligned16(dst16, sum);
      dst16 += 4 << 1;
    } else {
      const __m128i results = _mm_packus_epi16(sum, sum);
      Store4(dst8, results);
      dst8 += dst_stride;
      Store4(dst8, _mm_srli_si128(results, 4));
      dst8 += dst_stride;
    }

    srcs[0] = srcs[2];
    if (num_taps >= 4) {
      srcs[1] = srcs[3];
      srcs[2] = srcs[4];
      if (num_taps >= 6) {
        srcs[3] = srcs[5];
        srcs[4] = srcs[6];
        if (num_taps == 8) {
          srcs[5] = srcs[7];
          srcs[6] = srcs[8];
        }
      }
    }
    y += 2;
  } while (y < height);
}

// Take advantage of |src_stride| == |width| to process four rows at a time.
template <int num_taps>
void Filter2DVertical2xH(const uint16_t* src, void* const dst,
                         const ptrdiff_t dst_stride, const int height,
                         const __m128i* const taps) {
  constexpr int next_row = (num_taps < 6) ? 4 : 8;

  auto* dst8 = static_cast<uint8_t*>(dst);

  __m128i srcs[9];
  srcs[0] = LoadAligned16(src);
  src += 8;
  if (num_taps >= 6) {
    srcs[4] = LoadAligned16(src);
    src += 8;
    srcs[1] = _mm_alignr_epi8(srcs[4], srcs[0], 4);
    if (num_taps == 8) {
      srcs[2] = _mm_alignr_epi8(srcs[4], srcs[0], 8);
      srcs[3] = _mm_alignr_epi8(srcs[4], srcs[0], 12);
    }
  }

  int y = 0;
  do {
    srcs[next_row] = LoadAligned16(src);
    src += 8;
    if (num_taps == 2) {
      srcs[1] = _mm_alignr_epi8(srcs[4], srcs[0], 4);
    } else if (num_taps == 4) {
      srcs[1] = _mm_alignr_epi8(srcs[4], srcs[0], 4);
      srcs[2] = _mm_alignr_epi8(srcs[4], srcs[0], 8);
      srcs[3] = _mm_alignr_epi8(srcs[4], srcs[0], 12);
    } else if (num_taps == 6) {
      srcs[2] = _mm_alignr_epi8(srcs[4], srcs[0], 8);
      srcs[3] = _mm_alignr_epi8(srcs[4], srcs[0], 12);
      srcs[5] = _mm_alignr_epi8(srcs[8], srcs[4], 4);
    } else if (num_taps == 8) {
      srcs[5] = _mm_alignr_epi8(srcs[8], srcs[4], 4);
      srcs[6] = _mm_alignr_epi8(srcs[8], srcs[4], 8);
      srcs[7] = _mm_alignr_epi8(srcs[8], srcs[4], 12);
    }

    const __m128i sum =
        SimpleSum2DVerticalTaps<num_taps, /*is_compound=*/false>(srcs, taps);
    const __m128i results = _mm_packus_epi16(sum, sum);

    Store2(dst8, results);
    dst8 += dst_stride;
    Store2(dst8, _mm_srli_si128(results, 2));
    // When |height| <= 4 the taps are restricted to 2 and 4 tap variants.
    // Therefore we don't need to check this condition when |height| > 4.
    if (num_taps <= 4 && height == 2) return;
    dst8 += dst_stride;
    Store2(dst8, _mm_srli_si128(results, 4));
    dst8 += dst_stride;
    Store2(dst8, _mm_srli_si128(results, 6));
    dst8 += dst_stride;

    srcs[0] = srcs[4];
    if (num_taps == 6) {
      srcs[1] = srcs[5];
      srcs[4] = srcs[8];
    } else if (num_taps == 8) {
      srcs[1] = srcs[5];
      srcs[2] = srcs[6];
      srcs[3] = srcs[7];
      srcs[4] = srcs[8];
    }

    y += 4;
  } while (y < height);
}

template <bool is_2d = false, bool is_compound = false>
LIBGAV1_ALWAYS_INLINE void DoHorizontalPass(
    const uint8_t* const src, const ptrdiff_t src_stride, void* const dst,
    const ptrdiff_t dst_stride, const int width, const int height,
    const int filter_id, const int filter_index) {
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

void Convolve2D_SSE4_1(const void* const reference,
                       const ptrdiff_t reference_stride,
                       const int horizontal_filter_index,
                       const int vertical_filter_index,
                       const int horizontal_filter_id,
                       const int vertical_filter_id, const int width,
                       const int height, void* prediction,
                       const ptrdiff_t pred_stride) {
  const int horiz_filter_index = GetFilterIndex(horizontal_filter_index, width);
  const int vert_filter_index = GetFilterIndex(vertical_filter_index, height);
  const int vertical_taps = GetNumTapsInFilter(vert_filter_index);

  // The output of the horizontal filter is guaranteed to fit in 16 bits.
  alignas(16) uint16_t
      intermediate_result[kMaxSuperBlockSizeInPixels *
                          (kMaxSuperBlockSizeInPixels + kSubPixelTaps - 1)];
  const int intermediate_height = height + vertical_taps - 1;

  const ptrdiff_t src_stride = reference_stride;
  const auto* src = static_cast<const uint8_t*>(reference) -
                    (vertical_taps / 2 - 1) * src_stride - kHorizontalOffset;

  DoHorizontalPass</*is_2d=*/true>(src, src_stride, intermediate_result, width,
                                   width, intermediate_height,
                                   horizontal_filter_id, horiz_filter_index);

  // Vertical filter.
  auto* dest = static_cast<uint8_t*>(prediction);
  const ptrdiff_t dest_stride = pred_stride;
  assert(vertical_filter_id != 0);

  __m128i taps[4];
  const __m128i v_filter =
      LoadLo8(kHalfSubPixelFilters[vert_filter_index][vertical_filter_id]);

  if (vertical_taps == 8) {
    SetupTaps<8, /*is_2d_vertical=*/true>(&v_filter, taps);
    if (width == 2) {
      Filter2DVertical2xH<8>(intermediate_result, dest, dest_stride, height,
                             taps);
    } else if (width == 4) {
      Filter2DVertical4xH<8>(intermediate_result, dest, dest_stride, height,
                             taps);
    } else {
      Filter2DVertical<8>(intermediate_result, dest, dest_stride, width, height,
                          taps);
    }
  } else if (vertical_taps == 6) {
    SetupTaps<6, /*is_2d_vertical=*/true>(&v_filter, taps);
    if (width == 2) {
      Filter2DVertical2xH<6>(intermediate_result, dest, dest_stride, height,
                             taps);
    } else if (width == 4) {
      Filter2DVertical4xH<6>(intermediate_result, dest, dest_stride, height,
                             taps);
    } else {
      Filter2DVertical<6>(intermediate_result, dest, dest_stride, width, height,
                          taps);
    }
  } else if (vertical_taps == 4) {
    SetupTaps<4, /*is_2d_vertical=*/true>(&v_filter, taps);
    if (width == 2) {
      Filter2DVertical2xH<4>(intermediate_result, dest, dest_stride, height,
                             taps);
    } else if (width == 4) {
      Filter2DVertical4xH<4>(intermediate_result, dest, dest_stride, height,
                             taps);
    } else {
      Filter2DVertical<4>(intermediate_result, dest, dest_stride, width, height,
                          taps);
    }
  } else {  // |vertical_taps| == 2
    SetupTaps<2, /*is_2d_vertical=*/true>(&v_filter, taps);
    if (width == 2) {
      Filter2DVertical2xH<2>(intermediate_result, dest, dest_stride, height,
                             taps);
    } else if (width == 4) {
      Filter2DVertical4xH<2>(intermediate_result, dest, dest_stride, height,
                             taps);
    } else {
      Filter2DVertical<2>(intermediate_result, dest, dest_stride, width, height,
                          taps);
    }
  }
}

// The 1D compound shift is always |kInterRoundBitsHorizontal|, even for 1D
// Vertical calculations.
__m128i Compound1DShift(const __m128i sum) {
  return RightShiftWithRounding_S16(sum, kInterRoundBitsHorizontal - 1);
}

template <int filter_index>
__m128i SumVerticalTaps(const __m128i* const srcs, const __m128i* const v_tap) {
  __m128i v_src[4];

  if (filter_index < 2) {
    // 6 taps.
    v_src[0] = _mm_unpacklo_epi8(srcs[0], srcs[1]);
    v_src[1] = _mm_unpacklo_epi8(srcs[2], srcs[3]);
    v_src[2] = _mm_unpacklo_epi8(srcs[4], srcs[5]);
  } else if (filter_index == 2) {
    // 8 taps.
    v_src[0] = _mm_unpacklo_epi8(srcs[0], srcs[1]);
    v_src[1] = _mm_unpacklo_epi8(srcs[2], srcs[3]);
    v_src[2] = _mm_unpacklo_epi8(srcs[4], srcs[5]);
    v_src[3] = _mm_unpacklo_epi8(srcs[6], srcs[7]);
  } else if (filter_index == 3) {
    // 2 taps.
    v_src[0] = _mm_unpacklo_epi8(srcs[0], srcs[1]);
  } else if (filter_index > 3) {
    // 4 taps.
    v_src[0] = _mm_unpacklo_epi8(srcs[0], srcs[1]);
    v_src[1] = _mm_unpacklo_epi8(srcs[2], srcs[3]);
  }
  const __m128i sum = SumOnePassTaps<filter_index>(v_src, v_tap);
  return sum;
}

template <int filter_index, bool is_compound = false>
void FilterVertical(const uint8_t* src, const ptrdiff_t src_stride,
                    void* const dst, const ptrdiff_t dst_stride,
                    const int width, const int height,
                    const __m128i* const v_tap) {
  const int num_taps = GetNumTapsInFilter(filter_index);
  const int next_row = num_taps - 1;
  auto* dst8 = static_cast<uint8_t*>(dst);
  auto* dst16 = static_cast<uint16_t*>(dst);
  assert(width >= 8);

  int x = 0;
  do {
    const uint8_t* src_x = src + x;
    __m128i srcs[8];
    srcs[0] = LoadLo8(src_x);
    src_x += src_stride;
    if (num_taps >= 4) {
      srcs[1] = LoadLo8(src_x);
      src_x += src_stride;
      srcs[2] = LoadLo8(src_x);
      src_x += src_stride;
      if (num_taps >= 6) {
        srcs[3] = LoadLo8(src_x);
        src_x += src_stride;
        srcs[4] = LoadLo8(src_x);
        src_x += src_stride;
        if (num_taps == 8) {
          srcs[5] = LoadLo8(src_x);
          src_x += src_stride;
          srcs[6] = LoadLo8(src_x);
          src_x += src_stride;
        }
      }
    }

    int y = 0;
    do {
      srcs[next_row] = LoadLo8(src_x);
      src_x += src_stride;

      const __m128i sums = SumVerticalTaps<filter_index>(srcs, v_tap);
      if (is_compound) {
        const __m128i results = Compound1DShift(sums);
        StoreUnaligned16(dst16 + x + y * dst_stride, results);
      } else {
        const __m128i results =
            RightShiftWithRounding_S16(sums, kFilterBits - 1);
        StoreLo8(dst8 + x + y * dst_stride, _mm_packus_epi16(results, results));
      }

      srcs[0] = srcs[1];
      if (num_taps >= 4) {
        srcs[1] = srcs[2];
        srcs[2] = srcs[3];
        if (num_taps >= 6) {
          srcs[3] = srcs[4];
          srcs[4] = srcs[5];
          if (num_taps == 8) {
            srcs[5] = srcs[6];
            srcs[6] = srcs[7];
          }
        }
      }
    } while (++y < height);
    x += 8;
  } while (x < width);
}

template <int filter_index, bool is_compound = false>
void FilterVertical4xH(const uint8_t* src, const ptrdiff_t src_stride,
                       void* const dst, const ptrdiff_t dst_stride,
                       const int height, const __m128i* const v_tap) {
  const int num_taps = GetNumTapsInFilter(filter_index);
  auto* dst8 = static_cast<uint8_t*>(dst);
  auto* dst16 = static_cast<uint16_t*>(dst);

  __m128i srcs[9];

  if (num_taps == 2) {
    srcs[2] = _mm_setzero_si128();
    // 00 01 02 03
    srcs[0] = Load4(src);
    src += src_stride;

    int y = 0;
    do {
      // 10 11 12 13
      const __m128i a = Load4(src);
      // 00 01 02 03 10 11 12 13
      srcs[0] = _mm_unpacklo_epi32(srcs[0], a);
      src += src_stride;
      // 20 21 22 23
      srcs[2] = Load4(src);
      src += src_stride;
      // 10 11 12 13 20 21 22 23
      srcs[1] = _mm_unpacklo_epi32(a, srcs[2]);

      const __m128i sums = SumVerticalTaps<filter_index>(srcs, v_tap);
      if (is_compound) {
        const __m128i results = Compound1DShift(sums);
        StoreUnaligned16(dst16, results);
        dst16 += 4 << 1;
      } else {
        const __m128i results_16 =
            RightShiftWithRounding_S16(sums, kFilterBits - 1);
        const __m128i results = _mm_packus_epi16(results_16, results_16);
        Store4(dst8, results);
        dst8 += dst_stride;
        Store4(dst8, _mm_srli_si128(results, 4));
        dst8 += dst_stride;
      }

      srcs[0] = srcs[2];
      y += 2;
    } while (y < height);
  } else if (num_taps == 4) {
    srcs[4] = _mm_setzero_si128();
    // 00 01 02 03
    srcs[0] = Load4(src);
    src += src_stride;
    // 10 11 12 13
    const __m128i a = Load4(src);
    // 00 01 02 03 10 11 12 13
    srcs[0] = _mm_unpacklo_epi32(srcs[0], a);
    src += src_stride;
    // 20 21 22 23
    srcs[2] = Load4(src);
    src += src_stride;
    // 10 11 12 13 20 21 22 23
    srcs[1] = _mm_unpacklo_epi32(a, srcs[2]);

    int y = 0;
    do {
      // 30 31 32 33
      const __m128i b = Load4(src);
      // 20 21 22 23 30 31 32 33
      srcs[2] = _mm_unpacklo_epi32(srcs[2], b);
      src += src_stride;
      // 40 41 42 43
      srcs[4] = Load4(src);
      src += src_stride;
      // 30 31 32 33 40 41 42 43
      srcs[3] = _mm_unpacklo_epi32(b, srcs[4]);

      const __m128i sums = SumVerticalTaps<filter_index>(srcs, v_tap);
      if (is_compound) {
        const __m128i results = Compound1DShift(sums);
        StoreUnaligned16(dst16, results);
        dst16 += 4 << 1;
      } else {
        const __m128i results_16 =
            RightShiftWithRounding_S16(sums, kFilterBits - 1);
        const __m128i results = _mm_packus_epi16(results_16, results_16);
        Store4(dst8, results);
        dst8 += dst_stride;
        Store4(dst8, _mm_srli_si128(results, 4));
        dst8 += dst_stride;
      }

      srcs[0] = srcs[2];
      srcs[1] = srcs[3];
      srcs[2] = srcs[4];
      y += 2;
    } while (y < height);
  } else if (num_taps == 6) {
    srcs[6] = _mm_setzero_si128();
    // 00 01 02 03
    srcs[0] = Load4(src);
    src += src_stride;
    // 10 11 12 13
    const __m128i a = Load4(src);
    // 00 01 02 03 10 11 12 13
    srcs[0] = _mm_unpacklo_epi32(srcs[0], a);
    src += src_stride;
    // 20 21 22 23
    srcs[2] = Load4(src);
    src += src_stride;
    // 10 11 12 13 20 21 22 23
    srcs[1] = _mm_unpacklo_epi32(a, srcs[2]);
    // 30 31 32 33
    const __m128i b = Load4(src);
    // 20 21 22 23 30 31 32 33
    srcs[2] = _mm_unpacklo_epi32(srcs[2], b);
    src += src_stride;
    // 40 41 42 43
    srcs[4] = Load4(src);
    src += src_stride;
    // 30 31 32 33 40 41 42 43
    srcs[3] = _mm_unpacklo_epi32(b, srcs[4]);

    int y = 0;
    do {
      // 50 51 52 53
      const __m128i c = Load4(src);
      // 40 41 42 43 50 51 52 53
      srcs[4] = _mm_unpacklo_epi32(srcs[4], c);
      src += src_stride;
      // 60 61 62 63
      srcs[6] = Load4(src);
      src += src_stride;
      // 50 51 52 53 60 61 62 63
      srcs[5] = _mm_unpacklo_epi32(c, srcs[6]);

      const __m128i sums = SumVerticalTaps<filter_index>(srcs, v_tap);
      if (is_compound) {
        const __m128i results = Compound1DShift(sums);
        StoreUnaligned16(dst16, results);
        dst16 += 4 << 1;
      } else {
        const __m128i results_16 =
            RightShiftWithRounding_S16(sums, kFilterBits - 1);
        const __m128i results = _mm_packus_epi16(results_16, results_16);
        Store4(dst8, results);
        dst8 += dst_stride;
        Store4(dst8, _mm_srli_si128(results, 4));
        dst8 += dst_stride;
      }

      srcs[0] = srcs[2];
      srcs[1] = srcs[3];
      srcs[2] = srcs[4];
      srcs[3] = srcs[5];
      srcs[4] = srcs[6];
      y += 2;
    } while (y < height);
  } else if (num_taps == 8) {
    srcs[8] = _mm_setzero_si128();
    // 00 01 02 03
    srcs[0] = Load4(src);
    src += src_stride;
    // 10 11 12 13
    const __m128i a = Load4(src);
    // 00 01 02 03 10 11 12 13
    srcs[0] = _mm_unpacklo_epi32(srcs[0], a);
    src += src_stride;
    // 20 21 22 23
    srcs[2] = Load4(src);
    src += src_stride;
    // 10 11 12 13 20 21 22 23
    srcs[1] = _mm_unpacklo_epi32(a, srcs[2]);
    // 30 31 32 33
    const __m128i b = Load4(src);
    // 20 21 22 23 30 31 32 33
    srcs[2] = _mm_unpacklo_epi32(srcs[2], b);
    src += src_stride;
    // 40 41 42 43
    srcs[4] = Load4(src);
    src += src_stride;
    // 30 31 32 33 40 41 42 43
    srcs[3] = _mm_unpacklo_epi32(b, srcs[4]);
    // 50 51 52 53
    const __m128i c = Load4(src);
    // 40 41 42 43 50 51 52 53
    srcs[4] = _mm_unpacklo_epi32(srcs[4], c);
    src += src_stride;
    // 60 61 62 63
    srcs[6] = Load4(src);
    src += src_stride;
    // 50 51 52 53 60 61 62 63
    srcs[5] = _mm_unpacklo_epi32(c, srcs[6]);

    int y = 0;
    do {
      // 70 71 72 73
      const __m128i d = Load4(src);
      // 60 61 62 63 70 71 72 73
      srcs[6] = _mm_unpacklo_epi32(srcs[6], d);
      src += src_stride;
      // 80 81 82 83
      srcs[8] = Load4(src);
      src += src_stride;
      // 70 71 72 73 80 81 82 83
      srcs[7] = _mm_unpacklo_epi32(d, srcs[8]);

      const __m128i sums = SumVerticalTaps<filter_index>(srcs, v_tap);
      if (is_compound) {
        const __m128i results = Compound1DShift(sums);
        StoreUnaligned16(dst16, results);
        dst16 += 4 << 1;
      } else {
        const __m128i results_16 =
            RightShiftWithRounding_S16(sums, kFilterBits - 1);
        const __m128i results = _mm_packus_epi16(results_16, results_16);
        Store4(dst8, results);
        dst8 += dst_stride;
        Store4(dst8, _mm_srli_si128(results, 4));
        dst8 += dst_stride;
      }

      srcs[0] = srcs[2];
      srcs[1] = srcs[3];
      srcs[2] = srcs[4];
      srcs[3] = srcs[5];
      srcs[4] = srcs[6];
      srcs[5] = srcs[7];
      srcs[6] = srcs[8];
      y += 2;
    } while (y < height);
  }
}

template <int filter_index, bool negative_outside_taps = false>
void FilterVertical2xH(const uint8_t* src, const ptrdiff_t src_stride,
                       void* const dst, const ptrdiff_t dst_stride,
                       const int height, const __m128i* const v_tap) {
  const int num_taps = GetNumTapsInFilter(filter_index);
  auto* dst8 = static_cast<uint8_t*>(dst);

  __m128i srcs[9];

  if (num_taps == 2) {
    srcs[2] = _mm_setzero_si128();
    // 00 01
    srcs[0] = Load2(src);
    src += src_stride;

    int y = 0;
    do {
      // 00 01 10 11
      srcs[0] = Load2<1>(src, srcs[0]);
      src += src_stride;
      // 00 01 10 11 20 21
      srcs[0] = Load2<2>(src, srcs[0]);
      src += src_stride;
      // 00 01 10 11 20 21 30 31
      srcs[0] = Load2<3>(src, srcs[0]);
      src += src_stride;
      // 40 41
      srcs[2] = Load2<0>(src, srcs[2]);
      src += src_stride;
      // 00 01 10 11 20 21 30 31 40 41
      const __m128i srcs_0_2 = _mm_unpacklo_epi64(srcs[0], srcs[2]);
      // 10 11 20 21 30 31 40 41
      srcs[1] = _mm_srli_si128(srcs_0_2, 2);
      // This uses srcs[0]..srcs[1].
      const __m128i sums = SumVerticalTaps<filter_index>(srcs, v_tap);
      const __m128i results_16 =
          RightShiftWithRounding_S16(sums, kFilterBits - 1);
      const __m128i results = _mm_packus_epi16(results_16, results_16);

      Store2(dst8, results);
      dst8 += dst_stride;
      Store2(dst8, _mm_srli_si128(results, 2));
      if (height == 2) return;
      dst8 += dst_stride;
      Store2(dst8, _mm_srli_si128(results, 4));
      dst8 += dst_stride;
      Store2(dst8, _mm_srli_si128(results, 6));
      dst8 += dst_stride;

      srcs[0] = srcs[2];
      y += 4;
    } while (y < height);
  } else if (num_taps == 4) {
    srcs[4] = _mm_setzero_si128();

    // 00 01
    srcs[0] = Load2(src);
    src += src_stride;
    // 00 01 10 11
    srcs[0] = Load2<1>(src, srcs[0]);
    src += src_stride;
    // 00 01 10 11 20 21
    srcs[0] = Load2<2>(src, srcs[0]);
    src += src_stride;

    int y = 0;
    do {
      // 00 01 10 11 20 21 30 31
      srcs[0] = Load2<3>(src, srcs[0]);
      src += src_stride;
      // 40 41
      srcs[4] = Load2<0>(src, srcs[4]);
      src += src_stride;
      // 40 41 50 51
      srcs[4] = Load2<1>(src, srcs[4]);
      src += src_stride;
      // 40 41 50 51 60 61
      srcs[4] = Load2<2>(src, srcs[4]);
      src += src_stride;
      // 00 01 10 11 20 21 30 31 40 41 50 51 60 61
      const __m128i srcs_0_4 = _mm_unpacklo_epi64(srcs[0], srcs[4]);
      // 10 11 20 21 30 31 40 41
      srcs[1] = _mm_srli_si128(srcs_0_4, 2);
      // 20 21 30 31 40 41 50 51
      srcs[2] = _mm_srli_si128(srcs_0_4, 4);
      // 30 31 40 41 50 51 60 61
      srcs[3] = _mm_srli_si128(srcs_0_4, 6);

      // This uses srcs[0]..srcs[3].
      const __m128i sums = SumVerticalTaps<filter_index>(srcs, v_tap);
      const __m128i results_16 =
          RightShiftWithRounding_S16(sums, kFilterBits - 1);
      const __m128i results = _mm_packus_epi16(results_16, results_16);

      Store2(dst8, results);
      dst8 += dst_stride;
      Store2(dst8, _mm_srli_si128(results, 2));
      if (height == 2) return;
      dst8 += dst_stride;
      Store2(dst8, _mm_srli_si128(results, 4));
      dst8 += dst_stride;
      Store2(dst8, _mm_srli_si128(results, 6));
      dst8 += dst_stride;

      srcs[0] = srcs[4];
      y += 4;
    } while (y < height);
  } else if (num_taps == 6) {
    // During the vertical pass the number of taps is restricted when
    // |height| <= 4.
    assert(height > 4);
    srcs[8] = _mm_setzero_si128();

    // 00 01
    srcs[0] = Load2(src);
    src += src_stride;
    // 00 01 10 11
    srcs[0] = Load2<1>(src, srcs[0]);
    src += src_stride;
    // 00 01 10 11 20 21
    srcs[0] = Load2<2>(src, srcs[0]);
    src += src_stride;
    // 00 01 10 11 20 21 30 31
    srcs[0] = Load2<3>(src, srcs[0]);
    src += src_stride;
    // 40 41
    srcs[4] = Load2(src);
    src += src_stride;
    // 00 01 10 11 20 21 30 31 40 41 50 51 60 61
    const __m128i srcs_0_4x = _mm_unpacklo_epi64(srcs[0], srcs[4]);
    // 10 11 20 21 30 31 40 41
    srcs[1] = _mm_srli_si128(srcs_0_4x, 2);

    int y = 0;
    do {
      // 40 41 50 51
      srcs[4] = Load2<1>(src, srcs[4]);
      src += src_stride;
      // 40 41 50 51 60 61
      srcs[4] = Load2<2>(src, srcs[4]);
      src += src_stride;
      // 40 41 50 51 60 61 70 71
      srcs[4] = Load2<3>(src, srcs[4]);
      src += src_stride;
      // 80 81
      srcs[8] = Load2<0>(src, srcs[8]);
      src += src_stride;
      // 00 01 10 11 20 21 30 31 40 41 50 51 60 61
      const __m128i srcs_0_4 = _mm_unpacklo_epi64(srcs[0], srcs[4]);
      // 20 21 30 31 40 41 50 51
      srcs[2] = _mm_srli_si128(srcs_0_4, 4);
      // 30 31 40 41 50 51 60 61
      srcs[3] = _mm_srli_si128(srcs_0_4, 6);
      const __m128i srcs_4_8 = _mm_unpacklo_epi64(srcs[4], srcs[8]);
      // 50 51 60 61 70 71 80 81
      srcs[5] = _mm_srli_si128(srcs_4_8, 2);

      // This uses srcs[0]..srcs[5].
      const __m128i sums = SumVerticalTaps<filter_index>(srcs, v_tap);
      const __m128i results_16 =
          RightShiftWithRounding_S16(sums, kFilterBits - 1);
      const __m128i results = _mm_packus_epi16(results_16, results_16);

      Store2(dst8, results);
      dst8 += dst_stride;
      Store2(dst8, _mm_srli_si128(results, 2));
      dst8 += dst_stride;
      Store2(dst8, _mm_srli_si128(results, 4));
      dst8 += dst_stride;
      Store2(dst8, _mm_srli_si128(results, 6));
      dst8 += dst_stride;

      srcs[0] = srcs[4];
      srcs[1] = srcs[5];
      srcs[4] = srcs[8];
      y += 4;
    } while (y < height);
  } else if (num_taps == 8) {
    // During the vertical pass the number of taps is restricted when
    // |height| <= 4.
    assert(height > 4);
    srcs[8] = _mm_setzero_si128();
    // 00 01
    srcs[0] = Load2(src);
    src += src_stride;
    // 00 01 10 11
    srcs[0] = Load2<1>(src, srcs[0]);
    src += src_stride;
    // 00 01 10 11 20 21
    srcs[0] = Load2<2>(src, srcs[0]);
    src += src_stride;
    // 00 01 10 11 20 21 30 31
    srcs[0] = Load2<3>(src, srcs[0]);
    src += src_stride;
    // 40 41
    srcs[4] = Load2(src);
    src += src_stride;
    // 40 41 50 51
    srcs[4] = Load2<1>(src, srcs[4]);
    src += src_stride;
    // 40 41 50 51 60 61
    srcs[4] = Load2<2>(src, srcs[4]);
    src += src_stride;

    // 00 01 10 11 20 21 30 31 40 41 50 51 60 61
    const __m128i srcs_0_4 = _mm_unpacklo_epi64(srcs[0], srcs[4]);
    // 10 11 20 21 30 31 40 41
    srcs[1] = _mm_srli_si128(srcs_0_4, 2);
    // 20 21 30 31 40 41 50 51
    srcs[2] = _mm_srli_si128(srcs_0_4, 4);
    // 30 31 40 41 50 51 60 61
    srcs[3] = _mm_srli_si128(srcs_0_4, 6);

    int y = 0;
    do {
      // 40 41 50 51 60 61 70 71
      srcs[4] = Load2<3>(src, srcs[4]);
      src += src_stride;
      // 80 81
      srcs[8] = Load2<0>(src, srcs[8]);
      src += src_stride;
      // 80 81 90 91
      srcs[8] = Load2<1>(src, srcs[8]);
      src += src_stride;
      // 80 81 90 91 a0 a1
      srcs[8] = Load2<2>(src, srcs[8]);
      src += src_stride;

      // 40 41 50 51 60 61 70 71 80 81 90 91 a0 a1
      const __m128i srcs_4_8 = _mm_unpacklo_epi64(srcs[4], srcs[8]);
      // 50 51 60 61 70 71 80 81
      srcs[5] = _mm_srli_si128(srcs_4_8, 2);
      // 60 61 70 71 80 81 90 91
      srcs[6] = _mm_srli_si128(srcs_4_8, 4);
      // 70 71 80 81 90 91 a0 a1
      srcs[7] = _mm_srli_si128(srcs_4_8, 6);

      // This uses srcs[0]..srcs[7].
      const __m128i sums = SumVerticalTaps<filter_index>(srcs, v_tap);
      const __m128i results_16 =
          RightShiftWithRounding_S16(sums, kFilterBits - 1);
      const __m128i results = _mm_packus_epi16(results_16, results_16);

      Store2(dst8, results);
      dst8 += dst_stride;
      Store2(dst8, _mm_srli_si128(results, 2));
      dst8 += dst_stride;
      Store2(dst8, _mm_srli_si128(results, 4));
      dst8 += dst_stride;
      Store2(dst8, _mm_srli_si128(results, 6));
      dst8 += dst_stride;

      srcs[0] = srcs[4];
      srcs[1] = srcs[5];
      srcs[2] = srcs[6];
      srcs[3] = srcs[7];
      srcs[4] = srcs[8];
      y += 4;
    } while (y < height);
  }
}

void ConvolveVertical_SSE4_1(const void* const reference,
                             const ptrdiff_t reference_stride,
                             const int /*horizontal_filter_index*/,
                             const int vertical_filter_index,
                             const int /*horizontal_filter_id*/,
                             const int vertical_filter_id, const int width,
                             const int height, void* prediction,
                             const ptrdiff_t pred_stride) {
  const int filter_index = GetFilterIndex(vertical_filter_index, height);
  const int vertical_taps = GetNumTapsInFilter(filter_index);
  const ptrdiff_t src_stride = reference_stride;
  const auto* src = static_cast<const uint8_t*>(reference) -
                    (vertical_taps / 2 - 1) * src_stride;
  auto* dest = static_cast<uint8_t*>(prediction);
  const ptrdiff_t dest_stride = pred_stride;
  assert(vertical_filter_id != 0);

  __m128i taps[4];
  const __m128i v_filter =
      LoadLo8(kHalfSubPixelFilters[filter_index][vertical_filter_id]);

  if (filter_index < 2) {  // 6 tap.
    SetupTaps<6>(&v_filter, taps);
    if (width == 2) {
      FilterVertical2xH<0>(src, src_stride, dest, dest_stride, height, taps);
    } else if (width == 4) {
      FilterVertical4xH<0>(src, src_stride, dest, dest_stride, height, taps);
    } else {
      FilterVertical<0>(src, src_stride, dest, dest_stride, width, height,
                        taps);
    }
  } else if (filter_index == 2) {  // 8 tap.
    SetupTaps<8>(&v_filter, taps);
    if (width == 2) {
      FilterVertical2xH<2>(src, src_stride, dest, dest_stride, height, taps);
    } else if (width == 4) {
      FilterVertical4xH<2>(src, src_stride, dest, dest_stride, height, taps);
    } else {
      FilterVertical<2>(src, src_stride, dest, dest_stride, width, height,
                        taps);
    }
  } else if (filter_index == 3) {  // 2 tap.
    SetupTaps<2>(&v_filter, taps);
    if (width == 2) {
      FilterVertical2xH<3>(src, src_stride, dest, dest_stride, height, taps);
    } else if (width == 4) {
      FilterVertical4xH<3>(src, src_stride, dest, dest_stride, height, taps);
    } else {
      FilterVertical<3>(src, src_stride, dest, dest_stride, width, height,
                        taps);
    }
  } else if (filter_index == 4) {  // 4 tap.
    SetupTaps<4>(&v_filter, taps);
    if (width == 2) {
      FilterVertical2xH<4>(src, src_stride, dest, dest_stride, height, taps);
    } else if (width == 4) {
      FilterVertical4xH<4>(src, src_stride, dest, dest_stride, height, taps);
    } else {
      FilterVertical<4>(src, src_stride, dest, dest_stride, width, height,
                        taps);
    }
  } else {
    // TODO(slavarnway): Investigate adding |filter_index| == 1 special cases.
    // See convolve_neon.cc
    SetupTaps<4>(&v_filter, taps);

    if (width == 2) {
      FilterVertical2xH<5>(src, src_stride, dest, dest_stride, height, taps);
    } else if (width == 4) {
      FilterVertical4xH<5>(src, src_stride, dest, dest_stride, height, taps);
    } else {
      FilterVertical<5>(src, src_stride, dest, dest_stride, width, height,
                        taps);
    }
  }
}

void ConvolveCompoundCopy_SSE4(const void* const reference,
                               const ptrdiff_t reference_stride,
                               const int /*horizontal_filter_index*/,
                               const int /*vertical_filter_index*/,
                               const int /*horizontal_filter_id*/,
                               const int /*vertical_filter_id*/,
                               const int width, const int height,
                               void* prediction, const ptrdiff_t pred_stride) {
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

void ConvolveCompoundVertical_SSE4_1(
    const void* const reference, const ptrdiff_t reference_stride,
    const int /*horizontal_filter_index*/, const int vertical_filter_index,
    const int /*horizontal_filter_id*/, const int vertical_filter_id,
    const int width, const int height, void* prediction,
    const ptrdiff_t /*pred_stride*/) {
  const int filter_index = GetFilterIndex(vertical_filter_index, height);
  const int vertical_taps = GetNumTapsInFilter(filter_index);
  const ptrdiff_t src_stride = reference_stride;
  const auto* src = static_cast<const uint8_t*>(reference) -
                    (vertical_taps / 2 - 1) * src_stride;
  auto* dest = static_cast<uint16_t*>(prediction);
  assert(vertical_filter_id != 0);

  __m128i taps[4];
  const __m128i v_filter =
      LoadLo8(kHalfSubPixelFilters[filter_index][vertical_filter_id]);

  if (filter_index < 2) {  // 6 tap.
    SetupTaps<6>(&v_filter, taps);
    if (width == 4) {
      FilterVertical4xH<0, /*is_compound=*/true>(src, src_stride, dest, 4,
                                                 height, taps);
    } else {
      FilterVertical<0, /*is_compound=*/true>(src, src_stride, dest, width,
                                              width, height, taps);
    }
  } else if (filter_index == 2) {  // 8 tap.
    SetupTaps<8>(&v_filter, taps);

    if (width == 4) {
      FilterVertical4xH<2, /*is_compound=*/true>(src, src_stride, dest, 4,
                                                 height, taps);
    } else {
      FilterVertical<2, /*is_compound=*/true>(src, src_stride, dest, width,
                                              width, height, taps);
    }
  } else if (filter_index == 3) {  // 2 tap.
    SetupTaps<2>(&v_filter, taps);

    if (width == 4) {
      FilterVertical4xH<3, /*is_compound=*/true>(src, src_stride, dest, 4,
                                                 height, taps);
    } else {
      FilterVertical<3, /*is_compound=*/true>(src, src_stride, dest, width,
                                              width, height, taps);
    }
  } else if (filter_index == 4) {  // 4 tap.
    SetupTaps<4>(&v_filter, taps);

    if (width == 4) {
      FilterVertical4xH<4, /*is_compound=*/true>(src, src_stride, dest, 4,
                                                 height, taps);
    } else {
      FilterVertical<4, /*is_compound=*/true>(src, src_stride, dest, width,
                                              width, height, taps);
    }
  } else {
    SetupTaps<4>(&v_filter, taps);

    if (width == 4) {
      FilterVertical4xH<5, /*is_compound=*/true>(src, src_stride, dest, 4,
                                                 height, taps);
    } else {
      FilterVertical<5, /*is_compound=*/true>(src, src_stride, dest, width,
                                              width, height, taps);
    }
  }
}

void ConvolveHorizontal_SSE4_1(const void* const reference,
                               const ptrdiff_t reference_stride,
                               const int horizontal_filter_index,
                               const int /*vertical_filter_index*/,
                               const int horizontal_filter_id,
                               const int /*vertical_filter_id*/,
                               const int width, const int height,
                               void* prediction, const ptrdiff_t pred_stride) {
  const int filter_index = GetFilterIndex(horizontal_filter_index, width);
  // Set |src| to the outermost tap.
  const auto* src = static_cast<const uint8_t*>(reference) - kHorizontalOffset;
  auto* dest = static_cast<uint8_t*>(prediction);

  DoHorizontalPass(src, reference_stride, dest, pred_stride, width, height,
                   horizontal_filter_id, filter_index);
}

void ConvolveCompoundHorizontal_SSE4_1(
    const void* const reference, const ptrdiff_t reference_stride,
    const int horizontal_filter_index, const int /*vertical_filter_index*/,
    const int horizontal_filter_id, const int /*vertical_filter_id*/,
    const int width, const int height, void* prediction,
    const ptrdiff_t /*pred_stride*/) {
  const int filter_index = GetFilterIndex(horizontal_filter_index, width);
  const auto* src = static_cast<const uint8_t*>(reference) - kHorizontalOffset;
  auto* dest = static_cast<uint16_t*>(prediction);

  DoHorizontalPass</*is_2d=*/false, /*is_compound=*/true>(
      src, reference_stride, dest, width, width, height, horizontal_filter_id,
      filter_index);
}

void ConvolveCompound2D_SSE4_1(const void* const reference,
                               const ptrdiff_t reference_stride,
                               const int horizontal_filter_index,
                               const int vertical_filter_index,
                               const int horizontal_filter_id,
                               const int vertical_filter_id, const int width,
                               const int height, void* prediction,
                               const ptrdiff_t /*pred_stride*/) {
  // The output of the horizontal filter, i.e. the intermediate_result, is
  // guaranteed to fit in int16_t.
  alignas(16) uint16_t
      intermediate_result[kMaxSuperBlockSizeInPixels *
                          (kMaxSuperBlockSizeInPixels + kSubPixelTaps - 1)];

  // Horizontal filter.
  // Filter types used for width <= 4 are different from those for width > 4.
  // When width > 4, the valid filter index range is always [0, 3].
  // When width <= 4, the valid filter index range is always [4, 5].
  // Similarly for height.
  const int horiz_filter_index = GetFilterIndex(horizontal_filter_index, width);
  const int vert_filter_index = GetFilterIndex(vertical_filter_index, height);
  const int vertical_taps = GetNumTapsInFilter(vert_filter_index);
  const int intermediate_height = height + vertical_taps - 1;
  const ptrdiff_t src_stride = reference_stride;
  const auto* const src = static_cast<const uint8_t*>(reference) -
                          (vertical_taps / 2 - 1) * src_stride -
                          kHorizontalOffset;

  DoHorizontalPass</*is_2d=*/true, /*is_compound=*/true>(
      src, src_stride, intermediate_result, width, width, intermediate_height,
      horizontal_filter_id, horiz_filter_index);

  // Vertical filter.
  auto* dest = static_cast<uint16_t*>(prediction);
  assert(vertical_filter_id != 0);

  const ptrdiff_t dest_stride = width;
  __m128i taps[4];
  const __m128i v_filter =
      LoadLo8(kHalfSubPixelFilters[vert_filter_index][vertical_filter_id]);

  if (vertical_taps == 8) {
    SetupTaps<8, /*is_2d_vertical=*/true>(&v_filter, taps);
    if (width == 4) {
      Filter2DVertical4xH<8, /*is_compound=*/true>(intermediate_result, dest,
                                                   dest_stride, height, taps);
    } else {
      Filter2DVertical<8, /*is_compound=*/true>(
          intermediate_result, dest, dest_stride, width, height, taps);
    }
  } else if (vertical_taps == 6) {
    SetupTaps<6, /*is_2d_vertical=*/true>(&v_filter, taps);
    if (width == 4) {
      Filter2DVertical4xH<6, /*is_compound=*/true>(intermediate_result, dest,
                                                   dest_stride, height, taps);
    } else {
      Filter2DVertical<6, /*is_compound=*/true>(
          intermediate_result, dest, dest_stride, width, height, taps);
    }
  } else if (vertical_taps == 4) {
    SetupTaps<4, /*is_2d_vertical=*/true>(&v_filter, taps);
    if (width == 4) {
      Filter2DVertical4xH<4, /*is_compound=*/true>(intermediate_result, dest,
                                                   dest_stride, height, taps);
    } else {
      Filter2DVertical<4, /*is_compound=*/true>(
          intermediate_result, dest, dest_stride, width, height, taps);
    }
  } else {  // |vertical_taps| == 2
    SetupTaps<2, /*is_2d_vertical=*/true>(&v_filter, taps);
    if (width == 4) {
      Filter2DVertical4xH<2, /*is_compound=*/true>(intermediate_result, dest,
                                                   dest_stride, height, taps);
    } else {
      Filter2DVertical<2, /*is_compound=*/true>(
          intermediate_result, dest, dest_stride, width, height, taps);
    }
  }
}

// Pre-transposed filters.
template <int filter_index>
inline void GetHalfSubPixelFilter(__m128i* output) {
  // Filter 0
  alignas(
      16) static constexpr int8_t kHalfSubPixel6TapSignedFilterColumns[6][16] =
      {{0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
       {0, -3, -5, -6, -7, -7, -8, -7, -7, -6, -6, -6, -5, -4, -2, -1},
       {64, 63, 61, 58, 55, 51, 47, 42, 38, 33, 29, 24, 19, 14, 9, 4},
       {0, 4, 9, 14, 19, 24, 29, 33, 38, 42, 47, 51, 55, 58, 61, 63},
       {0, -1, -2, -4, -5, -6, -6, -6, -7, -7, -8, -7, -7, -6, -5, -3},
       {0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};
  // Filter 1
  alignas(16) static constexpr int8_t
      kHalfSubPixel6TapMixedSignedFilterColumns[6][16] = {
          {0, 1, 0, 0, 0, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0},
          {0, 14, 13, 11, 10, 9, 8, 8, 7, 6, 5, 4, 3, 2, 2, 1},
          {64, 31, 31, 31, 30, 29, 28, 27, 26, 24, 23, 22, 21, 20, 18, 17},
          {0, 17, 18, 20, 21, 22, 23, 24, 26, 27, 28, 29, 30, 31, 31, 31},
          {0, 1, 2, 2, 3, 4, 5, 6, 7, 8, 8, 9, 10, 11, 13, 14},
          {0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, 0, 0, 0, 1}};
  // Filter 2
  alignas(
      16) static constexpr int8_t kHalfSubPixel8TapSignedFilterColumns[8][16] =
      {{0, -1, -1, -1, -2, -2, -2, -2, -2, -1, -1, -1, -1, -1, -1, 0},
       {0, 1, 3, 4, 5, 5, 5, 5, 6, 5, 4, 4, 3, 3, 2, 1},
       {0, -3, -6, -9, -11, -11, -12, -12, -12, -11, -10, -9, -7, -5, -3, -1},
       {64, 63, 62, 60, 58, 54, 50, 45, 40, 35, 30, 24, 19, 13, 8, 4},
       {0, 4, 8, 13, 19, 24, 30, 35, 40, 45, 50, 54, 58, 60, 62, 63},
       {0, -1, -3, -5, -7, -9, -10, -11, -12, -12, -12, -11, -11, -9, -6, -3},
       {0, 1, 2, 3, 3, 4, 4, 5, 6, 5, 5, 5, 5, 4, 3, 1},
       {0, 0, -1, -1, -1, -1, -1, -1, -2, -2, -2, -2, -2, -1, -1, -1}};
  // Filter 3
  alignas(16) static constexpr uint8_t kHalfSubPixel2TapFilterColumns[2][16] = {
      {64, 60, 56, 52, 48, 44, 40, 36, 32, 28, 24, 20, 16, 12, 8, 4},
      {0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60}};
  // Filter 4
  alignas(
      16) static constexpr int8_t kHalfSubPixel4TapSignedFilterColumns[4][16] =
      {{0, -2, -4, -5, -6, -6, -7, -6, -6, -5, -5, -5, -4, -3, -2, -1},
       {64, 63, 61, 58, 55, 51, 47, 42, 38, 33, 29, 24, 19, 14, 9, 4},
       {0, 4, 9, 14, 19, 24, 29, 33, 38, 42, 47, 51, 55, 58, 61, 63},
       {0, -1, -2, -3, -4, -5, -5, -5, -6, -6, -7, -6, -6, -5, -4, -2}};
  // Filter 5
  alignas(
      16) static constexpr uint8_t kSubPixel4TapPositiveFilterColumns[4][16] = {
      {0, 15, 13, 11, 10, 9, 8, 7, 6, 6, 5, 4, 3, 2, 2, 1},
      {64, 31, 31, 31, 30, 29, 28, 27, 26, 24, 23, 22, 21, 20, 18, 17},
      {0, 17, 18, 20, 21, 22, 23, 24, 26, 27, 28, 29, 30, 31, 31, 31},
      {0, 1, 2, 2, 3, 4, 5, 6, 6, 7, 8, 9, 10, 11, 13, 15}};
  switch (filter_index) {
    case 0:
      output[0] = LoadAligned16(kHalfSubPixel6TapSignedFilterColumns[0]);
      output[1] = LoadAligned16(kHalfSubPixel6TapSignedFilterColumns[1]);
      output[2] = LoadAligned16(kHalfSubPixel6TapSignedFilterColumns[2]);
      output[3] = LoadAligned16(kHalfSubPixel6TapSignedFilterColumns[3]);
      output[4] = LoadAligned16(kHalfSubPixel6TapSignedFilterColumns[4]);
      output[5] = LoadAligned16(kHalfSubPixel6TapSignedFilterColumns[5]);
      break;
    case 1:
      // The term "mixed" refers to the fact that the outer taps have a mix of
      // negative and positive values.
      output[0] = LoadAligned16(kHalfSubPixel6TapMixedSignedFilterColumns[0]);
      output[1] = LoadAligned16(kHalfSubPixel6TapMixedSignedFilterColumns[1]);
      output[2] = LoadAligned16(kHalfSubPixel6TapMixedSignedFilterColumns[2]);
      output[3] = LoadAligned16(kHalfSubPixel6TapMixedSignedFilterColumns[3]);
      output[4] = LoadAligned16(kHalfSubPixel6TapMixedSignedFilterColumns[4]);
      output[5] = LoadAligned16(kHalfSubPixel6TapMixedSignedFilterColumns[5]);
      break;
    case 2:
      output[0] = LoadAligned16(kHalfSubPixel8TapSignedFilterColumns[0]);
      output[1] = LoadAligned16(kHalfSubPixel8TapSignedFilterColumns[1]);
      output[2] = LoadAligned16(kHalfSubPixel8TapSignedFilterColumns[2]);
      output[3] = LoadAligned16(kHalfSubPixel8TapSignedFilterColumns[3]);
      output[4] = LoadAligned16(kHalfSubPixel8TapSignedFilterColumns[4]);
      output[5] = LoadAligned16(kHalfSubPixel8TapSignedFilterColumns[5]);
      output[6] = LoadAligned16(kHalfSubPixel8TapSignedFilterColumns[6]);
      output[7] = LoadAligned16(kHalfSubPixel8TapSignedFilterColumns[7]);
      break;
    case 3:
      output[0] = LoadAligned16(kHalfSubPixel2TapFilterColumns[0]);
      output[1] = LoadAligned16(kHalfSubPixel2TapFilterColumns[1]);
      break;
    case 4:
      output[0] = LoadAligned16(kHalfSubPixel4TapSignedFilterColumns[0]);
      output[1] = LoadAligned16(kHalfSubPixel4TapSignedFilterColumns[1]);
      output[2] = LoadAligned16(kHalfSubPixel4TapSignedFilterColumns[2]);
      output[3] = LoadAligned16(kHalfSubPixel4TapSignedFilterColumns[3]);
      break;
    default:
      assert(filter_index == 5);
      output[0] = LoadAligned16(kSubPixel4TapPositiveFilterColumns[0]);
      output[1] = LoadAligned16(kSubPixel4TapPositiveFilterColumns[1]);
      output[2] = LoadAligned16(kSubPixel4TapPositiveFilterColumns[2]);
      output[3] = LoadAligned16(kSubPixel4TapPositiveFilterColumns[3]);
      break;
  }
}

// There are many opportunities for overreading in scaled convolve, because
// the range of starting points for filter windows is anywhere from 0 to 16
// for 8 destination pixels, and the window sizes range from 2 to 8. To
// accommodate this range concisely, we use |grade_x| to mean the most steps
// in src that can be traversed in a single |step_x| increment, i.e. 1 or 2.
// More importantly, |grade_x| answers the question "how many vector loads are
// needed to cover the source values?"
// When |grade_x| == 1, the maximum number of source values needed is 8 separate
// starting positions plus 7 more to cover taps, all fitting into 16 bytes.
// When |grade_x| > 1, we are guaranteed to exceed 8 whole steps in src for
// every 8 |step_x| increments, on top of 8 possible taps. The first load covers
// the starting sources for each kernel, while the final load covers the taps.
// Since the offset value of src_x cannot exceed 8 and |num_taps| does not
// exceed 4 when width <= 4, |grade_x| is set to 1 regardless of the value of
// |step_x|.
template <int num_taps, int grade_x>
inline void PrepareSourceVectors(const uint8_t* src, const __m128i src_indices,
                                 __m128i* const source /*[num_taps >> 1]*/) {
  const __m128i src_vals = LoadUnaligned16(src);
  source[0] = _mm_shuffle_epi8(src_vals, src_indices);
  if (grade_x == 1) {
    if (num_taps > 2) {
      source[1] = _mm_shuffle_epi8(_mm_srli_si128(src_vals, 2), src_indices);
    }
    if (num_taps > 4) {
      source[2] = _mm_shuffle_epi8(_mm_srli_si128(src_vals, 4), src_indices);
    }
    if (num_taps > 6) {
      source[3] = _mm_shuffle_epi8(_mm_srli_si128(src_vals, 6), src_indices);
    }
  } else {
    assert(grade_x > 1);
    assert(num_taps != 4);
    // grade_x > 1 also means width >= 8 && num_taps != 4
    const __m128i src_vals_ext = LoadLo8(src + 16);
    if (num_taps > 2) {
      source[1] = _mm_shuffle_epi8(_mm_alignr_epi8(src_vals_ext, src_vals, 2),
                                   src_indices);
      source[2] = _mm_shuffle_epi8(_mm_alignr_epi8(src_vals_ext, src_vals, 4),
                                   src_indices);
    }
    if (num_taps > 6) {
      source[3] = _mm_shuffle_epi8(_mm_alignr_epi8(src_vals_ext, src_vals, 6),
                                   src_indices);
    }
  }
}

template <int num_taps>
inline void PrepareHorizontalTaps(const __m128i subpel_indices,
                                  const __m128i* filter_taps,
                                  __m128i* out_taps) {
  const __m128i scale_index_offsets =
      _mm_srli_epi16(subpel_indices, kFilterIndexShift);
  const __m128i filter_index_mask = _mm_set1_epi8(kSubPixelMask);
  const __m128i filter_indices =
      _mm_and_si128(_mm_packus_epi16(scale_index_offsets, scale_index_offsets),
                    filter_index_mask);
  // Line up taps for maddubs_epi16.
  // The unpack is also assumed to be lighter than shift+alignr.
  for (int k = 0; k < (num_taps >> 1); ++k) {
    const __m128i taps0 = _mm_shuffle_epi8(filter_taps[2 * k], filter_indices);
    const __m128i taps1 =
        _mm_shuffle_epi8(filter_taps[2 * k + 1], filter_indices);
    out_taps[k] = _mm_unpacklo_epi8(taps0, taps1);
  }
}

inline __m128i HorizontalScaleIndices(const __m128i subpel_indices) {
  const __m128i src_indices16 =
      _mm_srli_epi16(subpel_indices, kScaleSubPixelBits);
  const __m128i src_indices = _mm_packus_epi16(src_indices16, src_indices16);
  return _mm_unpacklo_epi8(src_indices,
                           _mm_add_epi8(src_indices, _mm_set1_epi8(1)));
}

template <int grade_x, int filter_index, int num_taps>
inline void ConvolveHorizontalScale(const uint8_t* src, ptrdiff_t src_stride,
                                    int width, int subpixel_x, int step_x,
                                    int intermediate_height,
                                    int16_t* intermediate) {
  // Account for the 0-taps that precede the 2 nonzero taps.
  const int kernel_offset = (8 - num_taps) >> 1;
  const int ref_x = subpixel_x >> kScaleSubPixelBits;
  const int step_x8 = step_x << 3;
  __m128i filter_taps[num_taps];
  GetHalfSubPixelFilter<filter_index>(filter_taps);
  const __m128i index_steps =
      _mm_mullo_epi16(_mm_set_epi16(7, 6, 5, 4, 3, 2, 1, 0),
                      _mm_set1_epi16(static_cast<int16_t>(step_x)));

  __m128i taps[num_taps >> 1];
  __m128i source[num_taps >> 1];
  int p = subpixel_x;
  // Case when width <= 4 is possible.
  if (filter_index >= 3) {
    if (filter_index > 3 || width <= 4) {
      const uint8_t* src_x =
          &src[(p >> kScaleSubPixelBits) - ref_x + kernel_offset];
      // Only add steps to the 10-bit truncated p to avoid overflow.
      const __m128i p_fraction = _mm_set1_epi16(p & 1023);
      const __m128i subpel_indices = _mm_add_epi16(index_steps, p_fraction);
      PrepareHorizontalTaps<num_taps>(subpel_indices, filter_taps, taps);
      const __m128i packed_indices = HorizontalScaleIndices(subpel_indices);

      int y = intermediate_height;
      do {
        // Load and line up source values with the taps. Width 4 means no need
        // to load extended source.
        PrepareSourceVectors<num_taps, /*grade_x=*/1>(src_x, packed_indices,
                                                      source);

        StoreLo8(intermediate, RightShiftWithRounding_S16(
                                   SumOnePassTaps<filter_index>(source, taps),
                                   kInterRoundBitsHorizontal - 1));
        src_x += src_stride;
        intermediate += kIntermediateStride;
      } while (--y != 0);
      return;
    }
  }

  // |width| >= 8
  int x = 0;
  do {
    const uint8_t* src_x =
        &src[(p >> kScaleSubPixelBits) - ref_x + kernel_offset];
    int16_t* intermediate_x = intermediate + x;
    // Only add steps to the 10-bit truncated p to avoid overflow.
    const __m128i p_fraction = _mm_set1_epi16(p & 1023);
    const __m128i subpel_indices = _mm_add_epi16(index_steps, p_fraction);
    PrepareHorizontalTaps<num_taps>(subpel_indices, filter_taps, taps);
    const __m128i packed_indices = HorizontalScaleIndices(subpel_indices);

    int y = intermediate_height;
    do {
      // For each x, a lane of src_k[k] contains src_x[k].
      PrepareSourceVectors<num_taps, grade_x>(src_x, packed_indices, source);

      // Shift by one less because the taps are halved.
      StoreAligned16(
          intermediate_x,
          RightShiftWithRounding_S16(SumOnePassTaps<filter_index>(source, taps),
                                     kInterRoundBitsHorizontal - 1));
      src_x += src_stride;
      intermediate_x += kIntermediateStride;
    } while (--y != 0);
    x += 8;
    p += step_x8;
  } while (x < width);
}

template <int num_taps>
inline void PrepareVerticalTaps(const int8_t* taps, __m128i* output) {
  // Avoid overreading the filter due to starting at kernel_offset.
  // The only danger of overread is in the final filter, which has 4 taps.
  const __m128i filter =
      _mm_cvtepi8_epi16((num_taps > 4) ? LoadLo8(taps) : Load4(taps));
  output[0] = _mm_shuffle_epi32(filter, 0);
  if (num_taps > 2) {
    output[1] = _mm_shuffle_epi32(filter, 0x55);
  }
  if (num_taps > 4) {
    output[2] = _mm_shuffle_epi32(filter, 0xAA);
  }
  if (num_taps > 6) {
    output[3] = _mm_shuffle_epi32(filter, 0xFF);
  }
}

// Process eight 16 bit inputs and output eight 16 bit values.
template <int num_taps, bool is_compound>
inline __m128i Sum2DVerticalTaps(const __m128i* const src,
                                 const __m128i* taps) {
  const __m128i src_lo_01 = _mm_unpacklo_epi16(src[0], src[1]);
  __m128i sum_lo = _mm_madd_epi16(src_lo_01, taps[0]);
  const __m128i src_hi_01 = _mm_unpackhi_epi16(src[0], src[1]);
  __m128i sum_hi = _mm_madd_epi16(src_hi_01, taps[0]);
  if (num_taps > 2) {
    const __m128i src_lo_23 = _mm_unpacklo_epi16(src[2], src[3]);
    sum_lo = _mm_add_epi32(sum_lo, _mm_madd_epi16(src_lo_23, taps[1]));
    const __m128i src_hi_23 = _mm_unpackhi_epi16(src[2], src[3]);
    sum_hi = _mm_add_epi32(sum_hi, _mm_madd_epi16(src_hi_23, taps[1]));
  }
  if (num_taps > 4) {
    const __m128i src_lo_45 = _mm_unpacklo_epi16(src[4], src[5]);
    sum_lo = _mm_add_epi32(sum_lo, _mm_madd_epi16(src_lo_45, taps[2]));
    const __m128i src_hi_45 = _mm_unpackhi_epi16(src[4], src[5]);
    sum_hi = _mm_add_epi32(sum_hi, _mm_madd_epi16(src_hi_45, taps[2]));
  }
  if (num_taps > 6) {
    const __m128i src_lo_67 = _mm_unpacklo_epi16(src[6], src[7]);
    sum_lo = _mm_add_epi32(sum_lo, _mm_madd_epi16(src_lo_67, taps[3]));
    const __m128i src_hi_67 = _mm_unpackhi_epi16(src[6], src[7]);
    sum_hi = _mm_add_epi32(sum_hi, _mm_madd_epi16(src_hi_67, taps[3]));
  }
  if (is_compound) {
    return _mm_packs_epi32(
        RightShiftWithRounding_S32(sum_lo, kInterRoundBitsCompoundVertical - 1),
        RightShiftWithRounding_S32(sum_hi,
                                   kInterRoundBitsCompoundVertical - 1));
  }
  return _mm_packs_epi32(
      RightShiftWithRounding_S32(sum_lo, kInterRoundBitsVertical - 1),
      RightShiftWithRounding_S32(sum_hi, kInterRoundBitsVertical - 1));
}

// Bottom half of each src[k] is the source for one filter, and the top half
// is the source for the other filter, for the next destination row.
template <int num_taps, bool is_compound>
__m128i Sum2DVerticalTaps4x2(const __m128i* const src, const __m128i* taps_lo,
                             const __m128i* taps_hi) {
  const __m128i src_lo_01 = _mm_unpacklo_epi16(src[0], src[1]);
  __m128i sum_lo = _mm_madd_epi16(src_lo_01, taps_lo[0]);
  const __m128i src_hi_01 = _mm_unpackhi_epi16(src[0], src[1]);
  __m128i sum_hi = _mm_madd_epi16(src_hi_01, taps_hi[0]);
  if (num_taps > 2) {
    const __m128i src_lo_23 = _mm_unpacklo_epi16(src[2], src[3]);
    sum_lo = _mm_add_epi32(sum_lo, _mm_madd_epi16(src_lo_23, taps_lo[1]));
    const __m128i src_hi_23 = _mm_unpackhi_epi16(src[2], src[3]);
    sum_hi = _mm_add_epi32(sum_hi, _mm_madd_epi16(src_hi_23, taps_hi[1]));
  }
  if (num_taps > 4) {
    const __m128i src_lo_45 = _mm_unpacklo_epi16(src[4], src[5]);
    sum_lo = _mm_add_epi32(sum_lo, _mm_madd_epi16(src_lo_45, taps_lo[2]));
    const __m128i src_hi_45 = _mm_unpackhi_epi16(src[4], src[5]);
    sum_hi = _mm_add_epi32(sum_hi, _mm_madd_epi16(src_hi_45, taps_hi[2]));
  }
  if (num_taps > 6) {
    const __m128i src_lo_67 = _mm_unpacklo_epi16(src[6], src[7]);
    sum_lo = _mm_add_epi32(sum_lo, _mm_madd_epi16(src_lo_67, taps_lo[3]));
    const __m128i src_hi_67 = _mm_unpackhi_epi16(src[6], src[7]);
    sum_hi = _mm_add_epi32(sum_hi, _mm_madd_epi16(src_hi_67, taps_hi[3]));
  }

  if (is_compound) {
    return _mm_packs_epi32(
        RightShiftWithRounding_S32(sum_lo, kInterRoundBitsCompoundVertical - 1),
        RightShiftWithRounding_S32(sum_hi,
                                   kInterRoundBitsCompoundVertical - 1));
  }
  return _mm_packs_epi32(
      RightShiftWithRounding_S32(sum_lo, kInterRoundBitsVertical - 1),
      RightShiftWithRounding_S32(sum_hi, kInterRoundBitsVertical - 1));
}

// |width_class| is 2, 4, or 8, according to the Store function that should be
// used.
template <int num_taps, int width_class, bool is_compound>
#if LIBGAV1_MSAN
__attribute__((no_sanitize_memory)) void ConvolveVerticalScale(
#else
inline void ConvolveVerticalScale(
#endif
    const int16_t* src, const int width, const int subpixel_y,
    const int filter_index, const int step_y, const int height, void* dest,
    const ptrdiff_t dest_stride) {
  constexpr ptrdiff_t src_stride = kIntermediateStride;
  constexpr int kernel_offset = (8 - num_taps) / 2;
  const int16_t* src_y = src;
  // |dest| is 16-bit in compound mode, Pixel otherwise.
  auto* dest16_y = static_cast<uint16_t*>(dest);
  auto* dest_y = static_cast<uint8_t*>(dest);
  __m128i s[num_taps];

  int p = subpixel_y & 1023;
  int y = height;
  if (width_class <= 4) {
    __m128i filter_taps_lo[num_taps >> 1];
    __m128i filter_taps_hi[num_taps >> 1];
    do {  // y > 0
      for (int i = 0; i < num_taps; ++i) {
        s[i] = LoadLo8(src_y + i * src_stride);
      }
      int filter_id = (p >> 6) & kSubPixelMask;
      const int8_t* filter0 =
          kHalfSubPixelFilters[filter_index][filter_id] + kernel_offset;
      PrepareVerticalTaps<num_taps>(filter0, filter_taps_lo);
      p += step_y;
      src_y = src + (p >> kScaleSubPixelBits) * src_stride;

      for (int i = 0; i < num_taps; ++i) {
        s[i] = LoadHi8(s[i], src_y + i * src_stride);
      }
      filter_id = (p >> 6) & kSubPixelMask;
      const int8_t* filter1 =
          kHalfSubPixelFilters[filter_index][filter_id] + kernel_offset;
      PrepareVerticalTaps<num_taps>(filter1, filter_taps_hi);
      p += step_y;
      src_y = src + (p >> kScaleSubPixelBits) * src_stride;

      const __m128i sums = Sum2DVerticalTaps4x2<num_taps, is_compound>(
          s, filter_taps_lo, filter_taps_hi);
      if (is_compound) {
        assert(width_class > 2);
        StoreLo8(dest16_y, sums);
        dest16_y += dest_stride;
        StoreHi8(dest16_y, sums);
        dest16_y += dest_stride;
      } else {
        const __m128i result = _mm_packus_epi16(sums, sums);
        if (width_class == 2) {
          Store2(dest_y, result);
          dest_y += dest_stride;
          Store2(dest_y, _mm_srli_si128(result, 4));
        } else {
          Store4(dest_y, result);
          dest_y += dest_stride;
          Store4(dest_y, _mm_srli_si128(result, 4));
        }
        dest_y += dest_stride;
      }
      y -= 2;
    } while (y != 0);
    return;
  }

  // |width_class| >= 8
  __m128i filter_taps[num_taps >> 1];
  do {  // y > 0
    src_y = src + (p >> kScaleSubPixelBits) * src_stride;
    const int filter_id = (p >> 6) & kSubPixelMask;
    const int8_t* filter =
        kHalfSubPixelFilters[filter_index][filter_id] + kernel_offset;
    PrepareVerticalTaps<num_taps>(filter, filter_taps);

    int x = 0;
    do {  // x < width
      for (int i = 0; i < num_taps; ++i) {
        s[i] = LoadUnaligned16(src_y + i * src_stride);
      }

      const __m128i sums =
          Sum2DVerticalTaps<num_taps, is_compound>(s, filter_taps);
      if (is_compound) {
        StoreUnaligned16(dest16_y + x, sums);
      } else {
        StoreLo8(dest_y + x, _mm_packus_epi16(sums, sums));
      }
      x += 8;
      src_y += 8;
    } while (x < width);
    p += step_y;
    dest_y += dest_stride;
    dest16_y += dest_stride;
  } while (--y != 0);
}

template <bool is_compound>
void ConvolveScale2D_SSE4_1(const void* const reference,
                            const ptrdiff_t reference_stride,
                            const int horizontal_filter_index,
                            const int vertical_filter_index,
                            const int subpixel_x, const int subpixel_y,
                            const int step_x, const int step_y, const int width,
                            const int height, void* prediction,
                            const ptrdiff_t pred_stride) {
  const int horiz_filter_index = GetFilterIndex(horizontal_filter_index, width);
  const int vert_filter_index = GetFilterIndex(vertical_filter_index, height);
  assert(step_x <= 2048);
  // The output of the horizontal filter, i.e. the intermediate_result, is
  // guaranteed to fit in int16_t.
  // TODO(petersonab): Reduce intermediate block stride to width to make smaller
  // blocks faster.
  alignas(16) int16_t
      intermediate_result[kMaxSuperBlockSizeInPixels *
                          (2 * kMaxSuperBlockSizeInPixels + kSubPixelTaps)];
  const int num_vert_taps = GetNumTapsInFilter(vert_filter_index);
  const int intermediate_height =
      (((height - 1) * step_y + (1 << kScaleSubPixelBits) - 1) >>
       kScaleSubPixelBits) +
      num_vert_taps;

  // Horizontal filter.
  // Filter types used for width <= 4 are different from those for width > 4.
  // When width > 4, the valid filter index range is always [0, 3].
  // When width <= 4, the valid filter index range is always [3, 5].
  // Similarly for height.
  int16_t* intermediate = intermediate_result;
  const ptrdiff_t src_stride = reference_stride;
  const auto* src = static_cast<const uint8_t*>(reference);
  const int vert_kernel_offset = (8 - num_vert_taps) / 2;
  src += vert_kernel_offset * src_stride;

  // Derive the maximum value of |step_x| at which all source values fit in one
  // 16-byte load. Final index is src_x + |num_taps| - 1 < 16
  // step_x*7 is the final base sub-pixel index for the shuffle mask for filter
  // inputs in each iteration on large blocks. When step_x is large, we need a
  // second register and alignr in order to gather all filter inputs.
  // |num_taps| - 1 is the offset for the shuffle of inputs to the final tap.
  const int num_horiz_taps = GetNumTapsInFilter(horiz_filter_index);
  const int kernel_start_ceiling = 16 - num_horiz_taps;
  // This truncated quotient |grade_x_threshold| selects |step_x| such that:
  // (step_x * 7) >> kScaleSubPixelBits < single load limit
  const int grade_x_threshold =
      (kernel_start_ceiling << kScaleSubPixelBits) / 7;
  switch (horiz_filter_index) {
    case 0:
      if (step_x > grade_x_threshold) {
        ConvolveHorizontalScale<2, 0, 6>(src, src_stride, width, subpixel_x,
                                         step_x, intermediate_height,
                                         intermediate);
      } else {
        ConvolveHorizontalScale<1, 0, 6>(src, src_stride, width, subpixel_x,
                                         step_x, intermediate_height,
                                         intermediate);
      }
      break;
    case 1:
      if (step_x > grade_x_threshold) {
        ConvolveHorizontalScale<2, 1, 6>(src, src_stride, width, subpixel_x,
                                         step_x, intermediate_height,
                                         intermediate);

      } else {
        ConvolveHorizontalScale<1, 1, 6>(src, src_stride, width, subpixel_x,
                                         step_x, intermediate_height,
                                         intermediate);
      }
      break;
    case 2:
      if (step_x > grade_x_threshold) {
        ConvolveHorizontalScale<2, 2, 8>(src, src_stride, width, subpixel_x,
                                         step_x, intermediate_height,
                                         intermediate);
      } else {
        ConvolveHorizontalScale<1, 2, 8>(src, src_stride, width, subpixel_x,
                                         step_x, intermediate_height,
                                         intermediate);
      }
      break;
    case 3:
      if (step_x > grade_x_threshold) {
        ConvolveHorizontalScale<2, 3, 2>(src, src_stride, width, subpixel_x,
                                         step_x, intermediate_height,
                                         intermediate);
      } else {
        ConvolveHorizontalScale<1, 3, 2>(src, src_stride, width, subpixel_x,
                                         step_x, intermediate_height,
                                         intermediate);
      }
      break;
    case 4:
      assert(width <= 4);
      ConvolveHorizontalScale<1, 4, 4>(src, src_stride, width, subpixel_x,
                                       step_x, intermediate_height,
                                       intermediate);
      break;
    default:
      assert(horiz_filter_index == 5);
      assert(width <= 4);
      ConvolveHorizontalScale<1, 5, 4>(src, src_stride, width, subpixel_x,
                                       step_x, intermediate_height,
                                       intermediate);
  }

  // Vertical filter.
  intermediate = intermediate_result;
  switch (vert_filter_index) {
    case 0:
    case 1:
      if (!is_compound && width == 2) {
        ConvolveVerticalScale<6, 2, is_compound>(
            intermediate, width, subpixel_y, vert_filter_index, step_y, height,
            prediction, pred_stride);
      } else if (width == 4) {
        ConvolveVerticalScale<6, 4, is_compound>(
            intermediate, width, subpixel_y, vert_filter_index, step_y, height,
            prediction, pred_stride);
      } else {
        ConvolveVerticalScale<6, 8, is_compound>(
            intermediate, width, subpixel_y, vert_filter_index, step_y, height,
            prediction, pred_stride);
      }
      break;
    case 2:
      if (!is_compound && width == 2) {
        ConvolveVerticalScale<8, 2, is_compound>(
            intermediate, width, subpixel_y, vert_filter_index, step_y, height,
            prediction, pred_stride);
      } else if (width == 4) {
        ConvolveVerticalScale<8, 4, is_compound>(
            intermediate, width, subpixel_y, vert_filter_index, step_y, height,
            prediction, pred_stride);
      } else {
        ConvolveVerticalScale<8, 8, is_compound>(
            intermediate, width, subpixel_y, vert_filter_index, step_y, height,
            prediction, pred_stride);
      }
      break;
    case 3:
      if (!is_compound && width == 2) {
        ConvolveVerticalScale<2, 2, is_compound>(
            intermediate, width, subpixel_y, vert_filter_index, step_y, height,
            prediction, pred_stride);
      } else if (width == 4) {
        ConvolveVerticalScale<2, 4, is_compound>(
            intermediate, width, subpixel_y, vert_filter_index, step_y, height,
            prediction, pred_stride);
      } else {
        ConvolveVerticalScale<2, 8, is_compound>(
            intermediate, width, subpixel_y, vert_filter_index, step_y, height,
            prediction, pred_stride);
      }
      break;
    default:
      assert(vert_filter_index == 4 || vert_filter_index == 5);
      if (!is_compound && width == 2) {
        ConvolveVerticalScale<4, 2, is_compound>(
            intermediate, width, subpixel_y, vert_filter_index, step_y, height,
            prediction, pred_stride);
      } else if (width == 4) {
        ConvolveVerticalScale<4, 4, is_compound>(
            intermediate, width, subpixel_y, vert_filter_index, step_y, height,
            prediction, pred_stride);
      } else {
        ConvolveVerticalScale<4, 8, is_compound>(
            intermediate, width, subpixel_y, vert_filter_index, step_y, height,
            prediction, pred_stride);
      }
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
  dsp->convolve[0][0][0][1] = ConvolveHorizontal_SSE4_1;
  dsp->convolve[0][0][1][0] = ConvolveVertical_SSE4_1;
  dsp->convolve[0][0][1][1] = Convolve2D_SSE4_1;

  dsp->convolve[0][1][0][0] = ConvolveCompoundCopy_SSE4;
  dsp->convolve[0][1][0][1] = ConvolveCompoundHorizontal_SSE4_1;
  dsp->convolve[0][1][1][0] = ConvolveCompoundVertical_SSE4_1;
  dsp->convolve[0][1][1][1] = ConvolveCompound2D_SSE4_1;

  dsp->convolve_scale[0] = ConvolveScale2D_SSE4_1<false>;
  dsp->convolve_scale[1] = ConvolveScale2D_SSE4_1<true>;
}

}  // namespace
}  // namespace low_bitdepth

void ConvolveInit_SSE4_1() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else  // !LIBGAV1_ENABLE_SSE4_1
namespace libgav1 {
namespace dsp {

void ConvolveInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
