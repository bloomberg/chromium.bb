// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_USER_ACTIONS_USER_ACTIONS_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_USER_ACTIONS_USER_ACTIONS_UI_HANDLER_H_

#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "content/public/browser/web_ui_message_handler.h"

// UI Handler for chrome://user-actions/
// It listens to user action notifications and passes those notifications
// into the Javascript to update the page.
class UserActionsUIHandler : public content::WebUIMessageHandler {
 public:
  UserActionsUIHandler();
  ~UserActionsUIHandler() override;

  // WebUIMessageHandler implementation:
  // Does nothing for now.
  void RegisterMessages() override;

 private:
  void OnUserAction(const std::string& action);

  base::ActionCallback action_callback_;

  DISALLOW_COPY_AND_ASSIGN(UserActionsUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_USER_ACTIONS_USER_ACTIONS_UI_HANDLER_H_
