// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_result_provider.h"

#include <map>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/execution/default_model_manager.h"
#include "components/segmentation_platform/internal/execution/execution_request.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {
namespace {

int ComputeDiscreteMapping(const std::string& segmentation_key,
                           const proto::SegmentInfo& segment_info) {
  int rank = metadata_utils::ConvertToDiscreteScore(
      segmentation_key, segment_info.prediction_result().result(),
      segment_info.model_metadata());
  VLOG(1) << __func__
          << ": segment=" << SegmentId_Name(segment_info.segment_id())
          << ": result=" << segment_info.prediction_result().result()
          << ", rank=" << rank;

  return rank;
}

proto::SegmentInfo* FilterSegmentInfoBySource(
    DefaultModelManager::SegmentInfoList& available_segments,
    DefaultModelManager::SegmentSource needed_source) {
  proto::SegmentInfo* segment_info = nullptr;
  for (const auto& info : available_segments) {
    if (info->segment_source == needed_source) {
      segment_info = &info->segment_info;
      break;
    }
  }
  return segment_info;
}

class SegmentResultProviderImpl : public SegmentResultProvider {
 public:
  SegmentResultProviderImpl(SegmentInfoDatabase* segment_database,
                            SignalStorageConfig* signal_storage_config,
                            DefaultModelManager* default_model_manager,
                            ExecutionService* execution_service,
                            base::Clock* clock,
                            bool force_refresh_results)
      : segment_database_(segment_database),
        signal_storage_config_(signal_storage_config),
        default_model_manager_(default_model_manager),
        execution_service_(execution_service),
        clock_(clock),
        force_refresh_results_(force_refresh_results),
        task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

  void GetSegmentResult(std::unique_ptr<GetResultOptions> options) override;

  SegmentResultProviderImpl(SegmentResultProviderImpl&) = delete;
  SegmentResultProviderImpl& operator=(SegmentResultProviderImpl&) = delete;

 private:
  struct RequestState {
    std::unordered_map<DefaultModelManager::SegmentSource,
                       raw_ptr<ModelProvider>>
        model_providers;
    DefaultModelManager::SegmentInfoList available_segments;
    std::unique_ptr<GetResultOptions> options;
  };

  void OnGetSegmentInfo(
      std::unique_ptr<GetResultOptions> options,
      DefaultModelManager::SegmentInfoList available_segments);

  void OnGotDatabaseModelScore(std::unique_ptr<RequestState> request_state,
                               std::unique_ptr<SegmentResult> db_result);

  void TryGetScoreFromDefaultModel(
      std::unique_ptr<RequestState> request_state,
      SegmentResultProvider::ResultState existing_state);

  using ResultCallbackWithState =
      base::OnceCallback<void(std::unique_ptr<RequestState>,
                              std::unique_ptr<SegmentResult>)>;

  void GetCachedModelScore(std::unique_ptr<RequestState> request_state,
                           ResultCallbackWithState callback);
  void ExecuteModelAndGetScore(std::unique_ptr<RequestState> request_state,
                               DefaultModelManager::SegmentSource source,
                               ResultCallbackWithState callback);

  void OnModelExecuted(std::unique_ptr<RequestState> request_state,
                       DefaultModelManager::SegmentSource source,
                       ResultCallbackWithState callback,
                       const std::pair<float, ModelExecutionStatus>& result);

  void PostResultCallback(std::unique_ptr<RequestState> request_state,
                          std::unique_ptr<SegmentResult> result);

  const raw_ptr<SegmentInfoDatabase> segment_database_;
  const raw_ptr<SignalStorageConfig> signal_storage_config_;
  const raw_ptr<DefaultModelManager> default_model_manager_;
  const raw_ptr<ExecutionService> execution_service_;
  const raw_ptr<base::Clock> clock_;
  const bool force_refresh_results_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<SegmentResultProviderImpl> weak_ptr_factory_{this};
};

void SegmentResultProviderImpl::GetSegmentResult(
    std::unique_ptr<GetResultOptions> options) {
  const SegmentId segment_id = options->segment_id;
  default_model_manager_->GetAllSegmentInfoFromBothModels(
      {segment_id}, segment_database_,
      base::BindOnce(&SegmentResultProviderImpl::OnGetSegmentInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(options)));
}

void SegmentResultProviderImpl::OnGetSegmentInfo(
    std::unique_ptr<GetResultOptions> options,
    DefaultModelManager::SegmentInfoList available_segments) {
  const SegmentId segment_id = options->segment_id;
  auto request_state = std::make_unique<RequestState>();
  request_state->options = std::move(options);
  request_state->available_segments.swap(available_segments);
  request_state->model_providers[DefaultModelManager::SegmentSource::DATABASE] =
      execution_service_ ? execution_service_->GetModelProvider(segment_id)
                         : nullptr;
  // Default manager can be null in tests.
  request_state
      ->model_providers[DefaultModelManager::SegmentSource::DEFAULT_MODEL] =
      default_model_manager_
          ? default_model_manager_->GetDefaultProvider(segment_id)
          : nullptr;

  auto db_score_callback =
      base::BindOnce(&SegmentResultProviderImpl::OnGotDatabaseModelScore,
                     weak_ptr_factory_.GetWeakPtr());

  if (request_state->options->ignore_db_scores) {
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << " executing model to get score";
    ExecuteModelAndGetScore(std::move(request_state),
                            DefaultModelManager::SegmentSource::DATABASE,
                            std::move(db_score_callback));
    return;
  }

  GetCachedModelScore(std::move(request_state), std::move(db_score_callback));
}

void SegmentResultProviderImpl::OnGotDatabaseModelScore(
    std::unique_ptr<RequestState> request_state,
    std::unique_ptr<SegmentResult> db_result) {
  if (db_result && db_result->rank.has_value()) {
    PostResultCallback(std::move(request_state), std::move(db_result));
    return;
  }
  VLOG(1) << __func__
          << ": segment=" << SegmentId_Name(request_state->options->segment_id)
          << " failed to get database model score, trying default model.";
  TryGetScoreFromDefaultModel(std::move(request_state), db_result->state);
}

void SegmentResultProviderImpl::TryGetScoreFromDefaultModel(
    std::unique_ptr<RequestState> request_state,
    SegmentResultProvider::ResultState existing_state) {
  ModelProvider* default_model =
      request_state
          ->model_providers[DefaultModelManager::SegmentSource::DEFAULT_MODEL];
  if (!default_model || !default_model->ModelAvailable()) {
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << " default provider not available";
    // Make sure the metrics record state of database model failure when client
    // did not provide a default model.
    PostResultCallback(std::move(request_state),
                       std::make_unique<SegmentResult>(existing_state));
    return;
  }
  ExecuteModelAndGetScore(
      std::move(request_state),
      DefaultModelManager::SegmentSource::DEFAULT_MODEL,
      base::BindOnce(&SegmentResultProviderImpl::PostResultCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SegmentResultProviderImpl::GetCachedModelScore(
    std::unique_ptr<RequestState> request_state,
    ResultCallbackWithState callback) {
  const proto::SegmentInfo* db_segment_info =
      FilterSegmentInfoBySource(request_state->available_segments,
                                DefaultModelManager::SegmentSource::DATABASE);

  if (!db_segment_info) {
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << " does not have a segment info.";
    std::move(callback).Run(
        std::move(request_state),
        std::make_unique<SegmentResult>(ResultState::kSegmentNotAvailable));
    return;
  }

  if (metadata_utils::HasExpiredOrUnavailableResult(*db_segment_info,
                                                    clock_->Now())) {
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << " has expired or unavailable result.";
    std::move(callback).Run(
        std::move(request_state),
        std::make_unique<SegmentResult>(ResultState::kDatabaseScoreNotReady));
    return;
  }

  int rank = ComputeDiscreteMapping(request_state->options->segmentation_key,
                                    *db_segment_info);
  std::move(callback).Run(
      std::move(request_state),
      std::make_unique<SegmentResult>(ResultState::kSuccessFromDatabase, rank));
}

void SegmentResultProviderImpl::ExecuteModelAndGetScore(
    std::unique_ptr<RequestState> request_state,
    DefaultModelManager::SegmentSource source,
    ResultCallbackWithState callback) {
  const proto::SegmentInfo* segment_info =
      FilterSegmentInfoBySource(request_state->available_segments, source);
  if (!segment_info) {
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << " default segment info not available";
    auto state = source == DefaultModelManager::SegmentSource::DATABASE
                     ? ResultState::kSegmentNotAvailable
                     : ResultState::kDefaultModelMetadataMissing;
    std::move(callback).Run(std::move(request_state),
                            std::make_unique<SegmentResult>(state));
    return;
  }

  DCHECK_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadata(segment_info->model_metadata()));
  if (!force_refresh_results_ &&
      !signal_storage_config_->MeetsSignalCollectionRequirement(
          segment_info->model_metadata())) {
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << " signal collection not met";
    auto state = source == DefaultModelManager::SegmentSource::DATABASE
                     ? ResultState::kSignalsNotCollected
                     : ResultState::kDefaultModelSignalNotCollected;
    std::move(callback).Run(std::move(request_state),
                            std::make_unique<SegmentResult>(state));
    return;
  }

  ModelProvider* provider = request_state->model_providers[source];
  auto request = std::make_unique<ExecutionRequest>();
  // The pointer is kept alive by the `request_state`.
  request->segment_info = segment_info;
  request->record_metrics_for_default = true;
  request->callback =
      base::BindOnce(&SegmentResultProviderImpl::OnModelExecuted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request_state),
                     source, std::move(callback));
  request->model_provider = provider;
  execution_service_->RequestModelExecution(std::move(request));
}

void SegmentResultProviderImpl::OnModelExecuted(
    std::unique_ptr<RequestState> request_state,
    DefaultModelManager::SegmentSource source,
    ResultCallbackWithState callback,
    const std::pair<float, ModelExecutionStatus>& result) {
  auto* segment_info =
      FilterSegmentInfoBySource(request_state->available_segments, source);
  if (result.second == ModelExecutionStatus::kSuccess) {
    segment_info->mutable_prediction_result()->set_result(result.first);
    int rank = ComputeDiscreteMapping(request_state->options->segmentation_key,
                                      *segment_info);
    ResultState state =
        source == DefaultModelManager::SegmentSource::DEFAULT_MODEL
            ? ResultState::kDefaultModelScoreUsed
            : ResultState::kTfliteModelScoreUsed;
    std::move(callback).Run(std::move(request_state),
                            std::make_unique<SegmentResult>(state, rank));
  } else {
    ResultState state =
        source == DefaultModelManager::SegmentSource::DEFAULT_MODEL
            ? ResultState::kDefaultModelExecutionFailed
            : ResultState::kTfliteModelExecutionFailed;
    std::move(callback).Run(std::move(request_state),
                            std::make_unique<SegmentResult>(state));
  }
}

void SegmentResultProviderImpl::PostResultCallback(
    std::unique_ptr<RequestState> request_state,
    std::unique_ptr<SegmentResult> result) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(request_state->options->callback),
                                std::move(result)));
}

}  // namespace

SegmentResultProvider::SegmentResult::SegmentResult(ResultState state)
    : state(state) {}
SegmentResultProvider::SegmentResult::SegmentResult(ResultState state, int rank)
    : state(state), rank(rank) {}
SegmentResultProvider::SegmentResult::~SegmentResult() = default;

SegmentResultProvider::GetResultOptions::GetResultOptions() = default;
SegmentResultProvider::GetResultOptions::~GetResultOptions() = default;

// static
std::unique_ptr<SegmentResultProvider> SegmentResultProvider::Create(
    SegmentInfoDatabase* segment_database,
    SignalStorageConfig* signal_storage_config,
    DefaultModelManager* default_model_manager,
    ExecutionService* execution_service,
    base::Clock* clock,
    bool force_refresh_results) {
  return std::make_unique<SegmentResultProviderImpl>(
      segment_database, signal_storage_config, default_model_manager,
      execution_service, clock, force_refresh_results);
}

}  // namespace segmentation_platform
