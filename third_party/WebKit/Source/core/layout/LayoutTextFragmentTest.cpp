// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTextFragment.h"

#include "core/html/HTMLHeadElement.h"
#include "core/layout/LayoutTestHelper.h"
#include "platform/runtime_enabled_features.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"

namespace blink {

class LayoutTextFragmentTest : public RenderingTest {
 protected:
  void SetUp() override {
    RenderingTest::SetUp();
    GetDocument().head()->SetInnerHTMLFromString(
        "<style>#target::first-letter{color:red}</style>");
  }

  void SetBasicBody(const char* message) {
    SetBodyInnerHTML(String::Format(
        "<div id='target' style='font-size: 10px;'>%s</div>", message));
  }

  void SetAhemBody(const char* message, const unsigned width) {
    SetBodyInnerHTML(String::Format(
        "<div id='target' style='font: Ahem; width: %uem'>%s</div>", width,
        message));
  }

  const LayoutTextFragment* GetRemainingText() const {
    return ToLayoutTextFragment(
        GetElementById("target")->firstChild()->GetLayoutObject());
  }

  const LayoutTextFragment* GetFirstLetter() const {
    return ToLayoutTextFragment(
        AssociatedLayoutObjectOf(*GetElementById("target")->firstChild(), 0));
  }
};

// Helper class to run the same test code with and without LayoutNG
class ParameterizedLayoutTextFragmentTest
    : public ::testing::WithParamInterface<bool>,
      private ScopedLayoutNGForTest,
      private ScopedLayoutNGPaintFragmentsForTest,
      public LayoutTextFragmentTest {
 public:
  ParameterizedLayoutTextFragmentTest()
      : ScopedLayoutNGForTest(GetParam()),
        ScopedLayoutNGPaintFragmentsForTest(GetParam()) {}

 protected:
  bool LayoutNGEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_CASE_P(All,
                        ParameterizedLayoutTextFragmentTest,
                        ::testing::Bool());

TEST_P(ParameterizedLayoutTextFragmentTest, Basics) {
  SetBasicBody("foo");

  EXPECT_EQ(0, GetFirstLetter()->CaretMinOffset());
  EXPECT_EQ(1, GetFirstLetter()->CaretMaxOffset());
  EXPECT_EQ(1u, GetFirstLetter()->ResolvedTextLength());
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(0));

  EXPECT_EQ(0, GetRemainingText()->CaretMinOffset());
  EXPECT_EQ(2, GetRemainingText()->CaretMaxOffset());
  EXPECT_EQ(2u, GetRemainingText()->ResolvedTextLength());
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(0));
}

TEST_P(ParameterizedLayoutTextFragmentTest, CaretMinMaxOffset) {
  SetBasicBody("(f)oo");
  EXPECT_EQ(0, GetFirstLetter()->CaretMinOffset());
  EXPECT_EQ(3, GetFirstLetter()->CaretMaxOffset());
  EXPECT_EQ(0, GetRemainingText()->CaretMinOffset());
  EXPECT_EQ(2, GetRemainingText()->CaretMaxOffset());

  SetBasicBody("  (f)oo");
  EXPECT_EQ(2, GetFirstLetter()->CaretMinOffset());
  EXPECT_EQ(5, GetFirstLetter()->CaretMaxOffset());
  EXPECT_EQ(0, GetRemainingText()->CaretMinOffset());
  EXPECT_EQ(2, GetRemainingText()->CaretMaxOffset());

  SetBasicBody("(f)oo  ");
  EXPECT_EQ(0, GetFirstLetter()->CaretMinOffset());
  EXPECT_EQ(3, GetFirstLetter()->CaretMaxOffset());
  EXPECT_EQ(0, GetRemainingText()->CaretMinOffset());
  EXPECT_EQ(2, GetRemainingText()->CaretMaxOffset());

  SetBasicBody(" (f)oo  ");
  EXPECT_EQ(1, GetFirstLetter()->CaretMinOffset());
  EXPECT_EQ(4, GetFirstLetter()->CaretMaxOffset());
  EXPECT_EQ(0, GetRemainingText()->CaretMinOffset());
  EXPECT_EQ(2, GetRemainingText()->CaretMaxOffset());
}

TEST_P(ParameterizedLayoutTextFragmentTest, CaretMinMaxOffsetSpacesInBetween) {
  SetBasicBody("(f)  oo");
  EXPECT_EQ(0, GetFirstLetter()->CaretMinOffset());
  EXPECT_EQ(3, GetFirstLetter()->CaretMaxOffset());
  EXPECT_EQ(0, GetRemainingText()->CaretMinOffset());
  EXPECT_EQ(4, GetRemainingText()->CaretMaxOffset());

  SetBasicBody("  (f)  oo");
  EXPECT_EQ(2, GetFirstLetter()->CaretMinOffset());
  EXPECT_EQ(5, GetFirstLetter()->CaretMaxOffset());
  EXPECT_EQ(0, GetRemainingText()->CaretMinOffset());
  EXPECT_EQ(4, GetRemainingText()->CaretMaxOffset());

  SetBasicBody("(f)  oo  ");
  EXPECT_EQ(0, GetFirstLetter()->CaretMinOffset());
  EXPECT_EQ(3, GetFirstLetter()->CaretMaxOffset());
  EXPECT_EQ(0, GetRemainingText()->CaretMinOffset());
  EXPECT_EQ(4, GetRemainingText()->CaretMaxOffset());

  SetBasicBody(" (f)  oo  ");
  EXPECT_EQ(1, GetFirstLetter()->CaretMinOffset());
  EXPECT_EQ(4, GetFirstLetter()->CaretMaxOffset());
  EXPECT_EQ(0, GetRemainingText()->CaretMinOffset());
  EXPECT_EQ(4, GetRemainingText()->CaretMaxOffset());
}

TEST_P(ParameterizedLayoutTextFragmentTest,
       CaretMinMaxOffsetCollapsedRemainingText) {
  // Tests if the NG implementation matches the legacy behavior that, when the
  // remaining text is fully collapsed, its CaretMin/MaxOffset() return 0 and
  // FragmentLength().

  SetBasicBody("(f)  ");
  EXPECT_EQ(0, GetFirstLetter()->CaretMinOffset());
  EXPECT_EQ(3, GetFirstLetter()->CaretMaxOffset());
  EXPECT_EQ(0, GetRemainingText()->CaretMinOffset());
  EXPECT_EQ(2, GetRemainingText()->CaretMaxOffset());

  SetBasicBody("  (f)  ");
  EXPECT_EQ(2, GetFirstLetter()->CaretMinOffset());
  EXPECT_EQ(5, GetFirstLetter()->CaretMaxOffset());
  EXPECT_EQ(0, GetRemainingText()->CaretMinOffset());
  EXPECT_EQ(2, GetRemainingText()->CaretMaxOffset());
}

TEST_P(ParameterizedLayoutTextFragmentTest, ResolvedTextLength) {
  SetBasicBody("(f)oo");
  EXPECT_EQ(3u, GetFirstLetter()->ResolvedTextLength());
  EXPECT_EQ(2u, GetRemainingText()->ResolvedTextLength());

  SetBasicBody("  (f)oo");
  EXPECT_EQ(3u, GetFirstLetter()->ResolvedTextLength());
  EXPECT_EQ(2u, GetRemainingText()->ResolvedTextLength());

  SetBasicBody("(f)oo  ");
  EXPECT_EQ(3u, GetFirstLetter()->ResolvedTextLength());
  EXPECT_EQ(2u, GetRemainingText()->ResolvedTextLength());

  SetBasicBody(" (f)oo  ");
  EXPECT_EQ(3u, GetFirstLetter()->ResolvedTextLength());
  EXPECT_EQ(2u, GetRemainingText()->ResolvedTextLength());
}

TEST_P(ParameterizedLayoutTextFragmentTest, ResolvedTextLengthSpacesInBetween) {
  SetBasicBody("(f)  oo");
  EXPECT_EQ(3u, GetFirstLetter()->ResolvedTextLength());
  EXPECT_EQ(3u, GetRemainingText()->ResolvedTextLength());

  SetBasicBody("  (f)  oo");
  EXPECT_EQ(3u, GetFirstLetter()->ResolvedTextLength());
  EXPECT_EQ(3u, GetRemainingText()->ResolvedTextLength());

  SetBasicBody("(f)  oo  ");
  EXPECT_EQ(3u, GetFirstLetter()->ResolvedTextLength());
  EXPECT_EQ(3u, GetRemainingText()->ResolvedTextLength());

  SetBasicBody(" (f)  oo  ");
  EXPECT_EQ(3u, GetFirstLetter()->ResolvedTextLength());
  EXPECT_EQ(3u, GetRemainingText()->ResolvedTextLength());
}

TEST_P(ParameterizedLayoutTextFragmentTest,
       ResolvedTextLengthCollapsedRemainingText) {
  SetBasicBody("(f)  ");
  EXPECT_EQ(3u, GetFirstLetter()->ResolvedTextLength());
  EXPECT_EQ(0u, GetRemainingText()->ResolvedTextLength());

  SetBasicBody("  (f)  ");
  EXPECT_EQ(3u, GetFirstLetter()->ResolvedTextLength());
  EXPECT_EQ(0u, GetRemainingText()->ResolvedTextLength());
}

TEST_P(ParameterizedLayoutTextFragmentTest, ContainsCaretOffset) {
  SetBasicBody("(f)oo");
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(0));     // "|(f)oo"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(1));     // "(|f)oo"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(2));     // "(f|)oo"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(3));     // "(f)|oo"
  EXPECT_FALSE(GetFirstLetter()->ContainsCaretOffset(4));    // out of range
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(0));   // "(f)|oo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(1));   // "(f)o|o"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(2));   // "(f)oo|"
  EXPECT_FALSE(GetRemainingText()->ContainsCaretOffset(3));  // out of range

  SetBasicBody("  (f)oo");
  EXPECT_FALSE(GetFirstLetter()->ContainsCaretOffset(0));   // "|  (f)oo"
  EXPECT_FALSE(GetFirstLetter()->ContainsCaretOffset(1));   // " | (f)oo"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(2));    // "  |(f)oo"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(3));    // "  (|f)oo"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(4));    // "  (f|)oo"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(5));    // "  (f)|oo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(0));  // "  (f)|oo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(1));  // "  (f)o|o"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(2));  // "  (f)oo|"

  SetBasicBody("(f)oo  ");
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(0));     // "|(f)oo  "
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(1));     // "(|f)oo  "
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(2));     // "(f|)oo  "
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(3));     // "(f)|oo  "
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(0));   // "(f)|oo  "
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(1));   // "(f)o|o  "
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(2));   // "(f)oo|  "
  EXPECT_FALSE(GetRemainingText()->ContainsCaretOffset(3));  // "(f)oo | "
  EXPECT_FALSE(GetRemainingText()->ContainsCaretOffset(4));  // "(f)oo  |"

  SetBasicBody(" (f)oo  ");
  EXPECT_FALSE(GetFirstLetter()->ContainsCaretOffset(0));    // "| (f)oo  "
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(1));     // " |(f)oo  "
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(2));     // " (|f)oo  "
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(3));     // " (f|)oo  "
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(4));     // " (f)|oo  "
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(0));   // " (f)|oo  "
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(1));   // " (f)o|o  "
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(2));   // " (f)oo|  "
  EXPECT_FALSE(GetRemainingText()->ContainsCaretOffset(3));  // " (f)oo | "
  EXPECT_FALSE(GetRemainingText()->ContainsCaretOffset(4));  // " (f)oo  |"
}

TEST_P(ParameterizedLayoutTextFragmentTest,
       ContainsCaretOffsetSpacesInBetween) {
  SetBasicBody("(f)   oo");
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(0));     // "|(f)   oo"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(1));     // "(|f)   oo"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(2));     // "(f|)   oo"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(3));     // "(f)|   oo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(0));   // "(f)|   oo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(1));   // "(f) |  oo"
  EXPECT_FALSE(GetRemainingText()->ContainsCaretOffset(2));  // "(f)  | oo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(3));   // "(f)   |oo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(4));   // "(f)   o|o"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(5));   // "(f)   oo|"
}

TEST_P(ParameterizedLayoutTextFragmentTest, ContainsCaretOffsetPre) {
  SetBodyInnerHTML("<pre id='target'>(f)   oo\n</pre>");
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(0));     // "|(f)   oo\n"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(1));     // "(|f)   oo\n"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(2));     // "(f|)   oo\n"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(3));     // "(f)|   oo\n"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(0));   // "(f)|   oo\n"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(1));   // "(f) |  oo\n"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(2));   // "(f)  | oo\n"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(3));   // "(f)   |oo\n"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(4));   // "(f)   o|o\n"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(5));   // "(f)   oo|\n"
  EXPECT_FALSE(GetRemainingText()->ContainsCaretOffset(6));  // "(f)   oo\n|"
}

TEST_P(ParameterizedLayoutTextFragmentTest, ContainsCaretOffsetPreLine) {
  SetBodyInnerHTML("<div id='target' style='white-space: pre-line'>F \n \noo");
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(0));     // "|F \n \noo"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(1));     // "F| \n \noo"

  if (LayoutNGEnabled()) {
    // Legacy layout doesn't collapse this space correctly.
    EXPECT_FALSE(GetRemainingText()->ContainsCaretOffset(0));  // "F| \n \noo"
  }

  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(1));   // "F |\n \noo"
  EXPECT_FALSE(GetRemainingText()->ContainsCaretOffset(2));  // "F \n| \noo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(3));   // "F \n |\noo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(4));   // "F \n \n|oo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(5));   // "F \n \no|o"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(6));   // "F \n \noo|"
}

TEST_P(ParameterizedLayoutTextFragmentTest,
       IsBeforeAfterNonCollapsedCharacterNoLineWrap) {
  // Basic tests
  SetBasicBody("foo");
  EXPECT_TRUE(GetFirstLetter()->IsBeforeNonCollapsedCharacter(0));    // "|foo"
  EXPECT_TRUE(GetFirstLetter()->IsAfterNonCollapsedCharacter(1));     // "f|oo"
  EXPECT_TRUE(GetRemainingText()->IsBeforeNonCollapsedCharacter(0));  // "f|oo"
  EXPECT_TRUE(GetRemainingText()->IsAfterNonCollapsedCharacter(2));   // "foo|"

  // Return false at layout object end/start, respectively
  EXPECT_FALSE(GetFirstLetter()->IsAfterNonCollapsedCharacter(0));     // "|foo"
  EXPECT_FALSE(GetFirstLetter()->IsBeforeNonCollapsedCharacter(1));    // "f|oo"
  EXPECT_FALSE(GetRemainingText()->IsAfterNonCollapsedCharacter(0));   // "f|oo"
  EXPECT_FALSE(GetRemainingText()->IsBeforeNonCollapsedCharacter(2));  // "foo|"

  // Consecutive spaces between first letter and remaining text
  SetBasicBody("f   bar");
  EXPECT_TRUE(
      GetRemainingText()->IsBeforeNonCollapsedCharacter(0));  // "f|   bar"
  EXPECT_FALSE(
      GetRemainingText()->IsBeforeNonCollapsedCharacter(1));  // "f |  bar"
  EXPECT_FALSE(
      GetRemainingText()->IsBeforeNonCollapsedCharacter(2));  // "f  | bar"
  EXPECT_TRUE(
      GetRemainingText()->IsAfterNonCollapsedCharacter(1));  // "f |  bar"
  EXPECT_FALSE(
      GetRemainingText()->IsAfterNonCollapsedCharacter(2));  // "f  | bar"
  EXPECT_FALSE(
      GetRemainingText()->IsAfterNonCollapsedCharacter(3));  // "f   |bar"

  // Leading spaces in first letter are collapsed
  SetBasicBody("  foo");
  EXPECT_FALSE(GetFirstLetter()->IsBeforeNonCollapsedCharacter(0));  // "|  foo"
  EXPECT_FALSE(GetFirstLetter()->IsBeforeNonCollapsedCharacter(1));  // " | foo"
  EXPECT_FALSE(GetFirstLetter()->IsAfterNonCollapsedCharacter(1));   // " | foo"
  EXPECT_FALSE(GetFirstLetter()->IsAfterNonCollapsedCharacter(2));   // "  |foo"

  // Trailing spaces in remaining text, when at the end of block, are collapsed
  SetBasicBody("foo  ");
  EXPECT_FALSE(
      GetRemainingText()->IsBeforeNonCollapsedCharacter(2));  // "foo|  "
  EXPECT_FALSE(
      GetRemainingText()->IsBeforeNonCollapsedCharacter(3));  // "foo | "
  EXPECT_FALSE(
      GetRemainingText()->IsAfterNonCollapsedCharacter(3));  // "foo | "
  EXPECT_FALSE(GetRemainingText()->IsAfterNonCollapsedCharacter(4));  // "foo |"

  // Non-collapsed space at remaining text end
  SetBasicBody("foo <span>bar</span>");
  EXPECT_TRUE(GetRemainingText()->IsBeforeNonCollapsedCharacter(
      2));  // "foo| <span>bar</span>"
  EXPECT_TRUE(GetRemainingText()->IsAfterNonCollapsedCharacter(
      3));  // "foo |<span>bar</span>"

  // Non-collapsed space as remaining text
  SetBasicBody("f <span>bar</span>");
  EXPECT_TRUE(GetRemainingText()->IsBeforeNonCollapsedCharacter(
      0));  // "f| <span>bar</span>"
  EXPECT_TRUE(GetRemainingText()->IsAfterNonCollapsedCharacter(
      1));  // "f |<span>bar</span>"

  // Legacy layout fails in the remaining test case
  if (!LayoutNGEnabled())
    return;

  // Collapsed space as remaining text
  SetBasicBody("f <br>");
  EXPECT_FALSE(
      GetRemainingText()->IsBeforeNonCollapsedCharacter(0));  // "f| <br>"
  EXPECT_FALSE(
      GetRemainingText()->IsAfterNonCollapsedCharacter(1));  // "f |<br>"
}

TEST_P(ParameterizedLayoutTextFragmentTest,
       IsBeforeAfterNonCollapsedLineWrapSpace) {
  LoadAhem();

  // Line wrapping in the middle of remaining text
  SetAhemBody("xx xx", 2);
  EXPECT_TRUE(
      GetRemainingText()->IsBeforeNonCollapsedCharacter(1));         // "xx| xx"
  EXPECT_TRUE(GetRemainingText()->IsAfterNonCollapsedCharacter(2));  // "xx |xx"

  // Legacy layout fails in the remaining test cases
  if (!LayoutNGEnabled())
    return;

  // Line wrapping at remaining text start
  SetAhemBody("(x xx", 2);
  EXPECT_TRUE(
      GetRemainingText()->IsBeforeNonCollapsedCharacter(0));         // "(x| xx"
  EXPECT_TRUE(GetRemainingText()->IsAfterNonCollapsedCharacter(1));  // "(x |xx"

  // Line wrapping at remaining text end
  SetAhemBody("xx <span>xx</span>", 2);
  EXPECT_TRUE(GetRemainingText()->IsBeforeNonCollapsedCharacter(
      1));  // "xx| <span>xx</span>"
  EXPECT_TRUE(GetRemainingText()->IsAfterNonCollapsedCharacter(
      2));  // "xx |<span>xx</span>"

  // Entire remaining text as line wrapping
  SetAhemBody("(x <span>xx</span>", 2);
  EXPECT_TRUE(GetRemainingText()->IsBeforeNonCollapsedCharacter(
      0));  // "(x| <span>xx</span>"
  EXPECT_TRUE(GetRemainingText()->IsAfterNonCollapsedCharacter(
      1));  // "(x |<span>xx</span>"
}

}  // namespace blink
