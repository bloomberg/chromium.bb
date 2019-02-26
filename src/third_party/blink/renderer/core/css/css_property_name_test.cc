// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace blink
