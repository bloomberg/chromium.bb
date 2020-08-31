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

#ifndef LIBGAV1_SRC_LOOP_FILTER_MASK_H_
#define LIBGAV1_SRC_LOOP_FILTER_MASK_H_

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/obu_parser.h"
#include "src/utils/array_2d.h"
#include "src/utils/bit_mask_set.h"
#include "src/utils/block_parameters_holder.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"
#include "src/utils/segmentation.h"
#include "src/utils/types.h"

namespace libgav1 {

class LoopFilterMask {
 public:
  // This structure holds loop filter bit masks for a 64x64 block.
  // 64x64 block contains kNum4x4In64x64 = (64x64 / (4x4) = 256)
  // 4x4 blocks. It requires kNumLoopFilterMasks = 4 uint64_t to represent them.
  struct Data : public Allocable {
    uint8_t level[kMaxPlanes][kNumLoopFilterTypes][kNum4x4In64x64];
    uint64_t left[kMaxPlanes][kNumLoopFilterTransformSizeIds]
                 [kNumLoopFilterMasks];
    uint64_t top[kMaxPlanes][kNumLoopFilterTransformSizeIds]
                [kNumLoopFilterMasks];
  };

  LoopFilterMask() = default;

  // Loop filter mask is built and used for each superblock individually.
  // Thus not copyable/movable.
  LoopFilterMask(const LoopFilterMask&) = delete;
  LoopFilterMask& operator=(const LoopFilterMask&) = delete;
  LoopFilterMask(LoopFilterMask&&) = delete;
  LoopFilterMask& operator=(LoopFilterMask&&) = delete;

  // Allocates the loop filter masks for the given |width| and
  // |height| if necessary and zeros out the appropriate number of
  // entries. Returns true on success.
  bool Reset(int width, int height);

  // Builds bit masks for tile boundaries.
  // This function is called after the frame has been decoded so that
  // information across tiles is available.
  // Before this function call, bit masks of transform edges other than those
  // on tile boundaries are built together with tile decoding, in
  // Tile::BuildBitMask().
  void Build(const ObuSequenceHeader& sequence_header,
             const ObuFrameHeader& frame_header, int tile_group_start,
             int tile_group_end,
             const BlockParametersHolder& block_parameters_holder,
             const Array2D<TransformSize>& inter_transform_sizes);

  uint8_t GetLevel(int mask_id, int plane, LoopFilterType type,
                   int offset) const {
    return loop_filter_masks_[mask_id].level[plane][type][offset];
  }

  uint64_t GetLeft(int mask_id, int plane, LoopFilterTransformSizeId tx_size_id,
                   int index) const {
    return loop_filter_masks_[mask_id].left[plane][tx_size_id][index];
  }

  uint64_t GetTop(int mask_id, int plane, LoopFilterTransformSizeId tx_size_id,
                  int index) const {
    return loop_filter_masks_[mask_id].top[plane][tx_size_id][index];
  }

  int num_64x64_blocks_per_row() const { return num_64x64_blocks_per_row_; }

  void SetLeft(uint64_t new_mask, int mask_id, int plane,
               LoopFilterTransformSizeId transform_size_id, int index) {
    loop_filter_masks_[mask_id].left[plane][transform_size_id][index] |=
        new_mask;
  }

  void SetTop(uint64_t new_mask, int mask_id, int plane,
              LoopFilterTransformSizeId transform_size_id, int index) {
    loop_filter_masks_[mask_id].top[plane][transform_size_id][index] |=
        new_mask;
  }

  void SetLevel(uint8_t level, int mask_id, int plane, LoopFilterType type,
                int offset) {
    loop_filter_masks_[mask_id].level[plane][type][offset] = level;
  }

  static int GetIndex(int row4x4) { return row4x4 >> 2; }

  static int GetShift(int row4x4, int column4x4) {
    return ((row4x4 & 3) << 4) | column4x4;
  }

  static int GetLevelOffset(int row4x4, int column4x4) {
    assert(row4x4 < 16);
    assert(column4x4 < 16);
    return (row4x4 << 4) | column4x4;
  }

  static constexpr int GetModeId(PredictionMode mode) {
    return static_cast<int>(kPredictionModeDeltasMask.Contains(mode));
  }

  // 7.14.5.
  static void ComputeDeblockFilterLevels(
      const ObuFrameHeader& frame_header, int segment_id, int level_index,
      const int8_t delta_lf[kFrameLfCount],
      uint8_t deblock_filter_levels[kNumReferenceFrameTypes][2]) {
    const int delta = delta_lf[frame_header.delta_lf.multi ? level_index : 0];
    uint8_t level = Clip3(frame_header.loop_filter.level[level_index] + delta,
                          0, kMaxLoopFilterValue);
    const auto feature = static_cast<SegmentFeature>(
        kSegmentFeatureLoopFilterYVertical + level_index);
    level = Clip3(
        level + frame_header.segmentation.feature_data[segment_id][feature], 0,
        kMaxLoopFilterValue);
    if (!frame_header.loop_filter.delta_enabled) {
      static_assert(sizeof(deblock_filter_levels[0][0]) == 1, "");
      memset(deblock_filter_levels, level, kNumReferenceFrameTypes * 2);
      return;
    }
    assert(frame_header.loop_filter.delta_enabled);
    const int shift = level >> 5;
    deblock_filter_levels[kReferenceFrameIntra][0] = Clip3(
        level +
            LeftShift(frame_header.loop_filter.ref_deltas[kReferenceFrameIntra],
                      shift),
        0, kMaxLoopFilterValue);
    // deblock_filter_levels[kReferenceFrameIntra][1] is never used. So it does
    // not have to be populated.
    for (int reference_frame = kReferenceFrameIntra + 1;
         reference_frame < kNumReferenceFrameTypes; ++reference_frame) {
      for (int mode_id = 0; mode_id < 2; ++mode_id) {
        deblock_filter_levels[reference_frame][mode_id] = Clip3(
            level +
                LeftShift(frame_header.loop_filter.ref_deltas[reference_frame] +
                              frame_header.loop_filter.mode_deltas[mode_id],
                          shift),
            0, kMaxLoopFilterValue);
      }
    }
  }

 private:
  std::unique_ptr<Data[]> loop_filter_masks_;
  int num_64x64_blocks_ = -1;
  int num_64x64_blocks_per_row_;
  int num_64x64_blocks_per_column_;

  // Mask used to determine the index for mode_deltas lookup.
  static constexpr BitMaskSet kPredictionModeDeltasMask{
      BitMaskSet(kPredictionModeNearestMv, kPredictionModeNearMv,
                 kPredictionModeNewMv, kPredictionModeNearestNearestMv,
                 kPredictionModeNearNearMv, kPredictionModeNearestNewMv,
                 kPredictionModeNewNearestMv, kPredictionModeNearNewMv,
                 kPredictionModeNewNearMv, kPredictionModeNewNewMv)};

  // Validates that the loop filter masks at different transform sizes are
  // mutually exclusive. Only used in an assert. This function will not be used
  // in optimized builds.
  bool IsValid() const;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_LOOP_FILTER_MASK_H_
