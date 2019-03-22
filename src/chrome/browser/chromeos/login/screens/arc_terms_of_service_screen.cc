// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen.h"

#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen_view.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/screen_exit_code.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr char kUserActionBack[] = "go-back";

}  // namespace

namespace chromeos {

// static
void ArcTermsOfServiceScreen::MaybeLaunchArcSettings(Profile* profile) {
  if (profile->GetPrefs()->GetBoolean(prefs::kShowArcSettingsOnSessionStart)) {
    profile->GetPrefs()->ClearPref(prefs::kShowArcSettingsOnSessionStart);
    // TODO(jhorwich) Handle the case where the user chooses to review both ARC
    // settings and sync settings - currently the Settings window will only
    // show one settings page. See crbug.com/901184#c4 for details.
    chrome::ShowSettingsSubPageForProfile(profile, "androidApps/details");
  }
}

ArcTermsOfServiceScreen::ArcTermsOfServiceScreen(
    BaseScreenDelegate* base_screen_delegate,
    ArcTermsOfServiceScreenView* view)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE),
      view_(view) {
  DCHECK(view_);
  if (view_) {
    view_->AddObserver(this);
    view_->Bind(this);
  }
}

ArcTermsOfServiceScreen::~ArcTermsOfServiceScreen() {
  if (view_) {
    view_->RemoveObserver(this);
    view_->Bind(nullptr);
  }
}

void ArcTermsOfServiceScreen::Show() {
  if (!view_)
    return;

  // Show the screen.
  view_->Show();
}

void ArcTermsOfServiceScreen::Hide() {
  if (view_)
    view_->Hide();
}

void ArcTermsOfServiceScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionBack) {
    Finish(ScreenExitCode::ARC_TERMS_OF_SERVICE_BACK);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void ArcTermsOfServiceScreen::OnSkip() {
  Finish(ScreenExitCode::ARC_TERMS_OF_SERVICE_SKIPPED);
}

void ArcTermsOfServiceScreen::OnAccept(bool review_arc_settings) {
  if (review_arc_settings) {
    Profile* const profile = ProfileManager::GetActiveUserProfile();
    CHECK(profile);
    profile->GetPrefs()->SetBoolean(prefs::kShowArcSettingsOnSessionStart,
                                    true);
  }
  Finish(ScreenExitCode::ARC_TERMS_OF_SERVICE_ACCEPTED);
}

void ArcTermsOfServiceScreen::OnViewDestroyed(
    ArcTermsOfServiceScreenView* view) {
  DCHECK_EQ(view, view_);
  view_->RemoveObserver(this);
  view_ = nullptr;
}

}  // namespace chromeos
