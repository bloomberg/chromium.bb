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

  void SetAhemBody(const char* message, const unsigned width) {
    SetBodyInnerHTML(String::Format(
        "<div id='target' style='font: Ahem; width: %uem'>%s</div>", width,
        message));
  }

  LayoutText* GetLayoutTextById(const char* id) {
    return ToLayoutText(GetLayoutObjectByElementId(id)->SlowFirstChild());
  }

  LayoutText* GetBasicText() { return GetLayoutTextById("target"); }
};

const char kTacoText[] = "Los Compadres Taco Truck";

// Helper class to run the same test code with and without LayoutNG
class ParameterizedLayoutTextTest : public ::testing::WithParamInterface<bool>,
                                    private ScopedLayoutNGForTest,
                                    private ScopedLayoutNGPaintFragmentsForTest,
                                    public LayoutTextTest {
 public:
  ParameterizedLayoutTextTest()
      : ScopedLayoutNGForTest(GetParam()),
        ScopedLayoutNGPaintFragmentsForTest(GetParam()) {}

 protected:
  bool LayoutNGEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_CASE_P(All, ParameterizedLayoutTextTest, ::testing::Bool());

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

TEST_P(ParameterizedLayoutTextTest, CaretMinMaxOffset) {
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

TEST_P(ParameterizedLayoutTextTest, ResolvedTextLength) {
  SetBasicBody("foo");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody("  foo");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody("foo  ");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody(" foo  ");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());
}

TEST_P(ParameterizedLayoutTextTest, ContainsCaretOffset) {
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

TEST_P(ParameterizedLayoutTextTest, ContainsCaretOffsetInPre) {
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

TEST_P(ParameterizedLayoutTextTest,
       IsBeforeAfterNonCollapsedCharacterNoLineWrap) {
  // Basic tests
  SetBasicBody("foo");
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(0));  // "|foo"
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(3));   // "foo|"

  // Return false at node end/start, respectively
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(3));  // "foo|"
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(0));   // "|foo"

  // Consecutive spaces are collapsed into one
  SetBasicBody("f   bar");
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(1));   // "f|   bar"
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(2));  // "f |  bar"
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(3));  // "f  | bar"
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(2));    // "f |  bar"
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(3));   // "f  | bar"
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(4));   // "f   |bar"

  // Leading spaces in a block are collapsed
  SetBasicBody("  foo");
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(0));  // "|  foo"
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(1));  // " | foo"
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(1));   // " | foo"
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(2));   // "  |foo"

  // Trailing spaces in a block are collapsed
  SetBasicBody("foo  ");
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(3));  // "foo|  "
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(4));  // "foo | "
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(4));   // "foo | "
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(5));   // "foo  |"

  // Non-collapsed space at node end
  SetBasicBody("foo <span>bar</span>");
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(
      3));  // "foo| <span>bar</span>"
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(
      4));  // "foo |<span>bar</span>"

  // Non-collapsed space at node start
  SetBasicBody("foo<span id=bar> bar</span>");
  EXPECT_TRUE(GetLayoutTextById("bar")->IsBeforeNonCollapsedCharacter(
      0));  // "foo<span>| bar</span>"
  EXPECT_TRUE(GetLayoutTextById("bar")->IsAfterNonCollapsedCharacter(
      1));  // "foo<span> |bar</span>"

  // Consecutive spaces across nodes
  SetBasicBody("foo <span id=bar> bar</span>");
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(
      3));  // "foo| <span> bar</span>"
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(
      4));  // "foo |<span> bar</span>"
  EXPECT_FALSE(GetLayoutTextById("bar")->IsBeforeNonCollapsedCharacter(
      0));  // foo <span>| bar</span>
  EXPECT_FALSE(GetLayoutTextById("bar")->IsAfterNonCollapsedCharacter(
      1));  // foo <span> |bar</span>

  // Non-collapsed whitespace text node
  SetBasicBody("foo<span id=space> </span>bar");
  EXPECT_TRUE(GetLayoutTextById("space")->IsBeforeNonCollapsedCharacter(0));
  EXPECT_TRUE(GetLayoutTextById("space")->IsAfterNonCollapsedCharacter(1));

  // Collapsed whitespace text node
  SetBasicBody("foo <span id=space> </span>bar");
  EXPECT_FALSE(GetLayoutTextById("space")->IsBeforeNonCollapsedCharacter(0));
  EXPECT_FALSE(GetLayoutTextById("space")->IsAfterNonCollapsedCharacter(1));
}

TEST_P(ParameterizedLayoutTextTest, IsBeforeAfterNonCollapsedLineWrapSpace) {
  LoadAhem();

  // Line wrapping inside node
  SetAhemBody("xx xx", 2);
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(2));  // "xx| xx"
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(3));   // "xx |xx"

  // Legacy layout fails in the remaining test cases
  if (!LayoutNGEnabled())
    return;

  // Line wrapping at node start
  SetAhemBody("xx<span id=span> xx</span>", 2);
  EXPECT_TRUE(GetLayoutTextById("span")->IsBeforeNonCollapsedCharacter(
      0));  // "xx<span>| xx</span>"
  EXPECT_TRUE(GetLayoutTextById("span")->IsAfterNonCollapsedCharacter(
      1));  // "xx<span>| xx</span>"

  // Line wrapping at node end
  SetAhemBody("xx <span>xx</span>", 2);
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(
      2));  // "xx| <span>xx</span>"
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(
      3));  // "xx |<span>xx</span>"

  // Entire node as line wrapping
  SetAhemBody("xx<span id=space> </span>xx", 2);
  EXPECT_TRUE(GetLayoutTextById("space")->IsBeforeNonCollapsedCharacter(0));
  EXPECT_TRUE(GetLayoutTextById("space")->IsAfterNonCollapsedCharacter(1));
}

TEST_P(ParameterizedLayoutTextTest, IsBeforeAfterNonCollapsedCharacterBR) {
  SetBasicBody("<br>");
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(0));
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(1));
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(0));
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(1));
}

}  // namespace blink
