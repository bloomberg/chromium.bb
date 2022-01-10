// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/os_trial_screen.h"

#include "chrome/browser/ui/webui/chromeos/login/os_trial_screen_handler.h"

namespace ash {

constexpr const char kUserActionTryNextClicked[] = "os-trial-try";
constexpr const char kUserActionInstallNextClicked[] = "os-trial-install";
constexpr const char kUserActionBackClicked[] = "os-trial-back";

// static
std::string OsTrialScreen::GetResultString(Result result) {
  switch (result) {
    case Result::NEXT_TRY:
      return "NextTry";
    case Result::NEXT_INSTALL:
      return "NextInstall";
    case Result::BACK:
      return "Back";
  }
}

OsTrialScreen::OsTrialScreen(OsTrialScreenView* view,
                             const ScreenExitCallback& exit_callback)
    : BaseScreen(OsTrialScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->Bind(this);
}

OsTrialScreen::~OsTrialScreen() {
  if (view_)
    view_->Unbind();
}

void OsTrialScreen::OnViewDestroyed(OsTrialScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void OsTrialScreen::ShowImpl() {
  if (!view_)
    return;

  view_->Show();
}

void OsTrialScreen::HideImpl() {}

void OsTrialScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionTryNextClicked) {
    exit_callback_.Run(Result::NEXT_TRY);
  } else if (action_id == kUserActionInstallNextClicked) {
    exit_callback_.Run(Result::NEXT_INSTALL);
  } else if (action_id == kUserActionBackClicked) {
    exit_callback_.Run(Result::BACK);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}
}  // namespace ash
