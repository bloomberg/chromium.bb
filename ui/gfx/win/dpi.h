// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_DPI_WIN_H_
#define UI_GFX_DPI_WIN_H_

#include "ui/gfx/gfx_export.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace gfx {

// Initialization of the scale factor that should be applied for rendering
// in this process. Must be called before attempts to call any of the getter
// methods below in this file, e.g. in the early toolkit/resource bundle setup.
// This can be called multiple times during various tests, but subsequent calls
// have no effect.
GFX_EXPORT void InitDeviceScaleFactor(float scale);

GFX_EXPORT Size GetDPI();

// Gets the scale factor of the display. For example, if the display DPI is
// 96 then the scale factor is 1.0.
GFX_EXPORT float GetDPIScale();

// Tests to see if the command line flag "--high-dpi-support" is set.
GFX_EXPORT bool IsHighDPIEnabled();

GFX_EXPORT bool IsInHighDPIMode();

GFX_EXPORT void EnableHighDPISupport();

// TODO(kevers|girard):  Move above methods into win namespace.

namespace win {

GFX_EXPORT float GetDeviceScaleFactor();

GFX_EXPORT Point ScreenToDIPPoint(const Point& pixel_point);

GFX_EXPORT Point DIPToScreenPoint(const Point& dip_point);

GFX_EXPORT Rect ScreenToDIPRect(const Rect& pixel_bounds);

GFX_EXPORT Rect DIPToScreenRect(const Rect& dip_bounds);

GFX_EXPORT Size ScreenToDIPSize(const Size& size_in_pixels);

GFX_EXPORT Size DIPToScreenSize(const Size& dip_size);

// Win32's GetSystemMetrics uses pixel measures. This function calls
// GetSystemMetrics for the given |metric|, then converts the result to DIP.
GFX_EXPORT int GetSystemMetricsInDIP(int metric);

// Sometimes the OS secretly scales apps that are not DPIAware. This is not
// visible through standard OS calls like GetWindowPos(), or through
// GetDPIScale().
// Returns the scale factor of the display, where 96 DPI is 1.0.
// (Avoid this function... use GetDPIScale() instead.)
// TODO(girard): Remove this once DPIAware is enabled - http://crbug.com/149881
GFX_EXPORT double GetUndocumentedDPIScale();

// Win7 and Win8 send touch events scaled according to the current DPI
// scaling. Win8.1 corrects this, and sends touch events in DPI units.
// This function returns the appropriate scaling factor for touch events.
GFX_EXPORT double GetUndocumentedDPITouchScale();

}  // namespace win
}  // namespace gfx

#endif  // UI_GFX_DPI_WIN_H_
