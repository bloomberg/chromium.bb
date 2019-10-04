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

#include "src/dsp/mask_blend.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/dsp/dsp.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace {

template <int bitdepth, typename Pixel, bool is_inter_intra, int subsampling_x,
          int subsampling_y>
void MaskBlend_C(const uint16_t* prediction_0,
                 const ptrdiff_t prediction_stride_0,
                 const uint16_t* prediction_1,
                 const ptrdiff_t prediction_stride_1, const uint8_t* mask,
                 const ptrdiff_t mask_stride, const int width, const int height,
                 void* dest, const ptrdiff_t dest_stride) {
  assert(mask != nullptr);
  auto* dst = static_cast<Pixel*>(dest);
  const ptrdiff_t dst_stride = dest_stride / sizeof(Pixel);
  constexpr int step_y = subsampling_y ? 2 : 1;
  const uint8_t* mask_next_row = mask + mask_stride;
  // An offset to cancel offsets used in single predictor generation that
  // make intermediate computations non negative.
  const int single_round_offset = (1 << bitdepth) + (1 << (bitdepth - 1));
  // An offset to cancel offsets used in compound predictor generation that
  // make intermediate computations non negative.
  const int compound_round_offset =
      (1 << (bitdepth + 4)) + (1 << (bitdepth + 3));
  // 7.11.3.2 Rounding variables derivation process
  //   2 * FILTER_BITS(7) - (InterRound0(3|5) + InterRound1(7))
  constexpr int inter_post_round_bits = (bitdepth == 12) ? 2 : 4;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      uint8_t mask_value;
      if ((subsampling_x | subsampling_y) == 0) {
        mask_value = mask[x];
      } else if (subsampling_x == 1 && subsampling_y == 0) {
        mask_value = static_cast<uint8_t>(RightShiftWithRounding(
            mask[MultiplyBy2(x)] + mask[MultiplyBy2(x) + 1], 1));
      } else {
        assert(subsampling_x == 1 && subsampling_y == 1);
        mask_value = static_cast<uint8_t>(RightShiftWithRounding(
            mask[MultiplyBy2(x)] + mask[MultiplyBy2(x) + 1] +
                mask_next_row[MultiplyBy2(x)] +
                mask_next_row[MultiplyBy2(x) + 1],
            2));
      }

      if (is_inter_intra) {
        // In inter intra prediction mode, the intra prediction (prediction_1)
        // values are valid pixel values: [0, (1 << bitdepth) - 1].
        // While the inter prediction values come from subpixel prediction
        // from another frame, which involves interpolation and rounding.
        // Therefore prediction_0 has to be clipped.
        dst[x] = static_cast<Pixel>(RightShiftWithRounding(
            mask_value * prediction_1[x] +
                (64 - mask_value) * Clip3(prediction_0[x] - single_round_offset,
                                          0, (1 << bitdepth) - 1),
            6));
      } else {
        int res = (mask_value * prediction_0[x] +
                   (64 - mask_value) * prediction_1[x]) >>
                  6;
        res -= compound_round_offset;
        dst[x] = static_cast<Pixel>(
            Clip3(RightShiftWithRounding(res, inter_post_round_bits), 0,
                  (1 << bitdepth) - 1));
      }
    }
    dst += dst_stride;
    mask += mask_stride * step_y;
    mask_next_row += mask_stride * step_y;
    prediction_0 += prediction_stride_0;
    prediction_1 += prediction_stride_1;
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->mask_blend[0][0] = MaskBlend_C<8, uint8_t, false, 0, 0>;
  dsp->mask_blend[1][0] = MaskBlend_C<8, uint8_t, false, 1, 0>;
  dsp->mask_blend[2][0] = MaskBlend_C<8, uint8_t, false, 1, 1>;
  dsp->mask_blend[0][1] = MaskBlend_C<8, uint8_t, true, 0, 0>;
  dsp->mask_blend[1][1] = MaskBlend_C<8, uint8_t, true, 1, 0>;
  dsp->mask_blend[2][1] = MaskBlend_C<8, uint8_t, true, 1, 1>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp8bpp_MaskBlend444
  dsp->mask_blend[0][0] = MaskBlend_C<8, uint8_t, false, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_MaskBlend422
  dsp->mask_blend[1][0] = MaskBlend_C<8, uint8_t, false, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_MaskBlend420
  dsp->mask_blend[2][0] = MaskBlend_C<8, uint8_t, false, 1, 1>;
#endif
#ifndef LIBGAV1_Dsp8bpp_MaskBlendInterIntra444
  dsp->mask_blend[0][1] = MaskBlend_C<8, uint8_t, true, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_MaskBlendInterIntra422
  dsp->mask_blend[1][1] = MaskBlend_C<8, uint8_t, true, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_MaskBlendInterIntra420
  dsp->mask_blend[2][1] = MaskBlend_C<8, uint8_t, true, 1, 1>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#if LIBGAV1_MAX_BITDEPTH >= 10
void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->mask_blend[0][0] = MaskBlend_C<10, uint16_t, false, 0, 0>;
  dsp->mask_blend[1][0] = MaskBlend_C<10, uint16_t, false, 1, 0>;
  dsp->mask_blend[2][0] = MaskBlend_C<10, uint16_t, false, 1, 1>;
  dsp->mask_blend[0][1] = MaskBlend_C<10, uint16_t, true, 0, 0>;
  dsp->mask_blend[1][1] = MaskBlend_C<10, uint16_t, true, 1, 0>;
  dsp->mask_blend[2][1] = MaskBlend_C<10, uint16_t, true, 1, 1>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp10bpp_MaskBlend444
  dsp->mask_blend[0][0] = MaskBlend_C<10, uint16_t, false, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_MaskBlend422
  dsp->mask_blend[1][0] = MaskBlend_C<10, uint16_t, false, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_MaskBlend420
  dsp->mask_blend[2][0] = MaskBlend_C<10, uint16_t, false, 1, 1>;
#endif
#ifndef LIBGAV1_Dsp10bpp_MaskBlendInterIntra444
  dsp->mask_blend[0][1] = MaskBlend_C<10, uint16_t, true, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_MaskBlendInterIntra422
  dsp->mask_blend[1][1] = MaskBlend_C<10, uint16_t, true, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_MaskBlendInterIntra420
  dsp->mask_blend[2][1] = MaskBlend_C<10, uint16_t, true, 1, 1>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}
#endif

}  // namespace

void MaskBlendInit_C() {
  Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  Init10bpp();
#endif
}

}  // namespace dsp
}  // namespace libgav1
