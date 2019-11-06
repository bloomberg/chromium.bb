// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NET_EXPORT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NET_EXPORT_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// The C++ back-end for the chrome://net-export webui page.
class NetExportUI : public content::WebUIController {
 public:
  explicit NetExportUI(content::WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetExportUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NET_EXPORT_UI_H_
