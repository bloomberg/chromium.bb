// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/SVGComputedStyle.h"

#include "core/style/StyleDifference.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// Ensures RefPtr values are compared by their values, not by pointers.
#define TEST_STYLE_REFPTR_VALUE_NO_DIFF(type, fieldName)               \
  {                                                                    \
    scoped_refptr<SVGComputedStyle> svg1 = SVGComputedStyle::Create(); \
    scoped_refptr<SVGComputedStyle> svg2 = SVGComputedStyle::Create(); \
    scoped_refptr<type> value1 = type::Create();                       \
    scoped_refptr<type> value2 = value1->Copy();                       \
    svg1->Set##fieldName(value1);                                      \
    svg2->Set##fieldName(value2);                                      \
    EXPECT_FALSE(svg1->Diff(svg2.get()).HasDifference());              \
  }

// This is not very useful for fields directly stored by values, because they
// can only be compared by values. This macro mainly ensures that we update the
// comparisons and tests when we change some field to RefPtr in the future.
#define TEST_STYLE_VALUE_NO_DIFF(type, fieldName)                      \
  {                                                                    \
    scoped_refptr<SVGComputedStyle> svg1 = SVGComputedStyle::Create(); \
    scoped_refptr<SVGComputedStyle> svg2 = SVGComputedStyle::Create(); \
    svg1->Set##fieldName(SVGComputedStyle::Initial##fieldName());      \
    svg2->Set##fieldName(SVGComputedStyle::Initial##fieldName());      \
    EXPECT_FALSE(svg1->Diff(svg2.get()).HasDifference());              \
  }

TEST(SVGComputedStyleTest, StrokeStyleShouldCompareValue) {
  TEST_STYLE_VALUE_NO_DIFF(float, StrokeOpacity);
  TEST_STYLE_VALUE_NO_DIFF(float, StrokeMiterLimit);
  TEST_STYLE_VALUE_NO_DIFF(UnzoomedLength, StrokeWidth);
  TEST_STYLE_VALUE_NO_DIFF(Length, StrokeDashOffset);
  TEST_STYLE_REFPTR_VALUE_NO_DIFF(SVGDashArray, StrokeDashArray);

  {
    scoped_refptr<SVGComputedStyle> svg1 = SVGComputedStyle::Create();
    scoped_refptr<SVGComputedStyle> svg2 = SVGComputedStyle::Create();
    svg1->SetStrokePaint(SVGComputedStyle::InitialStrokePaintType(),
                         SVGComputedStyle::InitialStrokePaintColor(),
                         SVGComputedStyle::InitialStrokePaintUri(), true,
                         false);
    svg2->SetStrokePaint(SVGComputedStyle::InitialStrokePaintType(),
                         SVGComputedStyle::InitialStrokePaintColor(),
                         SVGComputedStyle::InitialStrokePaintUri(), true,
                         false);
    EXPECT_FALSE(svg1->Diff(svg2.get()).HasDifference());
  }
  {
    scoped_refptr<SVGComputedStyle> svg1 = SVGComputedStyle::Create();
    scoped_refptr<SVGComputedStyle> svg2 = SVGComputedStyle::Create();
    svg1->SetStrokePaint(SVGComputedStyle::InitialStrokePaintType(),
                         SVGComputedStyle::InitialStrokePaintColor(),
                         SVGComputedStyle::InitialStrokePaintUri(), false,
                         true);
    svg2->SetStrokePaint(SVGComputedStyle::InitialStrokePaintType(),
                         SVGComputedStyle::InitialStrokePaintColor(),
                         SVGComputedStyle::InitialStrokePaintUri(), false,
                         true);
    EXPECT_FALSE(svg1->Diff(svg2.get()).HasDifference());
  }
}

TEST(SVGComputedStyleTest, MiscStyleShouldCompareValue) {
  TEST_STYLE_VALUE_NO_DIFF(Color, FloodColor);
  TEST_STYLE_VALUE_NO_DIFF(float, FloodOpacity);
  TEST_STYLE_VALUE_NO_DIFF(Color, LightingColor);
  TEST_STYLE_VALUE_NO_DIFF(Length, BaselineShiftValue);
}

}  // namespace blink
