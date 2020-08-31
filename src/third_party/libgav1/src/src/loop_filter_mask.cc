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

#include "src/loop_filter_mask.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>

#include "src/utils/array_2d.h"
#include "src/utils/compiler_attributes.h"

namespace libgav1 {

#if !LIBGAV1_CXX17
// static.
constexpr BitMaskSet LoopFilterMask::kPredictionModeDeltasMask;
#endif

bool LoopFilterMask::Reset(int width, int height) {
  num_64x64_blocks_per_row_ = DivideBy64(width + 63);
  num_64x64_blocks_per_column_ = DivideBy64(height + 63);
  const int num_64x64_blocks =
      num_64x64_blocks_per_row_ * num_64x64_blocks_per_column_;
  if (num_64x64_blocks_ == -1 || num_64x64_blocks_ < num_64x64_blocks) {
    // Note that this need not be zero initialized here since we zero
    // initialize the required number of entries in the loop that follows.
    loop_filter_masks_.reset(new (std::nothrow)
                                 Data[num_64x64_blocks]);  // NOLINT.
    if (loop_filter_masks_ == nullptr) {
      return false;
    }
  }
  for (int i = 0; i < num_64x64_blocks; ++i) {
    memset(&loop_filter_masks_[i], 0, sizeof(loop_filter_masks_[i]));
  }
  num_64x64_blocks_ = num_64x64_blocks;
  return true;
}

void LoopFilterMask::Build(
    const ObuSequenceHeader& sequence_header,
    const ObuFrameHeader& frame_header, int tile_group_start,
    int tile_group_end, const BlockParametersHolder& block_parameters_holder,
    const Array2D<TransformSize>& inter_transform_sizes) {
  for (int tile_number = tile_group_start; tile_number <= tile_group_end;
       ++tile_number) {
    const int row = tile_number / frame_header.tile_info.tile_columns;
    const int column = tile_number % frame_header.tile_info.tile_columns;
    const int row4x4_start = frame_header.tile_info.tile_row_start[row];
    const int row4x4_end = frame_header.tile_info.tile_row_start[row + 1];
    const int column4x4_start =
        frame_header.tile_info.tile_column_start[column];
    const int column4x4_end =
        frame_header.tile_info.tile_column_start[column + 1];

    const int num_planes = sequence_header.color_config.is_monochrome
                               ? kMaxPlanesMonochrome
                               : kMaxPlanes;
    for (int plane = kPlaneY; plane < num_planes; ++plane) {
      // For U and V planes, do not build bit masks if level == 0.
      if (plane > kPlaneY && frame_header.loop_filter.level[plane + 1] == 0) {
        continue;
      }
      const int8_t subsampling_x =
          (plane == kPlaneY) ? 0 : sequence_header.color_config.subsampling_x;
      const int8_t subsampling_y =
          (plane == kPlaneY) ? 0 : sequence_header.color_config.subsampling_y;
      const int vertical_step = 1 << subsampling_y;
      const int horizontal_step = 1 << subsampling_x;

      // Build bit masks for vertical edges (except the frame boundary).
      if (column4x4_start != 0) {
        const int plane_height =
            RightShiftWithRounding(frame_header.height, subsampling_y);
        const int row4x4_limit =
            std::min(row4x4_end, DivideBy4(plane_height + 3) << subsampling_y);
        const int vertical_level_index =
            kDeblockFilterLevelIndex[plane][kLoopFilterTypeVertical];
        for (int row4x4 = GetDeblockPosition(row4x4_start, subsampling_y);
             row4x4 < row4x4_limit; row4x4 += vertical_step) {
          const int column4x4 =
              GetDeblockPosition(column4x4_start, subsampling_x);
          const BlockParameters& bp =
              *block_parameters_holder.Find(row4x4, column4x4);
          const uint8_t vertical_level =
              bp.deblock_filter_level[vertical_level_index];
          const BlockParameters& bp_left = *block_parameters_holder.Find(
              row4x4, column4x4 - horizontal_step);
          const uint8_t left_level =
              bp_left.deblock_filter_level[vertical_level_index];
          const int unit_id = DivideBy16(row4x4) * num_64x64_blocks_per_row_ +
                              DivideBy16(column4x4);
          const int row = row4x4 % kNum4x4InLoopFilterMaskUnit;
          const int column = column4x4 % kNum4x4InLoopFilterMaskUnit;
          const int shift = LoopFilterMask::GetShift(row, column);
          const int index = LoopFilterMask::GetIndex(row);
          const auto mask = static_cast<uint64_t>(1) << shift;
          // Tile boundary must be coding block boundary. So we don't have to
          // check (!left_skip || !skip || is_vertical_border).
          if (vertical_level != 0 || left_level != 0) {
            assert(inter_transform_sizes[row4x4] != nullptr);
            const TransformSize tx_size =
                (plane == kPlaneY) ? inter_transform_sizes[row4x4][column4x4]
                                   : bp.uv_transform_size;
            const TransformSize left_tx_size =
                (plane == kPlaneY)
                    ? inter_transform_sizes[row4x4][column4x4 - horizontal_step]
                    : bp_left.uv_transform_size;
            const LoopFilterTransformSizeId transform_size_id =
                GetTransformSizeIdWidth(tx_size, left_tx_size);
            SetLeft(mask, unit_id, plane, transform_size_id, index);
            const uint8_t current_level =
                (vertical_level == 0) ? left_level : vertical_level;
            SetLevel(current_level, unit_id, plane, kLoopFilterTypeVertical,
                     LoopFilterMask::GetLevelOffset(row, column));
          }
        }
      }

      // Build bit masks for horizontal edges (except the frame boundary).
      if (row4x4_start != 0) {
        const int plane_width =
            RightShiftWithRounding(frame_header.width, subsampling_x);
        const int column4x4_limit = std::min(
            column4x4_end, DivideBy4(plane_width + 3) << subsampling_y);
        const int horizontal_level_index =
            kDeblockFilterLevelIndex[plane][kLoopFilterTypeHorizontal];
        for (int column4x4 = GetDeblockPosition(column4x4_start, subsampling_x);
             column4x4 < column4x4_limit; column4x4 += horizontal_step) {
          const int row4x4 = GetDeblockPosition(row4x4_start, subsampling_y);
          const BlockParameters& bp =
              *block_parameters_holder.Find(row4x4, column4x4);
          const uint8_t horizontal_level =
              bp.deblock_filter_level[horizontal_level_index];
          const BlockParameters& bp_top =
              *block_parameters_holder.Find(row4x4 - vertical_step, column4x4);
          const uint8_t top_level =
              bp_top.deblock_filter_level[horizontal_level_index];
          const int unit_id = DivideBy16(row4x4) * num_64x64_blocks_per_row_ +
                              DivideBy16(column4x4);
          const int row = row4x4 % kNum4x4InLoopFilterMaskUnit;
          const int column = column4x4 % kNum4x4InLoopFilterMaskUnit;
          const int shift = LoopFilterMask::GetShift(row, column);
          const int index = LoopFilterMask::GetIndex(row);
          const auto mask = static_cast<uint64_t>(1) << shift;
          // Tile boundary must be coding block boundary. So we don't have to
          // check (!top_skip || !skip || is_horizontal_border).
          if (horizontal_level != 0 || top_level != 0) {
            assert(inter_transform_sizes[row4x4] != nullptr);
            const TransformSize tx_size =
                (plane == kPlaneY) ? inter_transform_sizes[row4x4][column4x4]
                                   : bp.uv_transform_size;
            const TransformSize top_tx_size =
                (plane == kPlaneY)
                    ? inter_transform_sizes[row4x4 - vertical_step][column4x4]
                    : bp_top.uv_transform_size;
            const LoopFilterTransformSizeId transform_size_id =
                static_cast<LoopFilterTransformSizeId>(
                    std::min({kTransformHeightLog2[tx_size] - 2,
                              kTransformHeightLog2[top_tx_size] - 2, 2}));
            SetTop(mask, unit_id, plane, transform_size_id, index);
            const uint8_t current_level =
                (horizontal_level == 0) ? top_level : horizontal_level;
            SetLevel(current_level, unit_id, plane, kLoopFilterTypeHorizontal,
                     LoopFilterMask::GetLevelOffset(row, column));
          }
        }
      }
    }
  }
  assert(IsValid());
}

bool LoopFilterMask::IsValid() const {
  for (int mask_id = 0; mask_id < num_64x64_blocks_; ++mask_id) {
    for (int plane = 0; plane < kMaxPlanes; ++plane) {
      for (int i = 0; i < kNumLoopFilterTransformSizeIds; ++i) {
        for (int j = i + 1; j < kNumLoopFilterTransformSizeIds; ++j) {
          for (int k = 0; k < kNumLoopFilterMasks; ++k) {
            if ((loop_filter_masks_[mask_id].left[plane][i][k] &
                 loop_filter_masks_[mask_id].left[plane][j][k]) != 0 ||
                (loop_filter_masks_[mask_id].top[plane][i][k] &
                 loop_filter_masks_[mask_id].top[plane][j][k]) != 0) {
              return false;
            }
          }
        }
      }
    }
  }
  return true;
}

}  // namespace libgav1
