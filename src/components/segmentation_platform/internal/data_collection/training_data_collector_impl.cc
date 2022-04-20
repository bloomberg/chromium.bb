// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector_impl.h"

#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/time/clock.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/execution/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"
#include "components/segmentation_platform/internal/stats.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {
namespace {

// Parse outputs into a map of metric hash of the uma output and its index in
// the output list.
std::map<uint64_t, int> ParseUmaOutputs(
    const proto::SegmentationModelMetadata& metadata) {
  std::map<uint64_t, int> hash_index_map;
  if (!metadata.has_training_outputs())
    return hash_index_map;

  const auto& training_outputs = metadata.training_outputs();
  for (int i = 0; i < training_outputs.outputs_size(); ++i) {
    const auto& output = training_outputs.outputs(i);
    if (!output.has_uma_output() || !output.uma_output().has_uma_feature())
      continue;

    hash_index_map[output.uma_output().uma_feature().name_hash()] = i;
  }
  return hash_index_map;
}

}  // namespace

TrainingDataCollectorImpl::TrainingDataCollectorImpl(
    SegmentInfoDatabase* segment_info_database,
    FeatureListQueryProcessor* processor,
    HistogramSignalHandler* histogram_signal_handler,
    SignalStorageConfig* signal_storage_config,
    base::Clock* clock)
    : segment_info_database_(segment_info_database),
      feature_list_query_processor_(processor),
      histogram_signal_handler_(histogram_signal_handler),
      signal_storage_config_(signal_storage_config),
      clock_(clock) {}

TrainingDataCollectorImpl::~TrainingDataCollectorImpl() {
  histogram_signal_handler_->RemoveObserver(this);
}

void TrainingDataCollectorImpl::OnModelMetadataUpdated() {
  NOTIMPLEMENTED();
}

void TrainingDataCollectorImpl::OnServiceInitialized() {
  segment_info_database_->GetAllSegmentInfo(
      base::BindOnce(&TrainingDataCollectorImpl::OnGetSegmentsInfoList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrainingDataCollectorImpl::OnGetSegmentsInfoList(
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segments) {
  histogram_signal_handler_->AddObserver(this);

  DCHECK(segments);
  for (const auto& segment : *segments) {
    const proto::SegmentInfo& segment_info = segment.second;
    // Validate segment info.
    auto validation_result = metadata_utils::ValidateSegmentInfo(segment_info);
    if (validation_result !=
        metadata_utils::ValidationResult::kValidationSuccess) {
      VLOG(1) << "Segment info validation failed for optimization target: "
              << segment.first
              << ", validation result:" << static_cast<int>(validation_result);
      RecordTrainingDataCollectionEvent(
          segment.first,
          stats::TrainingDataCollectionEvent::kMetadataValidationFailed);
      continue;
    }

    // Cache the histograms as outputs of training data, which needs to be
    // immediately reported when the histogram is recorded.
    auto hash_index_map = ParseUmaOutputs(segment_info.model_metadata());
    for (const auto& hash_index : hash_index_map) {
      const auto& output =
          segment_info.model_metadata().training_outputs().outputs(
              hash_index.second);
      // Ignore continuous collection UMA.
      if (output.uma_output().has_duration())
        continue;
      immediate_collection_histograms_[hash_index.first].emplace(segment.first);
    }
  }
}

void TrainingDataCollectorImpl::OnHistogramSignalUpdated(
    const std::string& histogram_name,
    base::HistogramBase::Sample sample) {
  auto hash = base::HashMetricName(histogram_name);
  auto it = immediate_collection_histograms_.find(hash);
  // Report training data for all models that are interested in
  // |histogram_name| as output.
  if (it != immediate_collection_histograms_.end()) {
    std::vector<OptimizationTarget> optimization_targets(it->second.begin(),
                                                         it->second.end());
    segment_info_database_->GetSegmentInfoForSegments(
        optimization_targets,
        base::BindOnce(&TrainingDataCollectorImpl::ReportForSegmentsInfoList,
                       weak_ptr_factory_.GetWeakPtr(), hash, sample));
  }
}

void TrainingDataCollectorImpl::ReportForSegmentsInfoList(
    uint64_t output_metric_hash,
    base::HistogramBase::Sample output_metric_sample,
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segments) {
  DCHECK(segments);
  for (const auto& segment : *segments) {
    RecordTrainingDataCollectionEvent(
        segment.first,
        stats::TrainingDataCollectionEvent::kImmediateCollectionStart);
    const proto::SegmentInfo& segment_info = segment.second;
    // Figure out the output index.
    auto hash_index_map = ParseUmaOutputs(segment_info.model_metadata());
    if (hash_index_map.find(output_metric_hash) == hash_index_map.end())
      continue;

    if (!CanReportImmediateTrainingData(segment.second))
      continue;

    // Generate training data input.
    // TODO(ssid): Validate immediate output is not included in the input
    // features and update the comment in model_metadata.proto.
    feature_list_query_processor_->ProcessFeatureList(
        segment_info.model_metadata(), segment_info.segment_id(), clock_->Now(),
        base::BindOnce(&TrainingDataCollectorImpl::OnGetInputTensor,
                       weak_ptr_factory_.GetWeakPtr(),
                       static_cast<float>(output_metric_sample),
                       hash_index_map[output_metric_hash],
                       segment_info.segment_id(),
                       segment_info.model_version()));
  }
}

bool TrainingDataCollectorImpl::CanReportImmediateTrainingData(
    const proto::SegmentInfo& segment_info) {
  if (!segment_info.has_model_version() ||
      !segment_info.has_model_update_time_s() ||
      segment_info.model_update_time_s() == 0) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kModelInfoMissing);
    return false;
  }

  base::TimeDelta min_signal_collection_length =
      segment_info.model_metadata().min_signal_collection_length() *
      metadata_utils::GetTimeUnit(segment_info.model_metadata());
  base::Time model_update_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Seconds(segment_info.model_update_time_s()));

  // Data must be collected for enough time after a new model is downloaded.
  // It's recommended to get the A/B testing experiment fully ramped up before
  // deploying a new model. Or the data collected might be partially based on
  // old behavior of Chrome.
  if (model_update_time + min_signal_collection_length >= clock_->Now()) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kNotEnoughCollectionTime);
    return false;
  }

  // Each input must be collected for enough time.
  if (!signal_storage_config_->MeetsSignalCollectionRequirement(
          segment_info.model_metadata())) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kNotEnoughCollectionTime);
    return false;
  }

  return true;
}

void TrainingDataCollectorImpl::OnGetInputTensor(
    float output_value,
    int output_index,
    OptimizationTarget segment_id,
    int64_t model_version,
    bool success,
    const std::vector<float>& inputs) {
  if (!success) {
    RecordTrainingDataCollectionEvent(
        segment_id, stats::TrainingDataCollectionEvent::kGetInputTensorsFailed);
    return;
  }

  auto ukm_source_id = SegmentationUkmHelper::GetInstance()->RecordTrainingData(
      segment_id, model_version, inputs, {output_value}, {output_index});
  if (ukm_source_id == ukm::kInvalidSourceId) {
    VLOG(1) << "Failed to collect training data for segment:" << segment_id;
    RecordTrainingDataCollectionEvent(
        segment_id, stats::TrainingDataCollectionEvent::kUkmReportingFailed);
  } else {
    RecordTrainingDataCollectionEvent(
        segment_id,
        stats::TrainingDataCollectionEvent::kImmediateCollectionSuccess);
  }
}

}  // namespace segmentation_platform
