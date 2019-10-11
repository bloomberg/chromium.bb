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

#include "src/dsp/weight_mask.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

#include "src/dsp/dsp.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace {

template <int width, int height, int bitdepth, bool mask_is_inverse>
void WeightMask_C(const uint16_t* prediction_0, ptrdiff_t stride_0,
                  const uint16_t* prediction_1, ptrdiff_t stride_1,
                  uint8_t* mask, ptrdiff_t mask_stride) {
  constexpr int rounding_bits = bitdepth - 8 + ((bitdepth == 12) ? 2 : 4);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int difference = RightShiftWithRounding(
          std::abs(prediction_0[x] - prediction_1[x]), rounding_bits);
      const auto mask_value =
          static_cast<uint8_t>(std::min(DivideBy16(difference) + 38, 64));
      mask[x] = mask_is_inverse ? 64 - mask_value : mask_value;
    }
    prediction_0 += stride_0;
    prediction_1 += stride_1;
    mask += mask_stride;
  }
}

#define INIT_WEIGHT_MASK(width, height, bitdepth, w_index, h_index, b_index) \
  dsp->weight_mask[w_index][h_index][b_index][0] =                           \
      WeightMask_C<width, height, bitdepth, 0>;                              \
  dsp->weight_mask[w_index][h_index][b_index][1] =                           \
      WeightMask_C<width, height, bitdepth, 1>

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  INIT_WEIGHT_MASK(4, 4, 8, 0, 0, 0);
  INIT_WEIGHT_MASK(4, 8, 8, 0, 1, 0);
  INIT_WEIGHT_MASK(4, 16, 8, 0, 2, 0);
  INIT_WEIGHT_MASK(8, 4, 8, 1, 0, 0);
  INIT_WEIGHT_MASK(8, 8, 8, 1, 1, 0);
  INIT_WEIGHT_MASK(8, 16, 8, 1, 2, 0);
  INIT_WEIGHT_MASK(8, 32, 8, 1, 3, 0);
  INIT_WEIGHT_MASK(16, 4, 8, 2, 0, 0);
  INIT_WEIGHT_MASK(16, 8, 8, 2, 1, 0);
  INIT_WEIGHT_MASK(16, 16, 8, 2, 2, 0);
  INIT_WEIGHT_MASK(16, 32, 8, 2, 3, 0);
  INIT_WEIGHT_MASK(16, 64, 8, 2, 4, 0);
  INIT_WEIGHT_MASK(32, 8, 8, 3, 1, 0);
  INIT_WEIGHT_MASK(32, 16, 8, 3, 2, 0);
  INIT_WEIGHT_MASK(32, 32, 8, 3, 3, 0);
  INIT_WEIGHT_MASK(32, 64, 8, 3, 4, 0);
  INIT_WEIGHT_MASK(64, 16, 8, 4, 2, 0);
  INIT_WEIGHT_MASK(64, 32, 8, 4, 3, 0);
  INIT_WEIGHT_MASK(64, 64, 8, 4, 4, 0);
  INIT_WEIGHT_MASK(64, 128, 8, 4, 5, 0);
  INIT_WEIGHT_MASK(128, 64, 8, 5, 4, 0);
  INIT_WEIGHT_MASK(128, 128, 8, 5, 5, 0);
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_4x4
  INIT_WEIGHT_MASK(4, 4, 8, 0, 0, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_4x8
  INIT_WEIGHT_MASK(4, 8, 8, 0, 1, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_4x16
  INIT_WEIGHT_MASK(4, 16, 8, 0, 2, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_8x4
  INIT_WEIGHT_MASK(8, 4, 8, 1, 0, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_8x8
  INIT_WEIGHT_MASK(8, 8, 8, 1, 1, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_8x16
  INIT_WEIGHT_MASK(8, 16, 8, 1, 2, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_8x32
  INIT_WEIGHT_MASK(8, 32, 8, 1, 3, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_16x4
  INIT_WEIGHT_MASK(16, 4, 8, 2, 0, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_16x8
  INIT_WEIGHT_MASK(16, 8, 8, 2, 1, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_16x16
  INIT_WEIGHT_MASK(16, 16, 8, 2, 2, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_16x32
  INIT_WEIGHT_MASK(16, 32, 8, 2, 3, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_16x64
  INIT_WEIGHT_MASK(16, 64, 8, 2, 4, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_32x8
  INIT_WEIGHT_MASK(32, 8, 8, 3, 1, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_32x16
  INIT_WEIGHT_MASK(32, 16, 8, 3, 2, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_32x32
  INIT_WEIGHT_MASK(32, 32, 8, 3, 3, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_32x64
  INIT_WEIGHT_MASK(32, 64, 8, 3, 4, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_64x16
  INIT_WEIGHT_MASK(64, 16, 8, 4, 2, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_64x32
  INIT_WEIGHT_MASK(64, 32, 8, 4, 3, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_64x64
  INIT_WEIGHT_MASK(64, 64, 8, 4, 4, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_64x128
  INIT_WEIGHT_MASK(64, 128, 8, 4, 5, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_128x64
  INIT_WEIGHT_MASK(128, 64, 8, 5, 4, 0);
#endif
#ifndef LIBGAV1_Dsp8bpp_WeightMask8_128x128
  INIT_WEIGHT_MASK(128, 128, 8, 5, 5, 0);
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#if LIBGAV1_MAX_BITDEPTH >= 10
void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  INIT_WEIGHT_MASK(4, 4, 8, 0, 0, 0);
  INIT_WEIGHT_MASK(4, 8, 8, 0, 1, 0);
  INIT_WEIGHT_MASK(4, 16, 8, 0, 2, 0);
  INIT_WEIGHT_MASK(8, 4, 8, 1, 0, 0);
  INIT_WEIGHT_MASK(8, 8, 8, 1, 1, 0);
  INIT_WEIGHT_MASK(8, 16, 8, 1, 2, 0);
  INIT_WEIGHT_MASK(8, 32, 8, 1, 3, 0);
  INIT_WEIGHT_MASK(16, 4, 8, 2, 0, 0);
  INIT_WEIGHT_MASK(16, 8, 8, 2, 1, 0);
  INIT_WEIGHT_MASK(16, 16, 8, 2, 2, 0);
  INIT_WEIGHT_MASK(16, 32, 8, 2, 3, 0);
  INIT_WEIGHT_MASK(16, 64, 8, 2, 4, 0);
  INIT_WEIGHT_MASK(32, 8, 8, 3, 1, 0);
  INIT_WEIGHT_MASK(32, 16, 8, 3, 2, 0);
  INIT_WEIGHT_MASK(32, 32, 8, 3, 3, 0);
  INIT_WEIGHT_MASK(32, 64, 8, 3, 4, 0);
  INIT_WEIGHT_MASK(64, 16, 8, 4, 2, 0);
  INIT_WEIGHT_MASK(64, 32, 8, 4, 3, 0);
  INIT_WEIGHT_MASK(64, 64, 8, 4, 4, 0);
  INIT_WEIGHT_MASK(64, 128, 8, 4, 5, 0);
  INIT_WEIGHT_MASK(128, 64, 8, 5, 4, 0);
  INIT_WEIGHT_MASK(128, 128, 8, 5, 5, 0);
  INIT_WEIGHT_MASK(4, 4, 10, 0, 0, 1);
  INIT_WEIGHT_MASK(4, 8, 10, 0, 1, 1);
  INIT_WEIGHT_MASK(4, 16, 10, 0, 2, 1);
  INIT_WEIGHT_MASK(8, 4, 10, 1, 0, 1);
  INIT_WEIGHT_MASK(8, 8, 10, 1, 1, 1);
  INIT_WEIGHT_MASK(8, 16, 10, 1, 2, 1);
  INIT_WEIGHT_MASK(8, 32, 10, 1, 3, 1);
  INIT_WEIGHT_MASK(16, 4, 10, 2, 0, 1);
  INIT_WEIGHT_MASK(16, 8, 10, 2, 1, 1);
  INIT_WEIGHT_MASK(16, 16, 10, 2, 2, 1);
  INIT_WEIGHT_MASK(16, 32, 10, 2, 3, 1);
  INIT_WEIGHT_MASK(16, 64, 10, 2, 4, 1);
  INIT_WEIGHT_MASK(32, 8, 10, 3, 1, 1);
  INIT_WEIGHT_MASK(32, 16, 10, 3, 2, 1);
  INIT_WEIGHT_MASK(32, 32, 10, 3, 3, 1);
  INIT_WEIGHT_MASK(32, 64, 10, 3, 4, 1);
  INIT_WEIGHT_MASK(64, 16, 10, 4, 2, 1);
  INIT_WEIGHT_MASK(64, 32, 10, 4, 3, 1);
  INIT_WEIGHT_MASK(64, 64, 10, 4, 4, 1);
  INIT_WEIGHT_MASK(64, 128, 10, 4, 5, 1);
  INIT_WEIGHT_MASK(128, 64, 10, 5, 4, 1);
  INIT_WEIGHT_MASK(128, 128, 10, 5, 5, 1);
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_4x4
  INIT_WEIGHT_MASK(4, 4, 8, 0, 0, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_4x8
  INIT_WEIGHT_MASK(4, 8, 8, 0, 1, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_4x16
  INIT_WEIGHT_MASK(4, 16, 8, 0, 2, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_8x4
  INIT_WEIGHT_MASK(8, 4, 8, 1, 0, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_8x8
  INIT_WEIGHT_MASK(8, 8, 8, 1, 1, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_8x16
  INIT_WEIGHT_MASK(8, 16, 8, 1, 2, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_8x32
  INIT_WEIGHT_MASK(8, 32, 8, 1, 3, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_16x4
  INIT_WEIGHT_MASK(16, 4, 8, 2, 0, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_16x8
  INIT_WEIGHT_MASK(16, 8, 8, 2, 1, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_16x16
  INIT_WEIGHT_MASK(16, 16, 8, 2, 2, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_16x32
  INIT_WEIGHT_MASK(16, 32, 8, 2, 3, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_16x64
  INIT_WEIGHT_MASK(16, 64, 8, 2, 4, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_32x8
  INIT_WEIGHT_MASK(32, 8, 8, 3, 1, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_32x16
  INIT_WEIGHT_MASK(32, 16, 8, 3, 2, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_32x32
  INIT_WEIGHT_MASK(32, 32, 8, 3, 3, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_32x64
  INIT_WEIGHT_MASK(32, 64, 8, 3, 4, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_64x16
  INIT_WEIGHT_MASK(64, 16, 8, 4, 2, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_64x32
  INIT_WEIGHT_MASK(64, 32, 8, 4, 3, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_64x64
  INIT_WEIGHT_MASK(64, 64, 8, 4, 4, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_64x128
  INIT_WEIGHT_MASK(64, 128, 8, 4, 5, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_128x64
  INIT_WEIGHT_MASK(128, 64, 8, 5, 4, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask8_128x128
  INIT_WEIGHT_MASK(128, 128, 8, 5, 5, 0);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_4x4
  INIT_WEIGHT_MASK(4, 4, 10, 0, 0, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_4x8
  INIT_WEIGHT_MASK(4, 8, 10, 0, 1, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_4x16
  INIT_WEIGHT_MASK(4, 16, 10, 0, 2, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_8x4
  INIT_WEIGHT_MASK(8, 4, 10, 1, 0, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_8x8
  INIT_WEIGHT_MASK(8, 8, 10, 1, 1, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_8x16
  INIT_WEIGHT_MASK(8, 16, 10, 1, 2, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_8x32
  INIT_WEIGHT_MASK(8, 32, 10, 1, 3, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_16x4
  INIT_WEIGHT_MASK(16, 4, 10, 2, 0, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_16x8
  INIT_WEIGHT_MASK(16, 8, 10, 2, 1, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_16x16
  INIT_WEIGHT_MASK(16, 16, 10, 2, 2, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_16x32
  INIT_WEIGHT_MASK(16, 32, 10, 2, 3, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_16x64
  INIT_WEIGHT_MASK(16, 64, 10, 2, 4, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_32x8
  INIT_WEIGHT_MASK(32, 8, 10, 3, 1, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_32x16
  INIT_WEIGHT_MASK(32, 16, 10, 3, 2, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_32x32
  INIT_WEIGHT_MASK(32, 32, 10, 3, 3, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_32x64
  INIT_WEIGHT_MASK(32, 64, 10, 3, 4, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_64x16
  INIT_WEIGHT_MASK(64, 16, 10, 4, 2, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_64x32
  INIT_WEIGHT_MASK(64, 32, 10, 4, 3, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_64x64
  INIT_WEIGHT_MASK(64, 64, 10, 4, 4, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_64x128
  INIT_WEIGHT_MASK(64, 128, 10, 4, 5, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_128x64
  INIT_WEIGHT_MASK(128, 64, 10, 5, 4, 1);
#endif
#ifndef LIBGAV1_Dsp10bpp_WeightMask10_128x128
  INIT_WEIGHT_MASK(128, 128, 10, 5, 5, 1);
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}
#endif

}  // namespace

void WeightMaskInit_C() {
  Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  Init10bpp();
#endif
}

}  // namespace dsp
}  // namespace libgav1
