// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

class PrefRegistrySimple;

namespace policy {
class DeviceCloudPolicyManagerChromeOS;
}

namespace chromeos {

// Controlls enrollment flow for setting up Demo Mode.
class DemoSetupController
    : public EnterpriseEnrollmentHelper::EnrollmentStatusConsumer,
      public policy::CloudPolicyStore::Observer {
 public:
  // Type of demo mode setup error.
  enum class DemoSetupError {
    // Recoverable or temporary demo mode setup error. Another attempt to setup
    // demo mode may succeed.
    kRecoverable,
    // Fatal demo mode setup error. Device requires powerwash to recover from
    // the resulting state.
    kFatal,
  };

  // Demo mode setup callbacks.
  using OnSetupSuccess = base::OnceClosure;
  using OnSetupError = base::OnceCallback<void(DemoSetupError)>;

  // Domain that demo mode devices are enrolled into.
  static constexpr char kDemoModeDomain[] = "cros-demo-mode.com";

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Clears demo device enrollment requisition on the given |policy_manager| if
  // it is set.
  static void ClearDemoRequisition(
      policy::DeviceCloudPolicyManagerChromeOS* policy_manager);

  // Utility method that returns whether demo mode is allowed on the device.
  static bool IsDemoModeAllowed();

  // Utility method that returns whether offline demo mode is allowed on the
  // device.
  static bool IsOfflineDemoModeAllowed();

  // Utility method that returns whether demo mode setup flow is in progress in
  // OOBE.
  static bool IsOobeDemoSetupFlowInProgress();

  DemoSetupController();
  ~DemoSetupController() override;

  // Sets demo mode config that will be used to setup the device. It has to be
  // set before calling Enroll().
  void set_demo_config(DemoSession::DemoModeConfig demo_config) {
    demo_config_ = demo_config;
  }

  // Whether offline enrollment is used for setup.
  bool IsOfflineEnrollment() const;

  // Initiates enrollment that sets up the device in the demo mode domain. The
  // |enrollment_type_| determines whether online or offline setup will be
  // performed and it should be set with set_enrollment_type() before calling
  // Enroll(). |on_setup_success| will be called when enrollment finishes
  // successfully. |on_setup_error| will be called when enrollment finishes with
  // an error.
  void Enroll(OnSetupSuccess on_setup_success, OnSetupError on_setup_error);

  // EnterpriseEnrollmentHelper::EnrollmentStatusConsumer:
  void OnDeviceEnrolled(const std::string& additional_token) override;
  void OnEnrollmentError(policy::EnrollmentStatus status) override;
  void OnAuthError(const GoogleServiceAuthError& error) override;
  void OnOtherError(EnterpriseEnrollmentHelper::OtherError error) override;
  void OnDeviceAttributeUploadCompleted(bool success) override;
  void OnDeviceAttributeUpdatePermission(bool granted) override;
  void OnMultipleLicensesAvailable(
      const EnrollmentLicenseMap& licenses) override;

  void SetCrOSComponentLoadErrorForTest(
      component_updater::CrOSComponentManager::Error error);
  void SetDeviceLocalAccountPolicyStoreForTest(policy::CloudPolicyStore* store);
  void SetOfflineDataDirForTest(const base::FilePath& offline_dir);

 private:
  // Attempts to load the CrOS component with demo resources for online
  // enrollment and passes the result to OnDemoResourcesCrOSComponentLoaded().
  void LoadDemoResourcesCrOSComponent();

  // Callback to initiate online enrollment once the CrOS component has loaded.
  // If the component loaded successfully, registers and sets up the device in
  // the demo mode domain. If |error| indicates the component couldn't be
  // loaded, demo setup will fail.
  void OnDemoResourcesCrOSComponentLoaded(
      component_updater::CrOSComponentManager::Error error,
      const base::FilePath& path);

  // Initiates offline enrollment that locks the device and sets up offline
  // policies required by demo mode. It requires no network connectivity since
  // all setup will be done locally. The policy files will be loaded from the
  // |policy_dir|.
  void EnrollOffline(const base::FilePath& policy_dir);

  // Called when the checks of policy files for the offline demo mode is done.
  void OnOfflinePolicyFilesExisted(std::string* message, bool ok);

  // Called when the device local account policy for the offline demo mode is
  // loaded.
  void OnDeviceLocalAccountPolicyLoaded(base::Optional<std::string> blob);

  // Called when device is marked as registered and the second part of OOBE flow
  // is completed. This is the last step of demo mode setup flow.
  void OnDeviceRegistered();

  // Finish the flow with an error message.
  void SetupFailed(const std::string& message, DemoSetupError error);

  // Clears the internal state.
  void Reset();

  // policy::CloudPolicyStore::Observer:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

  // Demo mode configuration type that will be setup when Enroll() is called.
  // Should be set explicitly.
  DemoSession::DemoModeConfig demo_config_ = DemoSession::DemoModeConfig::kNone;

  // Error code to use when attempting to load the demo resources CrOS
  // component.
  component_updater::CrOSComponentManager::Error component_error_for_tests_ =
      component_updater::CrOSComponentManager::Error::NONE;

  // Callback to call when enrollment finishes with an error.
  OnSetupError on_setup_error_;

  // Callback to call when enrollment finishes successfully.
  OnSetupSuccess on_setup_success_;

  // The directory which contains the policy blob files for the offline
  // enrollment (i.e. device_policy and local_account_policy). Should be empty
  // on the online enrollment.
  base::FilePath policy_dir_;

  // The directory containing policy blob files used for testing.
  base::FilePath policy_dir_for_tests_;

  // The CloudPolicyStore for the device local account for the offline policy.
  policy::CloudPolicyStore* device_local_account_policy_store_ = nullptr;

  std::unique_ptr<EnterpriseEnrollmentHelper> enrollment_helper_;

  base::WeakPtrFactory<DemoSetupController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupController);
};

}  //  namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_
