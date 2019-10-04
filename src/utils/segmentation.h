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

#ifndef LIBGAV1_SRC_UTILS_SEGMENTATION_H_
#define LIBGAV1_SRC_UTILS_SEGMENTATION_H_

#include <cstdint>

#include "src/utils/constants.h"

namespace libgav1 {

// The corresponding segment feature constants in the AV1 spec are named
// SEG_LVL_xxx.
enum SegmentFeature : uint8_t {
  kSegmentFeatureQuantizer,
  kSegmentFeatureLoopFilterYVertical,
  kSegmentFeatureLoopFilterYHorizontal,
  kSegmentFeatureLoopFilterU,
  kSegmentFeatureLoopFilterV,
  kSegmentFeatureReferenceFrame,
  kSegmentFeatureSkip,
  kSegmentFeatureGlobalMv,
  kSegmentFeatureMax
};

extern const int8_t kSegmentationFeatureBits[kSegmentFeatureMax];
extern const int kSegmentationFeatureMaxValues[kSegmentFeatureMax];

struct Segmentation {
  // 5.11.14.
  // Returns true if the feature is enabled in the segment.
  bool FeatureActive(int segment_id, SegmentFeature feature) const {
    return enabled && segment_id < kMaxSegments &&
           feature_enabled[segment_id][feature];
  }

  // Returns true if the feature is signed.
  static bool FeatureSigned(SegmentFeature feature) {
    // Only the first five segment features are signed, so this comparison
    // suffices.
    return feature <= kSegmentFeatureLoopFilterV;
  }

  bool enabled;
  bool update_map;
  bool update_data;
  bool temporal_update;
  // True if the segment id will be read before the skip syntax element. False
  // if the skip syntax element will be read first.
  bool segment_id_pre_skip;
  // The highest numbered segment id that has some enabled feature. Used as
  // the upper bound for decoding segment ids.
  int8_t last_active_segment_id;

  bool feature_enabled[kMaxSegments][kSegmentFeatureMax];
  int16_t feature_data[kMaxSegments][kSegmentFeatureMax];
  bool lossless[kMaxSegments];
  // Cached values of get_qindex(1, segmentId), to be consumed by
  // Tile::ReadTransformType(). The values are in the range [0, 255].
  uint8_t qindex[kMaxSegments];
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_UTILS_SEGMENTATION_H_
