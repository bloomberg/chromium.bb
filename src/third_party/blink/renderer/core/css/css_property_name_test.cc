// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"

namespace blink {

TEST(CSSPropertyNameTest, IdStandardProperty) {
  CSSPropertyName name(CSSPropertyFontSize);
  EXPECT_EQ(CSSPropertyFontSize, name.Id());
}

TEST(CSSPropertyNameTest, IdCustomProperty) {
  CSSPropertyName name(AtomicString("--x"));
  EXPECT_EQ(CSSPropertyVariable, name.Id());
  EXPECT_TRUE(name.IsCustomProperty());
}

TEST(CSSPropertyNameTest, GetNameStandardProperty) {
  CSSPropertyName name(CSSPropertyFontSize);
  EXPECT_EQ(AtomicString("font-size"), name.ToAtomicString());
}

TEST(CSSPropertyNameTest, GetNameCustomProperty) {
  CSSPropertyName name(AtomicString("--x"));
  EXPECT_EQ(AtomicString("--x"), name.ToAtomicString());
}

TEST(CSSPropertyNameTest, OperatorEquals) {
  EXPECT_EQ(CSSPropertyName("--x"), CSSPropertyName("--x"));
  EXPECT_EQ(CSSPropertyName(CSSPropertyColor),
            CSSPropertyName(CSSPropertyColor));
  EXPECT_NE(CSSPropertyName("--x"), CSSPropertyName("--y"));
  EXPECT_NE(CSSPropertyName(CSSPropertyColor),
            CSSPropertyName(CSSPropertyBackgroundColor));
}

TEST(CSSPropertyNameTest, From) {
  EXPECT_TRUE(CSSPropertyName::From("color"));
  EXPECT_TRUE(CSSPropertyName::From("--x"));
  EXPECT_FALSE(CSSPropertyName::From("notaproperty"));
  EXPECT_FALSE(CSSPropertyName::From("-not-a-property"));

  EXPECT_EQ(*CSSPropertyName::From("color"), CSSPropertyName(CSSPropertyColor));
  EXPECT_EQ(*CSSPropertyName::From("--x"), CSSPropertyName("--x"));
}

TEST(CSSPropertyNameTest, FromNativeCSSProperty) {
  CSSPropertyName name = GetCSSPropertyFontSize().GetCSSPropertyName();
  EXPECT_EQ(CSSPropertyName(CSSPropertyFontSize), name);
}

}  // namespace blink
