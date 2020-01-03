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

#include "src/dsp/dsp.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {

namespace {

template <int bitdepth, typename Pixel>
void ComputeSuperRes(const void* source, const int upscaled_width,
                     const int initial_subpixel_x, const int step,
                     void* const dest) {
  assert(step <= kSuperResScaleMask);
  const auto* src = reinterpret_cast<const Pixel*>(source);
  auto* dst = reinterpret_cast<Pixel*>(dest);
  src -= DivideBy2(kSuperResFilterTaps);
  int subpixel_x = initial_subpixel_x;
  for (int x = 0; x < upscaled_width; ++x) {
    int sum = 0;
    const Pixel* const src_x = &src[subpixel_x >> kSuperResScaleBits];
    const int src_x_subpixel =
        (subpixel_x & kSuperResScaleMask) >> kSuperResExtraBits;
    for (int i = 0; i < kSuperResFilterTaps; ++i) {
      sum += src_x[i] * kUpscaleFilter[src_x_subpixel][i];
    }
    dst[x] =
        Clip3(RightShiftWithRounding(sum, kFilterBits), 0, (1 << bitdepth) - 1);
    subpixel_x += step;
  }
}

void Init8bpp() {
  Dsp* dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->super_res_row = ComputeSuperRes<8, uint8_t>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp8bpp_SuperRes
  dsp->super_res_row = ComputeSuperRes<8, uint8_t>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#if LIBGAV1_MAX_BITDEPTH >= 10
void Init10bpp() {
  Dsp* dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->super_res_row = ComputeSuperRes<10, uint16_t>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp10bpp_SuperRes
  dsp->super_res_row = ComputeSuperRes<10, uint16_t>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}
#endif

}  // namespace

void SuperResInit_C() {
  Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  Init10bpp();
#endif
}

}  // namespace dsp

}  // namespace libgav1
