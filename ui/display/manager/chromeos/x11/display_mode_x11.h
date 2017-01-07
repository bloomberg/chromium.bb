// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CHROMEOS_X11_DISPLAY_MODE_X11_H_
#define UI_DISPLAY_MANAGER_CHROMEOS_X11_DISPLAY_MODE_X11_H_

#include "base/macros.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_mode.h"

// Forward declare from Xlib and Xrandr.
typedef unsigned long XID;
typedef XID RRMode;

namespace display {

class DISPLAY_MANAGER_EXPORT DisplayModeX11 : public DisplayMode {
 public:
  DisplayModeX11(const gfx::Size& size,
                 bool interlaced,
                 float refresh_rate,
                 RRMode mode_id);
  ~DisplayModeX11() override;
  std::unique_ptr<DisplayMode> Clone() const override;

  RRMode mode_id() const { return mode_id_; }

 private:
  RRMode mode_id_;

  DISALLOW_COPY_AND_ASSIGN(DisplayModeX11);
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CHROMEOS_X11_DISPLAY_MODE_X11_H_
