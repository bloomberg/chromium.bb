// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutText.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/line/InlineTextBox.h"
#include "platform/runtime_enabled_features.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class LayoutTextTest : public RenderingTest {
 public:
  void SetBasicBody(const char* message) {
    SetBodyInnerHTML(String::Format(
        "<div id='target' style='font-size: 10px;'>%s</div>", message));
  }

  LayoutText* GetBasicText() {
    return ToLayoutText(GetLayoutObjectByElementId("target")->SlowFirstChild());
  }
};

const char kTacoText[] = "Los Compadres Taco Truck";

}  // namespace

TEST_F(LayoutTextTest, WidthZeroFromZeroLength) {
  SetBasicBody(kTacoText);
  ASSERT_EQ(0, GetBasicText()->Width(0u, 0u, LayoutUnit(), TextDirection::kLtr,
                                     false));
}

TEST_F(LayoutTextTest, WidthMaxFromZeroLength) {
  SetBasicBody(kTacoText);
  ASSERT_EQ(0, GetBasicText()->Width(std::numeric_limits<unsigned>::max(), 0u,
                                     LayoutUnit(), TextDirection::kLtr, false));
}

TEST_F(LayoutTextTest, WidthZeroFromMaxLength) {
  SetBasicBody(kTacoText);
  float width = GetBasicText()->Width(0u, std::numeric_limits<unsigned>::max(),
                                      LayoutUnit(), TextDirection::kLtr, false);
  // Width may vary by platform and we just want to make sure it's something
  // roughly reasonable.
  ASSERT_GE(width, 100.f);
  ASSERT_LE(width, 160.f);
}

TEST_F(LayoutTextTest, WidthMaxFromMaxLength) {
  SetBasicBody(kTacoText);
  ASSERT_EQ(0, GetBasicText()->Width(std::numeric_limits<unsigned>::max(),
                                     std::numeric_limits<unsigned>::max(),
                                     LayoutUnit(), TextDirection::kLtr, false));
}

TEST_F(LayoutTextTest, WidthWithHugeLengthAvoidsOverflow) {
  // The test case from http://crbug.com/647820 uses a 288-length string, so for
  // posterity we follow that closely.
  SetBodyInnerHTML(
      "<div "
      "id='target'>"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      "xxxx"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      "</div>");
  // Width may vary by platform and we just want to make sure it's something
  // roughly reasonable.
  const float width = GetBasicText()->Width(
      23u, 4294967282u, LayoutUnit(2.59375), TextDirection::kRtl, false);
  ASSERT_GE(width, 100.f);
  ASSERT_LE(width, 300.f);
}

TEST_F(LayoutTextTest, WidthFromBeyondLength) {
  SetBasicBody("x");
  ASSERT_EQ(0u, GetBasicText()->Width(1u, 1u, LayoutUnit(), TextDirection::kLtr,
                                      false));
}

TEST_F(LayoutTextTest, WidthLengthBeyondLength) {
  SetBasicBody("x");
  // Width may vary by platform and we just want to make sure it's something
  // roughly reasonable.
  const float width =
      GetBasicText()->Width(0u, 2u, LayoutUnit(), TextDirection::kLtr, false);
  ASSERT_GE(width, 4.f);
  ASSERT_LE(width, 20.f);
}

TEST_F(LayoutTextTest, CaretMinMaxOffsetNG) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

  SetBasicBody("foo");
  EXPECT_EQ(0, GetBasicText()->CaretMinOffset());
  EXPECT_EQ(3, GetBasicText()->CaretMaxOffset());

  SetBasicBody("  foo");
  EXPECT_EQ(2, GetBasicText()->CaretMinOffset());
  EXPECT_EQ(5, GetBasicText()->CaretMaxOffset());

  SetBasicBody("foo  ");
  EXPECT_EQ(0, GetBasicText()->CaretMinOffset());
  EXPECT_EQ(3, GetBasicText()->CaretMaxOffset());

  SetBasicBody(" foo  ");
  EXPECT_EQ(1, GetBasicText()->CaretMinOffset());
  EXPECT_EQ(4, GetBasicText()->CaretMaxOffset());
}

TEST_F(LayoutTextTest, ResolvedTextLengthNG) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

  SetBasicBody("foo");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody("  foo");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody("foo  ");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody(" foo  ");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());
}

}  // namespace blink
