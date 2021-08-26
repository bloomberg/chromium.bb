// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_session_manager.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/arc_demo_mode_delegate_impl.h"
#include "chrome/browser/ash/arc/arc_migration_guide_notification.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/arc_support_host.h"
#include "chrome/browser/ash/arc/arc_ui_availability_reporter.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/auth/arc_auth_service.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_default_negotiator.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_oobe_negotiator.h"
#include "chrome/browser/ash/arc/policy/arc_android_management_checker.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/arc/session/arc_provisioning_result.h"
#include "chrome/browser/ash/login/demo_mode/demo_resources.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/policy/handlers/powerwash_requirements_checker.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/arc/arc_fast_app_reinstall_starter.h"
#include "chrome/browser/ui/app_list/arc/arc_pai_starter.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/webui/chromeos/diagnostics_dialog.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/system/statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/metrics/arc_metrics_service.h"
#include "components/arc/metrics/stability_metrics_manager.h"
#include "components/arc/session/arc_data_remover.h"
#include "components/arc/session/arc_instance_mode.h"
#include "components/arc/session/arc_management_transition.h"
#include "components/arc/session/arc_session.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/exo/wm_helper_chromeos.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "ui/display/types/display_constants.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ArcServiceLauncher.
ArcSessionManager* g_arc_session_manager = nullptr;

// Allows the session manager to skip creating UI in unit tests.
bool g_ui_enabled = true;

// Allows the session manager to create ArcTermsOfServiceOobeNegotiator in
// tests, even when the tests are set to skip creating UI.
bool g_enable_arc_terms_of_service_oobe_negotiator_in_tests = false;

absl::optional<bool> g_enable_check_android_management_in_tests;

constexpr const char kArcSaltPath[] = "/var/lib/misc/arc_salt";
constexpr const size_t kArcSaltFileSize = 16;

constexpr const char kArcPrepareHostGeneratedDirJobName[] =
    "arc_2dprepare_2dhost_2dgenerated_2ddir";

constexpr base::TimeDelta kWaitForPoliciesTimeout =
    base::TimeDelta::FromSeconds(20);

// Generates a unique, 20-character hex string from |chromeos_user| and
// |salt| which can be used as Android's ro.boot.serialno and ro.serialno
// properties. Note that Android treats serialno in a case-insensitive manner.
// |salt| cannot be the hex-encoded one.
// Note: The function must be the exact copy of the one in platform2/arc/setup/.
std::string GenerateFakeSerialNumber(const std::string& chromeos_user,
                                     const std::string& salt) {
  constexpr size_t kMaxHardwareIdLen = 20;
  const std::string hash(crypto::SHA256HashString(chromeos_user + salt));
  return base::HexEncode(hash.data(), hash.length())
      .substr(0, kMaxHardwareIdLen);
}

// Returns true if the hex-encoded salt in Local State is valid.
bool IsValidHexSalt(const std::string& hex_salt) {
  std::string salt;
  if (!base::HexStringToString(hex_salt, &salt)) {
    LOG(WARNING) << "Not a hex string: " << hex_salt;
    return false;
  }
  if (salt.size() != kArcSaltFileSize) {
    LOG(WARNING) << "Salt size invalid: " << salt.size();
    return false;
  }
  return true;
}

// Maximum amount of time we'll wait for ARC to finish booting up. Once this
// timeout expires, keep ARC running in case the user wants to file feedback,
// but present the UI to try again.
base::TimeDelta GetArcSignInTimeout() {
  constexpr base::TimeDelta kArcSignInTimeout = base::TimeDelta::FromMinutes(5);
  constexpr base::TimeDelta kArcVmSignInTimeoutForVM =
      base::TimeDelta::FromMinutes(20);

  if (chromeos::system::StatisticsProvider::GetInstance()->IsRunningOnVm() &&
      arc::IsArcVmEnabled()) {
    return kArcVmSignInTimeoutForVM;
  } else {
    return kArcSignInTimeout;
  }
}

// Updates UMA with user cancel only if error is not currently shown.
void MaybeUpdateOptInCancelUMA(const ArcSupportHost* support_host) {
  if (!support_host ||
      support_host->ui_page() == ArcSupportHost::UIPage::NO_PAGE ||
      support_host->ui_page() == ArcSupportHost::UIPage::ERROR) {
    return;
  }

  UpdateOptInCancelUMA(OptInCancelReason::USER_CANCEL);
}

// Returns true if launching the Play Store on OptIn succeeded is needed.
// Launch Play Store app, except for the following cases:
// * When Opt-in verification is disabled (for tests);
// * In case ARC is enabled from OOBE.
// * In ARC Kiosk mode, because the only one UI in kiosk mode must be the
//   kiosk app and device is not needed for opt-in;
// * In Public Session mode, because Play Store will be hidden from users
//   and only apps configured by policy should be installed.
// * When ARC is managed, and user does not go through OOBE opt-in,
//   because the whole OptIn flow should happen as seamless as possible for
//   the user.
// For Active Directory users we always show a page notifying them that they
// have to authenticate with their identity provider (through SAML) to make
// it less weird that a browser window pops up.
// Some tests require the Play Store to be shown and forces this using chromeos
// switch kArcForceShowPlayStoreApp.
bool ShouldLaunchPlayStoreApp(Profile* profile,
                              bool oobe_or_assistant_wizard_start) {
  if (!IsPlayStoreAvailable())
    return false;

  if (oobe_or_assistant_wizard_start)
    return false;

  if (ShouldShowOptInForTesting())
    return true;

  if (IsRobotOrOfflineDemoAccountMode())
    return false;

  if (IsArcOptInVerificationDisabled())
    return false;

  if (ShouldStartArcSilentlyForManagedProfile(profile))
    return false;

  return true;
}

// Defines the conditions that require UI to present eventual error conditions
// to the end user.
//
// Don't show UI for ARC Kiosk because the only one UI in kiosk mode must
// be the kiosk app. In case of error the UI will be useless as well, because
// in typical use case there will be no one nearby the kiosk device, who can
// do some action to solve the problem be means of UI.
// Same considerations apply for MGS sessions in Demo Mode.
// All other managed sessions will be attended by a user and require an error
// UI.
bool ShouldUseErrorDialog() {
  if (!g_ui_enabled)
    return false;

  if (IsArcOptInVerificationDisabled())
    return false;

  if (IsArcKioskMode())
    return false;

  if (ash::DemoSession::IsDeviceInDemoMode())
    return false;

  return true;
}

void ResetStabilityMetrics() {
  // TODO(shaochuan): Make this an event observable by StabilityMetricsManager
  // and eliminate this null check.
  auto* stability_metrics_manager = StabilityMetricsManager::Get();
  if (!stability_metrics_manager)
    return;
  stability_metrics_manager->ResetMetrics();
}

void SetArcEnabledStateMetric(bool enabled) {
  // TODO(shaochuan): Make this an event observable by StabilityMetricsManager
  // and eliminate this null check.
  auto* stability_metrics_manager = StabilityMetricsManager::Get();
  if (!stability_metrics_manager)
    return;
  stability_metrics_manager->SetArcEnabledState(enabled);
}

// Generates and returns a serial number from the salt in |local_state| and
// |chromeos_user|. When |local_state| does not have it (or has a corrupted
// one), this function creates a new random salt. When creates it, the function
// copies |arc_salt_on_disk| to |local_state| if |arc_salt_on_disk| is not
// empty.
std::string GetOrCreateSerialNumber(PrefService* local_state,
                                    const std::string& chromeos_user,
                                    const std::string& arc_salt_on_disk) {
  DCHECK(local_state);
  DCHECK(!chromeos_user.empty());

  std::string hex_salt = local_state->GetString(prefs::kArcSerialNumberSalt);
  if (hex_salt.empty() || !IsValidHexSalt(hex_salt)) {
    // This path is taken 1) on the very first ARC boot, 2) on the first boot
    // after powerwash, 3) on the first boot after upgrading to ARCVM, or 4)
    // when the salt in local state is corrupted.
    if (arc_salt_on_disk.empty()) {
      // The device doesn't have the salt file for ARC container. Create it from
      // scratch in the same way as ARC container.
      char rand_value[kArcSaltFileSize];
      crypto::RandBytes(rand_value, kArcSaltFileSize);
      hex_salt = base::HexEncode(rand_value, kArcSaltFileSize);
    } else {
      // The device has the one for container. Reuse it for ARCVM.
      DCHECK_EQ(kArcSaltFileSize, arc_salt_on_disk.size());
      hex_salt =
          base::HexEncode(arc_salt_on_disk.data(), arc_salt_on_disk.size());
    }
    local_state->SetString(prefs::kArcSerialNumberSalt, hex_salt);
  }

  // We store hex-encoded version of the salt in the local state, but to compute
  // the serial number, we use the decoded version to be compatible with the
  // arc-setup code for P.
  std::string decoded_salt;
  const bool result = base::HexStringToString(hex_salt, &decoded_salt);
  DCHECK(result) << hex_salt;
  return GenerateFakeSerialNumber(chromeos_user, decoded_salt);
}

// Reads a salt from |salt_path| and stores it in |out_salt|. Returns true
// when the file read is successful or the file does not exist.
bool ReadSaltOnDisk(const base::FilePath& salt_path, std::string* out_salt) {
  DCHECK(out_salt);
  if (!base::PathExists(salt_path)) {
    VLOG(2) << "ARC salt file doesn't exist: " << salt_path;
    return true;
  }
  if (!base::ReadFileToString(salt_path, out_salt)) {
    PLOG(ERROR) << "Failed to read " << salt_path;
    return false;
  }
  if (out_salt->size() != kArcSaltFileSize) {
    LOG(WARNING) << "Ignoring invalid ARC salt on disk. size="
                 << out_salt->size();
    out_salt->clear();
  }
  VLOG(1) << "Successfully read ARC salt on disk: " << salt_path;
  return true;
}

int GetSignInErrorCode(const arc::mojom::ArcSignInError* sign_in_error) {
  if (!sign_in_error)
    return 0;

#define IF_ERROR_RETURN_CODE(name, type)                          \
  if (sign_in_error->is_##name()) {                               \
    return static_cast<std::underlying_type_t<arc::mojom::type>>( \
        sign_in_error->get_##name());                             \
  }

  IF_ERROR_RETURN_CODE(cloud_provision_flow_error, CloudProvisionFlowError)
  IF_ERROR_RETURN_CODE(general_error, GeneralSignInError)
  IF_ERROR_RETURN_CODE(check_in_error, GMSCheckInError)
  IF_ERROR_RETURN_CODE(sign_in_error, GMSSignInError)
#undef IF_ERROR_RETURN_CODE

  LOG(ERROR) << "Unknown sign-in error "
             << std::underlying_type_t<arc::mojom::ArcSignInError::Tag>(
                    sign_in_error->which())
             << ".";

  return -1;
}

ArcSupportHost::Error GetCloudProvisionFlowError(
    mojom::CloudProvisionFlowError cloud_provision_flow_error) {
  switch (cloud_provision_flow_error) {
    case mojom::CloudProvisionFlowError::ERROR_ENROLLMENT_TOKEN_INVALID:
      return ArcSupportHost::Error::
          SIGN_IN_CLOUD_PROVISION_FLOW_ENROLLMENT_TOKEN_INVALID;

    case mojom::CloudProvisionFlowError::ERROR_DEVICE_QUOTA_EXCEEDED:
      return ArcSupportHost::Error::
          SIGN_IN_CLOUD_PROVISION_FLOW_DOMAIN_JOIN_FAIL_ERROR;

    case mojom::CloudProvisionFlowError::ERROR_NETWORK_UNAVAILABLE:
      return ArcSupportHost::Error::SIGN_IN_CLOUD_PROVISION_FLOW_NETWORK_ERROR;

    case mojom::CloudProvisionFlowError::ERROR_USER_CANCEL:
      return ArcSupportHost::Error::
          SIGN_IN_CLOUD_PROVISION_FLOW_INTERRUPTED_ERROR;

    case mojom::CloudProvisionFlowError::ERROR_NO_ACCOUNT_IN_WORK_PROFILE:
      return ArcSupportHost::Error::
          SIGN_IN_CLOUD_PROVISION_FLOW_ACCOUNT_MISSING_ERROR;

    case mojom::CloudProvisionFlowError::ERROR_ACCOUNT_NOT_READY:
    case mojom::CloudProvisionFlowError::ERROR_ACCOUNT_NOT_ALLOWLISTED:
    case mojom::CloudProvisionFlowError::ERROR_DPC_SUPPORT:
    case mojom::CloudProvisionFlowError::ERROR_ENTERPRISE_INVALID:
      return ArcSupportHost::Error::
          SIGN_IN_CLOUD_PROVISION_FLOW_PERMANENT_ERROR;

    case mojom::CloudProvisionFlowError::ERROR_ACCOUNT_OTHER:
    case mojom::CloudProvisionFlowError::ERROR_ADD_ACCOUNT_FAILED:
    case mojom::CloudProvisionFlowError::ERROR_CHECKIN_FAILED:
    case mojom::CloudProvisionFlowError::ERROR_INVALID_POLICY_STATE:
    case mojom::CloudProvisionFlowError::ERROR_INVALID_SETUP_ACTION:
    case mojom::CloudProvisionFlowError::ERROR_JSON:
    case mojom::CloudProvisionFlowError::ERROR_MANAGED_PROVISIONING_FAILED:
    case mojom::CloudProvisionFlowError::
        ERROR_OAUTH_TOKEN_AUTHENTICATOR_EXCEPTION:
    case mojom::CloudProvisionFlowError::ERROR_OAUTH_TOKEN_IO_EXCEPTION:
    case mojom::CloudProvisionFlowError::
        ERROR_OAUTH_TOKEN_OPERATION_CANCELED_EXCEPTION:
    case mojom::CloudProvisionFlowError::ERROR_OAUTH_TOKEN:
    case mojom::CloudProvisionFlowError::ERROR_OTHER:
    case mojom::CloudProvisionFlowError::ERROR_QUARANTINE:
    case mojom::CloudProvisionFlowError::ERROR_REMOVE_ACCOUNT_FAILED:
    case mojom::CloudProvisionFlowError::ERROR_REQUEST_ANDROID_ID_FAILED:
    case mojom::CloudProvisionFlowError::ERROR_SERVER_TRANSIENT_ERROR:
    case mojom::CloudProvisionFlowError::ERROR_SERVER:
    case mojom::CloudProvisionFlowError::ERROR_TIMEOUT:
    default:
      return ArcSupportHost::Error::
          SIGN_IN_CLOUD_PROVISION_FLOW_TRANSIENT_ERROR;
  }
}

ArcSupportHost::Error GetSupportHostError(const ArcProvisioningResult& result) {
  if (result.gms_sign_in_error() ==
      mojom::GMSSignInError::GMS_SIGN_IN_NETWORK_ERROR) {
    return ArcSupportHost::Error::SIGN_IN_NETWORK_ERROR;
  }

  if (result.gms_sign_in_error() ==
      mojom::GMSSignInError::GMS_SIGN_IN_BAD_AUTHENTICATION) {
    return ArcSupportHost::Error::SIGN_IN_BAD_AUTHENTICATION_ERROR;
  }

  if (result.gms_sign_in_error())
    return ArcSupportHost::Error::SIGN_IN_GMS_SIGNIN_ERROR;

  if (result.gms_check_in_error())
    return ArcSupportHost::Error::SIGN_IN_GMS_CHECKIN_ERROR;

  if (result.cloud_provision_flow_error()) {
    return GetCloudProvisionFlowError(
        result.cloud_provision_flow_error().value());
  }

  if (result.general_error() ==
      mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR) {
    return ArcSupportHost::Error::SERVER_COMMUNICATION_ERROR;
  }

  if (result.general_error() ==
      mojom::GeneralSignInError::NO_NETWORK_CONNECTION) {
    return ArcSupportHost::Error::NETWORK_UNAVAILABLE_ERROR;
  }

  if (result.general_error() == mojom::GeneralSignInError::ARC_DISABLED)
    return ArcSupportHost::Error::ANDROID_MANAGEMENT_REQUIRED_ERROR;

  if (result.stop_reason() == ArcStopReason::LOW_DISK_SPACE)
    return ArcSupportHost::Error::LOW_DISK_SPACE_ERROR;

  return ArcSupportHost::Error::SIGN_IN_UNKNOWN_ERROR;
}

bool ShouldShowNetworkTests(const ArcProvisioningResult& result) {
  if (!base::FeatureList::IsEnabled(
          ash::features::kButtonARCNetworkDiagnostics)) {
    return false;
  }

  // For GMS signin errors
  if (result.gms_sign_in_error() ==
          mojom::GMSSignInError::GMS_SIGN_IN_TIMEOUT ||
      result.gms_sign_in_error() ==
          mojom::GMSSignInError::GMS_SIGN_IN_SERVICE_UNAVAILABLE) {
    return true;
  }

  // For GMS checkin errors
  if (result.gms_check_in_error() ==
      mojom::GMSCheckInError::GMS_CHECK_IN_TIMEOUT) {
    return true;
  }

  // For Cloud Provision Flow errors
  if (result.cloud_provision_flow_error() ==
          mojom::CloudProvisionFlowError::ERROR_SERVER_TRANSIENT_ERROR ||
      result.cloud_provision_flow_error() ==
          mojom::CloudProvisionFlowError::ERROR_TIMEOUT ||
      result.cloud_provision_flow_error() ==
          mojom::CloudProvisionFlowError::ERROR_NETWORK_UNAVAILABLE ||
      result.cloud_provision_flow_error() ==
          mojom::CloudProvisionFlowError::ERROR_SERVER) {
    return true;
  }

  // For General signin errors
  if (result.general_error() ==
          mojom::GeneralSignInError::GENERIC_PROVISIONING_TIMEOUT ||
      result.general_error() ==
          mojom::GeneralSignInError::NO_NETWORK_CONNECTION ||
      result.general_error() ==
          mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR) {
    return true;
  }
  return false;
}

ArcSessionManager::ExpansionResult ReadSaltInternal() {
  DCHECK(arc::IsArcVmEnabled());

  // For ARCVM, read |kArcSaltPath| if that exists.
  std::string salt;
  if (!ReadSaltOnDisk(base::FilePath(kArcSaltPath), &salt))
    return ArcSessionManager::ExpansionResult{{}, false};
  return ArcSessionManager::ExpansionResult{salt, true};
}

}  // namespace

// This class is used to track statuses on OptIn flow. It is created in case ARC
// is activated, and it needs to OptIn. Once started OptInFlowResult::STARTED is
// recorded via UMA. If it finishes successfully OptInFlowResult::SUCCEEDED is
// recorded. Optional OptInFlowResult::SUCCEEDED_AFTER_RETRY is recorded in this
// case if an error occurred during OptIn flow, and user pressed Retry. In case
// the user cancels OptIn flow before it was completed then
// OptInFlowResult::CANCELED is recorded and if an error occurred optional
// OptInFlowResult::CANCELED_AFTER_ERROR. If a shutdown happens during the OptIn
// nothing is recorded, except initial OptInFlowResult::STARTED.
// OptInFlowResult::STARTED = OptInFlowResult::SUCCEEDED +
// OptInFlowResult::CANCELED + cases happened during the shutdown.
class ArcSessionManager::ScopedOptInFlowTracker {
 public:
  ScopedOptInFlowTracker() {
    UpdateOptInFlowResultUMA(OptInFlowResult::STARTED);
  }

  ~ScopedOptInFlowTracker() {
    if (shutdown_)
      return;

    UpdateOptInFlowResultUMA(success_ ? OptInFlowResult::SUCCEEDED
                                      : OptInFlowResult::CANCELED);
    if (error_) {
      UpdateOptInFlowResultUMA(success_
                                   ? OptInFlowResult::SUCCEEDED_AFTER_RETRY
                                   : OptInFlowResult::CANCELED_AFTER_ERROR);
    }
  }

  // Tracks error occurred during the OptIn flow.
  void TrackError() {
    DCHECK(!success_ && !shutdown_);
    error_ = true;
  }

  // Tracks that OptIn finished successfully.
  void TrackSuccess() {
    DCHECK(!success_ && !shutdown_);
    success_ = true;
  }

  // Tracks that OptIn was not completed before shutdown.
  void TrackShutdown() {
    DCHECK(!success_ && !shutdown_);
    shutdown_ = true;
  }

 private:
  bool error_ = false;
  bool success_ = false;
  bool shutdown_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedOptInFlowTracker);
};

ArcSessionManager::ArcSessionManager(
    std::unique_ptr<ArcSessionRunner> arc_session_runner,
    std::unique_ptr<AdbSideloadingAvailabilityDelegateImpl>
        adb_sideloading_availability_delegate)
    : arc_session_runner_(std::move(arc_session_runner)),
      adb_sideloading_availability_delegate_(
          std::move(adb_sideloading_availability_delegate)),
      attempt_user_exit_callback_(
          base::BindRepeating(chrome::AttemptUserExit)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!g_arc_session_manager);
  g_arc_session_manager = this;
  arc_session_runner_->AddObserver(this);
  arc_session_runner_->SetDemoModeDelegate(
      std::make_unique<ArcDemoModeDelegateImpl>());
  if (chromeos::SessionManagerClient::Get())
    chromeos::SessionManagerClient::Get()->AddObserver(this);
  ResetStabilityMetrics();
  chromeos::ConciergeClient::Get()->AddVmObserver(this);
}

ArcSessionManager::~ArcSessionManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  chromeos::ConciergeClient::Get()->RemoveVmObserver(this);

  if (chromeos::SessionManagerClient::Get())
    chromeos::SessionManagerClient::Get()->RemoveObserver(this);

  Shutdown();
  arc_session_runner_->RemoveObserver(this);

  DCHECK_EQ(this, g_arc_session_manager);
  g_arc_session_manager = nullptr;
}

// static
ArcSessionManager* ArcSessionManager::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_arc_session_manager;
}

// static
void ArcSessionManager::SetUiEnabledForTesting(bool enable) {
  g_ui_enabled = enable;
}

// static
void ArcSessionManager::SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(
    bool enable) {
  g_enable_arc_terms_of_service_oobe_negotiator_in_tests = enable;
}

// static
void ArcSessionManager::EnableCheckAndroidManagementForTesting(bool enable) {
  g_enable_check_android_management_in_tests = enable;
}

// static
std::string ArcSessionManager::GenerateFakeSerialNumberForTesting(
    const std::string& chromeos_user,
    const std::string& salt) {
  return GenerateFakeSerialNumber(chromeos_user, salt);
}

// static
std::string ArcSessionManager::GetOrCreateSerialNumberForTesting(
    PrefService* local_state,
    const std::string& chromeos_user,
    const std::string& arc_salt_on_disk) {
  return GetOrCreateSerialNumber(local_state, chromeos_user, arc_salt_on_disk);
}

// static
bool ArcSessionManager::ReadSaltOnDiskForTesting(
    const base::FilePath& salt_path,
    std::string* out_salt) {
  return ReadSaltOnDisk(salt_path, out_salt);
}

void ArcSessionManager::OnSessionStopped(ArcStopReason reason,
                                         bool restarting) {
  if (restarting) {
    DCHECK_EQ(state_, State::ACTIVE);
    // If ARC is being restarted, here do nothing, and just wait for its
    // next run.
    return;
  }

  DCHECK(state_ == State::ACTIVE || state_ == State::STOPPING) << state_;
  state_ = State::STOPPED;

  if (arc_sign_in_timer_.IsRunning())
    OnProvisioningFinished(ArcProvisioningResult(reason));

  for (auto& observer : observer_list_)
    observer.OnArcSessionStopped(reason);

  MaybeStartArcDataRemoval();
}

void ArcSessionManager::OnSessionRestarting() {
  for (auto& observer : observer_list_)
    observer.OnArcSessionRestarting();
}

void ArcSessionManager::OnProvisioningFinished(
    const ArcProvisioningResult& result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the Mojo message to notify finishing the provisioning is already sent
  // from the container, it will be processed even after requesting to stop the
  // container. Ignore all |result|s arriving while ARC is disabled, in order to
  // avoid popping up an error message triggered below. This code intentionally
  // does not support the case of re-enabling.
  if (!enable_requested_) {
    LOG(WARNING) << "Provisioning result received after ARC was disabled. "
                 << "Ignoring result " << result << ".";
    return;
  }

  // Due asynchronous nature of stopping the ARC instance,
  // OnProvisioningFinished may arrive after setting the |State::STOPPED| state
  // and |State::Active| is not guaranteed to be set here.
  // prefs::kArcDataRemoveRequested also can be active for now.

  const bool provisioning_successful = result.is_success();
  if (provisioning_reported_) {
    // We don't expect success ArcProvisnioningResult to be reported twice
    // or reported after an error.
    DCHECK(!provisioning_successful);
    // TODO(khmel): Consider changing LOG to NOTREACHED once we guaranty that
    // no double message can happen in production.
    LOG(WARNING) << "Provisioning result was already reported. Ignoring "
                 << "additional result " << result << ".";
    return;
  }
  provisioning_reported_ = true;
  if (scoped_opt_in_tracker_ && !provisioning_successful)
    scoped_opt_in_tracker_->TrackError();

  if (result.general_error() ==
      mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR) {
    // TODO(poromov): Consider ARC PublicSession offline mode.
    // Currently ARC session will be exited below, while the main user session
    // will be kept alive without Android apps.
    if (IsRobotOrOfflineDemoAccountMode())
      VLOG(1) << "Robot account auth code fetching error";
    if (IsArcKioskMode()) {
      VLOG(1) << "Exiting kiosk session due to provisioning failure";
      // Log out the user. All the cleanup will be done in Shutdown() method.
      // The callback is not called because auth code is empty.
      attempt_user_exit_callback_.Run();
      return;
    }

    // For backwards compatibility, use NETWORK_ERROR for
    // CHROME_SERVER_COMMUNICATION_ERROR case.
    UpdateOptInCancelUMA(OptInCancelReason::NETWORK_ERROR);
  } else if (!sign_in_start_time_.is_null()) {
    arc_sign_in_timer_.Stop();

    UpdateProvisioningTiming(base::TimeTicks::Now() - sign_in_start_time_,
                             provisioning_successful, profile_);
    UpdateProvisioningStatusUMA(GetProvisioningStatus(result), profile_);

    if (result.gms_sign_in_error()) {
      UpdateGMSSignInErrorUMA(result.gms_sign_in_error().value(), profile_);
    } else if (result.gms_check_in_error()) {
      UpdateGMSCheckInErrorUMA(result.gms_check_in_error().value(), profile_);
    } else if (result.cloud_provision_flow_error()) {
      UpdateCloudProvisionFlowErrorUMA(
          result.cloud_provision_flow_error().value(), profile_);
    }

    if (!provisioning_successful)
      UpdateOptInCancelUMA(OptInCancelReason::PROVISIONING_FAILED);
  }

  if (provisioning_successful) {
    if (support_host_)
      support_host_->Close();

    if (scoped_opt_in_tracker_) {
      scoped_opt_in_tracker_->TrackSuccess();
      scoped_opt_in_tracker_.reset();
    }

    PrefService* const prefs = profile_->GetPrefs();

    prefs->SetBoolean(prefs::kArcIsManaged,
                      policy_util::IsAccountManaged(profile_));

    if (prefs->GetBoolean(prefs::kArcSignedIn))
      return;

    prefs->SetBoolean(prefs::kArcSignedIn, true);

    if (ShouldLaunchPlayStoreApp(
            profile_,
            prefs->GetBoolean(prefs::kArcProvisioningInitiatedFromOobe))) {
      playstore_launcher_ = std::make_unique<ArcAppLauncher>(
          profile_, kPlayStoreAppId,
          apps_util::CreateIntentForActivity(
              kPlayStoreActivity, kInitialStartParam, kCategoryLauncher),
          false /* deferred_launch_allowed */, display::kInvalidDisplayId,
          apps::mojom::LaunchSource::kFromChromeInternal);
    }

    prefs->ClearPref(prefs::kArcProvisioningInitiatedFromOobe);

    for (auto& observer : observer_list_)
      observer.OnArcInitialStart();
    return;
  }

  VLOG(1) << "ARC provisioning failed: " << result << ".";

  // When ARC provisioning fails due to Chrome failing to talk to server, we
  // don't need to keep the ARC session running as the logs necessary to
  // investigate are already present. ARC session will not provide any useful
  // context.
  if (result.stop_reason() ||
      result.general_error() ==
          mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR) {
    if (profile_->GetPrefs()->HasPrefPath(prefs::kArcSignedIn))
      profile_->GetPrefs()->SetBoolean(prefs::kArcSignedIn, false);
    VLOG(1) << "Stopping ARC due to provisioning failure";
    ShutdownSession();
  }

  if (result.cloud_provision_flow_error() ||
      // OVERALL_SIGN_IN_TIMEOUT might be an indication that ARC believes it is
      // fully setup, but Chrome does not.
      result.is_timedout() ||
      // Just to be safe, remove data if we don't know the cause.
      result.general_error() == mojom::GeneralSignInError::UNKNOWN_ERROR) {
    VLOG(1) << "ARC provisioning failed permanently. Removing user data";
    RequestArcDataRemoval();
  }

  absl::optional<int> error_code;
  ArcSupportHost::Error support_error = GetSupportHostError(result);
  if (support_error == ArcSupportHost::Error::SIGN_IN_UNKNOWN_ERROR) {
    error_code = static_cast<std::underlying_type_t<ProvisioningStatus>>(
        GetProvisioningStatus(result));
  } else if (result.sign_in_error()) {
    error_code = GetSignInErrorCode(result.sign_in_error());
  }

  ShowArcSupportHostError({support_error, error_code} /* error_info */,
                          true /* should_show_send_feedback */,
                          ShouldShowNetworkTests(result));
}

bool ArcSessionManager::IsAllowed() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return profile_ != nullptr;
}

void ArcSessionManager::SetProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!profile_);
  DCHECK(IsArcAllowedForProfile(profile));
  profile_ = profile;
  adb_sideloading_availability_delegate_->SetProfile(profile);
  // RequestEnable() requires |profile_| set, therefore shouldn't have been
  // called at this point.
  SetArcEnabledStateMetric(false);
}

void ArcSessionManager::SetUserInfo() {
  DCHECK(profile_);

  const AccountId account(multi_user_util::GetAccountIdFromProfile(profile_));
  const cryptohome::Identification cryptohome_id(account);
  const std::string user_id_hash =
      chromeos::ProfileHelper::GetUserIdHashFromProfile(profile_);

  std::string serialno = GetSerialNumber();
  arc_session_runner_->SetUserInfo(cryptohome_id, user_id_hash, serialno);
}

void ArcSessionManager::TrimVmMemory(TrimVmMemoryCallback callback) {
  arc_session_runner_->TrimVmMemory(std::move(callback));
}

std::string ArcSessionManager::GetSerialNumber() const {
  DCHECK(profile_);
  DCHECK(arc_salt_on_disk_);

  const AccountId account(multi_user_util::GetAccountIdFromProfile(profile_));
  const std::string user_id_hash =
      chromeos::ProfileHelper::GetUserIdHashFromProfile(profile_);

  std::string serialno;
  // ARC container doesn't need the serial number.
  if (arc::IsArcVmEnabled()) {
    const std::string chromeos_user =
        cryptohome::CreateAccountIdentifierFromAccountId(account).account_id();
    serialno = GetOrCreateSerialNumber(g_browser_process->local_state(),
                                       chromeos_user, *arc_salt_on_disk_);
  }
  return serialno;
}

void ArcSessionManager::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  DCHECK_EQ(state_, State::NOT_INITIALIZED);
  state_ = State::STOPPED;

  // If ExpandPropertyFilesAndReadSaltInternal() takes time to finish,
  // Initialize() may be called before it finishes. In that case,
  // SetUserInfo() is called in OnExpandPropertyFilesAndReadSalt().
  if (arc_salt_on_disk_) {
    VLOG(1) << "Calling SetUserInfo() in ArcSessionManager::Initialize";
    SetUserInfo();
  }

  // Create the support host at initialization. Note that, practically,
  // ARC support Chrome app is rarely used (only opt-in and re-auth flow).
  // So, it may be better to initialize it lazily.
  // TODO(hidehiko): Revisit to think about lazy initialization.
  if (ShouldUseErrorDialog()) {
    DCHECK(!support_host_);
    support_host_ = std::make_unique<ArcSupportHost>(profile_);
    support_host_->SetErrorDelegate(this);
  }
  auto* prefs = profile_->GetPrefs();
  const cryptohome::Identification cryptohome_id(
      multi_user_util::GetAccountIdFromProfile(profile_));
  data_remover_ = std::make_unique<ArcDataRemover>(prefs, cryptohome_id);

  if (g_enable_check_android_management_in_tests.value_or(g_ui_enabled))
    ArcAndroidManagementChecker::StartClient();

  // Chrome may be shut down before completing ARC data removal.
  // For such a case, start removing the data now, if necessary.
  MaybeStartArcDataRemoval();
}

void ArcSessionManager::Shutdown() {
  VLOG(1) << "Shutting down session manager";
  enable_requested_ = false;
  ResetArcState();
  arc_session_runner_->OnShutdown();
  data_remover_.reset();
  if (support_host_) {
    support_host_->SetErrorDelegate(nullptr);
    support_host_->Close();
    support_host_.reset();
  }
  pai_starter_.reset();
  fast_app_reinstall_starter_.reset();
  arc_ui_availability_reporter_.reset();
  profile_ = nullptr;
  state_ = State::NOT_INITIALIZED;
  if (scoped_opt_in_tracker_) {
    scoped_opt_in_tracker_->TrackShutdown();
    scoped_opt_in_tracker_.reset();
  }
}

void ArcSessionManager::ShutdownSession() {
  ResetArcState();
  switch (state_) {
    case State::NOT_INITIALIZED:
      // Ignore in NOT_INITIALIZED case. This is called in initial SetProfile
      // invocation.
      // TODO(hidehiko): Remove this along with the clean up.
    case State::STOPPED:
      // Currently, ARC is stopped. Do nothing.
    case State::REMOVING_DATA_DIR:
      // When data removing is done, |state_| will be set to STOPPED.
      // Do nothing here.
    case State::STOPPING:
      // Now ARC is stopping. Do nothing here.
      VLOG(1) << "Skipping session shutdown because state is: " << state_;
      break;
    case State::NEGOTIATING_TERMS_OF_SERVICE:
    case State::CHECKING_ANDROID_MANAGEMENT:
      // We need to kill the mini-container that might be running here.
      arc_session_runner_->RequestStop();
      // While RequestStop is asynchronous, ArcSessionManager is agnostic to the
      // state of the mini-container, so we can set it's state_ to STOPPED
      // immediately.
      state_ = State::STOPPED;
      break;

      break;
    case State::ACTIVE:
      // Request to stop the ARC. |state_| will be set to STOPPED eventually.
      // Set state before requesting the runner to stop in order to prevent the
      // case when |OnSessionStopped| can be called inline and as result
      // |state_| might be changed.
      state_ = State::STOPPING;
      arc_session_runner_->RequestStop();
      break;
  }
}

void ArcSessionManager::ResetArcState() {
  pre_start_time_ = base::TimeTicks();
  start_time_ = base::TimeTicks();
  arc_sign_in_timer_.Stop();
  playstore_launcher_.reset();
  terms_of_service_negotiator_.reset();
  android_management_checker_.reset();
  wait_for_policy_timer_.AbandonAndStop();
}

void ArcSessionManager::AddObserver(ArcSessionManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.AddObserver(observer);
  if (property_files_expansion_result_)
    observer->OnPropertyFilesExpanded(*property_files_expansion_result_);
}

void ArcSessionManager::RemoveObserver(ArcSessionManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

void ArcSessionManager::NotifyArcPlayStoreEnabledChanged(bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& observer : observer_list_)
    observer.OnArcPlayStoreEnabledChanged(enabled);
}

// This is the special method to support enterprise mojo API.
// TODO(hidehiko): Remove this.
void ArcSessionManager::StopAndEnableArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  reenable_arc_ = true;
  StopArc();
}

void ArcSessionManager::OnArcSignInTimeout() {
  LOG(ERROR) << "Timed out waiting for first sign in.";
  OnProvisioningFinished(ArcProvisioningResult(ChromeProvisioningTimeout()));
}

void ArcSessionManager::CancelAuthCode() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state_ == State::NOT_INITIALIZED) {
    NOTREACHED();
    return;
  }

  // If ARC failed to boot normally, stop ARC. Similarly, if the current page is
  // ACTIVE_DIRECTORY_AUTH, closing the window should stop ARC since the user
  // chooses to not sign in. In any other case, ARC is booting normally and
  // the instance should not be stopped.
  if ((state_ != State::NEGOTIATING_TERMS_OF_SERVICE &&
       state_ != State::CHECKING_ANDROID_MANAGEMENT) &&
      (!support_host_ ||
       (support_host_->ui_page() != ArcSupportHost::UIPage::ERROR &&
        support_host_->ui_page() !=
            ArcSupportHost::UIPage::ACTIVE_DIRECTORY_AUTH))) {
    return;
  }

  MaybeUpdateOptInCancelUMA(support_host_.get());
  VLOG(1) << "Auth cancelled. Stopping ARC. state: " << state_;
  StopArc();
  SetArcPlayStoreEnabledForProfile(profile_, false);
}

void ArcSessionManager::RequestEnable() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  if (enable_requested_) {
    VLOG(1) << "ARC is already enabled. Do nothing.";
    return;
  }
  enable_requested_ = true;
  SetArcEnabledStateMetric(true);

  VLOG(1) << "ARC opt-in. Starting ARC session.";

  // |directly_started_| flag must be preserved during the internal ARC restart.
  // So set it only when ARC is externally requested to start.
  directly_started_ = RequestEnableImpl();
}

bool ArcSessionManager::IsPlaystoreLaunchRequestedForTesting() const {
  return playstore_launcher_.get();
}

void ArcSessionManager::OnBackgroundAndroidManagementCheckedForTesting(
    policy::AndroidManagementClient::Result result) {
  OnBackgroundAndroidManagementChecked(result);
}

void ArcSessionManager::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& vm_signal) {
  // When an ARCVM starts, store the vm info.
  if (vm_signal.name() == kArcVmName)
    vm_info_ = vm_signal.vm_info();
}

void ArcSessionManager::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& vm_signal) {
  // When an ARCVM stops, clear the stored vm info.
  if (vm_signal.name() == kArcVmName)
    vm_info_ = absl::nullopt;
}

const absl::optional<vm_tools::concierge::VmInfo>&
ArcSessionManager::GetVmInfo() const {
  return vm_info_;
}

bool ArcSessionManager::RequestEnableImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(enable_requested_);
  DCHECK(state_ == State::STOPPED || state_ == State::STOPPING ||
         state_ == State::REMOVING_DATA_DIR)
      << state_;

  if (state_ != State::STOPPED) {
    // If the previous invocation of ARC is still running (but currently being
    // stopped) or ARC data removal is in progress, postpone the enabling
    // procedure.
    reenable_arc_ = true;
    return false;
  }

  PrefService* const prefs = profile_->GetPrefs();

  // |prefs::kArcProvisioningInitiatedFromOobe| is used to remember
  // |IsArcOobeOptInActive| or |IsArcOptInWizardForAssistantActive| state when
  // ARC start request was made initially. |IsArcOobeOptInActive| or
  // |IsArcOptInWizardForAssistantActive| will be changed by the time when
  // decision to auto-launch the Play Store would be made.
  // |IsArcOobeOptInActive| and |IsArcOptInWizardForAssistantActive| are not
  // preserved on Chrome restart also and in last case
  // |prefs::kArcProvisioningInitiatedFromOobe| is used to remember the state of
  // the initial request.
  // |prefs::kArcProvisioningInitiatedFromOobe| is reset when provisioning is
  // done or ARC is opted out.
  const bool opt_in_start = IsArcOobeOptInActive();
  const bool signed_in = prefs->GetBoolean(prefs::kArcSignedIn);
  if (opt_in_start)
    prefs->SetBoolean(prefs::kArcProvisioningInitiatedFromOobe, true);

  // If it is marked that sign in has been successfully done or if Play Store is
  // not available, then directly start ARC with skipping Play Store ToS.
  // For Kiosk mode, skip ToS because it is very likely that near the device
  // there will be no one who is eligible to accept them.
  // In Public Session mode ARC should be started silently without user
  // interaction. If opt-in verification is disabled, skip negotiation, too.
  // This is for testing purpose.
  const bool start_arc_directly = signed_in || ShouldArcAlwaysStart() ||
                                  IsRobotOrOfflineDemoAccountMode() ||
                                  IsArcOptInVerificationDisabled();
  // When ARC is blocked because of filesystem compatibility, do not proceed
  // to starting ARC nor follow further state transitions.
  if (IsArcBlockedDueToIncompatibleFileSystem(profile_)) {
    // If the next step was the ToS negotiation, show a notification instead.
    // Otherwise, be silent now. Users are notified when clicking ARC app icons.
    if (!start_arc_directly && g_ui_enabled)
      arc::ShowArcMigrationGuideNotification(profile_);
    return false;
  }

  // When ARC is blocked because of powerwash request, do not proceed
  // to starting ARC nor follow further state transitions. Applications are
  // still visible but replaced with notification requesting powerwash.
  policy::PowerwashRequirementsChecker pw_checker(
      policy::PowerwashRequirementsChecker::Context::kArc, profile_);
  if (pw_checker.GetState() !=
      policy::PowerwashRequirementsChecker::State::kNotRequired) {
    return false;
  }

  // ARC might be re-enabled and in this case |arc_ui_availability_reporter_| is
  // already set.
  if (!arc_ui_availability_reporter_) {
    arc_ui_availability_reporter_ = std::make_unique<ArcUiAvailabilityReporter>(
        profile_,
        opt_in_start ? ArcUiAvailabilityReporter::Mode::kOobeProvisioning
        : signed_in  ? ArcUiAvailabilityReporter::Mode::kAlreadyProvisioned
                     : ArcUiAvailabilityReporter::Mode::kInSessionProvisioning);
  }

  if (!pai_starter_ && IsPlayStoreAvailable())
    pai_starter_ = ArcPaiStarter::CreateIfNeeded(profile_);

  if (!fast_app_reinstall_starter_ && IsPlayStoreAvailable()) {
    fast_app_reinstall_starter_ = ArcFastAppReinstallStarter::CreateIfNeeded(
        profile_, profile_->GetPrefs());
  }

  if (start_arc_directly) {
    StartArc();
    // When in ARC kiosk mode, there's no Chrome tabs to restore. Remove the
    // cgroups now.
    if (IsArcKioskMode())
      SetArcCpuRestriction(CpuRestrictionState::CPU_RESTRICTION_FOREGROUND);
    // Check Android management in parallel.
    // Note: StartBackgroundAndroidManagementCheck() may call
    // OnBackgroundAndroidManagementChecked() synchronously (or
    // asynchronously). In the callback, Google Play Store enabled preference
    // can be set to false if managed, and it triggers RequestDisable() via
    // ArcPlayStoreEnabledPreferenceHandler.
    // Thus, StartArc() should be called so that disabling should work even
    // if synchronous call case.
    StartBackgroundAndroidManagementCheck();
    return true;
  }

  MaybeStartTermsOfServiceNegotiation();
  return false;
}

void ArcSessionManager::RequestDisable() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  if (!enable_requested_) {
    VLOG(1) << "ARC is already disabled. "
            << "Killing an instance for login screen (if any).";
    arc_session_runner_->RequestStop();
    return;
  }

  VLOG(1) << "Disabling ARC.";

  directly_started_ = false;
  enable_requested_ = false;
  SetArcEnabledStateMetric(false);
  scoped_opt_in_tracker_.reset();
  pai_starter_.reset();
  fast_app_reinstall_starter_.reset();
  arc_ui_availability_reporter_.reset();

  // Reset any pending request to re-enable ARC.
  reenable_arc_ = false;
  StopArc();
}

void ArcSessionManager::RequestArcDataRemoval() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(data_remover_);
  VLOG(1) << "Scheduling ARC data removal.";

  // TODO(hidehiko): DCHECK the previous state. This is called for four cases;
  // 1) Supporting managed user initial disabled case (Please see also
  //    ArcPlayStoreEnabledPreferenceHandler::Start() for details).
  // 2) Supporting enterprise triggered data removal.
  // 3) One called in OnProvisioningFinished().
  // 4) On request disabling.
  // After the state machine is fixed, 2) should be replaced by
  // RequestDisable() immediately followed by RequestEnable().
  // 3) and 4) are internal state transition. So, as for public interface, 1)
  // should be the only use case, and the |state_| should be limited to
  // STOPPED, then.
  // TODO(hidehiko): Think a way to get rid of 1), too.

  data_remover_->Schedule();
  profile_->GetPrefs()->SetInteger(
      prefs::kArcManagementTransition,
      static_cast<int>(ArcManagementTransition::NO_TRANSITION));
  // To support 1) case above, maybe start data removal.
  if (state_ == State::STOPPED)
    MaybeStartArcDataRemoval();
}

void ArcSessionManager::MaybeStartTermsOfServiceNegotiation() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(!terms_of_service_negotiator_);
  // In Kiosk and Public Session mode, Terms of Service negotiation should be
  // skipped. See also RequestEnableImpl().
  DCHECK(!IsRobotOrOfflineDemoAccountMode());
  // If opt-in verification is disabled, Terms of Service negotiation should
  // be skipped, too. See also RequestEnableImpl().
  DCHECK(!IsArcOptInVerificationDisabled());

  DCHECK_EQ(state_, State::STOPPED);
  state_ = State::NEGOTIATING_TERMS_OF_SERVICE;

  // TODO(hidehiko): In kArcSignedIn = true case, this method should never
  // be called. Remove the check.
  // Conceptually, this is starting ToS negotiation, rather than opt-in flow.
  // Move to RequestEnabledImpl.
  if (!scoped_opt_in_tracker_ &&
      !profile_->GetPrefs()->GetBoolean(prefs::kArcSignedIn)) {
    scoped_opt_in_tracker_ = std::make_unique<ScopedOptInFlowTracker>();
  }

  if (!IsArcTermsOfServiceNegotiationNeeded(profile_)) {
    if (IsArcStatsReportingEnabled() &&
        !profile_->GetPrefs()->GetBoolean(prefs::kArcTermsAccepted)) {
      // Don't enable stats reporting for users who are not shown the reporting
      // notice during ARC setup.
      profile_->GetPrefs()->SetBoolean(prefs::kArcSkippedReportingNotice, true);
    }

    // Moves to next state, Android management check, immediately, as if
    // Terms of Service negotiation is done successfully.
    StartAndroidManagementCheck();
    return;
  }

  if (IsArcOobeOptInActive()) {
    if (g_enable_arc_terms_of_service_oobe_negotiator_in_tests ||
        g_ui_enabled) {
      VLOG(1) << "Use OOBE negotiator.";
      terms_of_service_negotiator_ =
          std::make_unique<ArcTermsOfServiceOobeNegotiator>();
    }
  } else if (support_host_) {
    VLOG(1) << "Use default negotiator.";
    terms_of_service_negotiator_ =
        std::make_unique<ArcTermsOfServiceDefaultNegotiator>(
            profile_->GetPrefs(), support_host_.get());
  }

  // Start the mini-container here to save time starting the container if the
  // user decides to opt-in.
  StartMiniArc();

  if (!terms_of_service_negotiator_) {
    // The only case reached here is when g_ui_enabled is false so
    // 1. ARC support host is not created in SetProfile(), and
    // 2. ArcTermsOfServiceOobeNegotiator is not created with OOBE test setup
    // unless test explicitly called
    // SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(true).
    if (IsArcOobeOptInActive()) {
      DCHECK(!g_enable_arc_terms_of_service_oobe_negotiator_in_tests &&
             !g_ui_enabled)
          << "OOBE negotiator is not created on production.";
    } else {
      DCHECK(!g_ui_enabled) << "Negotiator is not created on production.";
    }
    return;
  }

  terms_of_service_negotiator_->StartNegotiation(
      base::BindOnce(&ArcSessionManager::OnTermsOfServiceNegotiated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::OnTermsOfServiceNegotiated(bool accepted) {
  DCHECK_EQ(state_, State::NEGOTIATING_TERMS_OF_SERVICE);
  DCHECK(profile_);
  DCHECK(terms_of_service_negotiator_ || !g_ui_enabled);
  terms_of_service_negotiator_.reset();

  if (!accepted) {
    VLOG(1) << "Terms of services declined";
    // User does not accept the Terms of Service. Disable Google Play Store.
    MaybeUpdateOptInCancelUMA(support_host_.get());
    SetArcPlayStoreEnabledForProfile(profile_, false);
    return;
  }

  // Terms were accepted.
  VLOG(1) << "Terms of services accepted";
  profile_->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
  StartAndroidManagementCheck();
}

void ArcSessionManager::StartAndroidManagementCheck() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // State::STOPPED appears here in following scenario.
  // Initial provisioning finished with state
  // ProvisioningStatus::ArcStop or
  // ProvisioningStatus::CHROME_SERVER_COMMUNICATION_ERROR.
  // At this moment |prefs::kArcTermsAccepted| is set to true, once user
  // confirmed ToS prior to provisioning flow. Once user presses "Try Again"
  // button, OnRetryClicked calls this immediately.
  DCHECK(state_ == State::NEGOTIATING_TERMS_OF_SERVICE ||
         state_ == State::CHECKING_ANDROID_MANAGEMENT ||
         state_ == State::STOPPED)
      << state_;
  state_ = State::CHECKING_ANDROID_MANAGEMENT;

  // Show loading UI only if ARC support app's window is already shown.
  // User may not see any ARC support UI if everything needed is done in
  // background. In such a case, showing loading UI here (then closed sometime
  // soon later) would look just noisy.
  if (support_host_ &&
      support_host_->ui_page() != ArcSupportHost::UIPage::NO_PAGE) {
    support_host_->ShowArcLoading();
  }

  for (auto& observer : observer_list_)
    observer.OnArcOptInManagementCheckStarted();

  if (!g_ui_enabled)
    return;

  android_management_checker_ = std::make_unique<ArcAndroidManagementChecker>(
      profile_, false /* retry_on_error */);
  android_management_checker_->StartCheck(
      base::BindOnce(&ArcSessionManager::OnAndroidManagementChecked,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::OnAndroidManagementChecked(
    policy::AndroidManagementClient::Result result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::CHECKING_ANDROID_MANAGEMENT);
  DCHECK(android_management_checker_);
  android_management_checker_.reset();

  switch (result) {
    case policy::AndroidManagementClient::Result::UNMANAGED:
      VLOG(1) << "Starting ARC for first sign in.";
      StartArc();
      // Since opt-in is an explicit user (or admin) action, relax the
      // cgroups restriction now.
      SetArcCpuRestriction(CpuRestrictionState::CPU_RESTRICTION_FOREGROUND);
      break;
    case policy::AndroidManagementClient::Result::MANAGED:
      ShowArcSupportHostError(
          ArcSupportHost::ErrorInfo(
              ArcSupportHost::Error::ANDROID_MANAGEMENT_REQUIRED_ERROR),
          false /* should_show_send_feedback */,
          false /* should_show_run_network_tests */);
      UpdateOptInCancelUMA(OptInCancelReason::ANDROID_MANAGEMENT_REQUIRED);
      break;
    case policy::AndroidManagementClient::Result::ERROR:
      ShowArcSupportHostError(
          ArcSupportHost::ErrorInfo(
              ArcSupportHost::Error::SERVER_COMMUNICATION_ERROR),
          true /* should_show_send_feedback */,
          true /* should_show_run_network_tests */);
      UpdateOptInCancelUMA(OptInCancelReason::NETWORK_ERROR);
      break;
  }
}

void ArcSessionManager::StartBackgroundAndroidManagementCheck() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::ACTIVE);
  DCHECK(!android_management_checker_);

  // Skip Android management check for testing.
  // We also skip if Android management check for Kiosk and Public Session mode,
  // because there are no managed human users for them exist.
  if (IsArcOptInVerificationDisabled() || IsRobotOrOfflineDemoAccountMode() ||
      (!g_ui_enabled &&
       !g_enable_check_android_management_in_tests.value_or(false))) {
    return;
  }

  android_management_checker_ = std::make_unique<ArcAndroidManagementChecker>(
      profile_, true /* retry_on_error */);
  android_management_checker_->StartCheck(
      base::BindOnce(&ArcSessionManager::OnBackgroundAndroidManagementChecked,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::OnBackgroundAndroidManagementChecked(
    policy::AndroidManagementClient::Result result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (g_enable_check_android_management_in_tests.value_or(true)) {
    DCHECK(android_management_checker_);
    android_management_checker_.reset();
  }

  switch (result) {
    case policy::AndroidManagementClient::Result::UNMANAGED:
      // Do nothing. ARC should be started already.
      break;
    case policy::AndroidManagementClient::Result::MANAGED:
      if (base::FeatureList::IsEnabled(
              arc::kEnableUnmanagedToManagedTransitionFeature)) {
        WaitForPoliciesLoad();
      } else {
        SetArcPlayStoreEnabledForProfile(profile_, false /* enabled */);
      }
      break;
    case policy::AndroidManagementClient::Result::ERROR:
      // This code should not be reached. For background check,
      // retry_on_error should be set.
      NOTREACHED();
  }
}

void ArcSessionManager::WaitForPoliciesLoad() {
  auto* policy_service =
      profile()->GetProfilePolicyConnector()->policy_service();

  // User might be transitioning to managed state, wait for policies load
  // to confirm.
  if (policy_service->IsFirstPolicyLoadComplete(policy::POLICY_DOMAIN_CHROME)) {
    OnFirstPoliciesLoadedOrTimeout();
  } else {
    profile_->GetProfilePolicyConnector()->policy_service()->AddObserver(
        policy::POLICY_DOMAIN_CHROME, this);
    wait_for_policy_timer_.Start(
        FROM_HERE, kWaitForPoliciesTimeout,
        base::BindOnce(&ArcSessionManager::OnFirstPoliciesLoadedOrTimeout,
                       base::Unretained(this)));
  }
}

void ArcSessionManager::OnFirstPoliciesLoaded(policy::PolicyDomain domain) {
  DCHECK(domain == policy::POLICY_DOMAIN_CHROME);

  wait_for_policy_timer_.Stop();
  OnFirstPoliciesLoadedOrTimeout();
}

void ArcSessionManager::OnFirstPoliciesLoadedOrTimeout() {
  profile_->GetProfilePolicyConnector()->policy_service()->RemoveObserver(
      policy::POLICY_DOMAIN_CHROME, this);

  // OnFirstPoliciesLoaded callback is triggered for both unmanaged and managed
  // users, we need to check user state here.
  // If timeout comes before policies are loaded, we fallback to calling
  // SetArcPlayStoreEnabledForProfile(profile_, false).
  if (arc::policy_util::IsAccountManaged(profile_)) {
    // User has become managed, notify ARC by setting transition preference,
    // which is eventually passed to ARC via ArcSession parameters.
    profile_->GetPrefs()->SetInteger(
        arc::prefs::kArcManagementTransition,
        static_cast<int>(arc::ArcManagementTransition::UNMANAGED_TO_MANAGED));

    // Restart ARC to perform managed re-provisioning.
    // kArcIsManaged and kArcSignedIn are not reset during the restart.
    // In case of successful re-provisioning, OnProvisioningFinished is called
    // and kArcIsManaged is updated.
    // In case of re-provisioning failure, ARC data is removed and transition
    // preference is reset.
    // In case Chrome is terminated during re-provisioning, user transition will
    // be detected in ProfileManager::InitProfileUserPrefs, on next startup.
    StopAndEnableArc();
  } else {
    SetArcPlayStoreEnabledForProfile(profile_, false /* enabled */);
  }
}

void ArcSessionManager::StartArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(state_ == State::STOPPED ||
         state_ == State::CHECKING_ANDROID_MANAGEMENT)
      << state_;
  state_ = State::ACTIVE;

  MaybeStartTimer();

  // ARC must be started only if no pending data removal request exists.
  DCHECK(!profile_->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  for (auto& observer : observer_list_)
    observer.OnArcStarted();

  start_time_ = base::TimeTicks::Now();
  // In case ARC started without mini-ARC |pre_start_time_| is not set.
  if (pre_start_time_.is_null())
    pre_start_time_ = start_time_;
  provisioning_reported_ = false;

  std::string locale;
  std::string preferred_languages;
  if (IsArcLocaleSyncDisabled()) {
    // Use fixed locale and preferred languages for auto-tests.
    locale = "en-US";
    preferred_languages = "en-US,en";
    VLOG(1) << "Locale and preferred languages are fixed to " << locale << ","
            << preferred_languages << ".";
  } else {
    GetLocaleAndPreferredLanguages(profile_, &locale, &preferred_languages);
  }

  arc_session_runner_->set_default_device_scale_factor(
      exo::GetDefaultDeviceScaleFactor());

  UpgradeParams params;

  const auto* demo_session = ash::DemoSession::Get();
  params.is_demo_session = demo_session && demo_session->started();
  if (params.is_demo_session) {
    DCHECK(demo_session->resources()->loaded());
    params.demo_session_apps_path =
        demo_session->resources()->GetDemoAppsPath();
  }

  params.management_transition = GetManagementTransition(profile_);
  params.locale = locale;
  // Empty |preferred_languages| is converted to empty array.
  params.preferred_languages = base::SplitString(
      preferred_languages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  DCHECK(user_manager->GetPrimaryUser());
  params.account_id =
      cryptohome::Identification(user_manager->GetPrimaryUser()->GetAccountId())
          .id();

  params.is_account_managed =
      profile_->GetProfilePolicyConnector()->IsManaged();

  arc_session_runner_->RequestUpgrade(std::move(params));
}

void ArcSessionManager::StopArc() {
  // TODO(hidehiko): This STOPPED guard should be unnecessary. Remove it later.
  // |reenable_arc_| may be set in |StopAndEnableArc| in case enterprise
  // management state is lost.
  if (!reenable_arc_ && state_ != State::STOPPED) {
    profile_->GetPrefs()->SetBoolean(prefs::kArcSignedIn, false);
    profile_->GetPrefs()->SetBoolean(prefs::kArcPaiStarted, false);
    profile_->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, false);
    profile_->GetPrefs()->SetBoolean(prefs::kArcFastAppReinstallStarted, false);
    profile_->GetPrefs()->SetBoolean(prefs::kArcProvisioningInitiatedFromOobe,
                                     false);
    profile_->GetPrefs()->ClearPref(prefs::kArcIsManaged);
  }

  ShutdownSession();
  if (support_host_)
    support_host_->Close();
}

void ArcSessionManager::MaybeStartArcDataRemoval() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  // Data removal cannot run in parallel with ARC session.
  // LoginScreen instance does not use data directory, so removing should work.
  DCHECK_EQ(state_, State::STOPPED);

  state_ = State::REMOVING_DATA_DIR;
  data_remover_->Run(base::BindOnce(&ArcSessionManager::OnArcDataRemoved,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::OnArcDataRemoved(absl::optional<bool> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::REMOVING_DATA_DIR);
  DCHECK(profile_);
  state_ = State::STOPPED;

  if (result.has_value()) {
    // Remove Play user ID for Active Directory managed devices.
    profile_->GetPrefs()->SetString(prefs::kArcActiveDirectoryPlayUserId,
                                    std::string());

    // Regardless of whether it is successfully done or not, notify observers.
    for (auto& observer : observer_list_)
      observer.OnArcDataRemoved();

    // Note: Currently, we may re-enable ARC even if data removal fails.
    // We may have to avoid it.
  }

  MaybeReenableArc();
}

void ArcSessionManager::MaybeReenableArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::STOPPED);

  if (!reenable_arc_) {
    // Re-enabling is not triggered. Do nothing.
    return;
  }
  DCHECK(enable_requested_);

  // Restart ARC anyway. Let the enterprise reporting instance decide whether
  // the ARC user data wipe is still required or not.
  reenable_arc_ = false;
  VLOG(1) << "Reenable ARC";
  RequestEnableImpl();
}

// Starts a timer to check if provisioning takes too loong.
// The timer will not be set if this device was previously provisioned
// successfully.
void ArcSessionManager::MaybeStartTimer() {
  if (IsArcProvisioned(profile_)) {
    return;
  }

  VLOG(1) << "Setup provisioning timer";
  sign_in_start_time_ = base::TimeTicks::Now();
  arc_sign_in_timer_.Start(
      FROM_HERE, GetArcSignInTimeout(),
      base::BindOnce(&ArcSessionManager::OnArcSignInTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::StartMiniArc() {
  pre_start_time_ = base::TimeTicks::Now();
  arc_session_runner_->set_default_device_scale_factor(
      exo::GetDefaultDeviceScaleFactor());
  arc_session_runner_->RequestStartMiniInstance();
}

void ArcSessionManager::OnWindowClosed() {
  CancelAuthCode();

  // If network-related error occured, collect UMA stats on user action.
  if (support_host_ && support_host_->GetShouldShowRunNetworkTests())
    UpdateOptInNetworkErrorActionUMA(
        arc::OptInNetworkErrorActionType::WINDOW_CLOSED);
}

void ArcSessionManager::OnRetryClicked() {
  DCHECK(!g_ui_enabled || support_host_);
  DCHECK(!g_ui_enabled ||
         support_host_->ui_page() == ArcSupportHost::UIPage::ERROR);
  DCHECK(!terms_of_service_negotiator_);
  DCHECK(!g_ui_enabled || !support_host_->HasAuthDelegate());

  UpdateOptInActionUMA(OptInActionType::RETRY);

  VLOG(1) << "Retry button clicked";

  if (state_ == State::ACTIVE) {
    // ERROR_WITH_FEEDBACK is set in OnSignInFailed(). In the case, stopping
    // ARC was postponed to contain its internal state into the report.
    // Here, on retry, stop it, then restart.
    if (support_host_)
      support_host_->ShowArcLoading();
    // In unit tests ShutdownSession may be executed inline and OnSessionStopped
    // is called before |reenable_arc_| is set.
    reenable_arc_ = true;
    ShutdownSession();
  } else {
    // Otherwise, we start ARC once it is stopped now. Usually ARC container is
    // left active after provisioning failure but in case
    // ProvisioningStatus::ARC_STOPPED and
    // ProvisioningStatus::CHROME_SERVER_COMMUNICATION_ERROR failures
    // container is stopped. At this point ToS is already accepted and
    // IsArcTermsOfServiceNegotiationNeeded returns true or ToS needs not to be
    // shown at all. However there is an exception when this does not happen in
    // case an error page is shown when re-opt-in right after opt-out (this is a
    // bug as it should not show an error). When the user click the retry
    // button on this error page, we may start ToS negotiation instead of
    // recreating the instance.
    // TODO(hidehiko): consider removing this case after fixing the bug.
    MaybeStartTermsOfServiceNegotiation();
  }

  // If network-related error occured, collect UMA stats on user action.
  if (support_host_ && support_host_->GetShouldShowRunNetworkTests())
    UpdateOptInNetworkErrorActionUMA(arc::OptInNetworkErrorActionType::RETRY);
}

void ArcSessionManager::OnSendFeedbackClicked() {
  DCHECK(support_host_);
  chrome::OpenFeedbackDialog(nullptr, chrome::kFeedbackSourceArcApp);

  // If network-related error occured, collect UMA stats on user action.
  if (support_host_->GetShouldShowRunNetworkTests())
    UpdateOptInNetworkErrorActionUMA(
        arc::OptInNetworkErrorActionType::SEND_FEEDBACK);
}

void ArcSessionManager::OnRunNetworkTestsClicked() {
  DCHECK(support_host_);
  chromeos::DiagnosticsDialog::ShowDialog();

  // Network-related error occured so collect UMA stats on user action.
  UpdateOptInNetworkErrorActionUMA(
      arc::OptInNetworkErrorActionType::CHECK_NETWORK);
}

void ArcSessionManager::SetArcSessionRunnerForTesting(
    std::unique_ptr<ArcSessionRunner> arc_session_runner) {
  DCHECK(arc_session_runner);
  DCHECK(arc_session_runner_);
  arc_session_runner_->RemoveObserver(this);
  arc_session_runner_ = std::move(arc_session_runner);
  arc_session_runner_->AddObserver(this);
}

ArcSessionRunner* ArcSessionManager::GetArcSessionRunnerForTesting() {
  return arc_session_runner_.get();
}

void ArcSessionManager::SetAttemptUserExitCallbackForTesting(
    const base::RepeatingClosure& callback) {
  DCHECK(!callback.is_null());
  attempt_user_exit_callback_ = callback;
}

void ArcSessionManager::ShowArcSupportHostError(
    ArcSupportHost::ErrorInfo error_info,
    bool should_show_send_feedback,
    bool should_show_run_network_tests) {
  if (support_host_)
    support_host_->ShowError(error_info, should_show_send_feedback,
                             should_show_run_network_tests);
  for (auto& observer : observer_list_)
    observer.OnArcErrorShowRequested(error_info);
}

void ArcSessionManager::EmitLoginPromptVisibleCalled() {
  // Since 'login-prompt-visible' Upstart signal starts all Upstart jobs the
  // instance may depend on such as cras, EmitLoginPromptVisibleCalled() is the
  // safe place to start a mini instance.
  if (!IsArcAvailable())
    return;

  StartMiniArc();
}

void ArcSessionManager::ExpandPropertyFilesAndReadSalt() {
  VLOG(1) << "Started expanding *.prop files";

  // For ARCVM, generate <dest_path>/{combined.prop,fstab}. For ARC, generate
  // <dest_path>/{default,build,vendor_build}.prop.
  const bool is_arcvm = arc::IsArcVmEnabled();
  bool add_native_bridge_64bit_support = false;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kArcEnableNativeBridge64BitSupportExperiment)) {
    PrefService* local_pref_service = g_browser_process->local_state();
    if (base::FeatureList::IsEnabled(
            arc::kNativeBridge64BitSupportExperimentFeature)) {
      // Note that we treat this experiment as a one-way off->on switch, across
      // all users of the device, as the lifetime of ARC mini-container and user
      // sessions are different in different scenarios, and removing the
      // experiment after it has been in effect for a user's ARC instance can
      // lead to unexpected, and unsupported, results.
      local_pref_service->SetBoolean(
          prefs::kNativeBridge64BitSupportExperimentEnabled, true);
    }
    add_native_bridge_64bit_support = local_pref_service->GetBoolean(
        prefs::kNativeBridge64BitSupportExperimentEnabled);
  }

  std::deque<JobDesc> jobs = {
      JobDesc{kArcPrepareHostGeneratedDirJobName,
              UpstartOperation::JOB_START,
              {std::string("IS_ARCVM=") + (is_arcvm ? "1" : "0"),
               std::string("ADD_NATIVE_BRIDGE_64BIT_SUPPORT=") +
                   (add_native_bridge_64bit_support ? "1" : "0")}},
  };
  ConfigureUpstartJobs(std::move(jobs),
                       base::BindOnce(&ArcSessionManager::OnExpandPropertyFiles,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::OnExpandPropertyFiles(bool result) {
  if (!result) {
    LOG(ERROR) << "Failed to expand property files";
    OnExpandPropertyFilesAndReadSalt(
        ArcSessionManager::ExpansionResult{{}, false});
    return;
  }

  if (!arc::IsArcVmEnabled()) {
    OnExpandPropertyFilesAndReadSalt(
        ArcSessionManager::ExpansionResult{{}, true});
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadSaltInternal),
      base::BindOnce(&ArcSessionManager::OnExpandPropertyFilesAndReadSalt,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::OnExpandPropertyFilesAndReadSalt(
    ExpansionResult result) {
  // ExpandPropertyFilesAndReadSalt() should be called only once.
  DCHECK(!property_files_expansion_result_);

  arc_salt_on_disk_ = result.first;
  property_files_expansion_result_ = result.second;

  // See the comment in Initialize().
  if (profile_) {
    VLOG(1) << "Calling SetUserInfo() in "
            << "ArcSessionManager::OnExpandPropertyFilesAndReadSalt";
    SetUserInfo();
  }

  if (result.second)
    arc_session_runner_->ResumeRunner();
  for (auto& observer : observer_list_)
    observer.OnPropertyFilesExpanded(*property_files_expansion_result_);
}

void ArcSessionManager::StopMiniArcIfNecessary() {
  // This method should only be called before login.
  DCHECK(!profile_);
  pre_start_time_ = base::TimeTicks();
  VLOG(1) << "Stopping mini-ARC instance (if any)";
  arc_session_runner_->RequestStop();
}

std::ostream& operator<<(std::ostream& os,
                         const ArcSessionManager::State& state) {
#define MAP_STATE(name)                \
  case ArcSessionManager::State::name: \
    return os << #name

  switch (state) {
    MAP_STATE(NOT_INITIALIZED);
    MAP_STATE(STOPPED);
    MAP_STATE(NEGOTIATING_TERMS_OF_SERVICE);
    MAP_STATE(CHECKING_ANDROID_MANAGEMENT);
    MAP_STATE(REMOVING_DATA_DIR);
    MAP_STATE(ACTIVE);
    MAP_STATE(STOPPING);
  }

#undef MAP_STATE

  // Some compilers report an error even if all values of an enum-class are
  // covered exhaustively in a switch statement.
  NOTREACHED() << "Invalid value " << static_cast<int>(state);
  return os;
}

}  // namespace arc
