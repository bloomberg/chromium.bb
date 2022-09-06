// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector_impl.h"

#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/time/clock.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/local_state_helper.h"

namespace segmentation_platform {
namespace {
using processing::FeatureListQueryProcessor;

// Minimum intervals between collection.
// TODO(qinmin): make this configurable through finch.
static int kMinimumReportingIntervalInHours = 24;

// Given the last report time, calculate the next report time.
base::Time GetNextReportTime(base::Time last_report_time) {
  // The next report time is determined by |kMinimumReportingIntervalInHours|
  // hours after last report.
  return last_report_time + base::Hours(kMinimumReportingIntervalInHours);
}

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

// Find the segmentation key from the configs that contains the segment ID.
std::string GetSegmentationKey(std::vector<std::unique_ptr<Config>>* configs,
                               SegmentId segment_id) {
  if (!configs)
    return std::string();

  for (const auto& config : *configs) {
    if (std::find(config->segment_ids.begin(), config->segment_ids.end(),
                  segment_id) != config->segment_ids.end()) {
      return config->segmentation_key;
    }
  }
  return std::string();
}

}  // namespace

TrainingDataCollectorImpl::TrainingDataCollectorImpl(
    SegmentInfoDatabase* segment_info_database,
    processing::FeatureListQueryProcessor* processor,
    HistogramSignalHandler* histogram_signal_handler,
    SignalStorageConfig* signal_storage_config,
    std::vector<std::unique_ptr<Config>>* configs,
    PrefService* profile_prefs,
    base::Clock* clock)
    : segment_info_database_(segment_info_database),
      feature_list_query_processor_(processor),
      histogram_signal_handler_(histogram_signal_handler),
      signal_storage_config_(signal_storage_config),
      configs_(configs),
      clock_(clock),
      result_prefs_(std::make_unique<SegmentationResultPrefs>(profile_prefs)) {}

TrainingDataCollectorImpl::~TrainingDataCollectorImpl() {
  histogram_signal_handler_->RemoveObserver(this);
}

void TrainingDataCollectorImpl::OnModelMetadataUpdated() {
  NOTIMPLEMENTED();
}

void TrainingDataCollectorImpl::OnServiceInitialized() {
  if (!SegmentationUkmHelper::GetInstance()->allowed_segment_ids().empty()) {
    segment_info_database_->GetAllSegmentInfo(
        base::BindOnce(&TrainingDataCollectorImpl::OnGetSegmentsInfoList,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void TrainingDataCollectorImpl::OnGetSegmentsInfoList(
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segments) {
  histogram_signal_handler_->AddObserver(this);

  DCHECK(segments);
  const base::flat_set<SegmentId>& allowed_ids =
      SegmentationUkmHelper::GetInstance()->allowed_segment_ids();
  for (const auto& segment : *segments) {
    // Skip the segment if it is not in allowed list.
    if (!allowed_ids.contains(static_cast<int>(segment.first))) {
      continue;
    }

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
      // If tensor length is 0, the output is for immediate collection.
      if (output.uma_output().uma_feature().tensor_length() != 0) {
        continuous_collection_segments_.insert(segment.first);
        continue;
      }
      immediate_collection_histograms_[hash_index.first].emplace(segment.first);
    }
  }

  ReportCollectedContinuousTrainingData();
}

void TrainingDataCollectorImpl::OnHistogramSignalUpdated(
    const std::string& histogram_name,
    base::HistogramBase::Sample sample) {
  auto hash = base::HashMetricName(histogram_name);
  auto it = immediate_collection_histograms_.find(hash);
  // Report training data for all models that are interested in
  // |histogram_name| as output.
  if (it != immediate_collection_histograms_.end()) {
    std::vector<SegmentId> segment_ids(it->second.begin(), it->second.end());
    auto param = absl::make_optional<ImmediaCollectionParam>();
    param->output_metric_hash = hash;
    param->output_value = static_cast<float>(sample);
    segment_info_database_->GetSegmentInfoForSegments(
        segment_ids,
        base::BindOnce(&TrainingDataCollectorImpl::ReportForSegmentsInfoList,
                       weak_ptr_factory_.GetWeakPtr(), std::move(param)));
  }
}

void TrainingDataCollectorImpl::ReportForSegmentsInfoList(
    const absl::optional<ImmediaCollectionParam>& param,
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segments) {
  DCHECK(segments);
  // Make a copy of the input param so it can be modified later.
  absl::optional<ImmediaCollectionParam> immediate_collection_param = param;
  bool include_outputs = !param.has_value();
  for (const auto& segment : *segments) {
    RecordTrainingDataCollectionEvent(
        segment.first,
        immediate_collection_param.has_value()
            ? stats::TrainingDataCollectionEvent::kImmediateCollectionStart
            : stats::TrainingDataCollectionEvent::kContinousCollectionStart);

    const proto::SegmentInfo& segment_info = segment.second;
    // Figure out the output index.
    if (immediate_collection_param.has_value()) {
      auto hash_index_map = ParseUmaOutputs(segment_info.model_metadata());
      if (hash_index_map.find(immediate_collection_param->output_metric_hash) ==
          hash_index_map.end()) {
        continue;
      }
      immediate_collection_param->output_index =
          hash_index_map[immediate_collection_param->output_metric_hash];
    }

    // For non-immediate collections, we need to validate all output
    // tensors are allowed by UKM policies.
    if (!CanReportTrainingData(segment.second, include_outputs)) {
      continue;
    }

    // Generate training data input.
    // TODO(ssid): Validate immediate output is not included in the input
    // features and update the comment in model_metadata.proto.
    feature_list_query_processor_->ProcessFeatureList(
        segment_info.model_metadata(), /*input_context=*/nullptr,
        segment_info.segment_id(), clock_->Now(),
        include_outputs
            ? FeatureListQueryProcessor::ProcessOption::kInputsAndOutputs
            : FeatureListQueryProcessor::ProcessOption::kInputsOnly,
        base::BindOnce(&TrainingDataCollectorImpl::OnGetTrainingTensors,
                       weak_ptr_factory_.GetWeakPtr(),
                       immediate_collection_param, segment_info));
  }
}

bool TrainingDataCollectorImpl::CanReportTrainingData(
    const proto::SegmentInfo& segment_info,
    bool include_output) {
  if (!segment_info.has_model_version() ||
      !segment_info.has_model_update_time_s() ||
      segment_info.model_update_time_s() == 0) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kModelInfoMissing);
    return false;
  }

  const proto::SegmentationModelMetadata& model_metadata =
      segment_info.model_metadata();

  // If UKM is allowed recently, don't upload the metrics.
  DCHECK_LE(model_metadata.min_signal_collection_length(),
            model_metadata.signal_storage_length());
  base::TimeDelta signal_storage_length =
      model_metadata.signal_storage_length() *
      metadata_utils::GetTimeUnit(model_metadata);
  if (!SegmentationUkmHelper::AllowedToUploadData(signal_storage_length,
                                                  clock_)) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kPartialDataNotAllowed);
    return false;
  }

  base::TimeDelta min_signal_collection_length =
      model_metadata.min_signal_collection_length() *
      metadata_utils::GetTimeUnit(model_metadata);
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
          model_metadata, include_output)) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kNotEnoughCollectionTime);
    return false;
  }

  return true;
}

void TrainingDataCollectorImpl::OnGetTrainingTensors(
    const absl::optional<ImmediaCollectionParam>& param,
    const proto::SegmentInfo& segment_info,
    bool has_error,
    const std::vector<float>& input_tensors,
    const std::vector<float>& output_tensors) {
  if (has_error) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kGetInputTensorsFailed);
    return;
  }

  // TODO(qinmin): update SegmentationUkmHelper::RecordTrainingData()
  // and ukm file for description of the prediction result as it is
  // the segment selection result, rather than model result.
  std::string segmentation_key =
      GetSegmentationKey(configs_, segment_info.segment_id());

  std::vector<int> output_indexes;
  if (param.has_value()) {
    output_indexes = {param->output_index};
  } else {
    for (size_t i = 0; i < output_tensors.size(); ++i) {
      output_indexes.emplace_back(i);
    }
  }

  auto ukm_source_id = SegmentationUkmHelper::GetInstance()->RecordTrainingData(
      segment_info.segment_id(), segment_info.model_version(), input_tensors,
      param.has_value() ? std::vector<float>{param->output_value}
                        : output_tensors,
      output_indexes, segment_info.prediction_result(),
      result_prefs_->ReadSegmentationResultFromPref(segmentation_key));
  if (ukm_source_id == ukm::kInvalidSourceId) {
    VLOG(1) << "Failed to collect training data for segment:"
            << segment_info.segment_id();
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kUkmReportingFailed);
    return;
  }

  RecordTrainingDataCollectionEvent(
      segment_info.segment_id(),
      param.has_value()
          ? stats::TrainingDataCollectionEvent::kImmediateCollectionSuccess
          : stats::TrainingDataCollectionEvent::kContinousCollectionSuccess);
  if (!param.has_value()) {
    LocalStateHelper::GetInstance().SetPrefTime(
        kSegmentationLastCollectionTimePref, clock_->Now());
  }
}

void TrainingDataCollectorImpl::ReportCollectedContinuousTrainingData() {
  if (continuous_collection_segments_.empty())
    return;

  base::Time last_collection_time = LocalStateHelper::GetInstance().GetPrefTime(
      kSegmentationLastCollectionTimePref);
  base::Time next_collection_time = GetNextReportTime(last_collection_time);
  if (clock_->Now() >= next_collection_time) {
    segment_info_database_->GetSegmentInfoForSegments(
        std::vector<SegmentId>(continuous_collection_segments_.begin(),
                               continuous_collection_segments_.end()),
        base::BindOnce(&TrainingDataCollectorImpl::ReportForSegmentsInfoList,
                       weak_ptr_factory_.GetWeakPtr(), absl::nullopt));
  }
}

}  // namespace segmentation_platform
