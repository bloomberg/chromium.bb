// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/core/model_enums.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/prediction_model_download_observer.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "url/origin.h"

namespace download {
class BackgroundDownloadService;
}  // namespace download

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class OptimizationGuideLogger;
class PrefService;

namespace optimization_guide {

enum class OptimizationGuideDecision;
class OptimizationGuideStore;
class OptimizationTargetModelObserver;
class PredictionModelDownloadManager;
class PredictionModelFetcher;
class ModelInfo;

// A PredictionManager supported by the optimization guide that makes an
// OptimizationTargetDecision by evaluating the corresponding prediction model
// for an OptimizationTarget.
class PredictionManager : public PredictionModelDownloadObserver {
 public:
  // BackgroundDownloadService is only available once the profile is fully
  // initialized and that cannot be done as part of |Initialize|. Get a provider
  // to retrieve the service when it is needed.
  using BackgroundDownloadServiceProvider =
      base::OnceCallback<download::BackgroundDownloadService*(void)>;

  // Callback to whether component updates are enabled for the browser.
  using ComponentUpdatesEnabledProvider = base::RepeatingCallback<bool(void)>;

  PredictionManager(
      base::WeakPtr<OptimizationGuideStore> model_and_features_store,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      bool off_the_record,
      const std::string& application_locale,
      const base::FilePath& models_dir_path,
      OptimizationGuideLogger* optimization_guide_logger,
      BackgroundDownloadServiceProvider background_download_service_provider,
      ComponentUpdatesEnabledProvider component_updates_enabled_provider);

  PredictionManager(const PredictionManager&) = delete;
  PredictionManager& operator=(const PredictionManager&) = delete;

  ~PredictionManager() override;

  // Adds an observer for updates to the model for |optimization_target|.
  //
  // It is assumed that any model retrieved this way will be passed to the
  // Machine Learning Service for inference.
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      const absl::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer);

  // Removes an observer for updates to the model for |optimization_target|.
  //
  // If |observer| is registered for multiple targets, |observer| must be
  // removed for all observed targets for in order for it to be fully
  // removed from receiving any calls.
  void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      OptimizationTargetModelObserver* observer);

  // Set the prediction model fetcher for testing.
  void SetPredictionModelFetcherForTesting(
      std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher);

  PredictionModelFetcher* prediction_model_fetcher() const {
    return prediction_model_fetcher_.get();
  }

  // Set the prediction model download manager for testing.
  void SetPredictionModelDownloadManagerForTesting(
      std::unique_ptr<PredictionModelDownloadManager>
          prediction_model_download_manager);

  PredictionModelDownloadManager* prediction_model_download_manager() const {
    return prediction_model_download_manager_.get();
  }

  base::WeakPtr<OptimizationGuideStore> model_and_features_store() const {
    return model_and_features_store_;
  }

  // Return the optimization targets that are registered.
  base::flat_set<proto::OptimizationTarget> GetRegisteredOptimizationTargets()
      const;

  // Override |clock_| for testing.
  void SetClockForTesting(const base::Clock* clock);

  // Override the model file returned to observers for |optimization_target|.
  // Use |TestModelInfoBuilder| to construct the model files. For
  // testing purposes only.
  void OverrideTargetModelForTesting(
      proto::OptimizationTarget optimization_target,
      std::unique_ptr<ModelInfo> model_info);

  // PredictionModelDownloadObserver:
  void OnModelReady(const proto::PredictionModel& model) override;
  void OnModelDownloadStarted(
      proto::OptimizationTarget optimization_target) override;
  void OnModelDownloadFailed(
      proto::OptimizationTarget optimization_target) override;

 protected:
  // Process |prediction_models| to be stored in the in memory optimization
  // target prediction model map for immediate use and asynchronously write the
  // models to the model and features store to be persisted.
  void UpdatePredictionModels(
      const google::protobuf::RepeatedPtrField<proto::PredictionModel>&
          prediction_models);

 private:
  friend class PredictionManagerTestBase;

  // Called on construction to initialize the prediction model.
  // |background_dowload_service_provider| can provide the
  // BackgroundDownloadService if needed to download models.
  void Initialize(
      BackgroundDownloadServiceProvider background_dowload_service_provider);

  // Called to make a request to fetch models from the remote Optimization Guide
  // Service. Used to fetch models for the registered optimization targets.
  // |is_first_model_fetch| indicates whether this is the first model fetch
  // happening at startup, and is used to record metrics.
  void FetchModels(bool is_first_model_fetch);

  // Callback when the models have been fetched from the remote Optimization
  // Guide Service and are ready for parsing. Processes the prediction models in
  // the response and stores them for use. The metadata entry containing the
  // time that updates should be fetched from the remote Optimization Guide
  // Service is updated, even when the response is empty.
  void OnModelsFetched(const std::vector<proto::ModelInfo> models_request_info,
                       absl::optional<std::unique_ptr<proto::GetModelsResponse>>
                           get_models_response_data);

  // Callback run after the model and host model features store is fully
  // initialized. The prediction manager can load models from
  // the store for registered optimization targets. |store_is_ready_| is set to
  // true.
  void OnStoreInitialized(
      BackgroundDownloadServiceProvider background_dowload_service_provider);

  // Callback run after prediction models are stored in
  // |model_and_features_store_|.
  void OnPredictionModelsStored();

  // Load models for every target in |optimization_targets| that have not yet
  // been loaded from the store.
  void LoadPredictionModels(
      const base::flat_set<proto::OptimizationTarget>& optimization_targets);

  // Callback run after a prediction model is loaded from the store.
  // |prediction_model| is used to construct a PredictionModel capable of making
  // prediction for the appropriate |optimization_target|.
  void OnLoadPredictionModel(
      proto::OptimizationTarget optimization_target,
      bool record_availability_metrics,
      std::unique_ptr<proto::PredictionModel> prediction_model);

  // Callback run after a prediction model is loaded from a command-line
  // override.
  void OnPredictionModelOverrideLoaded(
      proto::OptimizationTarget optimization_target,
      std::unique_ptr<proto::PredictionModel> prediction_model);

  // Process loaded |model| into memory. Return true if a prediction
  // model object was created and successfully stored, otherwise false.
  bool ProcessAndStoreLoadedModel(const proto::PredictionModel& model);

  // Return whether the model stored in memory for |optimization_target| should
  // be updated based on what's currently stored and |new_version|.
  bool ShouldUpdateStoredModelForTarget(
      proto::OptimizationTarget optimization_target,
      int64_t new_version) const;

  // Updates the in-memory model file for |optimization_target| to
  // |prediction_model_file|.
  void StoreLoadedModelInfo(proto::OptimizationTarget optimization_target,
                            std::unique_ptr<ModelInfo> prediction_model_file);

  // Post-processing callback invoked after processing |model|.
  void OnProcessLoadedModel(const proto::PredictionModel& model, bool success);

  // Return the time when a prediction model fetch was last attempted.
  base::Time GetLastFetchAttemptTime() const;

  // Set the last time when a prediction model fetch was last attempted to
  // |last_attempt_time|.
  void SetLastModelFetchAttemptTime(base::Time last_attempt_time);

  // Return the time when a prediction model fetch was last successfully
  // completed.
  base::Time GetLastFetchSuccessTime() const;

  // Set the last time when a fetch for prediction models last succeeded to
  // |last_success_time|.
  void SetLastModelFetchSuccessTime(base::Time last_success_time);

  // Schedule first fetch for models if enabled for this profile.
  void MaybeScheduleFirstModelFetch();

  // Schedule |fetch_timer_| to fire based on:
  // 1. The update time for models in the store and
  // 2. The last time a fetch attempt was made.
  void ScheduleModelsFetch();

  // Notifies observers of |optimization_target| that the model has been
  // updated.
  void NotifyObserversOfNewModel(proto::OptimizationTarget optimization_target,
                                 const ModelInfo& model_info);

  // A map of optimization target to the model file containing the model for the
  // target.
  base::flat_map<proto::OptimizationTarget, std::unique_ptr<ModelInfo>>
      optimization_target_model_info_map_;

  // The map from optimization targets to feature-provided metadata that have
  // been registered with the prediction manager.
  base::flat_map<proto::OptimizationTarget, absl::optional<proto::Any>>
      registered_optimization_targets_and_metadata_;

  // The map from optimization target to observers that have been registered to
  // receive model updates from the prediction manager.
  std::map<proto::OptimizationTarget,
           base::ObserverList<OptimizationTargetModelObserver>>
      registered_observers_for_optimization_targets_;

  // The fetcher that handles making requests to update the models and host
  // model features from the remote Optimization Guide Service.
  std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher_;

  // The downloader that handles making requests to download the prediction
  // models. Can be null if model downloading is disabled.
  std::unique_ptr<PredictionModelDownloadManager>
      prediction_model_download_manager_;

  // TODO(crbug/1183507): Remove host model features store and all relevant
  // code, and deprecate the proto field too.
  // The optimization guide store that contains prediction models and host
  // model features from the remote Optimization Guide Service.
  base::WeakPtr<OptimizationGuideStore> model_and_features_store_;

  // A stored response from a model and host model features fetch used to hold
  // models to be stored once host model features are processed and stored.
  std::unique_ptr<proto::GetModelsResponse> get_models_response_data_to_store_;

  // The URL loader factory used for fetching model and host feature updates
  // from the remote Optimization Guide Service.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The logger that plumbs the debug logs to the optimization guide
  // internals page. Not owned. Guaranteed to outlive |this|, since the logger
  // and |this| are owned by the optimization guide keyed service.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  // A reference to the PrefService for this profile. Not owned.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // The repeating callback that will be used to determine if component updates
  // are enabled.
  ComponentUpdatesEnabledProvider component_updates_enabled_provider_;

  // Time the prediction manager got initialized.
  base::TimeTicks init_time_;

  // The timer used to schedule fetching prediction models and host model
  // features from the remote Optimization Guide Service.
  base::OneShotTimer fetch_timer_;

  // The clock used to schedule fetching from the remote Optimization Guide
  // Service.
  raw_ptr<const base::Clock> clock_;

  // Whether the |model_and_features_store_| is initialized and ready for use.
  bool store_is_ready_ = false;

  // Whether host model features have been loaded from the store and are ready
  // for use.
  bool host_model_features_loaded_ = false;

  // Whether the profile for this PredictionManager is off the record.
  bool off_the_record_ = false;

  // The locale of the application.
  std::string application_locale_;

  // The path to the directory containing the models.
  base::FilePath models_dir_path_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<PredictionManager> ui_weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MANAGER_H_
