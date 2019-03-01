// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/apply_high_contrast_check.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

using ApplyHighContrastCheckTest = RenderingTest;

TEST_F(ApplyHighContrastCheckTest, LightSolidBackgroundAlwaysFiltered) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyBackgroundColor,
                                               CSSValueWhite);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(ShouldApplyHighContrastFilterToPage(
      HighContrastPagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyHighContrastFilterToPage(
      HighContrastPagePolicy::kFilterAll, GetLayoutView()));
}

TEST_F(ApplyHighContrastCheckTest,
       DarkSolidBackgroundFilteredIfPolicyIsFilterAll) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyBackgroundColor,
                                               CSSValueBlack);
  // TODO(https://crbug.com/925949): Set opacity the same way as the other CSS
  // properties.
  GetLayoutView().MutableStyle()->SetOpacity(0.9);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(ShouldApplyHighContrastFilterToPage(
      HighContrastPagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyHighContrastFilterToPage(
      HighContrastPagePolicy::kFilterAll, GetLayoutView()));
}

TEST_F(ApplyHighContrastCheckTest, DarkLowOpacityBackgroundAlwaysFiltered) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyBackgroundColor,
                                               CSSValueBlack);
  // TODO(https://crbug.com/925949): Set opacity the same way as the other CSS
  // properties.
  GetLayoutView().MutableStyle()->SetOpacity(0.1);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(ShouldApplyHighContrastFilterToPage(
      HighContrastPagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyHighContrastFilterToPage(
      HighContrastPagePolicy::kFilterAll, GetLayoutView()));
}

TEST_F(ApplyHighContrastCheckTest, DarkTransparentBackgroundAlwaysFiltered) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyBackgroundColor,
                                               CSSValueTransparent);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(ShouldApplyHighContrastFilterToPage(
      HighContrastPagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyHighContrastFilterToPage(
      HighContrastPagePolicy::kFilterAll, GetLayoutView()));
}

TEST_F(ApplyHighContrastCheckTest, BackgroundColorNotDefinedAlwaysFiltered) {
  GetDocument().body()->RemoveInlineStyleProperty(CSSPropertyBackgroundColor);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(ShouldApplyHighContrastFilterToPage(
      HighContrastPagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyHighContrastFilterToPage(
      HighContrastPagePolicy::kFilterAll, GetLayoutView()));
}

}  // namespace
}  // namespace blink
