// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/InlineBoxPosition.h"

#include "core/editing/Position.h"
#include "core/editing/testing/EditingTestBase.h"
#include "core/layout/line/InlineTextBox.h"

namespace blink {

std::ostream& operator<<(std::ostream& ostream,
                         const InlineBoxPosition& inline_box_position) {
  if (!inline_box_position.inline_box)
    return ostream << "null";
  return ostream
         << inline_box_position.inline_box->GetLineLayoutItem().GetNode() << "@"
         << inline_box_position.offset_in_box;
}

class InlineBoxPositionTest : public EditingTestBase {};

TEST_F(InlineBoxPositionTest, ComputeInlineBoxPositionBidiIsolate) {
  // "|" is bidi-level 0, and "foo" and "bar" are bidi-level 2
  SetBodyContent(
      "|<span id=sample style='unicode-bidi: isolate;'>foo<br>bar</span>|");

  Element* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  const InlineBoxPosition& actual =
      ComputeInlineBoxPosition(Position(text, 0), TextAffinity::kDownstream);
  EXPECT_EQ(ToLayoutText(text->GetLayoutObject())->FirstTextBox(),
            actual.inline_box);
}

// http://crbug.com/716093
TEST_F(InlineBoxPositionTest, ComputeInlineBoxPositionMixedEditable) {
  SetBodyContent(
      "<div contenteditable id=sample>abc<input contenteditable=false></div>");
  Element* const sample = GetDocument().getElementById("sample");

  const InlineBoxPosition& actual = ComputeInlineBoxPosition(
      Position::LastPositionInNode(*sample), TextAffinity::kDownstream);
  // Should not be in infinite-loop
  EXPECT_EQ(nullptr, actual.inline_box);
  EXPECT_EQ(0, actual.offset_in_box);
}

}  // namespace blink
