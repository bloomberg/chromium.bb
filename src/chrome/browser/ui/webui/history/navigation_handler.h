// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_NAVIGATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_NAVIGATION_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace webui {

// A place to add handlers for messages shared across all WebUI pages.
class NavigationHandler : public content::WebUIMessageHandler {
 public:
  NavigationHandler();
  ~NavigationHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void HandleNavigateToUrl(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(NavigationHandler);
};

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_NAVIGATION_HANDLER_H_
