// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// The WebUI handler for chrome://device-emulator
class DeviceEmulatorUI : public content::WebUIController {
 public:
  explicit DeviceEmulatorUI(content::WebUI* web_ui);
  ~DeviceEmulatorUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceEmulatorUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_UI_H_
