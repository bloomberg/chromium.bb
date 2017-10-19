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

  const LayoutTextFragment* GetRemainingText() const {
    return ToLayoutTextFragment(
        GetElementById("target")->firstChild()->GetLayoutObject());
  }

  const LayoutTextFragment* GetFirstLetter() const {
    return ToLayoutTextFragment(
        AssociatedLayoutObjectOf(*GetElementById("target")->firstChild(), 0));
  }
};

TEST_F(LayoutTextFragmentTest, LegacyBasics) {
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

TEST_F(LayoutTextFragmentTest, CaretMinMaxOffsetNG) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

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

TEST_F(LayoutTextFragmentTest, CaretMinMaxOffsetSpacesInBetweenNG) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

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

TEST_F(LayoutTextFragmentTest, CaretMinMaxOffsetCollapsedRemainingTextNG) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

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

TEST_F(LayoutTextFragmentTest, ResolvedTextLengthNG) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

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

TEST_F(LayoutTextFragmentTest, ResolvedTextLengthSpacesInBetweenNG) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

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

TEST_F(LayoutTextFragmentTest, ResolvedTextLengthCollapsedRemainingTextNG) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

  SetBasicBody("(f)  ");
  EXPECT_EQ(3u, GetFirstLetter()->ResolvedTextLength());
  EXPECT_EQ(0u, GetRemainingText()->ResolvedTextLength());

  SetBasicBody("  (f)  ");
  EXPECT_EQ(3u, GetFirstLetter()->ResolvedTextLength());
  EXPECT_EQ(0u, GetRemainingText()->ResolvedTextLength());
}

TEST_F(LayoutTextFragmentTest, ContainsCaretOffsetNG) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

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

TEST_F(LayoutTextFragmentTest, ContainsCaretOffsetSpacesInBetweenNG) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

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

TEST_F(LayoutTextFragmentTest, ContainsCaretOffsetPreNG) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

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

TEST_F(LayoutTextFragmentTest, ContainsCaretOffsetPreLineNG) {
  ScopedLayoutNGForTest layout_ng(true);
  ScopedLayoutNGPaintFragmentsForTest layout_ng_paint_fragments(true);

  SetBodyInnerHTML("<div id='target' style='white-space: pre-line'>F \n \noo");
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(0));     // "|F \n \noo"
  EXPECT_TRUE(GetFirstLetter()->ContainsCaretOffset(1));     // "F| \n \noo"
  EXPECT_FALSE(GetRemainingText()->ContainsCaretOffset(0));  // "F| \n \noo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(1));   // "F |\n \noo"
  EXPECT_FALSE(GetRemainingText()->ContainsCaretOffset(2));  // "F \n| \noo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(3));   // "F \n |\noo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(4));   // "F \n \n|oo"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(5));   // "F \n \no|o"
  EXPECT_TRUE(GetRemainingText()->ContainsCaretOffset(6));   // "F \n \noo|"
}

}  // namespace blink
