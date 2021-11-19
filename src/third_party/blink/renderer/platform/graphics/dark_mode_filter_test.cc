// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include "cc/paint/paint_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
namespace {

TEST(DarkModeFilterTest, ApplyDarkModeToColorsAndFlags) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  DarkModeFilter filter(settings);

  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(SK_ColorWHITE,
            filter.InvertColorIfNeeded(
                SK_ColorBLACK, DarkModeFilter::ElementRole::kBackground));

  EXPECT_EQ(SK_ColorWHITE,
            filter.InvertColorIfNeeded(SK_ColorBLACK,
                                       DarkModeFilter::ElementRole::kSVG));
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(SK_ColorWHITE,
                                       DarkModeFilter::ElementRole::kSVG));

  cc::PaintFlags flags;
  flags.setColor(SK_ColorWHITE);
  auto flags_or_nullopt = filter.ApplyToFlagsIfNeeded(
      flags, DarkModeFilter::ElementRole::kBackground);
  ASSERT_NE(flags_or_nullopt, absl::nullopt);
  EXPECT_EQ(SK_ColorBLACK, flags_or_nullopt.value().getColor());
}

TEST(DarkModeFilterTest, InvertedColorCacheSize) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  DarkModeFilter filter(settings);
  EXPECT_EQ(0u, filter.GetInvertedColorCacheSizeForTesting());
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());
  // Should get cached value.
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());
}

TEST(DarkModeFilterTest, InvertedColorCacheZeroMaxKeys) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  DarkModeFilter filter(settings);

  EXPECT_EQ(0u, filter.GetInvertedColorCacheSizeForTesting());
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());
  EXPECT_EQ(SK_ColorTRANSPARENT,
            filter.InvertColorIfNeeded(
                SK_ColorTRANSPARENT, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(2u, filter.GetInvertedColorCacheSizeForTesting());

  // Results returned from cache.
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(SK_ColorTRANSPARENT,
            filter.InvertColorIfNeeded(
                SK_ColorTRANSPARENT, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(2u, filter.GetInvertedColorCacheSizeForTesting());
}

TEST(DarkModeFilterTest, ImageShouldHaveFilterAppliedBasedOnSizes) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  settings.image_policy = DarkModeImagePolicy::kFilterSmart;
  DarkModeFilter filter(settings);

  // |dst| is smaller than threshold size.
  EXPECT_TRUE(filter.ImageShouldHaveFilterAppliedBasedOnSizes(
      SkIRect::MakeWH(100, 100), SkIRect::MakeWH(100, 100)));

  // |dst| is smaller than threshold size, even |src| is larger.
  EXPECT_TRUE(filter.ImageShouldHaveFilterAppliedBasedOnSizes(
      SkIRect::MakeWH(200, 200), SkIRect::MakeWH(100, 100)));

  // |dst| is smaller than threshold size, |src| is smaller.
  EXPECT_TRUE(filter.ImageShouldHaveFilterAppliedBasedOnSizes(
      SkIRect::MakeWH(20, 20), SkIRect::MakeWH(100, 100)));

  // |src| having very smaller width, even |dst| is larger than threshold size.
  EXPECT_TRUE(filter.ImageShouldHaveFilterAppliedBasedOnSizes(
      SkIRect::MakeWH(5, 200), SkIRect::MakeWH(5, 200)));

  // |src| having very smaller height, even |dst| is larger than threshold size.
  EXPECT_TRUE(filter.ImageShouldHaveFilterAppliedBasedOnSizes(
      SkIRect::MakeWH(200, 5), SkIRect::MakeWH(200, 5)));

  // |dst| is larger than threshold size.
  EXPECT_FALSE(filter.ImageShouldHaveFilterAppliedBasedOnSizes(
      SkIRect::MakeWH(20, 20), SkIRect::MakeWH(200, 200)));

  // |dst| is larger than threshold size.
  EXPECT_FALSE(filter.ImageShouldHaveFilterAppliedBasedOnSizes(
      SkIRect::MakeWH(20, 200), SkIRect::MakeWH(20, 200)));
}

}  // namespace
}  // namespace blink
