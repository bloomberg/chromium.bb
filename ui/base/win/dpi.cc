// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/dpi.h"

#include <windows.h>

#include "base/win/scoped_hdc.h"

namespace {

int kDefaultDPIX = 96;
int kDefaultDPIY = 96;

}  // namespace

namespace ui {

gfx::Size GetDPI() {
  static int dpi_x = 0;
  static int dpi_y = 0;
  static bool should_initialize = true;

  if (should_initialize) {
    should_initialize = false;
    base::win::ScopedGetDC screen_dc(NULL);
    // This value is safe to cache for the life time of the app since the
    // user must logout to change the DPI setting. This value also applies
    // to all screens.
    dpi_x = GetDeviceCaps(screen_dc, LOGPIXELSX);
    dpi_y = GetDeviceCaps(screen_dc, LOGPIXELSY);
  }
  return gfx::Size(dpi_x, dpi_y);
}

float GetDPIScale() {
  return static_cast<float>(GetDPI().width()) /
      static_cast<float>(kDefaultDPIX);
}

bool IsInHighDPIMode() {
  gfx::Size dpi(GetDPI());
  return dpi.width() > kDefaultDPIX || dpi.height() > kDefaultDPIY;
}

void EnableHighDPISupport() {
  typedef BOOL(WINAPI *SetProcessDPIAwarePtr)(VOID);
  SetProcessDPIAwarePtr set_process_dpi_aware_func =
      reinterpret_cast<SetProcessDPIAwarePtr>(
          GetProcAddress(GetModuleHandleA("user32.dll"), "SetProcessDPIAware"));
  if (set_process_dpi_aware_func)
    set_process_dpi_aware_func();
}

}  // namespace ui
