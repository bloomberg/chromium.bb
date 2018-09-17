// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"

#include "base/sys_byteorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

bool ColorCorrectionTestUtils::IsNearlyTheSame(float expected,
                                               float actual,
                                               float tolerance) {
  EXPECT_LE(actual, expected + tolerance);
  EXPECT_GE(actual, expected - tolerance);
  return true;
}

void ColorCorrectionTestUtils::CompareColorCorrectedPixels(
    const void* actual_pixels,
    const void* expected_pixels,
    int num_pixels,
    PixelFormat pixel_format,
    PixelsAlphaMultiply alpha_multiplied,
    UnpremulRoundTripTolerance premul_unpremul_tolerance) {
  bool test_passed = true;
  int _8888_color_correction_tolerance = 3;
  int _16161616_color_correction_tolerance = 255;
  float floating_point_color_correction_tolerance = 0.01;
  if (premul_unpremul_tolerance == kNoUnpremulRoundTripTolerance)
    floating_point_color_correction_tolerance = 0;

  switch (pixel_format) {
    case kPixelFormat_8888: {
      if (premul_unpremul_tolerance == kUnpremulRoundTripTolerance) {
        // Premul->unpremul->premul round trip does not introduce any error when
        // rounding intermediate results. However, we still might see some error
        // introduced in consecutive color correction operations (error <= 3).
        // For unpremul->premul->unpremul round trip, we do premul and compare
        // the result.
        const uint8_t* actual_pixels_u8 =
            static_cast<const uint8_t*>(actual_pixels);
        const uint8_t* expected_pixels_u8 =
            static_cast<const uint8_t*>(expected_pixels);
        for (int i = 0; test_passed && i < num_pixels; i++) {
          test_passed &=
              (actual_pixels_u8[i * 4 + 3] == expected_pixels_u8[i * 4 + 3]);
          int alpha_multiplier =
              alpha_multiplied ? 1 : expected_pixels_u8[i * 4 + 3];
          for (int j = 0; j < 3; j++) {
            test_passed &= IsNearlyTheSame(
                actual_pixels_u8[i * 4 + j] * alpha_multiplier,
                expected_pixels_u8[i * 4 + j] * alpha_multiplier,
                _8888_color_correction_tolerance);
          }
        }
      } else {
        EXPECT_EQ(std::memcmp(actual_pixels, expected_pixels, num_pixels * 4),
                  0);
      }
      break;
    }

    case kPixelFormat_16161616: {
      const uint16_t* actual_pixels_u16 =
          static_cast<const uint16_t*>(actual_pixels);
      const uint16_t* expected_pixels_u16 =
          static_cast<const uint16_t*>(expected_pixels);
      for (int i = 0; test_passed && i < num_pixels * 4; i++) {
        test_passed &=
            IsNearlyTheSame(actual_pixels_u16[i], expected_pixels_u16[i],
                            _16161616_color_correction_tolerance);
      }
      break;
    }

    case kPixelFormat_hhhh: {
      float actual_pixels_f32[num_pixels * 4];
      float expected_pixels_f32[num_pixels * 4];
      EXPECT_TRUE(
          skcms_Transform(actual_pixels, skcms_PixelFormat_RGBA_hhhh,
                          skcms_AlphaFormat_Unpremul, nullptr,
                          actual_pixels_f32, skcms_PixelFormat_BGRA_ffff,
                          skcms_AlphaFormat_Unpremul, nullptr, num_pixels));
      EXPECT_TRUE(
          skcms_Transform(expected_pixels, skcms_PixelFormat_RGBA_hhhh,
                          skcms_AlphaFormat_Unpremul, nullptr,
                          expected_pixels_f32, skcms_PixelFormat_BGRA_ffff,
                          skcms_AlphaFormat_Unpremul, nullptr, num_pixels));

      for (int i = 0; test_passed && i < num_pixels * 4; i++) {
        test_passed &=
            IsNearlyTheSame(actual_pixels_f32[i], expected_pixels_f32[i],
                            floating_point_color_correction_tolerance);
      }
      break;
    }

    case kPixelFormat_ffff: {
      const float* actual_pixels_f32 = static_cast<const float*>(actual_pixels);
      const float* expected_pixels_f32 =
          static_cast<const float*>(expected_pixels);
      for (int i = 0; test_passed && i < num_pixels * 4; i++) {
        test_passed &=
            IsNearlyTheSame(actual_pixels_f32[i], expected_pixels_f32[i],
                            floating_point_color_correction_tolerance);
      }
      break;
    }

    default:
      NOTREACHED();
  }
  EXPECT_EQ(test_passed, true);
}

bool ColorCorrectionTestUtils::ConvertPixelsToColorSpaceAndPixelFormatForTest(
    void* src_data,
    int num_elements,
    CanvasColorSpace src_color_space,
    ImageDataStorageFormat src_storage_format,
    CanvasColorSpace dst_color_space,
    CanvasPixelFormat dst_canvas_pixel_format,
    std::unique_ptr<uint8_t[]>& converted_pixels,
    PixelFormat pixel_format_for_f16_canvas) {
  skcms_PixelFormat src_pixel_format = skcms_PixelFormat_RGBA_8888;
  if (src_storage_format == kUint16ArrayStorageFormat) {
    src_pixel_format = skcms_PixelFormat_RGBA_16161616LE;
  } else if (src_storage_format == kFloat32ArrayStorageFormat) {
    src_pixel_format = skcms_PixelFormat_RGBA_ffff;
  }

  skcms_PixelFormat dst_pixel_format = skcms_PixelFormat_RGBA_8888;
  if (dst_canvas_pixel_format == kF16CanvasPixelFormat) {
    dst_pixel_format = (pixel_format_for_f16_canvas == kPixelFormat_hhhh)
                           ? skcms_PixelFormat_RGBA_hhhh
                           : skcms_PixelFormat_RGBA_ffff;
  }

  sk_sp<SkColorSpace> src_sk_color_space = nullptr;
  src_sk_color_space =
      CanvasColorParams(src_color_space,
                        (src_storage_format == kUint8ClampedArrayStorageFormat)
                            ? kRGBA8CanvasPixelFormat
                            : kF16CanvasPixelFormat,
                        kNonOpaque)
          .GetSkColorSpaceForSkSurfaces();
  if (!src_sk_color_space.get())
    src_sk_color_space = SkColorSpace::MakeSRGB();

  sk_sp<SkColorSpace> dst_sk_color_space =
      CanvasColorParams(dst_color_space, dst_canvas_pixel_format, kNonOpaque)
          .GetSkColorSpaceForSkSurfaces();
  if (!dst_sk_color_space.get())
    dst_sk_color_space = SkColorSpace::MakeSRGB();

  skcms_ICCProfile src_profile, dst_profile;
  src_sk_color_space->toProfile(&src_profile);
  dst_sk_color_space->toProfile(&dst_profile);

  skcms_AlphaFormat alpha_format = skcms_AlphaFormat_Unpremul;
  bool conversion_result =
      skcms_Transform(src_data, src_pixel_format, alpha_format, &src_profile,
                      converted_pixels.get(), dst_pixel_format, alpha_format,
                      &dst_profile, num_elements / 4);

  return conversion_result;
}

bool ColorCorrectionTestUtils::MatchColorSpace(
    SkColorSpace* src_color_space,
    SkColorSpace* dst_color_space,
    float xyz_d50_component_tolerance) {
  if ((!src_color_space && dst_color_space) ||
      (src_color_space && !dst_color_space))
    return false;
  if (src_color_space) {
    const SkMatrix44* src_matrix = src_color_space->toXYZD50();
    const SkMatrix44* dst_matrix = dst_color_space->toXYZD50();
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        if (fabs(src_matrix->get(i, j) - dst_matrix->get(i, j)) >
            xyz_d50_component_tolerance) {
          return false;
        }
      }
    }
  }
  return true;
}

bool ColorCorrectionTestUtils::MatchSkImages(sk_sp<SkImage> src_image,
                                             sk_sp<SkImage> dst_image,
                                             unsigned uint8_tolerance,
                                             float f16_tolerance,
                                             float xyz_d50_component_tolerance,
                                             bool compare_alpha) {
  if ((!src_image && dst_image) || (src_image && !dst_image))
    return false;
  if (!src_image)
    return true;
  if ((src_image->width() != dst_image->width()) ||
      (src_image->height() != dst_image->height())) {
    return false;
  }

  if (compare_alpha && src_image->alphaType() != dst_image->alphaType())
    return false;
  if (src_image->makeRasterImage()->colorType() !=
      dst_image->makeRasterImage()->colorType()) {
    return false;
  }
  if (!MatchColorSpace(src_image->colorSpace(), dst_image->colorSpace(),
                       xyz_d50_component_tolerance)) {
    return false;
  }

  bool test_passed = true;
  int num_pixels = src_image->width() * src_image->height();
  int num_components = compare_alpha ? 4 : 3;

  SkImageInfo src_info = SkImageInfo::Make(
      src_image->width(), src_image->height(), kN32_SkColorType,
      src_image->alphaType(), src_image->refColorSpace());

  SkImageInfo dst_info = SkImageInfo::Make(
      dst_image->width(), dst_image->height(), kN32_SkColorType,
      src_image->alphaType(), dst_image->refColorSpace());

  if (src_image->colorType() != kRGBA_F16_SkColorType) {
    std::unique_ptr<uint8_t[]> src_pixels(new uint8_t[num_pixels * 4]());
    std::unique_ptr<uint8_t[]> dst_pixels(new uint8_t[num_pixels * 4]());

    src_image->readPixels(src_info, src_pixels.get(), src_info.minRowBytes(), 0,
                          0);
    dst_image->readPixels(dst_info, dst_pixels.get(), dst_info.minRowBytes(), 0,
                          0);

    for (int i = 0; test_passed && i < num_pixels; i++) {
      for (int j = 0; j < num_components; j++) {
        test_passed &= IsNearlyTheSame(src_pixels[i * 4 + j],
                                       dst_pixels[i * 4 + j], uint8_tolerance);
      }
    }
    return test_passed;
  }

  std::unique_ptr<float[]> src_pixels(new float[num_pixels * 4]());
  std::unique_ptr<float[]> dst_pixels(new float[num_pixels * 4]());

  src_info = src_info.makeColorType(kRGBA_F32_SkColorType);
  dst_info = dst_info.makeColorType(kRGBA_F32_SkColorType);

  src_image->readPixels(src_info, src_pixels.get(), src_info.minRowBytes(), 0,
                        0);
  dst_image->readPixels(dst_info, dst_pixels.get(), dst_info.minRowBytes(), 0,
                        0);

  for (int i = 0; test_passed && i < num_pixels; i++) {
    for (int j = 0; j < num_components; j++) {
      test_passed &= IsNearlyTheSame(src_pixels[i * 4 + j],
                                     dst_pixels[i * 4 + j], f16_tolerance);
    }
  }
  return test_passed;
}

}  // namespace blink
