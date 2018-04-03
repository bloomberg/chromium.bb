// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "core/editing/TextOffsetMapping.h"

#include "core/editing/Position.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/testing/EditingTestBase.h"
#include "platform/testing/runtime_enabled_features_test_helpers.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ParameterizedTextOffsetMappingTest
    : public ::testing::WithParamInterface<bool>,
      private ScopedLayoutNGForTest,
      public EditingTestBase {
 protected:
  ParameterizedTextOffsetMappingTest() : ScopedLayoutNGForTest(GetParam()) {}

  bool LayoutNGEnabled() const { return GetParam(); }

  std::string ComputeTextOffset(const std::string& selection_text) {
    const PositionInFlatTree position =
        ToPositionInFlatTree(SetSelectionTextToBody(selection_text).Base());
    TextOffsetMapping mapping(
        TextOffsetMapping::ComputeContainigBlock(position));
    const String text = mapping.GetText();
    const int offset = mapping.ComputeTextOffset(position);
    StringBuilder builder;
    builder.Append(text.Left(offset));
    builder.Append('|');
    builder.Append(text.Substring(offset));
    const CString result8 = builder.ToString().Utf8();
    return std::string(result8.data(), result8.length());
  }

  std::string GetRange(const std::string& selection_text) {
    const PositionInFlatTree position =
        ToPositionInFlatTree(SetSelectionTextToBody(selection_text).Base());
    TextOffsetMapping mapping(
        TextOffsetMapping::ComputeContainigBlock(position));
    return GetSelectionTextInFlatTreeFromBody(
        SelectionInFlatTree::Builder()
            .SetBaseAndExtent(mapping.GetRange())
            .Build());
  }

  std::string GetPositionBefore(const std::string& html_text, int offset) {
    SetBodyContent(html_text);
    TextOffsetMapping mapping(TextOffsetMapping::ComputeContainigBlock(
        PositionInFlatTree(*GetDocument().body()->firstChild(), 0)));
    return GetSelectionTextInFlatTreeFromBody(
        SelectionInFlatTree::Builder()
            .Collapse(mapping.GetPositionBefore(offset))
            .Build());
  }

  std::string GetPositionAfter(const std::string& html_text, int offset) {
    SetBodyContent(html_text);
    TextOffsetMapping mapping(TextOffsetMapping::ComputeContainigBlock(
        PositionInFlatTree(*GetDocument().body()->firstChild(), 0)));
    return GetSelectionTextInFlatTreeFromBody(
        SelectionInFlatTree::Builder()
            .Collapse(mapping.GetPositionAfter(offset))
            .Build());
  }
};

INSTANTIATE_TEST_CASE_P(All,
                        ParameterizedTextOffsetMappingTest,
                        ::testing::Bool());

TEST_P(ParameterizedTextOffsetMappingTest, ComputeTextOffsetBasic) {
  EXPECT_EQ("|(1) abc def", ComputeTextOffset("<p>| (1) abc def</p>"));
  EXPECT_EQ("|(1) abc def", ComputeTextOffset("<p> |(1) abc def</p>"));
  EXPECT_EQ("(|1) abc def", ComputeTextOffset("<p> (|1) abc def</p>"));
  EXPECT_EQ("(1|) abc def", ComputeTextOffset("<p> (1|) abc def</p>"));
  EXPECT_EQ("(1)| abc def", ComputeTextOffset("<p> (1)| abc def</p>"));
  EXPECT_EQ("(1) |abc def", ComputeTextOffset("<p> (1) |abc def</p>"));
  EXPECT_EQ("(1) a|bc def", ComputeTextOffset("<p> (1) a|bc def</p>"));
  EXPECT_EQ("(1) ab|c def", ComputeTextOffset("<p> (1) ab|c def</p>"));
  EXPECT_EQ("(1) abc| def", ComputeTextOffset("<p> (1) abc| def</p>"));
  EXPECT_EQ("(1) abc |def", ComputeTextOffset("<p> (1) abc |def</p>"));
  EXPECT_EQ("(1) abc d|ef", ComputeTextOffset("<p> (1) abc d|ef</p>"));
  EXPECT_EQ("(1) abc de|f", ComputeTextOffset("<p> (1) abc de|f</p>"));
  EXPECT_EQ("(1) abc def|", ComputeTextOffset("<p> (1) abc def|</p>"));
}

TEST_P(ParameterizedTextOffsetMappingTest, ComputeTextOffsetWithFirstLetter) {
  InsertStyleElement("p::first-letter {font-size:200%;}");
  // Expectation should be as same as |ComputeTextOffsetBasic|
  EXPECT_EQ("|(1) abc def", ComputeTextOffset("<p>| (1) abc def</p>"));
  EXPECT_EQ("|(1) abc def", ComputeTextOffset("<p> |(1) abc def</p>"));
  EXPECT_EQ("(|1) abc def", ComputeTextOffset("<p> (|1) abc def</p>"));
  EXPECT_EQ("(1|) abc def", ComputeTextOffset("<p> (1|) abc def</p>"));
  EXPECT_EQ("(1)| abc def", ComputeTextOffset("<p> (1)| abc def</p>"));
  EXPECT_EQ("(1) |abc def", ComputeTextOffset("<p> (1) |abc def</p>"));
  EXPECT_EQ("(1) a|bc def", ComputeTextOffset("<p> (1) a|bc def</p>"));
  EXPECT_EQ("(1) ab|c def", ComputeTextOffset("<p> (1) ab|c def</p>"));
  EXPECT_EQ("(1) abc| def", ComputeTextOffset("<p> (1) abc| def</p>"));
  EXPECT_EQ("(1) abc |def", ComputeTextOffset("<p> (1) abc |def</p>"));
  EXPECT_EQ("(1) abc d|ef", ComputeTextOffset("<p> (1) abc d|ef</p>"));
  EXPECT_EQ("(1) abc de|f", ComputeTextOffset("<p> (1) abc de|f</p>"));
  EXPECT_EQ("(1) abc def|", ComputeTextOffset("<p> (1) abc def|</p>"));
}

TEST_P(ParameterizedTextOffsetMappingTest, ComputeTextOffsetWithFloat) {
  InsertStyleElement("b { float:right; }");
  EXPECT_EQ("|aBCDe", ComputeTextOffset("<p>|a<b>BCD</b>e</p>"));
  EXPECT_EQ("a|BCDe", ComputeTextOffset("<p>a|<b>BCD</b>e</p>"));
  EXPECT_EQ("a|BCDe", ComputeTextOffset("<p>a<b>|BCD</b>e</p>"));
  EXPECT_EQ("aB|CDe", ComputeTextOffset("<p>a<b>B|CD</b>e</p>"));
  EXPECT_EQ("aBC|De", ComputeTextOffset("<p>a<b>BC|D</b>e</p>"));
  EXPECT_EQ("aBCD|e", ComputeTextOffset("<p>a<b>BCD|</b>e</p>"));
  EXPECT_EQ("aBCD|e", ComputeTextOffset("<p>a<b>BCD</b>|e</p>"));
  EXPECT_EQ("aBCDe|", ComputeTextOffset("<p>a<b>BCD</b>e|</p>"));
}

TEST_P(ParameterizedTextOffsetMappingTest, ComputeTextOffsetWithInlineBlock) {
  InsertStyleElement("b { display:inline-block; }");
  EXPECT_EQ("|aBCDe", ComputeTextOffset("<p>|a<b>BCD</b>e</p>"));
  EXPECT_EQ("a|BCDe", ComputeTextOffset("<p>a|<b>BCD</b>e</p>"));
  EXPECT_EQ("a|BCDe", ComputeTextOffset("<p>a<b>|BCD</b>e</p>"));
  EXPECT_EQ("aB|CDe", ComputeTextOffset("<p>a<b>B|CD</b>e</p>"));
  EXPECT_EQ("aBC|De", ComputeTextOffset("<p>a<b>BC|D</b>e</p>"));
  EXPECT_EQ("aBCD|e", ComputeTextOffset("<p>a<b>BCD|</b>e</p>"));
  EXPECT_EQ("aBCD|e", ComputeTextOffset("<p>a<b>BCD</b>|e</p>"));
  EXPECT_EQ("aBCDe|", ComputeTextOffset("<p>a<b>BCD</b>e|</p>"));
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeOfAnonymousBlock) {
  EXPECT_EQ("<div><p>abc</p>^def|<p>ghi</p></div>",
            GetRange("<div><p>abc</p>d|ef<p>ghi</p></div>"));
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeOfBlockOnInlineBlock) {
  // display:inline-block doesn't introduce block.
  EXPECT_EQ("^abc<p style=\"display:inline\">def<br>ghi</p>xyz|",
            GetRange("|abc<p style=display:inline>def<br>ghi</p>xyz"));
  EXPECT_EQ("^abc<p style=\"display:inline\">def<br>ghi</p>xyz|",
            GetRange("abc<p style=display:inline>|def<br>ghi</p>xyz"));
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeOfBlockWithAnonymousBlock) {
  // "abc" and "xyz" are in anonymous block.

  // Range is "abc"
  EXPECT_EQ("^abc|<p>def</p>xyz", GetRange("|abc<p>def</p>xyz"));
  EXPECT_EQ("^abc|<p>def</p>xyz", GetRange("a|bc<p>def</p>xyz"));

  // Range is "def"
  EXPECT_EQ("abc<p>^def|</p>xyz", GetRange("abc<p>|def</p>xyz"));
  EXPECT_EQ("abc<p>^def|</p>xyz", GetRange("abc<p>d|ef</p>xyz"));

  // Range is "xyz"
  EXPECT_EQ("abc<p>def</p>^xyz|", GetRange("abc<p>def</p>|xyz"));
  EXPECT_EQ("abc<p>def</p>^xyz|", GetRange("abc<p>def</p>xyz|"));
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeOfBlockWithBR) {
  EXPECT_EQ("^abc<br>xyz|", GetRange("abc|<br>xyz"))
      << "BR doesn't affect block";
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeOfBlockWithPRE) {
  // "\n" doesn't affect block.
  EXPECT_EQ("<pre>^abc\ndef\nghi\n|</pre>",
            GetRange("<pre>|abc\ndef\nghi\n</pre>"));
  EXPECT_EQ("<pre>^abc\ndef\nghi\n|</pre>",
            GetRange("<pre>abc\n|def\nghi\n</pre>"));
  EXPECT_EQ("<pre>^abc\ndef\nghi\n|</pre>",
            GetRange("<pre>abc\ndef\n|ghi\n</pre>"));
  EXPECT_EQ("<pre>^abc\ndef\nghi\n|</pre>",
            GetRange("<pre>abc\ndef\nghi\n|</pre>"));
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeOfBlockWithRUBY) {
  EXPECT_EQ("<ruby>^abc|<rt>123</rt></ruby>",
            GetRange("<ruby>|abc<rt>123</rt></ruby>"));
  EXPECT_EQ("<ruby>abc<rt>^123|</rt></ruby>",
            GetRange("<ruby>abc<rt>1|23</rt></ruby>"));
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeOfBlockWithRUBYandBR) {
  EXPECT_EQ("<ruby>^abc<br>def|<rt>123<br>456</rt></ruby>",
            GetRange("<ruby>|abc<br>def<rt>123<br>456</rt></ruby>"))
      << "RT(LayoutRubyRun) is a block";
  EXPECT_EQ("<ruby>abc<br>def<rt>^123<br>456|</rt></ruby>",
            GetRange("<ruby>abc<br>def<rt>123|<br>456</rt></ruby>"))
      << "RUBY introduce LayoutRuleBase for 'abc'";
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeOfBlockWithTABLE) {
  EXPECT_EQ("^abc|<table><tbody><tr><td>one</td></tr></tbody></table>xyz",
            GetRange("|abc<table><tr><td>one</td></tr></table>xyz"))
      << "Before TABLE";
  EXPECT_EQ("abc<table><tbody><tr><td>^one|</td></tr></tbody></table>xyz",
            GetRange("abc<table><tr><td>o|ne</td></tr></table>xyz"))
      << "In TD";
  EXPECT_EQ("abc<table><tbody><tr><td>one</td></tr></tbody></table>^xyz|",
            GetRange("abc<table><tr><td>one</td></tr></table>x|yz"))
      << "After TABLE";
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeOfEmptyBlock) {
  EXPECT_EQ("<div><p>abc</p><p>|</p><p>ghi</p></div>",
            GetRange("<div><p>abc</p><p>|</p><p>ghi</p></div>"));
}

TEST_P(ParameterizedTextOffsetMappingTest, GetPositionBefore) {
  EXPECT_EQ("  |012  456  ", GetPositionBefore("  012  456  ", 0));
  EXPECT_EQ("  0|12  456  ", GetPositionBefore("  012  456  ", 1));
  EXPECT_EQ("  01|2  456  ", GetPositionBefore("  012  456  ", 2));
  EXPECT_EQ("  012|  456  ", GetPositionBefore("  012  456  ", 3));
  EXPECT_EQ("  012  |456  ", GetPositionBefore("  012  456  ", 4));
  EXPECT_EQ("  012  4|56  ", GetPositionBefore("  012  456  ", 5));
  EXPECT_EQ("  012  45|6  ", GetPositionBefore("  012  456  ", 6));
  EXPECT_EQ("  012  456|  ", GetPositionBefore("  012  456  ", 7));
  // We hit DCHECK for offset 8, because we walk on "012 456".
}

TEST_P(ParameterizedTextOffsetMappingTest, GetPositionAfter) {
  EXPECT_EQ("  0|12  456  ", GetPositionAfter("  012  456  ", 0));
  EXPECT_EQ("  0|12  456  ", GetPositionAfter("  012  456  ", 1));
  EXPECT_EQ("  01|2  456  ", GetPositionAfter("  012  456  ", 2));
  EXPECT_EQ("  012|  456  ", GetPositionAfter("  012  456  ", 3));
  EXPECT_EQ("  012 | 456  ", GetPositionAfter("  012  456  ", 4));
  EXPECT_EQ("  012  4|56  ", GetPositionAfter("  012  456  ", 5));
  EXPECT_EQ("  012  45|6  ", GetPositionAfter("  012  456  ", 6));
  EXPECT_EQ("  012  456|  ", GetPositionAfter("  012  456  ", 7));
  // We hit DCHECK for offset 8, because we walk on "012 456".
}

}  // namespace blink
