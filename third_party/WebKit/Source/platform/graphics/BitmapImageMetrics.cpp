// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/BitmapImageMetrics.h"

#include "platform/Histogram.h"
#include "third_party/skia/include/core/SkColorSpaceXform.h"
#include "wtf/Threading.h"
#include "wtf/text/WTFString.h"

namespace blink {

void BitmapImageMetrics::countDecodedImageType(const String& type) {
  DecodedImageType decodedImageType =
      type == "jpg"
          ? ImageJPEG
          : type == "png"
                ? ImagePNG
                : type == "gif"
                      ? ImageGIF
                      : type == "webp"
                            ? ImageWebP
                            : type == "ico"
                                  ? ImageICO
                                  : type == "bmp"
                                        ? ImageBMP
                                        : DecodedImageType::ImageUnknown;

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, decodedImageTypeHistogram,
      new EnumerationHistogram("Blink.DecodedImageType",
                               DecodedImageTypeEnumEnd));
  decodedImageTypeHistogram.count(decodedImageType);
}

void BitmapImageMetrics::countImageOrientation(
    const ImageOrientationEnum orientation) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, orientationHistogram,
      new EnumerationHistogram("Blink.DecodedImage.Orientation",
                               ImageOrientationEnumEnd));
  orientationHistogram.count(orientation);
}

void BitmapImageMetrics::countImageGammaAndGamut(SkColorSpace* colorSpace) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, gammaNamedHistogram,
      new EnumerationHistogram("Blink.ColorSpace.Source", GammaEnd));
  gammaNamedHistogram.count(getColorSpaceGamma(colorSpace));

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, gamutNamedHistogram,
      new EnumerationHistogram("Blink.ColorGamut.Source", GamutEnd));
  gamutNamedHistogram.count(getColorSpaceGamut(colorSpace));
}

void BitmapImageMetrics::countOutputGammaAndGamut(SkColorSpace* colorSpace) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, gammaNamedHistogram,
      new EnumerationHistogram("Blink.ColorSpace.Destination", GammaEnd));
  gammaNamedHistogram.count(getColorSpaceGamma(colorSpace));

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, gamutNamedHistogram,
      new EnumerationHistogram("Blink.ColorGamut.Destination", GamutEnd));
  gamutNamedHistogram.count(getColorSpaceGamut(colorSpace));
}

BitmapImageMetrics::Gamma BitmapImageMetrics::getColorSpaceGamma(
    SkColorSpace* colorSpace) {
  Gamma gamma = GammaNull;
  if (colorSpace) {
    if (colorSpace->gammaCloseToSRGB()) {
      gamma = GammaSRGB;
    } else if (colorSpace->gammaIsLinear()) {
      gamma = GammaLinear;
    } else {
      gamma = GammaNonStandard;
    }
  }
  return gamma;
}

BitmapImageMetrics::Gamut BitmapImageMetrics::getColorSpaceGamut(
    SkColorSpace* colorSpace) {
  sk_sp<SkColorSpace> scRGB(
      SkColorSpace::MakeNamed(SkColorSpace::kSRGBLinear_Named));
  std::unique_ptr<SkColorSpaceXform> transform(
      SkColorSpaceXform::New(colorSpace, scRGB.get()));

  if (!transform)
    return GamutUnknown;

  unsigned char in[3][4];
  float out[3][4];
  memset(in, 0, sizeof(in));
  in[0][0] = 255;
  in[1][1] = 255;
  in[2][2] = 255;
  in[0][3] = 255;
  in[1][3] = 255;
  in[2][3] = 255;
  transform->apply(SkColorSpaceXform::kRGBA_F32_ColorFormat, out,
                   SkColorSpaceXform::kRGBA_8888_ColorFormat, in, 3,
                   kOpaque_SkAlphaType);
  float score = out[0][0] * out[1][1] * out[2][2];

  if (score < 0.9)
    return GamutLessThanNTSC;
  if (score < 0.95)
    return GamutNTSC;  // actual score 0.912839
  if (score < 1.1)
    return GamutSRGB;  // actual score 1.0
  if (score < 1.3)
    return GamutAlmostP3;
  if (score < 1.425)
    return GamutP3;  // actual score 1.401899
  if (score < 1.5)
    return GamutAdobeRGB;  // actual score 1.458385
  if (score < 2.0)
    return GamutWide;
  if (score < 2.2)
    return GamutBT2020;  // actual score 2.104520
  if (score < 2.7)
    return GamutProPhoto;  // actual score 2.913247
  return GamutUltraWide;
}

}  // namespace blink
