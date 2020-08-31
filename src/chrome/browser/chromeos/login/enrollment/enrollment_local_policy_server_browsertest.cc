// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screens_utils.h"
#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/test_support/local_policy_test_server.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

std::string GetDmTokenFromPolicy(const std::string& blob) {
  enterprise_management::PolicyFetchResponse policy;
  CHECK(policy.ParseFromString(blob));

  enterprise_management::PolicyData policy_data;
  CHECK(policy_data.ParseFromString(policy.policy_data()));
  return policy_data.request_token();
}

}  // namespace

class EnrollmentLocalPolicyServerBase : public OobeBaseTest {
 public:
  EnrollmentLocalPolicyServerBase() = default;

  void SetUpOnMainThread() override {
    fake_gaia_.SetupFakeGaiaForLogin(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeUserGaiaId,
                                     FakeGaiaMixin::kFakeRefreshToken);
    policy_server_.SetUpdateDeviceAttributesPermission(false);
    OobeBaseTest::SetUpOnMainThread();
  }

 protected:
  LoginDisplayHost* host() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    EXPECT_NE(host, nullptr);
    return host;
  }

  EnrollmentScreen* enrollment_screen() {
    EXPECT_NE(WizardController::default_controller(), nullptr);
    EnrollmentScreen* enrollment_screen = EnrollmentScreen::Get(
        WizardController::default_controller()->screen_manager());
    EXPECT_NE(enrollment_screen, nullptr);
    return enrollment_screen;
  }

  AutoEnrollmentCheckScreen* auto_enrollment_screen() {
    EXPECT_NE(WizardController::default_controller(), nullptr);
    AutoEnrollmentCheckScreen* auto_enrollment_screen =
        AutoEnrollmentCheckScreen::Get(
            WizardController::default_controller()->screen_manager());
    EXPECT_NE(auto_enrollment_screen, nullptr);
    return auto_enrollment_screen;
  }

  void TriggerEnrollmentAndSignInSuccessfully() {
    host()->StartWizard(EnrollmentScreenView::kScreenId);
    OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

    ASSERT_FALSE(StartupUtils::IsDeviceRegistered());
    ASSERT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
    enrollment_screen()->OnLoginDone(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeAuthCode);
  }

  std::unique_ptr<content::WindowedNotificationObserver>
  CreateLoginVisibleWaiter() {
    return std::make_unique<content::WindowedNotificationObserver>(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources());
  }

  void ConfirmAndWaitLoginScreen() {
    auto login_screen_waiter = CreateLoginVisibleWaiter();
    enrollment_screen()->OnConfirmationClosed();
    login_screen_waiter->Wait();
  }

  void AddPublicUser(const std::string& account_id) {
    enterprise_management::ChromeDeviceSettingsProto proto;
    enterprise_management::DeviceLocalAccountInfoProto* account =
        proto.mutable_device_local_accounts()->add_account();
    account->set_account_id(account_id);
    account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                          ACCOUNT_TYPE_PUBLIC_SESSION);
    policy_server_.UpdateDevicePolicy(proto);
  }

  void SetLoginScreenLocale(const std::string& locale) {
    enterprise_management::ChromeDeviceSettingsProto proto;
    proto.mutable_login_screen_locales()->add_login_screen_locales(locale);
    policy_server_.UpdateDevicePolicy(proto);
  }

  LocalPolicyTestServerMixin policy_server_{&mixin_host_};
  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
  DeviceStateMixin device_state_{&mixin_host_,
                                 DeviceStateMixin::State::BEFORE_OOBE};

 private:
  DISALLOW_COPY_AND_ASSIGN(EnrollmentLocalPolicyServerBase);
};

class AutoEnrollmentLocalPolicyServer : public EnrollmentLocalPolicyServerBase {
 public:
  AutoEnrollmentLocalPolicyServer() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentLocalPolicyServerBase::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableForcedReEnrollment,
        AutoEnrollmentController::kForcedReEnrollmentAlways);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnrollmentInitialModulus, "5");
    command_line->AppendSwitchASCII(switches::kEnterpriseEnrollmentModulusLimit,
                                    "5");
  }

  policy::ServerBackedStateKeysBroker* state_keys_broker() {
    return g_browser_process->platform_part()
        ->browser_policy_connector_chromeos()
        ->GetStateKeysBroker();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentLocalPolicyServer);
};

class AutoEnrollmentWithStatistics : public AutoEnrollmentLocalPolicyServer {
 public:
  AutoEnrollmentWithStatistics() : AutoEnrollmentLocalPolicyServer() {
    // AutoEnrollmentController assumes that VPD is in valid state if
    // "serial_number" or "Product_S/N" could be read from it.
    fake_statistics_provider_.SetMachineStatistic(
        system::kSerialNumberKeyForTest, test::kTestSerialNumber);
  }

  ~AutoEnrollmentWithStatistics() override = default;

 protected:
  void SetFRERequiredKey(const std::string& value) {
    fake_statistics_provider_.SetMachineStatistic(system::kCheckEnrollmentKey,
                                                  value);
  }

  void SetActivateDate(const std::string& value) {
    fake_statistics_provider_.SetMachineStatistic(system::kActivateDateKey,
                                                  value);
  }

  void SetVPDCorrupted() {
    fake_statistics_provider_.ClearMachineStatistic(
        system::kSerialNumberKeyForTest);
  }

 private:
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentWithStatistics);
};

class AutoEnrollmentNoStateKeys : public AutoEnrollmentWithStatistics {
 public:
  AutoEnrollmentNoStateKeys() = default;
  ~AutoEnrollmentNoStateKeys() override = default;

  // AutoEnrollmentWithStatistics:
  void SetUpInProcessBrowserTestFixture() override {
    AutoEnrollmentWithStatistics::SetUpInProcessBrowserTestFixture();
    // Session manager client is initialized by DeviceStateMixin.
    FakeSessionManagerClient::Get()->set_force_state_keys_missing(true);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentNoStateKeys);
};

class InitialEnrollmentTest : public EnrollmentLocalPolicyServerBase {
 public:
  InitialEnrollmentTest() {
    policy_server_.ConfigureFakeStatisticsForZeroTouch(
        &fake_statistics_provider_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentLocalPolicyServerBase::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableInitialEnrollment,
        AutoEnrollmentController::kInitialEnrollmentAlways);
  }

 private:
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  DISALLOW_COPY_AND_ASSIGN(InitialEnrollmentTest);
};

// Simple manual enrollment.
// TODO(https://crbug.com/1031275): Slow on MSAN and debug builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ManualEnrollment DISABLED_ManualEnrollment
#else
#define MAYBE_ManualEnrollment ManualEnrollment
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_ManualEnrollment) {
  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// Simple manual enrollment with device attributes prompt.
// TODO(https://crbug.com/1031275): Slow on MSAN and debug builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ManualEnrollmentWithDeviceAttributes \
  DISABLED_ManualEnrollmentWithDeviceAttributes
#else
#define MAYBE_ManualEnrollmentWithDeviceAttributes \
  ManualEnrollmentWithDeviceAttributes
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_ManualEnrollmentWithDeviceAttributes) {
  policy_server_.SetUpdateDeviceAttributesPermission(true);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
  enrollment_ui_.SubmitDeviceAttributes(test::values::kAssetId,
                                        test::values::kLocation);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// Negative scenarios: see different HTTP error codes in
// device_management_service.cc

// Error during enrollment : 402 - missing licenses.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EnrollmentErrorNoLicenses DISABLED_EnrollmentErrorNoLicenses
#else
#define MAYBE_EnrollmentErrorNoLicenses EnrollmentErrorNoLicenses
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_EnrollmentErrorNoLicenses) {
  policy_server_.SetExpectedDeviceEnrollmentError(402);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 403 - management not allowed.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EnrollmentErrorManagementNotAllowed \
  DISABLED_EnrollmentErrorManagementNotAllowed
#else
#define MAYBE_EnrollmentErrorManagementNotAllowed \
  EnrollmentErrorManagementNotAllowed
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_EnrollmentErrorManagementNotAllowed) {
  policy_server_.SetExpectedDeviceEnrollmentError(403);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_AUTH_ACCOUNT_ERROR, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 405 - invalid device serial.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EnrollmentErrorInvalidDeviceSerial \
  DISABLED_EnrollmentErrorInvalidDeviceSerial
#else
#define MAYBE_EnrollmentErrorInvalidDeviceSerial \
  EnrollmentErrorInvalidDeviceSerial
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_EnrollmentErrorInvalidDeviceSerial) {
  policy_server_.SetExpectedDeviceEnrollmentError(405);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  // TODO (antrim, rsorokin): find out why it makes sense to retry here?
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER,
      /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 406 - domain mismatch
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EnrollmentErrorDomainMismatch \
  DISABLED_EnrollmentErrorDomainMismatch
#else
#define MAYBE_EnrollmentErrorDomainMismatch EnrollmentErrorDomainMismatch
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_EnrollmentErrorDomainMismatch) {
  policy_server_.SetExpectedDeviceEnrollmentError(406);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_DOMAIN_MISMATCH_ERROR, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 409 - Device ID is already in use
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EnrollmentErrorDeviceIDConflict \
  DISABLED_EnrollmentErrorDeviceIDConflict
#else
#define MAYBE_EnrollmentErrorDeviceIDConflict EnrollmentErrorDeviceIDConflict
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_EnrollmentErrorDeviceIDConflict) {
  policy_server_.SetExpectedDeviceEnrollmentError(409);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  // TODO (antrim, rsorokin): find out why it makes sense to retry here?
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_DEVICE_ID_CONFLICT, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 412 - Activation is pending
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EnrollmentErrorActivationIsPending \
  DISABLED_EnrollmentErrorActivationIsPending
#else
#define MAYBE_EnrollmentErrorActivationIsPending \
  EnrollmentErrorActivationIsPending
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_EnrollmentErrorActivationIsPending) {
  policy_server_.SetExpectedDeviceEnrollmentError(412);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_ACTIVATION_PENDING, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 417 - Consumer account with packaged license.
// Disable due to flaky crash/timeout on ChromeOS. https://crbug.com/1028650
IN_PROC_BROWSER_TEST_F(
    EnrollmentLocalPolicyServerBase,
    DISABLED_EnrollmentErrorConsumerAccountWithPackagedLicense) {
  policy_server_.SetExpectedDeviceEnrollmentError(417);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE,
      /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 500 - Consumer account with packaged license.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EnrollmentErrorServerError DISABLED_EnrollmentErrorServerError
#else
#define MAYBE_EnrollmentErrorServerError EnrollmentErrorServerError
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_EnrollmentErrorServerError) {
  policy_server_.SetExpectedDeviceEnrollmentError(500);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_TEMPORARY_UNAVAILABLE,
                                    /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorEnterpriseTosHasNotBeenAccepeted) {
  policy_server_.SetExpectedDeviceEnrollmentError(906);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED,
      /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : Strange HTTP response from server.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EnrollmentErrorServerIsDrunk DISABLED_EnrollmentErrorServerIsDrunk
#else
#define MAYBE_EnrollmentErrorServerIsDrunk EnrollmentErrorServerIsDrunk
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_EnrollmentErrorServerIsDrunk) {
  policy_server_.SetExpectedDeviceEnrollmentError(12345);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_HTTP_STATUS_ERROR,
                                    /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : Can not update device attributes
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EnrollmentErrorUploadingDeviceAttributes \
  DISABLED_EnrollmentErrorUploadingDeviceAttributes
#else
#define MAYBE_EnrollmentErrorUploadingDeviceAttributes \
  EnrollmentErrorUploadingDeviceAttributes
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_EnrollmentErrorUploadingDeviceAttributes) {
  policy_server_.SetUpdateDeviceAttributesPermission(true);
  policy_server_.SetExpectedDeviceAttributeUpdateError(500);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
  enrollment_ui_.SubmitDeviceAttributes(test::values::kAssetId,
                                        test::values::kLocation);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
  auto login_waiter = CreateLoginVisibleWaiter();
  enrollment_ui_.LeaveDeviceAttributeErrorScreen();
  login_waiter->Wait();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// Error during enrollment : Error fetching policy : 500 server error.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EnrollmentErrorFetchingPolicyTransient \
  DISABLED_EnrollmentErrorFetchingPolicyTransient
#else
#define MAYBE_EnrollmentErrorFetchingPolicyTransient \
  EnrollmentErrorFetchingPolicyTransient
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_EnrollmentErrorFetchingPolicyTransient) {
  policy_server_.SetExpectedPolicyFetchError(500);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_TEMPORARY_UNAVAILABLE,
                                    /* can retry */ true);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_ui_.RetryAfterError();
}

// Error during enrollment : Error fetching policy : 902 - policy not found.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EnrollmentErrorFetchingPolicyNotFound \
  DISABLED_EnrollmentErrorFetchingPolicyNotFound
#else
#define MAYBE_EnrollmentErrorFetchingPolicyNotFound \
  EnrollmentErrorFetchingPolicyNotFound
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_EnrollmentErrorFetchingPolicyNotFound) {
  policy_server_.SetExpectedPolicyFetchError(902);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_POLICY_NOT_FOUND,
      /* can retry */ true);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_ui_.RetryAfterError();
}

// Error during enrollment : Error fetching policy : 903 - deprovisioned.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EnrollmentErrorFetchingPolicyDeprovisioned \
  DISABLED_EnrollmentErrorFetchingPolicyDeprovisioned
#else
#define MAYBE_EnrollmentErrorFetchingPolicyDeprovisioned \
  EnrollmentErrorFetchingPolicyDeprovisioned
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       MAYBE_EnrollmentErrorFetchingPolicyDeprovisioned) {
  policy_server_.SetExpectedPolicyFetchError(903);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_SERVICE_DEPROVISIONED,
                                    /* can retry */ true);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_ui_.RetryAfterError();
}

// No state keys on the server. Auto enrollment check should proceed to login.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_AutoEnrollmentCheck DISABLED_AutoEnrollmentCheck
#else
#define MAYBE_AutoEnrollmentCheck AutoEnrollmentCheck
#endif
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer,
                       MAYBE_AutoEnrollmentCheck) {
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// State keys are present but restore mode is not requested.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ReenrollmentNone DISABLED_ReenrollmentNone
#else
#define MAYBE_ReenrollmentNone ReenrollmentNone
#endif
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer,
                       MAYBE_ReenrollmentNone) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::RESTORE_MODE_NONE,
      test::kTestDomain));
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// Reenrollment requested. User can skip.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ReenrollmentRequested DISABLED_ReenrollmentRequested
#else
#define MAYBE_ReenrollmentRequested ReenrollmentRequested
#endif
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer,
                       MAYBE_ReenrollmentRequested) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_REQUESTED,
      test::kTestDomain));
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_screen()->OnCancel();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// Reenrollment forced. User can not skip.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ReenrollmentForced DISABLED_ReenrollmentForced
#else
#define MAYBE_ReenrollmentForced ReenrollmentForced
#endif
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer,
                       MAYBE_ReenrollmentForced) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ENFORCED,
      test::kTestDomain));
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, enrollment_ui_.WaitForScreenExit());
}

// Device is disabled.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_DeviceDisabled DISABLED_DeviceDisabled
#else
#define MAYBE_DeviceDisabled DeviceDisabled
#endif
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, MAYBE_DeviceDisabled) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_DISABLED,
      test::kTestDomain));
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(DeviceDisabledScreenView::kScreenId).Wait();
}

// Attestation enrollment.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_Attestation DISABLED_Attestation
#else
#define MAYBE_Attestation Attestation
#endif
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, MAYBE_Attestation) {
  policy_server_.SetFakeAttestationFlow();
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ZERO_TOUCH,
      test::kTestDomain));

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// FRE explicitly required in VPD, but the state keys are missing.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_FREExplicitlyRequired DISABLED_FREExplicitlyRequired
#else
#define MAYBE_FREExplicitlyRequired FREExplicitlyRequired
#endif
IN_PROC_BROWSER_TEST_F(AutoEnrollmentNoStateKeys, MAYBE_FREExplicitlyRequired) {
  SetFRERequiredKey("1");
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(AutoEnrollmentCheckScreenView::kScreenId).Wait();

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().ExpectHasNoClass("allow-guest-signin", {"error-message"});
}

// FRE not explicitly required and the state keys are missing. Should proceed to
// normal signin.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_NotRequired DISABLED_NotRequired
#else
#define MAYBE_NotRequired NotRequired
#endif
IN_PROC_BROWSER_TEST_F(AutoEnrollmentNoStateKeys, MAYBE_NotRequired) {
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// FRE explicitly not required in VPD, so it should not even contact the policy
// server.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ExplicitlyNotRequired DISABLED_ExplicitlyNotRequired
#else
#define MAYBE_ExplicitlyNotRequired ExplicitlyNotRequired
#endif
IN_PROC_BROWSER_TEST_F(AutoEnrollmentWithStatistics,
                       MAYBE_ExplicitlyNotRequired) {
  SetFRERequiredKey("0");

  // Should be ignored.
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ENFORCED,
      test::kTestDomain));

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// FRE is not required when VPD is valid and activate date is not there.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_MachineNotActivated DISABLED_MachineNotActivated
#else
#define MAYBE_MachineNotActivated MachineNotActivated
#endif
IN_PROC_BROWSER_TEST_F(AutoEnrollmentWithStatistics,
                       MAYBE_MachineNotActivated) {
  // Should be ignored.
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ENFORCED,
      test::kTestDomain));

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// FRE is required when VPD is valid and activate date is there.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_MachineActivated DISABLED_MachineActivated
#else
#define MAYBE_MachineActivated MachineActivated
#endif
IN_PROC_BROWSER_TEST_F(AutoEnrollmentWithStatistics, MAYBE_MachineActivated) {
  SetActivateDate("1970-01");

  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ENFORCED,
      test::kTestDomain));

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
}

// FRE is required when VPD in invalid state.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_CorruptedVPD DISABLED_CorruptedVPD
#else
#define MAYBE_CorruptedVPD CorruptedVPD
#endif
IN_PROC_BROWSER_TEST_F(AutoEnrollmentWithStatistics, MAYBE_CorruptedVPD) {
  SetVPDCorrupted();

  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ENFORCED,
      test::kTestDomain));

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
}

class EnrollmentRecoveryTest : public EnrollmentLocalPolicyServerBase {
 public:
  EnrollmentRecoveryTest() : EnrollmentLocalPolicyServerBase() {
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  }

  ~EnrollmentRecoveryTest() override = default;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    EnrollmentLocalPolicyServerBase::SetUpInProcessBrowserTestFixture();

    // This triggers recovery enrollment.
    device_state_.RequestDevicePolicyUpdate()->policy_data()->Clear();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EnrollmentRecoveryTest);
};

// TODO(https://crbug.com/995784): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_Success DISABLED_Success
#else
#define MAYBE_Success Success
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentRecoveryTest, MAYBE_Success) {
  test::SkipToEnrollmentOnRecovery();

  ASSERT_TRUE(StartupUtils::IsDeviceRegistered());
  ASSERT_TRUE(InstallAttributes::Get()->IsEnterpriseManaged());
  // No DM Token
  ASSERT_TRUE(
      GetDmTokenFromPolicy(FakeSessionManagerClient::Get()->device_policy())
          .empty());

  // User can't skip.
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, enrollment_ui_.WaitForScreenExit());

  enrollment_screen()->OnLoginDone(FakeGaiaMixin::kEnterpriseUser1,
                                   FakeGaiaMixin::kFakeAuthCode);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);

  // DM Token is in the device policy.
  EXPECT_FALSE(
      GetDmTokenFromPolicy(FakeSessionManagerClient::Get()->device_policy())
          .empty());
}

// TODO(https://crbug.com/995784): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_DifferentDomain DISABLED_DifferentDomain
#else
#define MAYBE_DifferentDomain DifferentDomain
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentRecoveryTest, MAYBE_DifferentDomain) {
  test::SkipToEnrollmentOnRecovery();

  ASSERT_TRUE(StartupUtils::IsDeviceRegistered());
  ASSERT_TRUE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_screen()->OnLoginDone(FakeGaiaMixin::kFakeUserEmail,
                                   FakeGaiaMixin::kFakeAuthCode);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_USER, true);
  enrollment_ui_.RetryAfterError();
}

// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EnrollmentForced DISABLED_EnrollmentForced
#else
#define MAYBE_EnrollmentForced EnrollmentForced
#endif
IN_PROC_BROWSER_TEST_F(InitialEnrollmentTest, MAYBE_EnrollmentForced) {
  auto initial_enrollment =
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ENROLLMENT_ENFORCED;
  policy_server_.SetDeviceInitialEnrollmentResponse(
      test::kTestRlzBrandCodeKey, test::kTestSerialNumber, initial_enrollment,
      test::kTestDomain, base::nullopt /* is_license_packaged_with_device */);

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

  // User can't skip.
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, enrollment_ui_.WaitForScreenExit());

  // Domain is actually different from what the server sent down. But Chrome
  // does not enforce that domain if device is not locked.
  enrollment_screen()->OnLoginDone(FakeGaiaMixin::kEnterpriseUser1,
                                   FakeGaiaMixin::kFakeAuthCode);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Zero touch with attestation authentication fail. Attestation fails because we
// send empty cert request. Should switch to interactive authentication.
// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ZeroTouchForcedAttestationFail \
  DISABLED_ZeroTouchForcedAttestationFail
#else
#define MAYBE_ZeroTouchForcedAttestationFail ZeroTouchForcedAttestationFail
#endif
IN_PROC_BROWSER_TEST_F(InitialEnrollmentTest,
                       MAYBE_ZeroTouchForcedAttestationFail) {
  auto initial_enrollment =
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED;
  policy_server_.SetDeviceInitialEnrollmentResponse(
      test::kTestRlzBrandCodeKey, test::kTestSerialNumber, initial_enrollment,
      test::kTestDomain, base::nullopt /* is_license_packaged_with_device */);

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

  // First it tries with attestation auth and should fail.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_CERT_FETCH_FAILED,
      /* can retry */ true);

  // Cancel bring up Gaia sing-in page.
  enrollment_screen()->OnCancel();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);

  // User can't skip.
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, enrollment_ui_.WaitForScreenExit());

  // Domain is actually different from what the server sent down. But Chrome
  // does not enforce that domain if device is not locked.
  enrollment_screen()->OnLoginDone(FakeGaiaMixin::kEnterpriseUser1,
                                   FakeGaiaMixin::kFakeAuthCode);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ZeroTouchForcedAttestationSuccess \
  DISABLED_ZeroTouchForcedAttestationSuccess
#else
#define MAYBE_ZeroTouchForcedAttestationSuccess \
  ZeroTouchForcedAttestationSuccess
#endif
IN_PROC_BROWSER_TEST_F(InitialEnrollmentTest,
                       MAYBE_ZeroTouchForcedAttestationSuccess) {
  policy_server_.SetupZeroTouchForcedEnrollment();

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

class OobeGuestButtonPolicy : public testing::WithParamInterface<bool>,
                              public EnrollmentLocalPolicyServerBase {
 public:
  OobeGuestButtonPolicy() = default;

  void SetUpOnMainThread() override {
    enterprise_management::ChromeDeviceSettingsProto proto;
    proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(GetParam());
    policy_server_.UpdateDevicePolicy(proto);
    EnrollmentLocalPolicyServerBase::SetUpOnMainThread();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeGuestButtonPolicy);
};

// TODO(https://crbug.com/1031275): Slow on MSAN builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_VisibilityAfterEnrollment DISABLED_VisibilityAfterEnrollment
#else
#define MAYBE_VisibilityAfterEnrollment VisibilityAfterEnrollment
#endif
IN_PROC_BROWSER_TEST_P(OobeGuestButtonPolicy, MAYBE_VisibilityAfterEnrollment) {
  TriggerEnrollmentAndSignInSuccessfully();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  ConfirmAndWaitLoginScreen();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();

  ASSERT_EQ(GetParam(),
            user_manager::UserManager::Get()->IsGuestSessionAllowed());
  EXPECT_EQ(GetParam(), ash::LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('showGuestInOobe', [false]);");
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('showGuestInOobe', [true]);");
  EXPECT_EQ(GetParam(), ash::LoginScreenTestApi::IsGuestButtonShown());
}

INSTANTIATE_TEST_SUITE_P(All,
                         OobeGuestButtonPolicy,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase, SwitchToViews) {
  base::HistogramTester histogram_tester;
  TriggerEnrollmentAndSignInSuccessfully();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  ConfirmAndWaitLoginScreen();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  histogram_tester.ExpectTotalCount("OOBE.WebUIToViewsSwitch.Duration", 1);
}

IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       SwitchToViewsLocalUsers) {
  AddPublicUser("test_user");
  base::HistogramTester histogram_tester;
  TriggerEnrollmentAndSignInSuccessfully();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  ConfirmAndWaitLoginScreen();
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 1);
  histogram_tester.ExpectTotalCount("OOBE.WebUIToViewsSwitch.Duration", 1);
}

IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase, SwitchToViewsLocales) {
  auto initial_label = ash::LoginScreenTestApi::GetShutDownButtonLabel();

  SetLoginScreenLocale("ru-RU");
  base::HistogramTester histogram_tester;
  TriggerEnrollmentAndSignInSuccessfully();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  ConfirmAndWaitLoginScreen();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_NE(ash::LoginScreenTestApi::GetShutDownButtonLabel(), initial_label);
  histogram_tester.ExpectTotalCount("OOBE.WebUIToViewsSwitch.Duration", 1);
}

}  // namespace chromeos
