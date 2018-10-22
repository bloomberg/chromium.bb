// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FLASH_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FLASH_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

// The Web UI handler for about:flash.
class FlashUI : public content::WebUIController {
 public:
  explicit FlashUI(content::WebUI* web_ui);

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

 private:
  DISALLOW_COPY_AND_ASSIGN(FlashUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FLASH_UI_H_
