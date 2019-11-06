// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEVTOOLS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DEVTOOLS_UI_H_

#include "base/macros.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "content/public/browser/web_ui_controller.h"

class DevToolsUI : public content::WebUIController {
 public:
  static GURL GetProxyURL(const std::string& frontend_url);
  static GURL GetRemoteBaseURL();
  static bool IsFrontendResourceURL(const GURL& url);

  explicit DevToolsUI(content::WebUI* web_ui);
  ~DevToolsUI() override;

 private:
  DevToolsUIBindings bindings_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEVTOOLS_UI_H_
