// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_USER_ACTIONS_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_USER_ACTIONS_HANDLER_H_

#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace base {
class TimeTicks;
}  // namespace base

// UI Handler for chrome://user-actions/
// It listens to user action notifications and passes those notifications
// into the Javascript to update the page.
class UserActionsHandler : public web::WebUIIOSMessageHandler {
 public:
  UserActionsHandler();
  ~UserActionsHandler() override;

  // WebUIIOSMessageHandler.
  void RegisterMessages() override;

 private:
  // Called whenever a user action is registered.
  void OnUserAction(const std::string& action, base::TimeTicks action_time);

  // The callback to invoke whenever a user action is registered.
  base::ActionCallback action_callback_;

  DISALLOW_COPY_AND_ASSIGN(UserActionsHandler);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_USER_ACTIONS_HANDLER_H_
