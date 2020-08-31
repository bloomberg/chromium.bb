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

#include "src/post_filter.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/array_2d.h"
#include "src/utils/constants.h"
#include "src/utils/memory.h"
#include "src/utils/types.h"

namespace libgav1 {
namespace {

// Row indices of deblocked pixels needed by loop restoration. This is used to
// populate the |deblock_buffer_| when cdef is on. The first dimension is
// subsampling_y.
constexpr int kDeblockedRowsForLoopRestoration[2][4] = {{54, 55, 56, 57},
                                                        {26, 27, 28, 29}};

// The following example illustrates how ExtendFrame() extends a frame.
// Suppose the frame width is 8 and height is 4, and left, right, top, and
// bottom are all equal to 3.
//
// Before:
//
//       ABCDEFGH
//       IJKLMNOP
//       QRSTUVWX
//       YZabcdef
//
// After:
//
//   AAA|ABCDEFGH|HHH  [3]
//   AAA|ABCDEFGH|HHH
//   AAA|ABCDEFGH|HHH
//   ---+--------+---
//   AAA|ABCDEFGH|HHH  [1]
//   III|IJKLMNOP|PPP
//   QQQ|QRSTUVWX|XXX
//   YYY|YZabcdef|fff
//   ---+--------+---
//   YYY|YZabcdef|fff  [2]
//   YYY|YZabcdef|fff
//   YYY|YZabcdef|fff
//
// ExtendFrame() first extends the rows to the left and to the right[1]. Then
// it copies the extended last row to the bottom borders[2]. Finally it copies
// the extended first row to the top borders[3].
template <typename Pixel>
void ExtendFrame(uint8_t* const frame_start, const int width, const int height,
                 ptrdiff_t stride, const int left, const int right,
                 const int top, const int bottom) {
  auto* const start = reinterpret_cast<Pixel*>(frame_start);
  const Pixel* src = start;
  Pixel* dst = start - left;
  stride /= sizeof(Pixel);
  // Copy to left and right borders.
  for (int y = 0; y < height; ++y) {
    Memset(dst, src[0], left);
    Memset(dst + (left + width), src[width - 1], right);
    src += stride;
    dst += stride;
  }
  // Copy to bottom borders. For performance we copy |stride| pixels
  // (including some padding pixels potentially) in each row, ending at the
  // bottom right border pixel. In the diagram the asterisks indicate padding
  // pixels.
  //
  // |<--- stride --->|
  // **YYY|YZabcdef|fff <-- Copy from the extended last row.
  // -----+--------+---
  // **YYY|YZabcdef|fff
  // **YYY|YZabcdef|fff
  // **YYY|YZabcdef|fff <-- bottom right border pixel
  assert(src == start + height * stride);
  dst = const_cast<Pixel*>(src) + width + right - stride;
  src = dst - stride;
  for (int y = 0; y < bottom; ++y) {
    memcpy(dst, src, sizeof(Pixel) * stride);
    dst += stride;
  }
  // Copy to top borders. For performance we copy |stride| pixels (including
  // some padding pixels potentially) in each row, starting from the top left
  // border pixel. In the diagram the asterisks indicate padding pixels.
  //
  // +-- top left border pixel
  // |
  // v
  // AAA|ABCDEFGH|HHH**
  // AAA|ABCDEFGH|HHH**
  // AAA|ABCDEFGH|HHH**
  // ---+--------+-----
  // AAA|ABCDEFGH|HHH** <-- Copy from the extended first row.
  // |<--- stride --->|
  src = start - left;
  dst = start - left - top * stride;
  for (int y = 0; y < top; ++y) {
    memcpy(dst, src, sizeof(Pixel) * stride);
    dst += stride;
  }
}

}  // namespace

PostFilter::PostFilter(
    const ObuFrameHeader& frame_header,
    const ObuSequenceHeader& sequence_header, LoopFilterMask* const masks,
    const Array2D<int16_t>& cdef_index,
    const Array2D<TransformSize>& inter_transform_sizes,
    LoopRestorationInfo* const restoration_info,
    BlockParametersHolder* block_parameters, YuvBuffer* const frame_buffer,
    YuvBuffer* const deblock_buffer, const dsp::Dsp* dsp,
    ThreadPool* const thread_pool, uint8_t* const threaded_window_buffer,
    uint8_t* const superres_line_buffer, int do_post_filter_mask)
    : frame_header_(frame_header),
      loop_restoration_(frame_header.loop_restoration),
      dsp_(*dsp),
      // Deblocking filter always uses 64x64 as step size.
      num_64x64_blocks_per_row_(DivideBy64(frame_header.width + 63)),
      upscaled_width_(frame_header.upscaled_width),
      width_(frame_header.width),
      height_(frame_header.height),
      bitdepth_(sequence_header.color_config.bitdepth),
      subsampling_x_{0, sequence_header.color_config.subsampling_x,
                     sequence_header.color_config.subsampling_x},
      subsampling_y_{0, sequence_header.color_config.subsampling_y,
                     sequence_header.color_config.subsampling_y},
      planes_(sequence_header.color_config.is_monochrome ? kMaxPlanesMonochrome
                                                         : kMaxPlanes),
      pixel_size_(static_cast<int>((bitdepth_ == 8) ? sizeof(uint8_t)
                                                    : sizeof(uint16_t))),
      masks_(masks),
      cdef_index_(cdef_index),
      inter_transform_sizes_(inter_transform_sizes),
      threaded_window_buffer_(threaded_window_buffer),
      restoration_info_(restoration_info),
      window_buffer_width_(GetWindowBufferWidth(thread_pool, frame_header)),
      window_buffer_height_(GetWindowBufferHeight(thread_pool, frame_header)),
      superres_line_buffer_(superres_line_buffer),
      block_parameters_(*block_parameters),
      frame_buffer_(*frame_buffer),
      deblock_buffer_(*deblock_buffer),
      do_post_filter_mask_(do_post_filter_mask),
      thread_pool_(thread_pool) {
  const int8_t zero_delta_lf[kFrameLfCount] = {};
  ComputeDeblockFilterLevels(zero_delta_lf, deblock_filter_levels_);
  if (DoDeblock()) {
    InitDeblockFilterParams();
  }
  if (DoSuperRes()) {
    for (int plane = 0; plane < planes_; ++plane) {
      const int downscaled_width =
          RightShiftWithRounding(width_, subsampling_x_[plane]);
      const int upscaled_width =
          RightShiftWithRounding(upscaled_width_, subsampling_x_[plane]);
      const int superres_width = downscaled_width << kSuperResScaleBits;
      super_res_info_[plane].step =
          (superres_width + upscaled_width / 2) / upscaled_width;
      const int error =
          super_res_info_[plane].step * upscaled_width - superres_width;
      super_res_info_[plane].initial_subpixel_x =
          ((-((upscaled_width - downscaled_width) << (kSuperResScaleBits - 1)) +
            DivideBy2(upscaled_width)) /
               upscaled_width +
           (1 << (kSuperResExtraBits - 1)) - error / 2) &
          kSuperResScaleMask;
      super_res_info_[plane].upscaled_width = upscaled_width;
    }
  }
  for (int plane = 0; plane < planes_; ++plane) {
    loop_restoration_buffer_[plane] = frame_buffer_.data(plane);
    cdef_buffer_[plane] = frame_buffer_.data(plane);
    superres_buffer_[plane] = frame_buffer_.data(plane);
    source_buffer_[plane] = frame_buffer_.data(plane);
  }
  // In single threaded mode, we apply SuperRes without making a copy of the
  // input row by writing the output to one row to the top (we refer to this
  // process as "in place superres" in our code).
  const bool in_place_superres = DoSuperRes() && thread_pool == nullptr;
  if (DoCdef() || DoRestoration() || in_place_superres) {
    for (int plane = 0; plane < planes_; ++plane) {
      int horizontal_shift = 0;
      int vertical_shift = 0;
      if (DoRestoration() &&
          loop_restoration_.type[plane] != kLoopRestorationTypeNone) {
        horizontal_shift += frame_buffer_.alignment();
        vertical_shift += kRestorationBorder;
        superres_buffer_[plane] +=
            vertical_shift * frame_buffer_.stride(plane) +
            horizontal_shift * pixel_size_;
      }
      if (in_place_superres) {
        vertical_shift += kSuperResVerticalBorder;
      }
      cdef_buffer_[plane] += vertical_shift * frame_buffer_.stride(plane) +
                             horizontal_shift * pixel_size_;
      if (DoCdef()) {
        horizontal_shift += frame_buffer_.alignment();
        vertical_shift += kCdefBorder;
      }
      source_buffer_[plane] += vertical_shift * frame_buffer_.stride(plane) +
                               horizontal_shift * pixel_size_;
    }
  }
}

void PostFilter::ExtendFrameBoundary(uint8_t* const frame_start,
                                     const int width, const int height,
                                     const ptrdiff_t stride, const int left,
                                     const int right, const int top,
                                     const int bottom) {
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth_ >= 10) {
    ExtendFrame<uint16_t>(frame_start, width, height, stride, left, right, top,
                          bottom);
    return;
  }
#endif
  ExtendFrame<uint8_t>(frame_start, width, height, stride, left, right, top,
                       bottom);
}

void PostFilter::ExtendBordersForReferenceFrame() {
  if (frame_header_.refresh_frame_flags == 0) return;
  for (int plane = kPlaneY; plane < planes_; ++plane) {
    const int plane_width =
        RightShiftWithRounding(upscaled_width_, subsampling_x_[plane]);
    const int plane_height =
        RightShiftWithRounding(height_, subsampling_y_[plane]);
    assert(frame_buffer_.left_border(plane) >= kMinLeftBorderPixels &&
           frame_buffer_.right_border(plane) >= kMinRightBorderPixels &&
           frame_buffer_.top_border(plane) >= kMinTopBorderPixels &&
           frame_buffer_.bottom_border(plane) >= kMinBottomBorderPixels);
    // plane subsampling_x_ left_border
    //   Y        N/A         64, 48
    //  U,V        0          64, 48
    //  U,V        1          32, 16
    assert(frame_buffer_.left_border(plane) >= 16);
    // The |left| argument to ExtendFrameBoundary() must be at least
    // kMinLeftBorderPixels (13) for warp.
    static_assert(16 >= kMinLeftBorderPixels, "");
    ExtendFrameBoundary(
        frame_buffer_.data(plane), plane_width, plane_height,
        frame_buffer_.stride(plane), frame_buffer_.left_border(plane),
        frame_buffer_.right_border(plane), frame_buffer_.top_border(plane),
        frame_buffer_.bottom_border(plane));
  }
}

void PostFilter::CopyDeblockedPixels(Plane plane, int row4x4) {
  const ptrdiff_t src_stride = frame_buffer_.stride(plane);
  const uint8_t* const src =
      GetSourceBuffer(static_cast<Plane>(plane), row4x4, 0);
  const ptrdiff_t dst_stride = deblock_buffer_.stride(plane);
  const int row_offset = DivideBy4(row4x4);
  uint8_t* dst = deblock_buffer_.data(plane) + dst_stride * row_offset;
  const int num_pixels = SubsampledValue(MultiplyBy4(frame_header_.columns4x4),
                                         subsampling_x_[plane]);
  int last_valid_row = -1;
  const int plane_height =
      SubsampledValue(frame_header_.height, subsampling_y_[plane]);
  for (int i = 0; i < 4; ++i) {
    int row = kDeblockedRowsForLoopRestoration[subsampling_y_[plane]][i];
    const int absolute_row =
        (MultiplyBy4(row4x4) >> subsampling_y_[plane]) + row;
    if (absolute_row >= plane_height) {
      if (last_valid_row == -1) {
        // We have run out of rows and there no valid row to copy. This will not
        // be used by loop restoration, so we can simply break here. However,
        // MSAN does not know that this is never used (since we sometimes apply
        // superres to this row as well). So zero it out in case of MSAN.
#if LIBGAV1_MSAN
        if (DoSuperRes()) {
          memset(dst, 0, num_pixels * pixel_size_);
          dst += dst_stride;
          continue;
        }
#endif
        break;
      }
      // If we run out of rows, copy the last valid row (mimics the bottom
      // border extension).
      row = last_valid_row;
    }
    memcpy(dst, src + src_stride * row, num_pixels * pixel_size_);
    last_valid_row = row;
    dst += dst_stride;
  }
}

void PostFilter::CopyBordersForOneSuperBlockRow(int row4x4, int sb4x4,
                                                bool for_loop_restoration) {
  // Number of rows to be subtracted from the start position described by
  // row4x4. We always lag by 8 rows (to account for in-loop post filters).
  const int row_offset = (row4x4 == 0) ? 0 : 8;
  // Number of rows to be subtracted from the height described by sb4x4.
  const int height_offset = (row4x4 == 0) ? 8 : 0;
  // If cdef is off, then loop restoration needs 2 extra rows for the bottom
  // border in each plane.
  const int extra_rows = (for_loop_restoration && !DoCdef()) ? 2 : 0;
  for (int plane = 0; plane < planes_; ++plane) {
    const int plane_width =
        RightShiftWithRounding(upscaled_width_, subsampling_x_[plane]);
    const int plane_height =
        RightShiftWithRounding(height_, subsampling_y_[plane]);
    const int row = (MultiplyBy4(row4x4) - row_offset) >> subsampling_y_[plane];
    assert(row >= 0);
    if (row >= plane_height) break;
    const int num_rows =
        std::min(RightShiftWithRounding(MultiplyBy4(sb4x4) - height_offset,
                                        subsampling_y_[plane]) +
                     extra_rows,
                 plane_height - row);
    // We only need to track the progress of the Y plane since the progress of
    // the U and V planes will be inferred from the progress of the Y plane.
    if (!for_loop_restoration && plane == kPlaneY) {
      progress_row_ = row + num_rows;
    }
    const bool copy_bottom = row + num_rows == plane_height;
    const int stride = frame_buffer_.stride(plane);
    uint8_t* const start = (for_loop_restoration ? superres_buffer_[plane]
                                                 : frame_buffer_.data(plane)) +
                           row * stride;
    const int left_border = for_loop_restoration
                                ? kRestorationBorder
                                : frame_buffer_.left_border(plane);
    const int right_border = for_loop_restoration
                                 ? kRestorationBorder
                                 : frame_buffer_.right_border(plane);
    const int top_border =
        (row == 0) ? (for_loop_restoration ? kRestorationBorder
                                           : frame_buffer_.top_border(plane))
                   : 0;
    const int bottom_border =
        copy_bottom
            ? (for_loop_restoration ? kRestorationBorder
                                    : frame_buffer_.bottom_border(plane))
            : 0;
#if LIBGAV1_MAX_BITDEPTH >= 10
    if (bitdepth_ >= 10) {
      ExtendFrame<uint16_t>(start, plane_width, num_rows, stride, left_border,
                            right_border, top_border, bottom_border);
      continue;
    }
#endif
    ExtendFrame<uint8_t>(start, plane_width, num_rows, stride, left_border,
                         right_border, top_border, bottom_border);
  }
}

void PostFilter::ApplyFilteringThreaded() {
  if (DoDeblock()) ApplyDeblockFilterThreaded();
  if (DoCdef() && DoRestoration()) {
    for (int row4x4 = 0; row4x4 < frame_header_.rows4x4;
         row4x4 += kNum4x4InLoopFilterMaskUnit) {
      SetupDeblockBuffer(row4x4, kNum4x4InLoopFilterMaskUnit);
    }
  }
  if (DoCdef()) ApplyCdef();
  if (DoSuperRes()) ApplySuperResThreaded();
  if (DoRestoration()) ApplyLoopRestoration();
  ExtendBordersForReferenceFrame();
}

int PostFilter::ApplyFilteringForOneSuperBlockRow(int row4x4, int sb4x4,
                                                  bool is_last_row) {
  if (row4x4 < 0) return -1;
  if (DoDeblock()) {
    ApplyDeblockFilterForOneSuperBlockRow(row4x4, sb4x4);
  }
  if (DoRestoration() && DoCdef()) {
    SetupDeblockBuffer(row4x4, sb4x4);
  }
  if (DoCdef()) {
    ApplyCdefForOneSuperBlockRow(row4x4, sb4x4, is_last_row);
  }
  if (DoSuperRes()) {
    ApplySuperResForOneSuperBlockRow(row4x4, sb4x4, is_last_row);
  }
  if (DoRestoration()) {
    CopyBordersForOneSuperBlockRow(row4x4, sb4x4, true);
    ApplyLoopRestorationForOneSuperBlockRow(row4x4, sb4x4);
    if (is_last_row) {
      // Loop restoration operates with a lag of 8 rows. So make sure to cover
      // all the rows of the last superblock row.
      CopyBordersForOneSuperBlockRow(row4x4 + sb4x4, 16, true);
      ApplyLoopRestorationForOneSuperBlockRow(row4x4 + sb4x4, 16);
    }
  }
  if (frame_header_.refresh_frame_flags != 0 && DoBorderExtensionInLoop()) {
    CopyBordersForOneSuperBlockRow(row4x4, sb4x4, false);
    if (is_last_row) {
      CopyBordersForOneSuperBlockRow(row4x4 + sb4x4, 16, false);
    }
  }
  if (is_last_row && !DoBorderExtensionInLoop()) {
    ExtendBordersForReferenceFrame();
  }
  return is_last_row ? height_ : progress_row_;
}

}  // namespace libgav1
