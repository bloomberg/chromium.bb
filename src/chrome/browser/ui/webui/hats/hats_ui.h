// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HATS_HATS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HATS_HATS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}

// The WebUI for chrome://hats
class HatsUI : public content::WebUIController {
 public:
  explicit HatsUI(content::WebUI* web_ui);
  ~HatsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HatsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HATS_HATS_UI_H_
