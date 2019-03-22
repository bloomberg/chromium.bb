// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/assistant_optin_flow_screen.h"

#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/login/screens/assistant_optin_flow_screen_view.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/chromeos_switches.h"

namespace chromeos {
namespace {

constexpr const char kFlowFinished[] = "flow-finished";

}  // namespace

AssistantOptInFlowScreen::AssistantOptInFlowScreen(
    BaseScreenDelegate* base_screen_delegate,
    AssistantOptInFlowScreenView* view)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_ASSISTANT_OPTIN_FLOW),
      view_(view) {
  DCHECK(view_);
  if (view_)
    view_->Bind(this);
}

AssistantOptInFlowScreen::~AssistantOptInFlowScreen() {
  if (view_)
    view_->Unbind();
}

void AssistantOptInFlowScreen::Show() {
  if (!view_)
    return;

#if BUILDFLAG(ENABLE_CROS_ASSISTANT)
  if (chromeos::switches::IsAssistantEnabled() &&
      arc::IsAssistantAllowedForProfile(
          ProfileManager::GetActiveUserProfile()) ==
          ash::mojom::AssistantAllowedState::ALLOWED) {
    view_->Show();
    return;
  }
#endif
  Finish(ScreenExitCode::ASSISTANT_OPTIN_FLOW_FINISHED);
}

void AssistantOptInFlowScreen::Hide() {
  if (view_)
    view_->Hide();
}

void AssistantOptInFlowScreen::OnViewDestroyed(
    AssistantOptInFlowScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void AssistantOptInFlowScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kFlowFinished)
    Finish(ScreenExitCode::ASSISTANT_OPTIN_FLOW_FINISHED);
  else
    BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos
