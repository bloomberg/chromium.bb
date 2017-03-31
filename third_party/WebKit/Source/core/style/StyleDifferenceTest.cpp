// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StyleDifference.h"

#include <sstream>
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(StyleDifferenceTest, StreamOutputDefault) {
  std::stringstream stringStream;
  StyleDifference diff;
  stringStream << diff;
  EXPECT_EQ(
      "StyleDifference{layoutType=NoLayout, "
      "paintInvalidationType=NoPaintInvalidation, recomputeOverflow=0, "
      "visualRectUpdate=0, propertySpecificDifferences=, "
      "scrollAnchorDisablingPropertyChanged=0}",
      stringStream.str());
}

TEST(StyleDifferenceTest, StreamOutputAllFieldsMutated) {
  std::stringstream stringStream;
  StyleDifference diff;
  diff.setNeedsPaintInvalidationObject();
  diff.setNeedsPositionedMovementLayout();
  diff.setNeedsRecomputeOverflow();
  diff.setNeedsVisualRectUpdate();
  diff.setTransformChanged();
  diff.setScrollAnchorDisablingPropertyChanged();
  stringStream << diff;
  EXPECT_EQ(
      "StyleDifference{layoutType=PositionedMovement, "
      "paintInvalidationType=PaintInvalidationObject, recomputeOverflow=1, "
      "visualRectUpdate=1, propertySpecificDifferences=TransformChanged, "
      "scrollAnchorDisablingPropertyChanged=1}",
      stringStream.str());
}

TEST(StyleDifferenceTest, StreamOutputSetAllProperties) {
  std::stringstream stringStream;
  StyleDifference diff;
  diff.setTransformChanged();
  diff.setOpacityChanged();
  diff.setZIndexChanged();
  diff.setFilterChanged();
  diff.setBackdropFilterChanged();
  diff.setCSSClipChanged();
  diff.setTextDecorationOrColorChanged();
  stringStream << diff;
  EXPECT_EQ(
      "StyleDifference{layoutType=NoLayout, "
      "paintInvalidationType=NoPaintInvalidation, recomputeOverflow=0, "
      "visualRectUpdate=0, "
      "propertySpecificDifferences=TransformChanged|OpacityChanged|"
      "ZIndexChanged|FilterChanged|BackdropFilterChanged|CSSClipChanged|"
      "TextDecorationOrColorChanged, scrollAnchorDisablingPropertyChanged=0}",
      stringStream.str());
}

}  // namespace blink
