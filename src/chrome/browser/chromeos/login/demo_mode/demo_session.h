// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SESSION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SESSION_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_extensions_external_loader.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_registry_observer.h"

class PrefRegistrySimple;

namespace session_manager {
class SessionManager;
}

namespace chromeos {

class DemoResources;

// Tracks global demo session state, such as whether the demo session has
// started and the state of demo mode resources.
class DemoSession : public session_manager::SessionManagerObserver,
                    public extensions::ExtensionRegistryObserver,
                    public user_manager::UserManager::UserSessionStateObserver {
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

  static std::string DemoConfigToString(DemoModeConfig config);

  // Whether the device is set up to run demo sessions.
  static bool IsDeviceInDemoMode();

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

  // Requests load of demo session resources, without marking the demo session
  // as started. Creates global DemoSession instance if required.
  static void PreloadOfflineResourcesIfInDemoMode();

  // Deletes the global DemoSession instance if it was previously created.
  static void ShutDownIfInitialized();

  // Gets the global demo session instance. Returns nullptr if the DemoSession
  // instance has not yet been initialized (either by calling
  // StartIfInDemoMode() or PreloadOfflineResourcesIfInDemoMode()).
  static DemoSession* Get();

  // Returns the id of the screensaver app based on the board name.
  static std::string GetScreensaverAppId();

  // Returns whether the app with |app_id| should be displayed in app launcher
  // in demo mode. Returns true for all apps in non-demo mode.
  static bool ShouldDisplayInAppLauncher(const std::string& app_id);

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Ensures that the load of offline demo session resources is requested.
  // |load_callback| will be run once the offline resource load finishes.
  void EnsureOfflineResourcesLoaded(base::OnceClosure load_callback);

  // Returns true if the Chrome app or ARC++ package, which is normally pinned
  // by policy, should actually not be force-pinned because the device is
  // in Demo Mode and offline.
  bool ShouldIgnorePinPolicy(const std::string& app_id_or_package);

  // Sets |extensions_external_loader_| and starts installing the screensaver.
  void SetExtensionsExternalLoader(
      scoped_refptr<DemoExtensionsExternalLoader> extensions_external_loader);

  // Sets app IDs and package names that shouldn't be pinned by policy when the
  // device is offline in Demo Mode.
  void OverrideIgnorePinPolicyAppsForTesting(std::vector<std::string> apps);

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(const user_manager::User* user) override;

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

  // Installs the CRX file from an update URL. Observes |ExtensionRegistry| to
  // launch the app upon installation.
  void InstallAppFromUpdateUrl(const std::string& id);

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;

  // Whether the device was offline-enrolled into demo mode, i.e. enrolled using
  // pre-built policies. Offline enrolled demo sessions do not have working
  // robot account associated with them.
  bool offline_enrolled_ = false;

  // Whether demo session has been started.
  bool started_ = false;

  // Apps that ShouldIgnorePinPolicy() will check for if the device is offline.
  std::vector<std::string> ignore_pin_policy_offline_apps_;

  std::unique_ptr<DemoResources> demo_resources_;

  ScopedObserver<session_manager::SessionManager,
                 session_manager::SessionManagerObserver>
      session_manager_observer_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  scoped_refptr<DemoExtensionsExternalLoader> extensions_external_loader_;

  base::WeakPtrFactory<DemoSession> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DemoSession);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SESSION_H_
