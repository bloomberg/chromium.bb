// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_INSPECT_INSPECT_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_INSPECT_INSPECT_UI_H_

#include "base/macros.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"

// The WebUI handler for chrome://inspect which displays JavaScript console
// messages.
class InspectUI : public web::WebUIIOSController {
 public:
  explicit InspectUI(web::WebUIIOS* web_ui);
  ~InspectUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InspectUI);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_INSPECT_INSPECT_UI_H_
