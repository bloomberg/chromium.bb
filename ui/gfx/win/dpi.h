// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_DPI_WIN_H_
#define UI_GFX_DPI_WIN_H_

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"

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

namespace win {

GFX_EXPORT Point ScreenToDIPPoint(const Point& pixel_point);

GFX_EXPORT Point DIPToScreenPoint(const Point& dip_point);

// WARNING: there is no right way to scale sizes and rects. The implementation
// of these strives to maintain a constant size by scaling the size independent
// of the origin. An alternative is to get the enclosing rect, which is the
// right way for some situations. Understand which you need before blindly
// assuming this is the right way.
GFX_EXPORT Rect ScreenToDIPRect(const Rect& pixel_bounds);
GFX_EXPORT Rect DIPToScreenRect(const Rect& dip_bounds);
GFX_EXPORT Size ScreenToDIPSize(const Size& size_in_pixels);
GFX_EXPORT Size DIPToScreenSize(const Size& dip_size);

// Win32's GetSystemMetrics uses pixel measures. This function calls
// GetSystemMetrics for the given |metric|, then converts the result to DIP.
GFX_EXPORT int GetSystemMetricsInDIP(int metric);

}  // namespace win
}  // namespace gfx

#endif  // UI_GFX_DPI_WIN_H_
