// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/enable_debugging_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

EnableDebuggingScreen::EnableDebuggingScreen(
    BaseScreenDelegate* delegate,
    EnableDebuggingScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(delegate, OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->SetDelegate(this);
}

EnableDebuggingScreen::~EnableDebuggingScreen() {
  if (view_)
    view_->SetDelegate(NULL);
}

void EnableDebuggingScreen::Show() {
  if (view_)
    view_->Show();
}

void EnableDebuggingScreen::Hide() {
  if (view_)
    view_->Hide();
}

void EnableDebuggingScreen::OnExit(bool success) {
  exit_callback_.Run();
}

void EnableDebuggingScreen::OnViewDestroyed(EnableDebuggingScreenView* view) {
  if (view_ == view)
    view_ = NULL;
}

}  // namespace chromeos
