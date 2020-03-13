// Copyright 2020 The libgav1 Authors
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

#include "src/dsp/super_res.h"
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_SSE4_1

#include <smmintrin.h>

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/dsp/x86/common_sse4.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

// Upscale_Filter as defined in AV1 Section 7.16
alignas(16) const int16_t
    kUpscaleFilter[kSuperResFilterShifts][kSuperResFilterTaps] = {
        {-0, 0, -0, 128, 0, -0, 0, -0},    {-0, 0, -1, 128, 2, -1, 0, -0},
        {-0, 1, -3, 127, 4, -2, 1, -0},    {-0, 1, -4, 127, 6, -3, 1, -0},
        {-0, 2, -6, 126, 8, -3, 1, -0},    {-0, 2, -7, 125, 11, -4, 1, -0},
        {-1, 2, -8, 125, 13, -5, 2, -0},   {-1, 3, -9, 124, 15, -6, 2, -0},
        {-1, 3, -10, 123, 18, -6, 2, -1},  {-1, 3, -11, 122, 20, -7, 3, -1},
        {-1, 4, -12, 121, 22, -8, 3, -1},  {-1, 4, -13, 120, 25, -9, 3, -1},
        {-1, 4, -14, 118, 28, -9, 3, -1},  {-1, 4, -15, 117, 30, -10, 4, -1},
        {-1, 5, -16, 116, 32, -11, 4, -1}, {-1, 5, -16, 114, 35, -12, 4, -1},
        {-1, 5, -17, 112, 38, -12, 4, -1}, {-1, 5, -18, 111, 40, -13, 5, -1},
        {-1, 5, -18, 109, 43, -14, 5, -1}, {-1, 6, -19, 107, 45, -14, 5, -1},
        {-1, 6, -19, 105, 48, -15, 5, -1}, {-1, 6, -19, 103, 51, -16, 5, -1},
        {-1, 6, -20, 101, 53, -16, 6, -1}, {-1, 6, -20, 99, 56, -17, 6, -1},
        {-1, 6, -20, 97, 58, -17, 6, -1},  {-1, 6, -20, 95, 61, -18, 6, -1},
        {-2, 7, -20, 93, 64, -18, 6, -2},  {-2, 7, -20, 91, 66, -19, 6, -1},
        {-2, 7, -20, 88, 69, -19, 6, -1},  {-2, 7, -20, 86, 71, -19, 6, -1},
        {-2, 7, -20, 84, 74, -20, 7, -2},  {-2, 7, -20, 81, 76, -20, 7, -1},
        {-2, 7, -20, 79, 79, -20, 7, -2},  {-1, 7, -20, 76, 81, -20, 7, -2},
        {-2, 7, -20, 74, 84, -20, 7, -2},  {-1, 6, -19, 71, 86, -20, 7, -2},
        {-1, 6, -19, 69, 88, -20, 7, -2},  {-1, 6, -19, 66, 91, -20, 7, -2},
        {-2, 6, -18, 64, 93, -20, 7, -2},  {-1, 6, -18, 61, 95, -20, 6, -1},
        {-1, 6, -17, 58, 97, -20, 6, -1},  {-1, 6, -17, 56, 99, -20, 6, -1},
        {-1, 6, -16, 53, 101, -20, 6, -1}, {-1, 5, -16, 51, 103, -19, 6, -1},
        {-1, 5, -15, 48, 105, -19, 6, -1}, {-1, 5, -14, 45, 107, -19, 6, -1},
        {-1, 5, -14, 43, 109, -18, 5, -1}, {-1, 5, -13, 40, 111, -18, 5, -1},
        {-1, 4, -12, 38, 112, -17, 5, -1}, {-1, 4, -12, 35, 114, -16, 5, -1},
        {-1, 4, -11, 32, 116, -16, 5, -1}, {-1, 4, -10, 30, 117, -15, 4, -1},
        {-1, 3, -9, 28, 118, -14, 4, -1},  {-1, 3, -9, 25, 120, -13, 4, -1},
        {-1, 3, -8, 22, 121, -12, 4, -1},  {-1, 3, -7, 20, 122, -11, 3, -1},
        {-1, 2, -6, 18, 123, -10, 3, -1},  {-0, 2, -6, 15, 124, -9, 3, -1},
        {-0, 2, -5, 13, 125, -8, 2, -1},   {-0, 1, -4, 11, 125, -7, 2, -0},
        {-0, 1, -3, 8, 126, -6, 2, -0},    {-0, 1, -3, 6, 127, -4, 1, -0},
        {-0, 1, -2, 4, 127, -3, 1, -0},    {-0, 0, -1, 2, 128, -1, 0, -0},
};

inline void ComputeSuperRes4(const uint8_t* src, uint8_t* dst_x, int step,
                             int* p) {
  __m128i weighted_src[4];
  for (int i = 0; i < 4; ++i, *p += step) {
    const __m128i src_x = LoadLo8(&src[*p >> kSuperResScaleBits]);
    const int remainder = *p & kSuperResScaleMask;
    const __m128i filter =
        LoadUnaligned16(kUpscaleFilter[remainder >> kSuperResExtraBits]);
    weighted_src[i] = _mm_madd_epi16(_mm_cvtepu8_epi16(src_x), filter);
  }

  // Pairwise add is chosen in favor of transpose and add because of the
  // ability to take advantage of madd.
  const __m128i res0 = _mm_hadd_epi32(weighted_src[0], weighted_src[1]);
  const __m128i res1 = _mm_hadd_epi32(weighted_src[2], weighted_src[3]);
  const __m128i result0 = _mm_hadd_epi32(res0, res1);
  const __m128i result = _mm_packus_epi32(
      RightShiftWithRounding_S32(result0, kFilterBits), result0);
  Store4(dst_x, _mm_packus_epi16(result, result));
}

inline void ComputeSuperRes8(const uint8_t* src, uint8_t* dst_x, int step,
                             int* p) {
  __m128i weighted_src[8];
  for (int i = 0; i < 8; ++i, *p += step) {
    const __m128i src_x = LoadLo8(&src[*p >> kSuperResScaleBits]);
    const int remainder = *p & kSuperResScaleMask;
    const __m128i filter =
        LoadUnaligned16(kUpscaleFilter[remainder >> kSuperResExtraBits]);
    weighted_src[i] = _mm_madd_epi16(_mm_cvtepu8_epi16(src_x), filter);
  }

  // Pairwise add is chosen in favor of transpose and add because of the
  // ability to take advantage of madd.
  const __m128i res0 = _mm_hadd_epi32(weighted_src[0], weighted_src[1]);
  const __m128i res1 = _mm_hadd_epi32(weighted_src[2], weighted_src[3]);
  const __m128i res2 = _mm_hadd_epi32(weighted_src[4], weighted_src[5]);
  const __m128i res3 = _mm_hadd_epi32(weighted_src[6], weighted_src[7]);
  const __m128i result0 = _mm_hadd_epi32(res0, res1);
  const __m128i result1 = _mm_hadd_epi32(res2, res3);
  const __m128i result =
      _mm_packus_epi32(RightShiftWithRounding_S32(result0, kFilterBits),
                       RightShiftWithRounding_S32(result1, kFilterBits));
  StoreLo8(dst_x, _mm_packus_epi16(result, result));
}

void ComputeSuperRes_SSE4_1(const void* source, const int upscaled_width,
                            const int initial_subpixel_x, const int step,
                            void* const dest) {
  const auto* src = static_cast<const uint8_t*>(source);
  auto* dst = static_cast<uint8_t*>(dest);
  src -= kSuperResFilterTaps >> 1;

  int p = initial_subpixel_x;
  int x = 0;
  for (; x < (upscaled_width & ~7); x += 8) {
    ComputeSuperRes8(src, &dst[x], step, &p);
  }
  // The below code can overwrite at most 3 bytes and overread at most 7.
  // kSuperResHorizontalBorder accounts for this.
  for (; x < upscaled_width; x += 4) {
    ComputeSuperRes4(src, &dst[x], step, &p);
  }
}

void Init8bpp() {
  Dsp* dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  dsp->super_res_row = ComputeSuperRes_SSE4_1;
}

}  // namespace
}  // namespace low_bitdepth

void SuperResInit_SSE4_1() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_SSE4_1

namespace libgav1 {
namespace dsp {

void SuperResInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
