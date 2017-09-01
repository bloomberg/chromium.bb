// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FloatShapeHelpers_h
#define FloatShapeHelpers_h

// Code that is useful for both FloatPolygon and FloatQuad.

#include "platform/geometry/FloatSize.h"

namespace blink {
inline float Determinant(const FloatSize& a, const FloatSize& b) {
  return a.Width() * b.Height() - a.Height() * b.Width();
}
}  // namespace blink

#endif  // FloatShapeHelpers_h
