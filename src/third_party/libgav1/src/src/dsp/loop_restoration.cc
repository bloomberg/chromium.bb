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

#include <algorithm>  // std::max
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/dsp/common.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {
namespace {

// Precision of a division table (mtable)
constexpr int kSgrProjScaleBits = 20;
constexpr int kSgrProjReciprocalBits = 12;
// Core self-guided restoration precision bits.
constexpr int kSgrProjSgrBits = 8;
// Precision bits of generated values higher than source before projection.
constexpr int kSgrProjRestoreBits = 4;

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

constexpr int kOneByX[25] = {
    4096, 2048, 1365, 1024, 819, 683, 585, 512, 455, 410, 372, 341, 315,
    293,  273,  256,  241,  228, 216, 205, 195, 186, 178, 171, 164,
};

// Compute integral image. In an integral image, each pixel value of (xi, yi)
// is the sum of all pixel values {(x, y) | x <= xi, y <= yi} from the source
// image.
// The integral image (II) can be calculated as:
// II(D) = Pixel(D) + II(B) + II(C) - II(A),
// where the rectangular region ABCD is
// A = (x, y), B = (x + 1, y), C = (x, y + 1), D = (x + 1, y + 1).
// Integral image helps to compute the sum of a rectangular area fast.
// The box centered at (x, y), with radius r, is rectangular ABCD:
// A = (x - r, y - r), B = (x + r, y - r),
// C = (x - r, y + r), D = (x + r, y + r),
// The sum of the box, or the rectangular ABCD can be calculated with the
// integral image (II):
// sum = II(D) - II(B') - II(C') + II(A').
// A' = (x - r - 1, y - r - 1), B' = (x + r, y - r - 1),
// C' = (x - r - 1, y + r), D = (x + r, y + r),
// Here we calculate the integral image, as well as the squared integral image.
template <typename Pixel>
void ComputeIntegralImage(const Pixel* const src, ptrdiff_t src_stride,
                          int width, int height, uint16_t* integral_image,
                          uint32_t* square_integral_image,
                          ptrdiff_t image_stride) {
  memset(integral_image, 0, image_stride * sizeof(integral_image[0]));
  memset(square_integral_image, 0,
         image_stride * sizeof(square_integral_image[0]));

  const Pixel* src_ptr = src;
  uint16_t* integral_image_ptr = integral_image + image_stride + 1;
  uint32_t* square_integral_image_ptr =
      square_integral_image + image_stride + 1;
  int y = 0;
  do {
    integral_image_ptr[-1] = 0;
    square_integral_image_ptr[-1] = 0;
    for (int x = 0; x < width; ++x) {
      integral_image_ptr[x] = src_ptr[x] + integral_image_ptr[x - 1] +
                              integral_image_ptr[x - image_stride] -
                              integral_image_ptr[x - image_stride - 1];
      square_integral_image_ptr[x] =
          src_ptr[x] * src_ptr[x] + square_integral_image_ptr[x - 1] +
          square_integral_image_ptr[x - image_stride] -
          square_integral_image_ptr[x - image_stride - 1];
    }
    src_ptr += src_stride;
    integral_image_ptr += image_stride;
    square_integral_image_ptr += image_stride;
  } while (++y < height);
}

template <int bitdepth, typename Pixel>
struct LoopRestorationFuncs_C {
  LoopRestorationFuncs_C() = delete;

  // |stride| for SelfGuidedFilter and WienerFilter is given in bytes.
  static void SelfGuidedFilter(const void* source, void* dest,
                               const RestorationUnitInfo& restoration_info,
                               ptrdiff_t source_stride, ptrdiff_t dest_stride,
                               int width, int height,
                               RestorationBuffer* buffer);
  static void WienerFilter(const void* source, void* dest,
                           const RestorationUnitInfo& restoration_info,
                           ptrdiff_t source_stride, ptrdiff_t dest_stride,
                           int width, int height, RestorationBuffer* buffer);
  // |stride| for box filter processing is in Pixels.
  static void BoxFilterPreProcess(const RestorationUnitInfo& restoration_info,
                                  const uint16_t* integral_image,
                                  const uint32_t* square_integral_image,
                                  int width, int height, int pass,
                                  RestorationBuffer* buffer);
  static void BoxFilterProcess(const RestorationUnitInfo& restoration_info,
                               const Pixel* src, ptrdiff_t stride, int width,
                               int height, RestorationBuffer* buffer);
};

// Note: range of wiener filter coefficients.
// Wiener filter coefficients are symmetric, and their sum is 1 (128).
// The range of each coefficient:
// filter[0] = filter[6], 4 bits, min = -5, max = 10.
// filter[1] = filter[5], 5 bits, min = -23, max = 8.
// filter[2] = filter[4], 6 bits, min = -17, max = 46.
// filter[3] = 128 - (filter[0] + filter[1] + filter[2]) * 2.
// The difference from libaom is that in libaom:
// filter[3] = 0 - (filter[0] + filter[1] + filter[2]) * 2.
// Thus in libaom's computation, an offset of 128 is needed for filter[3].
inline void PopulateWienerCoefficients(
    const RestorationUnitInfo& restoration_info, int direction,
    int16_t* const filter) {
  filter[3] = 128;
  for (int i = 0; i < 3; ++i) {
    const int16_t coeff = restoration_info.wiener_info.filter[direction][i];
    filter[i] = coeff;
    filter[3] -= MultiplyBy2(coeff);
  }
}

inline int CountZeroCoefficients(const int16_t* const filter) {
  int number_zero_coefficients = 0;
  if (filter[0] == 0) {
    number_zero_coefficients++;
    if (filter[1] == 0) {
      number_zero_coefficients++;
      if (filter[2] == 0) {
        number_zero_coefficients++;
      }
    }
  }
  return number_zero_coefficients;
}

template <typename Pixel>
inline int WienerHorizontal(const Pixel* const source,
                            const int16_t* const filter,
                            const int number_zero_coefficients, int sum) {
  constexpr int kCenterTap = (kSubPixelTaps - 1) / 2;
  for (int k = number_zero_coefficients; k < kCenterTap; ++k) {
    sum += filter[k] * (source[k] + source[kSubPixelTaps - 2 - k]);
  }
  return sum;
}

inline int WienerVertical(const uint16_t* const source,
                          const int16_t* const filter, const int width,
                          const int number_zero_coefficients, int sum) {
  constexpr int kCenterTap = (kSubPixelTaps - 1) / 2;
  for (int k = number_zero_coefficients; k < kCenterTap; ++k) {
    sum += filter[k] *
           (source[k * width] + source[(kSubPixelTaps - 2 - k) * width]);
  }
  return sum;
}

// Note: bit range for wiener filter.
// Wiener filter process first applies horizontal filtering to input pixels,
// followed by rounding with predefined bits (dependent on bitdepth).
// Then vertical filtering is applied, followed by rounding (dependent on
// bitdepth).
// The process is the same as convolution:
// <input> --> <horizontal filter> --> <rounding 0> --> <vertical filter>
// --> <rounding 1>
// By design:
// (a). horizontal/vertical filtering adds 7 bits to input.
// (b). The output of first rounding fits into 16 bits.
// (c). The output of second rounding fits into 16 bits.
// If input bitdepth > 8, the accumulator of the horizontal filter is larger
// than 16 bit and smaller than 32 bits.
// The accumulator of the vertical filter is larger than 16 bits and smaller
// than 32 bits.
template <int bitdepth, typename Pixel>
void LoopRestorationFuncs_C<bitdepth, Pixel>::WienerFilter(
    const void* const source, void* const dest,
    const RestorationUnitInfo& restoration_info, ptrdiff_t source_stride,
    ptrdiff_t dest_stride, int width, int height,
    RestorationBuffer* const buffer) {
  constexpr int kCenterTap = (kSubPixelTaps - 1) / 2;
  constexpr int kRoundBitsHorizontal = (bitdepth == 12)
                                           ? kInterRoundBitsHorizontal12bpp
                                           : kInterRoundBitsHorizontal;
  constexpr int kRoundBitsVertical =
      (bitdepth == 12) ? kInterRoundBitsVertical12bpp : kInterRoundBitsVertical;
  const int limit =
      (1 << (bitdepth + 1 + kWienerFilterBits - kRoundBitsHorizontal)) - 1;
  int16_t filter_horizontal[kSubPixelTaps / 2];
  int16_t filter_vertical[kSubPixelTaps / 2];
  PopulateWienerCoefficients(restoration_info, WienerInfo::kHorizontal,
                             filter_horizontal);
  PopulateWienerCoefficients(restoration_info, WienerInfo::kVertical,
                             filter_vertical);
  const int number_zero_coefficients_horizontal =
      CountZeroCoefficients(filter_horizontal);
  const int number_zero_coefficients_vertical =
      CountZeroCoefficients(filter_vertical);

  source_stride /= sizeof(Pixel);
  dest_stride /= sizeof(Pixel);

  // horizontal filtering.
  const auto* src = static_cast<const Pixel*>(source);
  src -= (kCenterTap - number_zero_coefficients_vertical) * source_stride +
         kCenterTap;
  auto* wiener_buffer =
      buffer->wiener_buffer + number_zero_coefficients_vertical * width;
  const int horizontal_rounding = 1 << (bitdepth + kWienerFilterBits - 1);
  int y = height + kSubPixelTaps - 2 - 2 * number_zero_coefficients_vertical;

  if (number_zero_coefficients_horizontal == 0) {
    do {
      int x = 0;
      do {
        // sum fits into 16 bits only when bitdepth = 8.
        int sum = horizontal_rounding;
        sum = WienerHorizontal<Pixel>(src + x, filter_horizontal, 0, sum);
        sum += filter_horizontal[kCenterTap] * src[x + kCenterTap];
        const int rounded_sum =
            RightShiftWithRounding(sum, kRoundBitsHorizontal);
        wiener_buffer[x] = static_cast<uint16_t>(Clip3(rounded_sum, 0, limit));
      } while (++x < width);
      src += source_stride;
      wiener_buffer += width;
    } while (--y != 0);
  } else if (number_zero_coefficients_horizontal == 1) {
    do {
      int x = 0;
      do {
        // sum fits into 16 bits only when bitdepth = 8.
        int sum = horizontal_rounding;
        sum = WienerHorizontal<Pixel>(src + x, filter_horizontal, 1, sum);
        sum += filter_horizontal[kCenterTap] * src[x + kCenterTap];
        const int rounded_sum =
            RightShiftWithRounding(sum, kRoundBitsHorizontal);
        wiener_buffer[x] = static_cast<uint16_t>(Clip3(rounded_sum, 0, limit));
      } while (++x < width);
      src += source_stride;
      wiener_buffer += width;
    } while (--y != 0);
  } else if (number_zero_coefficients_horizontal == 2) {
    do {
      int x = 0;
      do {
        // sum fits into 16 bits only when bitdepth = 8.
        int sum = horizontal_rounding;
        sum = WienerHorizontal<Pixel>(src + x, filter_horizontal, 2, sum);
        sum += filter_horizontal[kCenterTap] * src[x + kCenterTap];
        const int rounded_sum =
            RightShiftWithRounding(sum, kRoundBitsHorizontal);
        wiener_buffer[x] = static_cast<uint16_t>(Clip3(rounded_sum, 0, limit));
      } while (++x < width);
      src += source_stride;
      wiener_buffer += width;
    } while (--y != 0);
  } else {
    do {
      int x = 0;
      do {
        // sum fits into 16 bits only when bitdepth = 8.
        int sum = horizontal_rounding;
        sum += filter_horizontal[kCenterTap] * src[x + kCenterTap];
        const int rounded_sum =
            RightShiftWithRounding(sum, kRoundBitsHorizontal);
        wiener_buffer[x] = static_cast<uint16_t>(Clip3(rounded_sum, 0, limit));
      } while (++x < width);
      src += source_stride;
      wiener_buffer += width;
    } while (--y != 0);
  }

  // vertical filtering.
  const int vertical_rounding = -(1 << (bitdepth + kRoundBitsVertical - 1));
  auto* dst = static_cast<Pixel*>(dest);
  wiener_buffer = buffer->wiener_buffer;
  y = height;

  if (number_zero_coefficients_vertical == 0) {
    do {
      int x = 0;
      do {
        // sum needs 32 bits.
        int sum = vertical_rounding;
        sum = WienerVertical(wiener_buffer + x, filter_vertical, width, 0, sum);
        sum +=
            filter_vertical[kCenterTap] * wiener_buffer[kCenterTap * width + x];
        const int rounded_sum = RightShiftWithRounding(sum, kRoundBitsVertical);
        dst[x] = static_cast<Pixel>(Clip3(rounded_sum, 0, (1 << bitdepth) - 1));
      } while (++x < width);
      dst += dest_stride;
      wiener_buffer += width;
    } while (--y != 0);
  } else if (number_zero_coefficients_vertical == 1) {
    do {
      int x = 0;
      do {
        // sum needs 32 bits.
        int sum = vertical_rounding;
        sum = WienerVertical(wiener_buffer + x, filter_vertical, width, 1, sum);
        sum +=
            filter_vertical[kCenterTap] * wiener_buffer[kCenterTap * width + x];
        const int rounded_sum = RightShiftWithRounding(sum, kRoundBitsVertical);
        dst[x] = static_cast<Pixel>(Clip3(rounded_sum, 0, (1 << bitdepth) - 1));
      } while (++x < width);
      dst += dest_stride;
      wiener_buffer += width;
    } while (--y != 0);
  } else if (number_zero_coefficients_vertical == 2) {
    do {
      int x = 0;
      do {
        // sum needs 32 bits.
        int sum = vertical_rounding;
        sum = WienerVertical(wiener_buffer + x, filter_vertical, width, 2, sum);
        sum +=
            filter_vertical[kCenterTap] * wiener_buffer[kCenterTap * width + x];
        const int rounded_sum = RightShiftWithRounding(sum, kRoundBitsVertical);
        dst[x] = static_cast<Pixel>(Clip3(rounded_sum, 0, (1 << bitdepth) - 1));
      } while (++x < width);
      dst += dest_stride;
      wiener_buffer += width;
    } while (--y != 0);
  } else {
    do {
      int x = 0;
      do {
        // sum needs 32 bits.
        int sum = vertical_rounding;
        sum +=
            filter_vertical[kCenterTap] * wiener_buffer[kCenterTap * width + x];
        const int rounded_sum = RightShiftWithRounding(sum, kRoundBitsVertical);
        dst[x] = static_cast<Pixel>(Clip3(rounded_sum, 0, (1 << bitdepth) - 1));
      } while (++x < width);
      dst += dest_stride;
      wiener_buffer += width;
    } while (--y != 0);
  }
}

template <int bitdepth, typename Pixel>
void LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterPreProcess(
    const RestorationUnitInfo& restoration_info, const uint16_t* integral_image,
    const uint32_t* square_integral_image, int width, int height, int pass,
    RestorationBuffer* const buffer) {
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  const uint8_t radius = kSgrProjParams[sgr_proj_index][pass * 2];
  assert(radius != 0);
  const uint32_t n = (2 * radius + 1) * (2 * radius + 1);
  // const uint8_t scale = kSgrProjParams[sgr_proj_index][pass * 2 + 1];
  // n2_with_scale: max value < 2^16. min value is 4.
  // const uint32_t n2_with_scale = n * n * scale;
  // s: max value < 2^12.
  // const uint32_t s =
  // ((1 << kSgrProjScaleBits) + (n2_with_scale >> 1)) / n2_with_scale;
  const uint32_t s = kSgrScaleParameter[sgr_proj_index][pass];
  assert(s != 0);
  const ptrdiff_t array_stride = buffer->box_filter_process_intermediate_stride;
  const ptrdiff_t integral_image_stride =
      kRestorationProcessingUnitSizeWithBorders + 1;
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

  // Calculate intermediate results, including one-pixel border, for example,
  // if unit size is 64x64, we calculate 66x66 pixels.
  const int step = (pass == 0) ? 2 : 1;
  const ptrdiff_t intermediate_stride = step * array_stride;
  for (int y = -1; y <= height; y += step) {
    for (int x = -1; x <= width; ++x) {
      // The integral image helps to calculate the sum of the square
      // centered at (x, y).
      // The calculation of a, b is equal to the following lines:
      // uint32_t a = 0;
      // uint32_t b = 0;
      // for (int dy = -radius; dy <= radius; ++dy) {
      //   for (int dx = -radius; dx <= radius; ++dx) {
      //     const Pixel source = src[(y + dy) * stride + (x + dx)];
      //     a += source * source;
      //     b += source;
      //   }
      // }
      const int top_left =
          (y + kRestorationBorder - radius) * integral_image_stride + x +
          kRestorationBorder - radius;
      const int top_right = top_left + 2 * radius + 1;
      const int bottom_left =
          top_left + (2 * radius + 1) * integral_image_stride;
      const int bottom_right = bottom_left + 2 * radius + 1;
      uint32_t a = square_integral_image[bottom_right] -
                   square_integral_image[bottom_left] -
                   square_integral_image[top_right] +
                   square_integral_image[top_left];
      uint32_t b;

      if (bitdepth <= 10 || radius < 2) {
        // The following cast is mandatory to get truncated sum.
        b = static_cast<uint16_t>(
            integral_image[bottom_right] - integral_image[bottom_left] -
            integral_image[top_right] + integral_image[top_left]);
      } else {
        assert(radius == 2);
        const uint16_t b_top_15_pixels =
            integral_image[top_right + 3 * integral_image_stride] -
            integral_image[top_left + 3 * integral_image_stride] -
            integral_image[top_right] + integral_image[top_left];
        const uint16_t b_bottom_10_pixels =
            integral_image[bottom_right] - integral_image[bottom_left] -
            integral_image[top_right + 3 * integral_image_stride] +
            integral_image[top_left + 3 * integral_image_stride];
        b = b_top_15_pixels + b_bottom_10_pixels;
      }

      // a: before shift, max is 25 * (2^(bitdepth) - 1) * (2^(bitdepth) - 1).
      // since max bitdepth = 12, max < 2^31.
      // after shift, a < 2^16 * n < 2^22 regardless of bitdepth
      a = RightShiftWithRounding(a, (bitdepth - 8) << 1);
      // b: max is 25 * (2^(bitdepth) - 1). If bitdepth = 12, max < 2^19.
      // d < 2^8 * n < 2^14 regardless of bitdepth
      const uint32_t d = RightShiftWithRounding(b, bitdepth - 8);
      // p: Each term in calculating p = a * n - b * b is < 2^16 * n^2 < 2^28,
      // and p itself satisfies p < 2^14 * n^2 < 2^26.
      // This bound on p is due to:
      // https://en.wikipedia.org/wiki/Popoviciu's_inequality_on_variances
      // Note: Sometimes, in high bitdepth, we can end up with a*n < b*b.
      // This is an artifact of rounding, and can only happen if all pixels
      // are (almost) identical, so in this case we saturate to p=0.
      const uint32_t p = (a * n < d * d) ? 0 : a * n - d * d;
      // p * s < (2^14 * n^2) * round(2^20 / (n^2 * scale)) < 2^34 / scale <
      // 2^32 as long as scale >= 4. So p * s fits into a uint32_t, and z < 2^12
      // (this holds even after accounting for the rounding in s)
      const uint32_t z = RightShiftWithRounding(p * s, kSgrProjScaleBits);
      // a2: range [1, 256].
      uint32_t a2 = kXByXPlus1[std::min(z, 255u)];
      const uint32_t one_over_n = kOneByX[n - 1];
      // (kSgrProjSgrBits - a2) < 2^8, b < 2^(bitdepth) * n,
      // one_over_n = round(2^12 / n)
      // => the product here is < 2^(20 + bitdepth) <= 2^32,
      // and b is set to a value < 2^(8 + bitdepth).
      // This holds even with the rounding in one_over_n and in the overall
      // result, as long as (kSgrProjSgrBits - a2) is strictly less than 2^8.
      const uint32_t b2 = ((1 << kSgrProjSgrBits) - a2) * b * one_over_n;
      intermediate_result[0][x] = a2;
      intermediate_result[1][x] =
          RightShiftWithRounding(b2, kSgrProjReciprocalBits);
    }
    intermediate_result[0] += intermediate_stride;
    intermediate_result[1] += intermediate_stride;
  }
}

template <int bitdepth, typename Pixel>
void LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterProcess(
    const RestorationUnitInfo& restoration_info, const Pixel* src,
    ptrdiff_t stride, int width, int height, RestorationBuffer* const buffer) {
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;

  // We calculate intermediate values for the region (width + 1) x (height + 1).
  // The region we can access is (width + 1 + radius) x (height + 1 + radius).
  // The max radius is 2. width = height =
  // kRestorationProcessingUnitSizeWithBorders.
  // For the integral_image, we need one row before the accessible region,
  // so the stride is kRestorationProcessingUnitSizeWithBorders + 1.
  // We fix the first row and first column of integral image be 0 to facilitate
  // computation.

  // Note that the max sum = (2 ^ bitdepth - 1) *
  // kRestorationProcessingUnitSizeWithBorders *
  // kRestorationProcessingUnitSizeWithBorders.
  // The max sum is larger than 2^16.
  // Case 8 bit and 10 bit:
  // The final box sum has at most 25 pixels, which is within 16 bits. So
  // keeping truncated 16-bit values is enough.
  // Case 12 bit, radius 1:
  // The final box sum has 9 pixels, which is within 16 bits. So keeping
  // truncated 16-bit values is enough.
  // Case 12 bit, radius 2:
  // The final box sum has 25 pixels. It can be calculated by calculating the
  // top 15 pixels and the bottom 10 pixels separately, and adding them
  // together. So keeping truncated 16-bit values is enough.
  // If it is slower than using 32-bit for specific CPU targets, please split
  // into 2 paths.
  uint16_t integral_image[(kRestorationProcessingUnitSizeWithBorders + 1) *
                          (kRestorationProcessingUnitSizeWithBorders + 1)];

  // Note that the max squared sum =
  // (2 ^ bitdepth - 1) * (2 ^ bitdepth - 1) *
  // kRestorationProcessingUnitSizeWithBorders *
  // kRestorationProcessingUnitSizeWithBorders.
  // For 8 bit, 32-bit is enough. For 10 bit and up, the sum could be larger
  // than 2^32. However, the final box sum has at most 25 squares, which is
  // within 32 bits. So keeping truncated 32-bit values is enough.
  uint32_t
      square_integral_image[(kRestorationProcessingUnitSizeWithBorders + 1) *
                            (kRestorationProcessingUnitSizeWithBorders + 1)];
  const ptrdiff_t integral_image_stride =
      kRestorationProcessingUnitSizeWithBorders + 1;
  const ptrdiff_t filtered_output_stride =
      buffer->box_filter_process_output_stride;
  const ptrdiff_t intermediate_stride =
      buffer->box_filter_process_intermediate_stride;
  const ptrdiff_t intermediate_buffer_offset =
      kRestorationBorder * intermediate_stride + kRestorationBorder;

  ComputeIntegralImage<Pixel>(
      src - kRestorationBorder * stride - kRestorationBorder, stride,
      width + 2 * kRestorationBorder, height + 2 * kRestorationBorder,
      integral_image, square_integral_image, integral_image_stride);

  for (int pass = 0; pass < 2; ++pass) {
    const uint8_t radius = kSgrProjParams[sgr_proj_index][pass * 2];
    if (radius == 0) continue;
    LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterPreProcess(
        restoration_info, integral_image, square_integral_image, width, height,
        pass, buffer);

    const Pixel* src_ptr = src;
    // Set intermediate buffer start point to the actual start point of
    // filtering.
    const uint32_t* array_start[2] = {
        buffer->box_filter_process_intermediate[0] + intermediate_buffer_offset,
        buffer->box_filter_process_intermediate[1] +
            intermediate_buffer_offset};
    int* filtered_output = buffer->box_filter_process_output[pass];
    for (int y = 0; y < height; ++y) {
      const int shift = (pass == 0 && (y & 1) != 0) ? 4 : 5;
      // array_start[0]: range [1, 256].
      // array_start[1] < 2^20.
      for (int x = 0; x < width; ++x) {
        uint32_t a, b;
        if (pass == 0) {
          if ((y & 1) == 0) {
            a = 5 * (array_start[0][-intermediate_stride + x - 1] +
                     array_start[0][-intermediate_stride + x + 1] +
                     array_start[0][intermediate_stride + x - 1] +
                     array_start[0][intermediate_stride + x + 1]) +
                6 * (array_start[0][-intermediate_stride + x] +
                     array_start[0][intermediate_stride + x]);
            b = 5 * (array_start[1][-intermediate_stride + x - 1] +
                     array_start[1][-intermediate_stride + x + 1] +
                     array_start[1][intermediate_stride + x - 1] +
                     array_start[1][intermediate_stride + x + 1]) +
                6 * (array_start[1][-intermediate_stride + x] +
                     array_start[1][intermediate_stride + x]);
          } else {
            a = 5 * (array_start[0][x - 1] + array_start[0][x + 1]) +
                6 * array_start[0][x];
            b = 5 * (array_start[1][x - 1] + array_start[1][x + 1]) +
                6 * array_start[1][x];
          }
        } else {
          a = 3 * (array_start[0][-intermediate_stride + x - 1] +
                   array_start[0][-intermediate_stride + x + 1] +
                   array_start[0][intermediate_stride + x - 1] +
                   array_start[0][intermediate_stride + x + 1]) +
              4 * (array_start[0][-intermediate_stride + x] +
                   array_start[0][x - 1] + array_start[0][x] +
                   array_start[0][x + 1] +
                   array_start[0][intermediate_stride + x]);
          b = 3 * (array_start[1][-intermediate_stride + x - 1] +
                   array_start[1][-intermediate_stride + x + 1] +
                   array_start[1][intermediate_stride + x - 1] +
                   array_start[1][intermediate_stride + x + 1]) +
              4 * (array_start[1][-intermediate_stride + x] +
                   array_start[1][x - 1] + array_start[1][x] +
                   array_start[1][x + 1] +
                   array_start[1][intermediate_stride + x]);
        }
        // v < 2^32. All intermediate calculations are positive.
        const uint32_t v = a * src_ptr[x] + b;
        filtered_output[x] = RightShiftWithRounding(
            v, kSgrProjSgrBits + shift - kSgrProjRestoreBits);
      }
      src_ptr += stride;
      array_start[0] += intermediate_stride;
      array_start[1] += intermediate_stride;
      filtered_output += filtered_output_stride;
    }
  }
}

// Assume box_filter_process_output[2] are allocated before calling
// this function. Their sizes are width * height, stride equals width.
template <int bitdepth, typename Pixel>
void LoopRestorationFuncs_C<bitdepth, Pixel>::SelfGuidedFilter(
    const void* const source, void* const dest,
    const RestorationUnitInfo& restoration_info, ptrdiff_t source_stride,
    ptrdiff_t dest_stride, int width, int height,
    RestorationBuffer* const buffer) {
  const int w0 = restoration_info.sgr_proj_info.multiplier[0];
  const int w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int w2 = (1 << kSgrProjPrecisionBits) - w0 - w1;
  const int index = restoration_info.sgr_proj_info.index;
  const int radius_pass_0 = kSgrProjParams[index][0];
  const int radius_pass_1 = kSgrProjParams[index][2];
  const ptrdiff_t array_stride = buffer->box_filter_process_output_stride;
  const int* box_filter_process_output[2] = {
      buffer->box_filter_process_output[0],
      buffer->box_filter_process_output[1]};
  const auto* src = static_cast<const Pixel*>(source);
  auto* dst = static_cast<Pixel*>(dest);
  source_stride /= sizeof(Pixel);
  dest_stride /= sizeof(Pixel);
  LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterProcess(
      restoration_info, src, source_stride, width, height, buffer);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int u = src[x] << kSgrProjRestoreBits;
      int v = w1 * u;
      if (radius_pass_0 != 0) {
        v += w0 * box_filter_process_output[0][x];
      } else {
        v += w0 * u;
      }
      if (radius_pass_1 != 0) {
        v += w2 * box_filter_process_output[1][x];
      } else {
        v += w2 * u;
      }
      // if radius_pass_0 == 0 and radius_pass_1 == 0, the range of v is:
      // bits(u) + bits(w0/w1/w2) + 2 = bitdepth + 13.
      // Then, range of s is bitdepth + 2. This is a rough estimation, taking
      // the maximum value of each element.
      const int s = RightShiftWithRounding(
          v, kSgrProjRestoreBits + kSgrProjPrecisionBits);
      dst[x] = static_cast<Pixel>(Clip3(s, 0, (1 << bitdepth) - 1));
    }
    src += source_stride;
    dst += dest_stride;
    box_filter_process_output[0] += array_stride;
    box_filter_process_output[1] += array_stride;
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->loop_restorations[0] = LoopRestorationFuncs_C<8, uint8_t>::WienerFilter;
  dsp->loop_restorations[1] =
      LoopRestorationFuncs_C<8, uint8_t>::SelfGuidedFilter;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp8bpp_WienerFilter
  dsp->loop_restorations[0] = LoopRestorationFuncs_C<8, uint8_t>::WienerFilter;
#endif
#ifndef LIBGAV1_Dsp8bpp_SelfGuidedFilter
  dsp->loop_restorations[1] =
      LoopRestorationFuncs_C<8, uint8_t>::SelfGuidedFilter;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#if LIBGAV1_MAX_BITDEPTH >= 10

void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->loop_restorations[0] =
      LoopRestorationFuncs_C<10, uint16_t>::WienerFilter;
  dsp->loop_restorations[1] =
      LoopRestorationFuncs_C<10, uint16_t>::SelfGuidedFilter;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp10bpp_WienerFilter
  dsp->loop_restorations[0] =
      LoopRestorationFuncs_C<10, uint16_t>::WienerFilter;
#endif
#ifndef LIBGAV1_Dsp10bpp_SelfGuidedFilter
  dsp->loop_restorations[1] =
      LoopRestorationFuncs_C<10, uint16_t>::SelfGuidedFilter;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#endif  // LIBGAV1_MAX_BITDEPTH >= 10
}  // namespace

void LoopRestorationInit_C() {
  Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  Init10bpp();
#endif
  // Local functions that may be unused depending on the optimizations
  // available.
  static_cast<void>(CountZeroCoefficients);
  static_cast<void>(PopulateWienerCoefficients);
  static_cast<void>(WienerVertical);
}

}  // namespace dsp
}  // namespace libgav1
