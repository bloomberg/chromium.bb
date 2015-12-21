// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/BitmapImageMetrics.h"

#include "public/platform/Platform.h"
#include "wtf/text/WTFString.h"

namespace blink {

void BitmapImageMetrics::countDecodedImageType(const String& type)
{
    enum DecodedImageType { // Values synced with 'DecodedImageType' in src/tools/metrics/histograms/histograms.xml
        ImageUnknown = 0,
        ImageJPEG = 1,
        ImagePNG = 2,
        ImageGIF = 3,
        ImageWebP = 4,
        ImageICO = 5,
        ImageBMP = 6,
        DecodedImageTypeEnumEnd = ImageBMP + 1
    };

    DecodedImageType decodedImageType =
        type == "jpg"  ? ImageJPEG :
        type == "png"  ? ImagePNG  :
        type == "gif"  ? ImageGIF  :
        type == "webp" ? ImageWebP :
        type == "ico"  ? ImageICO  :
        type == "bmp"  ? ImageBMP  : DecodedImageType::ImageUnknown;

    Platform::current()->histogramEnumeration("Blink.DecodedImageType", decodedImageType, DecodedImageTypeEnumEnd);
}

void BitmapImageMetrics::countImageOrientation(const ImageOrientationEnum orientation)
{
    Platform::current()->histogramEnumeration("Blink.DecodedImage.Orientation", orientation, ImageOrientationEnumEnd);
}

} // namespace blink
