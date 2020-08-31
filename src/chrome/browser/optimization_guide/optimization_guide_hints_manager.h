// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_HINTS_MANAGER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_HINTS_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/mru_cache.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/hints_fetcher.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/optimization_guide_service_observer.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class NavigationHandle;
}  // namespace content

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace optimization_guide {
class HintCache;
class HintsFetcherFactory;
class OptimizationFilter;
class OptimizationMetadata;
class OptimizationGuideService;
enum class OptimizationTargetDecision;
enum class OptimizationTypeDecision;
class StoreUpdateData;
class TopHostProvider;
}  // namespace optimization_guide

class OptimizationGuideNavigationData;
class PrefService;
class Profile;

class OptimizationGuideHintsManager
    : public optimization_guide::OptimizationGuideServiceObserver,
      public network::NetworkQualityTracker::EffectiveConnectionTypeObserver,
      public NavigationPredictorKeyedService::Observer {
 public:
  OptimizationGuideHintsManager(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types_at_initialization,
      optimization_guide::OptimizationGuideService* optimization_guide_service,
      Profile* profile,
      const base::FilePath& profile_path,
      PrefService* pref_service,
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      optimization_guide::TopHostProvider* top_host_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~OptimizationGuideHintsManager() override;

  // Unhooks the observer to |optimization_guide_service_|.
  void Shutdown();

  // optimization_guide::OptimizationGuideServiceObserver implementation:
  void OnHintsComponentAvailable(
      const optimization_guide::HintsComponentInfo& info) override;

  // |next_update_closure| is called the next time OnHintsComponentAvailable()
  // is called and the corresponding hints have been updated.
  void ListenForNextUpdateForTesting(base::OnceClosure next_update_closure);

  // Registers the optimization types that have the potential for hints to be
  // called by consumers of the Optimization Guide.
  void RegisterOptimizationTypes(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types);

  // Returns the optimization types that are registered.
  base::flat_set<optimization_guide::proto::OptimizationType>
  registered_optimization_types() const {
    return registered_optimization_types_;
  }

  // Returns whether there is an optimization filter loaded for
  // |optimization_type|.
  bool HasLoadedOptimizationFilter(
      optimization_guide::proto::OptimizationType optimization_type);

  // Returns the OptimizationTargetDecision based on the given parameters.
  // TODO(crbug/1021364): Remove this method once the hints have nothing to do
  // with predicting navigations.
  optimization_guide::OptimizationTargetDecision ShouldTargetNavigation(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationTarget optimization_target);

  // Returns the OptimizationTypeDecision based on the given parameters.
  // |optimization_metadata| will be populated, if applicable.
  optimization_guide::OptimizationTypeDecision CanApplyOptimization(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata);

  // Invokes |callback| with the decision for |navigation_url| and
  // |optimization_type|, when sufficient information has been collected by
  // |this| to make the decision. Virtual for testing.
  virtual void CanApplyOptimizationAsync(
      const GURL& navigation_url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback);

  // Clears fetched hints from |hint_cache_|.
  void ClearFetchedHints();

  // Returns the current batch update hints fetcher.
  optimization_guide::HintsFetcher* batch_update_hints_fetcher() const {
    return batch_update_hints_fetcher_.get();
  }

  // Overrides |hints_fetcher_factory| for testing.
  void SetHintsFetcherFactoryForTesting(
      std::unique_ptr<optimization_guide::HintsFetcherFactory>
          hints_fetcher_factory);

  // Overrides |clock_| for testing.
  void SetClockForTesting(const base::Clock* clock);

  // network::NetworkQualityTracker::EffectiveConnectionTypeObserver
  // implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override;

  // Notifies |this| that a navigation with |navigation_handle| has started.
  // |callback| is run when the request has finished regardless of whether there
  // was actually a hint for that load or not. The callback can be used as a
  // signal for tests.
  void OnNavigationStartOrRedirect(content::NavigationHandle* navigation_handle,
                                   base::OnceClosure callback);

  // Notifies |this| that a navigation with redirect chain
  // |navigation_redirect_chain| has finished.
  void OnNavigationFinish(const std::vector<GURL>& navigation_redirect_chain);

  // Add hints to the cache with the provided metadata. For testing only.
  void AddHintForTesting(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      const base::Optional<optimization_guide::OptimizationMetadata>& metadata);

 private:
  FRIEND_TEST_ALL_PREFIXES(OptimizationGuideHintsManagerTest, IsGoogleURL);
  FRIEND_TEST_ALL_PREFIXES(OptimizationGuideHintsManagerFetchingTest,
                           HintsFetched_AtSRP_NoRegisteredOptimizationTypes);
  FRIEND_TEST_ALL_PREFIXES(OptimizationGuideHintsManagerFetchingTest,
                           HintsFetched_AtSRP_ECT_SLOW_2G);
  FRIEND_TEST_ALL_PREFIXES(OptimizationGuideHintsManagerFetchingTest,
                           HintsFetched_AtSRP_ECT_4G);
  FRIEND_TEST_ALL_PREFIXES(OptimizationGuideHintsManagerFetchingTest,
                           HintsFetched_AtNonSRP_ECT_SLOW_2G);
  FRIEND_TEST_ALL_PREFIXES(OptimizationGuideHintsManagerFetchingTest,
                           HintsFetched_AtSRP_ECT_SLOW_2G_DuplicatesRemoved);
  FRIEND_TEST_ALL_PREFIXES(
      OptimizationGuideHintsManagerFetchingTest,
      HintsFetched_AtSRP_ECT_SLOW_2G_NonHTTPOrHTTPSHostsRemoved);
  FRIEND_TEST_ALL_PREFIXES(
      OptimizationGuideHintsManagerFetchingTest,
      HintsFetched_ExternalAndroidApp_ECT_SLOW_2G_NonHTTPOrHTTPSHostsRemovedAppWhitelisted);
  FRIEND_TEST_ALL_PREFIXES(
      OptimizationGuideHintsManagerFetchingTest,
      HintsFetched_ExternalAndroidApp_ECT_SLOW_2G_NonHTTPOrHTTPSHostsRemovedNotAllAppsWhitelisted);
  FRIEND_TEST_ALL_PREFIXES(
      OptimizationGuideHintsManagerFetchingTest,
      HintsFetched_ExternalAndroidApp_ECT_SLOW_2G_NonHTTPOrHTTPSHostsRemovedAppNotWhitelisted);

  // Processes the hints component.
  //
  // Should always be called on the thread that belongs to
  // |background_task_runner_|.
  std::unique_ptr<optimization_guide::StoreUpdateData> ProcessHintsComponent(
      const optimization_guide::HintsComponentInfo& info,
      const base::flat_set<optimization_guide::proto::OptimizationType>&
          registered_optimization_types,
      std::unique_ptr<optimization_guide::StoreUpdateData> update_data);

  // Processes the optimization filters contained in the hints component.
  //
  // Should always be called on the thread that belongs to
  // |background_task_runner_|.
  void ProcessOptimizationFilters(
      const google::protobuf::RepeatedPtrField<
          optimization_guide::proto::OptimizationFilter>&
          blacklist_optimization_filters,
      const base::flat_set<optimization_guide::proto::OptimizationType>&
          registered_optimization_types);

  // Callback run after the hint cache is fully initialized. At this point, the
  // OptimizationGuideHintsManager is ready to process hints.
  void OnHintCacheInitialized();

  // Updates the cache with the latest hints sent by the Component Updater.
  void UpdateComponentHints(
      base::OnceClosure update_closure,
      std::unique_ptr<optimization_guide::StoreUpdateData> update_data);

  // Called when the hints have been fully updated with the latest hints from
  // the Component Updater. This is used as a signal during tests.
  void OnComponentHintsUpdated(base::OnceClosure update_closure,
                               bool hints_updated);

  // Method to decide whether to fetch new hints for user's top sites and
  // proceeds to schedule the fetch.
  void MaybeScheduleTopHostsHintsFetch();

  // Schedules |hints_fetch_timer_| to fire based on:
  // 1. The update time for the fetched hints in the store and
  // 2. The last time a fetch attempt was made.
  void ScheduleTopHostsHintsFetch();

  // Called to make a request to fetch hints from the remote Optimization Guide
  // Service. Used to fetch hints for origins frequently visited by the user.
  void FetchTopHostsHints();

  // Called when the hints for the top hosts have been fetched from the remote
  // Optimization Guide Service and are ready for parsing. This is used when
  // fetching hints in batch mode.
  void OnTopHostsHintsFetched(
      base::Optional<
          std::unique_ptr<optimization_guide::proto::GetHintsResponse>>
          get_hints_response);

  // Called when the hints for a navigation have been fetched from the remote
  // Optimization Guide Service and are ready for parsing. This is used when
  // fetching hints in real-time. |navigation_url| is the URL associated with
  // the navigation handle that initiated the fetch.
  // |page_navigation_urls_requested| contains the URLs that were requested  by
  // |this| to be fetched. |page_navigation_hosts_requested| contains the hosts
  // that were requested by |this| to be fetched.
  void OnPageNavigationHintsFetched(
      base::WeakPtr<OptimizationGuideNavigationData> navigation_data_weak_ptr,
      const base::Optional<GURL>& navigation_url,
      const base::flat_set<GURL>& page_navigation_urls_requested,
      const base::flat_set<std::string>& page_navigation_hosts_requested,
      base::Optional<
          std::unique_ptr<optimization_guide::proto::GetHintsResponse>>
          get_hints_response);

  // Called when the fetched hints have been stored in |hint_cache| and are
  // ready to be used. This is used when hints were fetched in batch mode.
  void OnFetchedTopHostsHintsStored();

  // Called when the fetched hints have been stored in |hint_cache| and are
  // ready to be used. This is used when hints were fetched in real-time.
  // |navigation_url| is the URL associated with the navigation handle that
  // initiated the fetch. |page_navigation_hosts_requested| contains the hosts
  // whose hints should be loaded into memory when invoked.
  void OnFetchedPageNavigationHintsStored(
      base::WeakPtr<OptimizationGuideNavigationData> navigation_data_weak_ptr,
      const base::Optional<GURL>& navigation_url,
      const base::flat_set<std::string>& page_navigation_hosts_requested);

  // Returns true if there is a fetch currently in-flight for |navigation_url|.
  bool IsHintBeingFetchedForNavigation(const GURL& navigation_url);

  // Cleans up the hints fetcher for |navigation_url|, if applicable.
  void CleanUpFetcherForNavigation(const GURL& navigation_url);

  // Returns the time when a hints fetch request was last attempted.
  base::Time GetLastHintsFetchAttemptTime() const;

  // Sets the time when a hints fetch was last attempted to |last_attempt_time|.
  void SetLastHintsFetchAttemptTime(base::Time last_attempt_time);

  // Called when the request to load a hint has completed.
  void OnHintLoaded(base::OnceClosure callback,
                    const optimization_guide::proto::Hint* loaded_hint) const;

  // Returns true if |this| is allowed to fetch hints at the navigation time for
  // |url|.
  bool IsAllowedToFetchNavigationHints(const GURL& url) const;

  // Loads the hint if available.
  // |callback| is run when the request has finished regardless of whether there
  // was actually a hint for that load or not. The callback can be used as a
  // signal for tests.
  void LoadHintForNavigation(content::NavigationHandle* navigation_handle,
                             base::OnceClosure callback);

  // Loads the hint for |host| if available.
  // |callback| is run when the request has finished regardless of whether there
  // was actually a hint for that |host| or not. The callback can be used as a
  // signal for tests.
  void LoadHintForHost(const std::string& host, base::OnceClosure callback);

  // Returns true if the hostname for |url| matches the host of google web
  // search results page (www.google.*).
  bool IsGoogleURL(const GURL& url) const;

  // Returns true if we can make a request for hints for |prediction|.
  bool IsAllowedToFetchForNavigationPrediction(
      const base::Optional<NavigationPredictorKeyedService::Prediction>
          prediction) const;

  // NavigationPredictorKeyedService::Observer:
  void OnPredictionUpdated(
      const base::Optional<NavigationPredictorKeyedService::Prediction>
          prediction) override;

  // Creates a hints fetch for |navigation_handle| if it is allowed. The
  // fetch will include the host and URL of the |navigation_handle| if the
  // associated hints for each are not already in the cache.
  void MaybeFetchHintsForNavigation(
      content::NavigationHandle* navigation_handle);

  // Returns the OptimizationTypeDecision based on the given parameters.
  // |optimization_metadata| will be populated, if applicable. If
  // |navigation_data| is provided, some metrics will be populated within it.
  optimization_guide::OptimizationTypeDecision CanApplyOptimization(
      OptimizationGuideNavigationData* navigation_data,
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata);

  // If an entry for |navigation_url| is contained in |registered_callbacks_|,
  // it will load the hint for |navigation_url|'s host and upon completion, will
  // invoke the registered callbacks for |navigation_url|.
  void PrepareToInvokeRegisteredCallbacks(const GURL& navigation_url);

  // Invokes the registered callbacks for |navigation_url|, if applicable.
  void OnReadyToInvokeRegisteredCallbacks(const GURL& navigation_url);

  // Whether all information was available to make a decision for
  // |navigation_url| and |optimization type}.
  bool HasAllInformationForDecisionAvailable(
      const GURL& navigation_url,
      optimization_guide::proto::OptimizationType optimization_type);

  // The OptimizationGuideService that this guide is listening to. Not owned.
  optimization_guide::OptimizationGuideService* const
      optimization_guide_service_;

  // The information of the latest component delivered by
  // |optimization_guide_service_|.
  base::Optional<optimization_guide::HintsComponentInfo> hints_component_info_;

  // The set of optimization types that have been registered with the hints
  // manager.
  //
  // Should only be read and modified on the UI thread.
  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types_;

  // Synchronizes access to member variables related to optimization filters.
  base::Lock optimization_filters_lock_;

  // The set of optimization types that the component specified by
  // |component_info_| has optimization filters for.
  base::flat_set<optimization_guide::proto::OptimizationType>
      optimization_types_with_filter_ GUARDED_BY(optimization_filters_lock_);

  // A map from optimization type to the host filter that holds the blacklist
  // for that type.
  base::flat_map<optimization_guide::proto::OptimizationType,
                 std::unique_ptr<optimization_guide::OptimizationFilter>>
      blacklist_optimization_filters_ GUARDED_BY(optimization_filters_lock_);

  // A map from URL to a map of callbacks keyed by their optimization type.
  base::flat_map<
      GURL,
      base::flat_map<
          optimization_guide::proto::OptimizationType,
          std::vector<optimization_guide::OptimizationGuideDecisionCallback>>>
      registered_callbacks_;

  // Background thread where hints processing should be performed.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // A reference to the profile. Not owned.
  Profile* profile_ = nullptr;

  // A reference to the PrefService for this profile. Not owned.
  PrefService* pref_service_ = nullptr;

  // The hint cache that holds both hints received from the component and
  // fetched from the remote Optimization Guide Service.
  std::unique_ptr<optimization_guide::HintCache> hint_cache_;

  // The fetcher that handles making requests for hints for multiple hosts from
  // the remote Optimization Guide Service.
  std::unique_ptr<optimization_guide::HintsFetcher> batch_update_hints_fetcher_;

  // A cache keyed by navigation URL to the fetcher making a request for a hint
  // for that URL and/or host to the remote Optimization Guide Service that
  // keeps track of when an entry has been placed in the cache.
  base::MRUCache<GURL, std::unique_ptr<optimization_guide::HintsFetcher>>
      page_navigation_hints_fetchers_;

  // The factory used to create hints fetchers. It is mostly used to create
  // new fetchers for use under the page navigation context, but will also be
  // used to create the initial fetcher for the batch update context.
  std::unique_ptr<optimization_guide::HintsFetcherFactory>
      hints_fetcher_factory_;

  // The external app packages that have been approved for fetching from the
  // remote Optimization Guide Service.
  base::flat_set<std::string> external_app_packages_approved_for_fetch_;

  // The top host provider that can be queried. Not owned.
  optimization_guide::TopHostProvider* top_host_provider_ = nullptr;

  // The timer used to schedule fetching hints from the remote Optimization
  // Guide Service.
  base::OneShotTimer top_hosts_hints_fetch_timer_;

  // The clock used to schedule fetching from the remote Optimization Guide
  // Service.
  const base::Clock* clock_;

  // The current estimate of the EffectiveConnectionType.
  net::EffectiveConnectionType current_effective_connection_type_ =
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  // Used in testing to subscribe to an update event in this class.
  base::OnceClosure next_update_closure_;

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<OptimizationGuideHintsManager> ui_weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideHintsManager);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_HINTS_MANAGER_H_
