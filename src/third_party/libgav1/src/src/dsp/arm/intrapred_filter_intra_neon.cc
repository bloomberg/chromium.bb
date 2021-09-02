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

namespace low_bitdepth {
namespace {

// Transpose kFilterIntraTaps and convert the first row to unsigned values.
//
// With the previous orientation we were able to multiply all the input values
// by a single tap. This required that all the input values be in one vector
// which requires expensive set up operations (shifts, vext, vtbl). All the
// elements of the result needed to be summed (easy on A64 - vaddvq_s16) but
// then the shifting, rounding, and clamping was done in GP registers.
//
// Switching to unsigned values allows multiplying the 8 bit inputs directly.
// When one value was negative we needed to vmovl_u8 first so that the results
// maintained the proper sign.
//
// We take this into account when summing the values by subtracting the product
// of the first row.
alignas(8) constexpr uint8_t kTransposedTaps[kNumFilterIntraPredictors][7][8] =
    {{{6, 5, 3, 3, 4, 3, 3, 3},  // Original values are negative.
      {10, 2, 1, 1, 6, 2, 2, 1},
      {0, 10, 1, 1, 0, 6, 2, 2},
      {0, 0, 10, 2, 0, 0, 6, 2},
      {0, 0, 0, 10, 0, 0, 0, 6},
      {12, 9, 7, 5, 2, 2, 2, 3},
      {0, 0, 0, 0, 12, 9, 7, 5}},
     {{10, 6, 4, 2, 10, 6, 4, 2},  // Original values are negative.
      {16, 0, 0, 0, 16, 0, 0, 0},
      {0, 16, 0, 0, 0, 16, 0, 0},
      {0, 0, 16, 0, 0, 0, 16, 0},
      {0, 0, 0, 16, 0, 0, 0, 16},
      {10, 6, 4, 2, 0, 0, 0, 0},
      {0, 0, 0, 0, 10, 6, 4, 2}},
     {{8, 8, 8, 8, 4, 4, 4, 4},  // Original values are negative.
      {8, 0, 0, 0, 4, 0, 0, 0},
      {0, 8, 0, 0, 0, 4, 0, 0},
      {0, 0, 8, 0, 0, 0, 4, 0},
      {0, 0, 0, 8, 0, 0, 0, 4},
      {16, 16, 16, 16, 0, 0, 0, 0},
      {0, 0, 0, 0, 16, 16, 16, 16}},
     {{2, 1, 1, 0, 1, 1, 1, 1},  // Original values are negative.
      {8, 3, 2, 1, 4, 3, 2, 2},
      {0, 8, 3, 2, 0, 4, 3, 2},
      {0, 0, 8, 3, 0, 0, 4, 3},
      {0, 0, 0, 8, 0, 0, 0, 4},
      {10, 6, 4, 2, 3, 4, 4, 3},
      {0, 0, 0, 0, 10, 6, 4, 3}},
     {{12, 10, 9, 8, 10, 9, 8, 7},  // Original values are negative.
      {14, 0, 0, 0, 12, 1, 0, 0},
      {0, 14, 0, 0, 0, 12, 0, 0},
      {0, 0, 14, 0, 0, 0, 12, 1},
      {0, 0, 0, 14, 0, 0, 0, 12},
      {14, 12, 11, 10, 0, 0, 1, 1},
      {0, 0, 0, 0, 14, 12, 11, 9}}};

void FilterIntraPredictor_NEON(void* const dest, ptrdiff_t stride,
                               const void* const top_row,
                               const void* const left_column,
                               FilterIntraPredictor pred, int width,
                               int height) {
  const uint8_t* const top = static_cast<const uint8_t*>(top_row);
  const uint8_t* const left = static_cast<const uint8_t*>(left_column);

  assert(width <= 32 && height <= 32);

  uint8_t* dst = static_cast<uint8_t*>(dest);

  uint8x8_t transposed_taps[7];
  for (int i = 0; i < 7; ++i) {
    transposed_taps[i] = vld1_u8(kTransposedTaps[pred][i]);
  }

  uint8_t relative_top_left = top[-1];
  const uint8_t* relative_top = top;
  uint8_t relative_left[2] = {left[0], left[1]};

  int y = 0;
  do {
    uint8_t* row_dst = dst;
    int x = 0;
    do {
      uint16x8_t sum = vdupq_n_u16(0);
      const uint16x8_t subtrahend =
          vmull_u8(transposed_taps[0], vdup_n_u8(relative_top_left));
      for (int i = 1; i < 5; ++i) {
        sum = vmlal_u8(sum, transposed_taps[i], vdup_n_u8(relative_top[i - 1]));
      }
      for (int i = 5; i < 7; ++i) {
        sum =
            vmlal_u8(sum, transposed_taps[i], vdup_n_u8(relative_left[i - 5]));
      }

      const int16x8_t sum_signed =
          vreinterpretq_s16_u16(vsubq_u16(sum, subtrahend));
      const int16x8_t sum_shifted = vrshrq_n_s16(sum_signed, 4);

      uint8x8_t sum_saturated = vqmovun_s16(sum_shifted);

      StoreLo4(row_dst, sum_saturated);
      StoreHi4(row_dst + stride, sum_saturated);

      // Progress across
      relative_top_left = relative_top[3];
      relative_top += 4;
      relative_left[0] = row_dst[3];
      relative_left[1] = row_dst[3 + stride];
      row_dst += 4;
      x += 4;
    } while (x < width);

    // Progress down.
    relative_top_left = left[y + 1];
    relative_top = dst + stride;
    relative_left[0] = left[y + 2];
    relative_left[1] = left[y + 3];

    dst += 2 * stride;
    y += 2;
  } while (y < height);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
  dsp->filter_intra_predictor = FilterIntraPredictor_NEON;
}

}  // namespace
}  // namespace low_bitdepth

void IntraPredFilterIntraInit_NEON() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_NEON
namespace libgav1 {
namespace dsp {

void IntraPredFilterIntraInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON
