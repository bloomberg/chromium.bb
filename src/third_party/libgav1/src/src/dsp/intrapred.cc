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

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>  // memset

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"
#include "src/utils/memory.h"

namespace libgav1 {
namespace dsp {
namespace {

constexpr TransformSize kTransformSizesLargerThan32x32[] = {
    kTransformSize16x64, kTransformSize32x64, kTransformSize64x16,
    kTransformSize64x32, kTransformSize64x64};

template <int block_width, int block_height, typename Pixel>
struct IntraPredFuncs_C {
  IntraPredFuncs_C() = delete;

  static void DcTop(void* dest, ptrdiff_t stride, const void* top_row,
                    const void* left_column);
  static void DcLeft(void* dest, ptrdiff_t stride, const void* top_row,
                     const void* left_column);
  static void Dc(void* dest, ptrdiff_t stride, const void* top_row,
                 const void* left_column);
  static void Vertical(void* dest, ptrdiff_t stride, const void* top_row,
                       const void* left_column);
  static void Horizontal(void* dest, ptrdiff_t stride, const void* top_row,
                         const void* left_column);
  static void Paeth(void* dest, ptrdiff_t stride, const void* top_row,
                    const void* left_column);
  static void Smooth(void* dest, ptrdiff_t stride, const void* top_row,
                     const void* left_column);
  static void SmoothVertical(void* dest, ptrdiff_t stride, const void* top_row,
                             const void* left_column);
  static void SmoothHorizontal(void* dest, ptrdiff_t stride,
                               const void* top_row, const void* left_column);
};

// Intra-predictors that require bitdepth.
template <int block_width, int block_height, int bitdepth, typename Pixel>
struct IntraPredBppFuncs_C {
  IntraPredBppFuncs_C() = delete;

  static void DcFill(void* dest, ptrdiff_t stride, const void* top_row,
                     const void* left_column);
};

//------------------------------------------------------------------------------
// IntraPredFuncs_C::DcPred

template <int block_width, int block_height, typename Pixel>
void IntraPredFuncs_C<block_width, block_height, Pixel>::DcTop(
    void* const dest, ptrdiff_t stride, const void* const top_row,
    const void* /*left_column*/) {
  int sum = block_width >> 1;  // rounder
  const auto* const top = static_cast<const Pixel*>(top_row);
  for (int x = 0; x < block_width; ++x) sum += top[x];
  const int dc = sum >> FloorLog2(block_width);

  auto* dst = static_cast<Pixel*>(dest);
  stride /= sizeof(Pixel);
  for (int y = 0; y < block_height; ++y) {
    Memset(dst, dc, block_width);
    dst += stride;
  }
}

template <int block_width, int block_height, typename Pixel>
void IntraPredFuncs_C<block_width, block_height, Pixel>::DcLeft(
    void* const dest, ptrdiff_t stride, const void* /*top_row*/,
    const void* const left_column) {
  int sum = block_height >> 1;  // rounder
  const auto* const left = static_cast<const Pixel*>(left_column);
  for (int y = 0; y < block_height; ++y) sum += left[y];
  const int dc = sum >> FloorLog2(block_height);

  auto* dst = static_cast<Pixel*>(dest);
  stride /= sizeof(Pixel);
  for (int y = 0; y < block_height; ++y) {
    Memset(dst, dc, block_width);
    dst += stride;
  }
}

// Note for square blocks the divide in the Dc() function reduces to a shift.
// For rectangular block sizes the following multipliers can be used with the
// corresponding shifts.
// 8-bit
//  1:2 (e.g,, 4x8):  scale = 0x5556
//  1:4 (e.g., 4x16): scale = 0x3334
//  final_descale = 16
// 10/12-bit
//  1:2: scale = 0xaaab
//  1:4: scale = 0x6667
//  final_descale = 17
//  Note these may be halved to the values used in 8-bit in all cases except
//  when bitdepth == 12 and block_width + block_height is divisible by 5 (as
//  opposed to 3).
//
// The calculation becomes:
//  (dc_sum >> intermediate_descale) * scale) >> final_descale
// where intermediate_descale is:
// sum = block_width + block_height
// intermediate_descale =
//     (sum <= 20) ? 2 : (sum <= 40) ? 3 : (sum <= 80) ? 4 : 5
//
// The constants (multiplier and shifts) for a given block size are obtained
// as follows:
// - Let sum = block width + block height
// - Shift 'sum' right until we reach an odd number
// - Let the number of shifts for that block size be called 'intermediate_scale'
//   and let the odd number be 'd' (d has only 2 possible values: d = 3 for a
//   1:2 rectangular block and d = 5 for a 1:4 rectangular block).
// - Find multipliers by dividing by 'd' using "Algorithm 1" in:
//   http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=1467632
//   by ensuring that m + n = 16 (in that algorithm). This ensures that our 2nd
//   shift will be 16, regardless of the block size.
// TODO(jzern): the base implementation could be updated to use this method.

template <int block_width, int block_height, typename Pixel>
void IntraPredFuncs_C<block_width, block_height, Pixel>::Dc(
    void* const dest, ptrdiff_t stride, const void* const top_row,
    const void* const left_column) {
  const int divisor = block_width + block_height;
  int sum = divisor >> 1;  // rounder

  const auto* const top = static_cast<const Pixel*>(top_row);
  const auto* const left = static_cast<const Pixel*>(left_column);
  for (int x = 0; x < block_width; ++x) sum += top[x];
  for (int y = 0; y < block_height; ++y) sum += left[y];

  const int dc = sum / divisor;

  auto* dst = static_cast<Pixel*>(dest);
  stride /= sizeof(Pixel);
  for (int y = 0; y < block_height; ++y) {
    Memset(dst, dc, block_width);
    dst += stride;
  }
}

//------------------------------------------------------------------------------
// IntraPredFuncs_C directional predictors

// IntraPredFuncs_C::Vertical -- apply top row vertically
template <int block_width, int block_height, typename Pixel>
void IntraPredFuncs_C<block_width, block_height, Pixel>::Vertical(
    void* const dest, ptrdiff_t stride, const void* const top_row,
    const void* /*left_column*/) {
  auto* dst = static_cast<uint8_t*>(dest);
  for (int y = 0; y < block_height; ++y) {
    memcpy(dst, top_row, block_width * sizeof(Pixel));
    dst += stride;
  }
}

// IntraPredFuncs_C::Horizontal -- apply left column horizontally
template <int block_width, int block_height, typename Pixel>
void IntraPredFuncs_C<block_width, block_height, Pixel>::Horizontal(
    void* const dest, ptrdiff_t stride, const void* /*top_row*/,
    const void* const left_column) {
  const auto* const left = static_cast<const Pixel*>(left_column);
  auto* dst = static_cast<Pixel*>(dest);
  stride /= sizeof(Pixel);
  for (int y = 0; y < block_height; ++y) {
    Memset(dst, left[y], block_width);
    dst += stride;
  }
}

template <typename Pixel>
inline Pixel Average(Pixel a, Pixel b) {
  return static_cast<Pixel>((a + b + 1) >> 1);
}

template <typename Pixel>
inline Pixel Average(Pixel a, Pixel b, Pixel c) {
  return static_cast<Pixel>((a + 2 * b + c + 2) >> 2);
}

// IntraPredFuncs_C::Paeth
template <int block_width, int block_height, typename Pixel>
void IntraPredFuncs_C<block_width, block_height, Pixel>::Paeth(
    void* const dest, ptrdiff_t stride, const void* const top_row,
    const void* const left_column) {
  const auto* const top = static_cast<const Pixel*>(top_row);
  const auto* const left = static_cast<const Pixel*>(left_column);
  const Pixel top_left = top[-1];
  const int top_left_x2 = top_left + top_left;
  auto* dst = static_cast<Pixel*>(dest);
  stride /= sizeof(Pixel);

  for (int y = 0; y < block_height; ++y) {
    const int left_pixel = left[y];
    for (int x = 0; x < block_width; ++x) {
      // The Paeth filter selects the value closest to:
      // top[x] + left[y] - top_left
      // To calculate the absolute distance for the left value this would be:
      // abs((top[x] + left[y] - top_left) - left[y])
      // or, because left[y] cancels out:
      // abs(top[x] - top_left)
      const int left_dist = std::abs(top[x] - top_left);
      const int top_dist = std::abs(left_pixel - top_left);
      const int top_left_dist = std::abs(top[x] + left_pixel - top_left_x2);

      // Select the closest value to the initial estimate of 'T + L - TL'.
      if (left_dist <= top_dist && left_dist <= top_left_dist) {
        dst[x] = left_pixel;
      } else if (top_dist <= top_left_dist) {
        dst[x] = top[x];
      } else {
        dst[x] = top_left;
      }
    }
    dst += stride;
  }
}

constexpr uint8_t kSmoothWeights[] = {
    // block dimension = 4
    255, 149, 85, 64,
    // block dimension = 8
    255, 197, 146, 105, 73, 50, 37, 32,
    // block dimension = 16
    255, 225, 196, 170, 145, 123, 102, 84, 68, 54, 43, 33, 26, 20, 17, 16,
    // block dimension = 32
    255, 240, 225, 210, 196, 182, 169, 157, 145, 133, 122, 111, 101, 92, 83, 74,
    66, 59, 52, 45, 39, 34, 29, 25, 21, 17, 14, 12, 10, 9, 8, 8,
    // block dimension = 64
    255, 248, 240, 233, 225, 218, 210, 203, 196, 189, 182, 176, 169, 163, 156,
    150, 144, 138, 133, 127, 121, 116, 111, 106, 101, 96, 91, 86, 82, 77, 73,
    69, 65, 61, 57, 54, 50, 47, 44, 41, 38, 35, 32, 29, 27, 25, 22, 20, 18, 16,
    15, 13, 12, 10, 9, 8, 7, 6, 6, 5, 5, 4, 4, 4};

// IntraPredFuncs_C::Smooth
template <int block_width, int block_height, typename Pixel>
void IntraPredFuncs_C<block_width, block_height, Pixel>::Smooth(
    void* const dest, ptrdiff_t stride, const void* const top_row,
    const void* const left_column) {
  const auto* const top = static_cast<const Pixel*>(top_row);
  const auto* const left = static_cast<const Pixel*>(left_column);
  const Pixel top_right = top[block_width - 1];
  const Pixel bottom_left = left[block_height - 1];
  static_assert(
      block_width >= 4 && block_height >= 4,
      "Weights for smooth predictor undefined for block width/height < 4");
  const uint8_t* const weights_x = kSmoothWeights + block_width - 4;
  const uint8_t* const weights_y = kSmoothWeights + block_height - 4;
  const uint16_t scale_value = (1 << kSmoothWeightScale);
  auto* dst = static_cast<Pixel*>(dest);
  stride /= sizeof(Pixel);

  for (int y = 0; y < block_height; ++y) {
    for (int x = 0; x < block_width; ++x) {
      assert(scale_value >= weights_y[y] && scale_value >= weights_x[x]);
      uint32_t pred = weights_y[y] * top[x];
      pred += weights_x[x] * left[y];
      pred += static_cast<uint8_t>(scale_value - weights_y[y]) * bottom_left;
      pred += static_cast<uint8_t>(scale_value - weights_x[x]) * top_right;
      // The maximum value of pred with the rounder is 2^9 * (2^bitdepth - 1)
      // + 256. With the descale there's no need for saturation.
      dst[x] = static_cast<Pixel>(
          RightShiftWithRounding(pred, kSmoothWeightScale + 1));
    }
    dst += stride;
  }
}

// IntraPredFuncs_C::SmoothVertical
template <int block_width, int block_height, typename Pixel>
void IntraPredFuncs_C<block_width, block_height, Pixel>::SmoothVertical(
    void* const dest, ptrdiff_t stride, const void* const top_row,
    const void* const left_column) {
  const auto* const top = static_cast<const Pixel*>(top_row);
  const auto* const left = static_cast<const Pixel*>(left_column);
  const Pixel bottom_left = left[block_height - 1];
  static_assert(block_height >= 4,
                "Weights for smooth predictor undefined for block height < 4");
  const uint8_t* const weights_y = kSmoothWeights + block_height - 4;
  const uint16_t scale_value = (1 << kSmoothWeightScale);
  auto* dst = static_cast<Pixel*>(dest);
  stride /= sizeof(Pixel);

  for (int y = 0; y < block_height; ++y) {
    for (int x = 0; x < block_width; ++x) {
      assert(scale_value >= weights_y[y]);
      uint32_t pred = weights_y[y] * top[x];
      pred += static_cast<uint8_t>(scale_value - weights_y[y]) * bottom_left;
      dst[x] =
          static_cast<Pixel>(RightShiftWithRounding(pred, kSmoothWeightScale));
    }
    dst += stride;
  }
}

// IntraPredFuncs_C::SmoothHorizontal
template <int block_width, int block_height, typename Pixel>
void IntraPredFuncs_C<block_width, block_height, Pixel>::SmoothHorizontal(
    void* const dest, ptrdiff_t stride, const void* const top_row,
    const void* const left_column) {
  const auto* const top = static_cast<const Pixel*>(top_row);
  const auto* const left = static_cast<const Pixel*>(left_column);
  const Pixel top_right = top[block_width - 1];
  static_assert(block_width >= 4,
                "Weights for smooth predictor undefined for block width < 4");
  const uint8_t* const weights_x = kSmoothWeights + block_width - 4;
  const uint16_t scale_value = (1 << kSmoothWeightScale);
  auto* dst = static_cast<Pixel*>(dest);
  stride /= sizeof(Pixel);

  for (int y = 0; y < block_height; ++y) {
    for (int x = 0; x < block_width; ++x) {
      assert(scale_value >= weights_x[x]);
      uint32_t pred = weights_x[x] * left[y];
      pred += static_cast<uint8_t>(scale_value - weights_x[x]) * top_right;
      dst[x] =
          static_cast<Pixel>(RightShiftWithRounding(pred, kSmoothWeightScale));
    }
    dst += stride;
  }
}

//------------------------------------------------------------------------------
// IntraPredBppFuncs_C
template <int fill, typename Pixel>
inline void DcFill_C(void* const dest, ptrdiff_t stride, const int block_width,
                     const int block_height) {
  static_assert(sizeof(Pixel) == 1 || sizeof(Pixel) == 2,
                "Only 1 & 2 byte pixels are supported");

  auto* dst = static_cast<Pixel*>(dest);
  stride /= sizeof(Pixel);
  for (int y = 0; y < block_height; ++y) {
    Memset(dst, fill, block_width);
    dst += stride;
  }
}

template <int block_width, int block_height, int bitdepth, typename Pixel>
void IntraPredBppFuncs_C<block_width, block_height, bitdepth, Pixel>::DcFill(
    void* const dest, ptrdiff_t stride, const void* /*top_row*/,
    const void* /*left_column*/) {
  DcFill_C<0x80 << (bitdepth - 8), Pixel>(dest, stride, block_width,
                                          block_height);
}

//------------------------------------------------------------------------------
// FilterIntraPredictor_C

template <int bitdepth, typename Pixel>
void FilterIntraPredictor_C(void* const dest, ptrdiff_t stride,
                            const void* const top_row,
                            const void* const left_column,
                            const FilterIntraPredictor pred, const int width,
                            const int height) {
  const int kMaxPixel = (1 << bitdepth) - 1;
  const auto* const top = static_cast<const Pixel*>(top_row);
  const auto* const left = static_cast<const Pixel*>(left_column);

  assert(width <= 32 && height <= 32);

  Pixel buffer[3][33];  // cache 2 rows + top & left boundaries
  memcpy(buffer[0], &top[-1], (width + 1) * sizeof(top[0]));

  auto* dst = static_cast<Pixel*>(dest);
  stride /= sizeof(Pixel);
  int row0 = 0, row2 = 2;
  int ystep = 1;
  int y = 0;
  do {
    buffer[1][0] = left[y];
    buffer[row2][0] = left[y + 1];
    int x = 1;
    do {
      const Pixel p0 = buffer[row0][x - 1];  // top-left
      const Pixel p1 = buffer[row0][x + 0];  // top 0
      const Pixel p2 = buffer[row0][x + 1];  // top 1
      const Pixel p3 = buffer[row0][x + 2];  // top 2
      const Pixel p4 = buffer[row0][x + 3];  // top 3
      const Pixel p5 = buffer[1][x - 1];     // left 0
      const Pixel p6 = buffer[row2][x - 1];  // left 1
      for (int i = 0; i < 8; ++i) {
        const int xoffset = i & 0x03;
        const int yoffset = (i >> 2) * ystep;
        const int value = kFilterIntraTaps[pred][i][0] * p0 +
                          kFilterIntraTaps[pred][i][1] * p1 +
                          kFilterIntraTaps[pred][i][2] * p2 +
                          kFilterIntraTaps[pred][i][3] * p3 +
                          kFilterIntraTaps[pred][i][4] * p4 +
                          kFilterIntraTaps[pred][i][5] * p5 +
                          kFilterIntraTaps[pred][i][6] * p6;
        buffer[1 + yoffset][x + xoffset] = static_cast<Pixel>(
            Clip3(RightShiftWithRounding(value, 4), 0, kMaxPixel));
      }
      x += 4;
    } while (x < width);
    memcpy(dst, &buffer[1][1], width * sizeof(dst[0]));
    dst += stride;
    memcpy(dst, &buffer[row2][1], width * sizeof(dst[0]));
    dst += stride;

    // The final row becomes the top for the next pass.
    row0 ^= 2;
    row2 ^= 2;
    ystep = -ystep;
    y += 2;
  } while (y < height);
}

//------------------------------------------------------------------------------
// CflIntraPredictor_C

// |luma| can be within +/-(((1 << bitdepth) - 1) << 3), inclusive.
// |alpha| can be -16 to 16 (inclusive).
template <int block_width, int block_height, int bitdepth, typename Pixel>
void CflIntraPredictor_C(
    void* const dest, ptrdiff_t stride,
    const int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int alpha) {
  auto* dst = static_cast<Pixel*>(dest);
  const int dc = dst[0];
  stride /= sizeof(Pixel);
  const int max_value = (1 << bitdepth) - 1;
  for (int y = 0; y < block_height; ++y) {
    for (int x = 0; x < block_width; ++x) {
      assert(luma[y][x] >= -(((1 << bitdepth) - 1) << 3));
      assert(luma[y][x] <= ((1 << bitdepth) - 1) << 3);
      dst[x] = Clip3(dc + RightShiftWithRoundingSigned(alpha * luma[y][x], 6),
                     0, max_value);
    }
    dst += stride;
  }
}

//------------------------------------------------------------------------------
// CflSubsampler_C

template <int block_width, int block_height, int bitdepth, typename Pixel,
          int subsampling_x, int subsampling_y>
void CflSubsampler_C(int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
                     const int max_luma_width, const int max_luma_height,
                     const void* const source, ptrdiff_t stride) {
  assert(max_luma_width >= 4);
  assert(max_luma_height >= 4);
  const auto* src = static_cast<const Pixel*>(source);
  stride /= sizeof(Pixel);
  int sum = 0;
  for (int y = 0; y < block_height; ++y) {
    for (int x = 0; x < block_width; ++x) {
      const ptrdiff_t luma_x =
          std::min(x << subsampling_x, max_luma_width - (1 << subsampling_x));
      const ptrdiff_t luma_x_next = luma_x + stride;
      luma[y][x] =
          (src[luma_x] + ((subsampling_x != 0) ? src[luma_x + 1] : 0) +
           ((subsampling_y != 0) ? (src[luma_x_next] + src[luma_x_next + 1])
                                 : 0))
          << (3 - subsampling_x - subsampling_y);
      sum += luma[y][x];
    }
    if ((y << subsampling_y) < (max_luma_height - (1 << subsampling_y))) {
      src += stride << subsampling_y;
    }
  }
  const int average = RightShiftWithRounding(
      sum, FloorLog2(block_width) + FloorLog2(block_height));
  for (int y = 0; y < block_height; ++y) {
    for (int x = 0; x < block_width; ++x) {
      luma[y][x] -= average;
    }
  }
}

//------------------------------------------------------------------------------
// 7.11.2.4. Directional intra prediction process

template <typename Pixel>
void DirectionalIntraPredictorZone1_C(void* const dest, ptrdiff_t stride,
                                      const void* const top_row,
                                      const int width, const int height,
                                      const int xstep,
                                      const bool upsampled_top) {
  const auto* const top = static_cast<const Pixel*>(top_row);
  auto* dst = static_cast<Pixel*>(dest);
  stride /= sizeof(Pixel);

  assert(xstep > 0);

  // If xstep == 64 then |shift| always evaluates to 0 which sets |val| to
  // |top[top_base_x]|. This corresponds to a 45 degree prediction.
  if (xstep == 64) {
    // 7.11.2.10. Intra edge upsample selection process
    // if ( d <= 0 || d >= 40 ) useUpsample = 0
    // For |upsampled_top| the delta is |predictor_angle - 90|. Since the
    // |predictor_angle| is 45 the delta is also 45.
    assert(!upsampled_top);
    const Pixel* top_ptr = top + 1;
    for (int y = 0; y < height; ++y, dst += stride, ++top_ptr) {
      memcpy(dst, top_ptr, sizeof(*top_ptr) * width);
    }
    return;
  }

  const int upsample_shift = static_cast<int>(upsampled_top);
  const int max_base_x = ((width + height) - 1) << upsample_shift;
  const int scale_bits = 6 - upsample_shift;
  const int base_step = 1 << upsample_shift;
  int top_x = xstep;
  int y = 0;
  do {
    int top_base_x = top_x >> scale_bits;

    if (top_base_x >= max_base_x) {
      for (int i = y; i < height; ++i) {
        Memset(dst, top[max_base_x], width);
        dst += stride;
      }
      return;
    }

    const int shift = ((top_x << upsample_shift) & 0x3F) >> 1;
    int x = 0;
    do {
      if (top_base_x >= max_base_x) {
        Memset(dst + x, top[max_base_x], width - x);
        break;
      }

      const int val =
          top[top_base_x] * (32 - shift) + top[top_base_x + 1] * shift;
      dst[x] = RightShiftWithRounding(val, 5);
      top_base_x += base_step;
    } while (++x < width);

    dst += stride;
    top_x += xstep;
  } while (++y < height);
}

template <typename Pixel>
void DirectionalIntraPredictorZone2_C(void* const dest, ptrdiff_t stride,
                                      const void* const top_row,
                                      const void* const left_column,
                                      const int width, const int height,
                                      const int xstep, const int ystep,
                                      const bool upsampled_top,
                                      const bool upsampled_left) {
  const auto* const top = static_cast<const Pixel*>(top_row);
  const auto* const left = static_cast<const Pixel*>(left_column);
  auto* dst = static_cast<Pixel*>(dest);
  stride /= sizeof(Pixel);

  assert(xstep > 0);
  assert(ystep > 0);

  const int upsample_top_shift = static_cast<int>(upsampled_top);
  const int upsample_left_shift = static_cast<int>(upsampled_left);
  const int scale_bits_x = 6 - upsample_top_shift;
  const int scale_bits_y = 6 - upsample_left_shift;
  const int min_base_x = -(1 << upsample_top_shift);
  const int base_step_x = 1 << upsample_top_shift;
  int y = 0;
  int top_x = -xstep;
  do {
    int top_base_x = top_x >> scale_bits_x;
    int left_y = (y << 6) - ystep;
    int x = 0;
    do {
      int val;
      if (top_base_x >= min_base_x) {
        const int shift = ((top_x * (1 << upsample_top_shift)) & 0x3F) >> 1;
        val = top[top_base_x] * (32 - shift) + top[top_base_x + 1] * shift;
      } else {
        // Note this assumes an arithmetic shift to handle negative values.
        const int left_base_y = left_y >> scale_bits_y;
        const int shift = ((left_y * (1 << upsample_left_shift)) & 0x3F) >> 1;
        assert(left_base_y >= -(1 << upsample_left_shift));
        val = left[left_base_y] * (32 - shift) + left[left_base_y + 1] * shift;
      }
      dst[x] = RightShiftWithRounding(val, 5);
      top_base_x += base_step_x;
      left_y -= ystep;
    } while (++x < width);

    top_x -= xstep;
    dst += stride;
  } while (++y < height);
}

template <typename Pixel>
void DirectionalIntraPredictorZone3_C(void* const dest, ptrdiff_t stride,
                                      const void* const left_column,
                                      const int width, const int height,
                                      const int ystep,
                                      const bool upsampled_left) {
  const auto* const left = static_cast<const Pixel*>(left_column);
  stride /= sizeof(Pixel);

  assert(ystep > 0);

  const int upsample_shift = static_cast<int>(upsampled_left);
  const int scale_bits = 6 - upsample_shift;
  const int base_step = 1 << upsample_shift;
  // Zone3 never runs out of left_column values.
  assert((width + height - 1) << upsample_shift >  // max_base_y
         ((ystep * width) >> scale_bits) +
             base_step * (height - 1));  // left_base_y

  int left_y = ystep;
  int x = 0;
  do {
    auto* dst = static_cast<Pixel*>(dest);

    int left_base_y = left_y >> scale_bits;
    int y = 0;
    do {
      const int shift = ((left_y << upsample_shift) & 0x3F) >> 1;
      const int val =
          left[left_base_y] * (32 - shift) + left[left_base_y + 1] * shift;
      dst[x] = RightShiftWithRounding(val, 5);
      dst += stride;
      left_base_y += base_step;
    } while (++y < height);

    left_y += ystep;
  } while (++x < width);
}

//------------------------------------------------------------------------------

template <typename Pixel>
struct IntraPredDefs {
  IntraPredDefs() = delete;

  using _4x4 = IntraPredFuncs_C<4, 4, Pixel>;
  using _4x8 = IntraPredFuncs_C<4, 8, Pixel>;
  using _4x16 = IntraPredFuncs_C<4, 16, Pixel>;
  using _8x4 = IntraPredFuncs_C<8, 4, Pixel>;
  using _8x8 = IntraPredFuncs_C<8, 8, Pixel>;
  using _8x16 = IntraPredFuncs_C<8, 16, Pixel>;
  using _8x32 = IntraPredFuncs_C<8, 32, Pixel>;
  using _16x4 = IntraPredFuncs_C<16, 4, Pixel>;
  using _16x8 = IntraPredFuncs_C<16, 8, Pixel>;
  using _16x16 = IntraPredFuncs_C<16, 16, Pixel>;
  using _16x32 = IntraPredFuncs_C<16, 32, Pixel>;
  using _16x64 = IntraPredFuncs_C<16, 64, Pixel>;
  using _32x8 = IntraPredFuncs_C<32, 8, Pixel>;
  using _32x16 = IntraPredFuncs_C<32, 16, Pixel>;
  using _32x32 = IntraPredFuncs_C<32, 32, Pixel>;
  using _32x64 = IntraPredFuncs_C<32, 64, Pixel>;
  using _64x16 = IntraPredFuncs_C<64, 16, Pixel>;
  using _64x32 = IntraPredFuncs_C<64, 32, Pixel>;
  using _64x64 = IntraPredFuncs_C<64, 64, Pixel>;
};

template <int bitdepth, typename Pixel>
struct IntraPredBppDefs {
  IntraPredBppDefs() = delete;

  using _4x4 = IntraPredBppFuncs_C<4, 4, bitdepth, Pixel>;
  using _4x8 = IntraPredBppFuncs_C<4, 8, bitdepth, Pixel>;
  using _4x16 = IntraPredBppFuncs_C<4, 16, bitdepth, Pixel>;
  using _8x4 = IntraPredBppFuncs_C<8, 4, bitdepth, Pixel>;
  using _8x8 = IntraPredBppFuncs_C<8, 8, bitdepth, Pixel>;
  using _8x16 = IntraPredBppFuncs_C<8, 16, bitdepth, Pixel>;
  using _8x32 = IntraPredBppFuncs_C<8, 32, bitdepth, Pixel>;
  using _16x4 = IntraPredBppFuncs_C<16, 4, bitdepth, Pixel>;
  using _16x8 = IntraPredBppFuncs_C<16, 8, bitdepth, Pixel>;
  using _16x16 = IntraPredBppFuncs_C<16, 16, bitdepth, Pixel>;
  using _16x32 = IntraPredBppFuncs_C<16, 32, bitdepth, Pixel>;
  using _16x64 = IntraPredBppFuncs_C<16, 64, bitdepth, Pixel>;
  using _32x8 = IntraPredBppFuncs_C<32, 8, bitdepth, Pixel>;
  using _32x16 = IntraPredBppFuncs_C<32, 16, bitdepth, Pixel>;
  using _32x32 = IntraPredBppFuncs_C<32, 32, bitdepth, Pixel>;
  using _32x64 = IntraPredBppFuncs_C<32, 64, bitdepth, Pixel>;
  using _64x16 = IntraPredBppFuncs_C<64, 16, bitdepth, Pixel>;
  using _64x32 = IntraPredBppFuncs_C<64, 32, bitdepth, Pixel>;
  using _64x64 = IntraPredBppFuncs_C<64, 64, bitdepth, Pixel>;
};

using Defs = IntraPredDefs<uint8_t>;
using Defs8bpp = IntraPredBppDefs<8, uint8_t>;

// Initializes dsp entries for kTransformSize|W|x|H| from |DEFS|/|DEFSBPP| of
// the same size.
#define INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, W, H)                         \
  dsp->intra_predictors[kTransformSize##W##x##H][kIntraPredictorDcFill] =     \
      DEFSBPP::_##W##x##H::DcFill;                                            \
  dsp->intra_predictors[kTransformSize##W##x##H][kIntraPredictorDcTop] =      \
      DEFS::_##W##x##H::DcTop;                                                \
  dsp->intra_predictors[kTransformSize##W##x##H][kIntraPredictorDcLeft] =     \
      DEFS::_##W##x##H::DcLeft;                                               \
  dsp->intra_predictors[kTransformSize##W##x##H][kIntraPredictorDc] =         \
      DEFS::_##W##x##H::Dc;                                                   \
  dsp->intra_predictors[kTransformSize##W##x##H][kIntraPredictorVertical] =   \
      DEFS::_##W##x##H::Vertical;                                             \
  dsp->intra_predictors[kTransformSize##W##x##H][kIntraPredictorHorizontal] = \
      DEFS::_##W##x##H::Horizontal;                                           \
  dsp->intra_predictors[kTransformSize##W##x##H][kIntraPredictorPaeth] =      \
      DEFS::_##W##x##H::Paeth;                                                \
  dsp->intra_predictors[kTransformSize##W##x##H][kIntraPredictorSmooth] =     \
      DEFS::_##W##x##H::Smooth;                                               \
  dsp->intra_predictors[kTransformSize##W##x##H]                              \
                       [kIntraPredictorSmoothVertical] =                      \
      DEFS::_##W##x##H::SmoothVertical;                                       \
  dsp->intra_predictors[kTransformSize##W##x##H]                              \
                       [kIntraPredictorSmoothHorizontal] =                    \
      DEFS::_##W##x##H::SmoothHorizontal

#define INIT_INTRAPREDICTORS(DEFS, DEFSBPP)        \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 4, 4);   \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 4, 8);   \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 4, 16);  \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 8, 4);   \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 8, 8);   \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 8, 16);  \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 8, 32);  \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 16, 4);  \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 16, 8);  \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 16, 16); \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 16, 32); \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 16, 64); \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 32, 8);  \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 32, 16); \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 32, 32); \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 32, 64); \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 64, 16); \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 64, 32); \
  INIT_INTRAPREDICTORS_WxH(DEFS, DEFSBPP, 64, 64)

#define INIT_CFL_INTRAPREDICTOR_WxH(W, H, BITDEPTH, PIXEL)             \
  dsp->cfl_intra_predictors[kTransformSize##W##x##H] =                 \
      CflIntraPredictor_C<W, H, BITDEPTH, PIXEL>;                      \
  dsp->cfl_subsamplers[kTransformSize##W##x##H][kSubsamplingType444] = \
      CflSubsampler_C<W, H, BITDEPTH, PIXEL, 0, 0>;                    \
  dsp->cfl_subsamplers[kTransformSize##W##x##H][kSubsamplingType422] = \
      CflSubsampler_C<W, H, BITDEPTH, PIXEL, 1, 0>;                    \
  dsp->cfl_subsamplers[kTransformSize##W##x##H][kSubsamplingType420] = \
      CflSubsampler_C<W, H, BITDEPTH, PIXEL, 1, 1>;

#define INIT_CFL_INTRAPREDICTORS(BITDEPTH, PIXEL)       \
  INIT_CFL_INTRAPREDICTOR_WxH(4, 4, BITDEPTH, PIXEL);   \
  INIT_CFL_INTRAPREDICTOR_WxH(4, 8, BITDEPTH, PIXEL);   \
  INIT_CFL_INTRAPREDICTOR_WxH(4, 16, BITDEPTH, PIXEL);  \
  INIT_CFL_INTRAPREDICTOR_WxH(8, 4, BITDEPTH, PIXEL);   \
  INIT_CFL_INTRAPREDICTOR_WxH(8, 8, BITDEPTH, PIXEL);   \
  INIT_CFL_INTRAPREDICTOR_WxH(8, 16, BITDEPTH, PIXEL);  \
  INIT_CFL_INTRAPREDICTOR_WxH(8, 32, BITDEPTH, PIXEL);  \
  INIT_CFL_INTRAPREDICTOR_WxH(16, 4, BITDEPTH, PIXEL);  \
  INIT_CFL_INTRAPREDICTOR_WxH(16, 8, BITDEPTH, PIXEL);  \
  INIT_CFL_INTRAPREDICTOR_WxH(16, 16, BITDEPTH, PIXEL); \
  INIT_CFL_INTRAPREDICTOR_WxH(16, 32, BITDEPTH, PIXEL); \
  INIT_CFL_INTRAPREDICTOR_WxH(32, 8, BITDEPTH, PIXEL);  \
  INIT_CFL_INTRAPREDICTOR_WxH(32, 16, BITDEPTH, PIXEL); \
  INIT_CFL_INTRAPREDICTOR_WxH(32, 32, BITDEPTH, PIXEL)

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  INIT_INTRAPREDICTORS(Defs, Defs8bpp);
  dsp->directional_intra_predictor_zone1 =
      DirectionalIntraPredictorZone1_C<uint8_t>;
  dsp->directional_intra_predictor_zone2 =
      DirectionalIntraPredictorZone2_C<uint8_t>;
  dsp->directional_intra_predictor_zone3 =
      DirectionalIntraPredictorZone3_C<uint8_t>;
  dsp->filter_intra_predictor = FilterIntraPredictor_C<8, uint8_t>;
  INIT_CFL_INTRAPREDICTORS(8, uint8_t);
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDcFill] =
      Defs8bpp::_4x4::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDcTop] =
      Defs::_4x4::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDcLeft] =
      Defs::_4x4::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorDc
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDc] = Defs::_4x4::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorVertical] =
      Defs::_4x4::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorHorizontal] =
      Defs::_4x4::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorPaeth] =
      Defs::_4x4::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorSmooth] =
      Defs::_4x4::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorSmoothVertical] =
      Defs::_4x4::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorSmoothHorizontal] =
      Defs::_4x4::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorDcFill] =
      Defs8bpp::_4x8::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorDcTop] =
      Defs::_4x8::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorDcLeft] =
      Defs::_4x8::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_IntraPredictorDc
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorDc] = Defs::_4x8::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorVertical] =
      Defs::_4x8::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorHorizontal] =
      Defs::_4x8::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorPaeth] =
      Defs::_4x8::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorSmooth] =
      Defs::_4x8::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorSmoothVertical] =
      Defs::_4x8::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorSmoothHorizontal] =
      Defs::_4x8::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorDcFill] =
      Defs8bpp::_4x16::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorDcTop] =
      Defs::_4x16::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorDcLeft] =
      Defs::_4x16::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_IntraPredictorDc
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorDc] =
      Defs::_4x16::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorVertical] =
      Defs::_4x16::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorHorizontal] =
      Defs::_4x16::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorPaeth] =
      Defs::_4x16::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorSmooth] =
      Defs::_4x16::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorSmoothVertical] =
      Defs::_4x16::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorSmoothHorizontal] =
      Defs::_4x16::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorDcFill] =
      Defs8bpp::_8x4::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorDcTop] =
      Defs::_8x4::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorDcLeft] =
      Defs::_8x4::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_IntraPredictorDc
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorDc] = Defs::_8x4::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorVertical] =
      Defs::_8x4::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorHorizontal] =
      Defs::_8x4::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorPaeth] =
      Defs::_8x4::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorSmooth] =
      Defs::_8x4::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorSmoothVertical] =
      Defs::_8x4::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorSmoothHorizontal] =
      Defs::_8x4::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorDcFill] =
      Defs8bpp::_8x8::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorDcTop] =
      Defs::_8x8::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorDcLeft] =
      Defs::_8x8::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_IntraPredictorDc
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorDc] = Defs::_8x8::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorVertical] =
      Defs::_8x8::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorHorizontal] =
      Defs::_8x8::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorPaeth] =
      Defs::_8x8::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorSmooth] =
      Defs::_8x8::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorSmoothVertical] =
      Defs::_8x8::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorSmoothHorizontal] =
      Defs::_8x8::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorDcFill] =
      Defs8bpp::_8x16::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorDcTop] =
      Defs::_8x16::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorDcLeft] =
      Defs::_8x16::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_IntraPredictorDc
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorDc] =
      Defs::_8x16::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorVertical] =
      Defs::_8x16::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorHorizontal] =
      Defs::_8x16::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorPaeth] =
      Defs::_8x16::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorSmooth] =
      Defs::_8x16::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorSmoothVertical] =
      Defs::_8x16::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorSmoothHorizontal] =
      Defs::_8x16::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorDcFill] =
      Defs8bpp::_8x32::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorDcTop] =
      Defs::_8x32::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorDcLeft] =
      Defs::_8x32::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_IntraPredictorDc
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorDc] =
      Defs::_8x32::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorVertical] =
      Defs::_8x32::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorHorizontal] =
      Defs::_8x32::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorPaeth] =
      Defs::_8x32::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorSmooth] =
      Defs::_8x32::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorSmoothVertical] =
      Defs::_8x32::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorSmoothHorizontal] =
      Defs::_8x32::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorDcFill] =
      Defs8bpp::_16x4::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorDcTop] =
      Defs::_16x4::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorDcLeft] =
      Defs::_16x4::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_IntraPredictorDc
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorDc] =
      Defs::_16x4::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorVertical] =
      Defs::_16x4::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorHorizontal] =
      Defs::_16x4::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorPaeth] =
      Defs::_16x4::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorSmooth] =
      Defs::_16x4::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorSmoothVertical] =
      Defs::_16x4::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorSmoothHorizontal] =
      Defs::_16x4::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorDcFill] =
      Defs8bpp::_16x8::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorDcTop] =
      Defs::_16x8::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorDcLeft] =
      Defs::_16x8::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_IntraPredictorDc
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorDc] =
      Defs::_16x8::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorVertical] =
      Defs::_16x8::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorHorizontal] =
      Defs::_16x8::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorPaeth] =
      Defs::_16x8::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorSmooth] =
      Defs::_16x8::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorSmoothVertical] =
      Defs::_16x8::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorSmoothHorizontal] =
      Defs::_16x8::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorDcFill] =
      Defs8bpp::_16x16::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorDcTop] =
      Defs::_16x16::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorDcLeft] =
      Defs::_16x16::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_IntraPredictorDc
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorDc] =
      Defs::_16x16::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorVertical] =
      Defs::_16x16::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorHorizontal] =
      Defs::_16x16::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorPaeth] =
      Defs::_16x16::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorSmooth] =
      Defs::_16x16::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorSmoothVertical] =
      Defs::_16x16::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorSmoothHorizontal] =
      Defs::_16x16::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorDcFill] =
      Defs8bpp::_16x32::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorDcTop] =
      Defs::_16x32::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorDcLeft] =
      Defs::_16x32::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_IntraPredictorDc
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorDc] =
      Defs::_16x32::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorVertical] =
      Defs::_16x32::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorHorizontal] =
      Defs::_16x32::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorPaeth] =
      Defs::_16x32::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorSmooth] =
      Defs::_16x32::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorSmoothVertical] =
      Defs::_16x32::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorSmoothHorizontal] =
      Defs::_16x32::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize16x64_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorDcFill] =
      Defs8bpp::_16x64::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x64_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorDcTop] =
      Defs::_16x64::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x64_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorDcLeft] =
      Defs::_16x64::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x64_IntraPredictorDc
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorDc] =
      Defs::_16x64::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x64_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorVertical] =
      Defs::_16x64::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x64_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorHorizontal] =
      Defs::_16x64::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x64_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorPaeth] =
      Defs::_16x64::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x64_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorSmooth] =
      Defs::_16x64::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x64_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorSmoothVertical] =
      Defs::_16x64::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x64_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorSmoothHorizontal] =
      Defs::_16x64::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorDcFill] =
      Defs8bpp::_32x8::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorDcTop] =
      Defs::_32x8::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorDcLeft] =
      Defs::_32x8::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_IntraPredictorDc
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorDc] =
      Defs::_32x8::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorVertical] =
      Defs::_32x8::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorHorizontal] =
      Defs::_32x8::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorPaeth] =
      Defs::_32x8::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorSmooth] =
      Defs::_32x8::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorSmoothVertical] =
      Defs::_32x8::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorSmoothHorizontal] =
      Defs::_32x8::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorDcFill] =
      Defs8bpp::_32x16::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorDcTop] =
      Defs::_32x16::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorDcLeft] =
      Defs::_32x16::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_IntraPredictorDc
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorDc] =
      Defs::_32x16::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorVertical] =
      Defs::_32x16::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorHorizontal] =
      Defs::_32x16::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorPaeth] =
      Defs::_32x16::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorSmooth] =
      Defs::_32x16::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorSmoothVertical] =
      Defs::_32x16::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorSmoothHorizontal] =
      Defs::_32x16::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorDcFill] =
      Defs8bpp::_32x32::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorDcTop] =
      Defs::_32x32::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorDcLeft] =
      Defs::_32x32::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_IntraPredictorDc
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorDc] =
      Defs::_32x32::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorVertical] =
      Defs::_32x32::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorHorizontal] =
      Defs::_32x32::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorPaeth] =
      Defs::_32x32::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorSmooth] =
      Defs::_32x32::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorSmoothVertical] =
      Defs::_32x32::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorSmoothHorizontal] =
      Defs::_32x32::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize32x64_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorDcFill] =
      Defs8bpp::_32x64::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x64_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorDcTop] =
      Defs::_32x64::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x64_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorDcLeft] =
      Defs::_32x64::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x64_IntraPredictorDc
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorDc] =
      Defs::_32x64::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x64_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorVertical] =
      Defs::_32x64::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x64_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorHorizontal] =
      Defs::_32x64::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x64_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorPaeth] =
      Defs::_32x64::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x64_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorSmooth] =
      Defs::_32x64::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x64_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorSmoothVertical] =
      Defs::_32x64::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x64_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorSmoothHorizontal] =
      Defs::_32x64::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize64x16_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorDcFill] =
      Defs8bpp::_64x16::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x16_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorDcTop] =
      Defs::_64x16::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x16_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorDcLeft] =
      Defs::_64x16::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x16_IntraPredictorDc
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorDc] =
      Defs::_64x16::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x16_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorVertical] =
      Defs::_64x16::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x16_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorHorizontal] =
      Defs::_64x16::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x16_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorPaeth] =
      Defs::_64x16::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x16_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorSmooth] =
      Defs::_64x16::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x16_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorSmoothVertical] =
      Defs::_64x16::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x16_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorSmoothHorizontal] =
      Defs::_64x16::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize64x32_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorDcFill] =
      Defs8bpp::_64x32::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x32_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorDcTop] =
      Defs::_64x32::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x32_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorDcLeft] =
      Defs::_64x32::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x32_IntraPredictorDc
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorDc] =
      Defs::_64x32::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x32_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorVertical] =
      Defs::_64x32::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x32_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorHorizontal] =
      Defs::_64x32::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x32_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorPaeth] =
      Defs::_64x32::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x32_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorSmooth] =
      Defs::_64x32::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x32_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorSmoothVertical] =
      Defs::_64x32::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x32_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorSmoothHorizontal] =
      Defs::_64x32::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize64x64_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorDcFill] =
      Defs8bpp::_64x64::DcFill;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x64_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorDcTop] =
      Defs::_64x64::DcTop;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x64_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorDcLeft] =
      Defs::_64x64::DcLeft;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x64_IntraPredictorDc
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorDc] =
      Defs::_64x64::Dc;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x64_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorVertical] =
      Defs::_64x64::Vertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x64_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorHorizontal] =
      Defs::_64x64::Horizontal;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x64_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorPaeth] =
      Defs::_64x64::Paeth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x64_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorSmooth] =
      Defs::_64x64::Smooth;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x64_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorSmoothVertical] =
      Defs::_64x64::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize64x64_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorSmoothHorizontal] =
      Defs::_64x64::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp8bpp_DirectionalIntraPredictorZone1
  dsp->directional_intra_predictor_zone1 =
      DirectionalIntraPredictorZone1_C<uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_DirectionalIntraPredictorZone2
  dsp->directional_intra_predictor_zone2 =
      DirectionalIntraPredictorZone2_C<uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_DirectionalIntraPredictorZone3
  dsp->directional_intra_predictor_zone3 =
      DirectionalIntraPredictorZone3_C<uint8_t>;
#endif

#ifndef LIBGAV1_Dsp8bpp_FilterIntraPredictor
  dsp->filter_intra_predictor = FilterIntraPredictor_C<8, uint8_t>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize4x4] =
      CflIntraPredictor_C<4, 4, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize4x4][kSubsamplingType444] =
      CflSubsampler_C<4, 4, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize4x4][kSubsamplingType422] =
      CflSubsampler_C<4, 4, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize4x4][kSubsamplingType420] =
      CflSubsampler_C<4, 4, 8, uint8_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize4x8] =
      CflIntraPredictor_C<4, 8, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize4x8][kSubsamplingType444] =
      CflSubsampler_C<4, 8, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize4x8][kSubsamplingType422] =
      CflSubsampler_C<4, 8, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x8_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize4x8][kSubsamplingType420] =
      CflSubsampler_C<4, 8, 8, uint8_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize4x16] =
      CflIntraPredictor_C<4, 16, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize4x16][kSubsamplingType444] =
      CflSubsampler_C<4, 16, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize4x16][kSubsamplingType422] =
      CflSubsampler_C<4, 16, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize4x16_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize4x16][kSubsamplingType420] =
      CflSubsampler_C<4, 16, 8, uint8_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize8x4] =
      CflIntraPredictor_C<8, 4, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize8x4][kSubsamplingType444] =
      CflSubsampler_C<8, 4, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize8x4][kSubsamplingType422] =
      CflSubsampler_C<8, 4, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x4_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize8x4][kSubsamplingType420] =
      CflSubsampler_C<8, 4, 8, uint8_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize8x8] =
      CflIntraPredictor_C<8, 8, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize8x8][kSubsamplingType444] =
      CflSubsampler_C<8, 8, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize8x8][kSubsamplingType422] =
      CflSubsampler_C<8, 8, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x8_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize8x8][kSubsamplingType420] =
      CflSubsampler_C<8, 8, 8, uint8_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize8x16] =
      CflIntraPredictor_C<8, 16, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize8x16][kSubsamplingType444] =
      CflSubsampler_C<8, 16, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize8x16][kSubsamplingType422] =
      CflSubsampler_C<8, 16, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x16_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize8x16][kSubsamplingType420] =
      CflSubsampler_C<8, 16, 8, uint8_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize8x32] =
      CflIntraPredictor_C<8, 32, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize8x32][kSubsamplingType444] =
      CflSubsampler_C<8, 32, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize8x32][kSubsamplingType422] =
      CflSubsampler_C<8, 32, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize8x32_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize8x32][kSubsamplingType420] =
      CflSubsampler_C<8, 32, 8, uint8_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize16x4] =
      CflIntraPredictor_C<16, 4, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize16x4][kSubsamplingType444] =
      CflSubsampler_C<16, 4, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize16x4][kSubsamplingType422] =
      CflSubsampler_C<16, 4, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x4_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize16x4][kSubsamplingType420] =
      CflSubsampler_C<16, 4, 8, uint8_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize16x8] =
      CflIntraPredictor_C<16, 8, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize16x8][kSubsamplingType444] =
      CflSubsampler_C<16, 8, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize16x8][kSubsamplingType422] =
      CflSubsampler_C<16, 8, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x8_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize16x8][kSubsamplingType420] =
      CflSubsampler_C<16, 8, 8, uint8_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize16x16] =
      CflIntraPredictor_C<16, 16, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize16x16][kSubsamplingType444] =
      CflSubsampler_C<16, 16, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize16x16][kSubsamplingType422] =
      CflSubsampler_C<16, 16, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x16_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize16x16][kSubsamplingType420] =
      CflSubsampler_C<16, 16, 8, uint8_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize16x32] =
      CflIntraPredictor_C<16, 32, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize16x32][kSubsamplingType444] =
      CflSubsampler_C<16, 32, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize16x32][kSubsamplingType422] =
      CflSubsampler_C<16, 32, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize16x32_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize16x32][kSubsamplingType420] =
      CflSubsampler_C<16, 32, 8, uint8_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize32x8] =
      CflIntraPredictor_C<32, 8, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize32x8][kSubsamplingType444] =
      CflSubsampler_C<32, 8, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize32x8][kSubsamplingType422] =
      CflSubsampler_C<32, 8, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x8_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize32x8][kSubsamplingType420] =
      CflSubsampler_C<32, 8, 8, uint8_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize32x16] =
      CflIntraPredictor_C<32, 16, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize32x16][kSubsamplingType444] =
      CflSubsampler_C<32, 16, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize32x16][kSubsamplingType422] =
      CflSubsampler_C<32, 16, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x16_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize32x16][kSubsamplingType420] =
      CflSubsampler_C<32, 16, 8, uint8_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize32x32] =
      CflIntraPredictor_C<32, 32, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize32x32][kSubsamplingType444] =
      CflSubsampler_C<32, 32, 8, uint8_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize32x32][kSubsamplingType422] =
      CflSubsampler_C<32, 32, 8, uint8_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_TransformSize32x32_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize32x32][kSubsamplingType420] =
      CflSubsampler_C<32, 32, 8, uint8_t, 1, 1>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  // Cfl predictors are available only for transform sizes with max(width,
  // height) <= 32. Set all others to nullptr.
  for (const auto i : kTransformSizesLargerThan32x32) {
    dsp->cfl_intra_predictors[i] = nullptr;
    for (int j = 0; j < kNumSubsamplingTypes; ++j) {
      dsp->cfl_subsamplers[i][j] = nullptr;
    }
  }
}  // NOLINT(readability/fn_size)

#if LIBGAV1_MAX_BITDEPTH >= 10
using DefsHbd = IntraPredDefs<uint16_t>;
using Defs10bpp = IntraPredBppDefs<10, uint16_t>;

void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  INIT_INTRAPREDICTORS(DefsHbd, Defs10bpp);
  dsp->directional_intra_predictor_zone1 =
      DirectionalIntraPredictorZone1_C<uint16_t>;
  dsp->directional_intra_predictor_zone2 =
      DirectionalIntraPredictorZone2_C<uint16_t>;
  dsp->directional_intra_predictor_zone3 =
      DirectionalIntraPredictorZone3_C<uint16_t>;
  dsp->filter_intra_predictor = FilterIntraPredictor_C<10, uint16_t>;
  INIT_CFL_INTRAPREDICTORS(10, uint16_t);
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDcFill] =
      Defs10bpp::_4x4::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDcTop] =
      DefsHbd::_4x4::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDcLeft] =
      DefsHbd::_4x4::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_IntraPredictorDc
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorDc] =
      DefsHbd::_4x4::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorVertical] =
      DefsHbd::_4x4::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorHorizontal] =
      DefsHbd::_4x4::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorPaeth] =
      DefsHbd::_4x4::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorSmooth] =
      DefsHbd::_4x4::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorSmoothVertical] =
      DefsHbd::_4x4::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize4x4][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_4x4::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorDcFill] =
      Defs10bpp::_4x8::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorDcTop] =
      DefsHbd::_4x8::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorDcLeft] =
      DefsHbd::_4x8::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_IntraPredictorDc
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorDc] =
      DefsHbd::_4x8::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorVertical] =
      DefsHbd::_4x8::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorHorizontal] =
      DefsHbd::_4x8::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorPaeth] =
      DefsHbd::_4x8::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorSmooth] =
      DefsHbd::_4x8::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorSmoothVertical] =
      DefsHbd::_4x8::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize4x8][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_4x8::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorDcFill] =
      Defs10bpp::_4x16::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorDcTop] =
      DefsHbd::_4x16::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorDcLeft] =
      DefsHbd::_4x16::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_IntraPredictorDc
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorDc] =
      DefsHbd::_4x16::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorVertical] =
      DefsHbd::_4x16::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorHorizontal] =
      DefsHbd::_4x16::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorPaeth] =
      DefsHbd::_4x16::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorSmooth] =
      DefsHbd::_4x16::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorSmoothVertical] =
      DefsHbd::_4x16::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize4x16][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_4x16::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorDcFill] =
      Defs10bpp::_8x4::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorDcTop] =
      DefsHbd::_8x4::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorDcLeft] =
      DefsHbd::_8x4::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_IntraPredictorDc
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorDc] =
      DefsHbd::_8x4::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorVertical] =
      DefsHbd::_8x4::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorHorizontal] =
      DefsHbd::_8x4::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorPaeth] =
      DefsHbd::_8x4::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorSmooth] =
      DefsHbd::_8x4::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorSmoothVertical] =
      DefsHbd::_8x4::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize8x4][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_8x4::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorDcFill] =
      Defs10bpp::_8x8::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorDcTop] =
      DefsHbd::_8x8::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorDcLeft] =
      DefsHbd::_8x8::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_IntraPredictorDc
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorDc] =
      DefsHbd::_8x8::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorVertical] =
      DefsHbd::_8x8::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorHorizontal] =
      DefsHbd::_8x8::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorPaeth] =
      DefsHbd::_8x8::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorSmooth] =
      DefsHbd::_8x8::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorSmoothVertical] =
      DefsHbd::_8x8::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize8x8][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_8x8::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorDcFill] =
      Defs10bpp::_8x16::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorDcTop] =
      DefsHbd::_8x16::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorDcLeft] =
      DefsHbd::_8x16::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_IntraPredictorDc
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorDc] =
      DefsHbd::_8x16::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorVertical] =
      DefsHbd::_8x16::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorHorizontal] =
      DefsHbd::_8x16::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorPaeth] =
      DefsHbd::_8x16::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorSmooth] =
      DefsHbd::_8x16::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorSmoothVertical] =
      DefsHbd::_8x16::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize8x16][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_8x16::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorDcFill] =
      Defs10bpp::_8x32::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorDcTop] =
      DefsHbd::_8x32::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorDcLeft] =
      DefsHbd::_8x32::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_IntraPredictorDc
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorDc] =
      DefsHbd::_8x32::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorVertical] =
      DefsHbd::_8x32::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorHorizontal] =
      DefsHbd::_8x32::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorPaeth] =
      DefsHbd::_8x32::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorSmooth] =
      DefsHbd::_8x32::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorSmoothVertical] =
      DefsHbd::_8x32::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize8x32][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_8x32::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorDcFill] =
      Defs10bpp::_16x4::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorDcTop] =
      DefsHbd::_16x4::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorDcLeft] =
      DefsHbd::_16x4::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_IntraPredictorDc
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorDc] =
      DefsHbd::_16x4::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorVertical] =
      DefsHbd::_16x4::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorHorizontal] =
      DefsHbd::_16x4::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorPaeth] =
      DefsHbd::_16x4::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorSmooth] =
      DefsHbd::_16x4::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorSmoothVertical] =
      DefsHbd::_16x4::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize16x4][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_16x4::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorDcFill] =
      Defs10bpp::_16x8::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorDcTop] =
      DefsHbd::_16x8::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorDcLeft] =
      DefsHbd::_16x8::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_IntraPredictorDc
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorDc] =
      DefsHbd::_16x8::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorVertical] =
      DefsHbd::_16x8::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorHorizontal] =
      DefsHbd::_16x8::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorPaeth] =
      DefsHbd::_16x8::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorSmooth] =
      DefsHbd::_16x8::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorSmoothVertical] =
      DefsHbd::_16x8::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize16x8][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_16x8::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorDcFill] =
      Defs10bpp::_16x16::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorDcTop] =
      DefsHbd::_16x16::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorDcLeft] =
      DefsHbd::_16x16::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_IntraPredictorDc
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorDc] =
      DefsHbd::_16x16::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorVertical] =
      DefsHbd::_16x16::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorHorizontal] =
      DefsHbd::_16x16::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorPaeth] =
      DefsHbd::_16x16::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorSmooth] =
      DefsHbd::_16x16::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorSmoothVertical] =
      DefsHbd::_16x16::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize16x16][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_16x16::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorDcFill] =
      Defs10bpp::_16x32::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorDcTop] =
      DefsHbd::_16x32::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorDcLeft] =
      DefsHbd::_16x32::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_IntraPredictorDc
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorDc] =
      DefsHbd::_16x32::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorVertical] =
      DefsHbd::_16x32::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorHorizontal] =
      DefsHbd::_16x32::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorPaeth] =
      DefsHbd::_16x32::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorSmooth] =
      DefsHbd::_16x32::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorSmoothVertical] =
      DefsHbd::_16x32::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize16x32][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_16x32::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize16x64_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorDcFill] =
      Defs10bpp::_16x64::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x64_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorDcTop] =
      DefsHbd::_16x64::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x64_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorDcLeft] =
      DefsHbd::_16x64::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x64_IntraPredictorDc
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorDc] =
      DefsHbd::_16x64::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x64_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorVertical] =
      DefsHbd::_16x64::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x64_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorHorizontal] =
      DefsHbd::_16x64::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x64_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorPaeth] =
      DefsHbd::_16x64::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x64_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorSmooth] =
      DefsHbd::_16x64::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x64_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorSmoothVertical] =
      DefsHbd::_16x64::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x64_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize16x64][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_16x64::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorDcFill] =
      Defs10bpp::_32x8::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorDcTop] =
      DefsHbd::_32x8::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorDcLeft] =
      DefsHbd::_32x8::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_IntraPredictorDc
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorDc] =
      DefsHbd::_32x8::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorVertical] =
      DefsHbd::_32x8::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorHorizontal] =
      DefsHbd::_32x8::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorPaeth] =
      DefsHbd::_32x8::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorSmooth] =
      DefsHbd::_32x8::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorSmoothVertical] =
      DefsHbd::_32x8::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize32x8][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_32x8::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorDcFill] =
      Defs10bpp::_32x16::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorDcTop] =
      DefsHbd::_32x16::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorDcLeft] =
      DefsHbd::_32x16::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_IntraPredictorDc
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorDc] =
      DefsHbd::_32x16::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorVertical] =
      DefsHbd::_32x16::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorHorizontal] =
      DefsHbd::_32x16::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorPaeth] =
      DefsHbd::_32x16::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorSmooth] =
      DefsHbd::_32x16::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorSmoothVertical] =
      DefsHbd::_32x16::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize32x16][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_32x16::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorDcFill] =
      Defs10bpp::_32x32::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorDcTop] =
      DefsHbd::_32x32::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorDcLeft] =
      DefsHbd::_32x32::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_IntraPredictorDc
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorDc] =
      DefsHbd::_32x32::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorVertical] =
      DefsHbd::_32x32::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorHorizontal] =
      DefsHbd::_32x32::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorPaeth] =
      DefsHbd::_32x32::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorSmooth] =
      DefsHbd::_32x32::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorSmoothVertical] =
      DefsHbd::_32x32::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize32x32][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_32x32::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize32x64_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorDcFill] =
      Defs10bpp::_32x64::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x64_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorDcTop] =
      DefsHbd::_32x64::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x64_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorDcLeft] =
      DefsHbd::_32x64::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x64_IntraPredictorDc
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorDc] =
      DefsHbd::_32x64::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x64_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorVertical] =
      DefsHbd::_32x64::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x64_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorHorizontal] =
      DefsHbd::_32x64::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x64_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorPaeth] =
      DefsHbd::_32x64::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x64_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorSmooth] =
      DefsHbd::_32x64::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x64_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorSmoothVertical] =
      DefsHbd::_32x64::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x64_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize32x64][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_32x64::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize64x16_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorDcFill] =
      Defs10bpp::_64x16::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x16_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorDcTop] =
      DefsHbd::_64x16::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x16_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorDcLeft] =
      DefsHbd::_64x16::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x16_IntraPredictorDc
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorDc] =
      DefsHbd::_64x16::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x16_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorVertical] =
      DefsHbd::_64x16::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x16_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorHorizontal] =
      DefsHbd::_64x16::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x16_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorPaeth] =
      DefsHbd::_64x16::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x16_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorSmooth] =
      DefsHbd::_64x16::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x16_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorSmoothVertical] =
      DefsHbd::_64x16::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x16_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize64x16][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_64x16::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize64x32_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorDcFill] =
      Defs10bpp::_64x32::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x32_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorDcTop] =
      DefsHbd::_64x32::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x32_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorDcLeft] =
      DefsHbd::_64x32::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x32_IntraPredictorDc
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorDc] =
      DefsHbd::_64x32::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x32_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorVertical] =
      DefsHbd::_64x32::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x32_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorHorizontal] =
      DefsHbd::_64x32::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x32_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorPaeth] =
      DefsHbd::_64x32::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x32_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorSmooth] =
      DefsHbd::_64x32::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x32_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorSmoothVertical] =
      DefsHbd::_64x32::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x32_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize64x32][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_64x32::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize64x64_IntraPredictorDcFill
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorDcFill] =
      Defs10bpp::_64x64::DcFill;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x64_IntraPredictorDcTop
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorDcTop] =
      DefsHbd::_64x64::DcTop;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x64_IntraPredictorDcLeft
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorDcLeft] =
      DefsHbd::_64x64::DcLeft;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x64_IntraPredictorDc
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorDc] =
      DefsHbd::_64x64::Dc;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x64_IntraPredictorVertical
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorVertical] =
      DefsHbd::_64x64::Vertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x64_IntraPredictorHorizontal
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorHorizontal] =
      DefsHbd::_64x64::Horizontal;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x64_IntraPredictorPaeth
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorPaeth] =
      DefsHbd::_64x64::Paeth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x64_IntraPredictorSmooth
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorSmooth] =
      DefsHbd::_64x64::Smooth;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x64_IntraPredictorSmoothVertical
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorSmoothVertical] =
      DefsHbd::_64x64::SmoothVertical;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize64x64_IntraPredictorSmoothHorizontal
  dsp->intra_predictors[kTransformSize64x64][kIntraPredictorSmoothHorizontal] =
      DefsHbd::_64x64::SmoothHorizontal;
#endif

#ifndef LIBGAV1_Dsp10bpp_DirectionalIntraPredictorZone1
  dsp->directional_intra_predictor_zone1 =
      DirectionalIntraPredictorZone1_C<uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_DirectionalIntraPredictorZone2
  dsp->directional_intra_predictor_zone2 =
      DirectionalIntraPredictorZone2_C<uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_DirectionalIntraPredictorZone3
  dsp->directional_intra_predictor_zone3 =
      DirectionalIntraPredictorZone3_C<uint16_t>;
#endif

#ifndef LIBGAV1_Dsp10bpp_FilterIntraPredictor
  dsp->filter_intra_predictor = FilterIntraPredictor_C<10, uint16_t>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize4x4] =
      CflIntraPredictor_C<4, 4, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize4x4][kSubsamplingType444] =
      CflSubsampler_C<4, 4, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize4x4][kSubsamplingType422] =
      CflSubsampler_C<4, 4, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x4_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize4x4][kSubsamplingType420] =
      CflSubsampler_C<4, 4, 10, uint16_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize4x8] =
      CflIntraPredictor_C<4, 8, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize4x8][kSubsamplingType444] =
      CflSubsampler_C<4, 8, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize4x8][kSubsamplingType422] =
      CflSubsampler_C<4, 8, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x8_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize4x8][kSubsamplingType420] =
      CflSubsampler_C<4, 8, 10, uint16_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize4x16] =
      CflIntraPredictor_C<4, 16, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize4x16][kSubsamplingType444] =
      CflSubsampler_C<4, 16, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize4x16][kSubsamplingType422] =
      CflSubsampler_C<4, 16, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize4x16_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize4x16][kSubsamplingType420] =
      CflSubsampler_C<4, 16, 10, uint16_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize8x4] =
      CflIntraPredictor_C<8, 4, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize8x4][kSubsamplingType444] =
      CflSubsampler_C<8, 4, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize8x4][kSubsamplingType422] =
      CflSubsampler_C<8, 4, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x4_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize8x4][kSubsamplingType420] =
      CflSubsampler_C<8, 4, 10, uint16_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize8x8] =
      CflIntraPredictor_C<8, 8, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize8x8][kSubsamplingType444] =
      CflSubsampler_C<8, 8, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize8x8][kSubsamplingType422] =
      CflSubsampler_C<8, 8, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x8_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize8x8][kSubsamplingType420] =
      CflSubsampler_C<8, 8, 10, uint16_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize8x16] =
      CflIntraPredictor_C<8, 16, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize8x16][kSubsamplingType444] =
      CflSubsampler_C<8, 16, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize8x16][kSubsamplingType422] =
      CflSubsampler_C<8, 16, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x16_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize8x16][kSubsamplingType420] =
      CflSubsampler_C<8, 16, 10, uint16_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize8x32] =
      CflIntraPredictor_C<8, 32, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize8x32][kSubsamplingType444] =
      CflSubsampler_C<8, 32, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize8x32][kSubsamplingType422] =
      CflSubsampler_C<8, 32, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize8x32_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize8x32][kSubsamplingType420] =
      CflSubsampler_C<8, 32, 10, uint16_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize16x4] =
      CflIntraPredictor_C<16, 4, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize16x4][kSubsamplingType444] =
      CflSubsampler_C<16, 4, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize16x4][kSubsamplingType422] =
      CflSubsampler_C<16, 4, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x4_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize16x4][kSubsamplingType420] =
      CflSubsampler_C<16, 4, 10, uint16_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize16x8] =
      CflIntraPredictor_C<16, 8, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize16x8][kSubsamplingType444] =
      CflSubsampler_C<16, 8, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize16x8][kSubsamplingType422] =
      CflSubsampler_C<16, 8, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x8_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize16x8][kSubsamplingType420] =
      CflSubsampler_C<16, 8, 10, uint16_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize16x16] =
      CflIntraPredictor_C<16, 16, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize16x16][kSubsamplingType444] =
      CflSubsampler_C<16, 16, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize16x16][kSubsamplingType422] =
      CflSubsampler_C<16, 16, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x16_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize16x16][kSubsamplingType420] =
      CflSubsampler_C<16, 16, 10, uint16_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize16x32] =
      CflIntraPredictor_C<16, 32, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize16x32][kSubsamplingType444] =
      CflSubsampler_C<16, 32, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize16x32][kSubsamplingType422] =
      CflSubsampler_C<16, 32, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize16x32_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize16x32][kSubsamplingType420] =
      CflSubsampler_C<16, 32, 10, uint16_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize32x8] =
      CflIntraPredictor_C<32, 8, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize32x8][kSubsamplingType444] =
      CflSubsampler_C<32, 8, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize32x8][kSubsamplingType422] =
      CflSubsampler_C<32, 8, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x8_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize32x8][kSubsamplingType420] =
      CflSubsampler_C<32, 8, 10, uint16_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize32x16] =
      CflIntraPredictor_C<32, 16, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize32x16][kSubsamplingType444] =
      CflSubsampler_C<32, 16, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize32x16][kSubsamplingType422] =
      CflSubsampler_C<32, 16, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x16_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize32x16][kSubsamplingType420] =
      CflSubsampler_C<32, 16, 10, uint16_t, 1, 1>;
#endif

#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_CflIntraPredictor
  dsp->cfl_intra_predictors[kTransformSize32x32] =
      CflIntraPredictor_C<32, 32, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_CflSubsampler444
  dsp->cfl_subsamplers[kTransformSize32x32][kSubsamplingType444] =
      CflSubsampler_C<32, 32, 10, uint16_t, 0, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_CflSubsampler422
  dsp->cfl_subsamplers[kTransformSize32x32][kSubsamplingType422] =
      CflSubsampler_C<32, 32, 10, uint16_t, 1, 0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_TransformSize32x32_CflSubsampler420
  dsp->cfl_subsamplers[kTransformSize32x32][kSubsamplingType420] =
      CflSubsampler_C<32, 32, 10, uint16_t, 1, 1>;
#endif

#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  // Cfl predictors are available only for transform sizes with max(width,
  // height) <= 32. Set all others to nullptr.
  for (const auto i : kTransformSizesLargerThan32x32) {
    dsp->cfl_intra_predictors[i] = nullptr;
    for (int j = 0; j < kNumSubsamplingTypes; ++j) {
      dsp->cfl_subsamplers[i][j] = nullptr;
    }
  }
}  // NOLINT(readability/fn_size)
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

#undef INIT_CFL_INTRAPREDICTOR_WxH
#undef INIT_CFL_INTRAPREDICTORS
#undef INIT_INTRAPREDICTORS_WxH
#undef INIT_INTRAPREDICTORS

}  // namespace

void IntraPredInit_C() {
  Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  Init10bpp();
#endif
}

}  // namespace dsp
}  // namespace libgav1
