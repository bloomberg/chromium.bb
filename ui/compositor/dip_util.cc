// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/dip_util.h"

#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/monitor.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"
#include "ui/gfx/rect.h"

namespace ui {

float GetDeviceScaleFactor(const Layer* layer) {
  return layer->device_scale_factor();
}

gfx::Point ConvertPointToDIP(const Layer* layer,
                             const gfx::Point& point_in_pixel) {
  return point_in_pixel.Scale(1.0f / GetDeviceScaleFactor(layer));
}

gfx::Size ConvertSizeToDIP(const Layer* layer,
                           const gfx::Size& size_in_pixel) {
  return size_in_pixel.Scale(1.0f / GetDeviceScaleFactor(layer));
}

gfx::Rect ConvertRectToDIP(const Layer* layer,
                           const gfx::Rect& rect_in_pixel) {
  float scale = 1.0f / GetDeviceScaleFactor(layer);
  return gfx::Rect(rect_in_pixel.origin().Scale(scale),
                   rect_in_pixel.size().Scale(scale));
}

gfx::Point ConvertPointToPixel(const Layer* layer,
                               const gfx::Point& point_in_dip) {
  return point_in_dip.Scale(GetDeviceScaleFactor(layer));
}

gfx::Size ConvertSizeToPixel(const Layer* layer,
                             const gfx::Size& size_in_dip) {
  return size_in_dip.Scale(GetDeviceScaleFactor(layer));
}

gfx::Rect ConvertRectToPixel(const Layer* layer,
                             const gfx::Rect& rect_in_dip) {
  float scale = GetDeviceScaleFactor(layer);
  return gfx::Rect(rect_in_dip.origin().Scale(scale),
                   rect_in_dip.size().Scale(scale));
}
}  // namespace ui
