// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DIP_UTIL_H_
#define UI_AURA_DIP_UTIL_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace gfx {
class Point;
class Size;
class Rect;
}  // namespace gfx

namespace aura {
class Window;

#if defined(ENABLE_DIP)
AURA_EXPORT float GetMonitorScaleFactor(const Window* window);
#endif

// Utility functions that convert point/size/rect between
// DIP and pixel coordinates system.
AURA_EXPORT gfx::Point ConvertPointToDIP(const Window* window,
                                         const gfx::Point& point_in_pixel);
AURA_EXPORT gfx::Size ConvertSizeToDIP(const Window* window,
                                       const gfx::Size& size_in_pixel);
AURA_EXPORT gfx::Rect ConvertRectToDIP(const Window* window,
                                       const gfx::Rect& rect_in_pixel);
AURA_EXPORT gfx::Point ConvertPointToPixel(const Window* window,
                                           const gfx::Point& point_in_dip);
AURA_EXPORT gfx::Size ConvertSizeToPixel(const Window* window,
                                         const gfx::Size& size_in_dip);
AURA_EXPORT gfx::Rect ConvertRectToPixel(const Window* window,
                                         const gfx::Rect& rect_in_dip);

}  // namespace aura

#endif  // UI_AURA_DIP_UTIL_H_
