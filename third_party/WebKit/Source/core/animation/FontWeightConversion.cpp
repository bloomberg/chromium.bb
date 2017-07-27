// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/FontWeightConversion.h"

#include "platform/wtf/MathExtras.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

FontSelectionValue DoubleToFontWeight(double value) {
  // Until we allow continuous animations for font weight in a subsequent CL
  // (crbug.com/739334), convert to a discrete font weight value here.
  static const FontSelectionValue kFontWeights[] = {
      FontSelectionValue(100), FontSelectionValue(200), FontSelectionValue(300),
      FontSelectionValue(400), FontSelectionValue(500), FontSelectionValue(600),
      FontSelectionValue(700), FontSelectionValue(800), FontSelectionValue(900),
  };

  int index = round(value / 100 - 1);
  int clamped_index =
      clampTo<int>(index, 0, WTF_ARRAY_LENGTH(kFontWeights) - 1);
  return kFontWeights[clamped_index];
}

}  // namespace blink
