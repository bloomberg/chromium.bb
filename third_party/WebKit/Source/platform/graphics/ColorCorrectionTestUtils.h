// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ColorCorrectionTestUtils_h
#define ColorCorrectionTestUtils_h

#include "platform/graphics/CanvasColorParams.h"

#include "platform/graphics/GraphicsTypes.h"
#include "platform/runtime_enabled_features.h"
#include "third_party/skia/include/core/SkColorSpaceXform.h"

namespace blink {

enum PixelsAlphaMultiply {
  kAlphaMultiplied,
  kAlphaUnmultiplied,
};

enum UnpremulRoundTripTolerance {
  kNoUnpremulRoundTripTolerance,
  kUnpremulRoundTripTolerance,
};

class ColorCorrectionTestUtils {
 public:
  static void CompareColorCorrectedPixels(
      const void* actual_pixels,
      const void* expected_pixels,
      int num_pixels,
      ImageDataStorageFormat src_storage_format,
      PixelsAlphaMultiply alpha_multiplied,
      UnpremulRoundTripTolerance premul_unpremul_tolerance);

  static bool ConvertPixelsToColorSpaceAndPixelFormatForTest(
      void* src_data,
      int num_elements,
      CanvasColorSpace src_color_space,
      ImageDataStorageFormat src_storage_format,
      CanvasColorSpace dst_color_space,
      CanvasPixelFormat dst_pixel_format,
      std::unique_ptr<uint8_t[]>& converted_pixels,
      SkColorSpaceXform::ColorFormat color_format_for_f16_canvas);

 private:
  static bool IsNearlyTheSame(float expected, float actual, float tolerance);
};

}  // namespace blink

#endif  // ColorCorrectionTestUtils_h
