// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/SelectionTemplate.h"

#include "core/editing/EditingTestBase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class SelectionTest : public EditingTestBase {};

TEST_F(SelectionTest, defaultConstructor) {
  SelectionInDOMTree selection;

  EXPECT_EQ(TextAffinity::kDownstream, selection.Affinity());
  EXPECT_FALSE(selection.IsDirectional());
  EXPECT_TRUE(selection.IsNone());
  EXPECT_EQ(Position(), selection.Base());
  EXPECT_EQ(Position(), selection.Extent());
}

TEST_F(SelectionTest, caret) {
  SetBodyContent("<div id='sample'>abcdef</div>");

  Element* sample = GetDocument().getElementById("sample");
  Position position(Position(sample->firstChild(), 2));
  SelectionInDOMTree::Builder builder;
  builder.Collapse(position);
  const SelectionInDOMTree& selection = builder.Build();

  EXPECT_EQ(TextAffinity::kDownstream, selection.Affinity());
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
  EXPECT_FALSE(selection.IsDirectional());
  EXPECT_FALSE(selection.IsNone());
  EXPECT_EQ(base, selection.Base());
  EXPECT_EQ(extent, selection.Extent());
}

}  // namespace blink
