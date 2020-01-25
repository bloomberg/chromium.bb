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

#include "src/dsp/warp.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"
#include "src/utils/memory.h"

namespace libgav1 {
namespace dsp {
namespace {

// Number of extra bits of precision in warped filtering.
constexpr int kWarpedDiffPrecisionBits = 10;

template <bool clip, int bitdepth, typename Pixel>
void Warp_C(const void* const source, ptrdiff_t source_stride,
            const int source_width, const int source_height,
            const int* const warp_params, const int subsampling_x,
            const int subsampling_y, const int inter_round_bits_vertical,
            const int block_start_x, const int block_start_y,
            const int block_width, const int block_height, const int16_t alpha,
            const int16_t beta, const int16_t gamma, const int16_t delta,
            void* dest, ptrdiff_t dest_stride) {
  constexpr int kRoundBitsHorizontal = (bitdepth == 12)
                                           ? kInterRoundBitsHorizontal12bpp
                                           : kInterRoundBitsHorizontal;
  // Compound calculations are the only ones which use a different value for
  // |inter_round_bits_vertical|. Inter/intra will be able to use the |clip|
  // path when the offset value is removed.
  assert(!clip || inter_round_bits_vertical == ((bitdepth == 12) ? 9 : 11));
  // These offsets allow intermediate calculations to remain in 16 bits when
  // |bitdepth| is 8.
  // The worst case calculations for |kWarpedFilters| are a set of positive taps
  // which sum to 175 or negative taps which sum to -47.
  // For |bitdepth| == 8 the right set of input values could generate
  // intermediate values of 255 * 175 = 44625 / 0xAE51 or 255 * -47 = -11985.
  // Both situations end up using the MSB which makes it impossible to
  // distinguish between negative values or positive values over 32768.
  // In order to compensate for this we add a constant value:
  // |horizontal_offset|. This shifts the range up. It is now 61009 to 4399 and
  // fits comfortably in uint16_t.
  // |vertical_offset| ensures the result of the second pass is unsigned but can
  // not keep the value within 16 bits.
  // |unsigned_offset| is the delta between the compensated result and the
  // natural result.
  // These are not beneficial for |bitdepth| > 8 because it will always be
  // necessary to step up to 32 bit intermediates.
  // TODO(b/146439793): Remove the offset before storing the result.
  const int unsigned_offset =
      ((1 << bitdepth) + (1 << (bitdepth - 1)))
      << ((14 - kRoundBitsHorizontal) - inter_round_bits_vertical);
  constexpr int horizontal_offset = (1 << (bitdepth - 1)) << kFilterBits;
  constexpr int vertical_offset = (1 << bitdepth)
                                  << (2 * kFilterBits - kRoundBitsHorizontal);
  constexpr int kMaxPixel = (1 << bitdepth) - 1;
  union {
    // Intermediate_result is the output of the horizontal filtering and
    // rounding. The range is within 16 bits (unsigned).
    uint16_t intermediate_result[15][8];  // 15 rows, 8 columns.
    // In the simple special cases where the samples in each row are all the
    // same, store one sample per row in a column vector.
    uint16_t intermediate_result_column[15];
  };
  const auto* const src = static_cast<const Pixel*>(source);
  source_stride /= sizeof(Pixel);
  using DestType = typename std::conditional<clip, Pixel, uint16_t>::type;
  auto* dst = static_cast<DestType*>(dest);
  if (clip) dest_stride /= sizeof(dst[0]);

  assert(block_width >= 8);
  assert(block_height >= 8);

  // Warp process applies for each 8x8 block (or smaller).
  for (int start_y = block_start_y; start_y < block_start_y + block_height;
       start_y += 8) {
    for (int start_x = block_start_x; start_x < block_start_x + block_width;
         start_x += 8) {
      const int src_x = (start_x + 4) << subsampling_x;
      const int src_y = (start_y + 4) << subsampling_y;
      const int dst_x =
          src_x * warp_params[2] + src_y * warp_params[3] + warp_params[0];
      const int dst_y =
          src_x * warp_params[4] + src_y * warp_params[5] + warp_params[1];
      const int x4 = dst_x >> subsampling_x;
      const int y4 = dst_y >> subsampling_y;
      const int ix4 = x4 >> kWarpedModelPrecisionBits;
      const int iy4 = y4 >> kWarpedModelPrecisionBits;

      // A prediction block may fall outside the frame's boundaries. If a
      // prediction block is calculated using only samples outside the frame's
      // boundary, the filtering can be simplified. We can divide the plane
      // into several regions and handle them differently.
      //
      //                |           |
      //            1   |     3     |   1
      //                |           |
      //         -------+-----------+-------
      //                |***********|
      //            2   |*****4*****|   2
      //                |***********|
      //         -------+-----------+-------
      //                |           |
      //            1   |     3     |   1
      //                |           |
      //
      // At the center, region 4 represents the frame and is the general case.
      //
      // In regions 1 and 2, the prediction block is outside the frame's
      // boundary horizontally. Therefore the horizontal filtering can be
      // simplified. Furthermore, in the region 1 (at the four corners), the
      // prediction is outside the frame's boundary both horizontally and
      // vertically, so we get a constant prediction block.
      //
      // In region 3, the prediction block is outside the frame's boundary
      // vertically. Unfortunately because we apply the horizontal filters
      // first, by the time we apply the vertical filters, they no longer see
      // simple inputs. So the only simplification is that all the rows are
      // the same, but we still need to apply all the horizontal and vertical
      // filters.

      // Check for two simple special cases, where the horizontal filter can
      // be significantly simplified.
      //
      // In general, for each row, the horizontal filter is calculated as
      // follows:
      //   for (int x = -4; x < 4; ++x) {
      //     const int offset = ...;
      //     int sum = horizontal_offset;
      //     for (int k = 0; k < 8; ++k) {
      //       const int column = Clip3(ix4 + x + k - 3, 0, source_width - 1);
      //       sum += kWarpedFilters[offset][k] * src_row[column];
      //     }
      //     ...
      //   }
      // The column index before clipping, ix4 + x + k - 3, varies in the range
      // ix4 - 7 <= ix4 + x + k - 3 <= ix4 + 7. If ix4 - 7 >= source_width - 1
      // or ix4 + 7 <= 0, then all the column indexes are clipped to the same
      // border index (source_width - 1 or 0, respectively). Then for each x,
      // the inner for loop of the horizontal filter is reduced to multiplying
      // the border pixel by the sum of the filter coefficients.
      if (ix4 - 7 >= source_width - 1 || ix4 + 7 <= 0) {
        // Regions 1 and 2.
        // Points to the left or right border of the first row of |src|.
        const Pixel* first_row_border =
            (ix4 + 7 <= 0) ? src : src + source_width - 1;
        // In general, for y in [-7, 8), the row number iy4 + y is clipped:
        //   const int row = Clip3(iy4 + y, 0, source_height - 1);
        // In two special cases, iy4 + y is clipped to either 0 or
        // source_height - 1 for all y. In the rest of the cases, iy4 + y is
        // bounded and we can avoid clipping iy4 + y by relying on a reference
        // frame's boundary extension on the top and bottom.
        if (iy4 - 7 >= source_height - 1 || iy4 + 7 <= 0) {
          // Region 1.
          // Every sample used to calculate the prediction block has the same
          // value. So the whole prediction block has the same value.
          const int row = (iy4 + 7 <= 0) ? 0 : source_height - 1;
          const Pixel row_border_pixel = first_row_border[row * source_stride];
          DestType* dst_row = dst + start_x - block_start_x;
          if (clip) {
            Memset(dst_row, row_border_pixel, 8);
          } else {
            int sum = row_border_pixel << ((14 - kRoundBitsHorizontal) -
                                           inter_round_bits_vertical);
            sum += unsigned_offset;
            Memset(dst_row, sum, 8);
          }
          const DestType* const first_dst_row = dst_row;
          dst_row += dest_stride;
          for (int y = 1; y < 8; ++y) {
            memcpy(dst_row, first_dst_row, 8 * sizeof(*dst_row));
            dst_row += dest_stride;
          }
          // End of region 1. Continue the |start_x| for loop.
          continue;
        }

        // Region 2.
        // Horizontal filter.
        // The input values in this region are generated by extending the border
        // which makes them identical in the horizontal direction. This
        // computation could be inlined in the vertical pass but most
        // implementations will need a transpose of some sort.
        // It is not necessary to use the offset values here because the
        // horizontal pass is a simple shift and the vertical pass will always
        // require using 32 bits.
        for (int y = -7; y < 8; ++y) {
          // We may over-read up to 13 pixels above the top source row, or up
          // to 13 pixels below the bottom source row. This is proved below.
          const int row = iy4 + y;
          int sum = first_row_border[row * source_stride];
          sum <<= kFilterBits - kRoundBitsHorizontal;
          intermediate_result_column[y + 7] = sum;
        }
        // Vertical filter.
        DestType* dst_row = dst + start_x - block_start_x;
        int sy4 =
            (y4 & ((1 << kWarpedModelPrecisionBits) - 1)) - MultiplyBy4(delta);
        for (int y = 0; y < 8; ++y) {
          int sy = sy4 - MultiplyBy4(gamma);
          for (int x = 0; x < 8; ++x) {
            const int offset =
                RightShiftWithRounding(sy, kWarpedDiffPrecisionBits) +
                kWarpedPixelPrecisionShifts;
            assert(offset >= 0);
            assert(offset < 3 * kWarpedPixelPrecisionShifts + 1);
            int sum = 0;
            for (int k = 0; k < 8; ++k) {
              sum +=
                  kWarpedFilters[offset][k] * intermediate_result_column[y + k];
            }
            sum = RightShiftWithRounding(sum, inter_round_bits_vertical);
            if (clip) {
              dst_row[x] = static_cast<DestType>(Clip3(sum, 0, kMaxPixel));
            } else {
              dst_row[x] = static_cast<DestType>(sum + unsigned_offset);
            }
            sy += gamma;
          }
          dst_row += dest_stride;
          sy4 += delta;
        }
        // End of region 2. Continue the |start_x| for loop.
        continue;
      }

      // Regions 3 and 4.
      // At this point, we know ix4 - 7 < source_width - 1 and ix4 + 7 > 0.
      // It follows that -6 <= ix4 <= source_width + 5. This inequality is
      // used below.

      // In general, for y in [-7, 8), the row number iy4 + y is clipped:
      //   const int row = Clip3(iy4 + y, 0, source_height - 1);
      // In two special cases, iy4 + y is clipped to either 0 or
      // source_height - 1 for all y. In the rest of the cases, iy4 + y is
      // bounded and we can avoid clipping iy4 + y by relying on a reference
      // frame's boundary extension on the top and bottom.
      if (iy4 - 7 >= source_height - 1 || iy4 + 7 <= 0) {
        // Region 3.
        // Horizontal filter.
        const int row = (iy4 + 7 <= 0) ? 0 : source_height - 1;
        const Pixel* const src_row = src + row * source_stride;
        int sx4 = (x4 & ((1 << kWarpedModelPrecisionBits) - 1)) - beta * 7;
        for (int y = -7; y < 8; ++y) {
          int sx = sx4 - MultiplyBy4(alpha);
          for (int x = -4; x < 4; ++x) {
            const int offset =
                RightShiftWithRounding(sx, kWarpedDiffPrecisionBits) +
                kWarpedPixelPrecisionShifts;
            // Since alpha and beta have been validated by SetupShear(), one
            // can prove that 0 <= offset <= 3 * 2^6.
            assert(offset >= 0);
            assert(offset < 3 * kWarpedPixelPrecisionShifts + 1);
            // For SIMD optimization:
            // For 8 bit, the range of sum is within uint16_t, if we add an
            // horizontal offset:
            int sum = horizontal_offset;
            // Horizontal_offset guarantees sum is non negative.
            // If horizontal_offset is used, intermediate_result needs to be
            // uint16_t.
            // For 10/12 bit, the range of sum is within 32 bits.
            for (int k = 0; k < 8; ++k) {
              // We assume the source frame has left and right borders of at
              // least 13 pixels that extend the frame boundary pixels.
              //
              // Since -4 <= x <= 3 and 0 <= k <= 7, using the inequality on
              // ix4 above, we have
              //   -13 <= ix4 + x + k - 3 <= source_width + 12,
              // or
              //   -13 <= column <= (source_width - 1) + 13.
              // Therefore we may over-read up to 13 pixels before the source
              // row, or up to 13 pixels after the source row.
              const int column = ix4 + x + k - 3;
              sum += kWarpedFilters[offset][k] * src_row[column];
            }
            assert(sum >= 0 && sum < (horizontal_offset << 2));
            intermediate_result[y + 7][x + 4] = static_cast<uint16_t>(
                RightShiftWithRounding(sum, kRoundBitsHorizontal));
            sx += alpha;
          }
          sx4 += beta;
        }
      } else {
        // Region 4.
        // Horizontal filter.
        // At this point, we know iy4 - 7 < source_height - 1 and iy4 + 7 > 0.
        // It follows that -6 <= iy4 <= source_height + 5. This inequality is
        // used below.
        int sx4 = (x4 & ((1 << kWarpedModelPrecisionBits) - 1)) - beta * 7;
        for (int y = -7; y < 8; ++y) {
          // We assume the source frame has top and bottom borders of at least
          // 13 pixels that extend the frame boundary pixels.
          //
          // Since -7 <= y <= 7, using the inequality on iy4 above, we have
          //   -13 <= iy4 + y <= source_height + 12,
          // or
          //   -13 <= row <= (source_height - 1) + 13.
          // Therefore we may over-read up to 13 pixels above the top source
          // row, or up to 13 pixels below the bottom source row.
          const int row = iy4 + y;
          const Pixel* const src_row = src + row * source_stride;
          int sx = sx4 - MultiplyBy4(alpha);
          for (int x = -4; x < 4; ++x) {
            const int offset =
                RightShiftWithRounding(sx, kWarpedDiffPrecisionBits) +
                kWarpedPixelPrecisionShifts;
            // Since alpha and beta have been validated by SetupShear(), one
            // can prove that 0 <= offset <= 3 * 2^6.
            assert(offset >= 0);
            assert(offset < 3 * kWarpedPixelPrecisionShifts + 1);
            // For SIMD optimization:
            // For 8 bit, the range of sum is within uint16_t, if we add an
            // horizontal offset:
            int sum = horizontal_offset;
            // Horizontal_offset guarantees sum is non negative.
            // If horizontal_offset is used, intermediate_result needs to be
            // uint16_t.
            // For 10/12 bit, the range of sum is within 32 bits.
            for (int k = 0; k < 8; ++k) {
              // We assume the source frame has left and right borders of at
              // least 13 pixels that extend the frame boundary pixels.
              //
              // Since -4 <= x <= 3 and 0 <= k <= 7, using the inequality on
              // ix4 above, we have
              //   -13 <= ix4 + x + k - 3 <= source_width + 12,
              // or
              //   -13 <= column <= (source_width - 1) + 13.
              // Therefore we may over-read up to 13 pixels before the source
              // row, or up to 13 pixels after the source row.
              const int column = ix4 + x + k - 3;
              sum += kWarpedFilters[offset][k] * src_row[column];
            }
            assert(sum >= 0 && sum < (horizontal_offset << 2));
            intermediate_result[y + 7][x + 4] = static_cast<uint16_t>(
                RightShiftWithRounding(sum, kRoundBitsHorizontal));
            sx += alpha;
          }
          sx4 += beta;
        }
      }

      // Regions 3 and 4.
      // Vertical filter.
      DestType* dst_row = dst + start_x - block_start_x;
      int sy4 =
          (y4 & ((1 << kWarpedModelPrecisionBits) - 1)) - MultiplyBy4(delta);
      // The spec says we should use the following loop condition:
      //   y < std::min(4, block_start_y + block_height - start_y - 4);
      // We can prove that block_start_y + block_height - start_y >= 8, which
      // implies std::min(4, block_start_y + block_height - start_y - 4) = 4.
      // So the loop condition is simply y < 4.
      //
      //   Proof:
      //      start_y < block_start_y + block_height
      //   => block_start_y + block_height - start_y > 0
      //   => block_height - (start_y - block_start_y) > 0
      //
      //   Since block_height >= 8 and is a power of 2, it follows that
      //   block_height is a multiple of 8. start_y - block_start_y is also a
      //   multiple of 8. Therefore their difference is a multiple of 8. Since
      //   their difference is > 0, their difference must be >= 8.
      //
      // We then add an offset of 4 to y so that the loop starts with y = 0
      // and continues if y < 8.
      for (int y = 0; y < 8; ++y) {
        int sy = sy4 - MultiplyBy4(gamma);
        // The spec says we should use the following loop condition:
        //   x < std::min(4, block_start_x + block_width - start_x - 4);
        // Similar to the above, we can prove that the loop condition can be
        // simplified to x < 4.
        //
        // We then add an offset of 4 to x so that the loop starts with x = 0
        // and continues if x < 8.
        for (int x = 0; x < 8; ++x) {
          const int offset =
              RightShiftWithRounding(sy, kWarpedDiffPrecisionBits) +
              kWarpedPixelPrecisionShifts;
          // Since gamma and delta have been validated by SetupShear(), one can
          // prove that 0 <= offset <= 3 * 2^6.
          assert(offset >= 0);
          assert(offset < 3 * kWarpedPixelPrecisionShifts + 1);
          // Similar to horizontal_offset, vertical_offset guarantees sum
          // before shifting is non negative:
          int sum = vertical_offset;
          for (int k = 0; k < 8; ++k) {
            sum += kWarpedFilters[offset][k] * intermediate_result[y + k][x];
          }
          assert(sum >= 0 && sum < (vertical_offset << 2));
          sum = RightShiftWithRounding(sum, inter_round_bits_vertical);
          if (clip) {
            dst_row[x] = static_cast<DestType>(
                Clip3(sum - unsigned_offset, 0, kMaxPixel));
          } else {
            // Warp output is a predictor, whose type is uint16_t.
            // Clipping is applied at the stage of final pixel value output.
            dst_row[x] = static_cast<DestType>(sum);
          }
          sy += gamma;
        }
        dst_row += dest_stride;
        sy4 += delta;
      }
    }
    dst += 8 * dest_stride;
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->warp = Warp_C<false, 8, uint8_t>;
  dsp->warp_clip = Warp_C<true, 8, uint8_t>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp8bpp_Warp
  dsp->warp = Warp_C<false, 8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_WarpClip
  dsp->warp_clip = Warp_C<true, 8, uint8_t>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#if LIBGAV1_MAX_BITDEPTH >= 10
void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->warp = Warp_C<false, 10, uint16_t>;
  dsp->warp_clip = Warp_C<true, 10, uint16_t>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp10bpp_Warp
  dsp->warp = Warp_C<false, 10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_WarpClip
  dsp->warp_clip = Warp_C<true, 10, uint16_t>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}
#endif

}  // namespace

void WarpInit_C() {
  Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  Init10bpp();
#endif
}

}  // namespace dsp
}  // namespace libgav1
