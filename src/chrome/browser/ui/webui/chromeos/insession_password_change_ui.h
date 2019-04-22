// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_INSESSION_PASSWORD_CHANGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_INSESSION_PASSWORD_CHANGE_UI_H_

#include "base/macros.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// For chrome:://password-change
class InSessionPasswordChangeUI : public ui::WebDialogUI {
 public:
  explicit InSessionPasswordChangeUI(content::WebUI* web_ui);
  ~InSessionPasswordChangeUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InSessionPasswordChangeUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_INSESSION_PASSWORD_CHANGE_UI_H_
