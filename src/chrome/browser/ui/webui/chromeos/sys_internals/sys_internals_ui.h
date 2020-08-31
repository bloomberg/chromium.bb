// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYS_INTERNALS_SYS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYS_INTERNALS_SYS_INTERNALS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// The UI controller for SysInternals page.
class SysInternalsUI : public content::WebUIController {
 public:
  explicit SysInternalsUI(content::WebUI* web_ui);
  ~SysInternalsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SysInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYS_INTERNALS_SYS_INTERNALS_UI_H_
