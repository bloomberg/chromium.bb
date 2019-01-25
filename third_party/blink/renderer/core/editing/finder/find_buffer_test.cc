// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"

#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class FindBufferTest : public EditingTestBase {
 protected:
  PositionInFlatTree LastPositionInDocument() {
    return GetDocument().documentElement()->lastChild()
               ? PositionInFlatTree::AfterNode(
                     *GetDocument().documentElement()->lastChild())
               : PositionInFlatTree::LastPositionInNode(
                     *GetDocument().documentElement());
  }

  EphemeralRangeInFlatTree WholeDocumentRange() {
    return EphemeralRangeInFlatTree(PositionInFlatTree::FirstPositionInNode(
                                        *GetDocument().documentElement()),
                                    LastPositionInDocument());
  }

  PositionInFlatTree PositionFromParentId(const char* id, unsigned offset) {
    return PositionInFlatTree(GetElementById(id)->firstChild(), offset);
  }

  std::string SerializeRange(const EphemeralRangeInFlatTree& range) {
    return GetSelectionTextInFlatTreeFromBody(
        SelectionInFlatTree::Builder().SetAsForwardSelection(range).Build());
  }
};

TEST_F(FindBufferTest, FindInline) {
  SetBodyContent(
      "<div id='container'>a<span id='span'>b</span><b id='b'>c</b><div "
      "id='none' style='display:none'>d</div><div id='inline-div' "
      "style='display: inline;'>e</div></div>");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_TRUE(buffer.PositionAfterBlock().IsNull());
  std::unique_ptr<FindBuffer::Results> results =
      buffer.FindMatches("abce", *mojom::blink::FindOptions::New());
  EXPECT_EQ(1u, results->CountForTesting());
  FindBuffer::BufferMatchResult match = *results->begin();
  EXPECT_EQ(0u, match.start);
  EXPECT_EQ(4u, match.length);
  EXPECT_EQ(
      EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                               PositionFromParentId("inline-div", 1)),
      buffer.RangeFromBufferIndex(match.start, match.start + match.length));
}

TEST_F(FindBufferTest, RangeFromBufferIndex) {
  SetBodyContent(
      "<div id='container'>a <span id='span'> b</span><b id='b'>cc</b><div "
      "id='none' style='display:none'>d</div><div id='inline-div' "
      "style='display: inline;'>e</div></div>");
  FindBuffer buffer(WholeDocumentRange());
  // Range for "a"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                                     PositionFromParentId("container", 1)),
            buffer.RangeFromBufferIndex(0, 1));
  EXPECT_EQ(
      "<div id=\"container\">^a| <span id=\"span\"> b</span><b "
      "id=\"b\">cc</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(0, 1)));
  // Range for "a "
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                                     PositionFromParentId("container", 2)),
            buffer.RangeFromBufferIndex(0, 2));
  EXPECT_EQ(
      "<div id=\"container\">^a |<span id=\"span\"> b</span><b "
      "id=\"b\">cc</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(0, 2)));
  // Range for "a b"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                                     PositionFromParentId("span", 2)),
            buffer.RangeFromBufferIndex(0, 3));
  EXPECT_EQ(
      "<div id=\"container\">^a <span id=\"span\"> b|</span><b "
      "id=\"b\">cc</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(0, 3)));
  // Range for "a bc"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                                     PositionFromParentId("b", 1)),
            buffer.RangeFromBufferIndex(0, 4));
  EXPECT_EQ(
      "<div id=\"container\">^a <span id=\"span\"> b</span><b "
      "id=\"b\">c|c</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(0, 4)));
  // Range for "a bcc"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                                     PositionFromParentId("b", 2)),
            buffer.RangeFromBufferIndex(0, 5));
  EXPECT_EQ(
      "<div id=\"container\">^a <span id=\"span\"> b</span><b "
      "id=\"b\">cc|</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(0, 5)));
  // Range for "a bcce"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                                     PositionFromParentId("inline-div", 1)),
            buffer.RangeFromBufferIndex(0, 6));
  EXPECT_EQ(
      "<div id=\"container\">^a <span id=\"span\"> b</span><b "
      "id=\"b\">cc</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e|</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(0, 6)));
  // Range for " b"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 1),
                                     PositionFromParentId("span", 2)),
            buffer.RangeFromBufferIndex(1, 3));
  EXPECT_EQ(
      "<div id=\"container\">a^ <span id=\"span\"> b|</span><b "
      "id=\"b\">cc</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(1, 3)));
  // Range for " bc"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 1),
                                     PositionFromParentId("b", 1)),
            buffer.RangeFromBufferIndex(1, 4));
  EXPECT_EQ(
      "<div id=\"container\">a^ <span id=\"span\"> b</span><b "
      "id=\"b\">c|c</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(1, 4)));
}

TEST_F(FindBufferTest, FindBetweenPositionsSameNode) {
  PositionInFlatTree start_position =
      ToPositionInFlatTree(SetCaretTextToBody("f|oofoo"));
  Node* node = start_position.ComputeContainerNode();
  // |end_position| = foofoo| (end of text).
  PositionInFlatTree end_position =
      PositionInFlatTree::LastPositionInNode(*node);
  FindBuffer buffer(EphemeralRangeInFlatTree(start_position, end_position));
  EXPECT_EQ(1u, buffer.FindMatches("foo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("oo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(4u, buffer.FindMatches("o", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("f", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  // |start_position| = fo|ofoo
  // |end_position| = foof|oo
  start_position = PositionInFlatTree(*node, 2u);
  end_position = PositionInFlatTree(*node, 4u);
  buffer = FindBuffer(EphemeralRangeInFlatTree(start_position, end_position));
  EXPECT_EQ(0u, buffer.FindMatches("foo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("oo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("o", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("f", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
}

TEST_F(FindBufferTest, FindBetweenPositionsDifferentNodes) {
  SetBodyContent(
      "<div id='div'>foo<span id='span'>foof<b id='b'>oo</b></span></div>");
  Element* div = GetElementById("div");
  Element* span = GetElementById("span");
  Element* b = GetElementById("b");
  // <div>^foo<span>foof|<b>oo</b></span></div>
  // So buffer = "foofoof"
  FindBuffer buffer(EphemeralRangeInFlatTree(
      PositionInFlatTree::FirstPositionInNode(*div->firstChild()),
      PositionInFlatTree::LastPositionInNode(*span->firstChild())));
  EXPECT_EQ(2u, buffer.FindMatches("foo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("fo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("oof", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("oo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(4u, buffer.FindMatches("o", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(3u, buffer.FindMatches("f", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  // <div>f^oo<span>foof<b>o|o</b></span></div>
  // So buffer = "oofoofo"
  buffer = FindBuffer(
      EphemeralRangeInFlatTree(PositionInFlatTree(*div->firstChild(), 1),
                               PositionInFlatTree(*b->firstChild(), 1)));
  EXPECT_EQ(1u, buffer.FindMatches("foo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("oof", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("fo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("oo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(5u, buffer.FindMatches("o", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("f", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  // <div>foo<span>f^oof|<b>oo</b></span></div>
  // So buffer = "oof"
  buffer = FindBuffer(
      EphemeralRangeInFlatTree(PositionInFlatTree(*span->firstChild(), 1),
                               PositionInFlatTree(*span->firstChild(), 4)));
  EXPECT_EQ(0u, buffer.FindMatches("foo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("oof", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("fo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("oo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("o", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("f", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  // <div>foo<span>foof^<b>oo|</b></span></div>
  // So buffer = "oo"
  buffer = FindBuffer(
      EphemeralRangeInFlatTree(PositionInFlatTree(*span->firstChild(), 4),
                               PositionInFlatTree(*b->firstChild(), 2)));
  EXPECT_EQ(0u, buffer.FindMatches("foo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("oof", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("fo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("oo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("o", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("f", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
}

TEST_F(FindBufferTest, FindBetweenPositionsSkippedNodes) {
  SetBodyContent(
      "<div id='div'>foo<span id='span' style='display:none'>foof</span><b "
      "id='b'>oo</b><script id='script'>fo</script><a id='a'>o</o></div>");
  Element* div = GetElementById("div");
  Element* span = GetElementById("span");
  Element* b = GetElementById("b");
  Element* script = GetElementById("script");
  Element* a = GetElementById("a");

  // <div>^foo<span style='display:none;'>foo|f</span><b>oo</b>
  // <script>fo</script><a>o</a></div>
  // So buffer = "foo"
  FindBuffer buffer(EphemeralRangeInFlatTree(
      PositionInFlatTree::FirstPositionInNode(*div->firstChild()),
      PositionInFlatTree(*span->firstChild(), 3)));
  EXPECT_EQ(1u, buffer.FindMatches("foo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("oof", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("fo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("oo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("o", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("f", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  // <div>foo<span style='display:none;'>f^oof</span><b>oo|</b>
  // <script>fo</script><a>o</a></div>
  // So buffer = "oo"
  buffer = FindBuffer(
      EphemeralRangeInFlatTree(PositionInFlatTree(*span->firstChild(), 1),
                               PositionInFlatTree(*b->firstChild(), 2)));
  EXPECT_EQ(0u, buffer.FindMatches("foo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("oo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("o", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("f", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  // <div>foo<span style='display:none;'>f^oof</span><b>oo|</b>
  // <script>f|o</script><a>o</a></div>
  // So buffer = "oo"
  buffer = FindBuffer(
      EphemeralRangeInFlatTree(PositionInFlatTree(*span->firstChild(), 1),
                               PositionInFlatTree(*script->firstChild(), 2)));
  EXPECT_EQ(0u, buffer.FindMatches("foo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("oo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("o", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("f", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  // <div>foo<span style='display:none;'>foof</span><b>oo|</b>
  // <script>f^o</script><a>o|</a></div>
  // So buffer = "o"
  buffer = FindBuffer(
      EphemeralRangeInFlatTree(PositionInFlatTree(*script->firstChild(), 1),
                               PositionInFlatTree(*a->firstChild(), 1)));
  EXPECT_EQ(0u, buffer.FindMatches("foo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("oo", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("o", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("f", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
}

class FindBufferBlockTest : public FindBufferTest,
                            public testing::WithParamInterface<std::string> {};

INSTANTIATE_TEST_CASE_P(Blocks,
                        FindBufferBlockTest,
                        testing::Values("block",
                                        "table",
                                        "flow-root",
                                        "grid",
                                        "flex",
                                        "list-item"));

TEST_P(FindBufferBlockTest, FindBlock) {
  SetBodyContent("text<div id='block' style='display: " + GetParam() +
                 ";'>block</div><span id='span'>span</span>");
  FindBuffer text_buffer(WholeDocumentRange());
  EXPECT_EQ(GetElementById("block"),
            *text_buffer.PositionAfterBlock().ComputeContainerNode());
  EXPECT_EQ(1u,
            text_buffer.FindMatches("text", *mojom::blink::FindOptions::New())
                ->CountForTesting());
  EXPECT_EQ(0u, text_buffer
                    .FindMatches("textblock", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(
      0u,
      text_buffer.FindMatches("text block", *mojom::blink::FindOptions::New())
          ->CountForTesting());

  FindBuffer block_buffer(EphemeralRangeInFlatTree(
      text_buffer.PositionAfterBlock(), LastPositionInDocument()));
  EXPECT_EQ(GetElementById("span"),
            *block_buffer.PositionAfterBlock().ComputeContainerNode());
  EXPECT_EQ(
      1u, block_buffer.FindMatches("block", *mojom::blink::FindOptions::New())
              ->CountForTesting());
  EXPECT_EQ(0u, block_buffer
                    .FindMatches("textblock", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(
      0u,
      block_buffer.FindMatches("text block", *mojom::blink::FindOptions::New())
          ->CountForTesting());
  EXPECT_EQ(0u, block_buffer
                    .FindMatches("blockspan", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(
      0u,
      block_buffer.FindMatches("block span", *mojom::blink::FindOptions::New())
          ->CountForTesting());

  FindBuffer span_buffer(EphemeralRangeInFlatTree(
      block_buffer.PositionAfterBlock(), LastPositionInDocument()));
  EXPECT_TRUE(span_buffer.PositionAfterBlock().IsNull());
  EXPECT_EQ(1u,
            span_buffer.FindMatches("span", *mojom::blink::FindOptions::New())
                ->CountForTesting());
  EXPECT_EQ(0u, span_buffer
                    .FindMatches("blockspan", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(
      0u,
      span_buffer.FindMatches("block span", *mojom::blink::FindOptions::New())
          ->CountForTesting());
}

class FindBufferSeparatorTest
    : public FindBufferTest,
      public testing::WithParamInterface<std::string> {};

INSTANTIATE_TEST_CASE_P(Separators,
                        FindBufferSeparatorTest,
                        testing::Values("br",
                                        "hr",
                                        "legend",
                                        "meter",
                                        "object",
                                        "progress",
                                        "select",
                                        "video"));

TEST_P(FindBufferSeparatorTest, FindSeparatedElements) {
  SetBodyContent("a<" + GetParam() + ">a</" + GetParam() + ">a");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_EQ(0u, buffer.FindMatches("aa", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
}

TEST_F(FindBufferTest, WhiteSpaceCollapsingPreWrap) {
  SetBodyContent(
      " a  \n   b  <b> c </b> d  <span style='white-space: pre-wrap'> e  "
      "</span>");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_EQ(
      1u, buffer.FindMatches("a b c d  e  ", *mojom::blink::FindOptions::New())
              ->CountForTesting());
};

TEST_F(FindBufferTest, WhiteSpaceCollapsingPre) {
  SetBodyContent("<div style='white-space: pre;'>a \n b</div>");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_EQ(1u, buffer.FindMatches("a", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("b", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("ab", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a b", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a  b", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a   b", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a\n b", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a \nb", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a \n b", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
}

TEST_F(FindBufferTest, WhiteSpaceCollapsingPreLine) {
  SetBodyContent("<div style='white-space: pre-line;'>a \n b</div>");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_EQ(1u, buffer.FindMatches("a", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("b", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("ab", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a b", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a  b", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a   b", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a \n b", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a\n b", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a \nb", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a\nb", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
}

TEST_F(FindBufferTest, BidiTest) {
  SetBodyContent("<bdo dir=rtl id=bdo>foo<span>bar</span></bdo>");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_EQ(1u, buffer.FindMatches("foobar", *mojom::blink::FindOptions::New())
                    ->CountForTesting());
}

}  // namespace blink
