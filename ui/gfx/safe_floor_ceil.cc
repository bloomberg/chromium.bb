// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>

namespace gfx {

int ClampToInt(float value) {
  if (value != value)
     return 0; // no int NaN.
  if (value > std::numeric_limits<int>::max())
    return std::numeric_limits<int>::max();
  if (value < std::numeric_limits<int>::min())
    return std::numeric_limits<int>::min();
  return static_cast<int>(value);
}

int ToFlooredInt(float value) {
  return ClampToInt(std::floor(value));
}

int ToCeiledInt(float value) {
  return ClampToInt(std::ceil(value));
}

}  // namespace gfx

