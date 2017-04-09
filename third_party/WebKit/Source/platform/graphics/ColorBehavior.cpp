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

// This must match ICCProfileAnalyzeResult enum in histograms.xml.
enum ICCAnalyzeResult {
  kICCExtractedMatrixAndAnalyticTrFn = 0,
  kICCExtractedMatrixAndApproximatedTrFn = 1,
  kICCFailedToApproximateTrFn = 2,
  kICCFailedToExtractRawTrFn = 3,
  kICCFailedToExtractMatrix = 4,
  kICCFailedToParse = 5,
  kICCProfileAnalyzeLast = kICCFailedToParse
};

// The output device color space is global and shared across multiple threads.
SpinLock g_target_color_space_lock;
gfx::ColorSpace* g_target_color_space = nullptr;

ICCAnalyzeResult HistogramICCProfile(const gfx::ICCProfile& profile) {
  std::vector<char> data = profile.GetData();
  sk_sp<SkICC> sk_icc = SkICC::Make(data.data(), data.size());
  if (!sk_icc)
    return kICCFailedToParse;

  SkMatrix44 to_xyzd50;
  bool to_xyzd50_result = sk_icc->toXYZD50(&to_xyzd50);
  if (!to_xyzd50_result)
    return kICCFailedToExtractMatrix;

  SkColorSpaceTransferFn fn;
  bool is_numerical_transfer_fn_result = sk_icc->isNumericalTransferFn(&fn);
  if (is_numerical_transfer_fn_result)
    return kICCExtractedMatrixAndAnalyticTrFn;

  // Analyze the numerical approximation of table-based transfer functions.
  // This should never fail in practice, because any profile from which a
  // primary matrix was extracted will also provide raw transfer data.
  SkICC::Tables tables;
  bool raw_transfer_fn_result = sk_icc->rawTransferFnData(&tables);
  DCHECK(raw_transfer_fn_result);
  if (!raw_transfer_fn_result)
    return kICCFailedToExtractRawTrFn;

  // Analyze the channels separately.
  std::vector<float> x_combined;
  std::vector<float> t_combined;
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
      x_combined.push_back(xi);
      t_combined.push_back(ti);
    }

    bool nonlinear_fit_converged =
        gfx::SkApproximateTransferFn(x.data(), t.data(), x.size(), &fn);
    UMA_HISTOGRAM_BOOLEAN("Blink.ColorSpace.Destination.NonlinearFitConverged",
                          nonlinear_fit_converged);

    // Record the accuracy of the fit, separating out by nonlinear and
    // linear fits.
    if (nonlinear_fit_converged) {
      float max_error = 0.f;
      for (size_t i = 0; i < x.size(); ++i) {
        float fn_of_xi = gfx::SkTransferFnEval(fn, x[i]);
        float error_at_xi = std::abs(t[i] - fn_of_xi);
        max_error = std::max(max_error, error_at_xi);
      }
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Blink.ColorSpace.Destination.NonlinearFitError",
          static_cast<int>(max_error * 255), 0, 127, 16);
    }
  }

  bool combined_nonlinear_fit_converged = gfx::SkApproximateTransferFn(
      x_combined.data(), t_combined.data(), x_combined.size(), &fn);
  if (!combined_nonlinear_fit_converged)
    return kICCFailedToApproximateTrFn;

  float combined_max_error = 0.f;
  for (size_t i = 0; i < x_combined.size(); ++i) {
    float fn_of_xi = gfx::SkTransferFnEval(fn, x_combined[i]);
    float error_at_xi = std::abs(t_combined[i] - fn_of_xi);
    combined_max_error = std::max(combined_max_error, error_at_xi);
  }
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Blink.ColorSpace.Destination.NonlinearFitErrorCombined",
      static_cast<int>(combined_max_error * 255), 0, 127, 16);

  return kICCExtractedMatrixAndApproximatedTrFn;
}

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
  if (profile != gfx::ICCProfile()) {
    g_target_color_space = new gfx::ColorSpace(profile.GetColorSpace());

    ICCAnalyzeResult analyze_result = HistogramICCProfile(profile);
    UMA_HISTOGRAM_ENUMERATION("Blink.ColorSpace.Destination.ICCResult",
                              analyze_result, kICCProfileAnalyzeLast);
  }

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
