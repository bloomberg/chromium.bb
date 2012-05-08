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
namespace {
bool dip_enabled_for_test = false;
}  // namespace

namespace test {

ScopedDIPEnablerForTest::ScopedDIPEnablerForTest() {
  CHECK(!dip_enabled_for_test);
  dip_enabled_for_test = true;
}

ScopedDIPEnablerForTest::~ScopedDIPEnablerForTest() {
  dip_enabled_for_test = false;
}

}  // namespace test

bool IsDIPEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  static const bool dip_enabled =
      command_line.HasSwitch(switches::kUIEnableDIP) ||
      command_line.HasSwitch(switches::kDefaultDeviceScaleFactor);
  return dip_enabled || dip_enabled_for_test;
}

float GetDeviceScaleFactor(const Layer* layer) {
  if (!IsDIPEnabled())
    return 1.0f;
  return layer->device_scale_factor();
}

gfx::Point ConvertPointToDIP(const Layer* layer,
                             const gfx::Point& point_in_pixel) {
  if (IsDIPEnabled())
    return point_in_pixel.Scale(1.0f / GetDeviceScaleFactor(layer));
  else
    return point_in_pixel;
}

gfx::Size ConvertSizeToDIP(const Layer* layer,
                           const gfx::Size& size_in_pixel) {
  if (IsDIPEnabled())
    return size_in_pixel.Scale(1.0f / GetDeviceScaleFactor(layer));
  else
    return size_in_pixel;
}

gfx::Rect ConvertRectToDIP(const Layer* layer,
                           const gfx::Rect& rect_in_pixel) {
  if (IsDIPEnabled()) {
    float scale = 1.0f / GetDeviceScaleFactor(layer);
    return gfx::Rect(rect_in_pixel.origin().Scale(scale),
                     rect_in_pixel.size().Scale(scale));
  } else {
    return rect_in_pixel;
  }
}

gfx::Point ConvertPointToPixel(const Layer* layer,
                               const gfx::Point& point_in_dip) {
  if (IsDIPEnabled()) {
    return point_in_dip.Scale(GetDeviceScaleFactor(layer));
  } else {
    return point_in_dip;
  }
}

gfx::Size ConvertSizeToPixel(const Layer* layer,
                             const gfx::Size& size_in_dip) {
  if (IsDIPEnabled()) {
    return size_in_dip.Scale(GetDeviceScaleFactor(layer));
  } else {
    return size_in_dip;
  }
}

gfx::Rect ConvertRectToPixel(const Layer* layer,
                             const gfx::Rect& rect_in_dip) {
  if (IsDIPEnabled()) {
    float scale = GetDeviceScaleFactor(layer);
    return gfx::Rect(rect_in_dip.origin().Scale(scale),
                     rect_in_dip.size().Scale(scale));
  } else {
    return rect_in_dip;
  }
}
}  // namespace ui
