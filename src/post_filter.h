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

#ifndef LIBGAV1_SRC_POST_FILTER_H_
#define LIBGAV1_SRC_POST_FILTER_H_

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "src/dsp/common.h"
#include "src/dsp/dsp.h"
#include "src/loop_filter_mask.h"
#include "src/loop_restoration_info.h"
#include "src/obu_parser.h"
#include "src/utils/array_2d.h"
#include "src/utils/block_parameters_holder.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"
#include "src/utils/memory.h"
#include "src/utils/threadpool.h"
#include "src/yuv_buffer.h"

namespace libgav1 {

// This class applies in-loop filtering for each frame after it is
// reconstructed. The in-loop filtering contains all post processing filtering
// for the reconstructed frame, including deblock filter, CDEF, superres,
// and loop restoration.
// Historically, for example in libaom, loop filter refers to deblock filter.
// To avoid name conflicts, we call this class PostFilter (post processing).
// Input info includes deblock parameters (bit masks), CDEF
// parameters, super resolution parameters and loop restoration parameters.
// In-loop post filtering order is:
// deblock --> CDEF --> super resolution--> loop restoration.
// When CDEF and super resolution is not used, we can combine deblock
// and restoration together to only filter frame buffer once.
class PostFilter {
 public:
  static constexpr int kCdefLargeValue = 0x4000;

  // This class does not take ownership of the masks/restoration_info, but it
  // may change their values.
  PostFilter(const ObuFrameHeader& frame_header,
             const ObuSequenceHeader& sequence_header,
             LoopFilterMask* const masks, const Array2D<int16_t>& cdef_index,
             const Array2D<TransformSize>& inter_transform_sizes,
             LoopRestorationInfo* const restoration_info,
             BlockParametersHolder* block_parameters,
             YuvBuffer* const source_buffer, const dsp::Dsp* dsp,
             ThreadPool* const thread_pool,
             uint8_t* const threaded_window_buffer, int do_post_filter_mask)
      : frame_header_(frame_header),
        loop_restoration_(frame_header.loop_restoration),
        dsp_(*dsp),
        // Deblocking filter always uses 64x64 as step size.
        num_64x64_blocks_per_row_(DivideBy64(frame_header.width + 63)),
        upscaled_width_(frame_header.upscaled_width),
        width_(frame_header.width),
        height_(frame_header.height),
        bitdepth_(sequence_header.color_config.bitdepth),
        subsampling_x_(sequence_header.color_config.subsampling_x),
        subsampling_y_(sequence_header.color_config.subsampling_y),
        planes_(sequence_header.color_config.is_monochrome
                    ? kMaxPlanesMonochrome
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
        block_parameters_(*block_parameters),
        source_buffer_(source_buffer),
        do_post_filter_mask_(do_post_filter_mask),
        thread_pool_(thread_pool) {
    const int8_t zero_delta_lf[kFrameLfCount] = {};
    ComputeDeblockFilterLevels(zero_delta_lf, deblock_filter_levels_);
  }

  // non copyable/movable.
  PostFilter(const PostFilter&) = delete;
  PostFilter& operator=(const PostFilter&) = delete;
  PostFilter(PostFilter&&) = delete;
  PostFilter& operator=(PostFilter&&) = delete;

  // The overall function that applies all post processing filtering.
  // * The filtering order is:
  //   deblock --> CDEF --> super resolution--> loop restoration.
  // * The output of each filter is the input for the following filter.
  //   A special case is that loop restoration needs both the deblocked frame
  //   and the cdef filtered frame:
  //   deblock --> CDEF --> super resolution --> loop restoration.
  //              |                                 ^
  //              |                                 |
  //              -----------> super resolution -----
  // * Any of these filters could be present or absent.
  // Two pointers are used in this class: source_buffer_ and cdef_buffer_.
  // * source_buffer_ always points to the input frame buffer, which holds the
  //   (upscaled) deblocked frame buffer during the ApplyFiltering() method.
  // * cdef_buffer_ points to the (upscaled) cdef filtered frame buffer,
  //   however, if cdef is not present, cdef_buffer_ is the same as
  //   source_buffer_.
  // Each filter:
  // * Deblock: in-place filtering. Input and output are both source_buffer_.
  // * Cdef: allocates cdef_filtered_buffer_.
  //         Sets cdef_buffer_ to cdef_filtered_buffer_.
  //         Input is source_buffer_. Output is cdef_buffer_.
  // * SuperRes: allocates super_res_buffer_.
  //             Inputs are source_buffer_ and cdef_buffer_.
  //             FrameSuperRes takes one input and applies super resolution.
  //             When FrameSuperRes is called, super_res_buffer_ is the
  //             intermediate buffer to hold a copy of the input.
  //             Super resolution process is applied and result
  //             is written to the input buffer.
  //             Therefore, contents of inputs are changed, but their meanings
  //             remain.
  // * Restoration: near in-place filtering. Allocates a local block for loop
  //                restoration units, which is 64x64.
  //                Inputs are source_buffer_ and cdef_buffer_.
  //                Output is source_buffer_.
  bool ApplyFiltering();
  bool DoCdef() const { return DoCdef(frame_header_, do_post_filter_mask_); }
  static bool DoCdef(const ObuFrameHeader& frame_header,
                     int do_post_filter_mask) {
    return (do_post_filter_mask & 0x02) != 0 &&
           (frame_header.cdef.bits > 0 ||
            frame_header.cdef.y_primary_strength[0] > 0 ||
            frame_header.cdef.y_secondary_strength[0] > 0 ||
            frame_header.cdef.uv_primary_strength[0] > 0 ||
            frame_header.cdef.uv_secondary_strength[0] > 0);
  }
  // If filter levels for Y plane (0 for vertical, 1 for horizontal),
  // are all zero, deblock filter will not be applied.
  static bool DoDeblock(const ObuFrameHeader& frame_header,
                        uint8_t do_post_filter_mask) {
    return (do_post_filter_mask & 0x01) != 0 &&
           (frame_header.loop_filter.level[0] > 0 ||
            frame_header.loop_filter.level[1] > 0);
  }
  bool DoDeblock() const {
    return DoDeblock(frame_header_, do_post_filter_mask_);
  }
  uint8_t GetZeroDeltaDeblockFilterLevel(int segment_id, int level_index,
                                         ReferenceFrameType type,
                                         int mode_id) const {
    return deblock_filter_levels_[segment_id][level_index][type][mode_id];
  }
  // Computes the deblock filter levels using |delta_lf| and stores them in
  // |deblock_filter_levels|.
  void ComputeDeblockFilterLevels(
      const int8_t delta_lf[kFrameLfCount],
      uint8_t deblock_filter_levels[kMaxSegments][kFrameLfCount]
                                   [kNumReferenceFrameTypes][2]) const;
  bool DoRestoration() const;
  // Returns true if loop restoration will be performed for the given parameters
  // and mask.
  static bool DoRestoration(const LoopRestoration& loop_restoration,
                            uint8_t do_post_filter_mask, int num_planes);
  bool DoSuperRes() const {
    return (do_post_filter_mask_ & 0x04) != 0 && width_ != upscaled_width_;
  }
  LoopFilterMask* masks() const { return masks_; }
  LoopRestorationInfo* restoration_info() const { return restoration_info_; }
  static uint8_t* SetBufferOffset(YuvBuffer* buffer, Plane plane, int row4x4,
                                  int column4x4, int8_t subsampling_x,
                                  int8_t subsampling_y) {
    const size_t pixel_size =
        (buffer->bitdepth() == 8) ? sizeof(uint8_t) : sizeof(uint16_t);
    return buffer->data(plane) +
           RowOrColumn4x4ToPixel(row4x4, plane, subsampling_y) *
               buffer->stride(plane) +
           RowOrColumn4x4ToPixel(column4x4, plane, subsampling_x) * pixel_size;
  }

  // Extends frame, sets border pixel values to its closest frame boundary.
  // Loop restoration needs three pixels border around current block.
  // If we are at the left boundary of the frame, we extend the frame 3
  // pixels to the left, and copy current pixel value to them.
  // If we are at the top boundary of the frame, we need to extend the frame
  // by three rows. They are copies of the first line of pixels.
  // Similarly for the right and bottom boundary.
  // The frame buffer should already be large enough to hold the extension.
  // Super resolution needs to fill frame border as well. The border size
  // is kBorderPixels.
  void ExtendFrameBoundary(uint8_t* frame_start, int width, int height,
                           ptrdiff_t stride, int left, int right, int top,
                           int bottom);

  static int GetWindowBufferWidth(const ThreadPool* const thread_pool,
                                  const ObuFrameHeader& frame_header) {
    return (thread_pool == nullptr) ? 0
                                    : Align(frame_header.upscaled_width, 64);
  }

  // For multi-threaded cdef and loop restoration, window height is the minimum
  // of the following two quantities:
  //  1) thread_count * 64
  //  2) frame_height rounded up to the nearest power of 64
  // Where 64 is the block size for cdef and loop restoration.
  static int GetWindowBufferHeight(const ThreadPool* const thread_pool,
                                   const ObuFrameHeader& frame_header) {
    if (thread_pool == nullptr) return 0;
    const int thread_count = 1 + thread_pool->num_threads();
    const int window_height = MultiplyBy64(thread_count);
    const int adjusted_frame_height = Align(frame_header.height, 64);
    return std::min(adjusted_frame_height, window_height);
  }

 private:
  // The type of the HorizontalDeblockFilter and VerticalDeblockFilter member
  // functions.
  using DeblockFilter = void (PostFilter::*)(Plane plane, int row4x4_start,
                                             int column4x4_start, int unit_id);
  // The lookup table for picking the deblock filter, according to:
  // kDeblockFilterBitMask (first dimension), and deblock filter type (second).
  const DeblockFilter deblock_filter_type_table_[2][2] = {
      {&PostFilter::VerticalDeblockFilterNoMask,
       &PostFilter::HorizontalDeblockFilterNoMask},
      {&PostFilter::VerticalDeblockFilter,
       &PostFilter::HorizontalDeblockFilter},
  };
  // Represents a job for a worker thread to apply the deblock filter.
  struct DeblockFilterJob : public Allocable {
    int plane;
    int row4x4;
    int row_unit;
  };
  // Buffers for loop restoration intermediate results. Depending on the filter
  // type, only one member of the union is used.
  union IntermediateBuffers {
    // For Wiener filter.
    // The array |intermediate| in Section 7.17.4, the intermediate results
    // between the horizontal and vertical filters.
    alignas(kMaxAlignment)
        uint16_t wiener[(kMaxSuperBlockSizeInPixels + kSubPixelTaps - 1) *
                        kMaxSuperBlockSizeInPixels];
    // For self-guided filter.
    struct {
      // The arrays flt0 and flt1 in Section 7.17.2, the outputs of the box
      // filter process in pass 0 and pass 1.
      alignas(
          kMaxAlignment) int32_t output[2][kMaxBoxFilterProcessOutputPixels];
      // The 2d arrays A and B in Section 7.17.3, the intermediate results in
      // the box filter process. Reused for pass 0 and pass 1.
      alignas(kMaxAlignment) uint32_t
          intermediate_a[kBoxFilterProcessIntermediatePixels];
      alignas(kMaxAlignment) uint32_t
          intermediate_b[kBoxFilterProcessIntermediatePixels];
    } box_filter;
  };

  bool ApplyDeblockFilter();
  void DeblockFilterWorker(const DeblockFilterJob* jobs, int num_jobs,
                           std::atomic<int>* job_counter,
                           DeblockFilter deblock_filter);
  bool ApplyDeblockFilterThreaded();

  uint8_t* GetCdefBufferAndStride(int start_x, int start_y, int plane,
                                  int subsampling_x, int subsampling_y,
                                  int window_buffer_plane_size,
                                  int vertical_shift, int horizontal_shift,
                                  int* cdef_stride) const;
  template <typename Pixel>
  void ApplyCdefForOneUnit(uint16_t* cdef_block, int index, int block_width4x4,
                           int block_height4x4, int row4x4_start,
                           int column4x4_start);
  template <typename Pixel>
  void ApplyCdefForOneRowInWindow(int row, int column);
  template <typename Pixel>
  bool ApplyCdefThreaded();
  bool ApplyCdef();  // Sections 7.15 and 7.15.1.

  bool ApplySuperRes();
  // Note for ApplyLoopRestoration():
  // First, we must differentiate loop restoration processing unit from loop
  // restoration unit.
  // (1). Loop restoration processing unit size is default to 64x64.
  // Only when the remaining filtering area is smaller than 64x64, the
  // processing unit size is the actual area size.
  // For U/V plane, it is (64 >> subsampling_x) x (64 >> subsampling_y).
  // (2). Loop restoration unit size can be 64x64, 128x128, 256x256 for Y
  // plane. The unit size for chroma can be the same or half, depending on
  // subsampling. If either subsampling_x or subsampling_y is one, unit size
  // is halved on both x and y sides.
  // All loop restoration units have the same size for one plane.
  // One loop restoration unit could contain multiple processing units.
  // But they share the same sets of loop restoration parameters.
  // (3). Loop restoration has a row offset, kRestorationUnitOffset = 8. The
  // size of first row of loop restoration units and processing units is
  // shrunk by the offset.
  // (4). Loop restoration units wrap the bottom and the right of the frame,
  // if the remaining area is small. The criteria is whether the number of
  // remaining rows/columns is smaller than half of loop restoration unit
  // size.
  // For example, if the frame size is 140x140, loop restoration unit size is
  // 128x128. The size of the first loop restoration unit is 128x(128-8) =
  // 128 columns x 120 rows.
  // Since 140 - 120 < 128/2. The remaining 20 rows will be folded to the loop
  // restoration unit. Similarly, the remaining 12 columns will also be folded
  // to current loop restoration unit. So, even frame size is 140x140,
  // there's only one loop restoration unit. Suppose processing unit is 64x64,
  // then sizes of the first row of processing units are 64x56, 64x56, 12x56,
  // respectively. The second row is 64x64, 64x64, 12x64.
  // The third row is 64x20, 64x20, 12x20.
  bool ApplyLoopRestoration();
  template <typename Pixel>
  bool ApplyLoopRestorationThreaded();
  template <typename Pixel>
  void ApplyLoopRestorationForOneRowInWindow(
      uint8_t* cdef_buffer, ptrdiff_t cdef_buffer_stride,
      uint8_t* deblock_buffer, ptrdiff_t deblock_buffer_stride, Plane plane,
      int plane_height, int plane_width, int x, int y, int row, int unit_row,
      int current_process_unit_height, int process_unit_width, int window_width,
      int plane_unit_size, int num_horizontal_units);
  template <typename Pixel>
  void ApplyLoopRestorationForOneUnit(
      uint8_t* cdef_buffer, ptrdiff_t cdef_buffer_stride,
      uint8_t* deblock_buffer, ptrdiff_t deblock_buffer_stride, Plane plane,
      int plane_height, int unit_id, LoopRestorationType type, int x, int y,
      int row, int column, int current_process_unit_width,
      int current_process_unit_height, int plane_process_unit_width,
      int window_width);
  static int GetIndex(int row4x4) { return DivideBy4(row4x4); }
  static int GetShift(int row4x4, int column4x4) {
    return ((row4x4 & 3) << 4) | column4x4;
  }
  int GetDeblockUnitId(int row_unit, int column_unit) const {
    return row_unit * num_64x64_blocks_per_row_ + column_unit;
  }
  void HorizontalDeblockFilter(Plane plane, int row4x4_start,
                               int column4x4_start, int unit_id);
  void VerticalDeblockFilter(Plane plane, int row4x4_start, int column4x4_start,
                             int unit_id);
  // |unit_id| is not used, keep it to match the same interface as
  // HorizontalDeblockFilter().
  void HorizontalDeblockFilterNoMask(Plane plane, int row4x4_start,
                                     int column4x4_start, int unit_id);
  // |unit_id| is not used, keep it to match the same interface as
  // VerticalDeblockFilter().
  void VerticalDeblockFilterNoMask(Plane plane, int row4x4_start,
                                   int column4x4_start, int unit_id);
  bool GetDeblockFilterEdgeInfo(Plane plane, int row4x4, int column4x4,
                                int8_t subsampling_x, int8_t subsampling_y,
                                LoopFilterType type, uint8_t* level, int* step,
                                int* filter_length) const;
  static dsp::LoopFilterSize GetLoopFilterSize(Plane plane, int step) {
    if (step == 4) {
      return dsp::kLoopFilterSize4;
    }
    if (step == 8) {
      return (plane == kPlaneY) ? dsp::kLoopFilterSize8 : dsp::kLoopFilterSize6;
    }
    return (plane == kPlaneY) ? dsp::kLoopFilterSize14 : dsp::kLoopFilterSize6;
  }
  // HorizontalDeblockFilter and VerticalDeblockFilter must have the correct
  // signature.
  static_assert(std::is_same<decltype(&PostFilter::HorizontalDeblockFilter),
                             DeblockFilter>::value,
                "");
  static_assert(std::is_same<decltype(&PostFilter::VerticalDeblockFilter),
                             DeblockFilter>::value,
                "");
  void InitDeblockFilterParams();  // Part of 7.14.4.
  void GetDeblockFilterParams(uint8_t level, int* outer_thresh,
                              int* inner_thresh, int* hev_thresh) const;
  // Applies super resolution and writes result to input_buffer.
  void FrameSuperRes(YuvBuffer* input_buffer);  // Section 7.16.

  const ObuFrameHeader& frame_header_;
  const LoopRestoration& loop_restoration_;
  const dsp::Dsp& dsp_;
  const int num_64x64_blocks_per_row_;
  const int upscaled_width_;
  const int width_;
  const int height_;
  const int8_t bitdepth_;
  const int8_t subsampling_x_;
  const int8_t subsampling_y_;
  const int8_t planes_;
  const int pixel_size_;
  // This class does not take ownership of the masks/restoration_info, but it
  // could change their values.
  LoopFilterMask* const masks_;
  uint8_t inner_thresh_[kMaxLoopFilterValue + 1] = {};
  uint8_t outer_thresh_[kMaxLoopFilterValue + 1] = {};
  uint8_t hev_thresh_[kMaxLoopFilterValue + 1] = {};
  // This stores the deblocking filter levels assuming that the delta is zero.
  // This will be used by all superblocks whose delta is zero (without having to
  // recompute them). The dimensions (in order) are: segment_id, level_index
  // (based on plane and direction), reference_frame and mode_id.
  uint8_t deblock_filter_levels_[kMaxSegments][kFrameLfCount]
                                [kNumReferenceFrameTypes][2];
  const Array2D<int16_t>& cdef_index_;
  const Array2D<TransformSize>& inter_transform_sizes_;
  // Pointer to the data buffer used for multi-threaded cdef or loop
  // restoration. The size of this buffer must be at least
  // |window_buffer_width_| * |window_buffer_height_| * |pixel_size_|.
  // Or |planes_| times that for multi-threaded cdef.
  // If |thread_pool_| is nullptr, then this buffer is not used and can be
  // nullptr as well.
  uint8_t* const threaded_window_buffer_;
  LoopRestorationInfo* const restoration_info_;
  const int window_buffer_width_;
  const int window_buffer_height_;
  const BlockParametersHolder& block_parameters_;
  // Frame buffer to hold cdef filtered frame.
  YuvBuffer cdef_filtered_buffer_;
  // Frame buffer to hold the copy of the buffer to be upscaled,
  // allocated only when super res is required.
  YuvBuffer super_res_buffer_;
  // Input frame buffer. During ApplyFiltering(), it holds the (upscaled)
  // deblocked frame.
  // When ApplyFiltering() is done, it holds the final output of PostFilter.
  YuvBuffer* const source_buffer_;
  // Frame buffer pointer. It always points to (upscaled) cdef filtered frame.
  // Set in ApplyCdef(). If cdef is not present, in ApplyLoopRestoration(),
  // cdef_buffer_ is the same as source_buffer_.
  YuvBuffer* cdef_buffer_ = nullptr;
  const uint8_t do_post_filter_mask_;

  ThreadPool* const thread_pool_;

  // A small buffer to hold input source image block for loop restoration.
  // Its size is one processing unit size + borders.
  // Self-guided filter needs an extra one-pixel border.
  // Wiener filter needs extended border of three pixels.
  // Therefore the size of the buffer is 70x70 pixels.
  alignas(alignof(uint16_t)) uint8_t
      block_buffer_[kRestorationProcessingUnitSizeWithBorders *
                    kRestorationProcessingUnitSizeWithBorders *
                    sizeof(uint16_t)];
  // A block buffer to hold the input that is converted to uint16_t before
  // cdef filtering. Only used in single threaded case.
  uint16_t cdef_block_[kRestorationProcessingUnitSizeWithBorders *
                       kRestorationProcessingUnitSizeWithBorders * 3];

  template <int bitdepth, typename Pixel>
  friend class PostFilterSuperResTest;

  template <int bitdepth, typename Pixel>
  friend class PostFilterHelperFuncTest;
};

// This function takes the cdef filtered buffer and the deblocked buffer to
// prepare a block as input for loop restoration.
// In striped loop restoration:
// The filtering needs to fetch the area of size (width + 6) x (height + 6),
// in which (width + 6) x height area is from cdef filtered frame
// (cdef_buffer). Top 3 rows and bottom 3 rows are from deblocked frame
// (deblock_buffer).
// Special cases are:
// (1). when it is the top border, the top 3 rows are from cdef
// filtered frame.
// (2). when it is the bottom border, the bottom 3 rows are from cdef
// filtered frame.
// For the top 3 rows and bottom 3 rows, the top_row[0] is a copy of the
// top_row[1]. The bottom_row[2] is a copy of the bottom_row[1]. If cdef is
// not applied for this frame, cdef_buffer is the same as deblock_buffer.
template <typename Pixel>
void PrepareLoopRestorationBlock(const uint8_t* cdef_buffer,
                                 ptrdiff_t cdef_stride,
                                 const uint8_t* deblock_buffer,
                                 ptrdiff_t deblock_stride, uint8_t* dest,
                                 ptrdiff_t dest_stride, const int width,
                                 const int height, const bool frame_top_border,
                                 const bool frame_bottom_border) {
  const auto* cdef_ptr = reinterpret_cast<const Pixel*>(cdef_buffer);
  cdef_stride /= sizeof(Pixel);
  const auto* deblock_ptr = reinterpret_cast<const Pixel*>(deblock_buffer);
  deblock_stride /= sizeof(Pixel);
  auto* dst = reinterpret_cast<Pixel*>(dest);
  dest_stride /= sizeof(Pixel);
  // Top 3 rows.
  cdef_ptr -= (kRestorationBorder - 1) * cdef_stride + kRestorationBorder;
  deblock_ptr -= (kRestorationBorder - 1) * deblock_stride + kRestorationBorder;
  for (int i = 0; i < kRestorationBorder; ++i) {
    if (frame_top_border) {
      memcpy(dst, cdef_ptr, sizeof(Pixel) * (width + 2 * kRestorationBorder));
    } else {
      memcpy(dst, deblock_ptr,
             sizeof(Pixel) * (width + 2 * kRestorationBorder));
    }
    if (i > 0) {
      cdef_ptr += cdef_stride;
      deblock_ptr += deblock_stride;
    }
    dst += dest_stride;
  }
  // Main body.
  for (int i = 0; i < height; ++i) {
    memcpy(dst, cdef_ptr, sizeof(Pixel) * (width + 2 * kRestorationBorder));
    cdef_ptr += cdef_stride;
    dst += dest_stride;
  }
  // Bottom 3 rows.
  deblock_ptr += height * deblock_stride;
  for (int i = 0; i < kRestorationBorder; ++i) {
    if (frame_bottom_border) {
      memcpy(dst, cdef_ptr, sizeof(Pixel) * (width + 2 * kRestorationBorder));
    } else {
      memcpy(dst, deblock_ptr,
             sizeof(Pixel) * (width + 2 * kRestorationBorder));
    }
    if (i < kRestorationBorder - 2) {
      cdef_ptr += cdef_stride;
      deblock_ptr += deblock_stride;
    }
    dst += dest_stride;
  }
}

template <typename Pixel>
void CopyRows(const Pixel* src, const ptrdiff_t src_stride,
              const int block_width, const int unit_width,
              const bool is_frame_top, const bool is_frame_bottom,
              const bool is_frame_left, const bool is_frame_right,
              const bool copy_top, const int num_rows, uint16_t* dst,
              const ptrdiff_t dst_stride) {
  if (is_frame_top || is_frame_bottom) {
    if (is_frame_bottom) dst -= kCdefBorder;
    for (int y = 0; y < num_rows; ++y) {
      Memset(dst, PostFilter::kCdefLargeValue, unit_width + 2 * kCdefBorder);
      dst += dst_stride;
    }
  } else {
    if (copy_top) {
      src -= kCdefBorder * src_stride;
      dst += kCdefBorder;
    }
    for (int y = 0; y < num_rows; ++y) {
      for (int x = -kCdefBorder; x < 0; ++x) {
        dst[x] = is_frame_left ? PostFilter::kCdefLargeValue : src[x];
      }
      for (int x = 0; x < block_width; ++x) {
        dst[x] = src[x];
      }
      for (int x = block_width; x < unit_width + kCdefBorder; ++x) {
        dst[x] = is_frame_right ? PostFilter::kCdefLargeValue : src[x];
      }
      dst += dst_stride;
      src += src_stride;
    }
  }
}

// This function prepares the input source block for cdef filtering.
// The input source block contains a 12x12 block, with the inner 8x8 as the
// desired filter region.
// It pads the block if the 12x12 block includes out of frame pixels with
// a large value.
// This achieves the required behavior defined in section 5.11.52 of the spec.
template <typename Pixel>
void PrepareCdefBlock(const YuvBuffer* const source_buffer, const int planes,
                      const int subsampling_x, const int subsampling_y,
                      const int frame_width, const int frame_height,
                      const int block_width4x4, const int block_height4x4,
                      const int row_64x64, const int column_64x64,
                      uint16_t* cdef_source, const ptrdiff_t cdef_stride) {
  for (int plane = kPlaneY; plane < planes; ++plane) {
    uint16_t* cdef_src =
        cdef_source + plane * kRestorationProcessingUnitSizeWithBorders *
                          kRestorationProcessingUnitSizeWithBorders;
    const int plane_subsampling_x = (plane == kPlaneY) ? 0 : subsampling_x;
    const int plane_subsampling_y = (plane == kPlaneY) ? 0 : subsampling_y;
    const int start_x = MultiplyBy4(column_64x64) >> plane_subsampling_x;
    const int start_y = MultiplyBy4(row_64x64) >> plane_subsampling_y;
    const int plane_width =
        RightShiftWithRounding(frame_width, plane_subsampling_x);
    const int plane_height =
        RightShiftWithRounding(frame_height, plane_subsampling_y);
    const int block_width = MultiplyBy4(block_width4x4) >> plane_subsampling_x;
    const int block_height =
        MultiplyBy4(block_height4x4) >> plane_subsampling_y;
    // unit_width, unit_height are the same as block_width, block_height unless
    // it reaches the frame boundary, where block_width < 64 or
    // block_height < 64. unit_width, unit_height guarantee we build blocks on
    // a multiple of 8.
    const int unit_width =
        Align(block_width, (plane_subsampling_x > 0) ? 4 : 8);
    const int unit_height =
        Align(block_height, (plane_subsampling_y > 0) ? 4 : 8);
    const bool is_frame_left = column_64x64 == 0;
    const bool is_frame_right = start_x + block_width >= plane_width;
    const bool is_frame_top = row_64x64 == 0;
    const bool is_frame_bottom = start_y + block_height >= plane_height;
    const int src_stride = source_buffer->stride(plane) / sizeof(Pixel);
    const Pixel* src_buffer =
        reinterpret_cast<const Pixel*>(source_buffer->data(plane)) +
        start_y * src_stride + start_x;
    // Copy to the top 2 rows.
    CopyRows(src_buffer, src_stride, block_width, unit_width, is_frame_top,
             false, is_frame_left, is_frame_right, true, kCdefBorder, cdef_src,
             cdef_stride);
    cdef_src += kCdefBorder * cdef_stride + kCdefBorder;

    // Copy the body.
    CopyRows(src_buffer, src_stride, block_width, unit_width, false, false,
             is_frame_left, is_frame_right, false, block_height, cdef_src,
             cdef_stride);
    src_buffer += block_height * src_stride;
    cdef_src += block_height * cdef_stride;

    // Copy to bottom rows.
    CopyRows(src_buffer, src_stride, block_width, unit_width, false,
             is_frame_bottom, is_frame_left, is_frame_right, false,
             kCdefBorder + unit_height - block_height, cdef_src, cdef_stride);
  }
}

}  // namespace libgav1

#endif  // LIBGAV1_SRC_POST_FILTER_H_
