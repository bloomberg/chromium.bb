// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/apply_dark_mode.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

using ApplyDarkModeCheckTest = RenderingTest;

TEST_F(ApplyDarkModeCheckTest, LightSolidBackgroundAlwaysFiltered) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor,
                                               CSSValueID::kWhite);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(
      DarkModePagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(DarkModePagePolicy::kFilterAll,
                                              GetLayoutView()));
}

TEST_F(ApplyDarkModeCheckTest, DarkSolidBackgroundFilteredIfPolicyIsFilterAll) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor,
                                               CSSValueID::kBlack);
  // TODO(https://crbug.com/925949): Set opacity the same way as the other CSS
  // properties.
  GetLayoutView().MutableStyle()->SetOpacity(0.9);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(ShouldApplyDarkModeFilterToPage(
      DarkModePagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(DarkModePagePolicy::kFilterAll,
                                              GetLayoutView()));
}

TEST_F(ApplyDarkModeCheckTest, DarkLowOpacityBackgroundAlwaysFiltered) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor,
                                               CSSValueID::kBlack);
  // TODO(https://crbug.com/925949): Set opacity the same way as the other CSS
  // properties.
  GetLayoutView().MutableStyle()->SetOpacity(0.1);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(
      DarkModePagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(DarkModePagePolicy::kFilterAll,
                                              GetLayoutView()));
}

TEST_F(ApplyDarkModeCheckTest, DarkTransparentBackgroundAlwaysFiltered) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor,
                                               CSSValueID::kTransparent);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(
      DarkModePagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(DarkModePagePolicy::kFilterAll,
                                              GetLayoutView()));
}

TEST_F(ApplyDarkModeCheckTest, BackgroundColorNotDefinedAlwaysFiltered) {
  GetDocument().body()->RemoveInlineStyleProperty(
      CSSPropertyID::kBackgroundColor);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(
      DarkModePagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(DarkModePagePolicy::kFilterAll,
                                              GetLayoutView()));
}

TEST_F(ApplyDarkModeCheckTest, MetaColorSchemeDark) {
  RuntimeEnabledFeatures::SetCSSColorSchemeEnabled(true);
  RuntimeEnabledFeatures::SetMetaColorSchemeEnabled(true);
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
  GetDocument().GetSettings()->SetPreferredColorScheme(
      PreferredColorScheme::kDark);
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="color-scheme" content="dark">
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  // Opting out of forced darkening when dark is among the supported color
  // schemes for the page.
  EXPECT_FALSE(ShouldApplyDarkModeFilterToPage(
      DarkModePagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_FALSE(ShouldApplyDarkModeFilterToPage(DarkModePagePolicy::kFilterAll,
                                               GetLayoutView()));
}

}  // namespace
}  // namespace blink
