// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/user_creation_screen.h"

#include "ash/public/cpp/login_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

namespace ash {
namespace {

constexpr char kUserActionSignIn[] = "signin";
constexpr char kUserActionChildSignIn[] = "child-signin";
constexpr char kUserActionChildAccountCreate[] = "child-account-create";
constexpr char kUserActionCancel[] = "cancel";

UserCreationScreen::UserCreationScreenExitTestDelegate* test_exit_delegate =
    nullptr;

}  // namespace

// static
std::string UserCreationScreen::GetResultString(Result result) {
  switch (result) {
    case Result::SIGNIN:
      return "SignIn";
    case Result::CHILD_SIGNIN:
      return "SignInAsChild";
    case Result::CHILD_ACCOUNT_CREATE:
      return "CreateChildAccount";
    case Result::ENTERPRISE_ENROLL:
      return "EnterpriseEnroll";
    case Result::CANCEL:
      return "Cancel";
    case Result::SKIPPED:
      return BaseScreen::kNotApplicable;
  }
}

UserCreationScreen::UserCreationScreen(UserCreationView* view,
                                       ErrorScreen* error_screen,
                                       const ScreenExitCallback& exit_callback)
    : BaseScreen(UserCreationView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      error_screen_(error_screen),
      exit_callback_(exit_callback) {
  network_state_informer_ = base::MakeRefCounted<NetworkStateInformer>();
  network_state_informer_->Init();
  if (view_)
    view_->Bind(this);
}

UserCreationScreen::~UserCreationScreen() {
  if (view_)
    view_->Unbind();
}

void UserCreationScreen::OnViewDestroyed(UserCreationView* view) {
  if (view_ == view)
    view_ = nullptr;
}

// static
void UserCreationScreen::SetUserCreationScreenExitTestDelegate(
    UserCreationScreen::UserCreationScreenExitTestDelegate* test_delegate) {
  test_exit_delegate = test_delegate;
}

bool UserCreationScreen::MaybeSkip(WizardContext* context) {
  if (g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->IsDeviceEnterpriseManaged() ||
      context->skip_to_login_for_tests) {
    context->is_user_creation_enabled = false;
    RunExitCallback(Result::SKIPPED);
    return true;
  }
  context->is_user_creation_enabled = true;
  return false;
}

void UserCreationScreen::ShowImpl() {
  if (!view_)
    return;

  scoped_observation_.Observe(network_state_informer_.get());

  LoginScreen::Get()->SetIsFirstSigninStep(true);

  // Back button is only available in login screen (add user flow) which is
  // indicated by if the device has users. Back button is hidden in the oobe
  // flow.
  view_->SetIsBackButtonVisible(
      LoginDisplayHost::default_host()->HasUserPods());

  UpdateState(NetworkError::ERROR_REASON_UPDATE);

  if (!error_screen_visible_)
    view_->Show();
}

void UserCreationScreen::HideImpl() {
  scoped_observation_.Reset();
  error_screen_visible_ = false;
  error_screen_->SetParentScreen(OobeScreen::SCREEN_UNKNOWN);
  error_screen_->Hide();
}

void UserCreationScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionSignIn) {
    context()->sign_in_as_child = false;
    RunExitCallback(Result::SIGNIN);
  } else if (action_id == kUserActionChildSignIn) {
    context()->sign_in_as_child = true;
    context()->is_child_gaia_account_new = false;
    RunExitCallback(Result::CHILD_SIGNIN);
  } else if (action_id == kUserActionChildAccountCreate) {
    context()->sign_in_as_child = true;
    context()->is_child_gaia_account_new = true;
    RunExitCallback(Result::CHILD_ACCOUNT_CREATE);
  } else if (action_id == kUserActionCancel) {
    context()->is_user_creation_enabled = false;
    RunExitCallback(Result::CANCEL);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

bool UserCreationScreen::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kStartEnrollment) {
    RunExitCallback(Result::ENTERPRISE_ENROLL);
    return true;
  }
  return false;
}

void UserCreationScreen::UpdateState(NetworkError::ErrorReason reason) {
  NetworkStateInformer::State state = network_state_informer_->state();
  const bool is_online = NetworkStateInformer::IsOnline(state, reason);
  if (!is_online) {
    error_screen_visible_ = true;
    error_screen_->SetParentScreen(UserCreationView::kScreenId);
    error_screen_->ShowNetworkErrorMessage(state, reason);
  } else {
    error_screen_->HideCaptivePortal();
    if (error_screen_visible_ &&
        error_screen_->GetParentScreen() == UserCreationView::kScreenId) {
      error_screen_visible_ = false;
      error_screen_->SetParentScreen(OobeScreen::SCREEN_UNKNOWN);
      error_screen_->Hide();
      view_->Show();
    }
  }
}

void UserCreationScreen::RunExitCallback(Result result) {
  if (test_exit_delegate) {
    test_exit_delegate->OnUserCreationScreenExit(result, exit_callback_);
  } else {
    exit_callback_.Run(result);
  }
}

}  // namespace ash
