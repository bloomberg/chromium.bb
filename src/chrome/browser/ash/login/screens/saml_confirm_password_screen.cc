// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/saml_confirm_password_screen.h"
#include "ash/components/login/auth/cryptohome_key_constants.h"
#include "base/containers/contains.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/saml_confirm_password_handler.h"

namespace ash {

// static
std::string SamlConfirmPasswordScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kCancel:
      return "Cancel";
    case Result::kTooManyAttempts:
      return "TooManyAttempts";
  }
}

SamlConfirmPasswordScreen::SamlConfirmPasswordScreen(
    base::WeakPtr<SamlConfirmPasswordView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(SamlConfirmPasswordView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

SamlConfirmPasswordScreen::~SamlConfirmPasswordScreen() = default;

void SamlConfirmPasswordScreen::SetContextAndPasswords(
    std::unique_ptr<UserContext> user_context,
    ::login::StringList scraped_saml_passwords) {
  user_context_ = std::move(user_context);
  scraped_saml_passwords_ = std::move(scraped_saml_passwords);
  DCHECK_NE(scraped_saml_passwords_.size(), 1);
}

void SamlConfirmPasswordScreen::TryPassword(const std::string& password) {
  Key key(password);
  key.SetLabel(kCryptohomeGaiaKeyLabel);
  if (scraped_saml_passwords_.empty() ||
      base::Contains(scraped_saml_passwords_, password)) {
    user_context_->SetKey(key);
    user_context_->SetPasswordKey(Key(password));
    LoginDisplayHost::default_host()->CompleteLogin(*user_context_);

    user_context_.reset();
    scraped_saml_passwords_.clear();
    return;
  }

  if (++attempt_count_ >= 2) {
    user_context_.reset();
    scraped_saml_passwords_.clear();
    exit_callback_.Run(Result::kTooManyAttempts);
    return;
  }

  if (!view_)
    return;
  view_->Retry();
}

void SamlConfirmPasswordScreen::ShowImpl() {
  if (!view_)
    return;
  view_->Show(user_context_->GetAccountId().GetUserEmail(),
              scraped_saml_passwords_.empty());
}

void SamlConfirmPasswordScreen::HideImpl() {}

void SamlConfirmPasswordScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == "inputPassword") {
    CHECK_EQ(args.size(), 2);
    TryPassword(args[1].GetString());
    return;
  }

  if (action_id == "cancel") {
    exit_callback_.Run(Result::kCancel);
    return;
  }

  BaseScreen::OnUserAction(args);
}

}  // namespace ash
