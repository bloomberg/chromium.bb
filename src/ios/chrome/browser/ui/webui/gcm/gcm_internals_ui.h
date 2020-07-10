// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_GCM_GCM_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_GCM_GCM_INTERNALS_UI_H_

#include "base/macros.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

// The WebUIIOS for chrome://gcm-internals.
class GCMInternalsUI : public web::WebUIIOSController {
 public:
  explicit GCMInternalsUI(web::WebUIIOS* web_ui);
  ~GCMInternalsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMInternalsUI);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_GCM_GCM_INTERNALS_UI_H_
