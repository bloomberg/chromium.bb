// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_display_host_common.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_accelerators.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/language_preferences.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/lock_screen_utils.h"
#include "chrome/browser/ash/login/screens/encryption_migration_screen.h"
#include "chrome/browser/ash/login/screens/gaia_screen.h"
#include "chrome/browser/ash/login/screens/pin_setup_screen.h"
#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/ui/login_feedback.h"
#include "chrome/browser/ash/login/ui/signin_ui.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/chromeos/diagnostics_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/locale_switch_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/management_transition_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/os_install_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_fatal_error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

namespace ash {
namespace {

// The delay of triggering initialization of the device policy subsystem
// after the login screen is initialized. This makes sure that device policy
// network requests are made while the system is idle waiting for user input.
constexpr int64_t kPolicyServiceInitializationDelayMilliseconds = 100;

void ScheduleCompletionCallbacks(std::vector<base::OnceClosure>&& callbacks) {
  for (auto& callback : callbacks) {
    if (callback.is_null())
      continue;

    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  }
}

void PushFrontImIfNotExists(const std::string& input_method_id,
                            std::vector<std::string>* input_method_ids) {
  if (input_method_id.empty())
    return;

  if (!base::Contains(*input_method_ids, input_method_id))
    input_method_ids->insert(input_method_ids->begin(), input_method_id);
}

void SetGaiaInputMethods(const AccountId& account_id) {
  input_method::InputMethodManager* imm =
      input_method::InputMethodManager::Get();

  scoped_refptr<input_method::InputMethodManager::State> gaia_ime_state =
      imm->GetActiveIMEState()->Clone();
  imm->SetState(gaia_ime_state);
  gaia_ime_state->SetUIStyle(input_method::InputMethodManager::UIStyle::kLogin);

  // Set Least Recently Used input method for the user.
  if (account_id.is_valid()) {
    lock_screen_utils::SetUserInputMethod(account_id, gaia_ime_state.get(),
                                          true /*honor_device_policy*/);
  } else {
    lock_screen_utils::EnforceDevicePolicyInputMethods(std::string());
    std::vector<std::string> input_method_ids;
    if (gaia_ime_state->GetAllowedInputMethodIds().empty()) {
      input_method_ids =
          imm->GetInputMethodUtil()->GetHardwareLoginInputMethodIds();
    } else {
      input_method_ids = gaia_ime_state->GetAllowedInputMethodIds();
    }
    const std::string owner_input_method_id =
        lock_screen_utils::GetUserLastInputMethodId(
            user_manager::UserManager::Get()->GetOwnerAccountId());
    const std::string system_input_method_id =
        g_browser_process->local_state()->GetString(
            language_prefs::kPreferredKeyboardLayout);

    PushFrontImIfNotExists(owner_input_method_id, &input_method_ids);
    PushFrontImIfNotExists(system_input_method_id, &input_method_ids);

    gaia_ime_state->EnableLoginLayouts(
        g_browser_process->GetApplicationLocale(), input_method_ids);

    if (!system_input_method_id.empty()) {
      gaia_ime_state->ChangeInputMethod(system_input_method_id,
                                        false /* show_message */);
    } else if (!owner_input_method_id.empty()) {
      gaia_ime_state->ChangeInputMethod(owner_input_method_id,
                                        false /* show_message */);
    }
  }
}

int ErrorToMessageId(SigninError error) {
  switch (error) {
    case SigninError::kCaptivePortalError:
      NOTREACHED();
      return 0;
    case SigninError::kGoogleAccountNotAllowed:
      return IDS_LOGIN_ERROR_GOOGLE_ACCOUNT_NOT_ALLOWED;
    case SigninError::kOwnerRequired:
      return IDS_LOGIN_ERROR_OWNER_REQUIRED;
    case SigninError::kTpmUpdateRequired:
      return IDS_LOGIN_ERROR_TPM_UPDATE_REQUIRED;
    case SigninError::kKnownUserFailedNetworkNotConnected:
      return IDS_LOGIN_ERROR_AUTHENTICATING;
    case SigninError::kNewUserFailedNetworkNotConnected:
      return IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED;
    case SigninError::kNewUserFailedNetworkConnected:
      return IDS_LOGIN_ERROR_AUTHENTICATING_NEW;
    case SigninError::kKnownUserFailedNetworkConnected:
      return IDS_LOGIN_ERROR_AUTHENTICATING;
    case SigninError::kOwnerKeyLost:
      return IDS_LOGIN_ERROR_OWNER_KEY_LOST;
    case SigninError::kChallengeResponseAuthMultipleClientCerts:
      return IDS_CHALLENGE_RESPONSE_AUTH_MULTIPLE_CLIENT_CERTS_ERROR;
    case SigninError::kChallengeResponseAuthInvalidClientCert:
      return IDS_CHALLENGE_RESPONSE_AUTH_INVALID_CLIENT_CERT_ERROR;
    case SigninError::kCookieWaitTimeout:
      return IDS_LOGIN_FATAL_ERROR_NO_AUTH_TOKEN;
    case SigninError::kFailedToFetchSamlRedirect:
      return IDS_FAILED_TO_FETCH_SAML_REDIRECT;
    case SigninError::kActiveDirectoryNetworkProblem:
      return IDS_AD_AUTH_NETWORK_ERROR;
    case SigninError::kActiveDirectoryNotSupportedEncryption:
      return IDS_AD_AUTH_NOT_SUPPORTED_ENCRYPTION;
    case SigninError::kActiveDirectoryUnknownError:
      return IDS_AD_AUTH_UNKNOWN_ERROR;
  }
}

bool IsAuthError(SigninError error) {
  return error == SigninError::kCaptivePortalError ||
         error == SigninError::kKnownUserFailedNetworkNotConnected ||
         error == SigninError::kNewUserFailedNetworkNotConnected ||
         error == SigninError::kNewUserFailedNetworkConnected ||
         error == SigninError::kKnownUserFailedNetworkConnected;
}

}  // namespace

LoginDisplayHostCommon::LoginDisplayHostCommon()
    : keep_alive_(KeepAliveOrigin::LOGIN_DISPLAY_HOST_WEBUI,
                  KeepAliveRestartOption::DISABLED),
      wizard_context_(std::make_unique<WizardContext>()) {
  // Close the login screen on NOTIFICATION_APP_TERMINATING (for the case where
  // shutdown occurs before login completes).
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
  BrowserList::AddObserver(this);
}

LoginDisplayHostCommon::~LoginDisplayHostCommon() {
  ScheduleCompletionCallbacks(std::move(completion_callbacks_));
}

void LoginDisplayHostCommon::BeforeSessionStart() {
  session_starting_ = true;
}

void LoginDisplayHostCommon::Finalize(base::OnceClosure completion_callback) {
  VLOG(4) << "Finalize";
  // If finalize is called twice the LoginDisplayHost instance will be deleted
  // multiple times.
  CHECK(!is_finalizing_);
  is_finalizing_ = true;

  completion_callbacks_.push_back(std::move(completion_callback));
  OnFinalize();
}

void LoginDisplayHostCommon::FinalizeImmediately() {
  CHECK(!is_finalizing_);
  CHECK(!shutting_down_);
  is_finalizing_ = true;
  shutting_down_ = true;
  OnFinalize();
  Cleanup();
  delete this;
}

KioskLaunchController* LoginDisplayHostCommon::GetKioskLaunchController() {
  return kiosk_launch_controller_.get();
}

void LoginDisplayHostCommon::StartUserAdding(
    base::OnceClosure completion_callback) {
  completion_callbacks_.push_back(std::move(completion_callback));
  OnStartUserAdding();
}

void LoginDisplayHostCommon::StartSignInScreen() {
  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();

  // Fix for users who updated device and thus never passed register screen.
  // If we already have users, we assume that it is not a second part of
  // OOBE. See http://crosbug.com/6289
  if (!StartupUtils::IsDeviceRegistered() && !users.empty()) {
    VLOG(1) << "Mark device registered because there are remembered users: "
            << users.size();
    StartupUtils::MarkDeviceRegistered(base::OnceClosure());
  }

  // Initiate device policy fetching.
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  connector->ScheduleServiceInitialization(
      kPolicyServiceInitializationDelayMilliseconds);

  // Run UI-specific logic.
  OnStartSignInScreen();

  // Enable status area after starting sign-in screen, as it may depend on the
  // UI being visible.
  SetStatusAreaVisible(true);
}

void LoginDisplayHostCommon::StartKiosk(const KioskAppId& kiosk_app_id,
                                        bool is_auto_launch) {
  VLOG(1) << "Login >> start kiosk of type "
          << static_cast<int>(kiosk_app_id.type);
  SetStatusAreaVisible(false);

  // Wait for the `CrosSettings` to become either trusted or permanently
  // untrusted.
  const CrosSettingsProvider::TrustedStatus status =
      CrosSettings::Get()->PrepareTrustedValues(base::BindOnce(
          &LoginDisplayHostCommon::StartKiosk, weak_factory_.GetWeakPtr(),
          kiosk_app_id, is_auto_launch));
  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED)
    return;

  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // If the `CrosSettings` are permanently untrusted, refuse to launch a
    // single-app kiosk mode session.
    LOG(ERROR) << "Login >> Refusing to launch single-app kiosk mode.";
    SetStatusAreaVisible(true);
    return;
  }

  if (system::DeviceDisablingManager::IsDeviceDisabledDuringNormalOperation()) {
    // If the device is disabled, bail out. A device disabled screen will be
    // shown by the DeviceDisablingManager.
    return;
  }

  OnStartAppLaunch();

  int auto_launch_delay = -1;
  if (is_auto_launch) {
    if (!CrosSettings::Get()->GetInteger(
            kAccountsPrefDeviceLocalAccountAutoLoginDelay,
            &auto_launch_delay)) {
      auto_launch_delay = 0;
    }
    DCHECK_EQ(0, auto_launch_delay)
        << "Kiosks do not support non-zero auto-login delays";
  }

  extensions::SetCurrentFeatureSessionType(
      is_auto_launch && auto_launch_delay == 0
          ? extensions::mojom::FeatureSessionType::kAutolaunchedKiosk
          : extensions::mojom::FeatureSessionType::kKiosk);

  kiosk_launch_controller_ =
      std::make_unique<KioskLaunchController>(GetOobeUI());
  kiosk_launch_controller_->Start(kiosk_app_id, is_auto_launch);
}

void LoginDisplayHostCommon::AttemptShowEnableConsumerKioskScreen() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (!connector->IsDeviceEnterpriseManaged() &&
      KioskAppManager::IsConsumerKioskEnabled()) {
    ShowEnableConsumerKioskScreen();
  }
}

void LoginDisplayHostCommon::CompleteLogin(const UserContext& user_context) {
  if (GetExistingUserController()) {
    GetExistingUserController()->CompleteLogin(user_context);
  } else {
    LOG(WARNING) << "LoginDisplayHostCommon::CompleteLogin - Failure : "
                 << "ExistingUserController not available.";
  }
}

void LoginDisplayHostCommon::OnGaiaScreenReady() {
  if (GetExistingUserController()) {
    GetExistingUserController()->OnGaiaScreenReady();
  } else {
    // Used to debug crbug.com/902315. Feel free to remove after that is fixed.
    LOG(ERROR) << "OnGaiaScreenReady: there is no existing user controller";
  }
}

void LoginDisplayHostCommon::SetDisplayEmail(const std::string& email) {
  if (GetExistingUserController())
    GetExistingUserController()->SetDisplayEmail(email);
}

void LoginDisplayHostCommon::SetDisplayAndGivenName(
    const std::string& display_name,
    const std::string& given_name) {
  if (GetExistingUserController())
    GetExistingUserController()->SetDisplayAndGivenName(display_name,
                                                        given_name);
}

void LoginDisplayHostCommon::LoadWallpaper(const AccountId& account_id) {
  WallpaperControllerClientImpl::Get()->ShowUserWallpaper(account_id);
}

void LoginDisplayHostCommon::LoadSigninWallpaper() {
  WallpaperControllerClientImpl::Get()->ShowSigninWallpaper();
}

bool LoginDisplayHostCommon::IsUserAllowlisted(
    const AccountId& account_id,
    const absl::optional<user_manager::UserType>& user_type) {
  if (!GetExistingUserController())
    return true;
  return GetExistingUserController()->IsUserAllowlisted(account_id, user_type);
}

void LoginDisplayHostCommon::CancelPasswordChangedFlow() {
  if (GetExistingUserController())
    GetExistingUserController()->CancelPasswordChangedFlow();

  OnCancelPasswordChangedFlow();
}

void LoginDisplayHostCommon::MigrateUserData(const std::string& old_password) {
  if (GetExistingUserController())
    GetExistingUserController()->MigrateUserData(old_password);
}

void LoginDisplayHostCommon::ResyncUserData() {
  if (GetExistingUserController())
    GetExistingUserController()->ResyncUserData();
}

bool LoginDisplayHostCommon::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kShowFeedback) {
    login_feedback_ = std::make_unique<LoginFeedback>(
        ProfileHelper::Get()->GetSigninProfile());
    login_feedback_->Request(
        std::string(),
        base::BindOnce(&LoginDisplayHostCommon::OnFeedbackFinished,
                       weak_factory_.GetWeakPtr()));
    return true;
  }

  if (action == LoginAcceleratorAction::kLaunchDiagnostics &&
      base::FeatureList::IsEnabled(features::kDiagnosticsApp)) {
    // Don't handle this action if device is disabled.
    if (system::DeviceDisablingManager::
            IsDeviceDisabledDuringNormalOperation()) {
      return false;
    }
    chromeos::DiagnosticsDialog::ShowDialog();
    return true;
  }

  // This path should only handle screen-specific acceletators, so we do not
  // need to create WebUI here.
  if (IsWizardControllerCreated() &&
      GetWizardController()->HandleAccelerator(action)) {
    return true;
  }
  // There are currently no global accelerators for the lock screen that
  // require WebUI. So we do not need to specifically load it when user is
  // logged in.
  if (GetOobeUI() || (!MapToWebUIAccelerator(action).empty() &&
                      !user_manager::UserManager::Get()->IsUserLoggedIn())) {
    // Ensure WebUI is loaded.
    GetWizardController();
    // TODO(crbug.com/1102393): Remove once all accelerators handling is
    // migrated to browser side.
    GetOobeUI()->ForwardAccelerator(MapToWebUIAccelerator(action));
  }
  return true;
}

void LoginDisplayHostCommon::SetScreenAfterManagedTos(OobeScreenId screen_id) {
  // If user stopped onboarding flow on TermsOfServiceScreen make sure that
  // next screen will be FamilyLinkNoticeView::kScreenId.
  if (screen_id == TermsOfServiceScreenView::kScreenId)
    screen_id = FamilyLinkNoticeView::kScreenId;
  wizard_context_->screen_after_managed_tos = screen_id;
}

void LoginDisplayHostCommon::StartUserOnboarding() {
  StartWizard(LocaleSwitchView::kScreenId);
}

void LoginDisplayHostCommon::ResumeUserOnboarding(OobeScreenId screen_id) {
  SetScreenAfterManagedTos(screen_id);
  // Try to show TermsOfServiceScreen first
  StartWizard(TermsOfServiceScreenView::kScreenId);
}

void LoginDisplayHostCommon::StartManagementTransition() {
  StartWizard(ManagementTransitionScreenView::kScreenId);
}

void LoginDisplayHostCommon::ShowTosForExistingUser() {
  SetScreenAfterManagedTos(OobeScreen::SCREEN_UNKNOWN);
  StartUserOnboarding();
}

void LoginDisplayHostCommon::SetAuthSessionForOnboarding(
    const UserContext& user_context) {
  if (PinSetupScreen::ShouldSkipBecauseOfPolicy())
    return;

  wizard_context_->extra_factors_auth_session =
      std::make_unique<UserContext>(user_context);
}

void LoginDisplayHostCommon::ClearOnboardingAuthSession() {
  wizard_context_->extra_factors_auth_session.reset();
}

void LoginDisplayHostCommon::StartEncryptionMigration(
    const UserContext& user_context,
    EncryptionMigrationMode migration_mode,
    base::OnceCallback<void(const UserContext&)> on_skip_migration) {
  StartWizard(EncryptionMigrationScreenView::kScreenId);

  EncryptionMigrationScreen* migration_screen =
      GetWizardController()->GetScreen<EncryptionMigrationScreen>();

  DCHECK(migration_screen);
  migration_screen->SetUserContext(user_context);
  migration_screen->SetMode(migration_mode);
  migration_screen->SetSkipMigrationCallback(std::move(on_skip_migration));
  migration_screen->SetupInitialView();
}

void LoginDisplayHostCommon::ShowSigninError(SigninError error,
                                             const std::string& details) {
  VLOG(1) << "Show error, error_id: " << static_cast<int>(error);

  if (error == SigninError::kKnownUserFailedNetworkNotConnected ||
      error == SigninError::kKnownUserFailedNetworkConnected) {
    if (!IsOobeUIDialogVisible())
      // Handled by Views UI.
      return;
  }

  std::string error_text;
  switch (error) {
    case SigninError::kCaptivePortalError:
      error_text = l10n_util::GetStringFUTF8(
          IDS_LOGIN_ERROR_CAPTIVE_PORTAL,
          GetExistingUserController()->GetConnectedNetworkName());
      break;
    default:
      error_text = l10n_util::GetStringUTF8(ErrorToMessageId(error));
      break;
  }

  std::string keyboard_hint;

  // Only display hints about keyboard layout if the error is authentication-
  // related.
  if (IsAuthError(error)) {
    input_method::InputMethodManager* ime_manager =
        input_method::InputMethodManager::Get();
    // Display a hint to switch keyboards if there are other enabled input
    // methods.
    if (ime_manager->GetActiveIMEState()->GetNumEnabledInputMethods() > 1) {
      keyboard_hint =
          l10n_util::GetStringUTF8(IDS_LOGIN_ERROR_KEYBOARD_SWITCH_HINT);
    }
  }

  std::string help_link_text = l10n_util::GetStringUTF8(IDS_LEARN_MORE);

  GetWizardController()->GetScreen<SignInFatalErrorScreen>()->SetCustomError(
      error_text, keyboard_hint, details, help_link_text);
  StartWizard(SignInFatalErrorView::kScreenId);
}

WizardContext* LoginDisplayHostCommon::GetWizardContextForTesting() {
  return GetWizardContext();
}

void LoginDisplayHostCommon::OnBrowserAdded(Browser* browser) {
  VLOG(4) << "OnBrowserAdded " << session_starting_;
  // Browsers created before session start (windows opened by extensions, for
  // example) are ignored.
  if (session_starting_) {
    // OnBrowserAdded is called when the browser is created, but not shown yet.
    // Lock window has to be closed at this point so that a browser window
    // exists and the window can acquire input focus.
    OnBrowserCreated();
    registrar_.RemoveAll();
    BrowserList::RemoveObserver(this);
  }
}

void LoginDisplayHostCommon::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_APP_TERMINATING)
    ShutdownDisplayHost();
}

WizardContext* LoginDisplayHostCommon::GetWizardContext() {
  return wizard_context_.get();
}

void LoginDisplayHostCommon::OnCancelPasswordChangedFlow() {
  LoginDisplayHost::default_host()->StartSignInScreen();
}

void LoginDisplayHostCommon::ShutdownDisplayHost() {
  if (shutting_down_)
    return;
  shutting_down_ = true;

  Cleanup();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void LoginDisplayHostCommon::OnStartSignInScreenCommon() {
  kiosk_app_menu_controller_.SendKioskApps();
}

void LoginDisplayHostCommon::ShowGaiaDialogCommon(
    const AccountId& prefilled_account) {
  if (prefilled_account.is_valid()) {
    LoadWallpaper(prefilled_account);
    if (GetLoginDisplay()->delegate()->IsSigninInProgress()) {
      return;
    }
  } else {
    LoadSigninWallpaper();
  }

  SetGaiaInputMethods(prefilled_account);

  if (!prefilled_account.is_valid()) {
    StartWizard(UserCreationView::kScreenId);
  } else {
    GaiaScreen* gaia_screen = GetWizardController()->GetScreen<GaiaScreen>();
    gaia_screen->LoadOnline(prefilled_account);
    StartWizard(GaiaView::kScreenId);
  }
}

void LoginDisplayHostCommon::AddWizardCreatedObserverForTests(
    base::RepeatingClosure on_created) {
  DCHECK(!on_wizard_controller_created_for_tests_);
  on_wizard_controller_created_for_tests_ = std::move(on_created);
}

void LoginDisplayHostCommon::NotifyWizardCreated() {
  if (on_wizard_controller_created_for_tests_)
    on_wizard_controller_created_for_tests_.Run();
}

void LoginDisplayHostCommon::Cleanup() {
  ProfileHelper::Get()->ClearSigninProfile(base::DoNothing());
  registrar_.RemoveAll();
  BrowserList::RemoveObserver(this);
}

void LoginDisplayHostCommon::OnFeedbackFinished() {
  login_feedback_.reset();
}

}  // namespace ash
