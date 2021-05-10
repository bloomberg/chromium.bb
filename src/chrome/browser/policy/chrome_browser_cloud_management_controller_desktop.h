// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_DESKTOP_H_
#define CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_DESKTOP_H_

#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"

#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"

class DeviceIdentityProvider;

namespace enterprise_reporting {
class ReportScheduler;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace invalidation {
class FCMInvalidationService;
}

namespace policy {
class ChromeBrowserCloudManagementRegisterWatcher;
class CloudPolicyInvalidator;
class MachineLevelDeviceAccountInitializerHelper;
class RemoteCommandsInvalidator;

// Desktop implementation of the platform-specific operations of CBCMController.
class ChromeBrowserCloudManagementControllerDesktop
    : public ChromeBrowserCloudManagementController::Delegate {
 public:
  ChromeBrowserCloudManagementControllerDesktop();
  ChromeBrowserCloudManagementControllerDesktop(
      const ChromeBrowserCloudManagementControllerDesktop&) = delete;
  ChromeBrowserCloudManagementControllerDesktop& operator=(
      const ChromeBrowserCloudManagementControllerDesktop&) = delete;

  ~ChromeBrowserCloudManagementControllerDesktop() override;

  // ChromeBrowserCloudManagementController::Delegate implementation.
  void SetDMTokenStorageDelegate() override;
  int GetUserDataDirKey() override;
  base::FilePath GetExternalPolicyPath() override;
  NetworkConnectionTrackerGetter CreateNetworkConnectionTrackerGetter()
      override;
  void InitializeOAuthTokenFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* local_state) override;
  void StartWatchingRegistration(
      ChromeBrowserCloudManagementController* controller) override;
  bool WaitUntilPolicyEnrollmentFinished() override;
  bool IsEnterpriseStartupDialogShowing() override;
  void OnServiceAccountSet(CloudPolicyClient* client,
                           const std::string& account_email) override;
  void ShutDown() override;
  MachineLevelUserCloudPolicyManager* GetMachineLevelUserCloudPolicyManager()
      override;
  DeviceManagementService* GetDeviceManagementService() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override;
  std::unique_ptr<enterprise_reporting::ReportScheduler> CreateReportScheduler(
      CloudPolicyClient* client) override;

  scoped_refptr<base::SingleThreadTaskRunner> GetBestEffortTaskRunner()
      override;

  void SetGaiaURLLoaderFactory(scoped_refptr<network::SharedURLLoaderFactory>
                                   url_loader_factory) override;

 private:
  // Starts the services required for Policy Invalidations over FCM to be
  // enabled.
  void StartInvalidations();

  // Called by the DeviceAccountInitializer when the device service account is
  // ready.
  void AccountInitCallback(const std::string& account_email, bool success);

  enterprise_reporting::ReportingDelegateFactoryDesktop
      reporting_delegate_factory_;

  std::unique_ptr<ChromeBrowserCloudManagementRegisterWatcher>
      cloud_management_register_watcher_;

  // These objects are all involved in Policy Invalidations.
  std::unique_ptr<MachineLevelDeviceAccountInitializerHelper>
      account_initializer_helper_;
  scoped_refptr<network::SharedURLLoaderFactory> gaia_url_loader_factory_;
  std::unique_ptr<DeviceIdentityProvider> identity_provider_;
  std::unique_ptr<instance_id::InstanceIDDriver> device_instance_id_driver_;
  std::unique_ptr<invalidation::FCMInvalidationService> invalidation_service_;
  std::unique_ptr<CloudPolicyInvalidator> policy_invalidator_;

  // This invalidator is responsible for receiving remote commands invalidations
  std::unique_ptr<RemoteCommandsInvalidator> commands_invalidator_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_DESKTOP_H_
