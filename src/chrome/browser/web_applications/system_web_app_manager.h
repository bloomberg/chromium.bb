// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class Version;
}

namespace content {
class NavigationHandle;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;
class Profile;

namespace web_app {

class WebAppUiManager;
class FileHandlerManager;
class AppRegistryController;

// An enum that lists the different System Apps that exist. Can be used to
// retrieve the App ID from the underlying Web App system.
enum class SystemAppType {
  SETTINGS,
  DISCOVER,
  CAMERA,
  TERMINAL,
  MEDIA,
  HELP,
  PRINT_MANAGEMENT,
#if !defined(OFFICIAL_BUILD)
  SAMPLE,
#endif  // !defined(OFFICIAL_BUILD)
};

using OriginTrialsMap = std::map<url::Origin, std::vector<std::string>>;

// The configuration options for a System App.
struct SystemAppInfo {
  SystemAppInfo(const std::string& name_for_logging, const GURL& install_url);
  SystemAppInfo(const SystemAppInfo& other);
  ~SystemAppInfo();

  // A developer-friendly name for reporting metrics. Should follow UMA naming
  // conventions.
  std::string name_for_logging;

  // The URL that the System App will be installed from.
  GURL install_url;

  // If specified, the apps in |uninstall_and_replace| will have their data
  // migrated to this System App.
  std::vector<AppId> uninstall_and_replace;

  // Minimum window size in DIPs. Empty if the app does not have a minimum.
  // TODO(https://github.com/w3c/manifest/issues/436): Replace with PWA manifest
  // properties for window size.
  gfx::Size minimum_window_size;

  // If set, we allow only a single window for this app.
  bool single_window = true;

  // If set, when the app is launched through the File Handling Web API, we will
  // include the file's directory in window.launchQueue as the first value.
  bool include_launch_directory = false;

  // Map from origin to enabled origin trial names for this app. For example,
  // "chrome://sample-web-app/" to ["Frobulate"]. If set, we will enable the
  // given origin trials when the corresponding origin is loaded in the app.
  OriginTrialsMap enabled_origin_trials;

  // Resource Ids for additional search terms.
  std::vector<int> additional_search_terms;

  // If set to false, this app will be hidden from the Chrome OS app launcher.
  bool show_in_launcher = true;

  // If set to false, this app will be hidden from the Chrome OS search.
  bool show_in_search = true;
};

// Installs, uninstalls, and updates System Web Apps.
class SystemWebAppManager {
 public:
  // Policy for when the SystemWebAppManager will update apps/install new apps.
  enum class UpdatePolicy {
    // Update every system start.
    kAlwaysUpdate,
    // Update when the Chrome version number changes.
    kOnVersionChange,
  };

  static constexpr char kInstallResultHistogramName[] =
      "Webapp.InstallResult.System";
  static constexpr char kInstallDurationHistogramName[] =
      "Webapp.InstallDuration.System";

  // Returns whether the given app type is enabled.
  static bool IsAppEnabled(SystemAppType type);

  explicit SystemWebAppManager(Profile* profile);
  virtual ~SystemWebAppManager();

  void SetSubsystems(PendingAppManager* pending_app_manager,
                     AppRegistrar* registrar,
                     AppRegistryController* registry_controller,
                     WebAppUiManager* ui_manager,
                     FileHandlerManager* file_handler_manager);

  void Start();

  static bool IsEnabled();

  // The SystemWebAppManager is disabled in browser tests by default because it
  // pollutes the startup state (several tests expect the Extensions state to be
  // clean).
  //
  // Call this to install apps for SystemWebApp specific tests, e.g if a test
  // needs to open OS Settings.
  //
  // This can also be called multiple times to simulate reinstallation from
  // system restart, e.g.
  void InstallSystemAppsForTesting();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the app id for the given System App |type|.
  base::Optional<AppId> GetAppIdForSystemApp(SystemAppType type) const;

  // Returns the System App Type for the given |app_id|.
  base::Optional<SystemAppType> GetSystemAppTypeForAppId(AppId app_id) const;

  // Returns the App Ids for all installed System Web Apps.
  std::vector<AppId> GetAppIds() const;

  // Returns whether |app_id| points to an installed System App.
  bool IsSystemWebApp(const AppId& app_id) const;

  // Returns whether the given System App |type| should use a single window.
  bool IsSingleWindow(SystemAppType type) const;

  // Returns whether the given System App |type| should get launch directory in
  // launch parameter.
  bool AppShouldReceiveLaunchDirectory(SystemAppType type) const;

  // Perform tab-specific setup when a navigation in a System Web App is about
  // to be committed.
  void OnReadyToCommitNavigation(const AppId& app_id,
                                 content::NavigationHandle* navigation_handle);

  // Returns terms to be used when searching for the app.
  std::vector<std::string> GetAdditionalSearchTerms(SystemAppType type) const;

  // Returns whether the app should be shown in the launcher.
  bool ShouldShowInLauncher(SystemAppType type) const;

  // Returns whether the app should be shown in search.
  bool ShouldShowInSearch(SystemAppType type) const;

  // Returns the minimum window size for |app_id| or an empty size if the app
  // doesn't specify a minimum.
  gfx::Size GetMinimumWindowSize(const AppId& app_id) const;

  const base::OneShotEvent& on_apps_synchronized() const {
    return *on_apps_synchronized_;
  }

  // This call will override default System Apps configuration. You should call
  // Start() after this call to install |system_apps|.
  void SetSystemAppsForTesting(
      base::flat_map<SystemAppType, SystemAppInfo> system_apps);

  void SetUpdatePolicyForTesting(UpdatePolicy policy);

  // Updates each system app either disabled/not disabled.
  void OnAppsPolicyChanged();

  void Shutdown();

 protected:
  virtual const base::Version& CurrentVersion() const;
  virtual const std::string& CurrentLocale() const;

 private:
  // Returns the list of origin trials to enable for |url| loaded in System App
  // |type|. Returns nullptr if the App does not specify origin trials for
  // |url|.
  const std::vector<std::string>* GetEnabledOriginTrials(SystemAppType type,
                                                         const GURL& url);

  bool AppHasFileHandlingOriginTrial(SystemAppType type);

  void OnAppsSynchronized(const base::TimeTicks& install_start_time,
                          std::map<GURL, InstallResultCode> install_results,
                          std::map<GURL, bool> uninstall_results);
  bool NeedsUpdate() const;

  void RecordSystemWebAppInstallMetrics(
      const std::map<GURL, InstallResultCode>& install_results,
      const base::TimeDelta& install_duration) const;

  Profile* profile_;

  std::unique_ptr<base::OneShotEvent> on_apps_synchronized_;

  bool shutting_down_ = false;

  std::string install_result_per_profile_histogram_name_;

  UpdatePolicy update_policy_;

  base::flat_map<SystemAppType, SystemAppInfo> system_app_infos_;

  base::flat_map<AppId, SystemAppType> app_id_to_app_type_;

  PrefService* const pref_service_;

  // Used to install, uninstall, and update apps. Should outlive this class.
  PendingAppManager* pending_app_manager_ = nullptr;

  AppRegistrar* registrar_ = nullptr;

  AppRegistryController* registry_controller_ = nullptr;

  WebAppUiManager* ui_manager_ = nullptr;

  FileHandlerManager* file_handler_manager_ = nullptr;

  PrefChangeRegistrar local_state_pref_change_registrar_;

  base::WeakPtrFactory<SystemWebAppManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_H_
