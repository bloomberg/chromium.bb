// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/ColorBehavior.h"

#include "base/metrics/histogram_macros.h"
#include "platform/graphics/BitmapImageMetrics.h"
#include "platform/wtf/SpinLock.h"
#include "third_party/skia/include/core/SkICC.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/skia_color_space_util.h"

namespace blink {

namespace {

// The output device color space is global and shared across multiple threads.
SpinLock g_target_color_space_lock;
gfx::ColorSpace* g_target_color_space = nullptr;

}  // namespace

// static
void ColorBehavior::SetGlobalTargetColorProfile(
    const gfx::ICCProfile& profile) {
  // Take a lock around initializing and accessing the global device color
  // profile.
  SpinLock::Guard guard(g_target_color_space_lock);

  // Layout tests expect that only the first call will take effect.
  if (g_target_color_space)
    return;

  // Attempt to convert the ICC profile to an SkColorSpace.
  if (profile != gfx::ICCProfile())
    g_target_color_space = new gfx::ColorSpace(profile.GetColorSpace());

  // If we do not succeed, assume sRGB.
  if (!g_target_color_space)
    g_target_color_space = new gfx::ColorSpace(gfx::ColorSpace::CreateSRGB());

  // UMA statistics.
  BitmapImageMetrics::CountOutputGammaAndGamut(
      g_target_color_space->ToSkColorSpace().get());
}

void ColorBehavior::SetGlobalTargetColorSpaceForTesting(
    const gfx::ColorSpace& color_space) {
  // Take a lock around initializing and accessing the global device color
  // profile.
  SpinLock::Guard guard(g_target_color_space_lock);

  delete g_target_color_space;
  g_target_color_space = new gfx::ColorSpace(color_space);
}

// static
const gfx::ColorSpace& ColorBehavior::GlobalTargetColorSpace() {
  // Take a lock around initializing and accessing the global device color
  // profile.
  SpinLock::Guard guard(g_target_color_space_lock);

  // Initialize the output device profile to sRGB if it has not yet been
  // initialized.
  if (!g_target_color_space)
    g_target_color_space = new gfx::ColorSpace(gfx::ColorSpace::CreateSRGB());

  return *g_target_color_space;
}

// static
ColorBehavior ColorBehavior::TransformToGlobalTarget() {
  return ColorBehavior(Type::kTransformTo, GlobalTargetColorSpace());
}

// static
ColorBehavior ColorBehavior::TransformToTargetForTesting() {
  return TransformToGlobalTarget();
}

bool ColorBehavior::operator==(const ColorBehavior& other) const {
  if (type_ != other.type_)
    return false;
  if (type_ != Type::kTransformTo)
    return true;
  return target_ == other.target_;
}

bool ColorBehavior::operator!=(const ColorBehavior& other) const {
  return !(*this == other);
}

}  // namespace blink
