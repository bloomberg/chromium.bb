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

#include "src/dsp/loop_restoration.h"
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_SSE4_1
#include <smmintrin.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "src/dsp/common.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/dsp/x86/common_sse4.h"
#include "src/utils/common.h"
#include "src/utils/compiler_attributes.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

// Precision of a division table (mtable)
constexpr int kSgrProjScaleBits = 20;
constexpr int kSgrProjReciprocalBits = 12;
// Core self-guided restoration precision bits.
constexpr int kSgrProjSgrBits = 8;
// Precision bits of generated values higher than source before projection.
constexpr int kSgrProjRestoreBits = 4;

// Note: range of wiener filter coefficients.
// Wiener filter coefficients are symmetric, and their sum is 1 (128).
// The range of each coefficient:
// filter[0] = filter[6], 4 bits, min = -5, max = 10.
// filter[1] = filter[5], 5 bits, min = -23, max = 8.
// filter[2] = filter[4], 6 bits, min = -17, max = 46.
// filter[3] = 128 - (filter[0] + filter[1] + filter[2]) * 2.
// int8_t is used for the sse4 code, so in order to fit in an int8_t, the 128
// offset must be removed from filter[3].
// filter[3] = 0 - (filter[0] + filter[1] + filter[2]) * 2.
// The 128 offset will be added back in the loop.
inline void PopulateWienerCoefficients(
    const RestorationUnitInfo& restoration_info, int direction,
    int8_t* const filter) {
  filter[3] = 0;
  for (int i = 0; i < 3; ++i) {
    const int8_t coeff = restoration_info.wiener_info.filter[direction][i];
    filter[i] = coeff;
    filter[6 - i] = coeff;
    filter[3] -= MultiplyBy2(coeff);
  }

  // The Wiener filter has only 7 coefficients, but we run it as an 8-tap
  // filter in SIMD. The 8th coefficient of the filter must be set to 0.
  filter[7] = 0;
}

// This function calls LoadUnaligned16() to read 10 bytes from the |source|
// buffer. Since the LoadUnaligned16() call over-reads 6 bytes, the |source|
// buffer must be at least (height + kSubPixelTaps - 2) * source_stride + 6
// bytes long.
void WienerFilter_SSE4_1(const void* source, void* const dest,
                         const RestorationUnitInfo& restoration_info,
                         ptrdiff_t source_stride, ptrdiff_t dest_stride,
                         int width, int height,
                         RestorationBuffer* const buffer) {
  int8_t filter[kSubPixelTaps];
  const int limit =
      (1 << (8 + 1 + kWienerFilterBits - kInterRoundBitsHorizontal)) - 1;
  const auto* src = static_cast<const uint8_t*>(source);
  auto* dst = static_cast<uint8_t*>(dest);
  const ptrdiff_t buffer_stride = buffer->wiener_buffer_stride;
  auto* wiener_buffer = buffer->wiener_buffer;
  // horizontal filtering.
  PopulateWienerCoefficients(restoration_info, WienerInfo::kHorizontal, filter);
  const int center_tap = 3;
  src -= center_tap * source_stride + center_tap;

  const int horizontal_rounding =
      1 << (8 + kWienerFilterBits - kInterRoundBitsHorizontal - 1);
  const __m128i v_horizontal_rounding =
      _mm_shufflelo_epi16(_mm_cvtsi32_si128(horizontal_rounding), 0);
  const __m128i v_limit = _mm_shufflelo_epi16(_mm_cvtsi32_si128(limit), 0);
  const __m128i v_horizontal_filter = LoadLo8(filter);
  __m128i v_k1k0 = _mm_shufflelo_epi16(v_horizontal_filter, 0x0);
  __m128i v_k3k2 = _mm_shufflelo_epi16(v_horizontal_filter, 0x55);
  __m128i v_k5k4 = _mm_shufflelo_epi16(v_horizontal_filter, 0xaa);
  __m128i v_k7k6 = _mm_shufflelo_epi16(v_horizontal_filter, 0xff);
  const __m128i v_round_0 = _mm_shufflelo_epi16(
      _mm_cvtsi32_si128(1 << (kInterRoundBitsHorizontal - 1)), 0);
  const __m128i v_round_0_shift = _mm_cvtsi32_si128(kInterRoundBitsHorizontal);
  const __m128i v_offset_shift =
      _mm_cvtsi32_si128(7 - kInterRoundBitsHorizontal);

  int y = 0;
  do {
    int x = 0;
    do {
      // Run the Wiener filter on four sets of source samples at a time:
      //   src[x + 0] ... src[x + 6]
      //   src[x + 1] ... src[x + 7]
      //   src[x + 2] ... src[x + 8]
      //   src[x + 3] ... src[x + 9]

      // Read 10 bytes (from src[x] to src[x + 9]). We over-read 6 bytes but
      // their results are discarded.
      const __m128i v_src = LoadUnaligned16(&src[x]);
      const __m128i v_src_dup_lo = _mm_unpacklo_epi8(v_src, v_src);
      const __m128i v_src_dup_hi = _mm_unpackhi_epi8(v_src, v_src);
      const __m128i v_src_10 = _mm_alignr_epi8(v_src_dup_hi, v_src_dup_lo, 1);
      const __m128i v_src_32 = _mm_alignr_epi8(v_src_dup_hi, v_src_dup_lo, 5);
      const __m128i v_src_54 = _mm_alignr_epi8(v_src_dup_hi, v_src_dup_lo, 9);
      // Shift right by 12 bytes instead of 13 bytes so that src[x + 10] is not
      // shifted into the low 8 bytes of v_src_66.
      const __m128i v_src_66 = _mm_alignr_epi8(v_src_dup_hi, v_src_dup_lo, 12);
      const __m128i v_madd_10 = _mm_maddubs_epi16(v_src_10, v_k1k0);
      const __m128i v_madd_32 = _mm_maddubs_epi16(v_src_32, v_k3k2);
      const __m128i v_madd_54 = _mm_maddubs_epi16(v_src_54, v_k5k4);
      const __m128i v_madd_76 = _mm_maddubs_epi16(v_src_66, v_k7k6);
      const __m128i v_sum_3210 = _mm_add_epi16(v_madd_10, v_madd_32);
      const __m128i v_sum_7654 = _mm_add_epi16(v_madd_54, v_madd_76);
      // The sum range here is [-128 * 255, 90 * 255].
      const __m128i v_sum_76543210 = _mm_add_epi16(v_sum_7654, v_sum_3210);
      const __m128i v_sum = _mm_add_epi16(v_sum_76543210, v_round_0);
      const __m128i v_rounded_sum0 = _mm_sra_epi16(v_sum, v_round_0_shift);
      // Add scaled down horizontal round here to prevent signed 16 bit
      // outranging
      const __m128i v_rounded_sum1 =
          _mm_add_epi16(v_rounded_sum0, v_horizontal_rounding);
      // Zero out the even bytes, calculate scaled down offset correction, and
      // add to sum here to prevent signed 16 bit outranging.
      // (src[3] * 128) >> kInterRoundBitsHorizontal
      const __m128i v_src_3x128 =
          _mm_sll_epi16(_mm_srli_epi16(v_src_32, 8), v_offset_shift);
      const __m128i v_rounded_sum = _mm_add_epi16(v_rounded_sum1, v_src_3x128);
      const __m128i v_a = _mm_max_epi16(v_rounded_sum, _mm_setzero_si128());
      const __m128i v_b = _mm_min_epi16(v_a, v_limit);
      StoreLo8(&wiener_buffer[x], v_b);
      x += 4;
    } while (x < width);
    src += source_stride;
    wiener_buffer += buffer_stride;
  } while (++y < height + kSubPixelTaps - 2);

  wiener_buffer = buffer->wiener_buffer;
  // vertical filtering.
  PopulateWienerCoefficients(restoration_info, WienerInfo::kVertical, filter);

  const int vertical_rounding = -(1 << (8 + kInterRoundBitsVertical - 1));
  const __m128i v_vertical_rounding =
      _mm_shuffle_epi32(_mm_cvtsi32_si128(vertical_rounding), 0);
  const __m128i v_offset_correction = _mm_set_epi16(0, 0, 0, 0, 128, 0, 0, 0);
  const __m128i v_round_1 = _mm_shuffle_epi32(
      _mm_cvtsi32_si128(1 << (kInterRoundBitsVertical - 1)), 0);
  const __m128i v_round_1_shift = _mm_cvtsi32_si128(kInterRoundBitsVertical);
  const __m128i v_vertical_filter0 = _mm_cvtepi8_epi16(LoadLo8(filter));
  const __m128i v_vertical_filter =
      _mm_add_epi16(v_vertical_filter0, v_offset_correction);
  v_k1k0 = _mm_shuffle_epi32(v_vertical_filter, 0x0);
  v_k3k2 = _mm_shuffle_epi32(v_vertical_filter, 0x55);
  v_k5k4 = _mm_shuffle_epi32(v_vertical_filter, 0xaa);
  v_k7k6 = _mm_shuffle_epi32(v_vertical_filter, 0xff);
  y = 0;
  do {
    int x = 0;
    do {
      const __m128i v_wb_0 = LoadLo8(&wiener_buffer[0 * buffer_stride + x]);
      const __m128i v_wb_1 = LoadLo8(&wiener_buffer[1 * buffer_stride + x]);
      const __m128i v_wb_2 = LoadLo8(&wiener_buffer[2 * buffer_stride + x]);
      const __m128i v_wb_3 = LoadLo8(&wiener_buffer[3 * buffer_stride + x]);
      const __m128i v_wb_4 = LoadLo8(&wiener_buffer[4 * buffer_stride + x]);
      const __m128i v_wb_5 = LoadLo8(&wiener_buffer[5 * buffer_stride + x]);
      const __m128i v_wb_6 = LoadLo8(&wiener_buffer[6 * buffer_stride + x]);
      const __m128i v_wb_10 = _mm_unpacklo_epi16(v_wb_0, v_wb_1);
      const __m128i v_wb_32 = _mm_unpacklo_epi16(v_wb_2, v_wb_3);
      const __m128i v_wb_54 = _mm_unpacklo_epi16(v_wb_4, v_wb_5);
      const __m128i v_wb_76 = _mm_unpacklo_epi16(v_wb_6, _mm_setzero_si128());
      const __m128i v_madd_10 = _mm_madd_epi16(v_wb_10, v_k1k0);
      const __m128i v_madd_32 = _mm_madd_epi16(v_wb_32, v_k3k2);
      const __m128i v_madd_54 = _mm_madd_epi16(v_wb_54, v_k5k4);
      const __m128i v_madd_76 = _mm_madd_epi16(v_wb_76, v_k7k6);
      const __m128i v_sum_3210 = _mm_add_epi32(v_madd_10, v_madd_32);
      const __m128i v_sum_7654 = _mm_add_epi32(v_madd_54, v_madd_76);
      const __m128i v_sum_76543210 = _mm_add_epi32(v_sum_7654, v_sum_3210);
      const __m128i v_sum = _mm_add_epi32(v_sum_76543210, v_vertical_rounding);
      const __m128i v_rounded_sum =
          _mm_sra_epi32(_mm_add_epi32(v_sum, v_round_1), v_round_1_shift);
      const __m128i v_a = _mm_packs_epi32(v_rounded_sum, v_rounded_sum);
      const __m128i v_b = _mm_packus_epi16(v_a, v_a);
      Store4(&dst[x], v_b);
      x += 4;
    } while (x < width);
    dst += dest_stride;
    wiener_buffer += buffer_stride;
  } while (++y < height);
}

// Section 7.17.3.
// a2: range [1, 256].
// if (z >= 255)
//   a2 = 256;
// else if (z == 0)
//   a2 = 1;
// else
//   a2 = ((z << kSgrProjSgrBits) + (z >> 1)) / (z + 1);
constexpr int kXByXPlus1[256] = {
    1,   128, 171, 192, 205, 213, 219, 224, 228, 230, 233, 235, 236, 238, 239,
    240, 241, 242, 243, 243, 244, 244, 245, 245, 246, 246, 247, 247, 247, 247,
    248, 248, 248, 248, 249, 249, 249, 249, 249, 250, 250, 250, 250, 250, 250,
    250, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 252, 252, 252, 252,
    252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 253, 253,
    253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
    253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    256};

inline __m128i HorizontalAddVerticalSumsRadius1(const uint32_t* vert_sums) {
  // Horizontally add vertical sums to get total box sum.
  const __m128i v_sums_3210 = LoadUnaligned16(&vert_sums[0]);
  const __m128i v_sums_7654 = LoadUnaligned16(&vert_sums[4]);
  const __m128i v_sums_4321 = _mm_alignr_epi8(v_sums_7654, v_sums_3210, 4);
  const __m128i v_sums_5432 = _mm_alignr_epi8(v_sums_7654, v_sums_3210, 8);
  const __m128i v_s0 = _mm_add_epi32(v_sums_3210, v_sums_4321);
  const __m128i v_s1 = _mm_add_epi32(v_s0, v_sums_5432);
  return v_s1;
}

inline __m128i HorizontalAddVerticalSumsRadius2(const uint32_t* vert_sums) {
  // Horizontally add vertical sums to get total box sum.
  const __m128i v_sums_3210 = LoadUnaligned16(&vert_sums[0]);
  const __m128i v_sums_7654 = LoadUnaligned16(&vert_sums[4]);
  const __m128i v_sums_4321 = _mm_alignr_epi8(v_sums_7654, v_sums_3210, 4);
  const __m128i v_sums_5432 = _mm_alignr_epi8(v_sums_7654, v_sums_3210, 8);
  const __m128i v_sums_6543 = _mm_alignr_epi8(v_sums_7654, v_sums_3210, 12);
  const __m128i v_s0 = _mm_add_epi32(v_sums_3210, v_sums_4321);
  const __m128i v_s1 = _mm_add_epi32(v_s0, v_sums_5432);
  const __m128i v_s2 = _mm_add_epi32(v_s1, v_sums_6543);
  const __m128i v_s3 = _mm_add_epi32(v_s2, v_sums_7654);
  return v_s3;
}

void BoxFilterPreProcessRadius1_SSE4_1(
    const uint8_t* const src, ptrdiff_t stride, int width, int height,
    uint32_t s, uint32_t* intermediate_result[2], ptrdiff_t array_stride,
    uint32_t* vertical_sums, uint32_t* vertical_sum_of_squares) {
  assert(s != 0);
  const uint32_t n = 9;
  const uint32_t one_over_n = ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;
  const __m128i v_one_over_n =
      _mm_shuffle_epi32(_mm_cvtsi32_si128(one_over_n), 0);
  const __m128i v_sgrbits =
      _mm_shuffle_epi32(_mm_cvtsi32_si128(1 << kSgrProjSgrBits), 0);

#if LIBGAV1_MSAN
  // Over-reads occur in the x loop, so set to a known value.
  memset(&vertical_sums[width], 0, 8 * sizeof(vertical_sums[0]));
  memset(&vertical_sum_of_squares[width], 0,
         8 * sizeof(vertical_sum_of_squares[0]));
#endif

  // Calculate intermediate results, including one-pixel border, for example,
  // if unit size is 64x64, we calculate 66x66 pixels.
  int y = -1;
  do {
    const uint8_t* top_left = &src[(y - 1) * stride - 2];
    // Calculate the box vertical sums for each x position.
    int vsx = -2;
    do {
      const __m128i v_box0 = _mm_cvtepu8_epi32(Load4(top_left));
      const __m128i v_box1 = _mm_cvtepu8_epi32(Load4(top_left + stride));
      const __m128i v_box2 = _mm_cvtepu8_epi32(Load4(top_left + stride * 2));
      const __m128i v_sqr0 = _mm_mullo_epi32(v_box0, v_box0);
      const __m128i v_sqr1 = _mm_mullo_epi32(v_box1, v_box1);
      const __m128i v_sqr2 = _mm_mullo_epi32(v_box2, v_box2);
      const __m128i v_a01 = _mm_add_epi32(v_sqr0, v_sqr1);
      const __m128i v_a012 = _mm_add_epi32(v_a01, v_sqr2);
      const __m128i v_b01 = _mm_add_epi32(v_box0, v_box1);
      const __m128i v_b012 = _mm_add_epi32(v_b01, v_box2);
      StoreUnaligned16(&vertical_sum_of_squares[vsx], v_a012);
      StoreUnaligned16(&vertical_sums[vsx], v_b012);
      top_left += 4;
      vsx += 4;
    } while (vsx <= width + 1);

    int x = -1;
    do {
      const __m128i v_a =
          HorizontalAddVerticalSumsRadius1(&vertical_sum_of_squares[x - 1]);
      const __m128i v_b =
          HorizontalAddVerticalSumsRadius1(&vertical_sums[x - 1]);
      // -----------------------
      // calc p, z, a2
      // -----------------------
      const __m128i v_255 = _mm_shuffle_epi32(_mm_cvtsi32_si128(255), 0);
      const __m128i v_n = _mm_shuffle_epi32(_mm_cvtsi32_si128(n), 0);
      const __m128i v_s = _mm_shuffle_epi32(_mm_cvtsi32_si128(s), 0);
      const __m128i v_dxd = _mm_mullo_epi32(v_b, v_b);
      const __m128i v_axn = _mm_mullo_epi32(v_a, v_n);
      const __m128i v_p = _mm_sub_epi32(v_axn, v_dxd);
      const __m128i v_z = _mm_min_epi32(
          v_255, RightShiftWithRounding_U32(_mm_mullo_epi32(v_p, v_s),
                                            kSgrProjScaleBits));
      const __m128i v_a2 = _mm_set_epi32(kXByXPlus1[_mm_extract_epi32(v_z, 3)],
                                         kXByXPlus1[_mm_extract_epi32(v_z, 2)],
                                         kXByXPlus1[_mm_extract_epi32(v_z, 1)],
                                         kXByXPlus1[_mm_extract_epi32(v_z, 0)]);
      // -----------------------
      // calc b2 and store
      // -----------------------
      const __m128i v_sgrbits_sub_a2 = _mm_sub_epi32(v_sgrbits, v_a2);
      const __m128i v_b2 =
          _mm_mullo_epi32(v_sgrbits_sub_a2, _mm_mullo_epi32(v_b, v_one_over_n));
      StoreUnaligned16(&intermediate_result[0][x], v_a2);
      StoreUnaligned16(
          &intermediate_result[1][x],
          RightShiftWithRounding_U32(v_b2, kSgrProjReciprocalBits));
      x += 4;
    } while (x <= width);
    intermediate_result[0] += array_stride;
    intermediate_result[1] += array_stride;
  } while (++y <= height);
}

void BoxFilterPreProcessRadius2_SSE4_1(
    const uint8_t* const src, ptrdiff_t stride, int width, int height,
    uint32_t s, uint32_t* intermediate_result[2], ptrdiff_t array_stride,
    uint32_t* vertical_sums, uint32_t* vertical_sum_of_squares) {
  assert(s != 0);
  const uint32_t n = 25;
  const uint32_t one_over_n = ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;
  const __m128i v_one_over_n =
      _mm_shuffle_epi32(_mm_cvtsi32_si128(one_over_n), 0);
  const __m128i v_sgrbits =
      _mm_shuffle_epi32(_mm_cvtsi32_si128(1 << kSgrProjSgrBits), 0);

  // Calculate intermediate results, including one-pixel border, for example,
  // if unit size is 64x64, we calculate 66x66 pixels.
  int y = -1;
  do {
    // Calculate the box vertical sums for each x position.
    const uint8_t* top_left = &src[(y - 2) * stride - 3];
    int vsx = -3;
    do {
      const __m128i v_box0 = _mm_cvtepu8_epi32(Load4(top_left));
      const __m128i v_box1 = _mm_cvtepu8_epi32(Load4(top_left + stride));
      const __m128i v_box2 = _mm_cvtepu8_epi32(Load4(top_left + stride * 2));
      const __m128i v_box3 = _mm_cvtepu8_epi32(Load4(top_left + stride * 3));
      const __m128i v_box4 = _mm_cvtepu8_epi32(Load4(top_left + stride * 4));
      const __m128i v_sqr0 = _mm_mullo_epi32(v_box0, v_box0);
      const __m128i v_sqr1 = _mm_mullo_epi32(v_box1, v_box1);
      const __m128i v_sqr2 = _mm_mullo_epi32(v_box2, v_box2);
      const __m128i v_sqr3 = _mm_mullo_epi32(v_box3, v_box3);
      const __m128i v_sqr4 = _mm_mullo_epi32(v_box4, v_box4);
      const __m128i v_a01 = _mm_add_epi32(v_sqr0, v_sqr1);
      const __m128i v_a012 = _mm_add_epi32(v_a01, v_sqr2);
      const __m128i v_a0123 = _mm_add_epi32(v_a012, v_sqr3);
      const __m128i v_a01234 = _mm_add_epi32(v_a0123, v_sqr4);
      const __m128i v_b01 = _mm_add_epi32(v_box0, v_box1);
      const __m128i v_b012 = _mm_add_epi32(v_b01, v_box2);
      const __m128i v_b0123 = _mm_add_epi32(v_b012, v_box3);
      const __m128i v_b01234 = _mm_add_epi32(v_b0123, v_box4);
      StoreUnaligned16(&vertical_sum_of_squares[vsx], v_a01234);
      StoreUnaligned16(&vertical_sums[vsx], v_b01234);
      top_left += 4;
      vsx += 4;
    } while (vsx <= width + 2);

    int x = -1;
    do {
      const __m128i v_a =
          HorizontalAddVerticalSumsRadius2(&vertical_sum_of_squares[x - 2]);
      const __m128i v_b =
          HorizontalAddVerticalSumsRadius2(&vertical_sums[x - 2]);
      // -----------------------
      // calc p, z, a2
      // -----------------------
      const __m128i v_255 = _mm_shuffle_epi32(_mm_cvtsi32_si128(255), 0);
      const __m128i v_n = _mm_shuffle_epi32(_mm_cvtsi32_si128(n), 0);
      const __m128i v_s = _mm_shuffle_epi32(_mm_cvtsi32_si128(s), 0);
      const __m128i v_dxd = _mm_mullo_epi32(v_b, v_b);
      const __m128i v_axn = _mm_mullo_epi32(v_a, v_n);
      const __m128i v_p = _mm_sub_epi32(v_axn, v_dxd);
      const __m128i v_z = _mm_min_epi32(
          v_255, RightShiftWithRounding_U32(_mm_mullo_epi32(v_p, v_s),
                                            kSgrProjScaleBits));
      const __m128i v_a2 = _mm_set_epi32(kXByXPlus1[_mm_extract_epi32(v_z, 3)],
                                         kXByXPlus1[_mm_extract_epi32(v_z, 2)],
                                         kXByXPlus1[_mm_extract_epi32(v_z, 1)],
                                         kXByXPlus1[_mm_extract_epi32(v_z, 0)]);
      // -----------------------
      // calc b2 and store
      // -----------------------
      const __m128i v_sgrbits_sub_a2 = _mm_sub_epi32(v_sgrbits, v_a2);
      const __m128i v_b2 =
          _mm_mullo_epi32(v_sgrbits_sub_a2, _mm_mullo_epi32(v_b, v_one_over_n));
      StoreUnaligned16(&intermediate_result[0][x], v_a2);
      StoreUnaligned16(
          &intermediate_result[1][x],
          RightShiftWithRounding_U32(v_b2, kSgrProjReciprocalBits));
      x += 4;
    } while (x <= width);
    intermediate_result[0] += 2 * array_stride;
    intermediate_result[1] += 2 * array_stride;
    y += 2;
  } while (y <= height);
}

void BoxFilterPreProcess_SSE4_1(const RestorationUnitInfo& restoration_info,
                                const uint8_t* const src, ptrdiff_t stride,
                                int width, int height, int pass,
                                RestorationBuffer* const buffer) {
  uint32_t vertical_sums_buf[kRestorationProcessingUnitSize +
                             2 * kRestorationBorder + kRestorationPadding];
  uint32_t vertical_sum_of_squares_buf[kRestorationProcessingUnitSize +
                                       2 * kRestorationBorder +
                                       kRestorationPadding];
  uint32_t* vertical_sums = &vertical_sums_buf[4];
  uint32_t* vertical_sum_of_squares = &vertical_sum_of_squares_buf[4];
  const ptrdiff_t array_stride = buffer->box_filter_process_intermediate_stride;
  // The size of the intermediate result buffer is the size of the filter area
  // plus horizontal (3) and vertical (3) padding. The processing start point
  // is the filter area start point -1 row and -1 column. Therefore we need to
  // set offset and use the intermediate_result as the start point for
  // processing.
  const ptrdiff_t intermediate_buffer_offset =
      kRestorationBorder * array_stride + kRestorationBorder;
  uint32_t* intermediate_result[2] = {
      buffer->box_filter_process_intermediate[0] + intermediate_buffer_offset -
          array_stride,
      buffer->box_filter_process_intermediate[1] + intermediate_buffer_offset -
          array_stride};
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  if (pass == 0) {
    assert(kSgrProjParams[sgr_proj_index][0] == 2);
    BoxFilterPreProcessRadius2_SSE4_1(src, stride, width, height,
                                      kSgrScaleParameter[sgr_proj_index][0],
                                      intermediate_result, array_stride,
                                      vertical_sums, vertical_sum_of_squares);
  } else {
    assert(kSgrProjParams[sgr_proj_index][2] == 1);
    BoxFilterPreProcessRadius1_SSE4_1(src, stride, width, height,
                                      kSgrScaleParameter[sgr_proj_index][1],
                                      intermediate_result, array_stride,
                                      vertical_sums, vertical_sum_of_squares);
  }
}

inline __m128i Sum565Row(const __m128i v_DBCA, const __m128i v_XXFE) {
  __m128i v_sum = v_DBCA;
  const __m128i v_EDCB = _mm_alignr_epi8(v_XXFE, v_DBCA, 4);
  v_sum = _mm_add_epi32(v_sum, v_EDCB);
  const __m128i v_FEDC = _mm_alignr_epi8(v_XXFE, v_DBCA, 8);
  v_sum = _mm_add_epi32(v_sum, v_FEDC);
  //   D C B A x4
  // + E D C B x4
  // + F E D C x4
  v_sum = _mm_slli_epi32(v_sum, 2);
  // + D C B A
  v_sum = _mm_add_epi32(v_sum, v_DBCA);  // 5
  // + E D C B x2
  v_sum = _mm_add_epi32(v_sum, _mm_slli_epi32(v_EDCB, 1));  // 6
  // + F E D C
  return _mm_add_epi32(v_sum, v_FEDC);  // 5
}

inline __m128i Process3x3Block_565_Odd(const uint32_t* src, ptrdiff_t stride) {
  // 0 0 0
  // 5 6 5
  // 0 0 0
  const uint32_t* top_left = src - 1;
  const __m128i v_src1_lo = LoadUnaligned16(top_left + stride);
  const __m128i v_src1_hi = LoadLo8(top_left + stride + 4);
  return Sum565Row(v_src1_lo, v_src1_hi);
}

inline __m128i Process3x3Block_565_Even(const uint32_t* src, ptrdiff_t stride) {
  // 5 6 5
  // 0 0 0
  // 5 6 5
  const uint32_t* top_left = src - 1;
  const __m128i v_src0_lo = LoadUnaligned16(top_left);
  const __m128i v_src0_hi = LoadLo8(top_left + 4);
  const __m128i v_src2_lo = LoadUnaligned16(top_left + stride * 2);
  const __m128i v_src2_hi = LoadLo8(top_left + stride * 2 + 4);
  const __m128i v_a0 = Sum565Row(v_src0_lo, v_src0_hi);
  const __m128i v_a2 = Sum565Row(v_src2_lo, v_src2_hi);
  return _mm_add_epi32(v_a0, v_a2);
}

inline __m128i Sum343Row(const __m128i v_DBCA, const __m128i v_XXFE) {
  __m128i v_sum = v_DBCA;
  const __m128i v_EDCB = _mm_alignr_epi8(v_XXFE, v_DBCA, 4);
  v_sum = _mm_add_epi32(v_sum, v_EDCB);
  const __m128i v_FEDC = _mm_alignr_epi8(v_XXFE, v_DBCA, 8);
  v_sum = _mm_add_epi32(v_sum, v_FEDC);
  //   D C B A x4
  // + E D C B x4
  // + F E D C x4
  v_sum = _mm_slli_epi32(v_sum, 2);  // 4
  // - D C B A
  v_sum = _mm_sub_epi32(v_sum, v_DBCA);  // 3
  // - F E D C
  return _mm_sub_epi32(v_sum, v_FEDC);  // 3
}

inline __m128i Sum444Row(const __m128i v_DBCA, const __m128i v_XXFE) {
  __m128i v_sum = v_DBCA;
  const __m128i v_EDCB = _mm_alignr_epi8(v_XXFE, v_DBCA, 4);
  v_sum = _mm_add_epi32(v_sum, v_EDCB);
  const __m128i v_FEDC = _mm_alignr_epi8(v_XXFE, v_DBCA, 8);
  v_sum = _mm_add_epi32(v_sum, v_FEDC);
  //   D C B A x4
  // + E D C B x4
  // + F E D C x4
  return _mm_slli_epi32(v_sum, 2);  // 4
}

inline __m128i Process3x3Block_343(const uint32_t* src, ptrdiff_t stride) {
  const uint32_t* top_left = src - 1;
  const __m128i v_ir0_lo = LoadUnaligned16(top_left);
  const __m128i v_ir0_hi = LoadLo8(top_left + 4);
  const __m128i v_ir1_lo = LoadUnaligned16(top_left + stride);
  const __m128i v_ir1_hi = LoadLo8(top_left + stride + 4);
  const __m128i v_ir2_lo = LoadUnaligned16(top_left + stride * 2);
  const __m128i v_ir2_hi = LoadLo8(top_left + stride * 2 + 4);
  const __m128i v_a0 = Sum343Row(v_ir0_lo, v_ir0_hi);
  const __m128i v_a1 = Sum444Row(v_ir1_lo, v_ir1_hi);
  const __m128i v_a2 = Sum343Row(v_ir2_lo, v_ir2_hi);
  return _mm_add_epi32(v_a0, _mm_add_epi32(v_a1, v_a2));
}

void BoxFilterProcess_SSE4_1(const RestorationUnitInfo& restoration_info,
                             const uint8_t* src, ptrdiff_t stride, int width,
                             int height, RestorationBuffer* const buffer) {
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  for (int pass = 0; pass < 2; ++pass) {
    const uint8_t radius = kSgrProjParams[sgr_proj_index][pass * 2];
    const uint8_t* src_ptr = src;
    if (radius == 0) continue;

    BoxFilterPreProcess_SSE4_1(restoration_info, src_ptr, stride, width, height,
                               pass, buffer);

    int* filtered_output = buffer->box_filter_process_output[pass];
    const ptrdiff_t filtered_output_stride =
        buffer->box_filter_process_output_stride;
    const ptrdiff_t intermediate_stride =
        buffer->box_filter_process_intermediate_stride;
    // Set intermediate buffer start point to the actual start point of
    // filtering.
    const ptrdiff_t intermediate_buffer_offset =
        kRestorationBorder * intermediate_stride + kRestorationBorder;

    if (pass == 0) {
      int y = 0;
      do {
        const int shift = ((y & 1) != 0) ? 4 : 5;
        uint32_t* const array_start[2] = {
            buffer->box_filter_process_intermediate[0] +
                intermediate_buffer_offset + y * intermediate_stride,
            buffer->box_filter_process_intermediate[1] +
                intermediate_buffer_offset + y * intermediate_stride};
        uint32_t* intermediate_result2[2] = {
            array_start[0] - intermediate_stride,
            array_start[1] - intermediate_stride};
        if ((y & 1) == 0) {  // even row
          int x = 0;
          do {
            // 5 6 5
            // 0 0 0
            // 5 6 5
            const __m128i v_A = Process3x3Block_565_Even(
                &intermediate_result2[0][x], intermediate_stride);
            const __m128i v_B = Process3x3Block_565_Even(
                &intermediate_result2[1][x], intermediate_stride);
            const __m128i v_src = _mm_cvtepu8_epi32(Load4(src_ptr + x));
            const __m128i v_v0 = _mm_mullo_epi32(v_A, v_src);
            const __m128i v_v = _mm_add_epi32(v_v0, v_B);
            const __m128i v_filtered = RightShiftWithRounding_U32(
                v_v, kSgrProjSgrBits + shift - kSgrProjRestoreBits);

            StoreUnaligned16(&filtered_output[x], v_filtered);
            x += 4;
          } while (x < width);
        } else {
          int x = 0;
          do {
            // 0 0 0
            // 5 6 5
            // 0 0 0
            const __m128i v_A = Process3x3Block_565_Odd(
                &intermediate_result2[0][x], intermediate_stride);
            const __m128i v_B = Process3x3Block_565_Odd(
                &intermediate_result2[1][x], intermediate_stride);
            const __m128i v_src = _mm_cvtepu8_epi32(Load4(src_ptr + x));
            const __m128i v_v0 = _mm_mullo_epi32(v_A, v_src);
            const __m128i v_v = _mm_add_epi32(v_v0, v_B);
            const __m128i v_filtered = RightShiftWithRounding_U32(
                v_v, kSgrProjSgrBits + shift - kSgrProjRestoreBits);

            StoreUnaligned16(&filtered_output[x], v_filtered);
            x += 4;
          } while (x < width);
        }
        src_ptr += stride;
        filtered_output += filtered_output_stride;
      } while (++y < height);
    } else {
      int y = 0;
      do {
        const int shift = 5;
        uint32_t* const array_start[2] = {
            buffer->box_filter_process_intermediate[0] +
                intermediate_buffer_offset + y * intermediate_stride,
            buffer->box_filter_process_intermediate[1] +
                intermediate_buffer_offset + y * intermediate_stride};
        uint32_t* intermediate_result2[2] = {
            array_start[0] - intermediate_stride,
            array_start[1] - intermediate_stride};
        int x = 0;
        do {
          const __m128i v_A = Process3x3Block_343(&intermediate_result2[0][x],
                                                  intermediate_stride);
          const __m128i v_B = Process3x3Block_343(&intermediate_result2[1][x],
                                                  intermediate_stride);
          const __m128i v_src = _mm_cvtepu8_epi32(Load4(src_ptr + x));
          const __m128i v_v0 = _mm_mullo_epi32(v_A, v_src);
          const __m128i v_v = _mm_add_epi32(v_v0, v_B);
          const __m128i v_filtered = RightShiftWithRounding_U32(
              v_v, kSgrProjSgrBits + shift - kSgrProjRestoreBits);

          StoreUnaligned16(&filtered_output[x], v_filtered);
          x += 4;
        } while (x < width);
        src_ptr += stride;
        filtered_output += filtered_output_stride;
      } while (++y < height);
    }
  }
}

void SelfGuidedFilter_SSE4_1(const void* source, void* dest,
                             const RestorationUnitInfo& restoration_info,
                             ptrdiff_t source_stride, ptrdiff_t dest_stride,
                             int width, int height,
                             RestorationBuffer* const buffer) {
  const auto* src = static_cast<const uint8_t*>(source);
  auto* dst = static_cast<uint8_t*>(dest);
  const int w0 = restoration_info.sgr_proj_info.multiplier[0];
  const int w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int w2 = (1 << kSgrProjPrecisionBits) - w0 - w1;
  const int index = restoration_info.sgr_proj_info.index;
  const uint8_t r0 = kSgrProjParams[index][0];
  const uint8_t r1 = kSgrProjParams[index][2];
  const ptrdiff_t array_stride = buffer->box_filter_process_output_stride;
  int* box_filter_process_output[2] = {buffer->box_filter_process_output[0],
                                       buffer->box_filter_process_output[1]};

  BoxFilterProcess_SSE4_1(restoration_info, src, source_stride, width, height,
                          buffer);

  const __m128i v_w0 = _mm_shuffle_epi32(_mm_cvtsi32_si128(w0), 0);
  const __m128i v_w1 = _mm_shuffle_epi32(_mm_cvtsi32_si128(w1), 0);
  const __m128i v_w2 = _mm_shuffle_epi32(_mm_cvtsi32_si128(w2), 0);
  const __m128i v_r0 = _mm_shuffle_epi32(_mm_cvtsi32_si128(r0), 0);
  const __m128i v_r1 = _mm_shuffle_epi32(_mm_cvtsi32_si128(r1), 0);
  const __m128i zero = _mm_setzero_si128();
  // Create masks used to select between src and box_filter_process_output.
  const __m128i v_r0_mask = _mm_cmpeq_epi32(v_r0, zero);
  const __m128i v_r1_mask = _mm_cmpeq_epi32(v_r1, zero);

  int y = 0;
  do {
    int x = 0;
    do {
      const __m128i v_src = _mm_cvtepu8_epi32(Load4(src + x));
      const __m128i v_u = _mm_slli_epi32(v_src, kSgrProjRestoreBits);
      const __m128i v_v_a = _mm_mullo_epi32(v_w1, v_u);
      const __m128i v_bfp_out0 =
          LoadUnaligned16(&box_filter_process_output[0][x]);
      // Select u or box_filter_process_output[0][x].
      const __m128i v_r0_mult = _mm_blendv_epi8(v_bfp_out0, v_u, v_r0_mask);
      const __m128i v_v_b = _mm_mullo_epi32(v_w0, v_r0_mult);
      const __m128i v_v_c = _mm_add_epi32(v_v_a, v_v_b);
      const __m128i v_bfp_out1 =
          LoadUnaligned16(&box_filter_process_output[1][x]);
      // Select u or box_filter_process_output[1][x].
      const __m128i v_r1_mult = _mm_blendv_epi8(v_bfp_out1, v_u, v_r1_mask);
      const __m128i v_v_d = _mm_mullo_epi32(v_w2, v_r1_mult);
      const __m128i v_v_e = _mm_add_epi32(v_v_c, v_v_d);
      __m128i v_s = RightShiftWithRounding_S32(
          v_v_e, kSgrProjRestoreBits + kSgrProjPrecisionBits);
      v_s = _mm_packs_epi32(v_s, v_s);
      v_s = _mm_packus_epi16(v_s, v_s);
      Store4(&dst[x], v_s);
      x += 4;
    } while (x < width);

    src += source_stride;
    dst += dest_stride;
    box_filter_process_output[0] += array_stride;
    box_filter_process_output[1] += array_stride;
  } while (++y < height);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
#if DSP_ENABLED_8BPP_SSE4_1(WienerFilter)
  dsp->loop_restorations[0] = WienerFilter_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(SelfGuidedFilter)
  dsp->loop_restorations[1] = SelfGuidedFilter_SSE4_1;
#endif
}

}  // namespace
}  // namespace low_bitdepth

void LoopRestorationInit_SSE4_1() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_SSE4_1
namespace libgav1 {
namespace dsp {

void LoopRestorationInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
