// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/ColorBehavior.h"

#include "platform/graphics/BitmapImageMetrics.h"
#include "wtf/SpinLock.h"

namespace blink {

namespace {

// The output device color space is global and shared across multiple threads.
SpinLock gTargetColorSpaceLock;
SkColorSpace* gTargetColorSpace = nullptr;

}  // namespace

// static
void ColorBehavior::setGlobalTargetColorProfile(
    const WebVector<char>& profile) {
  if (profile.isEmpty())
    return;

  // Take a lock around initializing and accessing the global device color
  // profile.
  SpinLock::Guard guard(gTargetColorSpaceLock);

  // Layout tests expect that only the first call will take effect.
  if (gTargetColorSpace)
    return;

  gTargetColorSpace =
      SkColorSpace::MakeICC(profile.data(), profile.size()).release();

  // UMA statistics.
  BitmapImageMetrics::countOutputGamma(gTargetColorSpace);
}

// static
sk_sp<SkColorSpace> ColorBehavior::globalTargetColorSpace() {
  // Take a lock around initializing and accessing the global device color
  // profile.
  SpinLock::Guard guard(gTargetColorSpaceLock);

  // Initialize the output device profile to sRGB if it has not yet been
  // initialized.
  if (!gTargetColorSpace) {
    gTargetColorSpace =
        SkColorSpace::MakeNamed(SkColorSpace::kSRGB_Named).release();
  }

  gTargetColorSpace->ref();
  return sk_sp<SkColorSpace>(gTargetColorSpace);
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
  return SkColorSpace::Equals(m_target.get(), other.m_target.get());
}

bool ColorBehavior::operator!=(const ColorBehavior& other) const {
  return !(*this == other);
}

}  // namespace blink
