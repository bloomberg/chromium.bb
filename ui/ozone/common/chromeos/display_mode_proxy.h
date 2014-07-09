// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_CHROMEOS_DISPLAY_MODE_PROXY_H_
#define UI_OZONE_COMMON_CHROMEOS_DISPLAY_MODE_PROXY_H_

#include "ui/display/types/chromeos/display_mode.h"

namespace ui {

struct DisplayMode_Params;

class DisplayModeProxy : public DisplayMode {
 public:
  DisplayModeProxy(const DisplayMode_Params& params);
  virtual ~DisplayModeProxy();

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayModeProxy);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_CHROMEOS_DISPLAY_MODE_PROXY_H_
