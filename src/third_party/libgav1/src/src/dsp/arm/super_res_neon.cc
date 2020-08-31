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

#include "src/dsp/arm/common_neon.h"
#include "src/dsp/super_res.h"
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_NEON

#include <arm_neon.h>

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {

namespace low_bitdepth {
namespace {

void ComputeSuperRes_NEON(const void* source, const int upscaled_width,
                          const int initial_subpixel_x, const int step,
                          void* const dest) {
  const auto* src = static_cast<const uint8_t*>(source);
  auto* dst = static_cast<uint8_t*>(dest);
  src -= kSuperResFilterTaps >> 1;

  int p = initial_subpixel_x;
  uint16x8_t weighted_src[8];
  for (int x = 0; x < upscaled_width; x += 8) {
    for (int i = 0; i < kSuperResFilterTaps; ++i, p += step) {
      const uint8x8_t src_x = vld1_u8(&src[p >> kSuperResScaleBits]);
      const int remainder = p & kSuperResScaleMask;
      const uint8x8_t filter =
          vld1_u8(kUpscaleFilterUnsigned[remainder >> kSuperResExtraBits]);
      weighted_src[i] = vmull_u8(src_x, filter);
    }
    Transpose8x8(weighted_src);

    // Maximum sum of positive taps: 171 = 7 + 86 + 71 + 7
    // Maximum sum: 255*171 == 0xAA55
    // The sum is clipped to [0, 255], so adding all positive and then
    // subtracting all negative with saturation is sufficient.
    //           0 1 2 3 4 5 6 7
    // tap sign: - + - + + - + -
    uint16x8_t res = weighted_src[1];
    res = vaddq_u16(res, weighted_src[3]);
    res = vaddq_u16(res, weighted_src[4]);
    res = vaddq_u16(res, weighted_src[6]);
    res = vqsubq_u16(res, weighted_src[0]);
    res = vqsubq_u16(res, weighted_src[2]);
    res = vqsubq_u16(res, weighted_src[5]);
    res = vqsubq_u16(res, weighted_src[7]);
    vst1_u8(&dst[x], vqrshrn_n_u16(res, kFilterBits));
  }
}

void Init8bpp() {
  Dsp* dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  dsp->super_res_row = ComputeSuperRes_NEON;
}

}  // namespace
}  // namespace low_bitdepth

void SuperResInit_NEON() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_NEON

namespace libgav1 {
namespace dsp {

void SuperResInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON
