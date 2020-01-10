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

#include "src/motion_vector.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <memory>

#include "src/utils/bit_mask_set.h"
#include "src/utils/common.h"
#include "src/utils/logging.h"

namespace libgav1 {
namespace {

constexpr int kMvBorder = 128;
constexpr int kProjectionMvClamp = 16383;
constexpr int kProjectionMvMaxHorizontalOffset = 8;

// Entry at index i is computed as:
// Clip3(std::max(kBlockWidthPixels[i], kBlockHeightPixels[i], 16, 112)).
constexpr int kWarpValidThreshold[kMaxBlockSizes] = {
    16, 16, 16, 16, 16, 16, 32, 16, 16,  16,  32,
    64, 32, 32, 32, 64, 64, 64, 64, 112, 112, 112};

// Applies |sign| (must be 0 or -1) to |value| and does so without a branch.
constexpr int ApplySign(int value, int sign) { return (value ^ sign) - sign; }

// 7.10.2.10.
void LowerMvPrecision(const Tile::Block& block, int16_t* const mv) {
  assert(mv != nullptr);
  if (block.tile.frame_header().allow_high_precision_mv) return;
  if (block.tile.frame_header().force_integer_mv != 0) {
    for (int i = 0; i < 2; ++i) {
      const int value = MultiplyBy8(DivideBy8(std::abs(mv[i]) + 3));
      const int sign = mv[i] >> 15;
      mv[i] = ApplySign(value, sign);
    }
  } else {
    for (int i = 0; i < 2; ++i) {
      if ((mv[i] & 1) != 0) {
        // The next line is equivalent to:
        // if (mv[i] > 0) { --mv[i]; } else { ++mv[i]; }
        mv[i] -= (mv[i] >> 14) | 1;
      }
    }
  }
}

constexpr int16_t kDivisionLookup[kMaxFrameDistance + 1] = {
    0,    16384, 8192, 5461, 4096, 3276, 2730, 2340, 2048, 1820, 1638,
    1489, 1365,  1260, 1170, 1092, 1024, 963,  910,  862,  819,  780,
    744,  712,   682,  655,  630,  606,  585,  564,  546,  528};

// 7.9.3.
void GetMvProjection(const MotionVector& mv, int numerator,
                     const int denominator, MotionVector* const projection_mv) {
  assert(denominator > 0);
  assert(denominator <= kMaxFrameDistance);
  numerator = Clip3(numerator, -kMaxFrameDistance, kMaxFrameDistance);
  for (int i = 0; i < 2; ++i) {
    projection_mv->mv[i] =
        Clip3(RightShiftWithRoundingSigned(
                  mv.mv[i] * numerator * kDivisionLookup[denominator], 14),
              -kProjectionMvClamp, kProjectionMvClamp);
  }
}

// 7.9.3. (without the clamp for numerator and denominator).
void GetMvProjectionNoClamp(const MotionVector& mv, const int numerator,
                            const int denominator,
                            MotionVector* const projection_mv) {
  // Allow numerator and denominator to be 0 to simplify branch conditions.
  assert(std::abs(numerator) <= kMaxFrameDistance);
  assert(denominator >= 0);
  assert(denominator <= kMaxFrameDistance);
  for (int i = 0; i < 2; ++i) {
    projection_mv->mv[i] =
        Clip3(RightShiftWithRoundingSigned(
                  mv.mv[i] * numerator * kDivisionLookup[denominator], 14),
              -kProjectionMvClamp, kProjectionMvClamp);
  }
}

// 7.10.2.1.
void SetupGlobalMv(const Tile::Block& block, int index,
                   MotionVector* const mv) {
  const BlockParameters& bp = *block.bp;
  ReferenceFrameType reference_type = bp.reference_frame[index];
  const auto& gm = block.tile.frame_header().global_motion[reference_type];
  GlobalMotionTransformationType global_motion_type =
      (reference_type != kReferenceFrameIntra)
          ? gm.type
          : kNumGlobalMotionTransformationTypes;
  if (reference_type == kReferenceFrameIntra ||
      global_motion_type == kGlobalMotionTransformationTypeIdentity) {
    mv->mv32 = 0;
    return;
  }
  if (global_motion_type == kGlobalMotionTransformationTypeTranslation) {
    for (int i = 0; i < 2; ++i) {
      mv->mv[i] = gm.params[i] >> (kWarpedModelPrecisionBits - 3);
    }
    LowerMvPrecision(block, mv->mv);
    return;
  }
  const int x = MultiplyBy4(block.column4x4) + DivideBy2(block.width) - 1;
  const int y = MultiplyBy4(block.row4x4) + DivideBy2(block.height) - 1;
  const int xc = (gm.params[2] - (1 << kWarpedModelPrecisionBits)) * x +
                 gm.params[3] * y + gm.params[0];
  const int yc = gm.params[4] * x +
                 (gm.params[5] - (1 << kWarpedModelPrecisionBits)) * y +
                 gm.params[1];
  if (block.tile.frame_header().allow_high_precision_mv) {
    mv->mv[MotionVector::kRow] =
        RightShiftWithRoundingSigned(yc, kWarpedModelPrecisionBits - 3);
    mv->mv[MotionVector::kColumn] =
        RightShiftWithRoundingSigned(xc, kWarpedModelPrecisionBits - 3);
  } else {
    mv->mv[MotionVector::kRow] = MultiplyBy2(
        RightShiftWithRoundingSigned(yc, kWarpedModelPrecisionBits - 2));
    mv->mv[MotionVector::kColumn] = MultiplyBy2(
        RightShiftWithRoundingSigned(xc, kWarpedModelPrecisionBits - 2));
    LowerMvPrecision(block, mv->mv);
  }
}

constexpr BitMaskSet kPredictionModeNewMvMask(kPredictionModeNewMv,
                                              kPredictionModeNewNewMv,
                                              kPredictionModeNearNewMv,
                                              kPredictionModeNewNearMv,
                                              kPredictionModeNearestNewMv,
                                              kPredictionModeNewNearestMv);

// 7.10.2.8. when is_compound is false and 7.10.2.9. when is_compound is true.
// |index| parameter is used only when is_compound is false. It is ignored
// otherwise.
template <bool is_compound>
void SearchStack(const Tile::Block& block, const BlockParameters& mv_bp,
                 int index, int weight, MotionVector global_mv_candidate[2],
                 bool* const found_new_mv, bool* const found_match,
                 int* const num_mv_found,
                 CandidateMotionVector ref_mv_stack[kMaxRefMvStackSize]) {
  const PredictionMode candidate_mode = mv_bp.y_mode;
  const BlockSize candidate_size = mv_bp.size;
  MotionVector candidate_mv[2];
  // Unifying the conditions below in the for loop causes clang to disable
  // vectorization for the LowerMvPrecision code. So this has to be kept this
  // way.
  if (is_compound) {
    for (int i = 0; i < 2; ++i) {
      const auto global_motion_type =
          block.tile.frame_header()
              .global_motion[block.bp->reference_frame[i]]
              .type;
      if (IsGlobalMvBlock(candidate_mode, global_motion_type, candidate_size)) {
        candidate_mv[i] = global_mv_candidate[i];
      } else {
        candidate_mv[i] = mv_bp.mv[i];
      }
      if (is_compound) LowerMvPrecision(block, candidate_mv[i].mv);
    }
  } else {
    const auto global_motion_type =
        block.tile.frame_header()
            .global_motion[block.bp->reference_frame[0]]
            .type;
    if (IsGlobalMvBlock(candidate_mode, global_motion_type, candidate_size)) {
      candidate_mv[0] = global_mv_candidate[0];
    } else {
      candidate_mv[0] = mv_bp.mv[index];
    }
    LowerMvPrecision(block, candidate_mv[0].mv);
    candidate_mv[1] = {};
  }
  *found_new_mv |= kPredictionModeNewMvMask.Contains(candidate_mode);
  *found_match = true;
  auto result =
      std::find_if(ref_mv_stack, ref_mv_stack + *num_mv_found,
                   [&candidate_mv](const CandidateMotionVector& ref_mv) {
                     return ref_mv.mv[0] == candidate_mv[0] &&
                            (!is_compound || ref_mv.mv[1] == candidate_mv[1]);
                   });
  if (result != ref_mv_stack + *num_mv_found) {
    ref_mv_stack[std::distance(ref_mv_stack, result)].weight += weight;
    return;
  }
  if (*num_mv_found >= kMaxRefMvStackSize) return;
  ref_mv_stack[*num_mv_found] = {{candidate_mv[0], candidate_mv[1]}, weight};
  ++*num_mv_found;
}

// 7.10.2.7.
void AddReferenceMvCandidate(
    const Tile::Block& block, const BlockParameters& mv_bp, bool is_compound,
    int weight, MotionVector global_mv[2], bool* const found_new_mv,
    bool* const found_match, int* const num_mv_found,
    CandidateMotionVector ref_mv_stack[kMaxRefMvStackSize]) {
  if (!mv_bp.is_inter) return;
  if (is_compound) {
    if (mv_bp.reference_frame[0] == block.bp->reference_frame[0] &&
        mv_bp.reference_frame[1] == block.bp->reference_frame[1]) {
      SearchStack</*is_compound=*/true>(block, mv_bp, 0, weight, global_mv,
                                        found_new_mv, found_match, num_mv_found,
                                        ref_mv_stack);
    }
    return;
  }
  for (int i = 0; i < 2; ++i) {
    if (mv_bp.reference_frame[i] == block.bp->reference_frame[0]) {
      SearchStack</*is_compound=*/false>(block, mv_bp, i, weight, global_mv,
                                         found_new_mv, found_match,
                                         num_mv_found, ref_mv_stack);
    }
  }
}

int GetMinimumStep(int block_width_or_height4x4, int delta_row_or_column) {
  if (block_width_or_height4x4 >= 16) return 4;
  if (std::abs(delta_row_or_column) > 1) return 2;
  return 0;
}

// 7.10.2.2.
void ScanRow(const Tile::Block& block, int delta_row, bool is_compound,
             MotionVector global_mv[2], bool* const found_new_mv,
             bool* const found_match, int* const num_mv_found,
             CandidateMotionVector ref_mv_stack[kMaxRefMvStackSize]) {
  int delta_column = 0;
  if (std::abs(delta_row) > 1) {
    delta_row += block.row4x4 & 1;
    delta_column = 1 - (block.column4x4 & 1);
  }
  const int end_mv_column =
      block.column4x4 + delta_column +
      std::min({static_cast<int>(block.width4x4),
                block.tile.frame_header().columns4x4 - block.column4x4, 16});
  const int mv_row = block.row4x4 + delta_row;
  const int min_step = GetMinimumStep(block.width4x4, delta_row);
  for (int step, mv_column = block.column4x4 + delta_column;
       mv_column < end_mv_column; mv_column += step) {
    if (!block.tile.IsInside(mv_row, mv_column)) break;
    const BlockParameters& mv_bp = block.tile.Parameters(mv_row, mv_column);
    step = std::max(static_cast<int>(std::min(block.width4x4,
                                              kNum4x4BlocksWide[mv_bp.size])),
                    min_step);
    AddReferenceMvCandidate(block, mv_bp, is_compound, MultiplyBy2(step),
                            global_mv, found_new_mv, found_match, num_mv_found,
                            ref_mv_stack);
  }
}

// 7.10.2.3.
void ScanColumn(const Tile::Block& block, int delta_column, bool is_compound,
                MotionVector global_mv[2], bool* const found_new_mv,
                bool* const found_match, int* const num_mv_found,
                CandidateMotionVector ref_mv_stack[kMaxRefMvStackSize]) {
  int delta_row = 0;
  if (std::abs(delta_column) > 1) {
    delta_row = 1 - (block.row4x4 & 1);
    delta_column += block.column4x4 & 1;
  }
  const int end_mv_row =
      block.row4x4 + delta_row +
      std::min({static_cast<int>(block.height4x4),
                block.tile.frame_header().rows4x4 - block.row4x4, 16});
  const int mv_column = block.column4x4 + delta_column;
  const int min_step = GetMinimumStep(block.height4x4, delta_column);
  for (int step, mv_row = block.row4x4 + delta_row; mv_row < end_mv_row;
       mv_row += step) {
    if (!block.tile.IsInside(mv_row, mv_column)) break;
    const BlockParameters& mv_bp = block.tile.Parameters(mv_row, mv_column);
    step = std::max(static_cast<int>(std::min(block.height4x4,
                                              kNum4x4BlocksHigh[mv_bp.size])),
                    min_step);
    AddReferenceMvCandidate(block, mv_bp, is_compound, MultiplyBy2(step),
                            global_mv, found_new_mv, found_match, num_mv_found,
                            ref_mv_stack);
  }
}

// 7.10.2.4.
void ScanPoint(const Tile::Block& block, int delta_row, int delta_column,
               bool is_compound, MotionVector global_mv[2],
               bool* const found_new_mv, bool* const found_match,
               int* const num_mv_found,
               CandidateMotionVector ref_mv_stack[kMaxRefMvStackSize]) {
  const int mv_row = block.row4x4 + delta_row;
  const int mv_column = block.column4x4 + delta_column;
  if (!block.tile.IsInside(mv_row, mv_column) ||
      !block.tile.HasParameters(mv_row, mv_column)) {
    return;
  }
  const BlockParameters& mv_bp = block.tile.Parameters(mv_row, mv_column);
  if (mv_bp.reference_frame[0] == kReferenceFrameNone) return;
  AddReferenceMvCandidate(block, mv_bp, is_compound, 4, global_mv, found_new_mv,
                          found_match, num_mv_found, ref_mv_stack);
}

// 7.10.2.6.
//
// The |zero_mv_context| output parameter may be null. If |zero_mv_context| is
// not null, the function may set |*zero_mv_context|.
void AddTemporalReferenceMvCandidate(
    const Tile::Block& block, int delta_row, int delta_column, bool is_compound,
    MotionVector global_mv[2], const TemporalMotionField& motion_field,
    int* const zero_mv_context, int* const num_mv_found,
    CandidateMotionVector ref_mv_stack[kMaxRefMvStackSize]) {
  const int mv_row = (block.row4x4 + delta_row) | 1;
  const int mv_column = (block.column4x4 + delta_column) | 1;
  if (!block.tile.IsInside(mv_row, mv_column)) return;
  const int x8 = mv_column >> 1;
  const int y8 = mv_row >> 1;
  if (zero_mv_context != nullptr && delta_row == 0 && delta_column == 0) {
    *zero_mv_context = 1;
  }
  const BlockParameters& bp = *block.bp;
  const MotionVector& temporal_mv = motion_field.mv[y8][x8];
  if (temporal_mv.mv[0] == kInvalidMvValue) return;
  const int temporal_reference_offset = motion_field.reference_offset[y8][x8];
  assert(temporal_reference_offset > 0);
  assert(temporal_reference_offset <= kMaxFrameDistance);
  if (is_compound) {
    MotionVector candidate_mv[2] = {};
    for (int i = 0; i < 2; ++i) {
      const int reference_offset = GetRelativeDistance(
          block.tile.frame_header().order_hint,
          block.tile.current_frame().order_hint(bp.reference_frame[i]),
          block.tile.sequence_header().order_hint_range);
      if (reference_offset != 0) {
        GetMvProjection(temporal_mv, reference_offset,
                        temporal_reference_offset, &candidate_mv[i]);
        LowerMvPrecision(block, candidate_mv[i].mv);
      }
    }
    if (zero_mv_context != nullptr && delta_row == 0 && delta_column == 0) {
      *zero_mv_context = static_cast<int>(
          std::abs(candidate_mv[0].mv[0] - global_mv[0].mv[0]) >= 16 ||
          std::abs(candidate_mv[0].mv[1] - global_mv[0].mv[1]) >= 16 ||
          std::abs(candidate_mv[1].mv[0] - global_mv[1].mv[0]) >= 16 ||
          std::abs(candidate_mv[1].mv[1] - global_mv[1].mv[1]) >= 16);
    }
    auto result =
        std::find_if(ref_mv_stack, ref_mv_stack + *num_mv_found,
                     [&candidate_mv](const CandidateMotionVector& ref_mv) {
                       return ref_mv.mv[0] == candidate_mv[0] &&
                              ref_mv.mv[1] == candidate_mv[1];
                     });
    if (result != ref_mv_stack + *num_mv_found) {
      ref_mv_stack[std::distance(ref_mv_stack, result)].weight += 2;
      return;
    }
    if (*num_mv_found >= kMaxRefMvStackSize) return;
    ref_mv_stack[*num_mv_found] = {{candidate_mv[0], candidate_mv[1]}, 2};
    ++*num_mv_found;
    return;
  }
  assert(!is_compound);
  MotionVector candidate_mv = {};
  const int reference_offset = GetRelativeDistance(
      block.tile.frame_header().order_hint,
      block.tile.current_frame().order_hint(bp.reference_frame[0]),
      block.tile.sequence_header().order_hint_range);
  if (reference_offset != 0) {
    GetMvProjection(temporal_mv, reference_offset, temporal_reference_offset,
                    &candidate_mv);
    LowerMvPrecision(block, candidate_mv.mv);
  }
  if (zero_mv_context != nullptr && delta_row == 0 && delta_column == 0) {
    *zero_mv_context = static_cast<int>(
        std::abs(candidate_mv.mv[0] - global_mv[0].mv[0]) >= 16 ||
        std::abs(candidate_mv.mv[1] - global_mv[0].mv[1]) >= 16);
  }
  auto result =
      std::find_if(ref_mv_stack, ref_mv_stack + *num_mv_found,
                   [&candidate_mv](const CandidateMotionVector& ref_mv) {
                     return ref_mv.mv[0] == candidate_mv;
                   });
  if (result != ref_mv_stack + *num_mv_found) {
    ref_mv_stack[std::distance(ref_mv_stack, result)].weight += 2;
    return;
  }
  if (*num_mv_found >= kMaxRefMvStackSize) return;
  ref_mv_stack[*num_mv_found] = {{candidate_mv, {}}, 2};
  ++*num_mv_found;
}

// Part of 7.10.2.5.
bool IsWithinTheSame64x64Block(const Tile::Block& block, int delta_row,
                               int delta_column) {
  const int row = (block.row4x4 & 15) + delta_row;
  const int column = (block.column4x4 & 15) + delta_column;
  return row >= 0 && row < 16 && column >= 0 && column < 16;
}

constexpr BitMaskSet kTemporalScanMask(kBlock8x8, kBlock8x16, kBlock8x32,
                                       kBlock16x8, kBlock16x16, kBlock16x32,
                                       kBlock32x8, kBlock32x16, kBlock32x32);

// 7.10.2.5.
//
// The |zero_mv_context| output parameter may be null. If |zero_mv_context| is
// not null, the function may set |*zero_mv_context|.
void TemporalScan(const Tile::Block& block, bool is_compound,
                  MotionVector global_mv[2],
                  const TemporalMotionField& motion_field,
                  int* const zero_mv_context, int* const num_mv_found,
                  CandidateMotionVector ref_mv_stack[kMaxRefMvStackSize]) {
  const int step_w = (block.width4x4 >= 16) ? 4 : 2;
  const int step_h = (block.height4x4 >= 16) ? 4 : 2;
  for (int row = 0; row < std::min(static_cast<int>(block.height4x4), 16);
       row += step_h) {
    for (int column = 0;
         column < std::min(static_cast<int>(block.width4x4), 16);
         column += step_w) {
      AddTemporalReferenceMvCandidate(block, row, column, is_compound,
                                      global_mv, motion_field, zero_mv_context,
                                      num_mv_found, ref_mv_stack);
    }
  }
  if (kTemporalScanMask.Contains(block.size)) {
    const int temporal_sample_positions[3][2] = {
        {block.height4x4, -2},
        {block.height4x4, block.width4x4},
        {block.height4x4 - 2, block.width4x4}};
    for (const auto& temporal_sample_position : temporal_sample_positions) {
      const int row = temporal_sample_position[0];
      const int column = temporal_sample_position[1];
      if (!IsWithinTheSame64x64Block(block, row, column)) continue;
      AddTemporalReferenceMvCandidate(block, row, column, is_compound,
                                      global_mv, motion_field, zero_mv_context,
                                      num_mv_found, ref_mv_stack);
    }
  }
}

// Part of 7.10.2.13.
void AddExtraCompoundMvCandidate(
    const Tile::Block& block, int mv_row, int mv_column,
    const std::array<bool, kNumReferenceFrameTypes>& reference_frame_sign_bias,
    int* const ref_id_count, MotionVector ref_id[2][2],
    int* const ref_diff_count, MotionVector ref_diff[2][2]) {
  const auto& bp = block.tile.Parameters(mv_row, mv_column);
  for (int i = 0; i < 2; ++i) {
    const ReferenceFrameType candidate_reference_frame = bp.reference_frame[i];
    if (candidate_reference_frame <= kReferenceFrameIntra) continue;
    for (int j = 0; j < 2; ++j) {
      MotionVector candidate_mv = bp.mv[i];
      const ReferenceFrameType block_reference_frame =
          block.bp->reference_frame[j];
      if (candidate_reference_frame == block_reference_frame &&
          ref_id_count[j] < 2) {
        ref_id[j][ref_id_count[j]] = candidate_mv;
        ++ref_id_count[j];
      } else if (ref_diff_count[j] < 2) {
        if (reference_frame_sign_bias[candidate_reference_frame] !=
            reference_frame_sign_bias[block_reference_frame]) {
          candidate_mv.mv[0] *= -1;
          candidate_mv.mv[1] *= -1;
        }
        ref_diff[j][ref_diff_count[j]] = candidate_mv;
        ++ref_diff_count[j];
      }
    }
  }
}

// Part of 7.10.2.13.
void AddExtraSingleMvCandidate(
    const Tile::Block& block, int mv_row, int mv_column,
    const std::array<bool, kNumReferenceFrameTypes>& reference_frame_sign_bias,
    int* const num_mv_found,
    CandidateMotionVector ref_mv_stack[kMaxRefMvStackSize]) {
  const auto& bp = block.tile.Parameters(mv_row, mv_column);
  const ReferenceFrameType block_reference_frame = block.bp->reference_frame[0];
  for (int i = 0; i < 2; ++i) {
    const ReferenceFrameType candidate_reference_frame = bp.reference_frame[i];
    if (candidate_reference_frame <= kReferenceFrameIntra) continue;
    MotionVector candidate_mv = bp.mv[i];
    if (reference_frame_sign_bias[candidate_reference_frame] !=
        reference_frame_sign_bias[block_reference_frame]) {
      candidate_mv.mv[0] *= -1;
      candidate_mv.mv[1] *= -1;
    }
    assert(*num_mv_found <= 2);
    if ((*num_mv_found == 1 && ref_mv_stack[0].mv[0] == candidate_mv) ||
        (*num_mv_found == 2 && (ref_mv_stack[0].mv[0] == candidate_mv ||
                                ref_mv_stack[1].mv[0] == candidate_mv))) {
      continue;
    }
    ref_mv_stack[*num_mv_found] = {{candidate_mv, {}}, 2};
    ++*num_mv_found;
  }
}

// 7.10.2.12.
void ExtraSearch(
    const Tile::Block& block, bool is_compound, MotionVector global_mv[2],
    const std::array<bool, kNumReferenceFrameTypes>& reference_frame_sign_bias,
    int* const num_mv_found,
    CandidateMotionVector ref_mv_stack[kMaxRefMvStackSize]) {
  const int num4x4 =
      std::min({static_cast<int>(block.width4x4),
                block.tile.frame_header().columns4x4 - block.column4x4,
                static_cast<int>(block.height4x4),
                block.tile.frame_header().rows4x4 - block.row4x4, 16});
  int ref_id_count[2] = {};
  MotionVector ref_id[2][2] = {};
  int ref_diff_count[2] = {};
  MotionVector ref_diff[2][2] = {};
  for (int pass = 0; pass < 2 && *num_mv_found < 2; ++pass) {
    for (int i = 0; i < num4x4;) {
      const int mv_row = block.row4x4 + ((pass == 0) ? -1 : i);
      const int mv_column = block.column4x4 + ((pass == 0) ? i : -1);
      if (!block.tile.IsInside(mv_row, mv_column)) break;
      if (is_compound) {
        AddExtraCompoundMvCandidate(block, mv_row, mv_column,
                                    reference_frame_sign_bias, ref_id_count,
                                    ref_id, ref_diff_count, ref_diff);
      } else {
        AddExtraSingleMvCandidate(block, mv_row, mv_column,
                                  reference_frame_sign_bias, num_mv_found,
                                  ref_mv_stack);
        if (*num_mv_found >= 2) break;
      }
      const auto& bp = block.tile.Parameters(mv_row, mv_column);
      i +=
          (pass == 0) ? kNum4x4BlocksWide[bp.size] : kNum4x4BlocksHigh[bp.size];
    }
  }
  if (is_compound) {
    // Merge compound mode extra search into mv stack.
    MotionVector combined_mvs[2][2] = {};
    for (int i = 0; i < 2; ++i) {
      int count = 0;
      assert(ref_id_count[i] <= 2);
      for (int j = 0; j < ref_id_count[i]; ++j, ++count) {
        combined_mvs[count][i] = ref_id[i][j];
      }
      for (int j = 0; j < ref_diff_count[i] && count < 2; ++j, ++count) {
        combined_mvs[count][i] = ref_diff[i][j];
      }
      for (; count < 2; ++count) {
        combined_mvs[count][i] = global_mv[i];
      }
    }
    if (*num_mv_found == 1) {
      if (combined_mvs[0][0] == ref_mv_stack[0].mv[0] &&
          combined_mvs[0][1] == ref_mv_stack[0].mv[1]) {
        ref_mv_stack[1] = {{combined_mvs[1][0], combined_mvs[1][1]}, 2};
      } else {
        ref_mv_stack[1] = {{combined_mvs[0][0], combined_mvs[0][1]}, 2};
      }
      ++*num_mv_found;
    } else {
      assert(*num_mv_found == 0);
      *num_mv_found = 2;
      for (int i = 0; i < 2; ++i) {
        ref_mv_stack[i] = {{combined_mvs[i][0], combined_mvs[i][1]}, 2};
      }
    }
  } else {
    // single prediction mode
    for (int i = *num_mv_found; i < 2; ++i) {
      ref_mv_stack[i].mv[0] = global_mv[0];
    }
  }
}

// 7.10.2.14 (part 1). (also contains implementations of 5.11.53 and 5.11.54).
void ClampMotionVectors(
    const Tile::Block& block, bool is_compound, int num_mv_found,
    CandidateMotionVector ref_mv_stack[kMaxRefMvStackSize]) {
  const int row_border = kMvBorder + MultiplyBy32(block.height4x4);
  const int column_border = kMvBorder + MultiplyBy32(block.width4x4);
  const int macroblocks_to_top_edge = -MultiplyBy32(block.row4x4);
  const int macroblocks_to_bottom_edge = MultiplyBy32(
      block.tile.frame_header().rows4x4 - block.height4x4 - block.row4x4);
  const int macroblocks_to_left_edge = -MultiplyBy32(block.column4x4);
  const int macroblocks_to_right_edge = MultiplyBy32(
      block.tile.frame_header().columns4x4 - block.width4x4 - block.column4x4);
  const int min[2] = {macroblocks_to_top_edge - row_border,
                      macroblocks_to_left_edge - column_border};
  const int max[2] = {macroblocks_to_bottom_edge + row_border,
                      macroblocks_to_right_edge + column_border};
  for (int i = 0; i < num_mv_found; ++i) {
    for (int mv_index = 0; mv_index < 1 + static_cast<int>(is_compound);
         ++mv_index) {
      for (int j = 0; j < 2; ++j) {
        ref_mv_stack[i].mv[mv_index].mv[j] =
            Clip3(ref_mv_stack[i].mv[mv_index].mv[j], min[j], max[j]);
      }
    }
  }
}

// 7.10.2.14 (part 2).
void ComputeContexts(bool found_new_mv, int nearest_matches, int total_matches,
                     int* new_mv_context, int* reference_mv_context) {
  switch (nearest_matches) {
    case 0:
      *new_mv_context = std::min(total_matches, 1);
      *reference_mv_context = total_matches;
      break;
    case 1:
      *new_mv_context = 3 - static_cast<int>(found_new_mv);
      *reference_mv_context = 2 + total_matches;
      break;
    default:
      *new_mv_context = 5 - static_cast<int>(found_new_mv);
      *reference_mv_context = 5;
      break;
  }
}

// 7.10.4.2.
void AddSample(const Tile::Block& block, int delta_row, int delta_column,
               int* const num_warp_samples, int* const num_samples_scanned,
               int candidates[kMaxLeastSquaresSamples][4]) {
  if (*num_samples_scanned >= kMaxLeastSquaresSamples) return;
  const int mv_row = block.row4x4 + delta_row;
  const int mv_column = block.column4x4 + delta_column;
  if (!block.tile.IsInside(mv_row, mv_column) ||
      !block.tile.HasParameters(mv_row, mv_column)) {
    return;
  }
  const BlockParameters& bp = block.tile.Parameters(mv_row, mv_column);
  if (bp.reference_frame[0] != block.bp->reference_frame[0] ||
      bp.reference_frame[1] != kReferenceFrameNone) {
    return;
  }
  ++*num_samples_scanned;
  const int candidate_height4x4 = kNum4x4BlocksHigh[bp.size];
  const int candidate_row = mv_row & ~(candidate_height4x4 - 1);
  const int candidate_width4x4 = kNum4x4BlocksWide[bp.size];
  const int candidate_column = mv_column & ~(candidate_width4x4 - 1);
  const BlockParameters& candidate_bp =
      block.tile.Parameters(candidate_row, candidate_column);
  const int mv_diff_row =
      std::abs(candidate_bp.mv[0].mv[0] - block.bp->mv[0].mv[0]);
  const int mv_diff_column =
      std::abs(candidate_bp.mv[0].mv[1] - block.bp->mv[0].mv[1]);
  const bool is_valid =
      mv_diff_row + mv_diff_column <= kWarpValidThreshold[block.size];
  if (!is_valid && *num_samples_scanned > 1) {
    return;
  }
  const int mid_y =
      MultiplyBy4(candidate_row) + MultiplyBy2(candidate_height4x4) - 1;
  const int mid_x =
      MultiplyBy4(candidate_column) + MultiplyBy2(candidate_width4x4) - 1;
  candidates[*num_warp_samples][0] = MultiplyBy8(mid_y);
  candidates[*num_warp_samples][1] = MultiplyBy8(mid_x);
  candidates[*num_warp_samples][2] =
      MultiplyBy8(mid_y) + candidate_bp.mv[0].mv[0];
  candidates[*num_warp_samples][3] =
      MultiplyBy8(mid_x) + candidate_bp.mv[0].mv[1];
  if (is_valid) ++*num_warp_samples;
}

// Comparator used for sorting candidate motion vectors in descending order of
// their weights (as specified in 7.10.2.11).
bool CompareCandidateMotionVectors(const CandidateMotionVector& lhs,
                                   const CandidateMotionVector& rhs) {
  return lhs.weight > rhs.weight;
}

// 7.9.4.
constexpr int Project(const int value, const int delta, const int dst_sign) {
  return value + ApplySign(delta / 64, dst_sign);
}

// 7.9.2.
bool MotionFieldProjection(
    const ObuFrameHeader& frame_header, const RefCountedBuffer& current_frame,
    const std::array<RefCountedBufferPtr, kNumReferenceFrameTypes>&
        reference_frames,
    ReferenceFrameType source, const unsigned int order_hint_range,
    const int reference_to_current_with_sign, const int dst_sign,
    const int y8_start, const int y8_end, const int x8_start, const int x8_end,
    TemporalMotionField* const motion_field) {
  const int source_index =
      frame_header.reference_frame_index[source - kReferenceFrameLast];
  auto* const source_frame = reference_frames[source_index].get();
  assert(source_frame != nullptr);
  if (source_frame->rows4x4() != frame_header.rows4x4 ||
      source_frame->columns4x4() != frame_header.columns4x4 ||
      IsIntraFrame(source_frame->frame_type())) {
    return false;
  }
  assert(reference_to_current_with_sign >= -kMaxFrameDistance);
  if (reference_to_current_with_sign > kMaxFrameDistance) return true;
  const ReferenceFrameType* source_reference_type =
      source_frame->motion_field_reference_frame(y8_start, 0);
  const MotionVector* mv = source_frame->motion_field_mv(y8_start, 0);
  const ptrdiff_t stride = motion_field->mv.columns();
  // The column range has to be offset by kProjectionMvMaxHorizontalOffset since
  // coordinates in that range could end up being position_x8 because of
  // projection.
  const int adjusted_x8_start =
      std::max(x8_start - kProjectionMvMaxHorizontalOffset, 0);
  const int adjusted_x8_end = std::min(
      x8_end + kProjectionMvMaxHorizontalOffset, static_cast<int>(stride));
  int8_t* dst_reference_offset = motion_field->reference_offset[y8_start];
  MotionVector* dst_mv = motion_field->mv[y8_start];
  int8_t reference_offsets[kNumReferenceFrameTypes];
  bool skip_reference[kNumReferenceFrameTypes];
  assert((y8_start & 7) == 0);

  // Initialize skip_reference[kReferenceFrameIntra] to simplify branch
  // conditions in projection.
  skip_reference[kReferenceFrameIntra] = true;
  for (int reference_type = kReferenceFrameLast;
       reference_type <= kNumInterReferenceFrameTypes; ++reference_type) {
    const int reference_offset = GetRelativeDistance(
        current_frame.order_hint(source),
        source_frame->order_hint(
            static_cast<ReferenceFrameType>(reference_type)),
        order_hint_range);
    skip_reference[reference_type] =
        reference_offset > kMaxFrameDistance || reference_offset <= 0;
    reference_offsets[reference_type] = reference_offset;
  }

  int y8 = y8_start;
  do {
    const int y8_floor = (y8 & ~7) - y8;
    const int y8_ceiling = std::min(y8_end - y8, y8_floor + 8);
    int x8 = adjusted_x8_start;
    do {
      if (skip_reference[source_reference_type[x8]]) continue;
      const int reference_offset = reference_offsets[source_reference_type[x8]];
      MotionVector projection_mv;
      // reference_to_current_with_sign could be 0.
      GetMvProjectionNoClamp(mv[x8], reference_to_current_with_sign,
                             reference_offset, &projection_mv);
      // Do not update the motion vector if the block position is not valid or
      // if position_x8 is outside the current range of x8_start and x8_end.
      // Note that position_y8 will always be within the range of y8_start and
      // y8_end.
      const int position_y8 = Project(0, projection_mv.mv[0], dst_sign);
      if (position_y8 < y8_floor || position_y8 >= y8_ceiling) continue;
      const int x8_base = x8 & ~7;
      const int x8_floor =
          std::max(x8_start, x8_base - kProjectionMvMaxHorizontalOffset);
      const int x8_ceiling =
          std::min(x8_end, x8_base + 8 + kProjectionMvMaxHorizontalOffset);
      const int position_x8 = Project(x8, projection_mv.mv[1], dst_sign);
      if (position_x8 < x8_floor || position_x8 >= x8_ceiling) continue;
      dst_mv[position_y8 * stride + position_x8] = mv[x8];
      dst_reference_offset[position_y8 * stride + position_x8] =
          reference_offset;
    } while (++x8 < adjusted_x8_end);
    source_reference_type += stride;
    mv += stride;
    dst_reference_offset += stride;
    dst_mv += stride;
  } while (++y8 < y8_end);
  return true;
}

}  // namespace

void FindMvStack(
    const Tile::Block& block, bool is_compound,
    const std::array<bool, kNumReferenceFrameTypes>& reference_frame_sign_bias,
    const TemporalMotionField& motion_field,
    CandidateMotionVector ref_mv_stack[kMaxRefMvStackSize],
    int* const num_mv_found, MvContexts* const contexts,
    MotionVector global_mv[2]) {
  SetupGlobalMv(block, 0, &global_mv[0]);
  if (is_compound) SetupGlobalMv(block, 1, &global_mv[1]);
  bool found_new_mv = false;
  bool found_row_match = false;
  *num_mv_found = 0;
  ScanRow(block, -1, is_compound, global_mv, &found_new_mv, &found_row_match,
          num_mv_found, ref_mv_stack);
  bool found_column_match = false;
  ScanColumn(block, -1, is_compound, global_mv, &found_new_mv,
             &found_column_match, num_mv_found, ref_mv_stack);
  if (std::max(block.width4x4, block.height4x4) <= 16) {
    ScanPoint(block, -1, block.width4x4, is_compound, global_mv, &found_new_mv,
              &found_row_match, num_mv_found, ref_mv_stack);
  }
  const int nearest_matches =
      static_cast<int>(found_row_match) + static_cast<int>(found_column_match);
  const int nearest_mv_count = *num_mv_found;
  for (int i = 0; i < nearest_mv_count; ++i) {
    ref_mv_stack[i].weight += kExtraWeightForNearestMvs;
  }
  if (contexts != nullptr) contexts->zero_mv = 0;
  if (block.tile.frame_header().use_ref_frame_mvs) {
    TemporalScan(block, is_compound, global_mv, motion_field,
                 (contexts != nullptr) ? &contexts->zero_mv : nullptr,
                 num_mv_found, ref_mv_stack);
  }
  bool dummy_bool = false;
  ScanPoint(block, -1, -1, is_compound, global_mv, &dummy_bool,
            &found_row_match, num_mv_found, ref_mv_stack);
  static constexpr int deltas[2] = {-3, -5};
  for (int i = 0; i < 2; ++i) {
    if (i == 0 || block.height4x4 > 1) {
      ScanRow(block, deltas[i], is_compound, global_mv, &dummy_bool,
              &found_row_match, num_mv_found, ref_mv_stack);
    }
    if (i == 0 || block.width4x4 > 1) {
      ScanColumn(block, deltas[i], is_compound, global_mv, &dummy_bool,
                 &found_column_match, num_mv_found, ref_mv_stack);
    }
  }
  std::stable_sort(&ref_mv_stack[0], &ref_mv_stack[nearest_mv_count],
                   CompareCandidateMotionVectors);
  std::stable_sort(&ref_mv_stack[nearest_mv_count],
                   &ref_mv_stack[*num_mv_found], CompareCandidateMotionVectors);
  if (*num_mv_found < 2) {
    ExtraSearch(block, is_compound, global_mv, reference_frame_sign_bias,
                num_mv_found, ref_mv_stack);
  }
  const int total_matches =
      static_cast<int>(found_row_match) + static_cast<int>(found_column_match);
  if (contexts != nullptr) {
    ComputeContexts(found_new_mv, nearest_matches, total_matches,
                    &contexts->new_mv, &contexts->reference_mv);
  }
  ClampMotionVectors(block, is_compound, *num_mv_found, ref_mv_stack);
}

void FindWarpSamples(const Tile::Block& block, int* const num_warp_samples,
                     int* const num_samples_scanned,
                     int candidates[kMaxLeastSquaresSamples][4]) {
  bool top_left = true;
  bool top_right = true;
  int step = 1;
  if (block.top_available) {
    BlockSize source_size =
        block.tile.Parameters(block.row4x4 - 1, block.column4x4).size;
    const int source_width4x4 = kNum4x4BlocksWide[source_size];
    if (block.width4x4 <= source_width4x4) {
      // The & here is equivalent to % since source_width4x4 is a power of
      // two.
      const int column_offset = -(block.column4x4 & (source_width4x4 - 1));
      if (column_offset < 0) top_left = false;
      if (column_offset + source_width4x4 > block.width4x4) top_right = false;
      AddSample(block, -1, 0, num_warp_samples, num_samples_scanned,
                candidates);
    } else {
      for (int i = 0;
           i < std::min(static_cast<int>(block.width4x4),
                        block.tile.frame_header().columns4x4 - block.column4x4);
           i += step) {
        source_size =
            block.tile.Parameters(block.row4x4 - 1, block.column4x4 + i).size;
        step = std::min(static_cast<int>(block.width4x4),
                        static_cast<int>(kNum4x4BlocksWide[source_size]));
        AddSample(block, -1, i, num_warp_samples, num_samples_scanned,
                  candidates);
      }
    }
  }
  if (block.left_available) {
    BlockSize source_size =
        block.tile.Parameters(block.row4x4, block.column4x4 - 1).size;
    const int source_height4x4 = kNum4x4BlocksHigh[source_size];
    if (block.height4x4 <= source_height4x4) {
      const int row_offset = -(block.row4x4 & (source_height4x4 - 1));
      if (row_offset < 0) top_left = false;
      AddSample(block, 0, -1, num_warp_samples, num_samples_scanned,
                candidates);
    } else {
      for (int i = 0;
           i < std::min(static_cast<int>(block.height4x4),
                        block.tile.frame_header().rows4x4 - block.row4x4);
           i += step) {
        source_size =
            block.tile.Parameters(block.row4x4 + i, block.column4x4 - 1).size;
        step = std::min(static_cast<int>(block.height4x4),
                        static_cast<int>(kNum4x4BlocksHigh[source_size]));
        AddSample(block, i, -1, num_warp_samples, num_samples_scanned,
                  candidates);
      }
    }
  }
  if (top_left) {
    AddSample(block, -1, -1, num_warp_samples, num_samples_scanned, candidates);
  }
  if (top_right && block.size <= kBlock64x64) {
    AddSample(block, -1, block.width4x4, num_warp_samples, num_samples_scanned,
              candidates);
  }
  if (*num_warp_samples == 0 && *num_samples_scanned > 0) *num_warp_samples = 1;
}

void SetupMotionField(
    const ObuFrameHeader& frame_header, const RefCountedBuffer& current_frame,
    const std::array<RefCountedBufferPtr, kNumReferenceFrameTypes>&
        reference_frames,
    const unsigned int order_hint_range, const int row4x4_start,
    const int row4x4_end, const int column4x4_start, const int column4x4_end,
    TemporalMotionField* const motion_field) {
  assert(frame_header.use_ref_frame_mvs);
  assert(order_hint_range != 0);
  const int y8_start = DivideBy2(row4x4_start);
  const int y8_end = DivideBy2(std::min(row4x4_end, frame_header.rows4x4));
  const int x8_start = DivideBy2(column4x4_start);
  const int x8_end =
      DivideBy2(std::min(column4x4_end, frame_header.columns4x4));
  const int8_t* const reference_frame_index =
      frame_header.reference_frame_index;
  const int last_index = reference_frame_index[0];
  const int last_alternate_order_hint =
      reference_frames[last_index]->order_hint(kReferenceFrameAlternate);
  const int current_gold_order_hint =
      current_frame.order_hint(kReferenceFrameGolden);
  if (last_alternate_order_hint != current_gold_order_hint) {
    const int reference_offset_last =
        -GetRelativeDistance(current_frame.order_hint(kReferenceFrameLast),
                             frame_header.order_hint, order_hint_range);
    if (std::abs(reference_offset_last) <= kMaxFrameDistance) {
      MotionFieldProjection(frame_header, current_frame, reference_frames,
                            kReferenceFrameLast, order_hint_range,
                            reference_offset_last, -1, y8_start, y8_end,
                            x8_start, x8_end, motion_field);
    }
  }
  int ref_stamp = 1;
  const int reference_offset_backward =
      GetRelativeDistance(current_frame.order_hint(kReferenceFrameBackward),
                          frame_header.order_hint, order_hint_range);
  if (reference_offset_backward > 0 &&
      MotionFieldProjection(frame_header, current_frame, reference_frames,
                            kReferenceFrameBackward, order_hint_range,
                            reference_offset_backward, 0, y8_start, y8_end,
                            x8_start, x8_end, motion_field)) {
    --ref_stamp;
  }
  const int reference_offset_alternate2 =
      GetRelativeDistance(current_frame.order_hint(kReferenceFrameAlternate2),
                          frame_header.order_hint, order_hint_range);
  if (reference_offset_alternate2 > 0 &&
      MotionFieldProjection(frame_header, current_frame, reference_frames,
                            kReferenceFrameAlternate2, order_hint_range,
                            reference_offset_alternate2, 0, y8_start, y8_end,
                            x8_start, x8_end, motion_field)) {
    --ref_stamp;
  }
  if (ref_stamp >= 0) {
    const int reference_offset_alternate =
        GetRelativeDistance(current_frame.order_hint(kReferenceFrameAlternate),
                            frame_header.order_hint, order_hint_range);
    if (reference_offset_alternate > 0 &&
        MotionFieldProjection(frame_header, current_frame, reference_frames,
                              kReferenceFrameAlternate, order_hint_range,
                              reference_offset_alternate, 0, y8_start, y8_end,
                              x8_start, x8_end, motion_field)) {
      --ref_stamp;
    }
  }
  if (ref_stamp >= 0) {
    const int reference_offset_last2 =
        -GetRelativeDistance(current_frame.order_hint(kReferenceFrameLast2),
                             frame_header.order_hint, order_hint_range);
    if (std::abs(reference_offset_last2) <= kMaxFrameDistance) {
      MotionFieldProjection(frame_header, current_frame, reference_frames,
                            kReferenceFrameLast2, order_hint_range,
                            reference_offset_last2, -1, y8_start, y8_end,
                            x8_start, x8_end, motion_field);
    }
  }
}

}  // namespace libgav1
