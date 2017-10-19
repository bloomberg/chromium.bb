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

TEST_F(LayoutTextTest, ContainsCaretOffset) {
  // This test records the behavior introduced in crrev.com/e3eb4e
  SetBasicBody(" foo   bar ");
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(0));   // "| foo   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(1));    // " |foo   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(2));    // " f|oo   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(3));    // " fo|o   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(4));    // " foo|   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(5));    // " foo |  bar "
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(6));   // " foo  | bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(7));    // " foo   |bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(8));    // " foo   b|ar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(9));    // " foo   ba|r "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(10));   // " foo   bar| "
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(11));  // " foo   bar |"
}

TEST_F(LayoutTextTest, ContainsCaretOffsetInPre) {
  // These tests record the behavior introduced in crrev.com/e3eb4e

  SetBodyInnerHTML("<pre id='target'>foo   bar</pre>");
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(0));  // "|foo   bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(1));  // "f|oo   bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(2));  // "fo|o   bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(3));  // "foo|   bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(4));  // "foo |  bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(5));  // "foo  | bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(6));  // "foo   |bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(7));  // "foo   b|ar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(8));  // "foo   ba|r"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(9));  // "foo   bar|"

  SetBodyInnerHTML("<pre id='target'>foo\n</div>");
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(0));   // "|foo\n"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(1));   // "f|oo\n"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(2));   // "fo|o\n"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(3));   // "foo|\n"
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(4));  // "foo\n|"

  SetBodyInnerHTML("<pre id='target'>foo\nbar</pre>");
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(0));  // "|foo\nbar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(1));  // "f|oo\nbar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(2));  // "fo|o\nbar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(3));  // "foo|\nbar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(4));  // "foo\n|bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(5));  // "foo\nb|ar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(6));  // "foo\nba|r"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(7));  // "foo\nbar|"
}

TEST_F(LayoutTextTest, ContainsCaretOffsetNG) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

  // This test records the behavior introduced in crrev.com/e3eb4e
  SetBasicBody(" foo   bar ");
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(0));   // "| foo   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(1));    // " |foo   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(2));    // " f|oo   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(3));    // " fo|o   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(4));    // " foo|   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(5));    // " foo |  bar "
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(6));   // " foo  | bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(7));    // " foo   |bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(8));    // " foo   b|ar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(9));    // " foo   ba|r "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(10));   // " foo   bar| "
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(11));  // " foo   bar |"
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(12));  // out of range
}

TEST_F(LayoutTextTest, ContainsCaretOffsetNGInPre) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

  // These tests record the behavior introduced in crrev.com/e3eb4e

  SetBodyInnerHTML("<pre id='target'>foo   bar</pre>");
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(0));  // "|foo   bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(1));  // "f|oo   bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(2));  // "fo|o   bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(3));  // "foo|   bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(4));  // "foo |  bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(5));  // "foo  | bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(6));  // "foo   |bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(7));  // "foo   b|ar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(8));  // "foo   ba|r"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(9));  // "foo   bar|"

  SetBodyInnerHTML("<pre id='target'>foo\n</pre>");
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(0));   // "|foo\n"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(1));   // "f|oo\n"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(2));   // "fo|o\n"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(3));   // "foo|\n"
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(4));  // "foo\n|"

  SetBodyInnerHTML("<pre id='target'>foo\nbar</pre>");
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(0));  // "|foo\nbar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(1));  // "f|oo\nbar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(2));  // "fo|o\nbar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(3));  // "foo|\nbar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(4));  // "foo\n|bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(5));  // "foo\nb|ar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(6));  // "foo\nba|r"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(7));  // "foo\nbar|"
}

}  // namespace blink
