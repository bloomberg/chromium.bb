// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_MANAGER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

class WebAppSyncBridge;
class SystemWebAppManager;
class OsIntegrationManager;

// Policy installation allows enterprise admins to control and manage
// Web Apps on behalf of their managed users. This class tracks the policy that
// affects Web Apps and also tracks which Web Apps are currently installed based
// on this policy. Based on these, it decides which apps to install, uninstall,
// and update, via a ExternallyManagedAppManager.
class WebAppPolicyManager {
 public:
  static constexpr char kInstallResultHistogramName[] =
      "Webapp.InstallResult.Policy";

  // Constructs a WebAppPolicyManager instance that uses
  // |externally_managed_app_manager| to manage apps.
  // |externally_managed_app_manager| should outlive this class.
  explicit WebAppPolicyManager(Profile* profile);
  WebAppPolicyManager(const WebAppPolicyManager&) = delete;
  WebAppPolicyManager& operator=(const WebAppPolicyManager&) = delete;
  ~WebAppPolicyManager();

  void SetSubsystems(
      ExternallyManagedAppManager* externally_managed_app_manager,
      WebAppRegistrar* app_registrar,
      WebAppSyncBridge* sync_bridge,
      SystemWebAppManager* web_app_manager,
      OsIntegrationManager* os_integration_manager);

  void Start();

  void ReinstallPlaceholderAppIfNecessary(const GURL& url);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Used for handling SystemFeaturesDisableList policy. Checks if the app is
  // disabled and notifies sync_bridge_ about the current app state.
  void OnDisableListPolicyChanged();

  // Gets system web apps disabled by SystemFeaturesDisableList policy.
  const std::set<SystemAppType>& GetDisabledSystemWebApps() const;

  // Gets ids of web apps disabled by SystemFeaturesDisableList policy.
  const std::set<AppId>& GetDisabledWebAppsIds() const;

  // Checks if web app is disabled by SystemFeaturesDisableList policy.
  bool IsWebAppInDisabledList(const AppId& app_id) const;

  // Checks if UI mode of disabled web apps is hidden.
  bool IsDisabledAppsModeHidden() const;

  RunOnOsLoginPolicy GetUrlRunOnOsLoginPolicy(const AppId& app_id) const;

  void AddObserver(WebAppPolicyManagerObserver* observer);
  void RemoveObserver(WebAppPolicyManagerObserver* observer);

  void SetOnAppsSynchronizedCompletedCallbackForTesting(
      base::OnceClosure callback);
  void SetRefreshPolicySettingsCompletedCallbackForTesting(
      base::OnceClosure callback);

  // Changes the manifest to conform to the WebAppInstallForceList policy.
  void MaybeOverrideManifest(content::RenderFrameHost* frame_host,
                             blink::mojom::ManifestPtr& manifest) const;

 private:
  friend class WebAppPolicyManagerTest;

  struct WebAppSetting {
    WebAppSetting();
    WebAppSetting(const WebAppSetting&) = default;
    WebAppSetting& operator=(const WebAppSetting&) = default;
    ~WebAppSetting() = default;

    bool Parse(const base::Value& dict, bool for_default_settings);
    void ResetSettings();

    RunOnOsLoginPolicy run_on_os_login_policy;
  };

  struct CustomManifestValues {
    // The constructors and destructors have the "= default" implementations,
    // but they cannot be inlined.
    CustomManifestValues();
    CustomManifestValues(const CustomManifestValues&);
    ~CustomManifestValues();

    void SetName(const std::string& utf8_name);
    void SetIcon(const GURL& icon_gurl);

    absl::optional<std::u16string> name;
    absl::optional<std::vector<blink::Manifest::ImageResource>> icons;
  };

  void InitChangeRegistrarAndRefreshPolicy(bool enable_pwa_support);

  void RefreshPolicyInstalledApps();
  void RefreshPolicySettings();
  void OnAppsSynchronized(
      std::map<GURL, ExternallyManagedAppManager::InstallResult>
          install_results,
      std::map<GURL, bool> uninstall_results);
  void ApplyPolicySettings();

  void OverrideManifest(const GURL& custom_values_key,
                        blink::mojom::ManifestPtr& manifest) const;
  RunOnOsLoginPolicy GetUrlRunOnOsLoginPolicyByUnhashedAppId(
      const std::string& unhashed_app_id) const;

  // Parses install options from a Value, which represents one entry of the
  // kWepAppInstallForceList. If the value contains a custom_name or
  // custom_icon, it is inserted into the custom_manifest_values_by_url_ map.
  ExternalInstallOptions ParseInstallPolicyEntry(const base::Value& entry);

  void ObserveDisabledSystemFeaturesPolicy();

  void OnDisableModePolicyChanged();

  // Populates ids lists of web apps disabled by SystemFeaturesDisableList
  // policy.
  void PopulateDisabledWebAppsIdsLists();

  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> pref_service_;

  // Used to install, uninstall, and update apps. Should outlive this class
  // (owned by WebAppProvider).
  raw_ptr<ExternallyManagedAppManager> externally_managed_app_manager_ =
      nullptr;
  raw_ptr<WebAppRegistrar> app_registrar_ = nullptr;
  raw_ptr<WebAppSyncBridge> sync_bridge_ = nullptr;
  raw_ptr<SystemWebAppManager> web_app_manager_ = nullptr;
  raw_ptr<OsIntegrationManager> os_integration_manager_ = nullptr;

  PrefChangeRegistrar pref_change_registrar_;
  PrefChangeRegistrar local_state_pref_change_registrar_;
  // List of disabled system web apps, containing app types.
  std::set<SystemAppType> disabled_system_apps_;
  // List of disabled system and progressive web apps, containing app ids.
  std::set<AppId> disabled_web_apps_;

  // Testing callbacks
  base::OnceClosure refresh_policy_settings_completed_;
  base::OnceClosure on_apps_synchronized_;

  bool is_refreshing_ = false;
  bool needs_refresh_ = false;

  base::flat_map<std::string, WebAppSetting> settings_by_url_;
  base::flat_map<GURL, CustomManifestValues> custom_manifest_values_by_url_;
  std::unique_ptr<WebAppSetting> default_settings_;
  base::ObserverList<WebAppPolicyManagerObserver, /*check_empty=*/true>
      observers_;

  ExternallyInstalledWebAppPrefs externally_installed_app_prefs_;

  base::WeakPtrFactory<WebAppPolicyManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_MANAGER_H_
