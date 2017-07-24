// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSStyleImageValue.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class FakeCSSStyleImageValue : public CSSStyleImageValue {
 public:
  FakeCSSStyleImageValue(CSSImageValue* image_value,
                         bool cache_pending,
                         LayoutSize layout_size)
      : CSSStyleImageValue(image_value),
        cache_pending_(cache_pending),
        layout_size_(layout_size) {}

  bool IsCachePending() const override { return cache_pending_; }
  LayoutSize ImageLayoutSize() const override { return layout_size_; }

  const CSSValue* ToCSSValue() const override { return nullptr; }
  StyleValueType GetType() const override { return kUnknownType; }

 private:
  bool cache_pending_;
  LayoutSize layout_size_;
};

}  // namespace

TEST(CSSStyleImageValueTest, PendingCache) {
  FakeCSSStyleImageValue* style_image_value = new FakeCSSStyleImageValue(
      CSSImageValue::Create(""), true, LayoutSize(100, 100));
  bool is_null;
  EXPECT_EQ(style_image_value->intrinsicWidth(is_null), 0);
  EXPECT_EQ(style_image_value->intrinsicHeight(is_null), 0);
  EXPECT_EQ(style_image_value->intrinsicRatio(is_null), 0);
  EXPECT_TRUE(is_null);
}

TEST(CSSStyleImageValueTest, ValidLoadedImage) {
  FakeCSSStyleImageValue* style_image_value = new FakeCSSStyleImageValue(
      CSSImageValue::Create(""), false, LayoutSize(480, 120));
  bool is_null;
  EXPECT_EQ(style_image_value->intrinsicWidth(is_null), 480);
  EXPECT_EQ(style_image_value->intrinsicHeight(is_null), 120);
  EXPECT_EQ(style_image_value->intrinsicRatio(is_null), 4);
  EXPECT_FALSE(is_null);
}

}  // namespace blink
