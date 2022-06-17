// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_UKM_HELPER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_UKM_HELPER_H_

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Clock;
}

namespace ukm::builders {
class Segmentation_ModelExecution;
}  // namespace ukm::builders

namespace segmentation_platform {

using proto::SegmentId;
struct SelectedSegment;

// A helper class to record segmentation model execution results in UKM.
class SegmentationUkmHelper {
 public:
  static SegmentationUkmHelper* GetInstance();
  SegmentationUkmHelper(const SegmentationUkmHelper&) = delete;
  SegmentationUkmHelper& operator=(const SegmentationUkmHelper&) = delete;

  // Record segmentation model information and input/output after the
  // executing the model, and return the UKM source ID.
  ukm::SourceId RecordModelExecutionResult(
      SegmentId segment_id,
      int64_t model_version,
      const std::vector<float>& input_tensor,
      float result);

  // Record segmentation model training data as UKM message.
  // `input_tensors` contains the values for training inputs.
  // `outputs` contains the values for outputs.
  // `output_indexes` contains the indexes for outputs that needs to be included
  // in the ukm message.
  // `prediction_result` is the most recent model execution result.
  // `selected_segment` is the recently selected segment for the feature that is
  // tied to the ML model.
  // Return the UKM source ID.
  ukm::SourceId RecordTrainingData(
      SegmentId segment_id,
      int64_t model_version,
      const std::vector<float>& input_tensors,
      const std::vector<float>& outputs,
      const std::vector<int>& output_indexes,
      absl::optional<proto::PredictionResult> prediction_result,
      absl::optional<SelectedSegment> selected_segment);

  // Helper method to encode a float number into int64.
  static int64_t FloatToInt64(float f);

  // Helper method to check if data is allowed to upload through ukm
  // given a clock and the signal storage length.
  static bool AllowedToUploadData(base::TimeDelta signal_storage_length,
                                  base::Clock* clock);

  // Gets a set of segment IDs that are allowed to upload metrics.
  const base::flat_set<SegmentId>& allowed_segment_ids() {
    return allowed_segment_ids_;
  }

 private:
  bool AddInputsToUkm(ukm::builders::Segmentation_ModelExecution* ukm_builder,
                      SegmentId segment_id,
                      int64_t model_version,
                      const std::vector<float>& input_tensor);

  bool AddOutputsToUkm(ukm::builders::Segmentation_ModelExecution* ukm_builder,
                       const std::vector<float>& outputs,
                       const std::vector<int>& output_indexes);

  friend class base::NoDestructor<SegmentationUkmHelper>;
  friend class SegmentationUkmHelperTest;
  SegmentationUkmHelper();
  ~SegmentationUkmHelper();

  void Initialize();

  base::flat_set<SegmentId> allowed_segment_ids_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_UKM_HELPER_H_
