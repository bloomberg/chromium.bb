// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_webui.h"

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_board_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

// LoginDisplayWebUI, public: --------------------------------------------------

LoginDisplayWebUI::~LoginDisplayWebUI() {
  if (webui_handler_)
    webui_handler_->ResetSigninScreenHandlerDelegate();
  ui::UserActivityDetector* activity_detector = ui::UserActivityDetector::Get();
  if (activity_detector && activity_detector->HasObserver(this))
    activity_detector->RemoveObserver(this);
}

// LoginDisplay implementation: ------------------------------------------------

LoginDisplayWebUI::LoginDisplayWebUI() = default;

void LoginDisplayWebUI::ClearAndEnablePassword() {
  if (webui_handler_)
    webui_handler_->ClearAndEnablePassword();
}

void LoginDisplayWebUI::Init(const user_manager::UserList& users,
                             bool show_guest,
                             bool show_users,
                             bool allow_new_user) {
  // Testing that the delegate has been set.
  DCHECK(delegate_);

  OobeUI* oobe_ui = LoginDisplayHost::default_host()->GetOobeUI();
  const std::string display_type = oobe_ui->display_type();
  if (display_type == OobeUI::kUserAddingDisplay && !user_selection_screen_) {
    user_selection_screen_ = std::make_unique<ChromeUserSelectionScreen>(
        DisplayedScreen::USER_ADDING_SCREEN);
    user_board_view_ = oobe_ui->GetView<UserBoardScreenHandler>()->GetWeakPtr();
    user_selection_screen_->SetView(user_board_view_.get());
    // TODO(jdufault): Bind and Unbind should be controlled by either the
    // Model/View which are then each responsible for automatically unbinding
    // the other associated View/Model instance. Then we can eliminate this
    // exposed WeakPtr logic. See crbug.com/685287.
    user_board_view_->Bind(user_selection_screen_.get());
    user_selection_screen_->Init(users);
  }
  show_users_changed_ = (show_users_ != show_users);
  show_users_ = show_users;
  allow_new_user_changed_ = (allow_new_user_ != allow_new_user);
  allow_new_user_ = allow_new_user;

  ui::UserActivityDetector* activity_detector = ui::UserActivityDetector::Get();
  if (activity_detector && !activity_detector->HasObserver(this))
    activity_detector->AddObserver(this);
}

// ---- Common methods

// ---- User selection screen methods

void LoginDisplayWebUI::HandleGetUsers() {
  if (user_selection_screen_)
    user_selection_screen_->HandleGetUsers();
}

void LoginDisplayWebUI::CheckUserStatus(const AccountId& account_id) {
  if (user_selection_screen_)
    user_selection_screen_->CheckUserStatus(account_id);
}

// ---- Gaia screen methods

// ---- Not yet classified methods

void LoginDisplayWebUI::OnPreferencesChanged() {
  if (webui_handler_)
    webui_handler_->OnPreferencesChanged();
}

void LoginDisplayWebUI::SetUIEnabled(bool is_enabled) {
  // TODO(nkostylev): Cleanup this condition,
  // see http://crbug.com/157885 and http://crbug.com/158255.
  // Allow this call only before user sign in or at lock screen.
  // If this call is made after new user signs in but login screen is still
  // around that would trigger a sign in extension refresh.
  if (is_enabled && (!user_manager::UserManager::Get()->IsUserLoggedIn() ||
                     ScreenLocker::default_screen_locker())) {
    ClearAndEnablePassword();
  }

  LoginDisplayHost* host = LoginDisplayHost::default_host();
  if (host && host->GetWebUILoginView())
    host->GetWebUILoginView()->SetUIEnabled(is_enabled);
}

void LoginDisplayWebUI::ShowError(int error_msg_id,
                                  int login_attempts,
                                  HelpAppLauncher::HelpTopic help_topic_id) {
  VLOG(1) << "Show error, error_id: " << error_msg_id
          << ", attempts:" << login_attempts
          << ", help_topic_id: " << help_topic_id;
  if (!webui_handler_)
    return;

  std::string error_text;
  switch (error_msg_id) {
    case IDS_LOGIN_ERROR_CAPTIVE_PORTAL:
      error_text = l10n_util::GetStringFUTF8(
          error_msg_id, delegate()->GetConnectedNetworkName());
      break;
    default:
      error_text = l10n_util::GetStringUTF8(error_msg_id);
      break;
  }

  // Only display hints about keyboard layout if the error is authentication-
  // related.
  if (error_msg_id != IDS_LOGIN_ERROR_ALLOWLIST &&
      error_msg_id != IDS_ENTERPRISE_LOGIN_ERROR_ALLOWLIST &&
      error_msg_id != IDS_ENTERPRISE_AND_FAMILY_LINK_LOGIN_ERROR_ALLOWLIST &&
      error_msg_id != IDS_LOGIN_ERROR_OWNER_KEY_LOST &&
      error_msg_id != IDS_LOGIN_ERROR_OWNER_REQUIRED &&
      error_msg_id != IDS_LOGIN_ERROR_GOOGLE_ACCOUNT_NOT_ALLOWED &&
      error_msg_id != IDS_LOGIN_ERROR_TPM_UPDATE_REQUIRED) {
    input_method::InputMethodManager* ime_manager =
        input_method::InputMethodManager::Get();

    // Display a hint to switch keyboards if there are other active input
    // methods.
    if (ime_manager->GetActiveIMEState()->GetNumActiveInputMethods() > 1) {
      error_text +=
          "\n" + l10n_util::GetStringUTF8(IDS_LOGIN_ERROR_KEYBOARD_SWITCH_HINT);
    }
  }

  std::string help_link;
  if (login_attempts > 1)
    help_link = l10n_util::GetStringUTF8(IDS_LEARN_MORE);

  webui_handler_->ShowError(login_attempts, error_text, help_link,
                            help_topic_id);
}

void LoginDisplayWebUI::ShowPasswordChangedDialog(bool show_password_error,
                                                  const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void LoginDisplayWebUI::ShowSigninUI(const std::string& email) {
  if (webui_handler_)
    webui_handler_->ShowSigninUI(email);
}

void LoginDisplayWebUI::ShowAllowlistCheckFailedError() {
  if (webui_handler_)
    webui_handler_->ShowAllowlistCheckFailedError();
}

// LoginDisplayWebUI, SigninScreenHandlerDelegate implementation: --------------
void LoginDisplayWebUI::CancelUserAdding() {
  if (!UserAddingScreen::Get()->IsRunning()) {
    LOG(ERROR) << "User adding screen not running.";
    return;
  }
  UserAddingScreen::Get()->Cancel();
}
void LoginDisplayWebUI::Login(const UserContext& user_context,
                              const SigninSpecifics& specifics) {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->Login(user_context, specifics);
}

void LoginDisplayWebUI::OnSigninScreenReady() {
  if (user_selection_screen_)
    user_selection_screen_->InitEasyUnlock();

  if (delegate_)
    delegate_->OnSigninScreenReady();
}

void LoginDisplayWebUI::ShowEnterpriseEnrollmentScreen() {
  if (delegate_)
    delegate_->OnStartEnterpriseEnrollment();
}

void LoginDisplayWebUI::ShowKioskEnableScreen() {
  if (delegate_)
    delegate_->OnStartKioskEnableScreen();
}

void LoginDisplayWebUI::ShowKioskAutolaunchScreen() {
  if (delegate_)
    delegate_->OnStartKioskAutolaunchScreen();
}

void LoginDisplayWebUI::ShowWrongHWIDScreen() {
  LoginDisplayHost::default_host()->StartWizard(WrongHWIDScreenView::kScreenId);
}

void LoginDisplayWebUI::SetWebUIHandler(
    LoginDisplayWebUIHandler* webui_handler) {
  webui_handler_ = webui_handler;
  if (user_selection_screen_)
    user_selection_screen_->SetHandler(webui_handler_);
}

bool LoginDisplayWebUI::IsShowUsers() const {
  return show_users_;
}

bool LoginDisplayWebUI::ShowUsersHasChanged() const {
  return show_users_changed_;
}

bool LoginDisplayWebUI::AllowNewUserChanged() const {
  return allow_new_user_changed_;
}

bool LoginDisplayWebUI::IsSigninInProgress() const {
  return delegate_->IsSigninInProgress();
}

bool LoginDisplayWebUI::IsUserSigninCompleted() const {
  return is_signin_completed();
}

void LoginDisplayWebUI::OnUserActivity(const ui::Event* event) {
  if (delegate_)
    delegate_->ResetAutoLoginTimer();
}

}  // namespace chromeos
