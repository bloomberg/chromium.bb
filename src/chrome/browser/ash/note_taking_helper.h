// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTE_TAKING_HELPER_H_
#define CHROME_BROWSER_ASH_NOTE_TAKING_HELPER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace apps {
class AppUpdate;
}

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;
namespace api {
namespace app_runtime {
struct ActionData;
}  // namespace app_runtime
}  // namespace api
}  // namespace extensions

namespace ash {

class NoteTakingControllerClient;

// Describes an app's level of support for lock screen enabled note taking.
// IMPORTANT: These constants are used in settings UI, so be careful about
//     reordering/adding/removing items.
enum class NoteTakingLockScreenSupport {
  // The app does not support note taking on lock screen.
  kNotSupported = 0,
  // The app supports lock screen note taking, but is not allowed to run on the
  // lock screen due to policy settings.
  kNotAllowedByPolicy = 1,
  // The app supports note taking on lock screen, but is not enabled as a
  // lock screen note taking app by the user. This state implies that the user
  // can be offered to enable this app as the lock screen note taking handler.
  kSupported = 2,
  // The app is enabled by the user to run as a note taking handler on the lock
  // screen. Note that, while more than one app can be in enabled state at a
  // same time, currently only the preferred note taking app will be launchable
  // from the lock screen UI.
  kEnabled = 3,
};

// Information about an installed note-taking app.
struct NoteTakingAppInfo {
  // Application name to display to user.
  std::string name;

  // Either an extension ID (in the case of a Chrome app) or a package name (in
  // the case of an Android app) or a web app ID (in the case of a web app).
  std::string app_id;

  // True if this is the preferred note-taking app.
  bool preferred;

  // Whether the app supports taking notes on Chrome OS lock screen. Note that
  // this ability is guarded by enable-lock-screen-apps feature flag, and is
  // currently restricted to Keep apps.
  NoteTakingLockScreenSupport lock_screen_support;
};

// Singleton class used to launch a note-taking app.
class NoteTakingHelper : public arc::ArcIntentHelperObserver,
                         public arc::ArcSessionManagerObserver,
                         public extensions::ExtensionRegistryObserver,
                         public apps::AppRegistryCache::Observer,
                         public ProfileManagerObserver {
 public:
  // Interface for observing changes to the list of available apps.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when the list of available apps that will be returned by
    // GetAvailableApps() changes or when |play_store_enabled_| changes state.
    virtual void OnAvailableNoteTakingAppsUpdated() = 0;

    // Called when the preferred note taking app (or its properties) in
    // |profile| is updated.
    virtual void OnPreferredNoteTakingAppUpdated(Profile* profile) = 0;
  };

  // Describes the result of an attempt to launch a note-taking app. Values must
  // not be renumbered, as this is used by histogram metrics.
  enum class LaunchResult {
    // A Chrome app was launched successfully.
    CHROME_SUCCESS = 0,
    // The requested Chrome app was unavailable.
    CHROME_APP_MISSING = 1,
    // An Android app was launched successfully.
    ANDROID_SUCCESS = 2,
    // An Android app couldn't be launched due to the profile not being allowed
    // to use ARC.
    ANDROID_NOT_SUPPORTED_BY_PROFILE = 3,
    // An Android app couldn't be launched due to ARC not running.
    ANDROID_NOT_RUNNING = 4,
    // An Android app couldn't be launched due to a failure to convert the
    // supplied path to an ARC URL.
    ANDROID_FAILED_TO_CONVERT_PATH = 5,
    // No attempt was made due to a preferred app not being specified.
    NO_APP_SPECIFIED = 6,
    // No Android or Chrome apps were available.
    NO_APPS_AVAILABLE = 7,
    // A web app was launched successfully.
    WEB_APP_SUCCESS = 8,
    // The requested web app was unavailable.
    WEB_APP_MISSING = 9,
    // This value must remain last and should be incremented when a new reason
    // is inserted.
    MAX = 10,
  };

  // Callback used to launch a Chrome app.
  using LaunchChromeAppCallback = base::RepeatingCallback<void(
      content::BrowserContext* context,
      const extensions::Extension*,
      std::unique_ptr<extensions::api::app_runtime::ActionData>,
      const base::FilePath&)>;

  // Intent action used to launch Android apps.
  static const char kIntentAction[];

  // Extension IDs for the development and released versions of the Google Keep
  // Chrome app.
  static const char kDevKeepExtensionId[];
  static const char kProdKeepExtensionId[];
  // Web app IDs for testing and development versions of note-taking web apps.
  static const char kNoteTakingWebAppIdTest[];
  static const char kNoteTakingWebAppIdDev[];

  // Names of histograms.
  static const char kPreferredLaunchResultHistogramName[];
  static const char kDefaultLaunchResultHistogramName[];

  static void Initialize();
  static void Shutdown();
  static NoteTakingHelper* Get();

  NoteTakingHelper(const NoteTakingHelper&) = delete;
  NoteTakingHelper& operator=(const NoteTakingHelper&) = delete;

  bool play_store_enabled() const { return play_store_enabled_; }
  bool android_apps_received() const { return android_apps_received_; }

  void set_launch_chrome_app_callback_for_test(
      const LaunchChromeAppCallback& callback) {
    launch_chrome_app_callback_ = callback;
  }

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns a list of available note-taking apps, in the order they should be
  // shown in UI.
  std::vector<NoteTakingAppInfo> GetAvailableApps(Profile* profile);

  // Returns info for the preferred lock screen app, if it exists.
  std::unique_ptr<NoteTakingAppInfo> GetPreferredLockScreenAppInfo(
      Profile* profile);

  // Sets the preferred note-taking app. |app_id| is a value from a
  // NoteTakingAppInfo object.
  void SetPreferredApp(Profile* profile, const std::string& app_id);

  // Enables or disables preferred note taking apps from running on the lock
  // screen.
  // Returns whether the app status changed.
  bool SetPreferredAppEnabledOnLockScreen(Profile* profile, bool enabled);

  // Returns true if an app that can be used to take notes is available. UI
  // surfaces that call LaunchAppForNewNote() should be hidden otherwise.
  bool IsAppAvailable(Profile* profile);

  // Launches the note-taking app to create a new note, optionally additionally
  // passing a file (|path| may be empty). IsAppAvailable() must be called
  // first.
  void LaunchAppForNewNote(Profile* profile, const base::FilePath& path);

  // arc::ArcIntentHelperObserver:
  void OnIntentFiltersUpdated(
      const absl::optional<std::string>& package_name) override;

  // arc::ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // Sets the profile which supports note taking apps on the lock screen.
  void SetProfileWithEnabledLockScreenApps(Profile* profile);

  NoteTakingControllerClient* GetNoteTakingControllerClientForTesting() {
    return note_taking_controller_client_.get();
  }

 private:
  // The state of the allowed app ID cache (used for determining the state of
  // note-taking apps allowed on the lock screen).
  enum class AllowedAppListState {
    // The allowed apps have not yet been determined.
    kUndetermined,
    // No app ID restriction exists in the profile.
    kAllAppsAllowed,
    // A list of allowed app IDs exists in the profile.
    kAllowedAppsListed
  };

  NoteTakingHelper();
  ~NoteTakingHelper() override;

  // Returns whether |app_id| has been explicitly allowed as a note-taking app
  // by command-line or default. |app_id| is a Chrome app or web app ID.
  bool IsAllowedApp(const std::string& app_id) const;

  // Queries and returns the app IDs of note-taking Chrome/web apps that are
  // installed, enabled, and allowed for |profile|.
  std::vector<std::string> GetNoteTakingAppIds(Profile* profile) const;

  // Requests a list of Android note-taking apps from ARC.
  void UpdateAndroidApps();

  // Handles ARC's response to an earlier UpdateAndroidApps() call.
  void OnGotAndroidApps(std::vector<arc::mojom::IntentHandlerInfoPtr> handlers);

  // Helper method that launches |app_id| (either an Android package name or a
  // Chrome extension ID) to create a new note with an optional attached file at
  // |path|. Returns the attempt's result.
  LaunchResult LaunchAppInternal(Profile* profile,
                                 const std::string& app_id,
                                 const base::FilePath& path);

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // apps::AppRegistryCache::Observer
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // Determines the state of the app's support for lock screen note taking using
  // installation information from |profile|.
  NoteTakingLockScreenSupport GetLockScreenSupportForAppId(
      Profile* profile,
      const std::string& app_id);

  // Called when kNoteTakingAppsLockScreenAllowlist pref changes for
  // |profile_with_enabled_lock_screen_apps_|.
  void OnAllowedNoteTakingAppsChanged();

  // Updates the cached list of note-taking apps allowed on the lock screen - it
  // sets |allowed_lock_screen_apps_state_|  and
  // |allowed_lock_screen_apps_by_policy_| to values appropriate for the current
  // |profile_with_enabled_lock_screen_apps_| state.
  void UpdateAllowedLockScreenAppsList();

  // True iff Play Store is enabled (i.e. per the checkbox on the settings
  // page). Note that ARC may not be fully started yet when this is true, but it
  // is expected to start eventually. Similarly, ARC may not be fully shut down
  // yet when this is false, but will be eventually.
  bool play_store_enabled_ = false;

  // This is set to true after |android_apps_| is updated.
  bool android_apps_received_ = false;

  // Callback used to launch Chrome apps. Can be overridden for tests.
  LaunchChromeAppCallback launch_chrome_app_callback_;

  // IDs of allowed (but not necessarily installed) Chrome apps or web apps for
  // note-taking, in the order in which they're chosen if the user hasn't
  // expressed a preference.
  std::vector<std::string> allowed_app_ids_;

  // Cached information about available Android note-taking apps.
  std::vector<NoteTakingAppInfo> android_apps_;

  // Tracks ExtensionRegistry observation for different profiles.
  // TODO(crbug.com/1225848): Remove when App Service publishes Chrome Apps.
  base::ScopedMultiSourceObservation<extensions::ExtensionRegistry,
                                     extensions::ExtensionRegistryObserver>
      extension_registry_observations_{this};

  // Obseves App Registry for all profiles with an App Registry.
  base::ScopedMultiSourceObservation<apps::AppRegistryCache,
                                     apps::AppRegistryCache::Observer>
      app_registry_observations_{this};

  // The profile for which lock screen apps are enabled,
  Profile* profile_with_enabled_lock_screen_apps_ = nullptr;

  // The current AllowedAppListState for lock screen note taking in
  // |profile_with_enabled_lock_screen_apps_|. If kAllowedAppsListed,
  // |lock_screen_apps_allowed_by_policy_| should contain the set of allowed
  // app IDs.
  AllowedAppListState allowed_lock_screen_apps_state_ =
      AllowedAppListState::kUndetermined;

  // If |allowed_lock_screen_apps_state_| is kAllowedAppsListed, contains all
  // app IDs that are allowed to handle new-note action on the lock screen. The
  // set should only be used for apps from
  // |profile_with_enabled_lock_screen_apps_| and when
  // |allowed_lock_screen_apps_state_| equals kAllowedAppsListed.
  std::set<std::string> allowed_lock_screen_apps_by_policy_;

  // Tracks kNoteTakingAppsLockScreenAllowlist pref for the profile for which
  // lock screen apps are enabled.
  PrefChangeRegistrar pref_change_registrar_;

  base::ObserverList<Observer>::Unchecked observers_;

  std::unique_ptr<NoteTakingControllerClient> note_taking_controller_client_;

  base::WeakPtrFactory<NoteTakingHelper> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
using ::ash::NoteTakingAppInfo;
using ::ash::NoteTakingHelper;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_NOTE_TAKING_HELPER_H_
