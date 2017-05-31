// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/BitmapImageMetrics.h"

#include "platform/Histogram.h"
#include "platform/graphics/ColorSpaceGamut.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

void BitmapImageMetrics::CountDecodedImageType(const String& type) {
  DecodedImageType decoded_image_type =
      type == "jpg"
          ? kImageJPEG
          : type == "png"
                ? kImagePNG
                : type == "gif"
                      ? kImageGIF
                      : type == "webp"
                            ? kImageWebP
                            : type == "ico"
                                  ? kImageICO
                                  : type == "bmp"
                                        ? kImageBMP
                                        : DecodedImageType::kImageUnknown;

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, decoded_image_type_histogram,
      ("Blink.DecodedImageType", kDecodedImageTypeEnumEnd));
  decoded_image_type_histogram.Count(decoded_image_type);
}

void BitmapImageMetrics::CountImageOrientation(
    const ImageOrientationEnum orientation) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, orientation_histogram,
      ("Blink.DecodedImage.Orientation", kImageOrientationEnumEnd));
  orientation_histogram.Count(orientation);
}

void BitmapImageMetrics::CountImageGammaAndGamut(SkColorSpace* color_space) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, gamma_named_histogram,
                                  ("Blink.ColorSpace.Source", kGammaEnd));
  gamma_named_histogram.Count(GetColorSpaceGamma(color_space));

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, gamut_named_histogram,
      ("Blink.ColorGamut.Source", static_cast<int>(ColorSpaceGamut::kEnd)));
  gamut_named_histogram.Count(
      static_cast<int>(ColorSpaceUtilities::GetColorSpaceGamut(color_space)));
}

void BitmapImageMetrics::CountOutputGammaAndGamut(SkColorSpace* color_space) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, gamma_named_histogram,
                                  ("Blink.ColorSpace.Destination", kGammaEnd));
  gamma_named_histogram.Count(GetColorSpaceGamma(color_space));

  DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, gamut_named_histogram,
                                  ("Blink.ColorGamut.Destination",
                                   static_cast<int>(ColorSpaceGamut::kEnd)));
  gamut_named_histogram.Count(
      static_cast<int>(ColorSpaceUtilities::GetColorSpaceGamut(color_space)));
}

BitmapImageMetrics::Gamma BitmapImageMetrics::GetColorSpaceGamma(
    SkColorSpace* color_space) {
  Gamma gamma = kGammaNull;
  if (color_space) {
    if (color_space->gammaCloseToSRGB()) {
      gamma = kGammaSRGB;
    } else if (color_space->gammaIsLinear()) {
      gamma = kGammaLinear;
    } else {
      gamma = kGammaNonStandard;
    }
  }
  return gamma;
}

}  // namespace blink
