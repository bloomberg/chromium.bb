// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEVICE_LOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DEVICE_LOG_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace chromeos {

class DeviceLogUI : public content::WebUIController {
 public:
  explicit DeviceLogUI(content::WebUI* web_ui);
  ~DeviceLogUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceLogUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_DEVICE_LOG_UI_H_
