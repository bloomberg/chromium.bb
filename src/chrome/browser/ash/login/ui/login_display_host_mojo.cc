// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_display_host_mojo.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ash/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/ash/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/ash/certificate_provider/pin_dialog_manager.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/mojo_system_info_dispatcher.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/ash/login/screens/gaia_screen.h"
#include "chrome/browser/ash/login/security_token_session_controller.h"
#include "chrome/browser/ash/login/ui/login_display.h"
#include "chrome/browser/ash/login/ui/login_display_mojo.h"
#include "chrome/browser/ash/login/user_board_view_mojo.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login_screen_client_impl.h"
#include "chrome/browser/ui/ash/system_tray_client_impl.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_password_changed_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/lacros_data_migration_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_fatal_error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chrome/common/channel_info.h"
#include "chromeos/login/auth/user_context.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/views/view.h"

namespace ash {
namespace {

CertificateProviderService* GetLoginScreenCertProviderService() {
  DCHECK(ProfileHelper::IsSigninProfileInitialized());
  return CertificateProviderServiceFactory::GetForBrowserContext(
      ProfileHelper::GetSigninProfile());
}

// Returns true iff
// (i)   log in is restricted to some user list,
// (ii)  all users in the restricted list are present.
bool AllAllowlistedUsersPresent() {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = false;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  if (allow_new_user)
    return false;
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::UserList& users = user_manager->GetUsers();

  // Max number of users to show.
  const size_t kMaxUsers = 18;
  if (users.size() > kMaxUsers)
    return false;

  bool allow_family_link = false;
  cros_settings->GetBoolean(kAccountsPrefFamilyLinkAccountsAllowed,
                            &allow_family_link);
  if (allow_family_link)
    return false;

  const base::ListValue* allowlist = nullptr;
  if (!cros_settings->GetList(kAccountsPrefUsers, &allowlist) || !allowlist)
    return false;
  for (const base::Value& i : allowlist->GetList()) {
    const std::string* allowlisted_user = i.GetIfString();
    // NB: Wildcards in the allowlist are also detected as not present here.
    if (!allowlisted_user || !user_manager->IsKnownUser(
                                 AccountId::FromUserEmail(*allowlisted_user))) {
      return false;
    }
  }
  return true;
}

bool IsLazyWebUILoadingEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::chromeos::switches::kEnableOobeTestAPI)) {
    // Load WebUI for the test API explicitly because it's Web API.
    return false;
  }

  // Policy override.
  if (g_browser_process->local_state()->IsManagedPreference(
          prefs::kLoginScreenWebUILazyLoading)) {
    return g_browser_process->local_state()->GetBoolean(
        ash::prefs::kLoginScreenWebUILazyLoading);
  }

  // Feature override.
  if (base::FeatureList::GetInstance()->IsFeatureOverridden(
          features::kEnableLazyLoginWebUILoading.name)) {
    return base::FeatureList::IsEnabled(features::kEnableLazyLoginWebUILoading);
  }

  // Disable for stable and beta.
  if ((chrome::GetChannel() == version_info::Channel::STABLE) ||
      (chrome::GetChannel() == version_info::Channel::BETA)) {
    return false;
  }

  // Enable for dev builds.
  return true;
}

}  // namespace

LoginDisplayHostMojo::AuthState::AuthState(
    AccountId account_id,
    base::OnceCallback<void(bool)> callback)
    : account_id(account_id), callback(std::move(callback)) {}

LoginDisplayHostMojo::AuthState::~AuthState() = default;

LoginDisplayHostMojo::LoginDisplayHostMojo(DisplayedScreen displayed_screen)
    : login_display_(std::make_unique<LoginDisplayMojo>(this)),
      user_board_view_mojo_(std::make_unique<UserBoardViewMojo>()),
      user_selection_screen_(
          std::make_unique<ChromeUserSelectionScreen>(displayed_screen)),
      system_info_updater_(std::make_unique<MojoSystemInfoDispatcher>()) {
  user_selection_screen_->SetView(user_board_view_mojo_.get());

  // Do not load WebUI before it is needed if policy and feature permit.
  // Force load WebUI if feature is not enabled.
  if (!IsLazyWebUILoadingEnabled() &&
      displayed_screen == DisplayedScreen::SIGN_IN_SCREEN) {
    EnsureOobeDialogLoaded();
  }
}

LoginDisplayHostMojo::~LoginDisplayHostMojo() {
  LoginScreenClientImpl::Get()->SetDelegate(nullptr);
  if (!dialog_)
    return;

  GetLoginScreenCertProviderService()
      ->pin_dialog_manager()
      ->RemovePinDialogHost(&security_token_pin_dialog_host_login_impl_);
  dialog_->GetOobeUI()->signin_screen_handler()->SetDelegate(nullptr);
  StopObservingOobeUI();
  dialog_->Close();
}

void LoginDisplayHostMojo::OnDialogDestroyed(
    const OobeUIDialogDelegate* dialog) {
  if (dialog == dialog_) {
    StopObservingOobeUI();
    dialog_ = nullptr;
    wizard_controller_.reset();
  }
}

void LoginDisplayHostMojo::SetUserCount(int user_count) {
  const bool was_zero_users = (user_count_ == 0);
  user_count_ = user_count;
  if (GetOobeUI())
    GetOobeUI()->SetLoginUserCount(user_count_);

  // Hide Gaia dialog in case empty list of users switched to a non-empty one.
  // And if the dialog shows login screen.
  if (was_zero_users && user_count_ != 0 && dialog_ && dialog_->IsVisible() &&
      (!wizard_controller_->is_initialized() ||
       (wizard_controller_->current_screen() &&
        WizardController::IsSigninScreen(
            wizard_controller_->current_screen()->screen_id())))) {
    HideOobeDialog();
  }
}

void LoginDisplayHostMojo::ShowPasswordChangedDialog(
    const AccountId& account_id,
    bool show_password_error) {
  EnsureOobeDialogLoaded();
  DCHECK(GetOobeUI());
  wizard_controller_->ShowGaiaPasswordChangedScreen(account_id,
                                                    show_password_error);
  ShowDialog();
}

void LoginDisplayHostMojo::StartBrowserDataMigration() {
  DCHECK(GetOobeUI());
  wizard_controller_->AdvanceToScreen(LacrosDataMigrationScreenView::kScreenId);
}

void LoginDisplayHostMojo::ShowAllowlistCheckFailedError() {
  EnsureOobeDialogLoaded();
  DCHECK(GetOobeUI());
  GetOobeUI()->signin_screen_handler()->ShowAllowlistCheckFailedError();
  ShowDialog();
}

void LoginDisplayHostMojo::HandleDisplayCaptivePortal() {
  EnsureOobeDialogLoaded();
  if (dialog_->IsVisible())
    GetOobeUI()->GetErrorScreen()->FixCaptivePortal();
  else
    dialog_->SetShouldDisplayCaptivePortal(true);
}

LoginDisplay* LoginDisplayHostMojo::GetLoginDisplay() {
  return login_display_.get();
}

ExistingUserController* LoginDisplayHostMojo::GetExistingUserController() {
  return existing_user_controller_.get();
}

gfx::NativeWindow LoginDisplayHostMojo::GetNativeWindow() const {
  // We can't access the login widget because it's in ash, return the native
  // window of the dialog widget if it exists.
  if (!dialog_)
    return nullptr;
  return dialog_->GetNativeWindow();
}

OobeUI* LoginDisplayHostMojo::GetOobeUI() const {
  if (!dialog_)
    return nullptr;
  return dialog_->GetOobeUI();
}

content::WebContents* LoginDisplayHostMojo::GetOobeWebContents() const {
  if (!dialog_)
    return nullptr;
  return dialog_->GetWebContents();
}

WebUILoginView* LoginDisplayHostMojo::GetWebUILoginView() const {
  NOTREACHED();
  return nullptr;
}

void LoginDisplayHostMojo::OnFinalize() {
  if (dialog_)
    dialog_->Close();

  ShutdownDisplayHost();
}

void LoginDisplayHostMojo::SetStatusAreaVisible(bool visible) {
  SystemTrayClientImpl::Get()->SetPrimaryTrayVisible(visible);
}

void LoginDisplayHostMojo::StartWizard(OobeScreenId first_screen) {
  EnsureOobeDialogLoaded();
  OobeUI* oobe_ui = GetOobeUI();
  DCHECK(oobe_ui);
  // Dialog is not shown immediately, and will be shown only when a screen
  // change occurs. This prevents the dialog from showing when there are no
  // screens to show.
  ObserveOobeUI();

  if (wizard_controller_->is_initialized()) {
    wizard_controller_->AdvanceToScreen(first_screen);
  } else {
    wizard_controller_->Init(first_screen);
    NotifyWizardCreated();
  }
}

WizardController* LoginDisplayHostMojo::GetWizardController() {
  EnsureOobeDialogLoaded();
  return wizard_controller_.get();
}

void LoginDisplayHostMojo::OnStartUserAdding() {
  VLOG(1) << "Login Mojo >> user adding";

  // Lock container can be transparent after lock screen animation.
  aura::Window* lock_container =
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_LockScreenContainersContainer);
  lock_container->layer()->SetOpacity(1.0);

  CreateExistingUserController();

  SetStatusAreaVisible(true);
  existing_user_controller_->Init(
      user_manager::UserManager::Get()->GetUsersAllowedForMultiProfile());
}

void LoginDisplayHostMojo::CancelUserAdding() {
  Finalize(base::OnceClosure());
}

void LoginDisplayHostMojo::OnStartSignInScreen() {
  // This function may be called early in startup flow, before
  // LoginScreenClientImpl has been initialized. Wait until
  // LoginScreenClientImpl is initialized as it is a common dependency.
  if (!LoginScreenClientImpl::HasInstance()) {
    // TODO(jdufault): Add a timeout here / make sure we do not post infinitely.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&LoginDisplayHostMojo::OnStartSignInScreen,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  if (signin_screen_started_) {
    // If we already have a signin screen instance, just reset the state of the
    // oobe dialog.

    // Try to switch to user creation screen.
    StartWizard(UserCreationView::kScreenId);

    if (wizard_controller_->current_screen() &&
        !WizardController::IsSigninScreen(
            wizard_controller_->current_screen()->screen_id())) {
      // Switching might fail due to the screen priorities. Do no hide the
      // dialog in that case.
      return;
    }

    // Maybe hide dialog if there are existing users. It also reloads Gaia.
    HideOobeDialog();

    return;
  }

  signin_screen_started_ = true;

  CreateExistingUserController();

  // Load the UI.
  existing_user_controller_->Init(user_manager::UserManager::Get()->GetUsers());

  user_selection_screen_->InitEasyUnlock();

  system_info_updater_->StartRequest();

  OnStartSignInScreenCommon();

  login::SecurityTokenSessionController::MaybeDisplayLoginScreenNotification();
}

void LoginDisplayHostMojo::OnPreferencesChanged() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::OnStartAppLaunch() {
  EnsureOobeDialogLoaded();

  ShowFullScreen();
}

void LoginDisplayHostMojo::OnBrowserCreated() {
  base::TimeTicks startup_time = startup_metric_utils::MainEntryPointTicks();
  if (startup_time.is_null())
    return;
  base::TimeDelta delta = base::TimeTicks::Now() - startup_time;
  UMA_HISTOGRAM_CUSTOM_TIMES("OOBE.BootToSignInCompleted", delta,
                             base::Milliseconds(10), base::Minutes(30), 100);
}

void LoginDisplayHostMojo::ShowGaiaDialog(const AccountId& prefilled_account) {
  EnsureOobeDialogLoaded();
  DCHECK(GetOobeUI());

  if (prefilled_account.is_valid()) {
    gaia_reauth_account_id_ = prefilled_account;
  } else {
    gaia_reauth_account_id_.reset();
  }
  ShowGaiaDialogCommon(prefilled_account);

  ShowDialog();
}

void LoginDisplayHostMojo::ShowOsInstallScreen() {
  StartWizard(OsInstallScreenView::kScreenId);
}

void LoginDisplayHostMojo::ShowGuestTosScreen() {
  StartWizard(GuestTosScreenView::kScreenId);
  ShowDialog();
}

void LoginDisplayHostMojo::HideOobeDialog() {
  DCHECK(dialog_);

  // The dialog can not be hidden if there are no users on the login screen.
  // Reload it instead.

  // As ShowDialogCommon will not reload GAIA upon show for performance reasons,
  // reload it to ensure that no state is persisted between hide and
  // subsequent show.
  const bool no_users =
      !login_display_->IsSigninInProgress() && user_count_ == 0;
  if (no_users || GetOobeUI()->current_screen() == GaiaView::kScreenId) {
    GaiaScreen* gaia_screen = GetWizardController()->GetScreen<GaiaScreen>();
    gaia_screen->LoadOnline(EmptyAccountId());
    if (no_users)
      return;
  }

  user_selection_screen_->OnBeforeShow();
  LoadWallpaper(focused_pod_account_id_);
  HideDialog();
}

void LoginDisplayHostMojo::SetShelfButtonsEnabled(bool enabled) {
  // Do nothing as we do not need to disable the shelf buttons on lock/login
  // screen.
}

void LoginDisplayHostMojo::UpdateOobeDialogState(OobeDialogState state) {
  if (dialog_)
    dialog_->SetState(state);
}

void LoginDisplayHostMojo::UpdateAddUserButtonStatus() {
  LoginScreen::Get()->EnableAddUserButton(!AllAllowlistedUsersPresent());
}

void LoginDisplayHostMojo::RequestSystemInfoUpdate() {
  system_info_updater_->StartRequest();
}

bool LoginDisplayHostMojo::HasUserPods() {
  return user_count_ > 0;
}

void LoginDisplayHostMojo::VerifyOwnerForKiosk(base::OnceClosure on_success) {
  // This UI is specific fo the consumer kiosk. We hide all the pods except for
  // the owner. User can't go back to the normal user screen from this. App
  // launch cancellation results in the Chrome restart (see
  // KioskLaunchController::OnCancelAppLaunch).
  CHECK(GetKioskLaunchController());
  DCHECK(!owner_verified_callback_);
  owner_verified_callback_ = std::move(on_success);
  owner_account_id_ = user_manager::UserManager::Get()->GetOwnerAccountId();
  CHECK(owner_account_id_.is_valid());
  login_display_->ShowOwnerPod(owner_account_id_);
  HideOobeDialog();
}

void LoginDisplayHostMojo::AddObserver(LoginDisplayHost::Observer* observer) {
  observers_.AddObserver(observer);
}

void LoginDisplayHostMojo::RemoveObserver(
    LoginDisplayHost::Observer* observer) {
  observers_.RemoveObserver(observer);
}

SigninUI* LoginDisplayHostMojo::GetSigninUI() {
  return this;
}

bool LoginDisplayHostMojo::IsWizardControllerCreated() const {
  return wizard_controller_.get();
}

void LoginDisplayHostMojo::OnCancelPasswordChangedFlow() {
  HideOobeDialog();
}

void LoginDisplayHostMojo::ShowEnableConsumerKioskScreen() {
  NOTREACHED();
}

bool LoginDisplayHostMojo::GetKeyboardRemappedPrefValue(
    const std::string& pref_name,
    int* value) const {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  return focused_pod_account_id_.is_valid() &&
         known_user.GetIntegerPref(focused_pod_account_id_, pref_name, value);
}

bool LoginDisplayHostMojo::IsWebUIStarted() const {
  return dialog_;
}

void LoginDisplayHostMojo::HandleAuthenticateUserWithPasswordOrPin(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_EQ(account_id.GetUserEmail(),
            gaia::SanitizeEmail(account_id.GetUserEmail()));

  CHECK(!pending_auth_state_);
  pending_auth_state_ =
      std::make_unique<AuthState>(account_id, std::move(callback));

  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  UserContext user_context(*user);
  user_context.SetIsUsingPin(authenticated_by_pin);
  user_context.SetKey(
      Key(chromeos::Key::KEY_TYPE_PASSWORD_PLAIN, "" /*salt*/, password));
  user_context.SetPasswordKey(Key(password));
  user_context.SetLoginInputMethodIdUsed(input_method::InputMethodManager::Get()
                                             ->GetActiveIMEState()
                                             ->GetCurrentInputMethod()
                                             .id());

  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY) {
    if (user_context.GetUserType() !=
        user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY) {
      LOG(FATAL) << "Incorrect Active Directory user type "
                 << user_context.GetUserType();
    }
    user_context.SetIsUsingOAuth(false);
  }

  if (owner_verified_callback_) {
    CheckOwnerCredentials(user_context);
    return;
  }

  existing_user_controller_->Login(user_context, SigninSpecifics());
}

void LoginDisplayHostMojo::HandleAuthenticateUserWithEasyUnlock(
    const AccountId& account_id) {
  user_selection_screen_->AttemptEasyUnlock(account_id);
}

void LoginDisplayHostMojo::HandleAuthenticateUserWithChallengeResponse(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  if (!ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id)) {
    LOG(ERROR)
        << "Challenge-response authentication isn't supported for the user";
    std::move(callback).Run(false);
    return;
  }

  challenge_response_auth_keys_loader_.LoadAvailableKeys(
      account_id,
      base::BindOnce(&LoginDisplayHostMojo::OnChallengeResponseKeysPrepared,
                     weak_factory_.GetWeakPtr(), account_id,
                     std::move(callback)));
}

void LoginDisplayHostMojo::HandleHardlockPod(const AccountId& account_id) {
  user_selection_screen_->HardLockPod(account_id);
}

void LoginDisplayHostMojo::HandleOnFocusPod(const AccountId& account_id) {
  user_selection_screen_->HandleFocusPod(account_id);
  WallpaperControllerClientImpl::Get()->ShowUserWallpaper(account_id);
  if (focused_pod_account_id_ != account_id)
    MaybeUpdateOfflineLoginLinkVisibility(account_id);
  focused_pod_account_id_ = account_id;
}

void LoginDisplayHostMojo::HandleOnNoPodFocused() {
  user_selection_screen_->HandleNoPodFocused();
  focused_pod_account_id_ = EmptyAccountId();
  ErrorScreen::AllowOfflineLoginPerUser(true);
}

bool LoginDisplayHostMojo::HandleFocusLockScreenApps(bool reverse) {
  NOTREACHED();
  return false;
}

void LoginDisplayHostMojo::HandleFocusOobeDialog() {
  if (!dialog_->IsVisible())
    return;

  dialog_->GetWebContents()->Focus();
}

void LoginDisplayHostMojo::HandleLaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT, account_id);
  context.SetPublicSessionLocale(locale);
  context.SetPublicSessionInputMethod(input_method);
  existing_user_controller_->Login(context, SigninSpecifics());
}

void LoginDisplayHostMojo::OnAuthFailure(const AuthFailure& error) {
  // OnAuthFailure and OnAuthSuccess can be called if an authentication attempt
  // is not initiated from mojo, ie, if LoginDisplay::Delegate::Login() is
  // called directly.
  if (pending_auth_state_) {
    login_display_->UpdatePinKeyboardState(pending_auth_state_->account_id);
    GetLoginScreenCertProviderService()
        ->AbortSignatureRequestsForAuthenticatingUser(
            pending_auth_state_->account_id);
    std::move(pending_auth_state_->callback).Run(false);
    pending_auth_state_.reset();
  }
}

void LoginDisplayHostMojo::OnAuthSuccess(const UserContext& user_context) {
  if (pending_auth_state_) {
    std::move(pending_auth_state_->callback).Run(true);
    pending_auth_state_.reset();
  }

  if (gaia_reauth_account_id_.has_value()) {
    SendReauthReason(gaia_reauth_account_id_.value(),
                     false /* password changed */);
    gaia_reauth_account_id_.reset();
  }
}

void LoginDisplayHostMojo::OnPasswordChangeDetected(
    const UserContext& user_context) {
  if (user_context.GetAccountId().is_valid()) {
    SendReauthReason(user_context.GetAccountId(), true /* password changed */);
  }
  gaia_reauth_account_id_.reset();
}

void LoginDisplayHostMojo::OnOldEncryptionDetected(
    const UserContext& user_context,
    bool has_incomplete_migration) {}

void LoginDisplayHostMojo::OnCurrentScreenChanged(OobeScreenId current_screen,
                                                  OobeScreenId new_screen) {
  DCHECK(dialog_);
  if (!dialog_->IsVisible())
    ShowDialog();
}

void LoginDisplayHostMojo::OnDestroyingOobeUI() {
  StopObservingOobeUI();
}

// views::ViewObserver:
void LoginDisplayHostMojo::OnViewBoundsChanged(views::View* observed_view) {
  DCHECK(scoped_observation_.IsObservingSource(observed_view));
  for (auto& observer : observers_)
    observer.WebDialogViewBoundsChanged(observed_view->GetBoundsInScreen());
}

void LoginDisplayHostMojo::OnViewIsDeleting(views::View* observed_view) {
  DCHECK(scoped_observation_.IsObservingSource(observed_view));
  scoped_observation_.Reset();
}

bool LoginDisplayHostMojo::IsOobeUIDialogVisible() const {
  return dialog_ && dialog_->IsVisible();
}

OobeUIDialogDelegate* LoginDisplayHostMojo::EnsureDialogForTest() {
  EnsureOobeDialogLoaded();
  return dialog_;
}

void LoginDisplayHostMojo::EnsureOobeDialogLoaded() {
  if (dialog_)
    return;

  dialog_ = new OobeUIDialogDelegate(weak_factory_.GetWeakPtr());
  dialog_->GetOobeUI()->signin_screen_handler()->SetDelegate(
      login_display_.get());

  views::View* web_dialog_view = dialog_->GetWebDialogView();
  scoped_observation_.Observe(web_dialog_view);

  // Should be created after dialog was created and OobeUI was loaded.
  wizard_controller_ = std::make_unique<WizardController>(GetWizardContext());

  GetLoginScreenCertProviderService()->pin_dialog_manager()->AddPinDialogHost(
      &security_token_pin_dialog_host_login_impl_);

  // Update status of add user button in the shelf.
  UpdateAddUserButtonStatus();
}

void LoginDisplayHostMojo::OnChallengeResponseKeysPrepared(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> on_auth_complete_callback,
    std::vector<ChallengeResponseKey> challenge_response_keys) {
  if (challenge_response_keys.empty()) {
    // TODO(crbug.com/826417): Indicate the error in the UI.
    std::move(on_auth_complete_callback).Run(false);
    return;
  }

  CHECK(!pending_auth_state_);
  pending_auth_state_ = std::make_unique<AuthState>(
      account_id, std::move(on_auth_complete_callback));

  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  UserContext user_context(*user);
  *user_context.GetMutableChallengeResponseKeys() =
      std::move(challenge_response_keys);

  existing_user_controller_->Login(user_context, SigninSpecifics());
}

void LoginDisplayHostMojo::ShowDialog() {
  EnsureOobeDialogLoaded();
  ObserveOobeUI();
  dialog_->Show();
}

void LoginDisplayHostMojo::ShowFullScreen() {
  EnsureOobeDialogLoaded();
  ObserveOobeUI();
  dialog_->ShowFullScreen();
}

void LoginDisplayHostMojo::HideDialog() {
  if (!dialog_)
    return;

  // Stop observing so that dialog will not be shown when a screen change
  // occurs. Screen changes can occur even when the dialog is not shown (e.g.
  // with hidden error screens).
  StopObservingOobeUI();
  dialog_->Hide();
  gaia_reauth_account_id_.reset();
}

void LoginDisplayHostMojo::ObserveOobeUI() {
  if (added_as_oobe_observer_)
    return;

  OobeUI* oobe_ui = GetOobeUI();
  if (!oobe_ui)
    return;

  oobe_ui->AddObserver(this);
  added_as_oobe_observer_ = true;
}

void LoginDisplayHostMojo::StopObservingOobeUI() {
  if (!added_as_oobe_observer_)
    return;

  added_as_oobe_observer_ = false;

  OobeUI* oobe_ui = GetOobeUI();
  if (oobe_ui)
    oobe_ui->RemoveObserver(this);
}

void LoginDisplayHostMojo::CreateExistingUserController() {
  existing_user_controller_ = std::make_unique<ExistingUserController>();
  login_display_->set_delegate(existing_user_controller_.get());

  // We need auth attempt results to notify views-based login screen.
  existing_user_controller_->AddLoginStatusConsumer(this);
}

void LoginDisplayHostMojo::CheckOwnerCredentials(
    const UserContext& user_context) {
  CHECK_EQ(owner_account_id_, user_context.GetAccountId());
  if (!extended_authenticator_)
    extended_authenticator_ = ExtendedAuthenticator::Create(this);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ExtendedAuthenticator::AuthenticateToCheck,
                     extended_authenticator_.get(), user_context,
                     base::BindOnce(&LoginDisplayHostMojo::OnOwnerSigninSuccess,
                                    base::Unretained(this))));
}

void LoginDisplayHostMojo::OnOwnerSigninSuccess() {
  DCHECK(owner_verified_callback_);
  std::move(owner_verified_callback_).Run();
  extended_authenticator_.reset();
  ShowFullScreen();
}

void LoginDisplayHostMojo::MaybeUpdateOfflineLoginLinkVisibility(
    const AccountId& account_id) {
  bool offline_limit_expired = false;

  const absl::optional<base::TimeDelta> offline_signin_interval =
      user_manager::known_user::GetOfflineSigninLimit(account_id);

  // Check if the limit is set only.
  if (offline_signin_interval) {
    const base::Time last_online_signin =
        user_manager::known_user::GetLastOnlineSignin(account_id);
    offline_limit_expired =
        login::TimeToOnlineSignIn(last_online_signin,
                                  offline_signin_interval.value()) <=
        base::TimeDelta();
  }

  ErrorScreen::AllowOfflineLoginPerUser(!offline_limit_expired);
}

}  // namespace ash
