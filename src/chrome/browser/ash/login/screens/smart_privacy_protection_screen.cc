// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/smart_privacy_protection_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/smart_privacy_protection_screen_handler.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

constexpr const char kUserActionFeatureTurnOn[] = "continue-feature-on";
constexpr const char kUserActionFeatureTurnOff[] = "continue-feature-off";
constexpr const char kUserActionShowLearnMore[] = "show-learn-more";

}  // namespace

// static
std::string SmartPrivacyProtectionScreen::GetResultString(Result result) {
  switch (result) {
    case Result::PROCEED_WITH_FEATURE_ON:
      return "ContinueWithFeatureOn";
    case Result::PROCEED_WITH_FEATURE_OFF:
      return "ContinueWithFeatureOff";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

SmartPrivacyProtectionScreen::SmartPrivacyProtectionScreen(
    SmartPrivacyProtectionView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(SmartPrivacyProtectionView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view);
  if (view_)
    view_->Bind(this);
}

SmartPrivacyProtectionScreen::~SmartPrivacyProtectionScreen() {
  if (view_)
    view_->Unbind();
}

void SmartPrivacyProtectionScreen::OnViewDestroyed(
    SmartPrivacyProtectionView* view) {
  if (view_ == view)
    view_ = nullptr;
}

bool SmartPrivacyProtectionScreen::MaybeSkip(WizardContext* context) {
  // SmartPrivacyProtectionScreen lets user set two settings simultaneously:
  // SnoopingProtection and QuickDim. The screen should be skipped if none of
  // them is enabled.
  if (ash::features::IsSnoopingProtectionEnabled() ||
      ash::features::IsQuickDimEnabled()) {
    return false;
  }
  exit_callback_.Run(Result::NOT_APPLICABLE);
  return true;
}

void SmartPrivacyProtectionScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void SmartPrivacyProtectionScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void SmartPrivacyProtectionScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionFeatureTurnOn) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    profile->GetPrefs()->SetBoolean(
        prefs::kSnoopingProtectionEnabled,
        ash::features::IsSnoopingProtectionEnabled());
    profile->GetPrefs()->SetBoolean(prefs::kPowerQuickDimEnabled,
                                    ash::features::IsQuickDimEnabled());
    exit_callback_.Run(Result::PROCEED_WITH_FEATURE_ON);
  } else if (action_id == kUserActionFeatureTurnOff) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    profile->GetPrefs()->SetBoolean(prefs::kSnoopingProtectionEnabled, false);
    profile->GetPrefs()->SetBoolean(prefs::kPowerQuickDimEnabled, false);
    exit_callback_.Run(Result::PROCEED_WITH_FEATURE_OFF);
  } else if (action_id == kUserActionShowLearnMore) {
    // TODO(crbug.com/1293320): add p-link once available
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

}  // namespace ash
