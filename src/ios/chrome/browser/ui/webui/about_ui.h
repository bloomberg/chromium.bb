// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_ABOUT_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_ABOUT_UI_H_

#include <string>

#include "base/macros.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

// The WebUI controller for chrome://chrome-urls, chrome://histograms,
// and chrome://credits.
class AboutUI : public web::WebUIIOSController {
 public:
  explicit AboutUI(web::WebUIIOS* web_ui, const std::string& name);
  ~AboutUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AboutUI);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_ABOUT_UI_H_
