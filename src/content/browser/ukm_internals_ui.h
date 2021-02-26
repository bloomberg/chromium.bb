// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_UKM_INTERNALS_UI_H_
#define CONTENT_BROWSER_UKM_INTERNALS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {

// Handles serving the chrome://ukm HTML and JS.
class UkmInternalsUI : public WebUIController {
 public:
  explicit UkmInternalsUI(WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(UkmInternalsUI);
};

}  // namespace content

#endif  // CONTENT_BROWSER_UKM_INTERNALS_UI_H_
