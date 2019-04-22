// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/dip_px_util.h"

#include <cmath>

#include "base/numerics/safe_conversions.h"
#include "ui/base/layout.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

// TODO(crbug.com/826982): plumb through enough information to use one of
// Screen::GetDisplayNearest{Window/View/Point}. That way in multi-monitor
// setups where one screen is hidpi and the other one isn't, we don't always do
// the wrong thing.

namespace {

float GetPrimaryDisplayScaleFactor() {
  display::Screen* screen = display::Screen::GetScreen();
  if (!screen) {
    return 1.0f;
  }
  return screen->GetPrimaryDisplay().device_scale_factor();
}

}  // namespace

namespace apps_util {

int ConvertDipToPx(int dip) {
  return base::saturated_cast<int>(
      std::floor(static_cast<float>(dip) * GetPrimaryDisplayScaleFactor()));
}

int ConvertPxToDip(int px) {
  return base::saturated_cast<int>(
      std::floor(static_cast<float>(px) / GetPrimaryDisplayScaleFactor()));
}

ui::ScaleFactor GetPrimaryDisplayUIScaleFactor() {
  return ui::GetSupportedScaleFactor(GetPrimaryDisplayScaleFactor());
}

}  // namespace apps_util
