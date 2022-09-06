// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_CAPTIVE_PORTAL_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_CAPTIVE_PORTAL_DIALOG_H_

#include <string>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/base_lock_dialog.h"
#include "ui/views/widget/widget.h"

class Profile;

namespace chromeos {

class LockScreenCaptivePortalDialog : public BaseLockDialog {
 public:
  LockScreenCaptivePortalDialog();
  LockScreenCaptivePortalDialog(LockScreenCaptivePortalDialog const&) = delete;
  ~LockScreenCaptivePortalDialog() override;

  void Show(Profile& profile);
  void Dismiss();
  void OnDialogClosed(const std::string& json_retval) override;
  bool IsRunning() const;

  // Used for waiting for the dialog to be closed or shown in tests.
  bool IsDialogClosedForTesting(base::OnceClosure callback);
  bool IsDialogShownForTesting(base::OnceClosure callback);

 private:
  bool is_running_ = false;

  base::OnceClosure on_closed_callback_for_testing_;
  base::OnceClosure on_shown_callback_for_testing_;

  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_CAPTIVE_PORTAL_DIALOG_H_
