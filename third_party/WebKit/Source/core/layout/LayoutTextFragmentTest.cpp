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

  EXPECT_EQ(0, GetRemainingText()->CaretMinOffset());
  EXPECT_EQ(2, GetRemainingText()->CaretMaxOffset());
  EXPECT_EQ(2u, GetRemainingText()->ResolvedTextLength());
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

}  // namespace blink
