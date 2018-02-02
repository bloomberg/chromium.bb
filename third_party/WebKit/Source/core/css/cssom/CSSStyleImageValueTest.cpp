// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSStyleImageValue.h"

#include "platform/graphics/Image.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class FakeCSSStyleImageValue : public CSSStyleImageValue {
 public:
  FakeCSSStyleImageValue(bool cache_pending, IntSize size)
      : cache_pending_(cache_pending), size_(size) {}

  // CSSStyleImageValue
  WTF::Optional<IntSize> IntrinsicSize() const final {
    if (cache_pending_)
      return WTF::nullopt;
    return size_;
  }

  // CanvasImageSource
  scoped_refptr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                               AccelerationHint,
                                               const FloatSize&) final {
    return nullptr;
  }
  ResourceStatus Status() const final {
    if (cache_pending_)
      return ResourceStatus::kNotStarted;
    return ResourceStatus::kCached;
  }
  bool IsAccelerated() const final { return false; }

  // CSSStyleValue
  const CSSValue* ToCSSValue() const final { return nullptr; }
  StyleValueType GetType() const final { return kUnknownType; }

 private:
  bool cache_pending_;
  IntSize size_;
};

}  // namespace

TEST(CSSStyleImageValueTest, PendingCache) {
  FakeCSSStyleImageValue style_image_value(true, IntSize(100, 100));
  bool is_null;
  EXPECT_EQ(style_image_value.intrinsicWidth(is_null), 0);
  EXPECT_EQ(style_image_value.intrinsicHeight(is_null), 0);
  EXPECT_EQ(style_image_value.intrinsicRatio(is_null), 0);
  EXPECT_TRUE(is_null);
}

TEST(CSSStyleImageValueTest, ValidLoadedImage) {
  FakeCSSStyleImageValue style_image_value(false, IntSize(480, 120));
  bool is_null;
  EXPECT_EQ(style_image_value.intrinsicWidth(is_null), 480);
  EXPECT_EQ(style_image_value.intrinsicHeight(is_null), 120);
  EXPECT_EQ(style_image_value.intrinsicRatio(is_null), 4);
  EXPECT_FALSE(is_null);
}

}  // namespace blink
