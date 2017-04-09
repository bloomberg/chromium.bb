// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BitmapImageMetrics_h
#define BitmapImageMetrics_h

#include "platform/PlatformExport.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace blink {

class PLATFORM_EXPORT BitmapImageMetrics {
  STATIC_ONLY(BitmapImageMetrics);

 public:
  // Values synced with 'DecodedImageType' in
  // src/tools/metrics/histograms/histograms.xml
  enum DecodedImageType {
    kImageUnknown = 0,
    kImageJPEG = 1,
    kImagePNG = 2,
    kImageGIF = 3,
    kImageWebP = 4,
    kImageICO = 5,
    kImageBMP = 6,
    kDecodedImageTypeEnumEnd = kImageBMP + 1
  };

  enum Gamma {
    // Values synced with 'Gamma' in src/tools/metrics/histograms/histograms.xml
    kGammaLinear = 0,
    kGammaSRGB = 1,
    kGamma2Dot2 = 2,
    kGammaNonStandard = 3,
    kGammaNull = 4,
    kGammaFail = 5,
    kGammaInvalid = 6,
    kGammaExponent = 7,
    kGammaTable = 8,
    kGammaParametric = 9,
    kGammaNamed = 10,
    kGammaEnd = kGammaNamed + 1,
  };

  static void CountDecodedImageType(const String& type);
  static void CountImageOrientation(const ImageOrientationEnum);
  static void CountImageGammaAndGamut(SkColorSpace*);
  static void CountOutputGammaAndGamut(SkColorSpace*);

 private:
  static Gamma GetColorSpaceGamma(SkColorSpace*);
};

}  // namespace blink

#endif
