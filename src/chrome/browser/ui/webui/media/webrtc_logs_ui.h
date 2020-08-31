// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_WEBRTC_LOGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_WEBRTC_LOGS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

// The WebUI handler for chrome://webrtc-logs.
class WebRtcLogsUI : public content::WebUIController {
 public:
  explicit WebRtcLogsUI(content::WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcLogsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_WEBRTC_LOGS_UI_H_
