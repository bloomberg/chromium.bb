// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_TERMS_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_TERMS_UI_H_

#include <string>

#include "base/macros.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"

// The WebUI handler for chrome://terms.
class TermsUI : public web::WebUIIOSController {
 public:
  TermsUI(web::WebUIIOS* web_ui, const std::string& name);
  ~TermsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TermsUI);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_TERMS_UI_H_
