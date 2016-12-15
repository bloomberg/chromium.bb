// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BitmapImageMetrics_h
#define BitmapImageMetrics_h

#include "platform/PlatformExport.h"
#include "platform/graphics/ImageOrientation.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"

namespace blink {

class PLATFORM_EXPORT BitmapImageMetrics {
  STATIC_ONLY(BitmapImageMetrics);

 public:
  // Values synced with 'DecodedImageType' in
  // src/tools/metrics/histograms/histograms.xml
  enum DecodedImageType {
    ImageUnknown = 0,
    ImageJPEG = 1,
    ImagePNG = 2,
    ImageGIF = 3,
    ImageWebP = 4,
    ImageICO = 5,
    ImageBMP = 6,
    DecodedImageTypeEnumEnd = ImageBMP + 1
  };

  enum Gamma {
    // Values synced with 'Gamma' in src/tools/metrics/histograms/histograms.xml
    GammaLinear = 0,
    GammaSRGB = 1,
    Gamma2Dot2 = 2,
    GammaNonStandard = 3,
    GammaNull = 4,
    GammaFail = 5,
    GammaInvalid = 6,
    GammaExponent = 7,
    GammaTable = 8,
    GammaParametric = 9,
    GammaNamed = 10,
    GammaEnd = GammaNamed + 1,
  };

  enum Gamut {
    // Values synced with 'Gamut' in src/tools/metrics/histograms/histograms.xml
    GamutUnknown = 0,
    GamutLessThanNTSC = 1,
    GamutNTSC = 2,
    GamutSRGB = 3,
    GamutAlmostP3 = 4,
    GamutP3 = 5,
    GamutAdobeRGB = 6,
    GamutWide = 7,
    GamutBT2020 = 8,
    GamutProPhoto = 9,
    GamutUltraWide = 10,
    GamutEnd
  };

  static void countDecodedImageType(const String& type);
  static void countImageOrientation(const ImageOrientationEnum);
  static void countImageGammaAndGamut(SkColorSpace*);
  static void countOutputGammaAndGamut(SkColorSpace*);

 private:
  static Gamma getColorSpaceGamma(SkColorSpace*);
  static Gamut getColorSpaceGamut(SkColorSpace*);
};

}  // namespace blink

#endif
