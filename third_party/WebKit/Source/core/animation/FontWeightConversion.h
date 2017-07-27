// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontWeightConversion_h
#define FontWeightConversion_h

#include "platform/fonts/FontTraits.h"

namespace blink {

double FontWeightToDouble(FontWeight);
FontWeight DoubleToFontWeight(double value);

}  // namespace blink

#endif  // FontWeightConversion_h
