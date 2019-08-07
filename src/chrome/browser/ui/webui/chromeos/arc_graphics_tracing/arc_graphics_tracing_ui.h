// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}

namespace chromeos {

// WebUI controller for arc graphics tracing.
class ArcGraphicsTracingUI : public content::WebUIController {
 public:
  explicit ArcGraphicsTracingUI(content::WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcGraphicsTracingUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_UI_H_
