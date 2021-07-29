// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "third_party/blink/renderer/core/editing/text_offset_mapping.h"

#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ParameterizedTextOffsetMappingTest
    : public ::testing::WithParamInterface<bool>,
      private ScopedLayoutNGForTest,
      public EditingTestBase {
 protected:
  ParameterizedTextOffsetMappingTest() : ScopedLayoutNGForTest(GetParam()) {}

  std::string ComputeTextOffset(const std::string& selection_text) {
    const PositionInFlatTree position =
        ToPositionInFlatTree(SetSelectionTextToBody(selection_text).Base());
    TextOffsetMapping mapping(GetInlineContents(position));
    const String text = mapping.GetText();
    const int offset = mapping.ComputeTextOffset(position);
    StringBuilder builder;
    builder.Append(text.Left(offset));
    builder.Append('|');
    builder.Append(text.Substring(offset));
    return builder.ToString().Utf8();
  }

  std::string GetRange(const std::string& selection_text) {
    return GetRange(
        ToPositionInFlatTree(SetSelectionTextToBody(selection_text).Base()));
  }

  std::string GetRange(const PositionInFlatTree& position) {
    return GetRange(GetInlineContents(position));
  }

  std::string GetRange(const TextOffsetMapping::InlineContents& contents) {
    TextOffsetMapping mapping(contents);
    return GetSelectionTextInFlatTreeFromBody(
        SelectionInFlatTree::Builder()
            .SetBaseAndExtent(mapping.GetRange())
            .Build());
  }

  std::string GetPositionBefore(const std::string& html_text, int offset) {
    SetBodyContent(html_text);
    TextOffsetMapping mapping(GetInlineContents(
        PositionInFlatTree(*GetDocument().body()->firstChild(), 0)));
    return GetSelectionTextInFlatTreeFromBody(
        SelectionInFlatTree::Builder()
            .Collapse(mapping.GetPositionBefore(offset))
            .Build());
  }

  std::string GetPositionAfter(const std::string& html_text, int offset) {
    SetBodyContent(html_text);
    TextOffsetMapping mapping(GetInlineContents(
        PositionInFlatTree(*GetDocument().body()->firstChild(), 0)));
    return GetSelectionTextInFlatTreeFromBody(
        SelectionInFlatTree::Builder()
            .Collapse(mapping.GetPositionAfter(offset))
            .Build());
  }

 private:
  static TextOffsetMapping::InlineContents GetInlineContents(
      const PositionInFlatTree& position) {
    const TextOffsetMapping::InlineContents inline_contents =
        TextOffsetMapping::FindForwardInlineContents(position);
    DCHECK(inline_contents.IsNotNull()) << position;
    return inline_contents;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
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

// http://crbug.com/1124584
TEST_P(ParameterizedTextOffsetMappingTest, RangeOfBlockWithRubyAsBlock) {
  // We should not make <ruby> as |InlineContent| container because "XYZ" comes
  // before "abc" but in DOM tree, order is "abc" then "XYZ".
  // Layout tree:
  //  LayoutNGBlockFlow {BODY} at (8,8) size 784x27
  //   LayoutNGRubyAsBlock {RUBY} at (0,0) size 784x27
  //     LayoutNGRubyRun (anonymous) at (0,7) size 22x20
  //       LayoutNGRubyText {RT} at (0,-10) size 22x12
  //         LayoutText {#text} at (2,0) size 18x12
  //           text run at (2,0) width 18: "XYZ"
  //       LayoutNGRubyBase (anonymous) at (0,0) size 22x20
  //         LayoutText {#text} at (0,0) size 22x19
  //           text run at (0,0) width 22: "abc"
  InsertStyleElement("ruby { display: block; }");
  EXPECT_EQ("<ruby>^abc|<rt>XYZ</rt></ruby>",
            GetRange("|<ruby>abc<rt>XYZ</rt></ruby>"));
  EXPECT_EQ("<ruby>^abc|<rt>XYZ</rt></ruby>",
            GetRange("<ruby>|abc<rt>XYZ</rt></ruby>"));
  EXPECT_EQ("<ruby>abc<rt>^XYZ|</rt></ruby>",
            GetRange("<ruby>abc<rt>|XYZ</rt></ruby>"));
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeOfBlockWithRubyAsInlineBlock) {
  InsertStyleElement("ruby { display: inline-block; }");
  EXPECT_EQ("<ruby>^abc|<rt>XYZ</rt></ruby>",
            GetRange("|<ruby>abc<rt>XYZ</rt></ruby>"));
  EXPECT_EQ("<ruby>^abc|<rt>XYZ</rt></ruby>",
            GetRange("<ruby>|abc<rt>XYZ</rt></ruby>"));
  EXPECT_EQ("<ruby>abc<rt>^XYZ|</rt></ruby>",
            GetRange("<ruby>abc<rt>|XYZ</rt></ruby>"));
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

// |InlineContents| can represent an empty block.
// See LinkSelectionClickEventsTest.SingleAndDoubleClickWillBeHandled
TEST_P(ParameterizedTextOffsetMappingTest, RangeOfEmptyBlock) {
  const PositionInFlatTree position = ToPositionInFlatTree(
      SetSelectionTextToBody(
          "<div><p>abc</p><p id='target'>|</p><p>ghi</p></div>")
          .Base());
  const LayoutObject* const target_layout_object =
      GetDocument().getElementById("target")->GetLayoutObject();
  const TextOffsetMapping::InlineContents inline_contents =
      TextOffsetMapping::FindForwardInlineContents(position);
  ASSERT_TRUE(inline_contents.IsNotNull());
  EXPECT_EQ(target_layout_object, inline_contents.GetEmptyBlock());
  EXPECT_EQ(inline_contents,
            TextOffsetMapping::FindBackwardInlineContents(position));
}

// http://crbug.com/900906
TEST_P(ParameterizedTextOffsetMappingTest,
       AnonymousBlockFlowWrapperForFloatPseudo) {
  InsertStyleElement("table::after{content:close-quote;float:right;}");
  const PositionInFlatTree position =
      ToPositionInFlatTree(SetCaretTextToBody("<table></table>|foo"));
  const TextOffsetMapping::InlineContents inline_contents =
      TextOffsetMapping::FindBackwardInlineContents(position);
  ASSERT_TRUE(inline_contents.IsNotNull());
  const TextOffsetMapping::InlineContents previous_contents =
      TextOffsetMapping::InlineContents::PreviousOf(inline_contents);
  EXPECT_TRUE(previous_contents.IsNull());
}

TEST_P(ParameterizedTextOffsetMappingTest, ForwardRangesWithTextControl) {
  // InlineContents for positions outside text control should cover the entire
  // containing block.
  const PositionInFlatTree outside_position = ToPositionInFlatTree(
      SetCaretTextToBody("foo<!--|--><input value=\"bla\">bar"));
  const TextOffsetMapping::InlineContents outside_contents =
      TextOffsetMapping::FindForwardInlineContents(outside_position);
  EXPECT_EQ("^foo<input value=\"bla\"><div>bla</div></input>bar|",
            GetRange(outside_contents));

  // InlineContents for positions inside text control should not escape the text
  // control in forward iteration.
  const Element* input = GetDocument().QuerySelector("input");
  const PositionInFlatTree inside_first =
      PositionInFlatTree::FirstPositionInNode(*input);
  const TextOffsetMapping::InlineContents inside_contents =
      TextOffsetMapping::FindForwardInlineContents(inside_first);
  EXPECT_EQ("foo<input value=\"bla\"><div>^bla|</div></input>bar",
            GetRange(inside_contents));
  EXPECT_TRUE(
      TextOffsetMapping::InlineContents::NextOf(inside_contents).IsNull());

  const PositionInFlatTree inside_last =
      PositionInFlatTree::LastPositionInNode(*input);
  EXPECT_TRUE(
      TextOffsetMapping::FindForwardInlineContents(inside_last).IsNull());
}

TEST_P(ParameterizedTextOffsetMappingTest, BackwardRangesWithTextControl) {
  // InlineContents for positions outside text control should cover the entire
  // containing block.
  const PositionInFlatTree outside_position = ToPositionInFlatTree(
      SetCaretTextToBody("foo<input value=\"bla\"><!--|-->bar"));
  const TextOffsetMapping::InlineContents outside_contents =
      TextOffsetMapping::FindBackwardInlineContents(outside_position);
  EXPECT_EQ("^foo<input value=\"bla\"><div>bla</div></input>bar|",
            GetRange(outside_contents));

  // InlineContents for positions inside text control should not escape the text
  // control in backward iteration.
  const Element* input = GetDocument().QuerySelector("input");
  const PositionInFlatTree inside_last =
      PositionInFlatTree::LastPositionInNode(*input);
  const TextOffsetMapping::InlineContents inside_contents =
      TextOffsetMapping::FindBackwardInlineContents(inside_last);
  EXPECT_EQ("foo<input value=\"bla\"><div>^bla|</div></input>bar",
            GetRange(inside_contents));
  EXPECT_TRUE(
      TextOffsetMapping::InlineContents::PreviousOf(inside_contents).IsNull());

  const PositionInFlatTree inside_first =
      PositionInFlatTree::FirstPositionInNode(*input);
  EXPECT_TRUE(
      TextOffsetMapping::FindBackwardInlineContents(inside_first).IsNull());
}

// http://crbug.com/832497
TEST_P(ParameterizedTextOffsetMappingTest, RangeWithCollapsedWhitespace) {
  // Whitespaces after <div> is collapsed.
  EXPECT_EQ(" <div> ^<a></a>|</div>", GetRange("| <div> <a></a></div>"));
}

// http://crbug.com//832055
TEST_P(ParameterizedTextOffsetMappingTest, RangeWithMulticol) {
  InsertStyleElement("div { columns: 3 100px; }");
  EXPECT_EQ("<div>^<b>foo|</b></div>", GetRange("<div><b>foo|</b></div>"));
}

// http://crbug.com/832101
TEST_P(ParameterizedTextOffsetMappingTest, RangeWithNestedFloat) {
  InsertStyleElement("b, i { float: right; }");
  // Note: Legacy: BODY is inline, NG: BODY is block.
  EXPECT_EQ("^<b>abc <i>def</i> ghi</b>xyz|",
            GetRange("<b>abc <i>d|ef</i> ghi</b>xyz"));
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeWithNestedInlineBlock) {
  InsertStyleElement("b, i { display: inline-block; }");
  EXPECT_EQ("^<b>a <i>b</i> d</b>e|", GetRange("|<b>a <i>b</i> d</b>e"));
  EXPECT_EQ("^<b>a <i>b</i> d</b>e|", GetRange("<b>|a <i>b</i> d</b>e"));
  EXPECT_EQ("^<b>a <i>b</i> d</b>e|", GetRange("<b>a| <i>b</i> d</b>e"));
  EXPECT_EQ("^<b>a <i>b</i> d</b>e|", GetRange("<b>a |<i>b</i> d</b>e"));
  EXPECT_EQ("^<b>a <i>b</i> d</b>e|", GetRange("<b>a <i>|b</i> d</b>e"));
  EXPECT_EQ("^<b>a <i>b</i> d</b>e|", GetRange("<b>a <i>b|</i> d</b>e"));
  EXPECT_EQ("^<b>a <i>b</i> d</b>e|", GetRange("<b>a <i>b</i>| d</b>e"));
  EXPECT_EQ("^<b>a <i>b</i> d</b>e|", GetRange("<b>a <i>b</i> |d</b>e"));
  EXPECT_EQ("^<b>a <i>b</i> d</b>e|", GetRange("<b>a <i>b</i> d|</b>e"));
  EXPECT_EQ("^<b>a <i>b</i> d</b>e|", GetRange("<b>a <i>b</i> d</b>|e"));
  EXPECT_EQ("^<b>a <i>b</i> d</b>e|", GetRange("<b>a <i>b</i> d</b>e|"));
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeWithInlineBlockBlock) {
  InsertStyleElement("b { display:inline-block; }");
  // TODO(editing-dev): We should have "^a<b>b|<p>"
  EXPECT_EQ("^a<b>b<p>c</p>d</b>e|", GetRange("|a<b>b<p>c</p>d</b>e"));
  EXPECT_EQ("^a<b>b<p>c</p>d</b>e|", GetRange("a|<b>b<p>c</p>d</b>e"));
  EXPECT_EQ("a<b>^b|<p>c</p>d</b>e", GetRange("a<b>|b<p>c</p>d</b>e"));
  EXPECT_EQ("a<b>^b|<p>c</p>d</b>e", GetRange("a<b>b|<p>c</p>d</b>e"));
  EXPECT_EQ("a<b>b<p>^c|</p>d</b>e", GetRange("a<b>b<p>|c</p>d</b>e"));
  EXPECT_EQ("a<b>b<p>^c|</p>d</b>e", GetRange("a<b>b<p>c|</p>d</b>e"));
  EXPECT_EQ("a<b>b<p>c</p>^d|</b>e", GetRange("a<b>b<p>c</p>|d</b>e"));
  EXPECT_EQ("^a<b>b<p>c</p>d</b>e|", GetRange("a<b>b<p>c</p>d</b>|e"));
  EXPECT_EQ("^a<b>b<p>c</p>d</b>e|", GetRange("a<b>b<p>c</p>d</b>e|"));
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeWithInlineBlockBlocks) {
  InsertStyleElement("b { display:inline-block; }");
  // TODO(editing-dev): We should have "^a|"
  EXPECT_EQ("^a<b><p>b</p><p>c</p></b>d|",
            GetRange("|a<b><p>b</p><p>c</p></b>d"));
  EXPECT_EQ("^a<b><p>b</p><p>c</p></b>d|",
            GetRange("a|<b><p>b</p><p>c</p></b>d"));
  EXPECT_EQ("a<b><p>^b|</p><p>c</p></b>d",
            GetRange("a<b>|<p>b</p><p>c</p></b>d"));
  EXPECT_EQ("a<b><p>^b|</p><p>c</p></b>d",
            GetRange("a<b><p>|b</p><p>c</p></b>d"));
  EXPECT_EQ("a<b><p>^b|</p><p>c</p></b>d",
            GetRange("a<b><p>b|</p><p>c</p></b>d"));
  EXPECT_EQ("a<b><p>b</p><p>^c|</p></b>d",
            GetRange("a<b><p>b</p>|<p>c</p></b>d"));
  EXPECT_EQ("a<b><p>b</p><p>^c|</p></b>d",
            GetRange("a<b><p>b</p><p>|c</p></b>d"));
  EXPECT_EQ("a<b><p>b</p><p>^c|</p></b>d",
            GetRange("a<b><p>b</p><p>c|</p></b>d"));
  EXPECT_EQ("^a<b><p>b</p><p>c</p></b>d|",
            GetRange("a<b><p>b</p><p>c</p>|</b>d"));
  EXPECT_EQ("^a<b><p>b</p><p>c</p></b>d|",
            GetRange("a<b><p>b</p><p>c</p></b>|d"));
  EXPECT_EQ("^a<b><p>b</p><p>c</p></b>d|",
            GetRange("a<b><p>b</p><p>c</p></b>d|"));
}

// http://crbug.com/832101
TEST_P(ParameterizedTextOffsetMappingTest, RangeWithNestedPosition) {
  InsertStyleElement("b, i { position: fixed; }");
  EXPECT_EQ("<b>abc <i>^def|</i> ghi</b>xyz",
            GetRange("<b>abc <i>d|ef</i> ghi</b>xyz"));
}

// http://crbug.com//834623
TEST_P(ParameterizedTextOffsetMappingTest, RangeWithSelect1) {
  SetBodyContent("<select></select>foo");
  Element* select = GetDocument().QuerySelector("select");
  const auto& expected_outer =
      "^<select>"
      "<div aria-hidden=\"true\"></div>"
      "<slot name=\"user-agent-custom-assign-slot\"></slot>"
      "</select>foo|";
  const auto& expected_inner =
      "<select>"
      "<div aria-hidden=\"true\">^|</div>"
      "<slot name=\"user-agent-custom-assign-slot\"></slot>"
      "</select>foo";
  EXPECT_EQ(expected_outer, GetRange(PositionInFlatTree::BeforeNode(*select)));
  EXPECT_EQ(expected_inner, GetRange(PositionInFlatTree(select, 0)));
  EXPECT_EQ(expected_outer, GetRange(PositionInFlatTree::AfterNode(*select)));
}

TEST_P(ParameterizedTextOffsetMappingTest, RangeWithSelect2) {
  SetBodyContent("<select>bar</select>foo");
  Element* select = GetDocument().QuerySelector("select");
  const auto& expected_outer =
      "^<select>"
      "<div aria-hidden=\"true\"></div>"
      "<slot name=\"user-agent-custom-assign-slot\"></slot>"
      "</select>foo|";
  const auto& expected_inner =
      "<select>"
      "<div aria-hidden=\"true\">^|</div>"
      "<slot name=\"user-agent-custom-assign-slot\"></slot>"
      "</select>foo";
  EXPECT_EQ(expected_outer, GetRange(PositionInFlatTree::BeforeNode(*select)));
  EXPECT_EQ(expected_inner, GetRange(PositionInFlatTree(select, 0)));
  EXPECT_EQ(expected_outer, GetRange(PositionInFlatTree(select, 1)));
  EXPECT_EQ(expected_outer, GetRange(PositionInFlatTree::AfterNode(*select)));
}

// http://crbug.com//832350
TEST_P(ParameterizedTextOffsetMappingTest, RangeWithShadowDOM) {
  EXPECT_EQ("<div><slot>^abc|</slot></div>",
            GetRange("<div>"
                     "<template data-mode='open'><slot></slot></template>"
                     "|abc"
                     "</div>"));
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
  EXPECT_EQ("  01|2  456  ", GetPositionAfter("  012  456  ", 1));
  EXPECT_EQ("  012|  456  ", GetPositionAfter("  012  456  ", 2));
  EXPECT_EQ("  012 | 456  ", GetPositionAfter("  012  456  ", 3));
  EXPECT_EQ("  012  4|56  ", GetPositionAfter("  012  456  ", 4));
  EXPECT_EQ("  012  45|6  ", GetPositionAfter("  012  456  ", 5));
  EXPECT_EQ("  012  456|  ", GetPositionAfter("  012  456  ", 6));
  EXPECT_EQ("  012  456  |", GetPositionAfter("  012  456  ", 7));
  // We hit DCHECK for offset 8, because we walk on "012 456".
}

// https://crbug.com/903723
TEST_P(ParameterizedTextOffsetMappingTest, InlineContentsWithDocumentBoundary) {
  InsertStyleElement("*{position:fixed}");
  SetBodyContent("");
  const PositionInFlatTree position =
      PositionInFlatTree::FirstPositionInNode(*GetDocument().body());
  const TextOffsetMapping::InlineContents inline_contents =
      TextOffsetMapping::FindForwardInlineContents(position);
  EXPECT_TRUE(inline_contents.IsNotNull());
  // Should not crash when previous/next iteration reaches document boundary.
  EXPECT_TRUE(
      TextOffsetMapping::InlineContents::PreviousOf(inline_contents).IsNull());
  EXPECT_TRUE(
      TextOffsetMapping::InlineContents::NextOf(inline_contents).IsNull());
}

// https://crbug.com/1224206
TEST_P(ParameterizedTextOffsetMappingTest, ComputeTextOffsetWithBrokenImage) {
  SetBodyContent("A<img alt='X'>B<div>C</div>D");
  Element* img = GetDocument().QuerySelector("img");
  To<HTMLImageElement>(img)->EnsureCollapsedOrFallbackContent();
  UpdateAllLifecyclePhasesForTest();
  ShadowRoot* shadow = img->UserAgentShadowRoot();
  DCHECK(shadow);
  const Element* alt_img = shadow->getElementById("alttext-image");
  DCHECK(alt_img);

  const PositionInFlatTree position = PositionInFlatTree::BeforeNode(*alt_img);
  for (const TextOffsetMapping::InlineContents& inline_contents :
       {TextOffsetMapping::FindForwardInlineContents(position),
        TextOffsetMapping::FindBackwardInlineContents(position)}) {
    const TextOffsetMapping mapping(inline_contents);
    const String text = mapping.GetText();
    const unsigned offset = mapping.ComputeTextOffset(position);
    EXPECT_LE(offset, text.length());
    EXPECT_EQ("A,B", text);
    EXPECT_EQ(2u, offset);
  }
}

}  // namespace blink
