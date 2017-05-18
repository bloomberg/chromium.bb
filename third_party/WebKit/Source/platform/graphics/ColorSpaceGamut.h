// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ColorSpaceGamut_h
#define ColorSpaceGamut_h

#include "platform/PlatformExport.h"

class SkColorSpace;

namespace blink {

struct WebScreenInfo;

enum class ColorSpaceGamut {
  // Values synced with 'Gamut' in src/tools/metrics/histograms/histograms.xml
  kUnknown = 0,
  kLessThanNTSC = 1,
  NTSC = 2,
  SRGB = 3,
  kAlmostP3 = 4,
  P3 = 5,
  kAdobeRGB = 6,
  kWide = 7,
  BT2020 = 8,
  kProPhoto = 9,
  kUltraWide = 10,
  kEnd
};

namespace ColorSpaceUtilities {

PLATFORM_EXPORT ColorSpaceGamut GetColorSpaceGamut(const WebScreenInfo&);
ColorSpaceGamut GetColorSpaceGamut(SkColorSpace*);

}  // namespace ColorSpaceUtilities

}  // namespace blink

#endif  // ColorSpaceGamut_h
