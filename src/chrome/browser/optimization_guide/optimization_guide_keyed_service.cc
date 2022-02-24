// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/optimization_guide/chrome_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/optimization_guide/core/command_line_top_host_provider.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_navigation_data.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/tab_url_provider.h"
#include "components/optimization_guide/core/top_host_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/commerce/price_tracking/android/price_tracking_notification_bridge.h"
#include "chrome/browser/optimization_guide/android/android_push_notification_manager.h"
#include "chrome/browser/optimization_guide/android/optimization_guide_tab_url_provider_android.h"
#else
#include "chrome/browser/optimization_guide/optimization_guide_tab_url_provider.h"
#endif

namespace {

const char kOldOptimizationGuideHintStore[] = "previews_hint_cache_store";

// Deletes old store paths that were written in incorrect locations.
void DeleteOldStorePaths(const base::FilePath& profile_path) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          base::GetDeletePathRecursivelyCallback(),
          profile_path.AddExtensionASCII(kOldOptimizationGuideHintStore)));
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          base::GetDeletePathRecursivelyCallback(),
          profile_path.AddExtension(
              optimization_guide::
                  kOptimizationGuidePredictionModelAndFeaturesStore)));
}

// Returns the profile to use for when setting up the keyed service when the
// profile is Off-The-Record. For guest profiles, returns a loaded profile if
// one exists, otherwise just the original profile of the OTR profile. Note:
// guest profiles are off-the-record and "original" profiles.
Profile* GetProfileForOTROptimizationGuide(Profile* profile) {
  DCHECK(profile);
  DCHECK(profile->IsOffTheRecord());

  if (profile->IsGuestSession()) {
    // Guest sessions need to rely on the stores from real profiles
    // as guest profiles cannot fetch or store new models. Note: only
    // loaded profiles should be used as we do not want to force load
    // another profile as that can lead to start up regressions.
    std::vector<Profile*> profiles =
        g_browser_process->profile_manager()->GetLoadedProfiles();
    if (!profiles.empty()) {
      return profiles[0];
    }
  }
  return profile->GetOriginalProfile();
}

// Logs info about the common optimization guide feature flags.
void LogFeatureFlagsInfo(OptimizationGuideLogger* optimization_guide_logger,
                         Profile* profile) {
  if (!optimization_guide::switches::IsDebugLogsEnabled())
    return;
  if (!optimization_guide::features::IsOptimizationHintsEnabled()) {
    OPTIMIZATION_GUIDE_LOG(optimization_guide_logger,
                           "FEATURE_FLAG Hints component disabled");
  }
  if (!optimization_guide::features::IsRemoteFetchingEnabled()) {
    OPTIMIZATION_GUIDE_LOG(optimization_guide_logger,
                           "FEATURE_FLAG remote fetching feature disabled");
  }
  if (!optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          profile->IsOffTheRecord(), profile->GetPrefs())) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_logger,
        "FEATURE_FLAG remote fetching user permission disabled");
  }
  if (!optimization_guide::features::IsPushNotificationsEnabled()) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_logger,
        "FEATURE_FLAG remote push notification feature disabled");
  }
  if (!optimization_guide::features::IsModelDownloadingEnabled()) {
    OPTIMIZATION_GUIDE_LOG(optimization_guide_logger,
                           "FEATURE_FLAG model downloading feature disabled");
  }
}

}  // namespace

// static
std::unique_ptr<optimization_guide::PushNotificationManager>
OptimizationGuideKeyedService::MaybeCreatePushNotificationManager(
    Profile* profile) {
#if BUILDFLAG(IS_ANDROID)
  if (optimization_guide::features::IsPushNotificationsEnabled()) {
    auto push_notification_manager = std::make_unique<
        optimization_guide::android::AndroidPushNotificationManager>(
        profile->GetPrefs());
    push_notification_manager->AddObserver(
        PriceTrackingNotificationBridge::GetForBrowserContext(profile));
    return push_notification_manager;
  }
#endif
  return nullptr;
}

OptimizationGuideKeyedService::OptimizationGuideKeyedService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Initialize();
}

OptimizationGuideKeyedService::~OptimizationGuideKeyedService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void OptimizationGuideKeyedService::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context_);

  // Regardless of whether the profile is off the record or not, we initialize
  // the Optimization Guide with the database associated with the original
  // profile.
  auto* proto_db_provider = profile->GetOriginalProfile()
                                ->GetDefaultStoragePartition()
                                ->GetProtoDatabaseProvider();
  base::FilePath profile_path = profile->GetOriginalProfile()->GetPath();

  // We have different behavior if |this| is created for an incognito profile.
  // For incognito profiles, we act in "read-only" mode of the original
  // profile's store and do not fetch any new hints or models.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store;
  base::WeakPtr<optimization_guide::OptimizationGuideStore>
      prediction_model_and_features_store;
  if (profile->IsOffTheRecord()) {
    OptimizationGuideKeyedService* original_ogks =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            GetProfileForOTROptimizationGuide(profile));
    DCHECK(original_ogks);
    hint_store = original_ogks->GetHintsManager()->hint_store();
    prediction_model_and_features_store =
        original_ogks->GetPredictionManager()->model_and_features_store();
  } else {
    url_loader_factory = profile->GetDefaultStoragePartition()
                             ->GetURLLoaderFactoryForBrowserProcess();

    // Only create a top host provider from the command line if provided.
    top_host_provider_ =
        optimization_guide::CommandLineTopHostProvider::CreateIfEnabled();

    bool optimization_guide_fetching_enabled =
        optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
            profile->IsOffTheRecord(), profile->GetPrefs());
    UMA_HISTOGRAM_BOOLEAN("OptimizationGuide.RemoteFetchingEnabled",
                          optimization_guide_fetching_enabled);
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "SyntheticOptimizationGuideRemoteFetching",
        optimization_guide_fetching_enabled ? "Enabled" : "Disabled");

#if BUILDFLAG(IS_ANDROID)
    tab_url_provider_ = std::make_unique<
        optimization_guide::android::OptimizationGuideTabUrlProviderAndroid>(
        profile);
#else
    tab_url_provider_ =
        std::make_unique<OptimizationGuideTabUrlProvider>(profile);
#endif

    hint_store_ =
        optimization_guide::features::ShouldPersistHintsToDisk()
            ? std::make_unique<optimization_guide::OptimizationGuideStore>(
                  proto_db_provider,
                  profile_path.Append(
                      optimization_guide::kOptimizationGuideHintStore),
                  base::ThreadPool::CreateSequencedTaskRunner(
                      {base::MayBlock(), base::TaskPriority::BEST_EFFORT}))
            : nullptr;
    hint_store = hint_store_ ? hint_store_->AsWeakPtr() : nullptr;

    prediction_model_and_features_store_ =
        std::make_unique<optimization_guide::OptimizationGuideStore>(
            proto_db_provider,
            profile_path.Append(
                optimization_guide::
                    kOptimizationGuidePredictionModelAndFeaturesStore),
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
    prediction_model_and_features_store =
        prediction_model_and_features_store_->AsWeakPtr();
  }

  optimization_guide_logger_ = std::make_unique<OptimizationGuideLogger>();
  hints_manager_ = std::make_unique<optimization_guide::ChromeHintsManager>(
      profile, profile->GetPrefs(), hint_store, top_host_provider_.get(),
      tab_url_provider_.get(), url_loader_factory,
      MaybeCreatePushNotificationManager(profile),
      optimization_guide_logger_.get());
  prediction_manager_ = std::make_unique<optimization_guide::PredictionManager>(
      prediction_model_and_features_store, url_loader_factory,
      profile->GetPrefs(), profile, optimization_guide_logger_.get());

  // The previous store paths were written in incorrect locations. Delete the
  // old paths. Remove this code in 04/2022 since it should be assumed that all
  // clients that had the previous path have had their previous stores deleted.
  DeleteOldStorePaths(profile_path);

  OPTIMIZATION_GUIDE_LOG(optimization_guide_logger_,
                         "OptimizationGuide: KeyedService is initalized");

  LogFeatureFlagsInfo(optimization_guide_logger_.get(), profile);
}

optimization_guide::ChromeHintsManager*
OptimizationGuideKeyedService::GetHintsManager() {
  return hints_manager_.get();
}

void OptimizationGuideKeyedService::OnNavigationStartOrRedirect(
    OptimizationGuideNavigationData* navigation_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types =
          hints_manager_->registered_optimization_types();
  if (!registered_optimization_types.empty()) {
    hints_manager_->OnNavigationStartOrRedirect(navigation_data,
                                                base::DoNothing());
  }

  if (navigation_data) {
    navigation_data->set_registered_optimization_types(
        hints_manager_->registered_optimization_types());
    navigation_data->set_registered_optimization_targets(
        prediction_manager_->GetRegisteredOptimizationTargets());
  }
}

void OptimizationGuideKeyedService::OnNavigationFinish(
    const std::vector<GURL>& navigation_redirect_chain) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  hints_manager_->OnNavigationFinish(navigation_redirect_chain);
}

void OptimizationGuideKeyedService::AddObserverForOptimizationTargetModel(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const absl::optional<optimization_guide::proto::Any>& model_metadata,
    optimization_guide::OptimizationTargetModelObserver* observer) {
  prediction_manager_->AddObserverForOptimizationTargetModel(
      optimization_target, model_metadata, observer);
}

void OptimizationGuideKeyedService::RemoveObserverForOptimizationTargetModel(
    optimization_guide::proto::OptimizationTarget optimization_target,
    optimization_guide::OptimizationTargetModelObserver* observer) {
  prediction_manager_->RemoveObserverForOptimizationTargetModel(
      optimization_target, observer);
}

void OptimizationGuideKeyedService::RegisterOptimizationTypes(
    const std::vector<optimization_guide::proto::OptimizationType>&
        optimization_types) {
  hints_manager_->RegisterOptimizationTypes(optimization_types);
}

optimization_guide::OptimizationGuideDecision
OptimizationGuideKeyedService::CanApplyOptimization(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationMetadata* optimization_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager_->CanApplyOptimization(url, optimization_type,
                                           optimization_metadata);
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ApplyDecision." +
          optimization_guide::GetStringNameForOptimizationType(
              optimization_type),
      optimization_type_decision);
  return optimization_guide::ChromeHintsManager::
      GetOptimizationGuideDecisionFromOptimizationTypeDecision(
          optimization_type_decision);
}

void OptimizationGuideKeyedService::CanApplyOptimizationAsync(
    content::NavigationHandle* navigation_handle,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationGuideDecisionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(navigation_handle->IsInMainFrame());

  hints_manager_->CanApplyOptimizationAsync(
      navigation_handle->GetURL(), optimization_type, std::move(callback));
}

void OptimizationGuideKeyedService::CanApplyOptimizationOnDemand(
    const std::vector<GURL>& urls,
    const base::flat_set<optimization_guide::proto::OptimizationType>&
        optimization_types,
    optimization_guide::proto::RequestContext request_context,
    optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(request_context !=
         optimization_guide::proto::RequestContext::CONTEXT_UNSPECIFIED);

  hints_manager_->CanApplyOptimizationOnDemand(urls, optimization_types,
                                               request_context, callback);
}

void OptimizationGuideKeyedService::AddHintForTesting(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    const absl::optional<optimization_guide::OptimizationMetadata>& metadata) {
  hints_manager_->AddHintForTesting(url, optimization_type, metadata);
}

void OptimizationGuideKeyedService::ClearData() {
  hints_manager_->ClearFetchedHints();
}

void OptimizationGuideKeyedService::Shutdown() {
  hints_manager_->Shutdown();
}

void OptimizationGuideKeyedService::OverrideTargetModelForTesting(
    optimization_guide::proto::OptimizationTarget optimization_target,
    std::unique_ptr<optimization_guide::ModelInfo> model_info) {
  prediction_manager_->OverrideTargetModelForTesting(  // IN-TEST
      optimization_target, std::move(model_info));
}
