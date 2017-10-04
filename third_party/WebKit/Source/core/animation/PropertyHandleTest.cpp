// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/PropertyHandle.h"

#include "core/svg_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using SVGNames::amplitudeAttr;
using SVGNames::exponentAttr;

class PropertyHandleTest : public ::testing::Test {};

TEST_F(PropertyHandleTest, Equality) {
  AtomicString name_a = "--a";
  AtomicString name_b = "--b";

  EXPECT_TRUE(PropertyHandle(CSSPropertyOpacity) ==
              PropertyHandle(CSSPropertyOpacity));
  EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity) !=
               PropertyHandle(CSSPropertyOpacity));
  EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity) ==
               PropertyHandle(CSSPropertyTransform));
  EXPECT_TRUE(PropertyHandle(CSSPropertyOpacity) !=
              PropertyHandle(CSSPropertyTransform));
  EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity) == PropertyHandle(name_a));
  EXPECT_TRUE(PropertyHandle(CSSPropertyOpacity) != PropertyHandle(name_a));
  EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity) ==
               PropertyHandle(amplitudeAttr));
  EXPECT_TRUE(PropertyHandle(CSSPropertyOpacity) !=
              PropertyHandle(amplitudeAttr));

  EXPECT_FALSE(PropertyHandle(name_a) == PropertyHandle(CSSPropertyOpacity));
  EXPECT_TRUE(PropertyHandle(name_a) != PropertyHandle(CSSPropertyOpacity));
  EXPECT_FALSE(PropertyHandle(name_a) == PropertyHandle(CSSPropertyTransform));
  EXPECT_TRUE(PropertyHandle(name_a) != PropertyHandle(CSSPropertyTransform));
  EXPECT_TRUE(PropertyHandle(name_a) == PropertyHandle(name_a));
  EXPECT_FALSE(PropertyHandle(name_a) != PropertyHandle(name_a));
  EXPECT_FALSE(PropertyHandle(name_a) == PropertyHandle(name_b));
  EXPECT_TRUE(PropertyHandle(name_a) != PropertyHandle(name_b));
  EXPECT_FALSE(PropertyHandle(name_a) == PropertyHandle(amplitudeAttr));
  EXPECT_TRUE(PropertyHandle(name_a) != PropertyHandle(amplitudeAttr));

  EXPECT_FALSE(PropertyHandle(amplitudeAttr) ==
               PropertyHandle(CSSPropertyOpacity));
  EXPECT_TRUE(PropertyHandle(amplitudeAttr) !=
              PropertyHandle(CSSPropertyOpacity));
  EXPECT_FALSE(PropertyHandle(amplitudeAttr) == PropertyHandle(name_a));
  EXPECT_TRUE(PropertyHandle(amplitudeAttr) != PropertyHandle(name_a));
  EXPECT_TRUE(PropertyHandle(amplitudeAttr) == PropertyHandle(amplitudeAttr));
  EXPECT_FALSE(PropertyHandle(amplitudeAttr) != PropertyHandle(amplitudeAttr));
  EXPECT_FALSE(PropertyHandle(amplitudeAttr) == PropertyHandle(exponentAttr));
  EXPECT_TRUE(PropertyHandle(amplitudeAttr) != PropertyHandle(exponentAttr));
}

TEST_F(PropertyHandleTest, Hash) {
  AtomicString name_a = "--a";
  AtomicString name_b = "--b";

  EXPECT_TRUE(PropertyHandle(CSSPropertyOpacity).GetHash() ==
              PropertyHandle(CSSPropertyOpacity).GetHash());
  EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity).GetHash() ==
               PropertyHandle(name_a).GetHash());
  EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity).GetHash() ==
               PropertyHandle(CSSPropertyTransform).GetHash());
  EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity).GetHash() ==
               PropertyHandle(amplitudeAttr).GetHash());

  EXPECT_FALSE(PropertyHandle(name_a).GetHash() ==
               PropertyHandle(CSSPropertyOpacity).GetHash());
  EXPECT_TRUE(PropertyHandle(name_a).GetHash() ==
              PropertyHandle(name_a).GetHash());
  EXPECT_FALSE(PropertyHandle(name_a).GetHash() ==
               PropertyHandle(name_b).GetHash());
  EXPECT_FALSE(PropertyHandle(name_a).GetHash() ==
               PropertyHandle(exponentAttr).GetHash());

  EXPECT_FALSE(PropertyHandle(amplitudeAttr).GetHash() ==
               PropertyHandle(CSSPropertyOpacity).GetHash());
  EXPECT_FALSE(PropertyHandle(amplitudeAttr).GetHash() ==
               PropertyHandle(name_a).GetHash());
  EXPECT_TRUE(PropertyHandle(amplitudeAttr).GetHash() ==
              PropertyHandle(amplitudeAttr).GetHash());
  EXPECT_FALSE(PropertyHandle(amplitudeAttr).GetHash() ==
               PropertyHandle(exponentAttr).GetHash());
}

TEST_F(PropertyHandleTest, Accessors) {
  AtomicString name = "--x";

  EXPECT_TRUE(PropertyHandle(CSSPropertyOpacity).IsCSSProperty());
  EXPECT_TRUE(PropertyHandle(name).IsCSSProperty());
  EXPECT_FALSE(PropertyHandle(amplitudeAttr).IsCSSProperty());

  EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity).IsSVGAttribute());
  EXPECT_FALSE(PropertyHandle(name).IsSVGAttribute());
  EXPECT_TRUE(PropertyHandle(amplitudeAttr).IsSVGAttribute());

  EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity).IsCSSCustomProperty());
  EXPECT_TRUE(PropertyHandle(name).IsCSSCustomProperty());
  EXPECT_FALSE(PropertyHandle(amplitudeAttr).IsCSSCustomProperty());

  EXPECT_EQ(PropertyHandle(CSSPropertyOpacity).CssProperty(),
            CSSPropertyOpacity);
  EXPECT_EQ(PropertyHandle(name).CssProperty(), CSSPropertyVariable);
  EXPECT_EQ(PropertyHandle(name).CustomPropertyName(), name);
  EXPECT_EQ(PropertyHandle(amplitudeAttr).SvgAttribute(), amplitudeAttr);
}

}  // namespace blink
