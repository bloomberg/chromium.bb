// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PictureMatchers_h
#define PictureMatchers_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/Color.h"
#include "testing/gmock/include/gmock/gmock.h"

class SkPicture;

namespace blink {

class FloatRect;

// Matches if the picture draws exactly one rectangle, which (after accounting
// for the total transformation matrix and applying any clips inside that
// transform) matches the rect provided, and whose paint has the color
// requested.
// Note that clips which appear outside of a transform are not currently
// supported.
::testing::Matcher<const SkPicture&> drawsRectangle(const FloatRect&, Color);

struct RectWithColor {
  RectWithColor(const FloatRect& rectArg, const Color& colorArg)
      : rect(rectArg), color(colorArg) {}
  FloatRect rect;
  Color color;
};

// Same as above, but matches a number of rectangles equal to the size of the
// given vector.
::testing::Matcher<const SkPicture&> drawsRectangles(
    const Vector<RectWithColor>&);

}  // namespace blink

#endif  // PictureMatchers_h
