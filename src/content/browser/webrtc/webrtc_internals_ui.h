// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_INTERNALS_UI_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_INTERNALS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {

// The implementation for the chrome://webrtc-internals page.
class WebRTCInternalsUI : public WebUIController {
 public:
  explicit WebRTCInternalsUI(WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRTCInternalsUI);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_INTERNALS_UI_H_
