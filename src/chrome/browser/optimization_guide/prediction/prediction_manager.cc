// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model_download_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/common/chrome_paths.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/prediction_model.h"
#include "components/optimization_guide/core/prediction_model_fetcher.h"
#include "components/optimization_guide/core/prediction_model_file.h"
#include "components/optimization_guide/core/store_update_data.h"
#include "components/optimization_guide/core/top_host_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

// Returns true if |optimization_target_decision| reflects that the model had
// already been evaluated.
bool ShouldUseCurrentOptimizationTargetDecision(
    optimization_guide::OptimizationTargetDecision
        optimization_target_decision) {
  switch (optimization_target_decision) {
    case optimization_guide::OptimizationTargetDecision::kPageLoadMatches:
    case optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch:
    case optimization_guide::OptimizationTargetDecision::
        kModelPredictionHoldback:
      return true;
    case optimization_guide::OptimizationTargetDecision::
        kModelNotAvailableOnClient:
    case optimization_guide::OptimizationTargetDecision::kUnknown:
    case optimization_guide::OptimizationTargetDecision::kDeciderNotInitialized:
      return false;
  }
}

// Delay between retries on failed fetch and store of prediction models and
// host model features from the remote Optimization Guide Service.
constexpr base::TimeDelta kFetchRetryDelay = base::TimeDelta::FromMinutes(16);

// The amount of time to wait after a successful fetch of models and host model
// features before requesting an update from the remote Optimization Guide
// Service.
constexpr base::TimeDelta kUpdateModelsAndFeaturesDelay =
    base::TimeDelta::FromHours(24);

// Provide a random time delta in seconds before fetching models and host model
// features.
base::TimeDelta RandomFetchDelay() {
  return base::TimeDelta::FromSeconds(base::RandInt(
      optimization_guide::features::PredictionModelFetchRandomMinDelaySecs(),
      optimization_guide::features::PredictionModelFetchRandomMaxDelaySecs()));
}

// Util class for recording the state of a prediction model. The result is
// recorded when it goes out of scope and its destructor is called.
class ScopedPredictionManagerModelStatusRecorder {
 public:
  explicit ScopedPredictionManagerModelStatusRecorder(
      optimization_guide::proto::OptimizationTarget optimization_target)
      : status_(optimization_guide::PredictionManagerModelStatus::kUnknown),
        optimization_target_(optimization_target) {}

  ~ScopedPredictionManagerModelStatusRecorder() {
    DCHECK_NE(status_,
              optimization_guide::PredictionManagerModelStatus::kUnknown);
    base::UmaHistogramEnumeration(
        "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus",
        status_);

    base::UmaHistogramEnumeration(
        "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        status_);
  }

  void set_status(optimization_guide::PredictionManagerModelStatus status) {
    status_ = status;
  }

 private:
  optimization_guide::PredictionManagerModelStatus status_;
  const optimization_guide::proto::OptimizationTarget optimization_target_;
};

// Util class for recording the construction and validation of a prediction
// model. The result is recorded when it goes out of scope and its destructor is
// called.
class ScopedPredictionModelConstructionAndValidationRecorder {
 public:
  explicit ScopedPredictionModelConstructionAndValidationRecorder(
      optimization_guide::proto::OptimizationTarget optimization_target)
      : validation_start_time_(base::TimeTicks::Now()),
        optimization_target_(optimization_target) {}

  ~ScopedPredictionModelConstructionAndValidationRecorder() {
    base::UmaHistogramBoolean("OptimizationGuide.IsPredictionModelValid",
                              is_valid_);
    base::UmaHistogramBoolean(
        "OptimizationGuide.IsPredictionModelValid." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        is_valid_);

    // Only record the timing if the model is valid and was able to be
    // constructed.
    if (is_valid_) {
      base::TimeDelta validation_latency =
          base::TimeTicks::Now() - validation_start_time_;
      base::UmaHistogramTimes(
          "OptimizationGuide.PredictionModelValidationLatency",
          validation_latency);
      base::UmaHistogramTimes(
          "OptimizationGuide.PredictionModelValidationLatency." +
              optimization_guide::GetStringNameForOptimizationTarget(
                  optimization_target_),
          validation_latency);
    }
  }

  void set_is_valid(bool is_valid) { is_valid_ = is_valid; }

 private:
  bool is_valid_ = true;
  const base::TimeTicks validation_start_time_;
  const optimization_guide::proto::OptimizationTarget optimization_target_;
};

void RecordModelUpdateVersion(
    const optimization_guide::proto::ModelInfo& model_info) {
  base::UmaHistogramSparse(
      "OptimizationGuide.PredictionModelUpdateVersion." +
          optimization_guide::GetStringNameForOptimizationTarget(
              model_info.optimization_target()),
      model_info.version());
}

void RecordModelTypeChanged(
    optimization_guide::proto::OptimizationTarget optimization_target,
    bool changed) {
  base::UmaHistogramBoolean(
      "OptimizationGuide.PredictionManager.ModelTypeChanged." +
          optimization_guide::GetStringNameForOptimizationTarget(
              optimization_target),
      changed);
}

// Returns whether models and host model features should be fetched from the
// remote Optimization Guide Service.
bool ShouldFetchModelsAndHostModelFeatures(Profile* profile) {
  return optimization_guide::features::IsRemoteFetchingEnabled() &&
         !profile->IsOffTheRecord();
}

}  // namespace

namespace optimization_guide {

struct PredictionDecisionParams {
  PredictionDecisionParams(
      base::WeakPtr<OptimizationGuideNavigationData> navigation_data,
      proto::OptimizationTarget optimization_target,
      OptimizationTargetDecisionCallback callback,
      int64_t version,
      base::TimeTicks model_evaluation_start_time)
      : navigation_data(navigation_data),
        optimization_target(optimization_target),
        callback(std::move(callback)),
        version(version),
        model_evaluation_start_time(model_evaluation_start_time) {}

  ~PredictionDecisionParams() = default;

  PredictionDecisionParams(const PredictionDecisionParams&) = delete;
  PredictionDecisionParams& operator=(const PredictionDecisionParams&) = delete;

  // Will store relevant prediction results, if not null.
  base::WeakPtr<OptimizationGuideNavigationData> navigation_data;
  // Target of the prediction.
  proto::OptimizationTarget optimization_target;
  // Callback to be invoked once a OptimizationTargetDecision is made.
  OptimizationTargetDecisionCallback callback;
  // Model version.
  int64_t version;
  // Time when the model evaluation is initiated.
  base::TimeTicks model_evaluation_start_time;
};

PredictionManager::PredictionManager(
    OptimizationGuideStore* model_and_features_store,
    TopHostProvider* top_host_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    Profile* profile)
    : host_model_features_cache_(
          std::max(features::MaxHostModelFeaturesCacheSize(), size_t(1))),
      prediction_model_download_manager_(nullptr),
      top_host_provider_(top_host_provider),
      model_and_features_store_(model_and_features_store),
      url_loader_factory_(url_loader_factory),
      pref_service_(pref_service),
      profile_(profile),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK(model_and_features_store_);

  Initialize();
}

PredictionManager::~PredictionManager() {
  if (prediction_model_download_manager_)
    prediction_model_download_manager_->RemoveObserver(this);
  g_browser_process->network_quality_tracker()
      ->RemoveEffectiveConnectionTypeObserver(this);
}

void PredictionManager::Initialize() {
  g_browser_process->network_quality_tracker()
      ->AddEffectiveConnectionTypeObserver(this);
  model_and_features_store_->Initialize(
      switches::ShouldPurgeModelAndFeaturesStoreOnStartup(),
      base::BindOnce(&PredictionManager::OnStoreInitialized,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void PredictionManager::UpdateFCPSessionStatistics(base::TimeDelta fcp) {
  previous_load_fcp_ms_ = fcp.InMillisecondsF();
  session_fcp_.AddSample(*previous_load_fcp_ms_);
  pref_service_->SetDouble(prefs::kSessionStatisticFCPMean,
                           session_fcp_.GetMean());
  pref_service_->SetDouble(prefs::kSessionStatisticFCPStdDev,
                           session_fcp_.GetStdDev());
}

void PredictionManager::RegisterOptimizationTargets(
    const std::vector<
        std::pair<proto::OptimizationTarget, base::Optional<proto::Any>>>&
        optimization_targets_and_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (optimization_targets_and_metadata.empty())
    return;

  base::flat_set<proto::OptimizationTarget> new_optimization_targets;
  for (const auto& optimization_target_and_metadata :
       optimization_targets_and_metadata) {
    proto::OptimizationTarget optimization_target =
        optimization_target_and_metadata.first;
    if (optimization_target == proto::OPTIMIZATION_TARGET_UNKNOWN)
      continue;
    if (registered_optimization_targets_and_metadata_.contains(
            optimization_target)) {
      continue;
    }
    registered_optimization_targets_and_metadata_.emplace(
        optimization_target_and_metadata);
    new_optimization_targets.insert(optimization_target);
  }

  // Before loading/fetching models and features, the store must be ready.
  if (!store_is_ready_)
    return;

  // Only proceed if there are newly registered targets to load/fetch models and
  // features for. Otherwise, the registered targets will have models loaded
  // when the store was initialized.
  if (new_optimization_targets.empty())
    return;

  // If no fetch is scheduled, maybe schedule one.
  if (!fetch_timer_.IsRunning())
    MaybeScheduleModelAndHostModelFeaturesFetch();

  // Start loading the host model features if they are not already.
  if (!host_model_features_loaded_) {
    LoadHostModelFeatures();
    return;
  }
  // Otherwise, the host model features are loaded, so load prediction models
  // for any newly registered targets.
  LoadPredictionModels(new_optimization_targets);
}

void PredictionManager::AddObserverForOptimizationTargetModel(
    proto::OptimizationTarget optimization_target,
    const base::Optional<proto::Any>& model_metadata,
    OptimizationTargetModelObserver* observer) {
  DCHECK(registered_observers_for_optimization_targets_.find(
             optimization_target) ==
         registered_observers_for_optimization_targets_.end());

  // As DCHECKS don't run in the wild, just do not register the observer if
  // something is already registered for the type. Otherwise, file reads may
  // blow up.
  if (registered_observers_for_optimization_targets_.find(
          optimization_target) !=
      registered_observers_for_optimization_targets_.end()) {
    DLOG(ERROR) << "Did not add observer for optimization target "
                << static_cast<int>(optimization_target)
                << " since an observer for the target was already registered";
    return;
  }

  registered_observers_for_optimization_targets_[optimization_target]
      .AddObserver(observer);

  // Notify observer of existing model file path.
  auto model_file_it =
      optimization_target_prediction_model_file_map_.find(optimization_target);
  if (model_file_it != optimization_target_prediction_model_file_map_.end()) {
    observer->OnModelFileUpdated(optimization_target,
                                 model_file_it->second->GetModelMetadata(),
                                 model_file_it->second->GetModelFilePath());
  }

  RegisterOptimizationTargets({{optimization_target, model_metadata}});
}

void PredictionManager::RemoveObserverForOptimizationTargetModel(
    proto::OptimizationTarget optimization_target,
    OptimizationTargetModelObserver* observer) {
  auto observers_it =
      registered_observers_for_optimization_targets_.find(optimization_target);
  if (observers_it == registered_observers_for_optimization_targets_.end())
    return;

  observers_it->second.RemoveObserver(observer);
}

base::Optional<float> PredictionManager::GetValueForClientFeature(
    const std::string& model_feature,
    content::NavigationHandle* navigation_handle,
    const base::flat_map<proto::ClientModelFeature, float>&
        override_client_model_feature_values) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  proto::ClientModelFeature client_model_feature;
  if (!proto::ClientModelFeature_Parse(model_feature, &client_model_feature))
    return base::nullopt;

  auto cmf_value_it =
      override_client_model_feature_values.find(client_model_feature);
  if (cmf_value_it != override_client_model_feature_values.end())
    return cmf_value_it->second;

  base::Optional<float> value;

  switch (client_model_feature) {
    case proto::CLIENT_MODEL_FEATURE_UNKNOWN: {
      return base::nullopt;
    }
    case proto::CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE: {
      value = static_cast<float>(current_effective_connection_type_);
      break;
    }
    case proto::CLIENT_MODEL_FEATURE_PAGE_TRANSITION: {
      value = static_cast<float>(navigation_handle->GetPageTransition());
      break;
    }
    case proto::CLIENT_MODEL_FEATURE_SITE_ENGAGEMENT_SCORE: {
      Profile* profile = Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
      site_engagement::SiteEngagementService* engagement_service =
          site_engagement::SiteEngagementService::Get(profile);
      // Precision loss is acceptable/expected for prediction models.
      value = static_cast<float>(
          engagement_service->GetScore(navigation_handle->GetURL()));
      break;
    }
    case proto::CLIENT_MODEL_FEATURE_SAME_ORIGIN_NAVIGATION: {
      OptimizationGuideNavigationData* nav_data =
          OptimizationGuideNavigationData::GetFromNavigationHandle(
              navigation_handle);

      bool is_same_origin = nav_data && nav_data->is_same_origin_navigation();

      LOCAL_HISTOGRAM_BOOLEAN(
          "OptimizationGuide.PredictionManager.IsSameOrigin", is_same_origin);

      value = static_cast<float>(is_same_origin);
      break;
    }
    case proto::CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_MEAN: {
      value = session_fcp_.GetNumberOfSamples() == 0
                  ? static_cast<float>(pref_service_->GetDouble(
                        prefs::kSessionStatisticFCPMean))
                  : session_fcp_.GetMean();
      break;
    }
    case proto::
        CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_STANDARD_DEVIATION: {
      value = session_fcp_.GetNumberOfSamples() == 0
                  ? static_cast<float>(pref_service_->GetDouble(
                        prefs::kSessionStatisticFCPStdDev))
                  : session_fcp_.GetStdDev();
      break;
    }
    case proto::
        CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_PREVIOUS_PAGE_LOAD: {
      value = previous_load_fcp_ms_.value_or(static_cast<float>(
          pref_service_->GetDouble(prefs::kSessionStatisticFCPMean)));
      break;
    }
    default: {
      return base::nullopt;
    }
  }

  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle);
  if (value && navigation_data) {
    navigation_data->SetValueForModelFeature(client_model_feature, *value);
    return value;
  }
  return base::nullopt;
}

base::flat_map<std::string, float> PredictionManager::BuildFeatureMap(
    content::NavigationHandle* navigation_handle,
    const base::flat_set<std::string>& model_features,
    const base::flat_map<proto::ClientModelFeature, float>&
        override_client_model_feature_values) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (model_features.empty())
    return {};

  const base::flat_map<std::string, float>* host_model_features = nullptr;

  std::string host = navigation_handle->GetURL().host();
  auto it = host_model_features_cache_.Get(host);
  if (it != host_model_features_cache_.end())
    host_model_features = &(it->second);

  UMA_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.HasHostModelFeaturesForHost",
      host_model_features != nullptr);

  // If the feature is not implemented by the client, it is assumed that it is a
  // host model feature we have in the map. If it is not in either, a default is
  // created for it. This ensures that the prediction model will have values for
  // every feature that it requires to be evaluated.
  std::vector<std::pair<std::string, float>> feature_map;
  feature_map.reserve(model_features.size());
  for (const auto& model_feature : model_features) {
    base::Optional<float> value = GetValueForClientFeature(
        model_feature, navigation_handle, override_client_model_feature_values);
    if (!value && host_model_features) {
      const auto it = host_model_features->find(model_feature);
      if (it != host_model_features->end())
        value = it->second;
    }
    feature_map.emplace_back(model_feature, value.value_or(-1.0f));
  }
  return {base::sorted_unique, std::move(feature_map)};
}

OptimizationTargetDecision PredictionManager::ShouldTargetNavigation(
    content::NavigationHandle* navigation_handle,
    proto::OptimizationTarget optimization_target,
    const base::flat_map<proto::ClientModelFeature, float>&
        override_client_model_feature_values) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(navigation_handle->GetURL().SchemeIsHTTPOrHTTPS());

  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle);
  if (navigation_data) {
    base::Optional<optimization_guide::OptimizationTargetDecision>
        optimization_target_decision =
            navigation_data->GetDecisionForOptimizationTarget(
                optimization_target);
    if (optimization_target_decision.has_value() &&
        ShouldUseCurrentOptimizationTargetDecision(
            *optimization_target_decision)) {
      return *optimization_target_decision;
    }
  }
  if (!registered_optimization_targets_and_metadata_.contains(
          optimization_target)) {
    return OptimizationTargetDecision::kUnknown;
  }

  ScopedPredictionManagerModelStatusRecorder model_status_recorder(
      optimization_target);
  auto it = optimization_target_prediction_model_map_.find(optimization_target);
  if (it == optimization_target_prediction_model_map_.end()) {
    if (store_is_ready_ && model_and_features_store_) {
      OptimizationGuideStore::EntryKey model_entry_key;
      if (model_and_features_store_->FindPredictionModelEntryKey(
              optimization_target, &model_entry_key)) {
        model_status_recorder.set_status(
            PredictionManagerModelStatus::kStoreAvailableModelNotLoaded);
      } else {
        model_status_recorder.set_status(
            PredictionManagerModelStatus::kStoreAvailableNoModelForTarget);
      }
    } else {
      model_status_recorder.set_status(
          PredictionManagerModelStatus::kStoreUnavailableModelUnknown);
    }
    return OptimizationTargetDecision::kModelNotAvailableOnClient;
  }
  model_status_recorder.set_status(
      PredictionManagerModelStatus::kModelAvailable);
  PredictionModel* prediction_model = it->second.get();

  base::flat_map<std::string, float> feature_map =
      BuildFeatureMap(navigation_handle, prediction_model->GetModelFeatures(),
                      override_client_model_feature_values);

  base::TimeTicks model_evaluation_start_time = base::TimeTicks::Now();
  double prediction_score = 0.0;
  optimization_guide::OptimizationTargetDecision target_decision =
      prediction_model->Predict(feature_map, &prediction_score);
  if (target_decision != OptimizationTargetDecision::kUnknown) {
    UmaHistogramTimes(
        "OptimizationGuide.PredictionModelEvaluationLatency." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target),
        base::TimeTicks::Now() - model_evaluation_start_time);
  }

  if (navigation_data) {
    navigation_data->SetModelVersionForOptimizationTarget(
        optimization_target, prediction_model->GetVersion());
    navigation_data->SetModelPredictionScoreForOptimizationTarget(
        optimization_target, prediction_score);
  }

  if (optimization_guide::features::
          ShouldOverrideOptimizationTargetDecisionForMetricsPurposes(
              optimization_target)) {
    return optimization_guide::OptimizationTargetDecision::
        kModelPredictionHoldback;
  }

  return target_decision;
}

void PredictionManager::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_effective_connection_type_ = effective_connection_type;
}

base::flat_set<proto::OptimizationTarget>
PredictionManager::GetRegisteredOptimizationTargets() const {
  base::flat_set<proto::OptimizationTarget> optimization_targets;
  for (const auto& optimization_target_and_metadata :
       registered_optimization_targets_and_metadata_) {
    optimization_targets.insert(optimization_target_and_metadata.first);
  }
  return optimization_targets;
}

PredictionModel* PredictionManager::GetPredictionModelForTesting(
    proto::OptimizationTarget optimization_target) const {
  auto it = optimization_target_prediction_model_map_.find(optimization_target);
  if (it != optimization_target_prediction_model_map_.end())
    return it->second.get();
  return nullptr;
}

const HostModelFeaturesMRUCache*
PredictionManager::GetHostModelFeaturesForTesting() const {
  return &host_model_features_cache_;
}

void PredictionManager::SetPredictionModelFetcherForTesting(
    std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher) {
  prediction_model_fetcher_ = std::move(prediction_model_fetcher);
}

void PredictionManager::SetPredictionModelDownloadManagerForTesting(
    std::unique_ptr<PredictionModelDownloadManager>
        prediction_model_download_manager) {
  prediction_model_download_manager_ =
      std::move(prediction_model_download_manager);
}

void PredictionManager::FetchModelsAndHostModelFeatures() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ShouldFetchModelsAndHostModelFeatures(profile_))
    return;

  ScheduleModelsAndHostModelFeaturesFetch();

  // We cannot download any models from the server, so don't refresh them.
  if (prediction_model_download_manager_) {
    bool download_service_available =
        prediction_model_download_manager_->IsAvailableForDownloads();
    base::UmaHistogramBoolean(
        "OptimizationGuide.PredictionManager."
        "DownloadServiceAvailabilityBlockedFetch",
        !download_service_available);
    if (!download_service_available)
      return;
  }

  // Models and host model features should not be fetched if there are no
  // optimization targets registered.
  if (registered_optimization_targets_and_metadata_.empty())
    return;

  // Cancel all pending downloads since the server will probably give us new
  // ones to fetch.
  if (prediction_model_download_manager_)
    prediction_model_download_manager_->CancelAllPendingDownloads();

  std::vector<std::string> top_hosts;
  std::vector<proto::FieldTrial> active_field_trials;
  // Top hosts and active field trials convey some sort of user information, so
  // ensure that the user has opted into the right permissions before adding
  // these fields to the request.
  if (IsUserPermittedToFetchFromRemoteOptimizationGuide(
          profile_->IsOffTheRecord(), pref_service_)) {
    if (top_host_provider_) {
      top_hosts = top_host_provider_->GetTopHosts();

      // Remove hosts that are already available in the host model features
      // cache. The request should still be made in case there is a new model or
      // a model that does not rely on host model features to be fetched.
      auto it = top_hosts.begin();
      while (it != top_hosts.end()) {
        if (host_model_features_cache_.Peek(*it) !=
            host_model_features_cache_.end()) {
          it = top_hosts.erase(it);
          continue;
        }
        ++it;
      }
    }
    google::protobuf::RepeatedPtrField<proto::FieldTrial> current_field_trials =
        GetActiveFieldTrialsAllowedForFetch();
    active_field_trials = std::vector<proto::FieldTrial>(
        {current_field_trials.begin(), current_field_trials.end()});
  }

  if (!prediction_model_fetcher_) {
    prediction_model_fetcher_ = std::make_unique<PredictionModelFetcher>(
        url_loader_factory_,
        features::GetOptimizationGuideServiceGetModelsURL(),
        content::GetNetworkConnectionTracker());
  }

  std::vector<proto::ModelInfo> models_info = std::vector<proto::ModelInfo>();

  proto::ModelInfo base_model_info;
  for (auto client_model_feature = proto::ClientModelFeature_MIN + 1;
       client_model_feature <= proto::ClientModelFeature_MAX;
       client_model_feature++) {
    if (proto::ClientModelFeature_IsValid(client_model_feature)) {
      base_model_info.add_supported_model_features(
          static_cast<proto::ClientModelFeature>(client_model_feature));
    }
  }
  base_model_info.add_supported_model_types(proto::MODEL_TYPE_DECISION_TREE);
  if (features::IsModelDownloadingEnabled())
    base_model_info.add_supported_model_types(proto::MODEL_TYPE_TFLITE_2_3_0);

  // For now, we will fetch for all registered optimization targets.
  for (const auto& optimization_target_and_metadata :
       registered_optimization_targets_and_metadata_) {
    proto::ModelInfo model_info(base_model_info);
    model_info.set_optimization_target(optimization_target_and_metadata.first);
    if (optimization_target_and_metadata.second.has_value()) {
      *model_info.mutable_model_metadata() =
          *optimization_target_and_metadata.second;
    }

    auto it = optimization_target_prediction_model_map_.find(
        optimization_target_and_metadata.first);
    if (it != optimization_target_prediction_model_map_.end())
      model_info.set_version(it->second.get()->GetVersion());

    auto file_it = optimization_target_prediction_model_file_map_.find(
        optimization_target_and_metadata.first);
    if (file_it != optimization_target_prediction_model_file_map_.end())
      model_info.set_version(file_it->second.get()->GetVersion());

    models_info.push_back(model_info);
  }

  prediction_model_fetcher_->FetchOptimizationGuideServiceModels(
      models_info, top_hosts, active_field_trials,
      optimization_guide::proto::CONTEXT_BATCH_UPDATE,
      base::BindOnce(&PredictionManager::OnModelsAndHostFeaturesFetched,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void PredictionManager::OnModelsAndHostFeaturesFetched(
    base::Optional<std::unique_ptr<proto::GetModelsResponse>>
        get_models_response_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!get_models_response_data)
    return;

  // Update host model features, even if empty so the store metadata
  // that contains the update time for new models and features to be fetched
  // from the remote Optimization Guide Service is updated.
  UpdateHostModelFeatures((*get_models_response_data)->host_model_features());

  if ((*get_models_response_data)->models_size() > 0) {
    // Stash the response so the models can be stored once the host
    // model features are stored.
    get_models_response_data_to_store_ = std::move(*get_models_response_data);
  }

  fetch_timer_.Stop();
  fetch_timer_.Start(
      FROM_HERE, kUpdateModelsAndFeaturesDelay, this,
      &PredictionManager::ScheduleModelsAndHostModelFeaturesFetch);
}

void PredictionManager::UpdateHostModelFeatures(
    const google::protobuf::RepeatedPtrField<proto::HostModelFeatures>&
        host_model_features) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<StoreUpdateData> host_model_features_update_data =
      StoreUpdateData::CreateHostModelFeaturesStoreUpdateData(
          /*update_time=*/clock_->Now() + kUpdateModelsAndFeaturesDelay,
          /*expiry_time=*/clock_->Now() +
              features::StoredHostModelFeaturesFreshnessDuration());
  for (const auto& host_model_features : host_model_features) {
    if (ProcessAndStoreHostModelFeatures(host_model_features)) {
      host_model_features_update_data->CopyHostModelFeaturesIntoUpdateData(
          host_model_features);
    }
  }

  model_and_features_store_->UpdateHostModelFeatures(
      std::move(host_model_features_update_data),
      base::BindOnce(&PredictionManager::OnHostModelFeaturesStored,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<PredictionModel> PredictionManager::CreatePredictionModel(
    const proto::PredictionModel& model) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return PredictionModel::Create(model);
}

void PredictionManager::UpdatePredictionModels(
    const google::protobuf::RepeatedPtrField<proto::PredictionModel>&
        prediction_models) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<StoreUpdateData> prediction_model_update_data =
      StoreUpdateData::CreatePredictionModelStoreUpdateData(
          clock_->Now() + features::StoredModelsInactiveDuration());
  bool has_models_to_update = false;
  for (const auto& model : prediction_models) {
    if (model.has_model() && !model.model().download_url().empty()) {
      if (prediction_model_download_manager_) {
        GURL download_url(model.model().download_url());
        if (download_url.is_valid()) {
          prediction_model_download_manager_->StartDownload(download_url);
        }
        base::UmaHistogramBoolean(
            "OptimizationGuide.PredictionManager.IsDownloadUrlValid",
            download_url.is_valid());
      }

      // Skip over models that have a download URL since they will be updated
      // once the download has completed successfully.
      continue;
    }
    if (!model.has_model()) {
      // We already have this updated model, so don't update in store.
      continue;
    }

    has_models_to_update = true;
    // Storing the model regardless of whether the model is valid or not. Model
    // will be removed from store if it fails to load.
    prediction_model_update_data->CopyPredictionModelIntoUpdateData(model);
    RecordModelUpdateVersion(model.model_info());
    OnLoadPredictionModel(std::make_unique<proto::PredictionModel>(model));
  }

  if (has_models_to_update) {
    model_and_features_store_->UpdatePredictionModels(
        std::move(prediction_model_update_data),
        base::BindOnce(&PredictionManager::OnPredictionModelsStored,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  }
}

void PredictionManager::OnModelReady(const proto::PredictionModel& model) {
  DCHECK(model.model_info().has_version() &&
         model.model_info().has_optimization_target());

  RecordModelUpdateVersion(model.model_info());

  // Store the received model in the store.
  std::unique_ptr<StoreUpdateData> prediction_model_update_data =
      StoreUpdateData::CreatePredictionModelStoreUpdateData(
          clock_->Now() + features::StoredModelsInactiveDuration());
  prediction_model_update_data->CopyPredictionModelIntoUpdateData(model);
  model_and_features_store_->UpdatePredictionModels(
      std::move(prediction_model_update_data),
      base::BindOnce(&PredictionManager::OnPredictionModelsStored,
                     ui_weak_ptr_factory_.GetWeakPtr()));

  if (registered_optimization_targets_and_metadata_.contains(
          model.model_info().optimization_target())) {
    OnLoadPredictionModel(std::make_unique<proto::PredictionModel>(model));
  }
}

void PredictionManager::NotifyObserversOfNewModelPath(
    proto::OptimizationTarget optimization_target,
    const base::Optional<proto::Any>& model_metadata,
    const base::FilePath& file_path) const {
  auto observers_it =
      registered_observers_for_optimization_targets_.find(optimization_target);
  if (observers_it == registered_observers_for_optimization_targets_.end())
    return;

  for (auto& observer : observers_it->second)
    observer.OnModelFileUpdated(optimization_target, model_metadata, file_path);
}

void PredictionManager::OnPredictionModelsStored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true);
}

void PredictionManager::OnHostModelFeaturesStored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true);

  if (get_models_response_data_to_store_ &&
      get_models_response_data_to_store_->models_size() > 0) {
    UpdatePredictionModels(get_models_response_data_to_store_->models());
  }
  // Clear any data remaining in the stored get models response.
  get_models_response_data_to_store_.reset();

  // Purge any expired host model features and inactive models from the store.
  model_and_features_store_->PurgeExpiredHostModelFeatures();
  model_and_features_store_->PurgeInactiveModels();

  fetch_timer_.Stop();
  ScheduleModelsAndHostModelFeaturesFetch();
}

void PredictionManager::OnStoreInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  store_is_ready_ = true;

  // Create the download manager here if we are allowed to.
  if (features::IsModelDownloadingEnabled() && !profile_->IsOffTheRecord() &&
      !prediction_model_download_manager_) {
    base::FilePath models_dir;
    base::PathService::Get(chrome::DIR_OPTIMIZATION_GUIDE_PREDICTION_MODELS,
                           &models_dir);
    prediction_model_download_manager_ =
        std::make_unique<PredictionModelDownloadManager>(
            DownloadServiceFactory::GetForKey(profile_->GetProfileKey()),
            models_dir,
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
    prediction_model_download_manager_->AddObserver(this);
  }

  // Only load host model features if there are optimization targets registered.
  if (registered_optimization_targets_and_metadata_.empty())
    return;

  // The store is ready so start loading host model features and the models for
  // the registered optimization targets.  Once the host model features are
  // loaded, prediction models for the registered optimization targets will be
  // loaded.
  LoadHostModelFeatures();

  MaybeScheduleModelAndHostModelFeaturesFetch();
}

void PredictionManager::LoadHostModelFeatures() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Load the host model features first, each prediction model requires the set
  // of host model features to be known before creation.
  model_and_features_store_->LoadAllHostModelFeatures(
      base::BindOnce(&PredictionManager::OnLoadHostModelFeatures,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void PredictionManager::OnLoadHostModelFeatures(
    std::unique_ptr<std::vector<proto::HostModelFeatures>>
        all_host_model_features) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the store returns an empty vector of host model features, the store
  // contains no host model features. However, the load is otherwise complete
  // and prediction models can be loaded but they will require no host model
  // feature information.
  host_model_features_loaded_ = true;
  if (all_host_model_features) {
    for (const auto& host_model_features : *all_host_model_features)
      ProcessAndStoreHostModelFeatures(host_model_features);
  }
  UMA_HISTOGRAM_COUNTS_1000(
      "OptimizationGuide.PredictionManager.HostModelFeaturesMapSize",
      host_model_features_cache_.size());

  // Load the prediction models for all the registered optimization targets now
  // that it is not blocked by loading the host model features.
  LoadPredictionModels(GetRegisteredOptimizationTargets());
}

void PredictionManager::LoadPredictionModels(
    const base::flat_set<proto::OptimizationTarget>& optimization_targets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host_model_features_loaded_);

  OptimizationGuideStore::EntryKey model_entry_key;
  for (const auto& optimization_target : optimization_targets) {
    // The prediction model for this optimization target has already been
    // loaded.
    if (optimization_target_prediction_model_map_.contains(
            optimization_target)) {
      continue;
    }
    if (!model_and_features_store_->FindPredictionModelEntryKey(
            optimization_target, &model_entry_key)) {
      continue;
    }
    model_and_features_store_->LoadPredictionModel(
        model_entry_key,
        base::BindOnce(&PredictionManager::OnLoadPredictionModel,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  }
}

void PredictionManager::OnLoadPredictionModel(
    std::unique_ptr<proto::PredictionModel> model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!model)
    return;

  bool success = ProcessAndStoreLoadedModel(*model);
  OnProcessLoadedModel(*model, success);
}

void PredictionManager::OnProcessLoadedModel(
    const proto::PredictionModel& model,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (success) {
    base::UmaHistogramSparse(
        "OptimizationGuide.PredictionModelLoadedVersion." +
            optimization_guide::GetStringNameForOptimizationTarget(
                model.model_info().optimization_target()),
        model.model_info().version());
    return;
  }

  // Remove model from store if it exists.
  OptimizationGuideStore::EntryKey model_entry_key;
  if (model_and_features_store_->FindPredictionModelEntryKey(
          model.model_info().optimization_target(), &model_entry_key)) {
    LOCAL_HISTOGRAM_BOOLEAN(
        "OptimizationGuide.PredictionModelRemoved." +
            optimization_guide::GetStringNameForOptimizationTarget(
                model.model_info().optimization_target()),
        true);
    model_and_features_store_->RemovePredictionModelFromEntryKey(
        model_entry_key);
  }
}

bool PredictionManager::ProcessAndStoreLoadedModel(
    const proto::PredictionModel& model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!model.model_info().has_optimization_target())
    return false;
  if (!model.model_info().has_version())
    return false;
  if (!model.has_model())
    return false;
  if (!registered_optimization_targets_and_metadata_.contains(
          model.model_info().optimization_target())) {
    return false;
  }

  ScopedPredictionModelConstructionAndValidationRecorder
      prediction_model_recorder(model.model_info().optimization_target());
  std::unique_ptr<PredictionModelFile> prediction_model_file =
      PredictionModelFile::Create(model);
  std::unique_ptr<PredictionModel> prediction_model =
      prediction_model_file ? nullptr : CreatePredictionModel(model);
  if (!prediction_model_file && !prediction_model) {
    prediction_model_recorder.set_is_valid(false);
    return false;
  }

  proto::OptimizationTarget optimization_target =
      model.model_info().optimization_target();

  // See if we should update the loaded model.
  if (!ShouldUpdateStoredModelForTarget(optimization_target,
                                        model.model_info().version())) {
    return true;
  }

  // Update prediction model file if that is what we have loaded.
  if (prediction_model_file) {
    StoreLoadedPredictionModelFile(optimization_target,
                                   std::move(prediction_model_file));
  }

  // Update prediction model if that is what we have loaded.
  if (prediction_model) {
    StoreLoadedPredictionModel(optimization_target,
                               std::move(prediction_model));
  }

  return true;
}

bool PredictionManager::ShouldUpdateStoredModelForTarget(
    proto::OptimizationTarget optimization_target,
    int64_t new_version) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto model_file_it =
      optimization_target_prediction_model_file_map_.find(optimization_target);
  if (model_file_it != optimization_target_prediction_model_file_map_.end())
    return model_file_it->second->GetVersion() != new_version;

  auto model_it =
      optimization_target_prediction_model_map_.find(optimization_target);
  if (model_it != optimization_target_prediction_model_map_.end())
    return model_it->second->GetVersion() != new_version;

  return true;
}

void PredictionManager::StoreLoadedPredictionModelFile(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<PredictionModelFile> prediction_model_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool has_model_for_target =
      optimization_target_prediction_model_map_.contains(optimization_target);
  RecordModelTypeChanged(optimization_target, has_model_for_target);
  if (has_model_for_target) {
    // Remove prediction model if we received the update as a model file. In
    // practice, this shouldn't happen.
    optimization_target_prediction_model_map_.erase(optimization_target);
  }

  // Notify observers of new model file path.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PredictionManager::NotifyObserversOfNewModelPath,
                     ui_weak_ptr_factory_.GetWeakPtr(), optimization_target,
                     prediction_model_file->GetModelMetadata(),
                     prediction_model_file->GetModelFilePath()));

  optimization_target_prediction_model_file_map_.insert_or_assign(
      optimization_target, std::move(prediction_model_file));
}

void PredictionManager::StoreLoadedPredictionModel(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<PredictionModel> prediction_model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool has_model_file_for_target =
      optimization_target_prediction_model_file_map_.contains(
          optimization_target);
  RecordModelTypeChanged(optimization_target, has_model_file_for_target);
  if (has_model_file_for_target) {
    // Remove prediction model file from map if we received the update as a
    // PredictionModel. In practice, this shouldn't happen.
    optimization_target_prediction_model_file_map_.erase(optimization_target);
  }
  optimization_target_prediction_model_map_.insert_or_assign(
      optimization_target, std::move(prediction_model));
}

bool PredictionManager::ProcessAndStoreHostModelFeatures(
    const proto::HostModelFeatures& host_model_features) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!host_model_features.has_host())
    return false;
  if (host_model_features.model_features_size() == 0)
    return false;

  base::flat_map<std::string, float> model_features_for_host;
  model_features_for_host.reserve(host_model_features.model_features_size());
  for (const auto& model_feature : host_model_features.model_features()) {
    if (!model_feature.has_feature_name())
      continue;
    switch (model_feature.feature_value_case()) {
      case proto::ModelFeature::kDoubleValue:
        // Loss of precision from double is acceptable for features supported
        // by the prediction models.
        model_features_for_host.emplace(
            model_feature.feature_name(),
            static_cast<float>(model_feature.double_value()));
        break;
      case proto::ModelFeature::kInt64Value:
        model_features_for_host.emplace(
            model_feature.feature_name(),
            static_cast<float>(model_feature.int64_value()));
        break;
      case proto::ModelFeature::FEATURE_VALUE_NOT_SET:
        NOTREACHED();
        break;
    }
  }
  if (model_features_for_host.empty())
    return false;
  host_model_features_cache_.Put(host_model_features.host(),
                                 model_features_for_host);
  return true;
}

void PredictionManager::MaybeScheduleModelAndHostModelFeaturesFetch() {
  if (!ShouldFetchModelsAndHostModelFeatures(profile_))
    return;

  if (optimization_guide::switches::
          ShouldOverrideFetchModelsAndFeaturesTimer()) {
    SetLastModelAndFeaturesFetchAttemptTime(clock_->Now());
    fetch_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1), this,
                       &PredictionManager::FetchModelsAndHostModelFeatures);
  } else {
    ScheduleModelsAndHostModelFeaturesFetch();
  }
}

base::Time PredictionManager::GetLastFetchAttemptTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(
          pref_service_->GetInt64(prefs::kModelAndFeaturesLastFetchAttempt)));
}

void PredictionManager::ScheduleModelsAndHostModelFeaturesFetch() {
  DCHECK(!fetch_timer_.IsRunning());
  DCHECK(store_is_ready_);
  const base::TimeDelta time_until_update_time =
      model_and_features_store_->GetHostModelFeaturesUpdateTime() -
      clock_->Now();
  const base::TimeDelta time_until_retry =
      GetLastFetchAttemptTime() + kFetchRetryDelay - clock_->Now();
  base::TimeDelta fetcher_delay =
      std::max(time_until_update_time, time_until_retry);
  if (fetcher_delay <= base::TimeDelta()) {
    SetLastModelAndFeaturesFetchAttemptTime(clock_->Now());
    fetch_timer_.Start(FROM_HERE, RandomFetchDelay(), this,
                       &PredictionManager::FetchModelsAndHostModelFeatures);
    return;
  }
  fetch_timer_.Start(
      FROM_HERE, fetcher_delay, this,
      &PredictionManager::ScheduleModelsAndHostModelFeaturesFetch);
}

void PredictionManager::SetLastModelAndFeaturesFetchAttemptTime(
    base::Time last_attempt_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(
      prefs::kModelAndFeaturesLastFetchAttempt,
      last_attempt_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void PredictionManager::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

void PredictionManager::ClearHostModelFeatures() {
  host_model_features_cache_.Clear();
  if (model_and_features_store_)
    model_and_features_store_->ClearHostModelFeaturesFromDatabase();
}

base::Optional<base::flat_map<std::string, float>>
PredictionManager::GetHostModelFeaturesForHost(const std::string& host) const {
  auto it = host_model_features_cache_.Peek(host);
  if (it == host_model_features_cache_.end())
    return base::nullopt;
  return it->second;
}

void PredictionManager::OverrideTargetDecisionForTesting(
    proto::OptimizationTarget optimization_target,
    OptimizationGuideDecision optimization_guide_decision) {
  auto it = optimization_target_prediction_model_map_.find(optimization_target);
  if (it != optimization_target_prediction_model_map_.end())
    optimization_target_prediction_model_map_.erase(it);

  // No model for |kUnknown|. This will make |ShouldTargetNavigation|
  // return an |OptimizationTargetDecision::kModelNotAvailableOnClient|,
  // which in turn yields an |OptimizationGuideDecision::kUnknown| in
  // |OptimizationGuideKeyedService::ShouldTargetNavigation|.
  if (optimization_guide_decision == OptimizationGuideDecision::kUnknown)
    return;

  // Construct a simple model that will return the provided
  // |optimization_guide_decision|.
  const double threshold = 5.0;
  const double weight = 1.0;
  double leaf_value =
      (optimization_guide_decision == OptimizationGuideDecision::kTrue)
          ? threshold + 1.0  // Value is greater than |threshold| to get |kTrue|
          : threshold - 1.0;  // Value is less than |threshold| to get |kFalse|

  std::unique_ptr<proto::PredictionModel> prediction_model =
      std::make_unique<proto::PredictionModel>();
  prediction_model->mutable_model()->mutable_threshold()->set_value(threshold);
  proto::DecisionTree* decision_tree =
      prediction_model->mutable_model()->mutable_decision_tree();
  decision_tree->set_weight(weight);
  proto::TreeNode* tree_node = decision_tree->add_nodes();
  tree_node->mutable_node_id()->set_value(0);
  tree_node->mutable_leaf()->mutable_vector()->add_value()->set_double_value(
      leaf_value);

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();

  model_info->set_version(1);
  model_info->set_optimization_target(optimization_target);
  model_info->add_supported_model_types(proto::MODEL_TYPE_DECISION_TREE);

  optimization_target_prediction_model_map_.emplace(
      optimization_target, CreatePredictionModel(*prediction_model));
}

void PredictionManager::OverrideTargetModelFileForTesting(
    proto::OptimizationTarget optimization_target,
    const base::Optional<proto::Any>& model_metadata,
    const base::FilePath& file_path) {
  proto::PredictionModel prediction_model;
  prediction_model.mutable_model_info()->set_version(1);
  prediction_model.mutable_model_info()->set_optimization_target(
      optimization_target);
  SetFilePathInPredictionModel(file_path, &prediction_model);
  if (model_metadata.has_value()) {
    *prediction_model.mutable_model_info()->mutable_model_metadata() =
        *model_metadata;
  }
  std::unique_ptr<PredictionModelFile> prediction_model_file =
      PredictionModelFile::Create(prediction_model);
  DCHECK(prediction_model_file);

  optimization_target_prediction_model_file_map_.insert_or_assign(
      optimization_target, std::move(prediction_model_file));

  NotifyObserversOfNewModelPath(optimization_target, model_metadata, file_path);
}

}  // namespace optimization_guide
