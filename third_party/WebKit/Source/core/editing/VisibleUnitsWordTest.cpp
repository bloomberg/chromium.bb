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
  std::string DoStartOfWord(
      const std::string& selection_text,
      EWordSide word_side = EWordSide::kNextWordIfOnBoundary) {
    const Position position = SetSelectionTextToBody(selection_text).Base();
    return GetSelectionTextFromBody(
        SelectionInDOMTree::Builder()
            .Collapse(StartOfWord(CreateVisiblePosition(position), word_side)
                          .DeepEquivalent())
            .Build());
  }

  std::string DoEndOfWord(
      const std::string& selection_text,
      EWordSide word_side = EWordSide::kNextWordIfOnBoundary) {
    const Position position = SetSelectionTextToBody(selection_text).Base();
    return GetSelectionTextFromBody(
        SelectionInDOMTree::Builder()
            .Collapse(EndOfWord(CreateVisiblePosition(position), word_side)
                          .DeepEquivalent())
            .Build());
  }

  std::string DoNextWord(const std::string& selection_text) {
    const Position position = SetSelectionTextToBody(selection_text).Base();
    return GetSelectionTextFromBody(
        SelectionInDOMTree::Builder()
            .Collapse(NextWordPosition(CreateVisiblePosition(position))
                          .DeepEquivalent())
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

TEST_F(VisibleUnitsWordTest, StartOfWordPreviousWordIfOnBoundaryBasic) {
  EXPECT_EQ("<p> |(1) abc def</p>",
            DoStartOfWord("<p>| (1) abc def</p>",
                          EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> |(1) abc def</p>",
            DoStartOfWord("<p> |(1) abc def</p>",
                          EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> |(1) abc def</p>",
            DoStartOfWord("<p> (|1) abc def</p>",
                          EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (|1) abc def</p>",
            DoStartOfWord("<p> (1|) abc def</p>",
                          EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1|) abc def</p>",
            DoStartOfWord("<p> (1)| abc def</p>",
                          EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1)| abc def</p>",
            DoStartOfWord("<p> (1) |abc def</p>",
                          EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) |abc def</p>",
            DoStartOfWord("<p> (1) a|bc def</p>",
                          EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) |abc def</p>",
            DoStartOfWord("<p> (1) ab|c def</p>",
                          EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) |abc def</p>",
            DoStartOfWord("<p> (1) abc| def</p>",
                          EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) abc| def</p>",
            DoStartOfWord("<p> (1) abc |def</p>",
                          EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) abc |def</p>",
            DoStartOfWord("<p> (1) abc d|ef</p>",
                          EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) abc |def</p>",
            DoStartOfWord("<p> (1) abc de|f</p>",
                          EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) abc |def</p>",
            DoStartOfWord("<p> (1) abc def|</p>",
                          EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) abc |def</p>",
            DoStartOfWord("<p> (1) abc def</p>|",
                          EWordSide::kPreviousWordIfOnBoundary));
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

TEST_F(VisibleUnitsWordTest, StartOfWordTextSecurity) {
  // Note: |StartOfWord()| considers security characters as a sequence "x".
  InsertStyleElement("s {-webkit-text-security:disc;}");
  EXPECT_EQ("|abc<s>foo bar</s>baz", DoStartOfWord("|abc<s>foo bar</s>baz"));
  EXPECT_EQ("|abc<s>foo bar</s>baz", DoStartOfWord("abc|<s>foo bar</s>baz"));
  EXPECT_EQ("|abc<s>foo bar</s>baz", DoStartOfWord("abc<s>|foo bar</s>baz"));
  EXPECT_EQ("|abc<s>foo bar</s>baz", DoStartOfWord("abc<s>f|oo bar</s>baz"));
  EXPECT_EQ("|abc<s>foo bar</s>baz", DoStartOfWord("abc<s>foo| bar</s>baz"));
  EXPECT_EQ("|abc<s>foo bar</s>baz", DoStartOfWord("abc<s>foo |bar</s>baz"));
  EXPECT_EQ("|abc<s>foo bar</s>baz", DoStartOfWord("abc<s>foo bar|</s>baz"));
  EXPECT_EQ("|abc<s>foo bar</s>baz", DoStartOfWord("abc<s>foo bar</s>|baz"));
  EXPECT_EQ("|abc<s>foo bar</s>baz", DoStartOfWord("abc<s>foo bar</s>b|az"));
}

TEST_F(VisibleUnitsWordTest, EndOfWordBasic) {
  EXPECT_EQ("<p> (|1) abc def</p>", DoEndOfWord("<p>| (1) abc def</p>"));
  EXPECT_EQ("<p> (|1) abc def</p>", DoEndOfWord("<p> |(1) abc def</p>"));
  EXPECT_EQ("<p> (1|) abc def</p>", DoEndOfWord("<p> (|1) abc def</p>"));
  EXPECT_EQ("<p> (1)| abc def</p>", DoEndOfWord("<p> (1|) abc def</p>"));
  EXPECT_EQ("<p> (1) |abc def</p>", DoEndOfWord("<p> (1)| abc def</p>"));
  EXPECT_EQ("<p> (1) abc| def</p>", DoEndOfWord("<p> (1) |abc def</p>"));
  EXPECT_EQ("<p> (1) abc| def</p>", DoEndOfWord("<p> (1) a|bc def</p>"));
  EXPECT_EQ("<p> (1) abc| def</p>", DoEndOfWord("<p> (1) ab|c def</p>"));
  EXPECT_EQ("<p> (1) abc |def</p>", DoEndOfWord("<p> (1) abc| def</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoEndOfWord("<p> (1) abc |def</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoEndOfWord("<p> (1) abc d|ef</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoEndOfWord("<p> (1) abc de|f</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoEndOfWord("<p> (1) abc def|</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoEndOfWord("<p> (1) abc def</p>|"));
}

TEST_F(VisibleUnitsWordTest, EndOfWordPreviousWordIfOnBoundaryBasic) {
  EXPECT_EQ("<p> |(1) abc def</p>",
            DoEndOfWord("<p>| (1) abc def</p>",
                        EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> |(1) abc def</p>",
            DoEndOfWord("<p> |(1) abc def</p>",
                        EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (|1) abc def</p>",
            DoEndOfWord("<p> (|1) abc def</p>",
                        EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1|) abc def</p>",
            DoEndOfWord("<p> (1|) abc def</p>",
                        EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1)| abc def</p>",
            DoEndOfWord("<p> (1)| abc def</p>",
                        EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) |abc def</p>",
            DoEndOfWord("<p> (1) |abc def</p>",
                        EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) abc| def</p>",
            DoEndOfWord("<p> (1) a|bc def</p>",
                        EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) abc| def</p>",
            DoEndOfWord("<p> (1) ab|c def</p>",
                        EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) abc| def</p>",
            DoEndOfWord("<p> (1) abc| def</p>",
                        EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) abc |def</p>",
            DoEndOfWord("<p> (1) abc |def</p>",
                        EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) abc def|</p>",
            DoEndOfWord("<p> (1) abc d|ef</p>",
                        EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) abc def|</p>",
            DoEndOfWord("<p> (1) abc de|f</p>",
                        EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) abc def|</p>",
            DoEndOfWord("<p> (1) abc def|</p>",
                        EWordSide::kPreviousWordIfOnBoundary));
  EXPECT_EQ("<p> (1) abc def|</p>",
            DoEndOfWord("<p> (1) abc def</p>|",
                        EWordSide::kPreviousWordIfOnBoundary));
}

TEST_F(VisibleUnitsWordTest, EndOfWordTextSecurity) {
  // Note: |EndOfWord()| considers security characters as a sequence "x".
  InsertStyleElement("s {-webkit-text-security:disc;}");
  EXPECT_EQ("abc<s>foo bar</s>baz|", DoEndOfWord("|abc<s>foo bar</s>baz"));
  EXPECT_EQ("abc<s>foo bar</s>baz|", DoEndOfWord("abc|<s>foo bar</s>baz"));
  EXPECT_EQ("abc<s>foo bar</s>baz|", DoEndOfWord("abc<s>|foo bar</s>baz"));
  EXPECT_EQ("abc<s>foo bar</s>baz|", DoEndOfWord("abc<s>f|oo bar</s>baz"));
  EXPECT_EQ("abc<s>foo bar</s>baz|", DoEndOfWord("abc<s>foo| bar</s>baz"));
  EXPECT_EQ("abc<s>foo bar</s>baz|", DoEndOfWord("abc<s>foo |bar</s>baz"));
  EXPECT_EQ("abc<s>foo bar</s>baz|", DoEndOfWord("abc<s>foo bar|</s>baz"));
  EXPECT_EQ("abc<s>foo bar</s>baz|", DoEndOfWord("abc<s>foo bar</s>|baz"));
  EXPECT_EQ("abc<s>foo bar</s>baz|", DoEndOfWord("abc<s>foo bar</s>b|az"));
}

TEST_F(VisibleUnitsWordTest, NextWordBasic) {
  EXPECT_EQ("<p> (1|) abc def</p>", DoNextWord("<p>| (1) abc def</p>"));
  EXPECT_EQ("<p> (1|) abc def</p>", DoNextWord("<p> |(1) abc def</p>"));
  EXPECT_EQ("<p> (1|) abc def</p>", DoNextWord("<p> (|1) abc def</p>"));
  EXPECT_EQ("<p> (1) abc| def</p>", DoNextWord("<p> (1|) abc def</p>"));
  EXPECT_EQ("<p> (1) abc| def</p>", DoNextWord("<p> (1)| abc def</p>"));
  EXPECT_EQ("<p> (1) abc| def</p>", DoNextWord("<p> (1) |abc def</p>"));
  EXPECT_EQ("<p> (1) abc| def</p>", DoNextWord("<p> (1) a|bc def</p>"));
  EXPECT_EQ("<p> (1) abc| def</p>", DoNextWord("<p> (1) ab|c def</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoNextWord("<p> (1) abc| def</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoNextWord("<p> (1) abc |def</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoNextWord("<p> (1) abc d|ef</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoNextWord("<p> (1) abc de|f</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoNextWord("<p> (1) abc def|</p>"));
  EXPECT_EQ("<p> (1) abc def|</p>", DoNextWord("<p> (1) abc def</p>|"));
}

TEST_F(VisibleUnitsWordTest, NextWordCrossingBlock) {
  EXPECT_EQ("<p>abc|</p><p>def</p>", DoNextWord("<p>|abc</p><p>def</p>"));
  EXPECT_EQ("<p>abc</p><p>def|</p>", DoNextWord("<p>abc|</p><p>def</p>"));
}

TEST_F(VisibleUnitsWordTest, NextWordMixedEditability) {
  EXPECT_EQ(
      "<p contenteditable>"
      "abc<b contenteditable=\"false\">def ghi</b>|jkl mno</p>",
      DoNextWord("<p contenteditable>"
                 "|abc<b contenteditable=false>def ghi</b>jkl mno</p>"));
  EXPECT_EQ(
      "<p contenteditable>"
      "abc<b contenteditable=\"false\">def| ghi</b>jkl mno</p>",
      DoNextWord("<p contenteditable>"
                 "abc<b contenteditable=false>|def ghi</b>jkl mno</p>"));
  EXPECT_EQ(
      "<p contenteditable>"
      "abc<b contenteditable=\"false\">def ghi|</b>jkl mno</p>",
      DoNextWord("<p contenteditable>"
                 "abc<b contenteditable=false>def |ghi</b>jkl mno</p>"));
  EXPECT_EQ(
      "<p contenteditable>"
      "abc<b contenteditable=\"false\">def ghi|</b>jkl mno</p>",
      DoNextWord("<p contenteditable>"
                 "abc<b contenteditable=false>def ghi|</b>jkl mno</p>"));
}

TEST_F(VisibleUnitsWordTest, NextWordPunctuation) {
  EXPECT_EQ("abc|.def", DoNextWord("|abc.def"));
  EXPECT_EQ("abc|.def", DoNextWord("a|bc.def"));
  EXPECT_EQ("abc|.def", DoNextWord("ab|c.def"));
  EXPECT_EQ("abc.def|", DoNextWord("abc|.def"));
  EXPECT_EQ("abc.def|", DoNextWord("abc.|def"));

  EXPECT_EQ("abc|...def", DoNextWord("|abc...def"));
  EXPECT_EQ("abc|...def", DoNextWord("a|bc...def"));
  EXPECT_EQ("abc|...def", DoNextWord("ab|c...def"));
  EXPECT_EQ("abc...def|", DoNextWord("abc|...def"));
  EXPECT_EQ("abc...def|", DoNextWord("abc.|..def"));
  EXPECT_EQ("abc...def|", DoNextWord("abc..|.def"));
  EXPECT_EQ("abc...def|", DoNextWord("abc...|def"));
}

TEST_F(VisibleUnitsWordTest, NextWordSkipTab) {
  InsertStyleElement("s { white-space: pre }");
  EXPECT_EQ("<p><s>\t</s>foo|</p>", DoNextWord("<p><s>\t|</s>foo</p>"));
}

}  // namespace blink
