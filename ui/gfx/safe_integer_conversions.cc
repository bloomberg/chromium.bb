// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/safe_integer_conversions.h"

#include <cmath>
#include <limits>

namespace gfx {

int ClampToInt(float value) {
  if (value != value)
     return 0; // no int NaN.
  if (value >= std::numeric_limits<int>::max())
    return std::numeric_limits<int>::max();
  if (value <= std::numeric_limits<int>::min())
    return std::numeric_limits<int>::min();
  return static_cast<int>(value);
}

int ToFlooredInt(float value) {
  return ClampToInt(std::floor(value));
}

int ToCeiledInt(float value) {
  return ClampToInt(std::ceil(value));
}

int ToRoundedInt(float value) {
  float rounded;
  if (value >= 0.0f)
    rounded = std::floor(value + 0.5f);
  else
    rounded = std::ceil(value - 0.5f);
  return ClampToInt(rounded);
}

}  // namespace gfx
