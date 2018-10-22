// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/marketing_opt_in_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/marketing_opt_in_screen_view.h"

namespace chromeos {

MarketingOptInScreen::MarketingOptInScreen(
    BaseScreenDelegate* base_screen_delegate,
    MarketingOptInScreenView* view)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_MARKETING_OPT_IN),
      view_(view) {
  DCHECK(view_);
  view_->Bind(this);
}

MarketingOptInScreen::~MarketingOptInScreen() {
  view_->Bind(nullptr);
}

void MarketingOptInScreen::Show() {
  view_->Show();
}

void MarketingOptInScreen::Hide() {
  view_->Hide();
}
void MarketingOptInScreen::OnAllSet(bool play_communications_opt_in,
                                    bool tips_communications_opt_in) {
  // TODO(https://crbug.com/852557)
  Finish(ScreenExitCode::MARKETING_OPT_IN_FINISHED);
}

}  // namespace chromeos
