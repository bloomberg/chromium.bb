// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_H_

#include <map>
#include <set>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/apps/app_service/metrics/browser_to_tab_list.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class Profile;

namespace apps {

class AppUpdate;

// This is used for logging, so do not remove or reorder existing entries.
enum class InstallTime {
  kInit = 0,
  kRunning = 1,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kRunning,
};

extern const char kAppRunningDuration[];
extern const char kAppActivatedCount[];

extern const char kAppLaunchPerAppTypeHistogramName[];
extern const char kAppLaunchPerAppTypeV2HistogramName[];

extern const char kArcHistogramName[];
extern const char kBuiltInHistogramName[];
extern const char kCrostiniHistogramName[];
extern const char kChromeAppHistogramName[];
extern const char kWebAppHistogramName[];
extern const char kMacOsHistogramName[];
extern const char kPluginVmHistogramName[];
extern const char kStandaloneBrowserHistogramName[];
extern const char kRemoteHistogramName[];
extern const char kBorealisHistogramName[];
extern const char kSystemWebAppHistogramName[];
extern const char kChromeBrowserHistogramName[];

extern const char kChromeAppTabHistogramName[];
extern const char kChromeAppWindowHistogramName[];
extern const char kWebAppTabHistogramName[];
extern const char kWebAppWindowHistogramName[];

std::string GetAppTypeHistogramName(apps::AppTypeName app_type_name);
std::string GetAppTypeHistogramNameV2(apps::AppTypeNameV2 app_type_name);

const std::set<apps::AppTypeName>& GetAppTypeNameSet();

// Records metrics when launching apps.
void RecordAppLaunchMetrics(Profile* profile,
                            AppType app_type,
                            const std::string& app_id,
                            apps::mojom::LaunchSource launch_source,
                            apps::mojom::LaunchContainer container);

class AppPlatformMetrics : public apps::AppRegistryCache::Observer,
                           public apps::InstanceRegistry::Observer {
 public:
  explicit AppPlatformMetrics(Profile* profile,
                              apps::AppRegistryCache& app_registry_cache,
                              InstanceRegistry& instance_registry);
  AppPlatformMetrics(const AppPlatformMetrics&) = delete;
  AppPlatformMetrics& operator=(const AppPlatformMetrics&) = delete;
  ~AppPlatformMetrics() override;

  // Returns the SourceId of UKM for `app_id`.
  static ukm::SourceId GetSourceId(Profile* profile, const std::string& app_id);

  // Returns the SourceId for a Borealis app_id.
  static ukm::SourceId GetSourceIdForBorealis(Profile* profile,
                                              const std::string& app_id);

  // Gets the source id for a Crostini app_id.
  static ukm::SourceId GetSourceIdForCrostini(Profile* profile,
                                              const std::string& app_id);

  // Informs UKM service that the source_id is no longer needed and can be
  // deleted later.
  static void RemoveSourceId(ukm::SourceId source_id);

  // UMA metrics name for installed apps count in Chrome OS.
  static std::string GetAppsCountHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for installed apps count per InstallReason in Chrome OS.
  static std::string GetAppsCountPerInstallReasonHistogramNameForTest(
      AppTypeName app_type_name,
      apps::mojom::InstallReason install_reason);

  // UMA metrics name for apps running duration in Chrome OS.
  static std::string GetAppsRunningDurationHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for apps running percentage in Chrome OS.
  static std::string GetAppsRunningPercentageHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for app window activated count in Chrome OS.
  static std::string GetAppsActivatedCountHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for apps usage time in Chrome OS for AppTypeName.
  static std::string GetAppsUsageTimeHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for apps usage time in Chrome OS for AppTypeNameV2.
  static std::string GetAppsUsageTimeHistogramNameForTest(
      AppTypeNameV2 app_type_name);

  void OnNewDay();
  void OnTenMinutes();
  void OnFiveMinutes();

  // Records UKM when launching an app.
  void RecordAppLaunchUkm(AppType app_type,
                          const std::string& app_id,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::LaunchContainer container);

  // Records UKM when uninstalling an app.
  void RecordAppUninstallUkm(AppType app_type,
                             const std::string& app_id,
                             apps::mojom::UninstallSource uninstall_source);

 private:
  struct RunningStartTime {
    base::TimeTicks start_time;
    AppTypeName app_type_name;
    AppTypeNameV2 app_type_name_v2;
    std::string app_id;
  };

  struct UsageTime {
    base::TimeDelta running_time;
    ukm::SourceId source_id = ukm::kInvalidSourceId;
    AppTypeName app_type_name = AppTypeName::kUnknown;
    bool window_is_closed = false;
  };

  // AppRegistryCache::Observer:
  void OnAppTypeInitialized(AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;
  void OnAppUpdate(const apps::AppUpdate& update) override;

  // apps::InstanceRegistry::Observer:
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

  // Returns the browser id and the browser instance state for the tab's
  // `update`. If there is no browser instance for the tab's `update`, the
  // returned token of the browser id will be empty, and the state will be
  // unknown.
  void GetBrowserIdAndState(const InstanceUpdate& update,
                            base::UnguessableToken& browser_id,
                            InstanceState& state) const;

  // Updates the browser window status when the web app tab `update` is
  // inactivated.
  void UpdateBrowserWindowStatus(const InstanceUpdate& update);

  void SetWindowActivated(AppType app_type,
                          AppTypeName app_type_name,
                          AppTypeNameV2 app_type_name_v2,
                          const std::string& app_id,
                          const base::UnguessableToken& instance_id);
  void SetWindowInActivated(const std::string& app_id,
                            const base::UnguessableToken& instance_id,
                            apps::InstanceState state);

  void InitRunningDuration();
  void ClearRunningDuration();

  // Records the number of apps of the given `app_type` that the family user has
  // recently used.
  void RecordAppsCount(AppType app_type);

  // Records the app running duration.
  void RecordAppsRunningDuration();

  // Records the app usage time metrics (both UMA and UKM) in five minutes
  // intervals.
  void RecordAppsUsageTime();

  // Records the app usage time UKM in five minutes intervals.
  void RecordAppsUsageTimeUkm();

  // Records the installed app in Chrome OS.
  void RecordAppsInstallUkm(const apps::AppUpdate& update,
                            InstallTime install_time);

  Profile* const profile_ = nullptr;

  AppRegistryCache& app_registry_cache_;

  bool should_record_metrics_on_new_day_ = false;

  bool should_refresh_duration_pref = false;
  bool should_refresh_activated_count_pref = false;

  int user_type_by_device_type_;

  BrowserToTabList browser_to_tab_list_;

  // |running_start_time_| and |running_duration_| are used for accumulating app
  // running duration per each day interval.
  std::map<const base::UnguessableToken, RunningStartTime> running_start_time_;
  std::map<AppTypeName, base::TimeDelta> running_duration_;
  std::map<AppTypeName, int> activated_count_;

  // |start_time_per_five_minutes_|, |app_type_running_time_per_five_minutes_|,
  // |app_type_v2_running_time_per_five_minutes_|, and
  // |usage_time_per_five_minutes_| are used for accumulating app
  // running duration per 5 minutes interval.
  std::map<const base::UnguessableToken, RunningStartTime>
      start_time_per_five_minutes_;
  std::map<AppTypeName, base::TimeDelta>
      app_type_running_time_per_five_minutes_;
  std::map<AppTypeNameV2, base::TimeDelta>
      app_type_v2_running_time_per_five_minutes_;
  std::map<const base::UnguessableToken, UsageTime>
      usage_time_per_five_minutes_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_H_
