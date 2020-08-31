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

#include "src/reconstruction.h"

#include <cassert>
#include <cstdint>

#include "src/utils/common.h"

namespace libgav1 {
namespace {

// Maps TransformType to dsp::Transform1D for the row transforms.
constexpr dsp::Transform1D kRowTransform[kNumTransformTypes] = {
    dsp::k1DTransformDct,      dsp::k1DTransformAdst,
    dsp::k1DTransformDct,      dsp::k1DTransformAdst,
    dsp::k1DTransformAdst,     dsp::k1DTransformDct,
    dsp::k1DTransformAdst,     dsp::k1DTransformAdst,
    dsp::k1DTransformAdst,     dsp::k1DTransformIdentity,
    dsp::k1DTransformIdentity, dsp::k1DTransformDct,
    dsp::k1DTransformIdentity, dsp::k1DTransformAdst,
    dsp::k1DTransformIdentity, dsp::k1DTransformAdst};

// Maps TransformType to dsp::Transform1D for the column transforms.
constexpr dsp::Transform1D kColumnTransform[kNumTransformTypes] = {
    dsp::k1DTransformDct,  dsp::k1DTransformDct,
    dsp::k1DTransformAdst, dsp::k1DTransformAdst,
    dsp::k1DTransformDct,  dsp::k1DTransformAdst,
    dsp::k1DTransformAdst, dsp::k1DTransformAdst,
    dsp::k1DTransformAdst, dsp::k1DTransformIdentity,
    dsp::k1DTransformDct,  dsp::k1DTransformIdentity,
    dsp::k1DTransformAdst, dsp::k1DTransformIdentity,
    dsp::k1DTransformAdst, dsp::k1DTransformIdentity};

dsp::TransformSize1D Get1DTransformSize(int size_log2) {
  return static_cast<dsp::TransformSize1D>(size_log2 - 2);
}

}  // namespace

template <typename Residual, typename Pixel>
void Reconstruct(const dsp::Dsp& dsp, TransformType tx_type,
                 TransformSize tx_size, bool lossless, Residual* const buffer,
                 int start_x, int start_y, Array2DView<Pixel>* frame,
                 int non_zero_coeff_count) {
  static_assert(sizeof(Residual) == 2 || sizeof(Residual) == 4, "");
  const int tx_width_log2 = kTransformWidthLog2[tx_size];
  const int tx_height_log2 = kTransformHeightLog2[tx_size];

  // Row transform.
  const dsp::TransformSize1D row_transform_size =
      Get1DTransformSize(tx_width_log2);
  const dsp::Transform1D row_transform =
      lossless ? dsp::k1DTransformWht : kRowTransform[tx_type];
  const dsp::InverseTransformAddFunc row_transform_func =
      dsp.inverse_transforms[row_transform_size][row_transform];
  assert(row_transform_func != nullptr);

  row_transform_func(tx_type, tx_size, buffer, start_x, start_y, frame,
                     /*is_row=*/true, non_zero_coeff_count);

  // Column transform.
  const dsp::TransformSize1D column_transform_size =
      Get1DTransformSize(tx_height_log2);
  const dsp::Transform1D column_transform =
      lossless ? dsp::k1DTransformWht : kColumnTransform[tx_type];
  const dsp::InverseTransformAddFunc column_transform_func =
      dsp.inverse_transforms[column_transform_size][column_transform];
  assert(column_transform_func != nullptr);

  column_transform_func(tx_type, tx_size, buffer, start_x, start_y, frame,
                        /*is_row=*/false, non_zero_coeff_count);
}

template void Reconstruct(const dsp::Dsp& dsp, TransformType tx_type,
                          TransformSize tx_size, bool lossless, int16_t* buffer,
                          int start_x, int start_y, Array2DView<uint8_t>* frame,
                          int non_zero_coeff_count);
#if LIBGAV1_MAX_BITDEPTH >= 10
template void Reconstruct(const dsp::Dsp& dsp, TransformType tx_type,
                          TransformSize tx_size, bool lossless, int32_t* buffer,
                          int start_x, int start_y,
                          Array2DView<uint16_t>* frame,
                          int non_zero_coeff_count);
#endif

}  // namespace libgav1
