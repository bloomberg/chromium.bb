/*
 * Copyright 2019 The libgav1 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBGAV1_SRC_DSP_FILM_GRAIN_IMPL_H_
#define LIBGAV1_SRC_DSP_FILM_GRAIN_IMPL_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

#include "src/dsp/common.h"
#include "src/utils/array_2d.h"
#include "src/utils/constants.h"
#include "src/utils/cpu.h"

namespace libgav1 {
namespace dsp {
namespace film_grain {

// bitdepth  min grain    max grain
// ------------------------------
//     8       -128        127
//    10       -512        511
//    12      -2048       2047
//
// So int8_t is big enough for bitdepth 8, whereas bitdepths 10 and 12 need
// int16_t.
template <int bitdepth>
using GrainTypeForBpp =
    typename std::conditional<bitdepth == 8, int8_t, int16_t>::type;

template <int bitdepth>
int GetGrainMax() {
  return (1 << (bitdepth - 1)) - 1;
}

template <int bitdepth>
int GetGrainMin() {
  return -(1 << (bitdepth - 1));
}

int GetRandomNumber(int bits, uint16_t* seed);

enum {
  kAutoRegressionBorder = 3,
  // The width of the luma noise array.
  kLumaWidth = 82,
  // The height of the luma noise array.
  kLumaHeight = 73,
  // The two possible widths of the chroma noise array.
  kMinChromaWidth = 44,
  kMaxChromaWidth = 82,
  // The two possible heights of the chroma noise array.
  kMinChromaHeight = 38,
  kMaxChromaHeight = 73,
  // The scaling lookup table maps bytes to bytes, so only uses 256 elements,
  // plus one for overflow in 10bit lookups.
  kScalingLookupTableSize = 257,
  // Padding is added to the scaling lookup table to permit overwrites by
  // InitializeScalingLookupTable_NEON.
  kScalingLookupTablePadding = 6,
  // Padding is added to each row of the noise image to permit overreads by
  // BlendNoiseWithImageLuma_NEON and overwrites by WriteOverlapLine8bpp_NEON.
  kNoiseImagePadding = 7,
  // Padding is added to the end of the |noise_stripes_| buffer to permit
  // overreads by WriteOverlapLine8bpp_NEON.
  kNoiseStripePadding = 7,
};  // anonymous enum

// Section 7.18.3.5. Add noise synthesis process.
template <int bitdepth>
class FilmGrain {
 public:
  using GrainType = GrainTypeForBpp<bitdepth>;

  FilmGrain(const FilmGrainParams& params, bool is_monochrome,
            bool color_matrix_is_identity, int subsampling_x, int subsampling_y,
            int width, int height);

  // Note: These static methods are declared public so that the unit tests can
  // call them.

  static void GenerateLumaGrain(const FilmGrainParams& params,
                                GrainType* luma_grain);

  // Generates white noise arrays u_grain and v_grain chroma_width samples wide
  // and chroma_height samples high.
  static void GenerateChromaGrains(const FilmGrainParams& params,
                                   int chroma_width, int chroma_height,
                                   GrainType* u_grain, GrainType* v_grain);

  // Copies rows from |noise_stripes| to |noise_image|, skipping rows that are
  // subject to overlap.
  static void ConstructNoiseImage(const Array2DView<GrainType>* noise_stripes,
                                  int width, int height, int subsampling_x,
                                  int subsampling_y, int stripe_start_offset,
                                  Array2D<GrainType>* noise_image);

  static void InitializeScalingLookupTable(int num_points,
                                           const uint8_t point_value[],
                                           const uint8_t point_scaling[],
                                           uint8_t scaling_lut[256]);

  // Combines the film grain with the image data.
  bool AddNoise(const void* source_plane_y, ptrdiff_t source_stride_y,
                const void* source_plane_u, ptrdiff_t source_stride_u,
                const void* source_plane_v, ptrdiff_t source_stride_v,
                void* dest_plane_y, ptrdiff_t dest_stride_y, void* dest_plane_u,
                ptrdiff_t dest_stride_u, void* dest_plane_v,
                ptrdiff_t dest_stride_v);

 private:
  using Pixel =
      typename std::conditional<bitdepth == 8, uint8_t, uint16_t>::type;

  bool Init();

  // Allocates noise_stripes_.
  bool AllocateNoiseStripes();

  bool AllocateNoiseImage();

  const FilmGrainParams& params_;
  const bool is_monochrome_;
  const bool color_matrix_is_identity_;
  const int subsampling_x_;
  const int subsampling_y_;
  const int width_;
  const int height_;
  const int chroma_width_;
  const int chroma_height_;
  // The luma_grain array contains white noise generated for luma.
  // The array size is fixed but subject to further optimization for SIMD.
  GrainType luma_grain_[kLumaHeight * kLumaWidth];
  // The maximum size of the u_grain and v_grain arrays is
  // kMaxChromaHeight * kMaxChromaWidth. The actual size is
  // chroma_height_ * chroma_width_.
  GrainType u_grain_[kMaxChromaHeight * kMaxChromaWidth];
  GrainType v_grain_[kMaxChromaHeight * kMaxChromaWidth];
  // Scaling lookup tables.
  uint8_t scaling_lut_y_[kScalingLookupTableSize + kScalingLookupTablePadding];
  uint8_t* scaling_lut_u_ = nullptr;
  uint8_t* scaling_lut_v_ = nullptr;
  // If allocated, this buffer is 256 * 2 bytes long and scaling_lut_u_ and
  // scaling_lut_v_ point into this buffer. Otherwise, scaling_lut_u_ and
  // scaling_lut_v_ point to scaling_lut_y_.
  std::unique_ptr<uint8_t[]> scaling_lut_chroma_buffer_;

  // A two-dimensional array of noise data for each plane. Generated for each 32
  // luma sample high stripe of the image. The first dimension is called
  // luma_num. The second dimension is the size of one noise stripe.
  //
  // Each row of the Array2DView noise_stripes_[plane] is a conceptually
  // two-dimensional array of |GrainType|s. The two-dimensional array of
  // |GrainType|s is flattened into a one-dimensional buffer in this
  // implementation.
  //
  // noise_stripes_[kPlaneY][luma_num] is an array that has 34 rows and
  // |width_| columns and contains noise for the luma component.
  //
  // noise_stripes_[kPlaneU][luma_num] or noise_stripes_[kPlaneV][luma_num]
  // is an array that has (34 >> subsampling_y_) rows and
  // RightShiftWithRounding(width_, subsampling_x_) columns and contains noise
  // for the chroma components.
  Array2DView<GrainType> noise_stripes_[kMaxPlanes];
  // Owns the memory that the elements of noise_stripes_ point to.
  std::unique_ptr<GrainType[]> noise_buffer_;

  Array2D<GrainType> noise_image_[kMaxPlanes];
};

}  // namespace film_grain
}  // namespace dsp
}  // namespace libgav1

#endif  // LIBGAV1_SRC_DSP_FILM_GRAIN_IMPL_H_
