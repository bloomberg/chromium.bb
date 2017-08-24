// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/ColorCorrectionTestUtils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// This function is a compact version of SkHalfToFloat.
float ColorCorrectionTestUtils::Float16ToFloat(const uint16_t& f16) {
  union FloatUIntUnion {
    uint32_t fUInt;
    float fFloat;
  };
  FloatUIntUnion magic = {126 << 23};
  FloatUIntUnion o;
  if (((f16 >> 10) & 0x001f) == 0) {
    o.fUInt = magic.fUInt + (f16 & 0x03ff);
    o.fFloat -= magic.fFloat;
  } else {
    o.fUInt = (f16 & 0x03ff) << 13;
    if (((f16 >> 10) & 0x001f) == 0x1f)
      o.fUInt |= (255 << 23);
    else
      o.fUInt |= ((127 - 15 + ((f16 >> 10) & 0x001f)) << 23);
  }
  o.fUInt |= ((f16 >> 15) << 31);
  return o.fFloat;
}

bool ColorCorrectionTestUtils::IsNearlyTheSame(float expected,
                                               float actual,
                                               float tolerance) {
  EXPECT_LE(actual, expected + tolerance);
  EXPECT_GE(actual, expected - tolerance);
  return true;
}

void ColorCorrectionTestUtils::CompareColorCorrectedPixels(
    std::unique_ptr<uint8_t[]>& converted_pixel,
    std::unique_ptr<uint8_t[]>& transformed_pixel,
    int bytes_per_pixel,
    float color_correction_tolerance) {
  if (bytes_per_pixel == 4) {
    EXPECT_EQ(std::memcmp(converted_pixel.get(), transformed_pixel.get(), 4),
              0);
  } else {
    uint16_t *f16_converted =
                 static_cast<uint16_t*>((void*)(converted_pixel.get())),
             *f16_trnasformed =
                 static_cast<uint16_t*>((void*)(transformed_pixel.get()));
    bool test_passed = true;
    for (int p = 0; p < 4; p++) {
      if (!IsNearlyTheSame(Float16ToFloat(f16_converted[p]),
                           Float16ToFloat(f16_trnasformed[p]),
                           color_correction_tolerance)) {
        test_passed = false;
        break;
      }
    }
    EXPECT_EQ(test_passed, true);
  }
}

void ColorCorrectionTestUtils::CompareColorCorrectedPixels(
    uint8_t* color_components_1,
    uint8_t* color_components_2,
    int num_components,
    int color_correction_tolerance) {
  bool test_passed = true;
  for (int i = 0; i < num_components; i++) {
    if (!IsNearlyTheSame(color_components_1[i], color_components_2[i],
                         color_correction_tolerance)) {
      test_passed = false;
      break;
    }
  }
  EXPECT_EQ(test_passed, true);
}

void ColorCorrectionTestUtils::CompareColorCorrectedPixels(
    float* color_components_1,
    float* color_components_2,
    int num_components,
    float color_correction_tolerance) {
  bool test_passed = true;
  for (int i = 0; i < num_components; i++) {
    if (!IsNearlyTheSame(color_components_1[i], color_components_2[i],
                         color_correction_tolerance)) {
      test_passed = false;
      break;
    }
  }
  EXPECT_EQ(test_passed, true);
}

}  // namespace blink
