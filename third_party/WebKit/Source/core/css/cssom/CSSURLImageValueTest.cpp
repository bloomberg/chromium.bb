// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSURLImageValue.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

void CheckNullURLImageValue(CSSURLImageValue* url_image_value) {
  EXPECT_EQ(url_image_value->state(), "unloaded");
  bool is_null;
  EXPECT_EQ(url_image_value->intrinsicWidth(is_null), 0);
  EXPECT_EQ(url_image_value->intrinsicHeight(is_null), 0);
  EXPECT_EQ(url_image_value->intrinsicRatio(is_null), 0);
  EXPECT_TRUE(is_null);
}

}  // namespace

TEST(CSSURLImageValueTest, CreateURLImageValueFromURL) {
  CheckNullURLImageValue(CSSURLImageValue::Create("http://localhost"));
}

TEST(CSSURLImageValueTest, CreateURLImageValueFromImageValue) {
  CheckNullURLImageValue(
      CSSURLImageValue::Create(CSSImageValue::Create("http://localhost")));
}

}  // namespace blink
