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

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "src/buffer_pool.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/motion_vector.h"
#include "src/obu_parser.h"
#include "src/prediction_mask.h"
#include "src/tile.h"
#include "src/utils/array_2d.h"
#include "src/utils/bit_mask_set.h"
#include "src/utils/block_parameters_holder.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"
#include "src/utils/logging.h"
#include "src/utils/memory.h"
#include "src/utils/types.h"
#include "src/warp_prediction.h"
#include "src/yuv_buffer.h"

namespace libgav1 {
namespace {

// Import all the constants in the anonymous namespace.
#include "src/inter_intra_masks.inc"

constexpr int kAngleStep = 3;
constexpr int kPredictionModeToAngle[kIntraPredictionModesUV] = {
    0, 90, 180, 45, 135, 113, 157, 203, 67, 0, 0, 0, 0};

enum : uint8_t {
  kNeedsLeft = 1,
  kNeedsTop = 2,
};

// The values for directional and dc modes are not used since the left/top
// requirement for those modes depend on the prediction angle and the type of dc
// mode.
constexpr BitMaskSet kPredictionModeNeedsMask[kIntraPredictionModesY] = {
    BitMaskSet(0),                      // kPredictionModeDc
    BitMaskSet(kNeedsTop),              // kPredictionModeVertical
    BitMaskSet(kNeedsLeft),             // kPredictionModeHorizontal
    BitMaskSet(kNeedsTop),              // kPredictionModeD45
    BitMaskSet(kNeedsLeft, kNeedsTop),  // kPredictionModeD135
    BitMaskSet(kNeedsLeft, kNeedsTop),  // kPredictionModeD113
    BitMaskSet(kNeedsLeft, kNeedsTop),  // kPredictionModeD157
    BitMaskSet(kNeedsLeft),             // kPredictionModeD203
    BitMaskSet(kNeedsTop),              // kPredictionModeD67
    BitMaskSet(kNeedsLeft, kNeedsTop),  // kPredictionModeSmooth
    BitMaskSet(kNeedsLeft, kNeedsTop),  // kPredictionModeSmoothVertical
    BitMaskSet(kNeedsLeft, kNeedsTop),  // kPredictionModeSmoothHorizontal
    BitMaskSet(kNeedsLeft, kNeedsTop)   // kPredictionModePaeth
};

int16_t GetDirectionalIntraPredictorDerivative(const int angle) {
  assert(angle >= 3);
  assert(angle <= 87);
  return kDirectionalIntraPredictorDerivative[DivideBy2(angle) - 1];
}

// Maps the block_size to an index as follows:
//  kBlock8x8 => 0.
//  kBlock8x16 => 1.
//  kBlock8x32 => 2.
//  kBlock16x8 => 3.
//  kBlock16x16 => 4.
//  kBlock16x32 => 5.
//  kBlock32x8 => 6.
//  kBlock32x16 => 7.
//  kBlock32x32 => 8.
int GetWedgeBlockSizeIndex(BlockSize block_size) {
  assert(block_size >= kBlock8x8);
  return block_size - kBlock8x8 - static_cast<int>(block_size >= kBlock16x8) -
         static_cast<int>(block_size >= kBlock32x8);
}

// Maps a dimension of 4, 8, 16 and 32 to indices 0, 1, 2 and 3 respectively.
int GetInterIntraMaskLookupIndex(int dimension) {
  assert(dimension == 4 || dimension == 8 || dimension == 16 ||
         dimension == 32);
  return FloorLog2(dimension) - 2;
}

// 7.11.2.9.
int GetIntraEdgeFilterStrength(int width, int height, int filter_type,
                               int delta) {
  const int sum = width + height;
  delta = std::abs(delta);
  if (filter_type == 0) {
    if (sum <= 8) {
      if (delta >= 56) return 1;
    } else if (sum <= 16) {
      if (delta >= 40) return 1;
    } else if (sum <= 24) {
      if (delta >= 32) return 3;
      if (delta >= 16) return 2;
      if (delta >= 8) return 1;
    } else if (sum <= 32) {
      if (delta >= 32) return 3;
      if (delta >= 4) return 2;
      return 1;
    } else {
      return 3;
    }
  } else {
    if (sum <= 8) {
      if (delta >= 64) return 2;
      if (delta >= 40) return 1;
    } else if (sum <= 16) {
      if (delta >= 48) return 2;
      if (delta >= 20) return 1;
    } else if (sum <= 24) {
      if (delta >= 4) return 3;
    } else {
      return 3;
    }
  }
  return 0;
}

// 7.11.2.10.
bool DoIntraEdgeUpsampling(int width, int height, int filter_type, int delta) {
  const int sum = width + height;
  delta = std::abs(delta);
  if (delta == 0 || delta >= 40) return false;
  return (filter_type == 1) ? sum <= 8 : sum <= 16;
}

constexpr uint8_t kQuantizedDistanceWeight[4][2] = {
    {2, 3}, {2, 5}, {2, 7}, {1, kMaxFrameDistance}};

constexpr uint8_t kQuantizedDistanceLookup[4][2] = {
    {9, 7}, {11, 5}, {12, 4}, {13, 3}};

void GetDistanceWeights(const int distance[2], int weight[2]) {
  // Note: distance[0] and distance[1] correspond to relative distance
  // between current frame and reference frame [1] and [0], respectively.
  const int order = static_cast<int>(distance[0] <= distance[1]);
  if (distance[0] == 0 || distance[1] == 0) {
    weight[0] = kQuantizedDistanceLookup[3][order];
    weight[1] = kQuantizedDistanceLookup[3][1 - order];
  } else {
    int i;
    for (i = 0; i < 3; ++i) {
      const int weight_0 = kQuantizedDistanceWeight[i][order];
      const int weight_1 = kQuantizedDistanceWeight[i][1 - order];
      if (order == 0) {
        if (distance[0] * weight_0 < distance[1] * weight_1) break;
      } else {
        if (distance[0] * weight_0 > distance[1] * weight_1) break;
      }
    }
    weight[0] = kQuantizedDistanceLookup[i][order];
    weight[1] = kQuantizedDistanceLookup[i][1 - order];
  }
}

template <int bitdepth, typename Pixel>
void ClipPrediction(const uint16_t* prediction,
                    const ptrdiff_t prediction_stride, const int width,
                    const int height, uint8_t* clipped_prediction,
                    ptrdiff_t clipped_prediction_stride) {
  // An offset to cancel offsets used in compound predictor generation that
  // make intermediate computations non negative.
  const int single_round_offset = (1 << bitdepth) + (1 << (bitdepth - 1));
  auto* clipped_pred = reinterpret_cast<Pixel*>(clipped_prediction);
  clipped_prediction_stride /= sizeof(Pixel);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      clipped_pred[x] = static_cast<Pixel>(
          Clip3(prediction[x] - single_round_offset, 0, (1 << bitdepth) - 1));
    }
    prediction += prediction_stride;
    clipped_pred += clipped_prediction_stride;
  }
}

dsp::IntraPredictor GetIntraPredictor(PredictionMode mode, bool has_left,
                                      bool has_top) {
  if (mode == kPredictionModeDc) {
    if (has_left && has_top) {
      return dsp::kIntraPredictorDc;
    }
    if (has_left) {
      return dsp::kIntraPredictorDcLeft;
    }
    if (has_top) {
      return dsp::kIntraPredictorDcTop;
    }
    return dsp::kIntraPredictorDcFill;
  }
  switch (mode) {
    case kPredictionModePaeth:
      return dsp::kIntraPredictorPaeth;
    case kPredictionModeSmooth:
      return dsp::kIntraPredictorSmooth;
    case kPredictionModeSmoothVertical:
      return dsp::kIntraPredictorSmoothVertical;
    case kPredictionModeSmoothHorizontal:
      return dsp::kIntraPredictorSmoothHorizontal;
    default:
      return dsp::kNumIntraPredictors;
  }
}

// 7.11.3.2. Note InterRoundBits0 is derived in the dsp layer.
int GetInterRoundingBits(const bool is_compound, const int bitdepth) {
  if (is_compound) return 7;
#if LIBGAV1_MAX_BITDEPTH == 12
  if (bitdepth == 12) return 9;
#else
  static_cast<void>(bitdepth);
#endif
  return 11;
}

uint8_t* GetStartPoint(Array2DView<uint8_t>* const buffer, const int plane,
                       const int x, const int y, const int bitdepth) {
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth > 8) {
    Array2DView<uint16_t> buffer16(
        buffer[plane].rows(), buffer[plane].columns() / sizeof(uint16_t),
        reinterpret_cast<uint16_t*>(&buffer[plane][0][0]));
    return reinterpret_cast<uint8_t*>(&buffer16[y][x]);
  }
#endif  // LIBGAV1_MAX_BITDEPTH >= 10
  static_cast<void>(bitdepth);
  return &buffer[plane][y][x];
}

int GetPixelPositionFromHighScale(int start, int step, int offset) {
  return (start + step * offset) >> kScaleSubPixelBits;
}

dsp::MaskBlendFunc GetMaskBlendFunc(const dsp::Dsp& dsp,
                                    InterIntraMode inter_intra_mode,
                                    bool is_wedge_inter_intra,
                                    int subsampling_x, int subsampling_y) {
  const int is_inter_intra =
      static_cast<int>(inter_intra_mode != kNumInterIntraModes);
  return (is_inter_intra == 1 && !is_wedge_inter_intra)
             ? dsp.mask_blend[0][is_inter_intra]
             : dsp.mask_blend[subsampling_x + subsampling_y][is_inter_intra];
}

}  // namespace

template <typename Pixel>
void Tile::IntraPrediction(const Block& block, Plane plane, int x, int y,
                           bool has_left, bool has_top, bool has_top_right,
                           bool has_bottom_left, PredictionMode mode,
                           TransformSize tx_size) {
  const int width = 1 << kTransformWidthLog2[tx_size];
  const int height = 1 << kTransformHeightLog2[tx_size];
  const int x_shift = subsampling_x_[plane];
  const int y_shift = subsampling_y_[plane];
  const int max_x = (MultiplyBy4(frame_header_.columns4x4) >> x_shift) - 1;
  const int max_y = (MultiplyBy4(frame_header_.rows4x4) >> y_shift) - 1;
  alignas(kMaxAlignment) Pixel top_row_data[160] = {};
  alignas(kMaxAlignment) Pixel left_column_data[160] = {};
  // Some predictors use |top_row_data| and |left_column_data| with a negative
  // offset to access pixels to the top-left of the current block. So have some
  // space before the arrays to allow populating those without having to move
  // the rest of the array.
  Pixel* const top_row = top_row_data + 16;
  Pixel* const left_column = left_column_data + 16;
  const int bitdepth = sequence_header_.color_config.bitdepth;
  const int top_and_left_size = width + height;
  const bool is_directional_mode = IsDirectionalMode(mode);
  const PredictionParameters& prediction_parameters =
      *block.bp->prediction_parameters;
  const bool use_filter_intra =
      (plane == kPlaneY && prediction_parameters.use_filter_intra);
  const int prediction_angle =
      is_directional_mode
          ? kPredictionModeToAngle[mode] +
                prediction_parameters.angle_delta[GetPlaneType(plane)] *
                    kAngleStep
          : 0;
  const bool needs_top = use_filter_intra ||
                         kPredictionModeNeedsMask[mode].Contains(kNeedsTop) ||
                         (is_directional_mode && prediction_angle < 180) ||
                         (mode == kPredictionModeDc && has_top);
  Array2DView<Pixel> buffer(buffer_[plane].rows(),
                            buffer_[plane].columns() / sizeof(Pixel),
                            reinterpret_cast<Pixel*>(&buffer_[plane][0][0]));
  if (needs_top) {
    // Compute top_row.
    top_row[-1] = (has_top || has_left)
                      ? buffer[has_top ? y - 1 : y][has_left ? x - 1 : x]
                      : (1 << (bitdepth - 1));
    if (!has_top && has_left) {
      Memset(top_row, buffer[y][x - 1], top_and_left_size);
    } else if (!has_top && !has_left) {
      Memset(top_row, (1 << (bitdepth - 1)) - 1, top_and_left_size);
    } else {
      const int top_limit =
          std::min(max_x - x + 1, (has_top_right ? 2 : 1) * width);
      memcpy(top_row, &buffer[y - 1][x], top_limit * sizeof(Pixel));
      // Even though it is safe to call Memset with a size of 0, accessing
      // buffer[y - 1][top_limit - x + 1] is not allowed when this condition is
      // false.
      if (top_and_left_size - top_limit > 0) {
        Memset(top_row + top_limit, buffer[y - 1][top_limit + x - 1],
               top_and_left_size - top_limit);
      }
    }
  }
  const bool needs_left = use_filter_intra ||
                          kPredictionModeNeedsMask[mode].Contains(kNeedsLeft) ||
                          (is_directional_mode && prediction_angle > 90) ||
                          (mode == kPredictionModeDc && has_left);
  if (needs_left) {
    // Compute left_column.
    left_column[-1] = (has_top || has_left)
                          ? buffer[has_top ? y - 1 : y][has_left ? x - 1 : x]
                          : (1 << (bitdepth - 1));
    if (!has_left && has_top) {
      Memset(left_column, buffer[y - 1][x], top_and_left_size);
    } else if (!has_left && !has_top) {
      Memset(left_column, (1 << (bitdepth - 1)) + 1, top_and_left_size);
    } else {
      const int left_limit =
          std::min(max_y, y - 1 + ((has_bottom_left ? 2 : 1) * height));
      for (int i = 0; i < top_and_left_size; ++i) {
        left_column[i] = buffer[std::min(left_limit, y + i)][x - 1];
      }
    }
  }
  Pixel* const dest = &buffer[y][x];
  const ptrdiff_t dest_stride = buffer_[plane].columns();
  if (use_filter_intra) {
    dsp_.filter_intra_predictor(reinterpret_cast<uint8_t*>(dest), dest_stride,
                                reinterpret_cast<uint8_t*>(top_row),
                                reinterpret_cast<uint8_t*>(left_column),
                                prediction_parameters.filter_intra_mode, width,
                                height);
  } else if (is_directional_mode) {
    DirectionalPrediction(block, plane, x, y, has_left, has_top,
                          prediction_angle, width, height, max_x, max_y,
                          tx_size, top_row, left_column);
  } else {
    const dsp::IntraPredictor predictor =
        GetIntraPredictor(mode, has_left, has_top);
    assert(predictor != dsp::kNumIntraPredictors);
    dsp_.intra_predictors[tx_size][predictor](
        reinterpret_cast<uint8_t*>(dest), dest_stride,
        reinterpret_cast<uint8_t*>(top_row),
        reinterpret_cast<uint8_t*>(left_column));
  }
}

template void Tile::IntraPrediction<uint8_t>(const Block& block, Plane plane,
                                             int x, int y, bool has_left,
                                             bool has_top, bool has_top_right,
                                             bool has_bottom_left,
                                             PredictionMode mode,
                                             TransformSize tx_size);
#if LIBGAV1_MAX_BITDEPTH >= 10
template void Tile::IntraPrediction<uint16_t>(const Block& block, Plane plane,
                                              int x, int y, bool has_left,
                                              bool has_top, bool has_top_right,
                                              bool has_bottom_left,
                                              PredictionMode mode,
                                              TransformSize tx_size);
#endif

constexpr BitMaskSet kPredictionModeSmoothMask(kPredictionModeSmooth,
                                               kPredictionModeSmoothHorizontal,
                                               kPredictionModeSmoothVertical);

bool Tile::IsSmoothPrediction(int row, int column, Plane plane) const {
  const BlockParameters& bp = *block_parameters_holder_.Find(row, column);
  PredictionMode mode;
  if (plane == kPlaneY) {
    mode = bp.y_mode;
  } else {
    if (bp.reference_frame[0] > kReferenceFrameIntra) return false;
    mode = bp.uv_mode;
  }
  return kPredictionModeSmoothMask.Contains(mode);
}

int Tile::GetIntraEdgeFilterType(const Block& block, Plane plane) const {
  const int subsampling_x = subsampling_x_[plane];
  const int subsampling_y = subsampling_y_[plane];
  if ((plane == kPlaneY && block.top_available) ||
      (plane != kPlaneY && block.TopAvailableChroma())) {
    const int row =
        block.row4x4 - 1 -
        static_cast<int>(subsampling_y != 0 && (block.row4x4 & 1) != 0);
    const int column =
        block.column4x4 +
        static_cast<int>(subsampling_x != 0 && (block.column4x4 & 1) == 0);
    if (IsSmoothPrediction(row, column, plane)) return 1;
  }
  if ((plane == kPlaneY && block.left_available) ||
      (plane != kPlaneY && block.LeftAvailableChroma())) {
    const int row = block.row4x4 + static_cast<int>(subsampling_y != 0 &&
                                                    (block.row4x4 & 1) == 0);
    const int column =
        block.column4x4 - 1 -
        static_cast<int>(subsampling_x != 0 && (block.column4x4 & 1) != 0);
    if (IsSmoothPrediction(row, column, plane)) return 1;
  }
  return 0;
}

template <typename Pixel>
void Tile::DirectionalPrediction(const Block& block, Plane plane, int x, int y,
                                 bool has_left, bool has_top,
                                 int prediction_angle, int width, int height,
                                 int max_x, int max_y, TransformSize tx_size,
                                 Pixel* const top_row,
                                 Pixel* const left_column) {
  bool upsampled_top = false;
  bool upsampled_left = false;
  if (sequence_header_.enable_intra_edge_filter) {
    const int filter_type = GetIntraEdgeFilterType(block, plane);
    if (prediction_angle != 90 && prediction_angle != 180) {
      if (prediction_angle > 90 && prediction_angle < 180 &&
          (width + height) >= 24) {
        // 7.11.2.7.
        left_column[-1] = top_row[-1] = RightShiftWithRounding(
            left_column[0] * 5 + top_row[-1] * 6 + top_row[0] * 5, 4);
      }
      if (has_top) {
        const int strength = GetIntraEdgeFilterStrength(
            width, height, filter_type, prediction_angle - 90);
        if (strength > 0) {
          const int num_pixels = std::min(width, max_x - x + 1) +
                                 ((prediction_angle < 90) ? height : 0) + 1;
          dsp_.intra_edge_filter(top_row - 1, num_pixels, strength);
        }
      }
      if (has_left) {
        const int strength = GetIntraEdgeFilterStrength(
            width, height, filter_type, prediction_angle - 180);
        if (strength > 0) {
          const int num_pixels = std::min(height, max_y - y + 1) +
                                 ((prediction_angle > 180) ? width : 0) + 1;
          dsp_.intra_edge_filter(left_column - 1, num_pixels, strength);
        }
      }
    }
    upsampled_top = DoIntraEdgeUpsampling(width, height, filter_type,
                                          prediction_angle - 90);
    if (upsampled_top) {
      const int num_pixels = width + ((prediction_angle < 90) ? height : 0);
      dsp_.intra_edge_upsampler(top_row, num_pixels);
    }
    upsampled_left = DoIntraEdgeUpsampling(width, height, filter_type,
                                           prediction_angle - 180);
    if (upsampled_left) {
      const int num_pixels = height + ((prediction_angle > 180) ? width : 0);
      dsp_.intra_edge_upsampler(left_column, num_pixels);
    }
  }
  Array2DView<Pixel> buffer(buffer_[plane].rows(),
                            buffer_[plane].columns() / sizeof(Pixel),
                            reinterpret_cast<Pixel*>(&buffer_[plane][0][0]));
  auto* const dest = reinterpret_cast<uint8_t* const>(&buffer[y][x]);
  const ptrdiff_t stride = buffer_[plane].columns();
  if (prediction_angle == 90) {
    dsp_.intra_predictors[tx_size][dsp::kIntraPredictorVertical](
        dest, stride, reinterpret_cast<uint8_t*>(top_row),
        reinterpret_cast<uint8_t*>(left_column));
  } else if (prediction_angle == 180) {
    dsp_.intra_predictors[tx_size][dsp::kIntraPredictorHorizontal](
        dest, stride, reinterpret_cast<uint8_t*>(top_row),
        reinterpret_cast<uint8_t*>(left_column));
  } else if (prediction_angle < 90) {
    const int dx = GetDirectionalIntraPredictorDerivative(prediction_angle);
    dsp_.directional_intra_predictor_zone1(dest, stride,
                                           reinterpret_cast<uint8_t*>(top_row),
                                           width, height, dx, upsampled_top);
  } else if (prediction_angle < 180) {
    const int dx =
        GetDirectionalIntraPredictorDerivative(180 - prediction_angle);
    const int dy =
        GetDirectionalIntraPredictorDerivative(prediction_angle - 90);
    dsp_.directional_intra_predictor_zone2(
        dest, stride, reinterpret_cast<uint8_t*>(top_row),
        reinterpret_cast<uint8_t*>(left_column), width, height, dx, dy,
        upsampled_top, upsampled_left);
  } else {
    assert(prediction_angle < 270);
    const int dy =
        GetDirectionalIntraPredictorDerivative(270 - prediction_angle);
    dsp_.directional_intra_predictor_zone3(
        dest, stride, reinterpret_cast<uint8_t*>(left_column), width, height,
        dy, upsampled_left);
  }
}

template <typename Pixel>
void Tile::PalettePrediction(const Block& block, const Plane plane,
                             const int start_x, const int start_y, const int x,
                             const int y, const TransformSize tx_size) {
  const int tx_width = kTransformWidth[tx_size];
  const int tx_height = kTransformHeight[tx_size];
  const uint16_t* const palette = block.bp->palette_mode_info.color[plane];
  const PlaneType plane_type = GetPlaneType(plane);
  const int x4 = MultiplyBy4(x);
  const int y4 = MultiplyBy4(y);
  Array2DView<Pixel> buffer(buffer_[plane].rows(),
                            buffer_[plane].columns() / sizeof(Pixel),
                            reinterpret_cast<Pixel*>(&buffer_[plane][0][0]));
  for (int row = 0; row < tx_height; ++row) {
    assert(block.bp->prediction_parameters
               ->color_index_map[plane_type][y4 + row] != nullptr);
    for (int column = 0; column < tx_width; ++column) {
      buffer[start_y + row][start_x + column] =
          palette[block.bp->prediction_parameters
                      ->color_index_map[plane_type][y4 + row][x4 + column]];
    }
  }
}

template void Tile::PalettePrediction<uint8_t>(
    const Block& block, const Plane plane, const int start_x, const int start_y,
    const int x, const int y, const TransformSize tx_size);
#if LIBGAV1_MAX_BITDEPTH >= 10
template void Tile::PalettePrediction<uint16_t>(
    const Block& block, const Plane plane, const int start_x, const int start_y,
    const int x, const int y, const TransformSize tx_size);
#endif

template <typename Pixel>
void Tile::ChromaFromLumaPrediction(const Block& block, const Plane plane,
                                    const int start_x, const int start_y,
                                    const TransformSize tx_size) {
  const int subsampling_x = subsampling_x_[plane];
  const int subsampling_y = subsampling_y_[plane];
  const PredictionParameters& prediction_parameters =
      *block.bp->prediction_parameters;
  Array2DView<Pixel> y_buffer(
      buffer_[kPlaneY].rows(), buffer_[kPlaneY].columns() / sizeof(Pixel),
      reinterpret_cast<Pixel*>(&buffer_[kPlaneY][0][0]));
  if (!block.scratch_buffer->cfl_luma_buffer_valid) {
    const int luma_x = start_x << subsampling_x;
    const int luma_y = start_y << subsampling_y;
    dsp_.cfl_subsamplers[tx_size][subsampling_x + subsampling_y](
        block.scratch_buffer->cfl_luma_buffer,
        prediction_parameters.max_luma_width - luma_x,
        prediction_parameters.max_luma_height - luma_y,
        reinterpret_cast<uint8_t*>(&y_buffer[luma_y][luma_x]),
        buffer_[kPlaneY].columns());
    block.scratch_buffer->cfl_luma_buffer_valid = true;
  }
  Array2DView<Pixel> buffer(buffer_[plane].rows(),
                            buffer_[plane].columns() / sizeof(Pixel),
                            reinterpret_cast<Pixel*>(&buffer_[plane][0][0]));
  dsp_.cfl_intra_predictors[tx_size](
      reinterpret_cast<uint8_t*>(&buffer[start_y][start_x]),
      buffer_[plane].columns(), block.scratch_buffer->cfl_luma_buffer,
      (plane == kPlaneU) ? prediction_parameters.cfl_alpha_u
                         : prediction_parameters.cfl_alpha_v);
}

template void Tile::ChromaFromLumaPrediction<uint8_t>(
    const Block& block, const Plane plane, const int start_x, const int start_y,
    const TransformSize tx_size);
#if LIBGAV1_MAX_BITDEPTH >= 10
template void Tile::ChromaFromLumaPrediction<uint16_t>(
    const Block& block, const Plane plane, const int start_x, const int start_y,
    const TransformSize tx_size);
#endif

void Tile::InterIntraPrediction(
    uint16_t* prediction[2], const ptrdiff_t prediction_stride,
    const uint8_t* const prediction_mask,
    const ptrdiff_t prediction_mask_stride,
    const PredictionParameters& prediction_parameters,
    const int prediction_width, const int prediction_height,
    const int subsampling_x, const int subsampling_y, uint8_t* const dest,
    const ptrdiff_t dest_stride) {
  assert(prediction_mask != nullptr);
  assert(prediction_parameters.compound_prediction_type ==
             kCompoundPredictionTypeIntra ||
         prediction_parameters.compound_prediction_type ==
             kCompoundPredictionTypeWedge);
  // The first buffer of InterIntra is from inter prediction.
  // The second buffer is from intra prediction.
  ptrdiff_t intra_stride;
  const int bitdepth = sequence_header_.color_config.bitdepth;
  if (bitdepth == 8) {
    // Both the input predictors must be of type uint16_t. For bitdepth ==
    // 8, |buffer_| is uint8_t and hence a copy has to be made. For higher
    // bitdepths, the |buffer_| itself can act as an uint16_t buffer so no
    // copy is necessary.
    uint8_t* dest_ptr = dest;
    Array2DView<uint16_t> intra_prediction(
        kMaxSuperBlockSizeInPixels, kMaxSuperBlockSizeInPixels, prediction[1]);
    for (int r = 0; r < prediction_height; ++r) {
      for (int c = 0; c < prediction_width; ++c) {
        intra_prediction[r][c] = dest_ptr[c];
      }
      dest_ptr += dest_stride;
    }
    intra_stride = kMaxSuperBlockSizeInPixels;
  } else {
    prediction[1] = reinterpret_cast<uint16_t*>(dest);
    intra_stride = dest_stride / sizeof(uint16_t);
  }
  GetMaskBlendFunc(dsp_, prediction_parameters.inter_intra_mode,
                   prediction_parameters.is_wedge_inter_intra, subsampling_x,
                   subsampling_y)(prediction[0], prediction_stride,
                                  prediction[1], intra_stride, prediction_mask,
                                  prediction_mask_stride, prediction_width,
                                  prediction_height, dest, dest_stride);
}

void Tile::CompoundInterPrediction(
    const Block& block, const ptrdiff_t prediction_stride,
    const uint8_t* const prediction_mask,
    const ptrdiff_t prediction_mask_stride, const int prediction_width,
    const int prediction_height, const int subsampling_x,
    const int subsampling_y, const int candidate_row,
    const int candidate_column, uint8_t* dest, const ptrdiff_t dest_stride) {
  const PredictionParameters& prediction_parameters =
      *block.bp->prediction_parameters;
  uint16_t* prediction[2] = {block.scratch_buffer->prediction_buffer[0],
                             block.scratch_buffer->prediction_buffer[1]};
  switch (prediction_parameters.compound_prediction_type) {
    case kCompoundPredictionTypeWedge:
    case kCompoundPredictionTypeDiffWeighted:
      GetMaskBlendFunc(dsp_, prediction_parameters.inter_intra_mode,
                       prediction_parameters.is_wedge_inter_intra,
                       subsampling_x, subsampling_y)(
          prediction[0], prediction_stride, prediction[1], prediction_stride,
          prediction_mask, prediction_mask_stride, prediction_width,
          prediction_height, dest, dest_stride);
      break;
    case kCompoundPredictionTypeDistance:
      DistanceWeightedPrediction(
          prediction[0], prediction_stride, prediction[1], prediction_stride,
          prediction_width, prediction_height, candidate_row, candidate_column,
          dest, dest_stride);
      break;
    default:
      assert(prediction_parameters.compound_prediction_type ==
             kCompoundPredictionTypeAverage);
      dsp_.average_blend(prediction[0], prediction_stride, prediction[1],
                         prediction_stride, prediction_width, prediction_height,
                         dest, dest_stride);
      break;
  }
}

GlobalMotion* Tile::GetWarpParams(
    const Block& block, const Plane plane, const int prediction_width,
    const int prediction_height,
    const PredictionParameters& prediction_parameters,
    const ReferenceFrameType reference_type, bool* const is_local_valid,
    GlobalMotion* const global_motion_params,
    GlobalMotion* const local_warp_params) const {
  if (prediction_width < 8 || prediction_height < 8 ||
      frame_header_.force_integer_mv == 1) {
    return nullptr;
  }
  if (plane == kPlaneY) {
    *is_local_valid =
        prediction_parameters.motion_mode == kMotionModeLocalWarp &&
        WarpEstimation(
            prediction_parameters.num_warp_samples, DivideBy4(prediction_width),
            DivideBy4(prediction_height), block.row4x4, block.column4x4,
            block.bp->mv[0], prediction_parameters.warp_estimate_candidates,
            local_warp_params) &&
        SetupShear(local_warp_params);
  }
  if (prediction_parameters.motion_mode == kMotionModeLocalWarp &&
      *is_local_valid) {
    return local_warp_params;
  }
  if (!IsScaled(reference_type)) {
    GlobalMotionTransformationType global_motion_type =
        (reference_type != kReferenceFrameIntra)
            ? global_motion_params->type
            : kNumGlobalMotionTransformationTypes;
    const bool is_global_valid =
        IsGlobalMvBlock(block.bp->y_mode, global_motion_type, block.size) &&
        SetupShear(global_motion_params);
    // Valid global motion type implies reference type can't be intra.
    assert(!is_global_valid || reference_type != kReferenceFrameIntra);
    if (is_global_valid) return global_motion_params;
  }
  return nullptr;
}

void Tile::InterPrediction(const Block& block, const Plane plane, const int x,
                           const int y, const int prediction_width,
                           const int prediction_height, int candidate_row,
                           int candidate_column, bool* const is_local_valid,
                           GlobalMotion* const local_warp_params) {
  const int bitdepth = sequence_header_.color_config.bitdepth;
  const BlockParameters& bp = *block.bp;
  const BlockParameters& bp_reference =
      *block_parameters_holder_.Find(candidate_row, candidate_column);
  const bool is_compound =
      bp_reference.reference_frame[1] > kReferenceFrameIntra;
  assert(bp.is_inter);
  const bool is_inter_intra = bp.reference_frame[1] == kReferenceFrameIntra;
  // This ensures that each row of the prediction buffer is aligned to
  // kMaxAlignment.
  const ptrdiff_t prediction_stride = Align(
      prediction_width, static_cast<int>(kMaxAlignment / sizeof(uint16_t)));

  const PredictionParameters& prediction_parameters =
      *block.bp->prediction_parameters;
  uint8_t* const dest = GetStartPoint(buffer_, plane, x, y, bitdepth);
  const ptrdiff_t dest_stride = buffer_[plane].columns();  // In bytes.
  const int round_bits =
      GetInterRoundingBits(is_compound, sequence_header_.color_config.bitdepth);
  for (int index = 0; index < 1 + static_cast<int>(is_compound); ++index) {
    const ReferenceFrameType reference_type =
        bp_reference.reference_frame[index];
    GlobalMotion global_motion_params =
        frame_header_.global_motion[reference_type];
    GlobalMotion* warp_params =
        GetWarpParams(block, plane, prediction_width, prediction_height,
                      prediction_parameters, reference_type, is_local_valid,
                      &global_motion_params, local_warp_params);
    if (warp_params != nullptr) {
      BlockWarpProcess(block, plane, index, x, y, prediction_width,
                       prediction_height, prediction_stride, warp_params,
                       round_bits, is_compound, is_inter_intra, dest,
                       dest_stride);
    } else {
      const int reference_index =
          prediction_parameters.use_intra_block_copy
              ? -1
              : frame_header_.reference_frame_index[reference_type -
                                                    kReferenceFrameLast];
      BlockInterPrediction(
          block, plane, reference_index, bp_reference.mv[index], x, y,
          prediction_width, prediction_height, candidate_row, candidate_column,
          block.scratch_buffer->prediction_buffer[index], prediction_stride,
          round_bits, is_compound, is_inter_intra, dest, dest_stride);
    }
  }

  const int subsampling_x = subsampling_x_[plane];
  const int subsampling_y = subsampling_y_[plane];
  ptrdiff_t prediction_mask_stride = 0;
  const uint8_t* prediction_mask = nullptr;
  if (prediction_parameters.compound_prediction_type ==
      kCompoundPredictionTypeWedge) {
    const Array2D<uint8_t>& wedge_mask =
        wedge_masks_[GetWedgeBlockSizeIndex(block.size)]
                    [prediction_parameters.wedge_sign]
                    [prediction_parameters.wedge_index];
    prediction_mask = wedge_mask[0];
    prediction_mask_stride = wedge_mask.columns();
  } else if (prediction_parameters.compound_prediction_type ==
             kCompoundPredictionTypeIntra) {
    // 7.11.3.13. The inter intra masks are precomputed and stored as a set of
    // look up tables.
    assert(prediction_parameters.inter_intra_mode < kNumInterIntraModes);
    prediction_mask =
        kInterIntraMasks[prediction_parameters.inter_intra_mode]
                        [GetInterIntraMaskLookupIndex(prediction_width)]
                        [GetInterIntraMaskLookupIndex(prediction_height)];
    prediction_mask_stride = prediction_width;
  } else if (prediction_parameters.compound_prediction_type ==
             kCompoundPredictionTypeDiffWeighted) {
    if (plane == kPlaneY) {
      GenerateWeightMask(
          block.scratch_buffer->prediction_buffer[0], prediction_stride,
          block.scratch_buffer->prediction_buffer[1], prediction_stride,
          prediction_parameters.mask_is_inverse, prediction_width,
          prediction_height, bitdepth, block.scratch_buffer->weight_mask,
          kMaxSuperBlockSizeInPixels);
    }
    prediction_mask = block.scratch_buffer->weight_mask;
    prediction_mask_stride = kMaxSuperBlockSizeInPixels;
  }

  if (is_compound) {
    CompoundInterPrediction(block, prediction_stride, prediction_mask,
                            prediction_mask_stride, prediction_width,
                            prediction_height, subsampling_x, subsampling_y,
                            candidate_row, candidate_column, dest, dest_stride);
  } else if (prediction_parameters.motion_mode == kMotionModeObmc) {
    // Obmc mode is allowed only for single reference (!is_compound).
    ObmcPrediction(block, plane, prediction_width, prediction_height,
                   round_bits);
  } else if (is_inter_intra) {
    // InterIntra and obmc must be mutually exclusive.
    uint16_t* prediction_ptr[2] = {block.scratch_buffer->prediction_buffer[0],
                                   block.scratch_buffer->prediction_buffer[1]};
    InterIntraPrediction(prediction_ptr, prediction_stride, prediction_mask,
                         prediction_mask_stride, prediction_parameters,
                         prediction_width, prediction_height, subsampling_x,
                         subsampling_y, dest, dest_stride);
  }
}

void Tile::ObmcBlockPrediction(const Block& block, const MotionVector& mv,
                               const Plane plane,
                               const int reference_frame_index, const int width,
                               const int height, const int x, const int y,
                               const int candidate_row,
                               const int candidate_column,
                               const ObmcDirection blending_direction,
                               const int round_bits) {
  const int bitdepth = sequence_header_.color_config.bitdepth;
  // Obmc's prediction needs to be clipped before blending with above/left
  // prediction blocks.
  // Obmc prediction is used only when is_compound is false. So it is safe to
  // use prediction_buffer[1] as a temporary buffer for the Obmc prediction.
  static_assert(sizeof(block.scratch_buffer->prediction_buffer[1]) >=
                    64 * 64 * sizeof(uint16_t),
                "");
  auto* const obmc_buffer =
      reinterpret_cast<uint8_t*>(block.scratch_buffer->prediction_buffer[1]);
  const ptrdiff_t obmc_buffer_stride =
      (bitdepth == 8) ? width : width * sizeof(uint16_t);
  BlockInterPrediction(block, plane, reference_frame_index, mv, x, y, width,
                       height, candidate_row, candidate_column, nullptr, width,
                       round_bits, false, false, obmc_buffer,
                       obmc_buffer_stride);

  uint8_t* const prediction = GetStartPoint(buffer_, plane, x, y, bitdepth);
  const ptrdiff_t prediction_stride = buffer_[plane].columns();
  dsp_.obmc_blend[blending_direction](prediction, prediction_stride, width,
                                      height, obmc_buffer, obmc_buffer_stride);
}

void Tile::ObmcPrediction(const Block& block, const Plane plane,
                          const int width, const int height,
                          const int round_bits) {
  const int subsampling_x = subsampling_x_[plane];
  const int subsampling_y = subsampling_y_[plane];
  if (block.top_available &&
      !IsBlockSmallerThan8x8(block.residual_size[GetPlaneType(plane)])) {
    const int num_limit = std::min(uint8_t{4}, k4x4WidthLog2[block.size]);
    const int column4x4_max =
        std::min(block.column4x4 + block.width4x4, frame_header_.columns4x4);
    const int candidate_row = block.row4x4 - 1;
    const int block_start_y = MultiplyBy4(block.row4x4) >> subsampling_y;
    int column4x4 = block.column4x4;
    const int prediction_height = std::min(height >> 1, 32 >> subsampling_y);
    for (int i = 0, step; i < num_limit && column4x4 < column4x4_max;
         column4x4 += step) {
      const int candidate_column = column4x4 | 1;
      const BlockParameters& bp_top =
          *block_parameters_holder_.Find(candidate_row, candidate_column);
      const int candidate_block_size = bp_top.size;
      step = Clip3(kNum4x4BlocksWide[candidate_block_size], 2, 16);
      if (bp_top.reference_frame[0] > kReferenceFrameIntra) {
        i++;
        const int candidate_reference_frame_index =
            frame_header_.reference_frame_index[bp_top.reference_frame[0] -
                                                kReferenceFrameLast];
        const int prediction_width =
            std::min(width, MultiplyBy4(step) >> subsampling_x);
        ObmcBlockPrediction(block, bp_top.mv[0], plane,
                            candidate_reference_frame_index, prediction_width,
                            prediction_height,
                            MultiplyBy4(column4x4) >> subsampling_x,
                            block_start_y, candidate_row, candidate_column,
                            kObmcDirectionVertical, round_bits);
      }
    }
  }

  if (block.left_available) {
    const int num_limit = std::min(uint8_t{4}, k4x4HeightLog2[block.size]);
    const int row4x4_max =
        std::min(block.row4x4 + block.height4x4, frame_header_.rows4x4);
    const int candidate_column = block.column4x4 - 1;
    int row4x4 = block.row4x4;
    const int block_start_x = MultiplyBy4(block.column4x4) >> subsampling_x;
    const int prediction_width = std::min(width >> 1, 32 >> subsampling_x);
    for (int i = 0, step; i < num_limit && row4x4 < row4x4_max;
         row4x4 += step) {
      const int candidate_row = row4x4 | 1;
      const BlockParameters& bp_left =
          *block_parameters_holder_.Find(candidate_row, candidate_column);
      const int candidate_block_size = bp_left.size;
      step = Clip3(kNum4x4BlocksHigh[candidate_block_size], 2, 16);
      if (bp_left.reference_frame[0] > kReferenceFrameIntra) {
        i++;
        const int candidate_reference_frame_index =
            frame_header_.reference_frame_index[bp_left.reference_frame[0] -
                                                kReferenceFrameLast];
        const int prediction_height =
            std::min(height, MultiplyBy4(step) >> subsampling_y);
        ObmcBlockPrediction(
            block, bp_left.mv[0], plane, candidate_reference_frame_index,
            prediction_width, prediction_height, block_start_x,
            MultiplyBy4(row4x4) >> subsampling_y, candidate_row,
            candidate_column, kObmcDirectionHorizontal, round_bits);
      }
    }
  }
}

void Tile::DistanceWeightedPrediction(
    uint16_t* prediction_0, ptrdiff_t prediction_stride_0,
    uint16_t* prediction_1, ptrdiff_t prediction_stride_1, const int width,
    const int height, const int candidate_row, const int candidate_column,
    uint8_t* dest, ptrdiff_t dest_stride) {
  int distance[2];
  int weight[2];
  for (int reference = 0; reference < 2; ++reference) {
    const BlockParameters& bp =
        *block_parameters_holder_.Find(candidate_row, candidate_column);
    const int reference_hint =
        current_frame_.order_hint(bp.reference_frame[reference]);
    // Note: distance[0] and distance[1] correspond to relative distance
    // between current frame and reference frame [1] and [0], respectively.
    distance[1 - reference] = Clip3(
        std::abs(GetRelativeDistance(reference_hint, frame_header_.order_hint,
                                     sequence_header_.enable_order_hint,
                                     sequence_header_.order_hint_bits)),
        0, kMaxFrameDistance);
  }
  GetDistanceWeights(distance, weight);

  dsp_.distance_weighted_blend(prediction_0, prediction_stride_0, prediction_1,
                               prediction_stride_1, weight[0], weight[1], width,
                               height, dest, dest_stride);
}

bool Tile::GetReferenceBlockPosition(
    const int reference_frame_index, const bool is_scaled, const int width,
    const int height, const int ref_start_x, const int ref_last_x,
    const int ref_start_y, const int ref_last_y, const int start_x,
    const int start_y, const int step_x, const int step_y,
    const int right_border, const int bottom_border, int* ref_block_start_x,
    int* ref_block_start_y, int* ref_block_end_x, int* ref_block_end_y) {
  *ref_block_start_x = GetPixelPositionFromHighScale(start_x, 0, 0);
  *ref_block_start_y = GetPixelPositionFromHighScale(start_y, 0, 0);
  if (reference_frame_index == -1) {
    return false;
  }
  *ref_block_start_x -= kConvolveBorderLeftTop;
  *ref_block_start_y -= kConvolveBorderLeftTop;
  *ref_block_end_x = GetPixelPositionFromHighScale(start_x, step_x, width - 1) +
                     kConvolveBorderRight;
  *ref_block_end_y =
      GetPixelPositionFromHighScale(start_y, step_y, height - 1) +
      kConvolveBorderBottom;
  if (is_scaled) {
    const int block_height =
        (((height - 1) * step_y + (1 << kScaleSubPixelBits) - 1) >>
         kScaleSubPixelBits) +
        kSubPixelTaps;
    *ref_block_end_y = *ref_block_start_y + block_height - 1;
  }
  const bool extend_left = *ref_block_start_x < ref_start_x;
  const bool extend_right = *ref_block_end_x > (ref_last_x + right_border);
  const bool extend_top = *ref_block_start_y < ref_start_y;
  const bool extend_bottom = *ref_block_end_y > (ref_last_y + bottom_border);
  return extend_left || extend_right || extend_top || extend_bottom;
}

// Builds a block as the input for convolve, by copying the content of
// reference frame (either a decoded reference frame, or current frame).
// |block_extended_width| is the combined width of the block and its borders.
template <typename Pixel>
void Tile::BuildConvolveBlock(
    const Plane plane, const int reference_frame_index, const bool is_scaled,
    const int height, const int ref_start_x, const int ref_last_x,
    const int ref_start_y, const int ref_last_y, const int step_y,
    const int ref_block_start_x, const int ref_block_end_x,
    const int ref_block_start_y, uint8_t* block_buffer,
    ptrdiff_t convolve_buffer_stride, ptrdiff_t block_extended_width) {
  const YuvBuffer* const reference_buffer =
      (reference_frame_index == -1)
          ? current_frame_.buffer()
          : reference_frames_[reference_frame_index]->buffer();
  Array2DView<const Pixel> reference_block(
      reference_buffer->height(plane),
      reference_buffer->stride(plane) / sizeof(Pixel),
      reinterpret_cast<const Pixel*>(reference_buffer->data(plane)));
  auto* const block_head = reinterpret_cast<Pixel*>(block_buffer);
  convolve_buffer_stride /= sizeof(Pixel);
  int block_height = height + kConvolveBorderLeftTop + kConvolveBorderBottom;
  if (is_scaled) {
    block_height = (((height - 1) * step_y + (1 << kScaleSubPixelBits) - 1) >>
                    kScaleSubPixelBits) +
                   kSubPixelTaps;
  }
  const int copy_start_x =
      std::min(std::max(ref_block_start_x, ref_start_x), ref_last_x);
  const int copy_end_x =
      std::max(std::min(ref_block_end_x, ref_last_x), copy_start_x);
  const int copy_start_y =
      std::min(std::max(ref_block_start_y, ref_start_y), ref_last_y);
  const int block_width = copy_end_x - copy_start_x + 1;
  const bool extend_left = ref_block_start_x < ref_start_x;
  const bool extend_right = ref_block_end_x > ref_last_x;
  const bool out_of_left = copy_start_x > ref_block_end_x;
  const bool out_of_right = copy_end_x < ref_block_start_x;
  if (out_of_left || out_of_right) {
    const int ref_x = out_of_left ? copy_start_x : copy_end_x;
    Pixel* buf_ptr = block_head;
    for (int y = 0, ref_y = copy_start_y; y < block_height; ++y) {
      Memset(buf_ptr, reference_block[ref_y][ref_x], block_extended_width);
      if (ref_block_start_y + y >= ref_start_y &&
          ref_block_start_y + y < ref_last_y) {
        ++ref_y;
      }
      buf_ptr += convolve_buffer_stride;
    }
  } else {
    Pixel* buf_ptr = block_head;
    const int left_width = copy_start_x - ref_block_start_x;
    for (int y = 0, ref_y = copy_start_y; y < block_height; ++y) {
      if (extend_left) {
        Memset(buf_ptr, reference_block[ref_y][copy_start_x], left_width);
      }
      memcpy(buf_ptr + left_width, &reference_block[ref_y][copy_start_x],
             block_width * sizeof(Pixel));
      if (extend_right) {
        Memset(buf_ptr + left_width + block_width,
               reference_block[ref_y][copy_end_x],
               block_extended_width - left_width - block_width);
      }
      if (ref_block_start_y + y >= ref_start_y &&
          ref_block_start_y + y < ref_last_y) {
        ++ref_y;
      }
      buf_ptr += convolve_buffer_stride;
    }
  }
}

void Tile::BlockInterPrediction(
    const Block& block, const Plane plane, const int reference_frame_index,
    const MotionVector& mv, const int x, const int y, const int width,
    const int height, const int candidate_row, const int candidate_column,
    uint16_t* const prediction, const ptrdiff_t prediction_stride,
    const int round_bits, const bool is_compound, const bool is_inter_intra,
    uint8_t* const dest, const ptrdiff_t dest_stride) {
  const BlockParameters& bp =
      *block_parameters_holder_.Find(candidate_row, candidate_column);
  int start_x;
  int start_y;
  int step_x;
  int step_y;
  ScaleMotionVector(mv, plane, reference_frame_index, x, y, &start_x, &start_y,
                    &step_x, &step_y);
  const int horizontal_filter_index = bp.interpolation_filter[1];
  const int vertical_filter_index = bp.interpolation_filter[0];
  const int subsampling_x = subsampling_x_[plane];
  const int subsampling_y = subsampling_y_[plane];
  // reference_frame_index equal to -1 indicates using current frame as
  // reference.
  const YuvBuffer* const reference_buffer =
      (reference_frame_index == -1)
          ? current_frame_.buffer()
          : reference_frames_[reference_frame_index]->buffer();
  const int reference_upscaled_width =
      (reference_frame_index == -1)
          ? MultiplyBy4(frame_header_.columns4x4)
          : reference_frames_[reference_frame_index]->upscaled_width();
  const int reference_height =
      (reference_frame_index == -1)
          ? MultiplyBy4(frame_header_.rows4x4)
          : reference_frames_[reference_frame_index]->frame_height();
  const int ref_start_x = 0;
  const int ref_last_x =
      ((reference_upscaled_width + subsampling_x) >> subsampling_x) - 1;
  const int ref_start_y = 0;
  const int ref_last_y =
      ((reference_height + subsampling_y) >> subsampling_y) - 1;

  const bool is_scaled = (reference_frame_index != -1) &&
                         (frame_header_.width != reference_upscaled_width ||
                          frame_header_.height != reference_height);
  const int bitdepth = sequence_header_.color_config.bitdepth;
  const size_t pixel_size =
      (bitdepth == 8) ? sizeof(uint8_t) : sizeof(uint16_t);
  int ref_block_start_x;
  int ref_block_start_y;
  int ref_block_end_x;
  int ref_block_end_y;
  bool extend_block = GetReferenceBlockPosition(
      reference_frame_index, is_scaled, width, height, ref_start_x, ref_last_x,
      ref_start_y, ref_last_y, start_x, start_y, step_x, step_y,
      reference_buffer->right_border(plane),
      reference_buffer->bottom_border(plane), &ref_block_start_x,
      &ref_block_start_y, &ref_block_end_x, &ref_block_end_y);
  const uint8_t* block_start = nullptr;
  ptrdiff_t convolve_buffer_stride;
  if (!extend_block) {
    const YuvBuffer* const reference_buffer =
        (reference_frame_index == -1)
            ? current_frame_.buffer()
            : reference_frames_[reference_frame_index]->buffer();
    convolve_buffer_stride = reference_buffer->stride(plane);
    if (reference_frame_index == -1 || is_scaled) {
      block_start = reference_buffer->data(plane) +
                    ref_block_start_y * reference_buffer->stride(plane) +
                    ref_block_start_x * pixel_size;
    } else {
      block_start = reference_buffer->data(plane) +
                    (ref_block_start_y + kConvolveBorderLeftTop) *
                        reference_buffer->stride(plane) +
                    (ref_block_start_x + kConvolveBorderLeftTop) * pixel_size;
    }
  } else {
    // The block width can be at most 2 times as much as current
    // block's width because of scaling.
    auto block_extended_width = Align<ptrdiff_t>(
        (2 * width + kConvolveBorderLeftTop + kConvolveBorderRight) *
            pixel_size,
        kMaxAlignment);
    convolve_buffer_stride = block.scratch_buffer->convolve_block_buffer_stride;
    if (bitdepth == 8) {
      BuildConvolveBlock<uint8_t>(
          plane, reference_frame_index, is_scaled, height, ref_start_x,
          ref_last_x, ref_start_y, ref_last_y, step_y, ref_block_start_x,
          ref_block_end_x, ref_block_start_y,
          block.scratch_buffer->convolve_block_buffer.get(),
          convolve_buffer_stride, block_extended_width);
#if LIBGAV1_MAX_BITDEPTH >= 10
    } else {
      BuildConvolveBlock<uint16_t>(
          plane, reference_frame_index, is_scaled, height, ref_start_x,
          ref_last_x, ref_start_y, ref_last_y, step_y, ref_block_start_x,
          ref_block_end_x, ref_block_start_y,
          block.scratch_buffer->convolve_block_buffer.get(),
          convolve_buffer_stride, block_extended_width);
#endif
    }
    block_start = block.scratch_buffer->convolve_block_buffer.get() +
                  (is_scaled ? 0
                             : kConvolveBorderLeftTop * convolve_buffer_stride +
                                   kConvolveBorderLeftTop * pixel_size);
  }

  const int has_horizontal_filter = static_cast<int>(
      ((mv.mv[MotionVector::kColumn] * (1 << (1 - subsampling_x))) & 15) != 0);
  const int has_vertical_filter = static_cast<int>(
      ((mv.mv[MotionVector::kRow] * (1 << (1 - subsampling_y))) & 15) != 0);
  void* const output =
      (is_compound || is_inter_intra) ? prediction : static_cast<void*>(dest);
  const ptrdiff_t output_stride =
      (is_compound || is_inter_intra) ? prediction_stride : dest_stride;
  assert(output != nullptr);
  dsp::ConvolveFunc convolve_func =
      is_scaled ? dsp_.convolve_scale[is_compound || is_inter_intra]
                : dsp_.convolve[reference_frame_index == -1][is_compound]
                               [has_vertical_filter][has_horizontal_filter];
  assert(convolve_func != nullptr);
  // TODO(b/127805357): Refactor is_inter_intra into single prediction.
  if (is_inter_intra && !is_scaled) {
    convolve_func = dsp_.convolve[0][1][1][1];
  }
  convolve_func(block_start, convolve_buffer_stride, horizontal_filter_index,
                vertical_filter_index, round_bits, start_x, start_y, step_x,
                step_y, width, height, output, output_stride);
}

void Tile::BlockWarpProcess(const Block& block, const Plane plane,
                            const int index, const int block_start_x,
                            const int block_start_y, const int width,
                            const int height, const ptrdiff_t prediction_stride,
                            GlobalMotion* const warp_params,
                            const int round_bits, const bool is_compound,
                            const bool is_inter_intra, uint8_t* const dest,
                            const ptrdiff_t dest_stride) {
  assert(width >= 8 && height >= 8);
  const BlockParameters& bp = *block.bp;
  const int reference_frame_index =
      frame_header_.reference_frame_index[bp.reference_frame[index] -
                                          kReferenceFrameLast];
  const uint8_t* const source =
      reference_frames_[reference_frame_index]->buffer()->data(plane);
  ptrdiff_t source_stride =
      reference_frames_[reference_frame_index]->buffer()->stride(plane);
  const int source_width =
      reference_frames_[reference_frame_index]->buffer()->displayed_width(
          plane);
  const int source_height =
      reference_frames_[reference_frame_index]->buffer()->displayed_height(
          plane);
  uint16_t* const prediction = block.scratch_buffer->prediction_buffer[index];
  dsp_.warp(source, source_stride, source_width, source_height,
            warp_params->params, subsampling_x_[plane], subsampling_y_[plane],
            round_bits, block_start_x, block_start_y, width, height,
            warp_params->alpha, warp_params->beta, warp_params->gamma,
            warp_params->delta, prediction, prediction_stride);
  if (!is_compound && !is_inter_intra) {
    const int bitdepth = sequence_header_.color_config.bitdepth;
    if (bitdepth == 8) {
      ClipPrediction<8, uint8_t>(prediction, prediction_stride, width, height,
                                 dest, dest_stride);
#if LIBGAV1_MAX_BITDEPTH >= 10
    } else {
      ClipPrediction<10, uint16_t>(prediction, prediction_stride, width, height,
                                   dest, dest_stride);
#endif
    }
  }
}

}  // namespace libgav1
