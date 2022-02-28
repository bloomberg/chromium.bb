// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_SERVICE_H_
#define COMPONENTS_UKM_UKM_SERVICE_H_

#include <stddef.h>
#include <memory>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_rotation_scheduler.h"
#include "components/metrics/ukm_demographic_metrics_provider.h"
#include "components/ukm/ukm_entry_filter.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "components/ukm/ukm_reporting_service.h"

class PrefRegistrySimple;
class PrefService;
FORWARD_DECLARE_TEST(ChromeMetricsServiceClientTest, TestRegisterUKMProviders);
FORWARD_DECLARE_TEST(IOSChromeMetricsServiceClientTest,
                     TestRegisterUkmProvidersWhenUKMFeatureEnabled);

namespace metrics {
class MetricsServiceClient;
class UkmBrowserTestBase;
}

namespace ukm {
class Report;
class UkmTestHelper;

namespace debug {
class UkmDebugDataExtractor;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This maps to the enum UkmResetReason.
enum class ResetReason {
  kOnUkmAllowedStateChanged = 0,
  kUpdatePermissions = 1,
  kClonedInstall = 2,
  kMaxValue = kClonedInstall,
};

// The URL-Keyed Metrics (UKM) service is responsible for gathering and
// uploading reports that contain fine grained performance metrics including
// URLs for top-level navigations.
class UkmService : public UkmRecorderImpl {
 public:
  // Constructs a UkmService.
  // Calling code is responsible for ensuring that the lifetime of
  // |pref_service| is longer than the lifetime of UkmService. The parameters
  // |pref_service|, |client| must not be null. |demographics_provider| may be
  // null.
  UkmService(PrefService* pref_service,
             metrics::MetricsServiceClient* client,
             std::unique_ptr<metrics::UkmDemographicMetricsProvider>
                 demographics_provider);

  UkmService(const UkmService&) = delete;
  UkmService& operator=(const UkmService&) = delete;

  ~UkmService() override;

  // Initializes the UKM service.
  void Initialize();

  // Enables/disables transmission of accumulated logs. Logs that have already
  // been created will remain persisted to disk.
  void EnableReporting();
  void DisableReporting();

#if defined(OS_ANDROID) || defined(OS_IOS)
  void OnAppEnterBackground();
  void OnAppEnterForeground();
#endif

  // Records all collected data into logs, and writes to disk.
  void Flush();

  // Deletes all unsent local data (Sources, Events, aggregate info for
  // collected event metrics, etc.).
  void Purge();

  // Deletes all unsent local data related to Chrome extensions.
  void PurgeExtensionsData();

  // Deletes all unsent local data related to Apps.
  void PurgeAppsData();

  // Resets the client prefs (client_id/session_id). |reason| should be passed
  // to provide the reason of the reset - this is only used for UMA logging.
  void ResetClientState(ResetReason reason);

  // Registers the specified |provider| to provide additional metrics into the
  // UKM log. Should be called during MetricsService initialization only.
  virtual void RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider> provider);

  // Registers the |filter| that is guaranteed to be applied to all subsequent
  // events that are recorded via this UkmService.
  void RegisterEventFilter(std::unique_ptr<UkmEntryFilter> filter);

  // Registers the names of all of the preferences used by UkmService in
  // the provided PrefRegistry.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  int32_t report_count() const { return report_count_; }

  // Enables adding the synced user's noised birth year and gender to the UKM
  // report. For more details, see doc of metrics::DemographicMetricsProvider in
  // components/metrics/demographics/demographic_metrics_provider.h.
  static const base::Feature kReportUserNoisedUserBirthYearAndGender;

  // Makes sure that the serialized UKM report can be parsed.
  static bool LogCanBeParsed(const std::string& serialized_data);

  // Serializes the input UKM report into a string and validates it.
  static std::string SerializeReportProtoToString(Report* report);

 private:
  friend ::metrics::UkmBrowserTestBase;
  friend ::ukm::UkmTestHelper;
  friend ::ukm::debug::UkmDebugDataExtractor;
  friend ::ukm::UkmUtilsForTest;
  FRIEND_TEST_ALL_PREFIXES(::ChromeMetricsServiceClientTest,
                           TestRegisterUKMProviders);
  FRIEND_TEST_ALL_PREFIXES(::IOSChromeMetricsServiceClientTest,
                           TestRegisterUkmProvidersWhenUKMFeatureEnabled);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest,
                           PurgeExtensionDataFromUnsentLogStore);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, PurgeAppDataFromUnsentLogStore);

  // Starts metrics client initialization.
  void StartInitTask();

  // Called when initialization tasks are complete, to notify the scheduler
  // that it can begin calling RotateLog.
  void FinishedInitTask();

  // Periodically called by scheduler_ to advance processing of logs.
  void RotateLog();

  // Constructs a new Report from available data and stores it in
  // unsent_log_store_.
  void BuildAndStoreLog();

  // Starts an upload of the next log from unsent_log_store_.
  void StartScheduledUpload();

  // Called by log_uploader_ when the an upload is completed.
  void OnLogUploadComplete(int response_code);

  // Adds the user's birth year and gender to the UKM |report| only if (1) the
  // provider is registered and (2) the feature is enabled. For more details,
  // see doc of metrics::DemographicMetricsProvider in
  // components/metrics/demographics/demographic_metrics_provider.h.
  void AddSyncedUserNoiseBirthYearAndGenderToReport(Report* report);

  void SetInitializationCompleteCallbackForTesting(base::OnceClosure callback);

  // A weak pointer to the PrefService used to read and write preferences.
  raw_ptr<PrefService> pref_service_;

  // The UKM client id stored in prefs.
  uint64_t client_id_ = 0;

  // The UKM session id stored in prefs.
  int32_t session_id_ = 0;

  // The number of reports generated this session.
  int32_t report_count_ = 0;

  // Used to interact with the embedder. Weak pointer; must outlive |this|
  // instance.
  const raw_ptr<metrics::MetricsServiceClient> client_;

  // Registered metrics providers.
  metrics::DelegatingProvider metrics_providers_;

  // Provider of the synced user's noised birth and gender.
  std::unique_ptr<metrics::UkmDemographicMetricsProvider>
      demographics_provider_;

  // Log reporting service.
  ukm::UkmReportingService reporting_service_;

  // The scheduler for determining when uploads should happen.
  std::unique_ptr<metrics::MetricsRotationScheduler> scheduler_;

  base::TimeTicks log_creation_time_;

  bool initialize_started_ = false;
  bool initialize_complete_ = false;

  // A callback invoked when initialization of the service is complete.
  base::OnceClosure initialization_complete_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointers factory used to post task on different threads. All weak
  // pointers managed by this factory have the same lifetime as UkmService.
  base::WeakPtrFactory<UkmService> self_ptr_factory_{this};
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_SERVICE_H_
