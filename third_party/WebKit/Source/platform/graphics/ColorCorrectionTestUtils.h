// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ColorCorrectionTestUtils_h
#define ColorCorrectionTestUtils_h

#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

class ScopedEnableColorCanvasExtensions {
 public:
  ScopedEnableColorCanvasExtensions()
      : experimental_canvas_features_(
            RuntimeEnabledFeatures::ExperimentalCanvasFeaturesEnabled()),
        color_correct_rendering_(
            RuntimeEnabledFeatures::ColorCorrectRenderingEnabled()),
        color_canvas_extensions_(
            RuntimeEnabledFeatures::ColorCanvasExtensionsEnabled()) {
    RuntimeEnabledFeatures::SetExperimentalCanvasFeaturesEnabled(true);
    RuntimeEnabledFeatures::SetColorCorrectRenderingEnabled(true);
    RuntimeEnabledFeatures::SetColorCanvasExtensionsEnabled(true);
  }
  ~ScopedEnableColorCanvasExtensions() {
    RuntimeEnabledFeatures::SetExperimentalCanvasFeaturesEnabled(
        experimental_canvas_features_);
    RuntimeEnabledFeatures::SetColorCorrectRenderingEnabled(
        color_correct_rendering_);
    RuntimeEnabledFeatures::SetColorCanvasExtensionsEnabled(
        color_canvas_extensions_);
  }

 private:
  bool experimental_canvas_features_;
  bool color_correct_rendering_;
  bool color_canvas_extensions_;
};

class ColorCorrectionTestUtils {
 public:
  static bool IsNearlyTheSame(
      float expected,
      float actual,
      float tolerance = wide_gamut_color_correction_tolerance_);
  static void CompareColorCorrectedPixels(
      std::unique_ptr<uint8_t[]>& converted_pixel,
      std::unique_ptr<uint8_t[]>& transformed_pixel,
      int bytes_per_pixel = 4,
      float color_correction_tolerance =
          wide_gamut_color_correction_tolerance_);
  static void CompareColorCorrectedPixels(uint8_t* color_components_1,
                                          uint8_t* color_components_2,
                                          int,
                                          int);
  static void CompareColorCorrectedPixels(
      float* color_components_1,
      float* color_components_2,
      int num_components,
      float color_correction_tolerance =
          wide_gamut_color_correction_tolerance_);

 private:
  static constexpr float wide_gamut_color_correction_tolerance_ = 0.01;
  // This function is a compact version of SkHalfToFloat from Skia. If many
  // color correction tests fail at the same time, please check if SkHalf format
  // has changed.
  static float Float16ToFloat(const uint16_t& f16);
};

}  // namespace blink

#endif  // ColorCorrectionTestUtils_h
