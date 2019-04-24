// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_VERSION_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_VERSION_UI_H_

#include <string>

#include "base/macros.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"

// The WebUI handler for chrome://version.
class VersionUI : public web::WebUIIOSController {
 public:
  explicit VersionUI(web::WebUIIOS* web_ui);
  ~VersionUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VersionUI);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_VERSION_UI_H_
