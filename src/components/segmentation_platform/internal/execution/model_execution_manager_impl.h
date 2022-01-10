// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/execution/segmentation_model_handler.h"
#include "components/segmentation_platform/internal/proto/aggregation.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Clock;
}  // namespace base

namespace segmentation_platform {
class FeatureAggregator;
class SignalDatabase;

namespace proto {
class SegmentInfo;
}  // namespace proto

// The ModelExecutionManagerImpl is the core implementation of the
// ModelExecutionManager that hooks up the SegmentInfoDatabase (metadata) and
// SignalDatabase (raw signals) databases, and uses a FeatureCalculator for each
// feature to go from metadata and raw signals to create an input tensor to use
// when executing the ML model. It then uses this input tensor to execute the
// model and returns the result through a callback.
// This class is implemented by having a long chain of callbacks and storing all
// necessary state as part of an ExecutionState struct. This simplifies state
// management, particularly in the case of executing multiple models
// simultaneously, or the same model multiple times without waiting for the
// requests to finish.
// The vector of OptimizationTargets need to be passed in at construction time
// so the SegmentationModelHandler instances can be created early.
class ModelExecutionManagerImpl : public ModelExecutionManager {
 public:
  using ModelHandlerCreator =
      base::RepeatingCallback<std::unique_ptr<SegmentationModelHandler>(
          optimization_guide::proto::OptimizationTarget,
          const SegmentationModelHandler::ModelUpdatedCallback&)>;

  explicit ModelExecutionManagerImpl(
      const base::flat_set<OptimizationTarget>& segment_ids,
      ModelHandlerCreator model_handler_creator,
      base::Clock* clock,
      SegmentInfoDatabase* segment_database,
      SignalDatabase* signal_database,
      std::unique_ptr<FeatureAggregator> feature_aggregator,
      const SegmentationModelUpdatedCallback& model_updated_callback);
  ~ModelExecutionManagerImpl() override;

  // Disallow copy/assign.
  ModelExecutionManagerImpl(const ModelExecutionManagerImpl&) = delete;
  ModelExecutionManagerImpl& operator=(const ModelExecutionManagerImpl&) =
      delete;

  // ModelExecutionManager overrides.
  void ExecuteModel(optimization_guide::proto::OptimizationTarget segment_id,
                    ModelExecutionCallback callback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SegmentationPlatformServiceImplTest,
                           InitializationFlow);
  struct ExecutionState;
  struct FeatureState;
  struct ModelExecutionTraceEvent;

  // Callback method for when the SegmentInfo (segment metadata) has been
  // loaded.
  void OnSegmentInfoFetchedForExecution(
      std::unique_ptr<ExecutionState> state,
      absl::optional<proto::SegmentInfo> segment_info);

  // ProcessFeatures is the core function for processing all the required ML
  // features in the correct order. It fetches samples for one feature at a
  // time, makes sure the data is processed, and then is invoked again to
  // process the next feature.
  void ProcessFeatures(std::unique_ptr<ExecutionState> state);

  // Callback method for when all relevant samples for a particular feature has
  // been loaded. Processes the samples, and inserts them into the input tensor
  // that is later given to the ML execution.
  void OnGetSamplesForFeature(std::unique_ptr<ExecutionState> state,
                              std::unique_ptr<FeatureState> feature_state,
                              std::vector<SignalDatabase::Sample> samples);

  // ExecuteModel takes the current input tensor and passes it to the ML model
  // for execution.
  void ExecuteModel(std::unique_ptr<ExecutionState> state);

  // Callback method for when the model execution has completed which gives the
  // end result to the initial ModelExecutionCallback passed to
  // ExecuteModel(...).
  void OnModelExecutionComplete(std::unique_ptr<ExecutionState> state,
                                const absl::optional<float>& result);

  // Helper function for synchronously invoking the callback with the given
  // result and status.
  void RunModelExecutionCallback(std::unique_ptr<ExecutionState> state,
                                 float result,
                                 ModelExecutionStatus status);

  // Callback for whenever a SegmentationModelHandler is informed that the
  // underlying ML model file has been updated. If there is an available
  // model, this will be called at least once per session.
  void OnSegmentationModelUpdated(
      optimization_guide::proto::OptimizationTarget segment_id,
      proto::SegmentationModelMetadata metadata);

  // Callback after fetching the current SegmentInfo from the
  // SegmentInfoDatabase. This is part of the flow for informing the
  // SegmentationModelUpdatedCallback about a changed model.
  // Merges the PredictionResult from the previously stored SegmentInfo with the
  // newly updated one, and stores the new version in the DB.
  void OnSegmentInfoFetchedForModelUpdate(
      optimization_guide::proto::OptimizationTarget segment_id,
      proto::SegmentationModelMetadata metadata,
      absl::optional<proto::SegmentInfo> segment_info);

  // Callback after storing the updated version of the SegmentInfo. Responsible
  // for invoking the SegmentationModelUpdatedCallback.
  void OnUpdatedSegmentInfoStored(proto::SegmentInfo segment_info,
                                  bool success);

  // All the relevant handlers for each of the segments.
  std::map<OptimizationTarget, std::unique_ptr<SegmentationModelHandler>>
      model_handlers_;

  // Used to access the current time.
  raw_ptr<base::Clock> clock_;

  // Database for segment information and metadata.
  raw_ptr<SegmentInfoDatabase> segment_database_;

  // Main signal database for user actions and histograms.
  raw_ptr<SignalDatabase> signal_database_;

  // The FeatureAggregator aggregates all the data based on metadata and input.
  std::unique_ptr<FeatureAggregator> feature_aggregator_;

  // Invoked whenever there is an update to any of the relevant ML models.
  SegmentationModelUpdatedCallback model_updated_callback_;

  base::WeakPtrFactory<ModelExecutionManagerImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_IMPL_H_
