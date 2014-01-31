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

float GetUnforcedDeviceScaleFactor() {
  return static_cast<float>(gfx::GetDPI().width()) /
      static_cast<float>(kDefaultDPIX);
}

float GetModernUIScaleWrapper() {
  float result = 1.0f;
  typedef float(WINAPI *GetModernUIScalePtr)(VOID);
  HMODULE lib = LoadLibraryA("metro_driver.dll");
  if (lib) {
    GetModernUIScalePtr func =
        reinterpret_cast<GetModernUIScalePtr>(
        GetProcAddress(lib, "GetModernUIScale"));
    if (func)
      result = func();
    FreeLibrary(lib);
  }
  return result;
}

// Duplicated from Win8.1 SDK ShellScalingApi.h
typedef enum PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;

typedef enum MONITOR_DPI_TYPE {
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;

// Win8.1 supports monitor-specific DPI scaling.
bool SetProcessDpiAwarenessWrapper(PROCESS_DPI_AWARENESS value) {
  typedef BOOL(WINAPI *SetProcessDpiAwarenessPtr)(PROCESS_DPI_AWARENESS);
  SetProcessDpiAwarenessPtr set_process_dpi_awareness_func =
      reinterpret_cast<SetProcessDpiAwarenessPtr>(
          GetProcAddress(GetModuleHandleA("user32.dll"),
                          "SetProcessDpiAwarenessInternal"));
  if (set_process_dpi_awareness_func) {
    HRESULT hr = set_process_dpi_awareness_func(value);
    if (SUCCEEDED(hr)) {
      VLOG(1) << "SetProcessDpiAwareness succeeded.";
      return true;
    } else if (hr == E_ACCESSDENIED) {
      LOG(ERROR) << "Access denied error from SetProcessDpiAwareness. "
          "Function called twice, or manifest was used.";
    }
  }
  return false;
}

// This function works for Windows Vista through Win8. Win8.1 must use
// SetProcessDpiAwareness[Wrapper]
BOOL SetProcessDPIAwareWrapper() {
  typedef BOOL(WINAPI *SetProcessDPIAwarePtr)(VOID);
  SetProcessDPIAwarePtr set_process_dpi_aware_func =
      reinterpret_cast<SetProcessDPIAwarePtr>(
      GetProcAddress(GetModuleHandleA("user32.dll"),
                      "SetProcessDPIAware"));
  return set_process_dpi_aware_func &&
    set_process_dpi_aware_func();
}

}  // namespace

namespace gfx {

float GetModernUIScale() {
  return GetModernUIScaleWrapper();
}

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
    return gfx::Display::HasForceDeviceScaleFactor() ?
        gfx::Display::GetForcedDeviceScaleFactor() :
        GetUnforcedDeviceScaleFactor();
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
  if (IsHighDPIEnabled() &&
      (base::win::GetVersion() < base::win::VERSION_WIN8_1)) {
    if (!SetProcessDpiAwarenessWrapper(PROCESS_SYSTEM_DPI_AWARE)) {
      SetProcessDPIAwareWrapper();
    }
  }
}

namespace win {

float GetDeviceScaleFactor() {
  DCHECK_NE(0.0f, g_device_scale_factor);
  return g_device_scale_factor;
}

Point ScreenToDIPPoint(const Point& pixel_point) {
  static float scaling_factor =
      GetDeviceScaleFactor() > GetUnforcedDeviceScaleFactor() ?
      1.0f / GetDeviceScaleFactor() :
      1.0f;
  return ToFlooredPoint(ScalePoint(pixel_point,
      scaling_factor));
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
