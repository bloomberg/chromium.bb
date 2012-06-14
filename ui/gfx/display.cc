// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/display.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/insets.h"

namespace gfx {
namespace {

bool HasForceDeviceScaleFactor() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceDeviceScaleFactor);
}

float GetForcedDeviceScaleFactorImpl() {
  double scale_in_double = 1.0;
  if (HasForceDeviceScaleFactor()) {
    std::string value = CommandLine::ForCurrentProcess()->
        GetSwitchValueASCII(switches::kForceDeviceScaleFactor);
    if (!base::StringToDouble(value, &scale_in_double))
      LOG(ERROR) << "Failed to parse the deafult device scale factor:" << value;
  }
  return static_cast<float>(scale_in_double);
}

} // namespace

// static
float Display::GetForcedDeviceScaleFactor() {
  static const float kForcedDeviceScaleFactor =
      GetForcedDeviceScaleFactorImpl();
  return kForcedDeviceScaleFactor;
}

Display::Display()
    : id_(-1),
      device_scale_factor_(GetForcedDeviceScaleFactor()) {
}

Display::Display(int id)
    : id_(id),
      device_scale_factor_(GetForcedDeviceScaleFactor()) {
}

Display::Display(int id, const gfx::Rect& bounds)
    : id_(id),
      bounds_(bounds),
      work_area_(bounds),
      device_scale_factor_(GetForcedDeviceScaleFactor()) {
#if defined(USE_AURA)
  SetScaleAndBounds(device_scale_factor_, bounds);
#endif
}

Display::~Display() {
}

Insets Display::GetWorkAreaInsets() const {
  return gfx::Insets(work_area_.y() - bounds_.y(),
                     work_area_.x() - bounds_.x(),
                     bounds_.bottom() - work_area_.bottom(),
                     bounds_.right() - work_area_.right());
}

void Display::SetScaleAndBounds(
    float device_scale_factor,
    const gfx::Rect& bounds_in_pixel) {
  Insets insets = bounds_.InsetsFrom(work_area_);
  if (!HasForceDeviceScaleFactor())
    device_scale_factor_ = device_scale_factor;
#if defined(USE_AURA)
  bounds_in_pixel_ = bounds_in_pixel;
#endif
  // TODO(oshima): For m19, work area/display bounds that chrome/webapps sees
  // has (0, 0) origin because it's simpler and enough. Fix this when
  // real multi display support is implemented.
  bounds_ = gfx::Rect(
      bounds_in_pixel.size().Scale(1.0f / device_scale_factor_));
  UpdateWorkAreaFromInsets(insets);
}

void Display::SetSize(const gfx::Size& size_in_pixel) {
  SetScaleAndBounds(
      device_scale_factor_,
#if defined(USE_AURA)
      gfx::Rect(bounds_in_pixel_.origin(), size_in_pixel));
#else
      gfx::Rect(bounds_.origin(), size_in_pixel));
#endif
}

void Display::UpdateWorkAreaFromInsets(const gfx::Insets& insets) {
  work_area_ = bounds_;
  work_area_.Inset(insets);
}

gfx::Size Display::GetSizeInPixel() const {
  return size().Scale(device_scale_factor_);
}

std::string Display::ToString() const {
  return base::StringPrintf("Display[%d] bounds=%s, workarea=%s, scale=%f",
                            id_,
                            bounds_.ToString().c_str(),
                            work_area_.ToString().c_str(),
                            device_scale_factor_);
}

}  // namespace gfx
