// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_UKM_UKM_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_UKM_UKM_INTERNALS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// Handles serving the chrome://ukm HTML and JS.
class UkmInternalsUI : public content::WebUIController {
 public:
  explicit UkmInternalsUI(content::WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(UkmInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_UKM_UKM_INTERNALS_UI_H_
