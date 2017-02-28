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

// This must match ICCProfileAnalyzeResult enum in histograms.xml.
enum ICCAnalyzeResult {
  ICCExtractedMatrixAndAnalyticTrFn = 0,
  ICCExtractedMatrixAndApproximatedTrFn = 1,
  ICCFailedToApproximateTrFn = 2,
  ICCFailedToExtractRawTrFn = 3,
  ICCFailedToExtractMatrix = 4,
  ICCFailedToParse = 5,
  ICCProfileAnalyzeLast = ICCFailedToParse
};

// The output device color space is global and shared across multiple threads.
SpinLock gTargetColorSpaceLock;
gfx::ColorSpace* gTargetColorSpace = nullptr;

ICCAnalyzeResult HistogramICCProfile(const gfx::ICCProfile& profile) {
  std::vector<char> data = profile.GetData();
  sk_sp<SkICC> skICC = SkICC::Make(data.data(), data.size());
  if (!skICC)
    return ICCFailedToParse;

  SkMatrix44 toXYZD50;
  bool toXYZD50Result = skICC->toXYZD50(&toXYZD50);
  if (!toXYZD50Result)
    return ICCFailedToExtractMatrix;

  SkColorSpaceTransferFn fn;
  bool isNumericalTransferFnResult = skICC->isNumericalTransferFn(&fn);
  if (isNumericalTransferFnResult)
    return ICCExtractedMatrixAndAnalyticTrFn;

  // Analyze the numerical approximation of table-based transfer functions.
  // This should never fail in practice, because any profile from which a
  // primary matrix was extracted will also provide raw transfer data.
  SkICC::Tables tables;
  bool rawTransferFnResult = skICC->rawTransferFnData(&tables);
  DCHECK(rawTransferFnResult);
  if (!rawTransferFnResult)
    return ICCFailedToExtractRawTrFn;

  // Analyze the channels separately.
  std::vector<float> xCombined;
  std::vector<float> tCombined;
  for (size_t c = 0; c < 3; ++c) {
    SkICC::Channel* channels[3] = {&tables.fRed, &tables.fGreen, &tables.fBlue};
    SkICC::Channel* channel = channels[c];
    DCHECK_GE(channel->fCount, 2);
    const float* data = reinterpret_cast<const float*>(
        tables.fStorage->bytes() + channel->fOffset);
    std::vector<float> x;
    std::vector<float> t;
    for (int i = 0; i < channel->fCount; ++i) {
      float xi = i / (channel->fCount - 1.f);
      float ti = data[i];
      x.push_back(xi);
      t.push_back(ti);
      xCombined.push_back(xi);
      tCombined.push_back(ti);
    }

    bool nonlinearFitConverged =
        gfx::SkApproximateTransferFn(x.data(), t.data(), x.size(), &fn);
    UMA_HISTOGRAM_BOOLEAN("Blink.ColorSpace.Destination.NonlinearFitConverged",
                          nonlinearFitConverged);

    // Record the accuracy of the fit, separating out by nonlinear and
    // linear fits.
    if (nonlinearFitConverged) {
      float maxError = 0.f;
      for (size_t i = 0; i < x.size(); ++i) {
        float fnOfXi = gfx::SkTransferFnEval(fn, x[i]);
        float errorAtXi = std::abs(t[i] - fnOfXi);
        maxError = std::max(maxError, errorAtXi);
      }
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Blink.ColorSpace.Destination.NonlinearFitError",
          static_cast<int>(maxError * 255), 0, 127, 16);
    }
  }

  bool combinedNonlinearFitConverged = gfx::SkApproximateTransferFn(
      xCombined.data(), tCombined.data(), xCombined.size(), &fn);
  if (!combinedNonlinearFitConverged)
    return ICCFailedToApproximateTrFn;

  float combinedMaxError = 0.f;
  for (size_t i = 0; i < xCombined.size(); ++i) {
    float fnOfXi = gfx::SkTransferFnEval(fn, xCombined[i]);
    float errorAtXi = std::abs(tCombined[i] - fnOfXi);
    combinedMaxError = std::max(combinedMaxError, errorAtXi);
  }
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Blink.ColorSpace.Destination.NonlinearFitErrorCombined",
      static_cast<int>(combinedMaxError * 255), 0, 127, 16);

  return ICCExtractedMatrixAndApproximatedTrFn;
}

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

    ICCAnalyzeResult analyzeResult = HistogramICCProfile(profile);
    UMA_HISTOGRAM_ENUMERATION("Blink.ColorSpace.Destination.ICCResult",
                              analyzeResult, ICCProfileAnalyzeLast);
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
