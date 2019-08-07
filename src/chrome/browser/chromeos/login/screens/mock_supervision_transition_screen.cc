// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_supervision_transition_screen.h"

namespace chromeos {

MockSupervisionTransitionScreen::MockSupervisionTransitionScreen(
    SupervisionTransitionScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : SupervisionTransitionScreen(view, exit_callback) {}

MockSupervisionTransitionScreen::~MockSupervisionTransitionScreen() = default;

void MockSupervisionTransitionScreen::ExitScreen() {
  exit_callback()->Run();
}

MockSupervisionTransitionScreenView::MockSupervisionTransitionScreenView() =
    default;

MockSupervisionTransitionScreenView::~MockSupervisionTransitionScreenView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockSupervisionTransitionScreenView::Bind(
    SupervisionTransitionScreen* screen) {
  screen_ = screen;
  MockBind(screen);
}

void MockSupervisionTransitionScreenView::Unbind() {
  screen_ = nullptr;
  MockUnbind();
}

}  // namespace chromeos
