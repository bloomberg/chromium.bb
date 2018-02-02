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
                         IntSize size)
      : CSSStyleImageValue(image_value),
        cache_pending_(cache_pending),
        size_(size) {}

  bool IsCachePending() const final { return cache_pending_; }
  IntSize ImageSize() const final { return size_; }

  ResourceStatus Status() const final {
    if (IsCachePending())
      return ResourceStatus::kNotStarted;
    return ResourceStatus::kCached;
  }

  const CSSValue* ToCSSValue() const final { return nullptr; }
  StyleValueType GetType() const final { return kUnknownType; }

 private:
  bool cache_pending_;
  IntSize size_;
};

}  // namespace

TEST(CSSStyleImageValueTest, PendingCache) {
  FakeCSSStyleImageValue* style_image_value = new FakeCSSStyleImageValue(
      CSSImageValue::Create(""), true, IntSize(100, 100));
  bool is_null;
  EXPECT_EQ(style_image_value->intrinsicWidth(is_null), 0);
  EXPECT_EQ(style_image_value->intrinsicHeight(is_null), 0);
  EXPECT_EQ(style_image_value->intrinsicRatio(is_null), 0);
  EXPECT_TRUE(is_null);
}

TEST(CSSStyleImageValueTest, ValidLoadedImage) {
  FakeCSSStyleImageValue* style_image_value = new FakeCSSStyleImageValue(
      CSSImageValue::Create(""), false, IntSize(480, 120));
  bool is_null;
  EXPECT_EQ(style_image_value->intrinsicWidth(is_null), 480);
  EXPECT_EQ(style_image_value->intrinsicHeight(is_null), 120);
  EXPECT_EQ(style_image_value->intrinsicRatio(is_null), 4);
  EXPECT_FALSE(is_null);
}

}  // namespace blink
