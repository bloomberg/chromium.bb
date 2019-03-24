// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"

namespace blink {

TEST(CSSPropertyNameTest, IdStandardProperty) {
  CSSPropertyName name(CSSPropertyID::kFontSize);
  EXPECT_EQ(CSSPropertyID::kFontSize, name.Id());
}

TEST(CSSPropertyNameTest, IdCustomProperty) {
  CSSPropertyName name(AtomicString("--x"));
  EXPECT_EQ(CSSPropertyID::kVariable, name.Id());
  EXPECT_TRUE(name.IsCustomProperty());
}

TEST(CSSPropertyNameTest, GetNameStandardProperty) {
  CSSPropertyName name(CSSPropertyID::kFontSize);
  EXPECT_EQ(AtomicString("font-size"), name.ToAtomicString());
}

TEST(CSSPropertyNameTest, GetNameCustomProperty) {
  CSSPropertyName name(AtomicString("--x"));
  EXPECT_EQ(AtomicString("--x"), name.ToAtomicString());
}

TEST(CSSPropertyNameTest, OperatorEquals) {
  EXPECT_EQ(CSSPropertyName("--x"), CSSPropertyName("--x"));
  EXPECT_EQ(CSSPropertyName(CSSPropertyID::kColor),
            CSSPropertyName(CSSPropertyID::kColor));
  EXPECT_NE(CSSPropertyName("--x"), CSSPropertyName("--y"));
  EXPECT_NE(CSSPropertyName(CSSPropertyID::kColor),
            CSSPropertyName(CSSPropertyID::kBackgroundColor));
}

TEST(CSSPropertyNameTest, From) {
  EXPECT_TRUE(CSSPropertyName::From("color"));
  EXPECT_TRUE(CSSPropertyName::From("--x"));
  EXPECT_FALSE(CSSPropertyName::From("notaproperty"));
  EXPECT_FALSE(CSSPropertyName::From("-not-a-property"));

  EXPECT_EQ(*CSSPropertyName::From("color"),
            CSSPropertyName(CSSPropertyID::kColor));
  EXPECT_EQ(*CSSPropertyName::From("--x"), CSSPropertyName("--x"));
}

TEST(CSSPropertyNameTest, FromNativeCSSProperty) {
  CSSPropertyName name = GetCSSPropertyFontSize().GetCSSPropertyName();
  EXPECT_EQ(CSSPropertyName(CSSPropertyID::kFontSize), name);
}

}  // namespace blink
