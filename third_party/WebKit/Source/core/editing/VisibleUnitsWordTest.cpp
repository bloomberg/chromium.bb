// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/VisibleUnits.h"

#include "core/editing/SelectionTemplate.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/testing/EditingTestBase.h"

namespace blink {

class VisibleUnitsWordTest : public EditingTestBase {
 protected:
  std::string DoStartOfWord(const std::string& selection_text) {
    const Position position = SetSelectionTextToBody(selection_text).Base();
    return GetSelectionTextFromBody(
        SelectionInDOMTree::Builder()
            .Collapse(
                StartOfWord(CreateVisiblePosition(position)).DeepEquivalent())
            .Build());
  }
};

TEST_F(VisibleUnitsWordTest, StartOfWordBasic) {
  EXPECT_EQ("<p> |(1) abc def</p>", DoStartOfWord("<p>| (1) abc def</p>"));
  EXPECT_EQ("<p> |(1) abc def</p>", DoStartOfWord("<p> |(1) abc def</p>"));
  EXPECT_EQ("<p> (|1) abc def</p>", DoStartOfWord("<p> (|1) abc def</p>"));
  EXPECT_EQ("<p> (1|) abc def</p>", DoStartOfWord("<p> (1|) abc def</p>"));
  EXPECT_EQ("<p> (1)| abc def</p>", DoStartOfWord("<p> (1)| abc def</p>"));
  EXPECT_EQ("<p> (1) |abc def</p>", DoStartOfWord("<p> (1) |abc def</p>"));
  EXPECT_EQ("<p> (1) |abc def</p>", DoStartOfWord("<p> (1) a|bc def</p>"));
  EXPECT_EQ("<p> (1) |abc def</p>", DoStartOfWord("<p> (1) ab|c def</p>"));
  EXPECT_EQ("<p> (1) abc| def</p>", DoStartOfWord("<p> (1) abc| def</p>"));
  EXPECT_EQ("<p> (1) abc |def</p>", DoStartOfWord("<p> (1) abc |def</p>"));
  EXPECT_EQ("<p> (1) abc |def</p>", DoStartOfWord("<p> (1) abc d|ef</p>"));
  EXPECT_EQ("<p> (1) abc |def</p>", DoStartOfWord("<p> (1) abc de|f</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoStartOfWord("<p> (1) abc def|</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoStartOfWord("<p> (1) abc def</p>|"));
}

TEST_F(VisibleUnitsWordTest, StartOfWordCrossing) {
  EXPECT_EQ("<b>|abc</b><i>def</i>", DoStartOfWord("<b>abc</b><i>|def</i>"));
  EXPECT_EQ("<b>abc</b><i>def|</i>", DoStartOfWord("<b>abc</b><i>def</i>|"));
}

TEST_F(VisibleUnitsWordTest, StartOfWordFirstLetter) {
  InsertStyleElement("p::first-letter {font-size:200%;}");
  // Note: Expectations should match with |StartOfWordBasic|.
  EXPECT_EQ("<p> |(1) abc def</p>", DoStartOfWord("<p>| (1) abc def</p>"));
  EXPECT_EQ("<p> |(1) abc def</p>", DoStartOfWord("<p> |(1) abc def</p>"));
  EXPECT_EQ("<p> (|1) abc def</p>", DoStartOfWord("<p> (|1) abc def</p>"));
  EXPECT_EQ("<p> (1|) abc def</p>", DoStartOfWord("<p> (1|) abc def</p>"));
  EXPECT_EQ("<p> (1)| abc def</p>", DoStartOfWord("<p> (1)| abc def</p>"));
  EXPECT_EQ("<p> (1) |abc def</p>", DoStartOfWord("<p> (1) |abc def</p>"));
  EXPECT_EQ("<p> (1) |abc def</p>", DoStartOfWord("<p> (1) a|bc def</p>"));
  EXPECT_EQ("<p> (1) |abc def</p>", DoStartOfWord("<p> (1) ab|c def</p>"));
  EXPECT_EQ("<p> (1) abc| def</p>", DoStartOfWord("<p> (1) abc| def</p>"));
  EXPECT_EQ("<p> (1) abc |def</p>", DoStartOfWord("<p> (1) abc |def</p>"));
  EXPECT_EQ("<p> (1) abc |def</p>", DoStartOfWord("<p> (1) abc d|ef</p>"));
  EXPECT_EQ("<p> (1) abc |def</p>", DoStartOfWord("<p> (1) abc de|f</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoStartOfWord("<p> (1) abc def|</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoStartOfWord("<p> (1) abc def</p>|"));
}

}  // namespace blink
