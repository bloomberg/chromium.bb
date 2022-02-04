// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/guest_tos_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/chromeos/login/guest_tos_screen_handler.h"
#include "chrome/common/url_constants.h"

namespace ash {
namespace {

constexpr const char kUserActionBackClicked[] = "back-button";
constexpr const char kUserActionCancelClicked[] = "cancel";

std::string GetGoogleEulaOnlineUrl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeEulaUrlForTests)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kOobeEulaUrlForTests);
  }

  return base::StringPrintf(chrome::kGoogleEulaOnlineURLPath,
                            g_browser_process->GetApplicationLocale().c_str());
}

std::string GetCrosEulaOnlineUrl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeEulaUrlForTests)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kOobeEulaUrlForTests);
  }

  return base::StringPrintf(chrome::kCrosEulaOnlineURLPath,
                            g_browser_process->GetApplicationLocale().c_str());
}

}  // namespace

// static
std::string GuestTosScreen::GetResultString(Result result) {
  switch (result) {
    case Result::ACCEPT:
      return "Accept";
    case Result::BACK:
      return "Back";
    case Result::CANCEL:
      return "Cancel";
  }
}

GuestTosScreen::GuestTosScreen(GuestTosScreenView* view,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(GuestTosScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->Bind(this);
}

GuestTosScreen::~GuestTosScreen() {
  if (view_)
    view_->Unbind();
}

void GuestTosScreen::OnViewDestroyed(GuestTosScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void GuestTosScreen::ShowImpl() {
  if (!view_)
    return;
  view_->Show(GetGoogleEulaOnlineUrl(), GetCrosEulaOnlineUrl());
}

void GuestTosScreen::HideImpl() {}

void GuestTosScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionBackClicked) {
    exit_callback_.Run(Result::BACK);
  } else if (action_id == kUserActionCancelClicked) {
    exit_callback_.Run(Result::CANCEL);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void GuestTosScreen::OnAccept(bool enable_usage_stats) {
  // TODO(crbug.com/1241468): Handle usage stats reporting for guest session.
  exit_callback_.Run(Result::ACCEPT);
}
}  // namespace ash
