// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_UKM_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_UKM_INTERNALS_UI_H_

#include "base/macros.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

// The WebUI controller for chrome://ukm.
class UkmInternalsUI : public web::WebUIIOSController {
 public:
  explicit UkmInternalsUI(web::WebUIIOS* web_ui);
  ~UkmInternalsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UkmInternalsUI);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_UKM_INTERNALS_UI_H_
