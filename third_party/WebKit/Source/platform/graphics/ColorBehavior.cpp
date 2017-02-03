// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/ColorBehavior.h"

#include "base/metrics/histogram_macros.h"
#include "platform/graphics/BitmapImageMetrics.h"
#include "third_party/skia/include/core/SkICC.h"
#include "ui/gfx/icc_profile.h"
#include "wtf/SpinLock.h"

namespace blink {

namespace {

// The output device color space is global and shared across multiple threads.
SpinLock gTargetColorSpaceLock;
gfx::ColorSpace* gTargetColorSpace = nullptr;

}  // namespace

// static
void ColorBehavior::setGlobalTargetColorProfile(
    const gfx::ICCProfile& profile) {
  // Take a lock around initializing and accessing the global device color
  // profile.
  SpinLock::Guard guard(gTargetColorSpaceLock);

  // Layout tests expect that only the first call will take effect.
  if (gTargetColorSpace)
    return;

  // Attempt to convert the ICC profile to an SkColorSpace.
  if (profile != gfx::ICCProfile()) {
    gTargetColorSpace = new gfx::ColorSpace(profile.GetColorSpace());

    std::vector<char> data = profile.GetData();
    sk_sp<SkICC> skICC = SkICC::Make(data.data(), data.size());
    if (skICC) {
      SkMatrix44 toXYZD50;
      bool toXYZD50Result = skICC->toXYZD50(&toXYZD50);
      UMA_HISTOGRAM_BOOLEAN("Blink.ColorSpace.Destination.Matrix",
                            toXYZD50Result);

      SkColorSpaceTransferFn fn;
      bool isNumericalTransferFnResult = skICC->isNumericalTransferFn(&fn);
      UMA_HISTOGRAM_BOOLEAN("Blink.ColorSpace.Destination.Numerical",
                            isNumericalTransferFnResult);
    }
  }

  // If we do not succeed, assume sRGB.
  if (!gTargetColorSpace)
    gTargetColorSpace = new gfx::ColorSpace(gfx::ColorSpace::CreateSRGB());

  // UMA statistics.
  BitmapImageMetrics::countOutputGammaAndGamut(
      gTargetColorSpace->ToSkColorSpace().get());
}

void ColorBehavior::setGlobalTargetColorSpaceForTesting(
    const gfx::ColorSpace& colorSpace) {
  // Take a lock around initializing and accessing the global device color
  // profile.
  SpinLock::Guard guard(gTargetColorSpaceLock);

  delete gTargetColorSpace;
  gTargetColorSpace = new gfx::ColorSpace(colorSpace);
}

// static
const gfx::ColorSpace& ColorBehavior::globalTargetColorSpace() {
  // Take a lock around initializing and accessing the global device color
  // profile.
  SpinLock::Guard guard(gTargetColorSpaceLock);

  // Initialize the output device profile to sRGB if it has not yet been
  // initialized.
  if (!gTargetColorSpace)
    gTargetColorSpace = new gfx::ColorSpace(gfx::ColorSpace::CreateSRGB());

  return *gTargetColorSpace;
}

// static
ColorBehavior ColorBehavior::transformToGlobalTarget() {
  return ColorBehavior(Type::TransformTo, globalTargetColorSpace());
}

// static
ColorBehavior ColorBehavior::transformToTargetForTesting() {
  return transformToGlobalTarget();
}

bool ColorBehavior::operator==(const ColorBehavior& other) const {
  if (m_type != other.m_type)
    return false;
  if (m_type != Type::TransformTo)
    return true;
  return m_target == other.m_target;
}

bool ColorBehavior::operator!=(const ColorBehavior& other) const {
  return !(*this == other);
}

}  // namespace blink
