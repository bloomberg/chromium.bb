// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/uma_feature_processor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/feature_processor_state.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/stats.h"

namespace segmentation_platform {

UmaFeatureProcessor::UmaFeatureProcessor(
    base::flat_map<FeatureIndex, proto::UMAFeature>&& uma_features,
    SignalDatabase* signal_database,
    FeatureAggregator* feature_aggregator,
    const base::Time prediction_time,
    const base::TimeDelta bucket_duration,
    const OptimizationTarget segment_id)
    : uma_features_(std::move(uma_features)),
      signal_database_(signal_database),
      feature_aggregator_(feature_aggregator),
      prediction_time_(prediction_time),
      bucket_duration_(bucket_duration),
      segment_id_(segment_id) {}

UmaFeatureProcessor::~UmaFeatureProcessor() = default;

void UmaFeatureProcessor::Process(
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    QueryProcessorCallback callback) {
  feature_processor_state_ = std::move(feature_processor_state);
  callback_ = std::move(callback);
  ProcessNextUmaFeature();
}

void UmaFeatureProcessor::ProcessNextUmaFeature() {
  // Process the feature list.
  absl::optional<proto::UMAFeature> next_feature;
  FeatureIndex index;
  while (!uma_features_.empty()) {
    auto it = uma_features_.begin();
    if (it->second.bucket_count() == 0) {
      // Skip collection-only features.
      uma_features_.erase(it);
    } else {
      next_feature = std::move(it->second);
      index = it->first;
      uma_features_.erase(it);
      break;
    }
  }

  // Processing of the feature list has completed.
  if (!next_feature.has_value() || feature_processor_state_->error()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_),
                                  std::move(feature_processor_state_),
                                  std::move(result_)));
    return;
  }

  // Validate the proto::UMAFeature metadata.
  if (metadata_utils::ValidateMetadataUmaFeature(next_feature.value()) !=
      metadata_utils::ValidationResult::kValidationSuccess) {
    feature_processor_state_->SetError();
    ProcessNextUmaFeature();
    return;
  }

  ProcessSingleUmaFeature(index, next_feature.value());
}

void UmaFeatureProcessor::ProcessSingleUmaFeature(
    FeatureIndex index,
    const proto::UMAFeature& feature) {
  auto name_hash = feature.name_hash();

  // Enum histograms can optionally only accept some of the enum values.
  // While the proto::UMAFeature is available, capture a vector of the
  // accepted enum values. An empty vector is ignored (all values are
  // considered accepted).
  std::vector<int32_t> accepted_enum_ids{};
  if (feature.type() == proto::SignalType::HISTOGRAM_ENUM) {
    for (int i = 0; i < feature.enum_ids_size(); ++i)
      accepted_enum_ids.emplace_back(feature.enum_ids(i));
  }

  // Only fetch data that is relevant for the current proto::UMAFeature, since
  // the FeatureAggregator assumes that only relevant data is given to it.
  base::TimeDelta duration = bucket_duration_ * feature.bucket_count();
  base::Time start_time = prediction_time_ - duration;

  // Fetch the relevant samples for the current proto::UMAFeature. Once the
  // result has come back, it will be processed and inserted into the
  // FeatureProcessorState::input_tensor and will then invoke
  // ProcessInputFeatures(...) again to ensure we continue until all features
  // have been processed. Note: All parameters from the FeatureProcessorState
  // need to be captured locally before invoking GetSamples, because the state
  // is moved with the callback, and the order of the move and accessing the
  // members while invoking GetSamples is not guaranteed.
  auto signal_type = feature.type();
  signal_database_->GetSamples(
      signal_type, name_hash, start_time, prediction_time_,
      base::BindOnce(&UmaFeatureProcessor::OnGetSamplesForUmaFeature,
                     weak_ptr_factory_.GetWeakPtr(), index, feature,
                     accepted_enum_ids));
}

void UmaFeatureProcessor::OnGetSamplesForUmaFeature(
    FeatureIndex index,
    const proto::UMAFeature& feature,
    const std::vector<int32_t>& accepted_enum_ids,
    std::vector<SignalDatabase::Sample> samples) {
  base::ElapsedTimer timer;
  // HISTOGRAM_ENUM features might require us to filter out the result to only
  // keep enum values that match the accepted list. If the accepted list is'
  // empty, all histogram enum values are kept.
  // The SignalDatabase does not currently support this type of data filter,
  // so instead we are doing this here.
  if (feature.type() == proto::SignalType::HISTOGRAM_ENUM) {
    feature_aggregator_->FilterEnumSamples(accepted_enum_ids, samples);
  }

  // We now have all the data required to process a single feature, so we can
  // process it synchronously, and insert it into the
  // FeatureProcessorState::input_tensor so we can later pass it to the ML model
  // executor.
  std::vector<float> feature_data = feature_aggregator_->Process(
      feature.type(), feature.aggregation(), feature.bucket_count(),
      prediction_time_, bucket_duration_, samples);

  DCHECK_EQ(feature.tensor_length(), feature_data.size());
  result_[index] =
      std::vector<ProcessedValue>(feature_data.begin(), feature_data.end());

  stats::RecordModelExecutionDurationFeatureProcessing(segment_id_,
                                                       timer.Elapsed());

  // Continue with the rest of the features.
  ProcessNextUmaFeature();
}

}  // namespace segmentation_platform
