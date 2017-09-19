// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/SelectionTemplate.h"

#include "core/editing/EphemeralRange.h"
#include "core/editing/testing/EditingTestBase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class SelectionTest : public EditingTestBase {};

TEST_F(SelectionTest, defaultConstructor) {
  SelectionInDOMTree selection;

  EXPECT_EQ(TextAffinity::kDownstream, selection.Affinity());
  EXPECT_TRUE(selection.IsBaseFirst());
  EXPECT_FALSE(selection.IsDirectional());
  EXPECT_TRUE(selection.IsNone());
  EXPECT_EQ(Position(), selection.Base());
  EXPECT_EQ(Position(), selection.Extent());
}

TEST_F(SelectionTest, IsBaseFirst) {
  SetBodyContent("<div id='sample'>abcdef</div>");

  Element* sample = GetDocument().getElementById("sample");
  Position base(Position(sample->firstChild(), 4));
  Position extent(Position(sample->firstChild(), 2));
  SelectionInDOMTree::Builder builder;
  builder.Collapse(base);
  builder.Extend(extent);
  const SelectionInDOMTree& selection = builder.Build();

  EXPECT_EQ(TextAffinity::kDownstream, selection.Affinity());
  EXPECT_FALSE(selection.IsBaseFirst());
  EXPECT_FALSE(selection.IsDirectional());
  EXPECT_FALSE(selection.IsNone());
  EXPECT_EQ(base, selection.Base());
  EXPECT_EQ(extent, selection.Extent());
}

TEST_F(SelectionTest, caret) {
  SetBodyContent("<div id='sample'>abcdef</div>");

  Element* sample = GetDocument().getElementById("sample");
  Position position(Position(sample->firstChild(), 2));
  SelectionInDOMTree::Builder builder;
  builder.Collapse(position);
  const SelectionInDOMTree& selection = builder.Build();

  EXPECT_EQ(TextAffinity::kDownstream, selection.Affinity());
  EXPECT_TRUE(selection.IsBaseFirst());
  EXPECT_FALSE(selection.IsDirectional());
  EXPECT_FALSE(selection.IsNone());
  EXPECT_EQ(position, selection.Base());
  EXPECT_EQ(position, selection.Extent());
}

TEST_F(SelectionTest, range) {
  SetBodyContent("<div id='sample'>abcdef</div>");

  Element* sample = GetDocument().getElementById("sample");
  Position base(Position(sample->firstChild(), 2));
  Position extent(Position(sample->firstChild(), 4));
  SelectionInDOMTree::Builder builder;
  builder.Collapse(base);
  builder.Extend(extent);
  const SelectionInDOMTree& selection = builder.Build();

  EXPECT_EQ(TextAffinity::kDownstream, selection.Affinity());
  EXPECT_TRUE(selection.IsBaseFirst());
  EXPECT_FALSE(selection.IsDirectional());
  EXPECT_FALSE(selection.IsNone());
  EXPECT_EQ(base, selection.Base());
  EXPECT_EQ(extent, selection.Extent());
}

TEST_F(SelectionTest, SetAsBacwardAndForward) {
  SetBodyContent("<div id='sample'>abcdef</div>");

  Element* sample = GetDocument().getElementById("sample");
  Position start(Position(sample->firstChild(), 2));
  Position end(Position(sample->firstChild(), 4));
  EphemeralRange range(start, end);
  const SelectionInDOMTree& backward_selection =
      SelectionInDOMTree::Builder().SetAsBackwardSelection(range).Build();
  const SelectionInDOMTree& forward_selection =
      SelectionInDOMTree::Builder().SetAsForwardSelection(range).Build();
  const SelectionInDOMTree& collapsed_selection =
      SelectionInDOMTree::Builder()
          .SetAsForwardSelection(EphemeralRange(start))
          .Build();

  EXPECT_EQ(TextAffinity::kDownstream, backward_selection.Affinity());
  EXPECT_FALSE(backward_selection.IsBaseFirst());
  EXPECT_FALSE(backward_selection.IsDirectional());
  EXPECT_FALSE(backward_selection.IsNone());
  EXPECT_EQ(end, backward_selection.Base());
  EXPECT_EQ(start, backward_selection.Extent());

  EXPECT_EQ(TextAffinity::kDownstream, forward_selection.Affinity());
  EXPECT_TRUE(forward_selection.IsBaseFirst());
  EXPECT_FALSE(forward_selection.IsDirectional());
  EXPECT_FALSE(forward_selection.IsNone());
  EXPECT_EQ(start, forward_selection.Base());
  EXPECT_EQ(end, forward_selection.Extent());

  EXPECT_EQ(TextAffinity::kDownstream, collapsed_selection.Affinity());
  EXPECT_TRUE(collapsed_selection.IsBaseFirst());
  EXPECT_FALSE(collapsed_selection.IsDirectional());
  EXPECT_FALSE(collapsed_selection.IsNone());
  EXPECT_EQ(start, collapsed_selection.Base());
  EXPECT_EQ(start, collapsed_selection.Extent());
}

}  // namespace blink
