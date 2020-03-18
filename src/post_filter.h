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
  // This class does not take ownership of the masks/restoration_info, but it
  // may change their values.
  //
  // The overall flow of data in this class (for both single and multi-threaded
  // cases) is as follows:
  //   -> Input: |frame_buffer_|.
  //   -> Initialize |source_buffer_|, |cdef_buffer_| and
  //      |loop_restoration_buffer_|.
  //   -> Deblocking:
  //      * Input: |source_buffer_|
  //      * Output: |source_buffer_|
  //   -> CDEF:
  //      * Input: |source_buffer_|
  //      * Output: |cdef_buffer_|
  //   -> SuperRes:
  //      * Input: |cdef_buffer_|
  //      * Output: |cdef_buffer_|
  //   -> Loop Restoration:
  //      * Input: |cdef_buffer_|
  //      * Output: |loop_restoration_buffer_|.
  //   -> Now |frame_buffer_| contains the filtered frame.
  PostFilter(const ObuFrameHeader& frame_header,
             const ObuSequenceHeader& sequence_header, LoopFilterMask* masks,
             const Array2D<int16_t>& cdef_index,
             const Array2D<TransformSize>& inter_transform_sizes,
             LoopRestorationInfo* restoration_info,
             BlockParametersHolder* block_parameters, YuvBuffer* frame_buffer,
             YuvBuffer* deblock_buffer, const dsp::Dsp* dsp,
             ThreadPool* thread_pool, uint8_t* threaded_window_buffer,
             uint8_t* superres_line_buffer, int do_post_filter_mask);

  // non copyable/movable.
  PostFilter(const PostFilter&) = delete;
  PostFilter& operator=(const PostFilter&) = delete;
  PostFilter(PostFilter&&) = delete;
  PostFilter& operator=(PostFilter&&) = delete;

  // The overall function that applies all post processing filtering with
  // multiple threads.
  // * The filtering order is:
  //   deblock --> CDEF --> super resolution--> loop restoration.
  // * The output of each filter is the input for the following filter. A
  //   special case is that loop restoration needs a few rows of the deblocked
  //   frame and the entire cdef filtered frame:
  //   deblock --> CDEF --> super resolution --> loop restoration.
  //              |                                 ^
  //              |                                 |
  //              -----------> super resolution -----
  // * Any of these filters could be present or absent.
  // * |frame_buffer_| points to the decoded frame buffer. When
  //   ApplyFilteringThreaded() is called, |frame_buffer_| is modified by each
  //   of the filters as described below.
  // Filter behavior (multi-threaded):
  // * Deblock: In-place filtering. The output is written to |source_buffer_|.
  //            If cdef and loop restoration are both on, then 4 rows (as
  //            specified by |kDeblockedRowsForLoopRestoration|) in every 64x64
  //            block is copied into |deblock_buffer_|.
  // * Cdef: Filtering output is written into |threaded_window_buffer_| and then
  //         copied into the |cdef_buffer_| (which is just |source_buffer_| with
  //         a shift to the top-left).
  // * SuperRes: Near in-place filtering (with an additional line buffer for
  //             each row). The output is written to |cdef_buffer_|.
  // * Restoration: Uses the |cdef_buffer_| and |deblock_buffer_| as the input
  //                and the output is written into the
  //                |threaded_window_buffer_|. It is then copied to the
  //                |loop_restoration_buffer_| (which is just |cdef_buffer_|
  //                with a shift to the top-left).
  void ApplyFilteringThreaded();

  // Does the overall post processing filter for one superblock row (starting at
  // |row4x4| with height 4*|sb4x4|. Cdef, SuperRes and Loop Restoration lag by
  // one superblock row to account for deblocking.
  //
  // Filter behavior (single-threaded):
  // * Deblock: In-place filtering. The output is written to |source_buffer_|.
  //            If cdef and loop restoration are both on, then 4 rows (as
  //            specified by |kDeblockedRowsForLoopRestoration|) in every 64x64
  //            block is copied into |deblock_buffer_|.
  // * Cdef: In-place filtering. The output is written into |cdef_buffer_|
  //         (which is just |source_buffer_| with a shift to the top-left).
  // * SuperRes: Near in-place filtering (with an additional line buffer for
  //             each row). The output is written to |cdef_buffer_|.
  // * Restoration: Near in-place filtering. Uses a local block of size 64x64.
  //                Uses the |cdef_buffer_| and |deblock_buffer_| as the input
  //                and the output is written into |loop_restoration_buffer_|
  //                (which is just |source_buffer_| with a shift to the
  //                top-left).
  // Returns the index of the last row whose post processing is complete and can
  // be used for referencing.
  int ApplyFilteringForOneSuperBlockRow(int row4x4, int sb4x4,
                                        bool is_last_row);

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
  bool DoRestoration() const {
    return DoRestoration(loop_restoration_, do_post_filter_mask_, planes_);
  }
  // Returns true if loop restoration will be performed for the given parameters
  // and mask.
  static bool DoRestoration(const LoopRestoration& loop_restoration,
                            uint8_t do_post_filter_mask, int num_planes) {
    if ((do_post_filter_mask & 0x08) == 0) return false;
    if (num_planes == kMaxPlanesMonochrome) {
      return loop_restoration.type[kPlaneY] != kLoopRestorationTypeNone;
    }
    return loop_restoration.type[kPlaneY] != kLoopRestorationTypeNone ||
           loop_restoration.type[kPlaneU] != kLoopRestorationTypeNone ||
           loop_restoration.type[kPlaneV] != kLoopRestorationTypeNone;
  }

  // Returns a pointer to the unfiltered buffer. This is used by the Tile class
  // to determine where to write the output of the tile decoding process taking
  // in-place filtering offsets into consideration.
  uint8_t* GetUnfilteredBuffer(int plane) { return source_buffer_[plane]; }
  const YuvBuffer& frame_buffer() const { return frame_buffer_; }

  // Returns true if SuperRes will be performed for the given frame header and
  // mask.
  static bool DoSuperRes(const ObuFrameHeader& frame_header,
                         uint8_t do_post_filter_mask) {
    return (do_post_filter_mask & 0x04) != 0 &&
           frame_header.width != frame_header.upscaled_width;
  }
  bool DoSuperRes() const {
    return DoSuperRes(frame_header_, do_post_filter_mask_);
  }
  LoopFilterMask* masks() const { return masks_; }
  LoopRestorationInfo* restoration_info() const { return restoration_info_; }
  uint8_t* GetBufferOffset(uint8_t* base_buffer, int stride, Plane plane,
                           int row4x4, int column4x4) const {
    return base_buffer +
           RowOrColumn4x4ToPixel(row4x4, plane, subsampling_y_[plane]) *
               stride +
           RowOrColumn4x4ToPixel(column4x4, plane, subsampling_x_[plane]) *
               pixel_size_;
  }
  uint8_t* GetSourceBuffer(Plane plane, int row4x4, int column4x4) const {
    return GetBufferOffset(source_buffer_[plane], frame_buffer_.stride(plane),
                           plane, row4x4, column4x4);
  }

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

  // Functions common to all post filters.

  // Extends the frame by setting the border pixel values to the one from its
  // closest frame boundary.
  void ExtendFrameBoundary(uint8_t* frame_start, int width, int height,
                           ptrdiff_t stride, int left, int right, int top,
                           int bottom);
  // Extend frame boundary for referencing if the frame will be saved as a
  // reference frame.
  void ExtendBordersForReferenceFrame();
  // Copies the deblocked pixels needed for loop restoration.
  void CopyDeblockedPixels(Plane plane, int row4x4);
  // Copies the border for one superblock row. If |for_loop_restoration| is
  // true, then it assumes that the border extension is being performed for the
  // input of the loop restoration process. If |for_loop_restoration| is false,
  // then it assumes that the border extension is being performed for using the
  // current frame as a reference frame. In this case, |progress_row_| is also
  // updated.
  void CopyBordersForOneSuperBlockRow(int row4x4, int sb4x4,
                                      bool for_loop_restoration);
  // Sets up the |deblock_buffer_| for loop restoration.
  void SetupDeblockBuffer(int row4x4_start, int sb4x4);
  // Returns true if we can perform border extension in loop (i.e.) without
  // waiting until the entire frame is decoded. If intra_block_copy is true, we
  // do in-loop border extension only if the upscaled_width is the same as 4 *
  // columns4x4. Otherwise, we cannot do in loop border extension since those
  // pixels may be used by intra block copy.
  bool DoBorderExtensionInLoop() const {
    return !frame_header_.allow_intrabc ||
           frame_header_.upscaled_width ==
               MultiplyBy4(frame_header_.columns4x4);
  }
  template <typename Pixel>
  void CopyPlane(const uint8_t* source, int source_stride, int width,
                 int height, uint8_t* dest, int dest_stride) {
    auto* dst = reinterpret_cast<Pixel*>(dest);
    const auto* src = reinterpret_cast<const Pixel*>(source);
    source_stride /= sizeof(Pixel);
    dest_stride /= sizeof(Pixel);
    for (int y = 0; y < height; ++y) {
      memcpy(dst, src, width * sizeof(Pixel));
      src += source_stride;
      dst += dest_stride;
    }
  }

  // Functions for the Deblocking filter.

  static int GetIndex(int row4x4) { return DivideBy4(row4x4); }
  static int GetShift(int row4x4, int column4x4) {
    return ((row4x4 & 3) << 4) | column4x4;
  }
  int GetDeblockUnitId(int row_unit, int column_unit) const {
    return row_unit * num_64x64_blocks_per_row_ + column_unit;
  }
  static dsp::LoopFilterSize GetLoopFilterSize(Plane plane, int step) {
    if (step == 4) {
      return dsp::kLoopFilterSize4;
    }
    if (step == 8) {
      return (plane == kPlaneY) ? dsp::kLoopFilterSize8 : dsp::kLoopFilterSize6;
    }
    return (plane == kPlaneY) ? dsp::kLoopFilterSize14 : dsp::kLoopFilterSize6;
  }
  void InitDeblockFilterParams();  // Part of 7.14.4.
  void GetDeblockFilterParams(uint8_t level, int* outer_thresh,
                              int* inner_thresh, int* hev_thresh) const;
  template <LoopFilterType type>
  bool GetDeblockFilterEdgeInfo(Plane plane, int row4x4, int column4x4,
                                int8_t subsampling_x, int8_t subsampling_y,
                                uint8_t* level, int* step,
                                int* filter_length) const;
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
  // HorizontalDeblockFilter and VerticalDeblockFilter must have the correct
  // signature.
  static_assert(std::is_same<decltype(&PostFilter::HorizontalDeblockFilter),
                             DeblockFilter>::value,
                "");
  static_assert(std::is_same<decltype(&PostFilter::VerticalDeblockFilter),
                             DeblockFilter>::value,
                "");
  // Applies deblock filtering for the superblock row starting at |row4x4| with
  // a height of 4*|sb4x4|.
  void ApplyDeblockFilterForOneSuperBlockRow(int row4x4, int sb4x4);
  void DeblockFilterWorker(int jobs_per_plane, const Plane* planes,
                           int num_planes, std::atomic<int>* job_counter,
                           DeblockFilter deblock_filter);
  void ApplyDeblockFilterThreaded();

  // Functions for the cdef filter.

  uint8_t* GetCdefBufferAndStride(int start_x, int start_y, int plane,
                                  int subsampling_x, int subsampling_y,
                                  int window_buffer_plane_size,
                                  int* cdef_stride) const;
  // This function prepares the input source block for cdef filtering. The input
  // source block contains a 12x12 block, with the inner 8x8 as the desired
  // filter region. It pads the block if the 12x12 block includes out of frame
  // pixels with a large value. This achieves the required behavior defined in
  // section 5.11.52 of the spec.
  template <typename Pixel>
  void PrepareCdefBlock(int block_width4x4, int block_height4x4, int row_64x64,
                        int column_64x64, uint16_t* cdef_source,
                        ptrdiff_t cdef_stride);
  template <typename Pixel>
  void ApplyCdefForOneUnit(uint16_t* cdef_block, int index, int block_width4x4,
                           int block_height4x4, int row4x4_start,
                           int column4x4_start);
  // Helper function used by ApplyCdefForOneSuperBlockRow to avoid some code
  // duplication.
  void ApplyCdefForOneSuperBlockRowHelper(int row4x4, int block_height4x4);
  // Applies cdef filtering for the superblock row starting at |row4x4| with a
  // height of 4*|sb4x4|.
  void ApplyCdefForOneSuperBlockRow(int row4x4, int sb4x4, bool is_last_row);
  template <typename Pixel>
  void ApplyCdefForOneRowInWindow(int row, int column);
  template <typename Pixel>
  void ApplyCdefThreaded();
  void ApplyCdef();  // Sections 7.15 and 7.15.1.

  // Functions for the SuperRes filter.

  // Applies super resolution for the |buffers| for |rows[plane]| rows of each
  // plane. If |in_place| is true, the line buffer will not be used and the
  // SuperRes output will be written to a row above the input row. If |in_place|
  // is false, the line buffer will be used to store a copy of the input and the
  // output will be written to the same row as the input row.
  template <bool in_place>
  void ApplySuperRes(const std::array<uint8_t*, kMaxPlanes>& buffers,
                     const std::array<int, kMaxPlanes>& strides,
                     const std::array<int, kMaxPlanes>& rows,
                     size_t line_buffer_offset);  // Section 7.16.
  // Applies SuperRes for the superblock row starting at |row4x4| with a height
  // of 4*|sb4x4|.
  void ApplySuperResForOneSuperBlockRow(int row4x4, int sb4x4,
                                        bool is_last_row);
  void ApplySuperResThreaded();

  // Functions for the Loop Restoration filter.

  template <typename Pixel>
  void ApplyLoopRestorationForOneUnit(
      uint8_t* cdef_buffer, ptrdiff_t cdef_buffer_stride, Plane plane,
      int plane_height, int x, int y, int row, int column, int unit_row,
      int current_process_unit_height, int plane_process_unit_width,
      int plane_unit_size, int num_horizontal_units, int plane_width,
      Array2DView<Pixel>* loop_restored_window);
  template <typename Pixel>
  void ApplyLoopRestorationForSuperBlock(Plane plane, int x, int y,
                                         int unit_row,
                                         int current_process_unit_height,
                                         int process_unit_width);
  // Applies loop restoration for the superblock row starting at |row4x4_start|
  // with a height of 4*|sb4x4|.
  void ApplyLoopRestorationForOneSuperBlockRow(int row4x4_start, int sb4x4);
  template <typename Pixel>
  void ApplyLoopRestorationThreaded();
  template <typename Pixel>
  void ApplyLoopRestorationForOneRowInWindow(
      uint8_t* cdef_buffer, ptrdiff_t cdef_buffer_stride, Plane plane,
      int plane_height, int plane_width, int x, int y, int row, int unit_row,
      int current_process_unit_height, int process_unit_width, int window_width,
      int plane_unit_size, int num_horizontal_units);
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
  void ApplyLoopRestoration();

  const ObuFrameHeader& frame_header_;
  const LoopRestoration& loop_restoration_;
  const dsp::Dsp& dsp_;
  const int num_64x64_blocks_per_row_;
  const int upscaled_width_;
  const int width_;
  const int height_;
  const int8_t bitdepth_;
  const int8_t subsampling_x_[kMaxPlanes];
  const int8_t subsampling_y_[kMaxPlanes];
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
  // Stores the SuperRes info for the frame.
  struct {
    int upscaled_width;
    int initial_subpixel_x;
    int step;
  } super_res_info_[kMaxPlanes];
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
  // Pointer to the line buffer used by ApplySuperRes(). If SuperRes is on, then
  // the buffer will be large enough to hold one downscaled row +
  // kSuperResHorizontalBorder.
  uint8_t* const superres_line_buffer_;
  const BlockParametersHolder& block_parameters_;
  // Frame buffer to hold cdef filtered frame.
  YuvBuffer cdef_filtered_buffer_;
  // Input frame buffer.
  YuvBuffer& frame_buffer_;
  // A view into |frame_buffer_| that points to the input and output of the
  // deblocking process.
  uint8_t* source_buffer_[kMaxPlanes];
  // A view into |frame_buffer_| that points to the output of the CDEF filtered
  // planes (to facilitate in-place CDEF filtering).
  uint8_t* cdef_buffer_[kMaxPlanes];
  // A view into |frame_buffer_| that points to the planes after the SuperRes
  // filter is applied (to facilitate in-place SuperRes).
  uint8_t* superres_buffer_[kMaxPlanes];
  // A view into |frame_buffer_| that points to the output of the Loop Restored
  // planes (to facilitate in-place Loop Restoration).
  uint8_t* loop_restoration_buffer_[kMaxPlanes];
  // Buffer used to store the deblocked pixels that are necessary for loop
  // restoration. This buffer will store 4 rows for every 64x64 block (4 rows
  // for every 32x32 for chroma with subsampling). The indices of the rows that
  // are stored are specified in |kDeblockedRowsForLoopRestoration|. First 4
  // rows of this buffer are never populated and never used.
  // This buffer is used only when both Cdef and Loop Restoration are on.
  YuvBuffer& deblock_buffer_;
  const uint8_t do_post_filter_mask_;

  ThreadPool* const thread_pool_;
  // Tracks the progress of the post filters.
  int progress_row_ = -1;

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
void PrepareLoopRestorationBlock(const bool do_cdef, const uint8_t* cdef_buffer,
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
  if (deblock_ptr != nullptr) deblock_ptr -= kRestorationBorder;
  for (int i = 0; i < kRestorationBorder; ++i) {
    if (frame_top_border || !do_cdef) {
      memcpy(dst, cdef_ptr, sizeof(Pixel) * (width + 2 * kRestorationBorder));
    } else {
      memcpy(dst, deblock_ptr,
             sizeof(Pixel) * (width + 2 * kRestorationBorder));
    }
    if (i > 0) {
      if (deblock_ptr != nullptr) deblock_ptr += deblock_stride;
      cdef_ptr += cdef_stride;
    }
    dst += dest_stride;
  }
  // Main body.
  for (int i = 0; i < height; ++i) {
    memcpy(dst, cdef_ptr, sizeof(Pixel) * (width + 2 * kRestorationBorder));
    cdef_ptr += cdef_stride;
    dst += dest_stride;
  }
  // Bottom 3 rows. If |frame_top_border| is true, then we are in the first
  // superblock row, so in that case, do not increment |deblock_ptr| since we
  // don't store anything from the first superblock row into |deblock_buffer|.
  if (deblock_ptr != nullptr && !frame_top_border) {
    deblock_ptr += deblock_stride * 4;
  }
  for (int i = 0; i < kRestorationBorder; ++i) {
    if (frame_bottom_border || !do_cdef) {
      memcpy(dst, cdef_ptr, sizeof(Pixel) * (width + 2 * kRestorationBorder));
    } else {
      memcpy(dst, deblock_ptr,
             sizeof(Pixel) * (width + 2 * kRestorationBorder));
    }
    if (i < kRestorationBorder - 2) {
      if (deblock_ptr != nullptr) deblock_ptr += deblock_stride;
      cdef_ptr += cdef_stride;
    }
    dst += dest_stride;
  }
}

}  // namespace libgav1

#endif  // LIBGAV1_SRC_POST_FILTER_H_
