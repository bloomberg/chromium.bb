// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SET_TIME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SET_TIME_UI_H_

#include "base/macros.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// The WebUI for chrome://set-time.
class SetTimeUI : public ui::WebDialogUI {
 public:
  explicit SetTimeUI(content::WebUI* web_ui);
  ~SetTimeUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SetTimeUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SET_TIME_UI_H_
