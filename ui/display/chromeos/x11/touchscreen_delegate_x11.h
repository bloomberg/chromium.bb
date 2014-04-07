// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_X11_TOUCHSCREEN_DELEGATE_X11_H_
#define UI_DISPLAY_CHROMEOS_X11_TOUCHSCREEN_DELEGATE_X11_H_

#include "ui/display/chromeos/display_configurator.h"

struct _XDisplay;
typedef struct _XDisplay Display;

namespace ui {

class TouchscreenDelegateX11 : public DisplayConfigurator::TouchscreenDelegate {
 public:
  TouchscreenDelegateX11();
  virtual ~TouchscreenDelegateX11();

  // DisplayConfigurator::TouchscreenDelegate implementation:
  virtual void AssociateTouchscreens(
      DisplayConfigurator::DisplayStateList* outputs) OVERRIDE;
  virtual void ConfigureCTM(
      int touch_device_id,
      const DisplayConfigurator::CoordinateTransformation& ctm) OVERRIDE;

 private:
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(TouchscreenDelegateX11);
};

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_X11_TOUCHSCREEN_DELEGATE_X11_H_
