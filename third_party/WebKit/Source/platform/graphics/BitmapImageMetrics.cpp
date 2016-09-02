// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/BitmapImageMetrics.h"

#include "platform/Histogram.h"
#include "wtf/Threading.h"
#include "wtf/text/WTFString.h"

namespace blink {

void BitmapImageMetrics::countDecodedImageType(const String& type)
{
    DecodedImageType decodedImageType =
        type == "jpg"  ? ImageJPEG :
        type == "png"  ? ImagePNG  :
        type == "gif"  ? ImageGIF  :
        type == "webp" ? ImageWebP :
        type == "ico"  ? ImageICO  :
        type == "bmp"  ? ImageBMP  : DecodedImageType::ImageUnknown;

    DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, decodedImageTypeHistogram, new EnumerationHistogram("Blink.DecodedImageType", DecodedImageTypeEnumEnd));
    decodedImageTypeHistogram.count(decodedImageType);
}

void BitmapImageMetrics::countImageOrientation(const ImageOrientationEnum orientation)
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, orientationHistogram, new EnumerationHistogram("Blink.DecodedImage.Orientation", ImageOrientationEnumEnd));
    orientationHistogram.count(orientation);
}

void BitmapImageMetrics::countGamma(SkColorSpace* colorSpace)
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, gammaNamedHistogram, new EnumerationHistogram("Blink.ColorSpace.Destination", GammaEnd));

    if (colorSpace) {
        SkColorSpace::GammaNamed skGamma = colorSpace->gammaNamed();

        Gamma gamma;
        switch (skGamma) {
        case SkColorSpace::kLinear_GammaNamed:
            gamma = GammaLinear;
            break;
        case SkColorSpace::kSRGB_GammaNamed:
            gamma = GammaSRGB;
            break;
        case SkColorSpace::k2Dot2Curve_GammaNamed:
            gamma = Gamma2Dot2;
            break;
        default:
            if (colorSpace->gammasAreMatching()) {
                if (colorSpace->gammasAreValues()) {
                    gamma = GammaExponent;
                } else if (colorSpace->gammasAreParams()) {
                    gamma = GammaParametric;
                } else if (colorSpace->gammasAreTables()) {
                    gamma = GammaTable;
                } else if (colorSpace->gammasAreNamed()) {
                    gamma = GammaNamed;
                } else {
                    gamma = GammaFail;
                }
            } else {
                gamma = GammaNonStandard;
            }
            break;
        }

        gammaNamedHistogram.count(gamma);
    } else {
        gammaNamedHistogram.count(GammaNull);
    }
}

} // namespace blink
