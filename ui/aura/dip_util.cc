// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/dip_util.h"

#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"
#include "ui/gfx/rect.h"

#if defined(ENABLE_DIP)
#include "ui/aura/monitor_manager.h"
#include "ui/gfx/monitor.h"
#endif

namespace aura {
#if defined(ENABLE_DIP)
float GetMonitorScaleFactor(const Window* window) {
  gfx::Monitor monitor = aura::Env::GetInstance()->monitor_manager()->
      GetMonitorNearestWindow(window);
  return monitor.device_scale_factor();
}  // namespace
#endif

gfx::Point ConvertPointToDIP(const Window* window,
                             const gfx::Point& point_in_pixel) {
#if defined(ENABLE_DIP)
  return point_in_pixel.Scale(1.0f / GetMonitorScaleFactor(window));
#else
  return point_in_pixel;
#endif
}

gfx::Size ConvertSizeToDIP(const Window* window,
                           const gfx::Size& size_in_pixel) {
#if defined(ENABLE_DIP)
  return size_in_pixel.Scale(1.0f / GetMonitorScaleFactor(window));
#else
  return size_in_pixel;
#endif
}

gfx::Rect ConvertRectToDIP(const Window* window,
                           const gfx::Rect& rect_in_pixel) {
#if defined(ENABLE_DIP)
  float scale = 1.0f / GetMonitorScaleFactor(window);
  return gfx::Rect(rect_in_pixel.origin().Scale(scale),
                   rect_in_pixel.size().Scale(scale));
#else
  return rect_in_pixel;
#endif
}

gfx::Point ConvertPointToPixel(const Window* window,
                               const gfx::Point& point_in_dip) {
#if defined(ENABLE_DIP)
  return point_in_dip.Scale(GetMonitorScaleFactor(window));
#else
  return point_in_dip;
#endif
}

gfx::Size ConvertSizeToPixel(const Window* window,
                             const gfx::Size& size_in_dip) {
#if defined(ENABLE_DIP)
  return size_in_dip.Scale(GetMonitorScaleFactor(window));
#else
  return size_in_dip;
#endif
}

gfx::Rect ConvertRectToPixel(const Window* window,
                             const gfx::Rect& rect_in_dip) {
#if defined(ENABLE_DIP)
  float scale = GetMonitorScaleFactor(window);
  return gfx::Rect(rect_in_dip.origin().Scale(scale),
                   rect_in_dip.size().Scale(scale));
#else
  return rect_in_dip;
#endif
}
}  // namespace aura
