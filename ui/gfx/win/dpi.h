// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_DPI_WIN_H_
#define UI_GFX_DPI_WIN_H_

#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Sets the device scale factor that will be used unless overridden on the
// command line by --force-device-scale-factor.  If this is not called, and that
// flag is not used, the scale factor used by the DIP conversion functions below
// will be that returned by GetDPIScale().
GFX_EXPORT void SetDefaultDeviceScaleFactor(float scale);

GFX_EXPORT Size GetDPI();

// Gets the scale factor of the display. For example, if the display DPI is
// 96 then the scale factor is 1.0.  This clamps scale factors <= 1.25 to 1.0 to
// maintain previous (non-DPI-aware) behavior where only the font size was
// boosted.
GFX_EXPORT float GetDPIScale();

namespace win {

// Win32's GetSystemMetrics uses pixel measures. This function calls
// GetSystemMetrics for the given |metric|, then converts the result to DIP.
GFX_EXPORT int GetSystemMetricsInDIP(int metric);

}  // namespace win
}  // namespace gfx

#endif  // UI_GFX_DPI_WIN_H_
