// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/policy_service.h"

#if defined(OS_ANDROID)
#include "components/policy/core/browser/android/policy_cache_updater_android.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/device_settings_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

class PrefService;

namespace policy {
class ConfigurationPolicyProvider;
class ProxyPolicyProvider;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
class ChromeBrowserCloudManagementController;
class MachineLevelUserCloudPolicyManager;
#endif

// Extends BrowserPolicyConnector with the setup shared among the desktop
// implementations and Android.
class ChromeBrowserPolicyConnector : public BrowserPolicyConnector {
 public:
  // Service initialization delay time in millisecond on startup. (So that
  // displaying Chrome's GUI does not get delayed.)
  static const int64_t kServiceInitializationStartupDelay = 5000;

  // Builds an uninitialized ChromeBrowserPolicyConnector, suitable for testing.
  // Init() should be called to create and start the policy machinery.
  ChromeBrowserPolicyConnector();
  ChromeBrowserPolicyConnector(const ChromeBrowserPolicyConnector&) = delete;
  ChromeBrowserPolicyConnector& operator=(const ChromeBrowserPolicyConnector&) =
      delete;
  ~ChromeBrowserPolicyConnector() override;

  // Called once the resource bundle has been created. Calls through to super
  // class to notify observers.
  void OnResourceBundleCreated();

  void Init(PrefService* local_state,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;

  bool IsDeviceEnterpriseManaged() const override;

  bool HasMachineLevelPolicies() override;

  void Shutdown() override;

  ConfigurationPolicyProvider* GetPlatformProvider();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ChromeBrowserCloudManagementController*
  chrome_browser_cloud_management_controller() {
    return chrome_browser_cloud_management_controller_.get();
  }
  MachineLevelUserCloudPolicyManager*
  machine_level_user_cloud_policy_manager() {
    return machine_level_user_cloud_policy_manager_.get();
  }

  ProxyPolicyProvider* proxy_policy_provider() {
    return proxy_policy_provider_;
  }

  ConfigurationPolicyProvider* command_line_policy_provider() {
    return command_line_provider_;
  }

  // On non-Android platforms, starts controller initialization right away.
  // On Android, delays controller initialization until platform policies have
  // been initialized, or starts controller initialization right away otherwise.
  void InitCloudManagementController(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Set ProxyPolicyProvider for testing, caller needs to init and shutdown the
  // provider.
  void SetProxyPolicyProviderForTesting(
      ProxyPolicyProvider* proxy_policy_provider);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // BrowserPolicyConnector:
  // Command line switch only supports Dev and Canary channel, trunk and
  // browser tests on Win, Mac, Linux and Android.
  bool IsCommandLineSwitchSupported() const override;

  static void EnableCommandLineSupportForTesting();

  virtual base::flat_set<std::string> device_affiliation_ids() const;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Checks if the main / primary user is managed or not.
  // TODO(crbug/1245077): Remove once Lacros handles all profiles the same way.
  bool IsMainUserManaged() const;

  // The device settings used in Lacros.
  crosapi::mojom::DeviceSettings* GetDeviceSettings() const;
#endif

 protected:
  // BrowserPolicyConnectorBase::
  std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
  CreatePolicyProviders() override;

 private:
  // Returns the policy provider that supplies platform policies.
  std::unique_ptr<ConfigurationPolicyProvider> CreatePlatformProvider();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Creates the MachineLevelUserCloudPolicyManager if the browser should be
  // enrolled in CBCM. On Android, the decision may be postponed until platform
  // policies have been loaded and it can be decided if an enrollment token is
  // available or not.
  void MaybeCreateCloudPolicyManager(
      std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>*
          providers);

  // Invoked once it can be decided if cloud management is enabled. If enabled,
  // invoked with a MachineLevelUserCloudPolicyManager instance. Otherwise,
  // nullptr is passed on instead.
  void OnMachineLevelCloudPolicyManagerCreated(
      std::unique_ptr<MachineLevelUserCloudPolicyManager>
          machine_level_user_cloud_policy_manager);

  std::unique_ptr<ChromeBrowserCloudManagementController>
      chrome_browser_cloud_management_controller_;

  // If CBCM enrollment is needed, then this proxy points to a
  // MachineLevelUserCloudPolicyManager object. Otherwise, this is innocuous.
  // Using a proxy allows unblocking PolicyService creation on platforms where
  // cloud management decision depends on non-CBCM policy providers to be
  // initialized (e.g. Android). Once platform policies are loaded, the proxy
  // can refer to the actual policy manager if cloud management is enabled.
  // Owned by base class.
  ProxyPolicyProvider* proxy_policy_provider_ = nullptr;

  // The MachineLevelUserCloudPolicyManager is not directly included in the
  // vector of policy providers (defined in the base class). A proxy policy
  // provider is used instead, so this class is responsible for holding
  // ownership of this object.
  std::unique_ptr<MachineLevelUserCloudPolicyManager>
      machine_level_user_cloud_policy_manager_;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
  std::unique_ptr<android::PolicyCacheUpdater> policy_cache_updater_;
#endif  // defined(OS_ANDROID)

  // Owned by base class.
  ConfigurationPolicyProvider* platform_provider_ = nullptr;

  // Owned by base class.
  ConfigurationPolicyProvider* command_line_provider_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<DeviceSettingsLacros> device_settings_ = nullptr;
#endif

  // Holds a callback to |ChromeBrowserCloudManagementController::Init| so that
  // its execution can be deferred until an enrollment token is available.
  base::OnceClosure deferred_init_callback_;

  // Weak pointers needed for tasks that need to wait until it can be decided
  // if an enrollment token is available or not.
  base::WeakPtrFactory<ChromeBrowserPolicyConnector> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_
