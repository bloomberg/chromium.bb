// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_USER_ACTIONS_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_USER_ACTIONS_UI_H_

#include "base/macros.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"

// The UI for chrome://user-actions/
class UserActionsUI : public web::WebUIIOSController {
 public:
  explicit UserActionsUI(web::WebUIIOS* contents);
  ~UserActionsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserActionsUI);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_USER_ACTIONS_UI_H_
