// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_PAGE_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_PAGE_UI_HANDLER_H_

#include "base/macros.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace settings {

// The base class handler of Javascript messages of settings pages.
class SettingsPageUIHandler : public content::WebUIMessageHandler {
 public:
  SettingsPageUIHandler();
  ~SettingsPageUIHandler() override;

 private:
  // SettingsPageUIHandler subclasses must be JavaScript-lifecycle safe.
  void OnJavascriptAllowed() override = 0;
  void OnJavascriptDisallowed() override = 0;

  DISALLOW_COPY_AND_ASSIGN(SettingsPageUIHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_PAGE_UI_HANDLER_H_
