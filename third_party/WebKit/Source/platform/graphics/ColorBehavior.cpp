// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/ColorBehavior.h"

#include "base/metrics/histogram_macros.h"
#include "platform/graphics/BitmapImageMetrics.h"
#include "third_party/skia/include/core/SkICC.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/skia_color_space_util.h"
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

      // Analyze the numerical approximation of table-based transfer functions.
      if (!isNumericalTransferFnResult) {
        SkICC::Tables tables;
        bool rawTransferFnResult = skICC->rawTransferFnData(&tables);
        UMA_HISTOGRAM_BOOLEAN("Blink.ColorSpace.Destination.ExtractedRawData",
                              rawTransferFnResult);
        if (rawTransferFnResult) {
          SkICC::Channel* channels[3] = {&tables.fRed, &tables.fGreen,
                                         &tables.fBlue};
          for (size_t c = 0; c < 3; ++c) {
            SkICC::Channel* channel = channels[c];
            DCHECK_GE(channel->fCount, 2);
            const float* data = reinterpret_cast<const float*>(
                tables.fStorage->bytes() + channel->fOffset);
            std::vector<float> x;
            std::vector<float> t;
            float tMin = data[0];
            float tMax = data[0];
            for (int i = 0; i < channel->fCount; ++i) {
              float xi = i / (channel->fCount - 1.f);
              float ti = data[i];
              x.push_back(xi);
              t.push_back(ti);
              tMin = std::min(tMin, ti);
              tMax = std::max(tMax, ti);
            }

            // Record the range of the table-based transfer function. If it is
            // found that almost all ranges are from 0 to 1, then we will bake
            // the assumption that the range is 0 to 1 into the numerical
            // approximation code (which will improve stability).
            UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.ColorSpace.Destination.TMin",
                                        static_cast<int>(tMin * 255), 0, 31, 8);
            UMA_HISTOGRAM_CUSTOM_COUNTS(
                "Blink.ColorSpace.Destination.OneMinusTMax",
                static_cast<int>((1.f - tMax) * 255), 0, 31, 8);
            bool nonlinearFitConverged = false;
            float error = 0;
            gfx::SkApproximateTransferFn(x.data(), t.data(), x.size(), &fn,
                                         &error, &nonlinearFitConverged);

            // Record if the numerical fit converged, or if it didn't.
            UMA_HISTOGRAM_BOOLEAN(
                "Blink.ColorSpace.Destination.NonlinearFitConverged",
                nonlinearFitConverged);

            // Record the accuracy of the fit, separating out by nonlinear and
            // linear fits.
            if (nonlinearFitConverged) {
              UMA_HISTOGRAM_CUSTOM_COUNTS(
                  "Blink.ColorSpace.Destination.NonlinearFitError",
                  static_cast<int>(error * 255), 0, 127, 16);
            } else {
              UMA_HISTOGRAM_CUSTOM_COUNTS(
                  "Blink.ColorSpace.Destination.LinearFitError",
                  static_cast<int>(error * 255), 0, 127, 16);
            }
          }
        }
      }
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
