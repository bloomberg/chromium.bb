// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_X11_TOUCHSCREEN_DEVICE_MANAGER_X11_H_
#define UI_DISPLAY_CHROMEOS_X11_TOUCHSCREEN_DEVICE_MANAGER_X11_H_

#include "base/macros.h"
#include "ui/display/types/chromeos/touchscreen_device_manager.h"

struct _XDisplay;
typedef struct _XDisplay Display;

namespace ui {

class TouchscreenDeviceManagerX11 : public TouchscreenDeviceManager {
 public:
  TouchscreenDeviceManagerX11();
  virtual ~TouchscreenDeviceManagerX11();

  // TouchscreenDeviceManager implementation:
  virtual std::vector<TouchscreenDevice> GetDevices() OVERRIDE;

 private:
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(TouchscreenDeviceManagerX11);
};

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_X11_TOUCHSCREEN_DEVICE_MANAGER_X11_H_
