// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/demo_preferences_screen.h"

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/ash/input_method_descriptor.h"

namespace ash {
namespace {

constexpr char kUserActionContinue[] = "continue-setup";
constexpr char kUserActionClose[] = "close-setup";
constexpr char kUserActionSetDemoModeCountry[] = "set-demo-mode-country";

}  // namespace

// static
std::string DemoPreferencesScreen::GetResultString(Result result) {
  switch (result) {
    case Result::COMPLETED:
      return "Completed";
    case Result::CANCELED:
      return "Canceled";
  }
}

DemoPreferencesScreen::DemoPreferencesScreen(
    base::WeakPtr<DemoPreferencesScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(DemoPreferencesScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  // TODO(agawronska): Add tests for locale and input changes.
  input_method::InputMethodManager* input_manager =
      input_method::InputMethodManager::Get();
  UpdateInputMethod(input_manager);
  input_manager_observation_.Observe(input_manager);
}

DemoPreferencesScreen::~DemoPreferencesScreen() {
  input_method::InputMethodManager::Get()->RemoveObserver(this);
}

void DemoPreferencesScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void DemoPreferencesScreen::HideImpl() {}

void DemoPreferencesScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionContinue) {
    std::string country(
        g_browser_process->local_state()->GetString(prefs::kDemoModeCountry));
    if (country == DemoSession::kCountryNotSelectedId) {
      return;
    }
    exit_callback_.Run(Result::COMPLETED);
  } else if (action_id == kUserActionClose) {
    exit_callback_.Run(Result::CANCELED);
  } else if (action_id == kUserActionSetDemoModeCountry) {
    CHECK_EQ(args.size(), 2);
    g_browser_process->local_state()->SetString(prefs::kDemoModeCountry,
                                                args[1].GetString());
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void DemoPreferencesScreen::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* profile,
    bool show_message) {
  UpdateInputMethod(manager);
}

void DemoPreferencesScreen::UpdateInputMethod(
    input_method::InputMethodManager* input_manager) {
  if (view_) {
    const input_method::InputMethodDescriptor input_method =
        input_manager->GetActiveIMEState()->GetCurrentInputMethod();
    view_->SetInputMethodId(input_method.id());
  }
}

}  // namespace ash
