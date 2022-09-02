// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_FEATURE_LIST_QUERY_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_FEATURE_LIST_QUERY_PROCESSOR_H_

#include <deque>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/execution/processing/custom_input_processor.h"
#include "components/segmentation_platform/internal/execution/processing/query_processor.h"
#include "components/segmentation_platform/internal/execution/processing/uma_feature_processor.h"
#include "components/segmentation_platform/internal/input_context.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {
class StorageService;

namespace processing {

class FeatureAggregator;
class FeatureProcessorState;
class InputDelegateHolder;

using proto::SegmentId;

// FeatureListQueryProcessor takes a segmentation model's metadata, processes
// each feature in the metadata's feature list in order and computes an input
// tensor to use when executing the ML model.
class FeatureListQueryProcessor {
 public:
  FeatureListQueryProcessor(
      StorageService* storage_service,
      std::unique_ptr<InputDelegateHolder> input_delegate_holder,
      std::unique_ptr<FeatureAggregator> feature_aggregator);
  virtual ~FeatureListQueryProcessor();

  // Disallow copy/assign.
  FeatureListQueryProcessor(const FeatureListQueryProcessor&) = delete;
  FeatureListQueryProcessor& operator=(const FeatureListQueryProcessor&) =
      delete;

  using FeatureProcessorCallback =
      base::OnceCallback<void(bool,
                              const std::vector<float>& /*inputs*/,
                              const std::vector<float>& /*outputs*/)>;

  // Options for determining what should be included in the result.
  enum class ProcessOption { kInputsOnly, kOutputsOnly, kInputsAndOutputs };

  // Given a model's metadata, processes the feature list from the metadata and
  // computes the input tensor for the ML model. Result is returned through a
  // callback.
  // |segment_id| is only used for recording performance metrics. This class
  // does not need to know about the segment itself. |prediction_time| is the
  // time at which we predict the model execution should happen.
  virtual void ProcessFeatureList(
      const proto::SegmentationModelMetadata& model_metadata,
      scoped_refptr<InputContext> input_context,
      SegmentId segment_id,
      base::Time prediction_time,
      ProcessOption process_option,
      FeatureProcessorCallback callback);

 private:
  // Called by ProcessFeatureList to process the next input feature in the list.
  // It then delegates the processing to the correct feature processor class
  // until the feature list is empty.
  void ProcessNext(
      std::unique_ptr<FeatureProcessorState> feature_processor_state);

  // Callback called after a feature has been processed, indicating that we can
  // safely discard the feature processor that handled the processing. Continue
  // with the rest of the input features by calling ProcessNextInputFeature.
  // `is_input' indicates whether the feature is for input tensors.
  void OnFeatureProcessed(
      std::unique_ptr<QueryProcessor> feature_processor,
      bool is_input,
      std::unique_ptr<FeatureProcessorState> feature_processor_state,
      QueryProcessor::IndexedTensors result);

  // Gets a UMA feature processor for a
  std::unique_ptr<UmaFeatureProcessor> GetUmaFeatureProcessor(
      base::flat_map<FeatureIndex, proto::UMAFeature>&& uma_features,
      FeatureProcessorState* feature_processor_state);

  // Storage service which provides signals to process.
  const raw_ptr<StorageService> storage_service_;

  // Holds InputDelegate for feature processing.
  std::unique_ptr<InputDelegateHolder> input_delegate_holder_;

  // Feature aggregator that aggregates data for uma features.
  const std::unique_ptr<FeatureAggregator> feature_aggregator_;

  base::WeakPtrFactory<FeatureListQueryProcessor> weak_ptr_factory_{this};
};

}  // namespace processing
}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_FEATURE_LIST_QUERY_PROCESSOR_H_
