// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/dpi.h"

#include <windows.h>
#include "base/command_line.h"
#include "base/win/scoped_hdc.h"
#include "base/win/windows_version.h"
#include "base/win/registry.h"
#include "ui/gfx/display.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace {

int kDefaultDPIX = 96;
int kDefaultDPIY = 96;

BOOL IsProcessDPIAwareWrapper() {
  typedef BOOL(WINAPI *IsProcessDPIAwarePtr)(VOID);
  IsProcessDPIAwarePtr is_process_dpi_aware_func =
      reinterpret_cast<IsProcessDPIAwarePtr>(
          GetProcAddress(GetModuleHandleA("user32.dll"), "IsProcessDPIAware"));
  if (is_process_dpi_aware_func)
    return is_process_dpi_aware_func();
  return FALSE;
}

float g_device_scale_factor = 0.0f;

}  // namespace

namespace gfx {

void InitDeviceScaleFactor(float scale) {
  DCHECK_NE(0.0f, scale);
  g_device_scale_factor = scale;
}

Size GetDPI() {
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
  return Size(dpi_x, dpi_y);
}

float GetDPIScale() {
  if (IsHighDPIEnabled()) {
    return static_cast<float>(GetDPI().width()) /
        static_cast<float>(kDefaultDPIX);
  }
  return 1.0;
}

bool IsHighDPIEnabled() {
  // Default is disabled.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kHighDPISupport)) {
    return CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kHighDPISupport).compare("1") == 0;
  }
  return false;
}

bool IsInHighDPIMode() {
  return GetDPIScale() > 1.0;
}

void EnableHighDPISupport() {
  if (IsHighDPIEnabled()) {
    typedef BOOL(WINAPI *SetProcessDPIAwarePtr)(VOID);
    SetProcessDPIAwarePtr set_process_dpi_aware_func =
        reinterpret_cast<SetProcessDPIAwarePtr>(
            GetProcAddress(GetModuleHandleA("user32.dll"),
                           "SetProcessDPIAware"));
    if (set_process_dpi_aware_func)
      set_process_dpi_aware_func();
  }
}

namespace win {

float GetDeviceScaleFactor() {
  DCHECK_NE(0.0f, g_device_scale_factor);
  return g_device_scale_factor;
}

Point ScreenToDIPPoint(const Point& pixel_point) {
  return ToFlooredPoint(ScalePoint(pixel_point, 1.0f / GetDeviceScaleFactor()));
}

Point DIPToScreenPoint(const Point& dip_point) {
  return ToFlooredPoint(ScalePoint(dip_point, GetDeviceScaleFactor()));
}

Rect ScreenToDIPRect(const Rect& pixel_bounds) {
  // TODO(kevers): Switch to non-deprecated method for float to int conversions.
  return ToFlooredRectDeprecated(
      ScaleRect(pixel_bounds, 1.0f / GetDeviceScaleFactor()));
}

Rect DIPToScreenRect(const Rect& dip_bounds) {
  // TODO(kevers): Switch to non-deprecated method for float to int conversions.
  return ToFlooredRectDeprecated(
      ScaleRect(dip_bounds, GetDeviceScaleFactor()));
}

Size ScreenToDIPSize(const Size& size_in_pixels) {
  return ToFlooredSize(
      ScaleSize(size_in_pixels, 1.0f / GetDeviceScaleFactor()));
}

Size DIPToScreenSize(const Size& dip_size) {
  return ToFlooredSize(ScaleSize(dip_size, GetDeviceScaleFactor()));
}

int GetSystemMetricsInDIP(int metric) {
  return static_cast<int>(GetSystemMetrics(metric) /
      GetDeviceScaleFactor() + 0.5);
}

double GetUndocumentedDPIScale() {
  // TODO(girard): Remove this code when chrome is DPIAware.
  static double scale = -1.0;
  if (scale == -1.0) {
    scale = 1.0;
    if (!IsProcessDPIAwareWrapper()) {
      base::win::RegKey key(HKEY_CURRENT_USER,
                            L"Control Panel\\Desktop\\WindowMetrics",
                            KEY_QUERY_VALUE);
      if (key.Valid()) {
        DWORD value = 0;
        if (key.ReadValueDW(L"AppliedDPI", &value) == ERROR_SUCCESS) {
          scale = static_cast<double>(value) / kDefaultDPIX;
        }
      }
    }
  }
  return scale;
}


double GetUndocumentedDPITouchScale() {
  static double scale =
      (base::win::GetVersion() < base::win::VERSION_WIN8_1) ?
      GetUndocumentedDPIScale() : 1.0;
  return scale;
}


}  // namespace win
}  // namespace gfx
