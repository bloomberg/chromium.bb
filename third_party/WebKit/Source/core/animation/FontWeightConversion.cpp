// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/FontWeightConversion.h"

#include "platform/wtf/MathExtras.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

double fontWeightToDouble(FontWeight fontWeight) {
  switch (fontWeight) {
    case FontWeight100:
      return 100;
    case FontWeight200:
      return 200;
    case FontWeight300:
      return 300;
    case FontWeight400:
      return 400;
    case FontWeight500:
      return 500;
    case FontWeight600:
      return 600;
    case FontWeight700:
      return 700;
    case FontWeight800:
      return 800;
    case FontWeight900:
      return 900;
    default:
      NOTREACHED();
      return 400;
  }
}

FontWeight doubleToFontWeight(double value) {
  static const FontWeight fontWeights[] = {
      FontWeight100, FontWeight200, FontWeight300, FontWeight400, FontWeight500,
      FontWeight600, FontWeight700, FontWeight800, FontWeight900,
  };

  int index = round(value / 100 - 1);
  int clampedIndex = clampTo<int>(index, 0, WTF_ARRAY_LENGTH(fontWeights) - 1);
  return fontWeights[clampedIndex];
}

}  // namespace blink
