// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/FontWeightConversion.h"

#include "platform/wtf/MathExtras.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

double FontWeightToDouble(FontWeight font_weight) {
  switch (font_weight) {
    case kFontWeight100:
      return 100;
    case kFontWeight200:
      return 200;
    case kFontWeight300:
      return 300;
    case kFontWeight400:
      return 400;
    case kFontWeight500:
      return 500;
    case kFontWeight600:
      return 600;
    case kFontWeight700:
      return 700;
    case kFontWeight800:
      return 800;
    case kFontWeight900:
      return 900;
    default:
      NOTREACHED();
      return 400;
  }
}

FontWeight DoubleToFontWeight(double value) {
  static const FontWeight kFontWeights[] = {
      kFontWeight100, kFontWeight200, kFontWeight300,
      kFontWeight400, kFontWeight500, kFontWeight600,
      kFontWeight700, kFontWeight800, kFontWeight900,
  };

  int index = round(value / 100 - 1);
  int clamped_index =
      clampTo<int>(index, 0, WTF_ARRAY_LENGTH(kFontWeights) - 1);
  return kFontWeights[clamped_index];
}

}  // namespace blink
