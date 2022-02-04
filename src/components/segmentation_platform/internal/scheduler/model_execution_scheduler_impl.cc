// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/stats.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

ModelExecutionSchedulerImpl::ModelExecutionSchedulerImpl(
    std::vector<Observer*>&& observers,
    SegmentInfoDatabase* segment_database,
    SignalStorageConfig* signal_storage_config,
    ModelExecutionManager* model_execution_manager,
    base::flat_set<optimization_guide::proto::OptimizationTarget> segment_ids,
    base::Clock* clock,
    const PlatformOptions& platform_options)
    : observers_(observers),
      segment_database_(segment_database),
      signal_storage_config_(signal_storage_config),
      model_execution_manager_(model_execution_manager),
      all_segment_ids_(segment_ids),
      clock_(clock),
      platform_options_(platform_options) {}

ModelExecutionSchedulerImpl::~ModelExecutionSchedulerImpl() = default;

void ModelExecutionSchedulerImpl::OnNewModelInfoReady(
    const proto::SegmentInfo& segment_info) {
  DCHECK(metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info) ==
         metadata_utils::ValidationResult::kValidationSuccess);

  if (!ShouldExecuteSegment(/*expired_only=*/true, segment_info)) {
    // We usually cancel any outstanding requests right before executing the
    // model, but in this case we alreday know that 1) we got a new model, and
    // b) the new model is not yet valid for execution. Therefore, we cancel
    // the current execution and we will have to execute this model later.
    CancelOutstandingExecutionRequests(segment_info.segment_id());
    return;
  }

  RequestModelExecution(segment_info.segment_id());
}

void ModelExecutionSchedulerImpl::RequestModelExecutionForEligibleSegments(
    bool expired_only) {
  std::vector<OptimizationTarget> segment_ids(all_segment_ids_.begin(),
                                              all_segment_ids_.end());
  segment_database_->GetSegmentInfoForSegments(
      segment_ids,
      base::BindOnce(&ModelExecutionSchedulerImpl::FilterEligibleSegments,
                     weak_ptr_factory_.GetWeakPtr(), expired_only));
}

void ModelExecutionSchedulerImpl::RequestModelExecution(
    OptimizationTarget segment_id) {
  CancelOutstandingExecutionRequests(segment_id);
  outstanding_requests_.insert(std::make_pair(
      segment_id,
      base::BindOnce(&ModelExecutionSchedulerImpl::OnModelExecutionCompleted,
                     weak_ptr_factory_.GetWeakPtr(), segment_id)));
  model_execution_manager_->ExecuteModel(
      segment_id, outstanding_requests_[segment_id].callback());
}

void ModelExecutionSchedulerImpl::OnModelExecutionCompleted(
    OptimizationTarget segment_id,
    const std::pair<float, ModelExecutionStatus>& result) {
  // TODO(shaktisahu): Check ModelExecutionStatus and handle failure cases.
  // Should we save it to DB?
  proto::PredictionResult segment_result;
  bool success = result.second == ModelExecutionStatus::kSuccess;
  if (success) {
    segment_result.set_result(result.first);
    segment_result.set_timestamp_us(
        clock_->Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
    stats::RecordModelScore(segment_id, result.first);
  } else {
    stats::RecordSegmentSelectionFailure(
        stats::SegmentationSelectionFailureReason::
            kAtLeastOneModelFailedExecution);
  }

  segment_database_->SaveSegmentResult(
      segment_id, success ? absl::make_optional(segment_result) : absl::nullopt,
      base::BindOnce(&ModelExecutionSchedulerImpl::OnResultSaved,
                     weak_ptr_factory_.GetWeakPtr(), segment_id));
}

void ModelExecutionSchedulerImpl::FilterEligibleSegments(
    bool expired_only,
    std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>
        all_segments) {
  std::vector<OptimizationTarget> models_to_run;
  for (const auto& pair : all_segments) {
    OptimizationTarget segment_id = pair.first;
    const proto::SegmentInfo& segment_info = pair.second;
    if (!ShouldExecuteSegment(expired_only, segment_info)) {
      VLOG(1) << "Segmentation scheduler: Skipped executed segment "
              << optimization_guide::proto::OptimizationTarget_Name(segment_id);
      continue;
    }

    models_to_run.emplace_back(segment_id);
  }

  for (OptimizationTarget segment_id : models_to_run)
    RequestModelExecution(segment_id);
}

bool ModelExecutionSchedulerImpl::ShouldExecuteSegment(
    bool expired_only,
    const proto::SegmentInfo& segment_info) {
  if (platform_options_.force_refresh_results)
    return true;

  // Filter out the segments computed recently.
  if (metadata_utils::HasFreshResults(segment_info, clock_->Now())) {
    VLOG(1) << "Segmentation model not executed since it has fresh results.";
    return false;
  }

  // Filter out the segments that aren't expired yet.
  if (expired_only && !metadata_utils::HasExpiredOrUnavailableResult(
                          segment_info, clock_->Now())) {
    VLOG(1) << "Segmentation model not executed since results are not expired.";
    return false;
  }

  // Filter out segments that don't match signal collection min length.
  if (!signal_storage_config_->MeetsSignalCollectionRequirement(
          segment_info.model_metadata())) {
    stats::RecordSegmentSelectionFailure(
        stats::SegmentationSelectionFailureReason::
            kAtLeastOneModelNeedsMoreSignals);
    VLOG(1) << "Segmentation model not executed since metadata requirements "
               "not met.";
    return false;
  }

  return true;
}

void ModelExecutionSchedulerImpl::CancelOutstandingExecutionRequests(
    OptimizationTarget segment_id) {
  const auto& iter = outstanding_requests_.find(segment_id);
  if (iter != outstanding_requests_.end()) {
    iter->second.Cancel();
    outstanding_requests_.erase(iter);
  }
}

void ModelExecutionSchedulerImpl::OnResultSaved(OptimizationTarget segment_id,
                                                bool success) {
  stats::RecordModelExecutionSaveResult(segment_id, success);
  if (!success) {
    stats::RecordSegmentSelectionFailure(
        stats::SegmentationSelectionFailureReason::kFailedToSaveModelResult);
    return;
  }

  for (Observer* observer : observers_)
    observer->OnModelExecutionCompleted(segment_id);
}

}  // namespace segmentation_platform
