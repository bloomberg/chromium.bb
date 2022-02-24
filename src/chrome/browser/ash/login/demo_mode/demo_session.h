// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_SESSION_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_SESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/demo_mode/demo_extensions_external_loader.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/app_window/app_window_registry.h"

class PrefRegistrySimple;

namespace base {
class OneShotTimer;
}

namespace ash {
class DemoResources;

// Tracks global demo session state, such as whether the demo session has
// started and the state of demo mode resources.
class DemoSession : public session_manager::SessionManagerObserver,
                    public user_manager::UserManager::UserSessionStateObserver,
                    public extensions::AppWindowRegistry::Observer,
                    public apps::AppRegistryCache::Observer {
 public:
  // Type of demo mode configuration.
  // Warning: DemoModeConfig is stored in local state. Existing entries should
  // not be reordered and new values should be added at the end.
  enum class DemoModeConfig : int {
    // No demo mode configuration or configuration unknown.
    kNone = 0,
    // Online enrollment into demo mode was established with DMServer.
    // Policies are applied from the cloud.
    kOnline = 1,
    // Offline enrollment into demo mode was established locally.
    // Offline policy set is applied to the device.
    kOffline = 2,
    // Add new entries above this line and make sure to update kLast value.
    kLast = kOffline,
  };

  // Indicates the source of an app launch when in Demo mode for UMA
  // stat reporting purposes.  Because they are used for a UMA stat,
  // these values should not be changed or moved.
  enum class AppLaunchSource {
    // Logged when apps are launched from the Shelf in Demo Mode.
    kShelf = 0,
    // Logged when apps are launched from the App List in Demo Mode.
    kAppList = 1,
    // Logged by any Extension APIs used by the Highlights App to launch apps in
    // Demo Mode.
    kExtensionApi = 2,
    // Add future entries above this comment, in sync with enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kExtensionApi
  };

  // The list of countries that Demo Mode supports, ie the countries we have
  // created OUs and admin users for in the admin console.
  // Sorted by the English name of the country (not the country code), except US
  // is first.
  // TODO(crbug.com/983359): Sort these by country name in the current locale
  // instead of using this hard-coded US-centric order.
  static constexpr char kSupportedCountries[][3] = {
      "US", "BE", "CA", "DK", "FI", "FR", "DE", "IE",
      "IT", "JP", "LU", "NL", "NO", "ES", "SE", "GB"};

  static constexpr char kCountryNotSelectedId[] = "N/A";

  DemoSession(const DemoSession&) = delete;
  DemoSession& operator=(const DemoSession&) = delete;

  static std::string DemoConfigToString(DemoModeConfig config);

  // Whether the device is set up to run demo sessions.
  static bool IsDeviceInDemoMode();

  // Whether the device is set up to enroll Demo Mode offline.
  // The device needs to be set up for Demo Mode in order to return true.
  // TODO(b/154290639): Move into anonymous namespace when fixed.
  static bool IsDemoModeOfflineEnrolled();

  // Returns current demo mode configuration.
  static DemoModeConfig GetDemoConfig();

  // Sets demo mode configuration for tests. Should be cleared by calling
  // ResetDemoConfigForTesting().
  static void SetDemoConfigForTesting(DemoModeConfig demo_config);

  // Resets demo mode configuration that was used for tests.
  static void ResetDemoConfigForTesting();

  // If the device is set up to run in demo mode, marks demo session as started,
  // and requests load of demo session resources.
  // Creates global DemoSession instance if required.
  static DemoSession* StartIfInDemoMode();

  // Deletes the global DemoSession instance if it was previously created.
  static void ShutDownIfInitialized();

  // Gets the global demo session instance. Returns nullptr if the DemoSession
  // instance has not yet been initialized (either by calling
  // StartIfInDemoMode() or PreloadOfflineResourcesIfInDemoMode()).
  static DemoSession* Get();

  // Returns an additional comma-separated language list for demo mode.
  static std::string GetAdditionalLanguageList();

  // Returns the id of the screensaver app based on the board name.
  static std::string GetScreensaverAppId();

  // Returns whether the chrome extension app with `app_id` should be displayed
  // in app launcher in demo mode. Returns true for all apps in non-demo mode.
  static bool ShouldShowExtensionInAppLauncher(const std::string& app_id);

  // Returns whether the Web app with `app_id` should be shown in demo mode,
  // in any of launcher, search and shelf.
  // Returns true for the app in non-demo mode.
  static bool ShouldShowWebApp(const std::string& app_id);

  // Returns the list of countries that Demo Mode supports. Each country is
  // denoted by:
  // `value`: The ISO country code.
  // `title`: The display name of the country in the current locale.
  // `selected`: Whether the country is currently selected.
  static base::Value GetCountryList();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Records the launch of an app in Demo mode from the specified source.
  static void RecordAppLaunchSourceIfInDemoMode(AppLaunchSource source);

  // Ensures that the load of demo session resources is requested.
  // `load_callback` will be run once the resource load finishes.
  void EnsureResourcesLoaded(base::OnceClosure load_callback);

  // Returns false if the Chrome app or ARC++ package, which is normally pinned
  // by policy, should actually not be force-pinned because the device is
  // in Demo Mode and offline.
  bool ShouldShowAndroidOrChromeAppInShelf(
      const std::string& app_id_or_package);

  // Sets `extensions_external_loader_` and starts installing the screensaver.
  void SetExtensionsExternalLoader(
      scoped_refptr<DemoExtensionsExternalLoader> extensions_external_loader);

  // Sets app IDs and package names that shouldn't be pinned by policy when the
  // device is offline in Demo Mode.
  void OverrideIgnorePinPolicyAppsForTesting(std::vector<std::string> apps);

  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer);
  base::OneShotTimer* GetTimerForTesting();

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // extensions::AppWindowRegistry::Observer:
  void OnAppWindowActivated(extensions::AppWindow* app_window) override;

  bool offline_enrolled() const { return offline_enrolled_; }

  bool started() const { return started_; }

  const DemoResources* resources() const { return demo_resources_.get(); }

 private:
  DemoSession();
  ~DemoSession() override;

  // Installs resources for Demo Mode from the offline demo mode resources, such
  // as apps and media.
  void InstallDemoResources();

  // Loads the highlights app from offline resources and launches it upon
  // success.
  void LoadAndLaunchHighlightsApp();

  // Installs the CRX file from an update URL. Observes `AppRegistryCache` to
  // launch the app upon installation.
  void InstallAppFromUpdateUrl(const std::string& id);

  // Shows the splash screen after demo mode resources are installed.
  void ShowSplashScreen();

  // Removes the splash screen.
  void RemoveSplashScreen();

  // Returns whether splash screen should be removed. The splash screen should
  // be removed when both active session starts (i.e. login screen is destroyed)
  // and screensaver is shown, to ensure a smooth transition.
  bool ShouldRemoveSplashScreen();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // Whether the device was offline-enrolled into demo mode, i.e. enrolled using
  // pre-built policies. Offline enrolled demo sessions do not have working
  // robot account associated with them.
  bool offline_enrolled_ = false;

  // Whether demo session has been started.
  bool started_ = false;

  // Apps that ShouldShowAndroidOrChromeAppInShelf() will check for if the
  // device is offline.
  std::vector<std::string> ignore_pin_policy_offline_apps_;

  std::unique_ptr<DemoResources> demo_resources_;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};

  base::ScopedMultiSourceObservation<extensions::AppWindowRegistry,
                                     extensions::AppWindowRegistry::Observer>
      app_window_registry_observations_{this};

  base::ScopedMultiSourceObservation<apps::AppRegistryCache,
                                     apps::AppRegistryCache::Observer>
      app_registry_cache_observation_{this};

  scoped_refptr<DemoExtensionsExternalLoader> extensions_external_loader_;

  // The fallback timer that ensures the splash screen is removed in case the
  // screensaver app takes an extra long time to be shown.
  std::unique_ptr<base::OneShotTimer> remove_splash_screen_fallback_timer_;

  bool splash_screen_removed_ = false;
  bool screensaver_activated_ = false;

  base::WeakPtrFactory<DemoSession> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::DemoSession;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_SESSION_H_
