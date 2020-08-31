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
#include <atomic>

#include "src/post_filter.h"
#include "src/utils/blocking_counter.h"

namespace libgav1 {

void PostFilter::ComputeDeblockFilterLevels(
    const int8_t delta_lf[kFrameLfCount],
    uint8_t deblock_filter_levels[kMaxSegments][kFrameLfCount]
                                 [kNumReferenceFrameTypes][2]) const {
  if (!DoDeblock()) return;
  for (int segment_id = 0;
       segment_id < (frame_header_.segmentation.enabled ? kMaxSegments : 1);
       ++segment_id) {
    int level_index = 0;
    for (; level_index < 2; ++level_index) {
      LoopFilterMask::ComputeDeblockFilterLevels(
          frame_header_, segment_id, level_index, delta_lf,
          deblock_filter_levels[segment_id][level_index]);
    }
    for (; level_index < kFrameLfCount; ++level_index) {
      if (frame_header_.loop_filter.level[level_index] != 0) {
        LoopFilterMask::ComputeDeblockFilterLevels(
            frame_header_, segment_id, level_index, delta_lf,
            deblock_filter_levels[segment_id][level_index]);
      }
    }
  }
}

void PostFilter::InitDeblockFilterParams() {
  const int8_t sharpness = frame_header_.loop_filter.sharpness;
  assert(0 <= sharpness && sharpness < 8);
  const int shift = DivideBy4(sharpness + 3);  // ceil(sharpness / 4.0)
  for (int level = 0; level <= kMaxLoopFilterValue; ++level) {
    uint8_t limit = level >> shift;
    if (sharpness > 0) {
      limit = Clip3(limit, 1, 9 - sharpness);
    } else {
      limit = std::max(limit, static_cast<uint8_t>(1));
    }
    inner_thresh_[level] = limit;
    outer_thresh_[level] = 2 * (level + 2) + limit;
    hev_thresh_[level] = level >> 4;
  }
}

void PostFilter::GetDeblockFilterParams(uint8_t level, int* outer_thresh,
                                        int* inner_thresh,
                                        int* hev_thresh) const {
  *outer_thresh = outer_thresh_[level];
  *inner_thresh = inner_thresh_[level];
  *hev_thresh = hev_thresh_[level];
}

template <LoopFilterType type>
bool PostFilter::GetDeblockFilterEdgeInfo(const Plane plane, int row4x4,
                                          int column4x4,
                                          const int8_t subsampling_x,
                                          const int8_t subsampling_y,
                                          uint8_t* level, int* step,
                                          int* filter_length) const {
  row4x4 = GetDeblockPosition(row4x4, subsampling_y);
  column4x4 = GetDeblockPosition(column4x4, subsampling_x);
  const BlockParameters* bp = block_parameters_.Find(row4x4, column4x4);
  const TransformSize transform_size =
      (plane == kPlaneY) ? inter_transform_sizes_[row4x4][column4x4]
                         : bp->uv_transform_size;
  *step = (type == kLoopFilterTypeHorizontal) ? kTransformHeight[transform_size]
                                              : kTransformWidth[transform_size];
  if ((type == kLoopFilterTypeHorizontal && row4x4 == subsampling_y) ||
      (type == kLoopFilterTypeVertical && column4x4 == subsampling_x)) {
    return false;
  }

  const int filter_id = kDeblockFilterLevelIndex[plane][type];
  const uint8_t level_this = bp->deblock_filter_level[filter_id];
  const int row4x4_prev = (type == kLoopFilterTypeHorizontal)
                              ? row4x4 - (1 << subsampling_y)
                              : row4x4;
  const int column4x4_prev = (type == kLoopFilterTypeHorizontal)
                                 ? column4x4
                                 : column4x4 - (1 << subsampling_x);
  assert(row4x4_prev >= 0 && column4x4_prev >= 0);
  const BlockParameters* bp_prev =
      block_parameters_.Find(row4x4_prev, column4x4_prev);
  const uint8_t level_prev = bp_prev->deblock_filter_level[filter_id];
  *level = level_this;
  if (level_this == 0) {
    if (level_prev == 0) return false;
    *level = level_prev;
  }

  const BlockSize size =
      kPlaneResidualSize[bp->size][subsampling_x][subsampling_y];
  const int prediction_masks = (type == kLoopFilterTypeHorizontal)
                                   ? kBlockHeightPixels[size] - 1
                                   : kBlockWidthPixels[size] - 1;
  const int pixel_position = MultiplyBy4((type == kLoopFilterTypeHorizontal)
                                             ? row4x4 >> subsampling_y
                                             : column4x4 >> subsampling_x);
  const bool is_border = (pixel_position & prediction_masks) == 0;
  const bool skip = bp->skip && bp->is_inter;
  const bool skip_prev = bp_prev->skip && bp_prev->is_inter;
  if (!skip || !skip_prev || is_border) {
    const TransformSize transform_size_prev =
        (plane == kPlaneY) ? inter_transform_sizes_[row4x4_prev][column4x4_prev]
                           : bp_prev->uv_transform_size;
    const int step_prev = (type == kLoopFilterTypeHorizontal)
                              ? kTransformHeight[transform_size_prev]
                              : kTransformWidth[transform_size_prev];
    *filter_length = std::min(*step, step_prev);
    return true;
  }
  return false;
}

void PostFilter::HorizontalDeblockFilter(Plane plane, int row4x4_start,
                                         int column4x4_start, int unit_id) {
  const int8_t subsampling_x = subsampling_x_[plane];
  const int8_t subsampling_y = subsampling_y_[plane];
  const int row_step = 1 << subsampling_y;
  const int column_step = 1 << subsampling_x;
  const size_t src_step = 4 * pixel_size_;
  const ptrdiff_t row_stride = MultiplyBy4(frame_buffer_.stride(plane));
  const ptrdiff_t src_stride = frame_buffer_.stride(plane);
  uint8_t* src = GetSourceBuffer(plane, row4x4_start, column4x4_start);
  const uint64_t single_row_mask = 0xffff;
  // 3 (11), 5 (0101).
  const uint64_t two_block_mask = (subsampling_x > 0) ? 5 : 3;
  const LoopFilterType type = kLoopFilterTypeHorizontal;
  // Subsampled UV samples correspond to the right/bottom position of
  // Y samples.
  const int column = subsampling_x;

  // AV1 smallest transform size is 4x4, thus minimum horizontal edge size is
  // 4x4. For SIMD implementation, sse2 could compute 8 pixels at the same time.
  // __m128i = 8 x uint16_t, AVX2 could compute 16 pixels at the same time.
  // __m256i = 16 x uint16_t, assuming pixel type is 16 bit. It means we could
  // filter 2 horizontal edges using sse2 and 4 edges using AVX2.
  // The bitmask enables us to call different SIMD implementations to filter
  // 1 edge, or 2 edges or 4 edges.
  // TODO(chengchen): Here, the implementation only consider 1 and 2 edges.
  // Add support for 4 edges. More branches involved, for example, if input is
  // 8 bit, __m128i = 16 x 8 bit, we could apply filtering for 4 edges using
  // sse2, 8 edges using AVX2. If input is 16 bit, __m128 = 8 x 16 bit, then
  // we apply filtering for 2 edges using sse2, and 4 edges using AVX2.
  for (int row4x4 = 0; MultiplyBy4(row4x4_start + row4x4) < height_ &&
                       row4x4 < kNum4x4InLoopFilterMaskUnit;
       row4x4 += row_step) {
    if (row4x4_start + row4x4 == 0) {
      src += row_stride;
      continue;
    }
    // Subsampled UV samples correspond to the right/bottom position of
    // Y samples.
    const int row = GetDeblockPosition(row4x4, subsampling_y);
    const int index = GetIndex(row);
    const int shift = GetShift(row, column);
    const int level_offset = LoopFilterMask::GetLevelOffset(row, column);
    // Mask of current row. mask4x4 represents the vertical filter length for
    // the current horizontal edge is 4, and we needs to apply 3-tap filtering.
    // Similarly, mask8x8 and mask16x16 represent filter lengths are 8 and 16.
    uint64_t mask4x4 =
        (masks_->GetTop(unit_id, plane, kLoopFilterTransformSizeId4x4, index) >>
         shift) &
        single_row_mask;
    uint64_t mask8x8 =
        (masks_->GetTop(unit_id, plane, kLoopFilterTransformSizeId8x8, index) >>
         shift) &
        single_row_mask;
    uint64_t mask16x16 =
        (masks_->GetTop(unit_id, plane, kLoopFilterTransformSizeId16x16,
                        index) >>
         shift) &
        single_row_mask;
    // mask4x4, mask8x8, mask16x16 are mutually exclusive.
    assert((mask4x4 & mask8x8) == 0 && (mask4x4 & mask16x16) == 0 &&
           (mask8x8 & mask16x16) == 0);
    // Apply deblock filter for one row.
    uint8_t* src_row = src;
    int column_offset = 0;
    for (uint64_t mask = mask4x4 | mask8x8 | mask16x16; mask != 0;) {
      int edge_count = 1;
      if ((mask & 1) != 0) {
        // Filter parameters of current edge.
        const uint8_t level = masks_->GetLevel(unit_id, plane, type,
                                               level_offset + column_offset);
        int outer_thresh_0;
        int inner_thresh_0;
        int hev_thresh_0;
        GetDeblockFilterParams(level, &outer_thresh_0, &inner_thresh_0,
                               &hev_thresh_0);
        // Filter parameters of next edge. Clip the index to avoid over
        // reading at the edge of the block. The values will be unused in that
        // case.
        const int level_next_index = level_offset + column_offset + column_step;
        const uint8_t level_next =
            masks_->GetLevel(unit_id, plane, type, level_next_index & 0xff);
        int outer_thresh_1;
        int inner_thresh_1;
        int hev_thresh_1;
        GetDeblockFilterParams(level_next, &outer_thresh_1, &inner_thresh_1,
                               &hev_thresh_1);

        if ((mask16x16 & 1) != 0) {
          const dsp::LoopFilterSize size = (plane == kPlaneY)
                                               ? dsp::kLoopFilterSize14
                                               : dsp::kLoopFilterSize6;
          const dsp::LoopFilterFunc filter_func = dsp_.loop_filters[size][type];
          if ((mask16x16 & two_block_mask) == two_block_mask) {
            edge_count = 2;
            // Apply filtering for two edges.
            filter_func(src_row, src_stride, outer_thresh_0, inner_thresh_0,
                        hev_thresh_0);
            filter_func(src_row + src_step, src_stride, outer_thresh_1,
                        inner_thresh_1, hev_thresh_1);
          } else {
            // Apply single edge filtering.
            filter_func(src_row, src_stride, outer_thresh_0, inner_thresh_0,
                        hev_thresh_0);
          }
        }

        if ((mask8x8 & 1) != 0) {
          const dsp::LoopFilterSize size =
              plane == kPlaneY ? dsp::kLoopFilterSize8 : dsp::kLoopFilterSize6;
          const dsp::LoopFilterFunc filter_func = dsp_.loop_filters[size][type];
          if ((mask8x8 & two_block_mask) == two_block_mask) {
            edge_count = 2;
            // Apply filtering for two edges.
            filter_func(src_row, src_stride, outer_thresh_0, inner_thresh_0,
                        hev_thresh_0);
            filter_func(src_row + src_step, src_stride, outer_thresh_1,
                        inner_thresh_1, hev_thresh_1);
          } else {
            // Apply single edge filtering.
            filter_func(src_row, src_stride, outer_thresh_0, inner_thresh_0,
                        hev_thresh_0);
          }
        }

        if ((mask4x4 & 1) != 0) {
          const dsp::LoopFilterSize size = dsp::kLoopFilterSize4;
          const dsp::LoopFilterFunc filter_func = dsp_.loop_filters[size][type];
          if ((mask4x4 & two_block_mask) == two_block_mask) {
            edge_count = 2;
            // Apply filtering for two edges.
            filter_func(src_row, src_stride, outer_thresh_0, inner_thresh_0,
                        hev_thresh_0);
            filter_func(src_row + src_step, src_stride, outer_thresh_1,
                        inner_thresh_1, hev_thresh_1);
          } else {
            // Apply single edge filtering.
            filter_func(src_row, src_stride, outer_thresh_0, inner_thresh_0,
                        hev_thresh_0);
          }
        }
      }

      const int step = edge_count * column_step;
      mask4x4 >>= step;
      mask8x8 >>= step;
      mask16x16 >>= step;
      mask >>= step;
      column_offset += step;
      src_row += MultiplyBy4(edge_count) * pixel_size_;
    }
    src += row_stride;
  }
}

void PostFilter::VerticalDeblockFilter(Plane plane, int row4x4_start,
                                       int column4x4_start, int unit_id) {
  const int8_t subsampling_x = subsampling_x_[plane];
  const int8_t subsampling_y = subsampling_y_[plane];
  const int row_step = 1 << subsampling_y;
  const int two_row_step = row_step << 1;
  const int column_step = 1 << subsampling_x;
  const size_t src_step = (bitdepth_ == 8) ? 4 : 4 * sizeof(uint16_t);
  const ptrdiff_t row_stride = MultiplyBy4(frame_buffer_.stride(plane));
  const ptrdiff_t two_row_stride = row_stride << 1;
  const ptrdiff_t src_stride = frame_buffer_.stride(plane);
  uint8_t* src = GetSourceBuffer(plane, row4x4_start, column4x4_start);
  const uint64_t single_row_mask = 0xffff;
  const LoopFilterType type = kLoopFilterTypeVertical;
  // Subsampled UV samples correspond to the right/bottom position of
  // Y samples.
  const int column = subsampling_x;

  // AV1 smallest transform size is 4x4, thus minimum vertical edge size is 4x4.
  // For SIMD implementation, sse2 could compute 8 pixels at the same time.
  // __m128i = 8 x uint16_t, AVX2 could compute 16 pixels at the same time.
  // __m256i = 16 x uint16_t, assuming pixel type is 16 bit. It means we could
  // filter 2 vertical edges using sse2 and 4 edges using AVX2.
  // The bitmask enables us to call different SIMD implementations to filter
  // 1 edge, or 2 edges or 4 edges.
  // TODO(chengchen): Here, the implementation only consider 1 and 2 edges.
  // Add support for 4 edges. More branches involved, for example, if input is
  // 8 bit, __m128i = 16 x 8 bit, we could apply filtering for 4 edges using
  // sse2, 8 edges using AVX2. If input is 16 bit, __m128 = 8 x 16 bit, then
  // we apply filtering for 2 edges using sse2, and 4 edges using AVX2.
  for (int row4x4 = 0; MultiplyBy4(row4x4_start + row4x4) < height_ &&
                       row4x4 < kNum4x4InLoopFilterMaskUnit;
       row4x4 += two_row_step) {
    // Subsampled UV samples correspond to the right/bottom position of
    // Y samples.
    const int row = GetDeblockPosition(row4x4, subsampling_y);
    const int row_next = row + row_step;
    const int index = GetIndex(row);
    const int shift = GetShift(row, column);
    const int level_offset = LoopFilterMask::GetLevelOffset(row, column);
    const int index_next = GetIndex(row_next);
    const int shift_next_row = GetShift(row_next, column);
    const int level_offset_next_row =
        LoopFilterMask::GetLevelOffset(row_next, column);
    // TODO(chengchen): replace 0, 1, 2 to meaningful enum names.
    // mask of current row. mask4x4 represents the horizontal filter length for
    // the current vertical edge is 4, and we needs to apply 3-tap filtering.
    // Similarly, mask8x8 and mask16x16 represent filter lengths are 8 and 16.
    uint64_t mask4x4_0 =
        (masks_->GetLeft(unit_id, plane, kLoopFilterTransformSizeId4x4,
                         index) >>
         shift) &
        single_row_mask;
    uint64_t mask8x8_0 =
        (masks_->GetLeft(unit_id, plane, kLoopFilterTransformSizeId8x8,
                         index) >>
         shift) &
        single_row_mask;
    uint64_t mask16x16_0 =
        (masks_->GetLeft(unit_id, plane, kLoopFilterTransformSizeId16x16,
                         index) >>
         shift) &
        single_row_mask;
    // mask4x4, mask8x8, mask16x16 are mutually exclusive.
    assert((mask4x4_0 & mask8x8_0) == 0 && (mask4x4_0 & mask16x16_0) == 0 &&
           (mask8x8_0 & mask16x16_0) == 0);
    // mask of the next row. With mask of current and the next row, we can call
    // the corresponding SIMD function to apply filtering for two vertical
    // edges together.
    uint64_t mask4x4_1 =
        (masks_->GetLeft(unit_id, plane, kLoopFilterTransformSizeId4x4,
                         index_next) >>
         shift_next_row) &
        single_row_mask;
    uint64_t mask8x8_1 =
        (masks_->GetLeft(unit_id, plane, kLoopFilterTransformSizeId8x8,
                         index_next) >>
         shift_next_row) &
        single_row_mask;
    uint64_t mask16x16_1 =
        (masks_->GetLeft(unit_id, plane, kLoopFilterTransformSizeId16x16,
                         index_next) >>
         shift_next_row) &
        single_row_mask;
    // mask4x4, mask8x8, mask16x16 are mutually exclusive.
    assert((mask4x4_1 & mask8x8_1) == 0 && (mask4x4_1 & mask16x16_1) == 0 &&
           (mask8x8_1 & mask16x16_1) == 0);
    // Apply deblock filter for two rows.
    uint8_t* src_row = src;
    int column_offset = 0;
    for (uint64_t mask = mask4x4_0 | mask8x8_0 | mask16x16_0 | mask4x4_1 |
                         mask8x8_1 | mask16x16_1;
         mask != 0;) {
      if ((mask & 1) != 0) {
        // Filter parameters of current row.
        const uint8_t level = masks_->GetLevel(unit_id, plane, type,
                                               level_offset + column_offset);
        int outer_thresh_0;
        int inner_thresh_0;
        int hev_thresh_0;
        GetDeblockFilterParams(level, &outer_thresh_0, &inner_thresh_0,
                               &hev_thresh_0);
        // Filter parameters of next row. Clip the index to avoid over
        // reading at the edge of the block. The values will be unused in that
        // case.
        const int level_next_index = level_offset_next_row + column_offset;
        const uint8_t level_next =
            masks_->GetLevel(unit_id, plane, type, level_next_index & 0xff);
        int outer_thresh_1;
        int inner_thresh_1;
        int hev_thresh_1;
        GetDeblockFilterParams(level_next, &outer_thresh_1, &inner_thresh_1,
                               &hev_thresh_1);
        uint8_t* const src_row_next = src_row + row_stride;

        if (((mask16x16_0 | mask16x16_1) & 1) != 0) {
          const dsp::LoopFilterSize size = (plane == kPlaneY)
                                               ? dsp::kLoopFilterSize14
                                               : dsp::kLoopFilterSize6;
          const dsp::LoopFilterFunc filter_func = dsp_.loop_filters[size][type];
          if ((mask16x16_0 & mask16x16_1 & 1) != 0) {
            // Apply dual vertical edge filtering.
            filter_func(src_row, src_stride, outer_thresh_0, inner_thresh_0,
                        hev_thresh_0);
            filter_func(src_row_next, src_stride, outer_thresh_1,
                        inner_thresh_1, hev_thresh_1);
          } else if ((mask16x16_0 & 1) != 0) {
            filter_func(src_row, src_stride, outer_thresh_0, inner_thresh_0,
                        hev_thresh_0);
          } else {
            filter_func(src_row_next, src_stride, outer_thresh_1,
                        inner_thresh_1, hev_thresh_1);
          }
        }

        if (((mask8x8_0 | mask8x8_1) & 1) != 0) {
          const dsp::LoopFilterSize size = (plane == kPlaneY)
                                               ? dsp::kLoopFilterSize8
                                               : dsp::kLoopFilterSize6;
          const dsp::LoopFilterFunc filter_func = dsp_.loop_filters[size][type];
          if ((mask8x8_0 & mask8x8_1 & 1) != 0) {
            filter_func(src_row, src_stride, outer_thresh_0, inner_thresh_0,
                        hev_thresh_0);
            filter_func(src_row_next, src_stride, outer_thresh_1,
                        inner_thresh_1, hev_thresh_1);
          } else if ((mask8x8_0 & 1) != 0) {
            filter_func(src_row, src_stride, outer_thresh_0, inner_thresh_0,
                        hev_thresh_0);
          } else {
            filter_func(src_row_next, src_stride, outer_thresh_1,
                        inner_thresh_1, hev_thresh_1);
          }
        }

        if (((mask4x4_0 | mask4x4_1) & 1) != 0) {
          const dsp::LoopFilterSize size = dsp::kLoopFilterSize4;
          const dsp::LoopFilterFunc filter_func = dsp_.loop_filters[size][type];
          if ((mask4x4_0 & mask4x4_1 & 1) != 0) {
            filter_func(src_row, src_stride, outer_thresh_0, inner_thresh_0,
                        hev_thresh_0);
            filter_func(src_row_next, src_stride, outer_thresh_1,
                        inner_thresh_1, hev_thresh_1);
          } else if ((mask4x4_0 & 1) != 0) {
            filter_func(src_row, src_stride, outer_thresh_0, inner_thresh_0,
                        hev_thresh_0);
          } else {
            filter_func(src_row_next, src_stride, outer_thresh_1,
                        inner_thresh_1, hev_thresh_1);
          }
        }
      }

      mask4x4_0 >>= column_step;
      mask8x8_0 >>= column_step;
      mask16x16_0 >>= column_step;
      mask4x4_1 >>= column_step;
      mask8x8_1 >>= column_step;
      mask16x16_1 >>= column_step;
      mask >>= column_step;
      column_offset += column_step;
      src_row += src_step;
    }
    src += two_row_stride;
  }
}

void PostFilter::HorizontalDeblockFilterNoMask(Plane plane, int row4x4_start,
                                               int column4x4_start,
                                               int unit_id) {
  static_cast<void>(unit_id);
  const int8_t subsampling_x = subsampling_x_[plane];
  const int8_t subsampling_y = subsampling_y_[plane];
  const int column_step = 1 << subsampling_x;
  const size_t src_step = MultiplyBy4(pixel_size_);
  const ptrdiff_t src_stride = frame_buffer_.stride(plane);
  uint8_t* src = GetSourceBuffer(plane, row4x4_start, column4x4_start);
  const LoopFilterType type = kLoopFilterTypeHorizontal;
  int row_step;
  uint8_t level;
  int filter_length;

  for (int column4x4 = 0; MultiplyBy4(column4x4_start + column4x4) < width_ &&
                          column4x4 < kNum4x4InLoopFilterMaskUnit;
       column4x4 += column_step, src += src_step) {
    uint8_t* src_row = src;
    for (int row4x4 = 0; MultiplyBy4(row4x4_start + row4x4) < height_ &&
                         row4x4 < kNum4x4InLoopFilterMaskUnit;
         row4x4 += row_step) {
      const bool need_filter =
          GetDeblockFilterEdgeInfo<kLoopFilterTypeHorizontal>(
              plane, row4x4_start + row4x4, column4x4_start + column4x4,
              subsampling_x, subsampling_y, &level, &row_step, &filter_length);
      if (need_filter) {
        int outer_thresh;
        int inner_thresh;
        int hev_thresh;
        GetDeblockFilterParams(level, &outer_thresh, &inner_thresh,
                               &hev_thresh);
        const dsp::LoopFilterSize size =
            GetLoopFilterSize(plane, filter_length);
        const dsp::LoopFilterFunc filter_func = dsp_.loop_filters[size][type];
        filter_func(src_row, src_stride, outer_thresh, inner_thresh,
                    hev_thresh);
      }
      // TODO(chengchen): use shifts instead of multiplication.
      src_row += row_step * src_stride;
      row_step = DivideBy4(row_step << subsampling_y);
    }
  }
}

void PostFilter::VerticalDeblockFilterNoMask(Plane plane, int row4x4_start,
                                             int column4x4_start, int unit_id) {
  static_cast<void>(unit_id);
  const int8_t subsampling_x = subsampling_x_[plane];
  const int8_t subsampling_y = subsampling_y_[plane];
  const int row_step = 1 << subsampling_y;
  const ptrdiff_t row_stride = MultiplyBy4(frame_buffer_.stride(plane));
  const ptrdiff_t src_stride = frame_buffer_.stride(plane);
  uint8_t* src = GetSourceBuffer(plane, row4x4_start, column4x4_start);
  const LoopFilterType type = kLoopFilterTypeVertical;
  int column_step;
  uint8_t level;
  int filter_length;

  for (int row4x4 = 0; MultiplyBy4(row4x4_start + row4x4) < height_ &&
                       row4x4 < kNum4x4InLoopFilterMaskUnit;
       row4x4 += row_step, src += row_stride) {
    uint8_t* src_row = src;
    for (int column4x4 = 0; MultiplyBy4(column4x4_start + column4x4) < width_ &&
                            column4x4 < kNum4x4InLoopFilterMaskUnit;
         column4x4 += column_step) {
      const bool need_filter =
          GetDeblockFilterEdgeInfo<kLoopFilterTypeVertical>(
              plane, row4x4_start + row4x4, column4x4_start + column4x4,
              subsampling_x, subsampling_y, &level, &column_step,
              &filter_length);
      if (need_filter) {
        int outer_thresh;
        int inner_thresh;
        int hev_thresh;
        GetDeblockFilterParams(level, &outer_thresh, &inner_thresh,
                               &hev_thresh);
        const dsp::LoopFilterSize size =
            GetLoopFilterSize(plane, filter_length);
        const dsp::LoopFilterFunc filter_func = dsp_.loop_filters[size][type];
        filter_func(src_row, src_stride, outer_thresh, inner_thresh,
                    hev_thresh);
      }
      src_row += column_step * pixel_size_;
      column_step = DivideBy4(column_step << subsampling_x);
    }
  }
}

void PostFilter::ApplyDeblockFilterForOneSuperBlockRow(int row4x4_start,
                                                       int sb4x4) {
  assert(row4x4_start >= 0);
  assert(DoDeblock());
  for (int plane = kPlaneY; plane < planes_; ++plane) {
    if (plane != kPlaneY && frame_header_.loop_filter.level[plane + 1] == 0) {
      continue;
    }

    for (int y = 0; y < sb4x4; y += 16) {
      const int row4x4 = row4x4_start + y;
      if (row4x4 >= frame_header_.rows4x4) break;
      int column4x4;
      for (column4x4 = 0; column4x4 < frame_header_.columns4x4;
           column4x4 += kNum4x4InLoopFilterMaskUnit) {
        // First apply vertical filtering
        VerticalDeblockFilterNoMask(static_cast<Plane>(plane), row4x4,
                                    column4x4, 0);

        // Delay one superblock to apply horizontal filtering.
        if (column4x4 != 0) {
          HorizontalDeblockFilterNoMask(static_cast<Plane>(plane), row4x4,
                                        column4x4 - kNum4x4InLoopFilterMaskUnit,
                                        0);
        }
      }
      // Horizontal filtering for the last 64x64 block.
      HorizontalDeblockFilterNoMask(static_cast<Plane>(plane), row4x4,
                                    column4x4 - kNum4x4InLoopFilterMaskUnit, 0);
    }
  }
}

void PostFilter::DeblockFilterWorker(int jobs_per_plane, const Plane* planes,
                                     int num_planes,
                                     std::atomic<int>* job_counter,
                                     DeblockFilter deblock_filter) {
  const int total_jobs = jobs_per_plane * num_planes;
  int job_index;
  while ((job_index = job_counter->fetch_add(1, std::memory_order_relaxed)) <
         total_jobs) {
    const Plane plane = planes[job_index / jobs_per_plane];
    const int row_unit = job_index % jobs_per_plane;
    const int row4x4 = row_unit * kNum4x4InLoopFilterMaskUnit;
    for (int column4x4 = 0, column_unit = 0;
         column4x4 < frame_header_.columns4x4;
         column4x4 += kNum4x4InLoopFilterMaskUnit, ++column_unit) {
      const int unit_id = GetDeblockUnitId(row_unit, column_unit);
      (this->*deblock_filter)(plane, row4x4, column4x4, unit_id);
    }
  }
}

void PostFilter::ApplyDeblockFilterThreaded() {
  const int jobs_per_plane = DivideBy16(frame_header_.rows4x4 + 15);
  const int num_workers = thread_pool_->num_threads();
  std::array<Plane, kMaxPlanes> planes;
  planes[0] = kPlaneY;
  int num_planes = 1;
  for (int plane = kPlaneU; plane < planes_; ++plane) {
    if (frame_header_.loop_filter.level[plane + 1] != 0) {
      planes[num_planes++] = static_cast<Plane>(plane);
    }
  }
  // The vertical filters are not dependent on each other. So simply schedule
  // them for all possible rows.
  //
  // The horizontal filter for a row/column depends on the vertical filter being
  // finished for the blocks to the top and to the right. To work around
  // this synchronization, we simply wait for the vertical filter to finish for
  // all rows. Now, the horizontal filters can also be scheduled
  // unconditionally similar to the vertical filters.
  //
  // The only synchronization involved is to know when the each directional
  // filter is complete for the entire frame.
  for (auto& type : {kLoopFilterTypeVertical, kLoopFilterTypeHorizontal}) {
    const DeblockFilter deblock_filter =
        deblock_filter_type_table_[kDeblockFilterBitMask][type];
    std::atomic<int> job_counter(0);
    BlockingCounter pending_workers(num_workers);
    for (int i = 0; i < num_workers; ++i) {
      thread_pool_->Schedule([this, jobs_per_plane, &planes, num_planes,
                              &job_counter, deblock_filter,
                              &pending_workers]() {
        DeblockFilterWorker(jobs_per_plane, planes.data(), num_planes,
                            &job_counter, deblock_filter);
        pending_workers.Decrement();
      });
    }
    // Run the jobs on the current thread.
    DeblockFilterWorker(jobs_per_plane, planes.data(), num_planes, &job_counter,
                        deblock_filter);
    // Wait for the threadpool jobs to finish.
    pending_workers.Wait();
  }
}

}  // namespace libgav1
