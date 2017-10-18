// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_node.h"

#include "core/dom/FirstLetterPseudoElement.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/ng/inline/ng_offset_mapping_result.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// TODO(xiaochengh): Rename this test to NGOffsetMappingTest.

class NGInlineNodeOffsetMappingTest : public RenderingTest {
 protected:
  void SetUp() override {
    RenderingTest::SetUp();
    RuntimeEnabledFeatures::SetLayoutNGEnabled(true);
    style_ = ComputedStyle::Create();
    style_->GetFont().Update(nullptr);
  }

  void TearDown() override {
    RuntimeEnabledFeatures::SetLayoutNGEnabled(false);
    RenderingTest::TearDown();
  }

  void SetupHtml(const char* id, String html) {
    SetBodyInnerHTML(html);
    layout_block_flow_ = ToLayoutNGBlockFlow(GetLayoutObjectByElementId(id));
    layout_object_ = layout_block_flow_->FirstChild();
    style_ = layout_object_->Style();
  }

  const NGOffsetMappingResult& GetOffsetMapping() const {
    return NGInlineNode(layout_block_flow_).ComputeOffsetMappingIfNeeded();
  }

  bool IsOffsetMappingStored() const {
    return layout_block_flow_->GetNGInlineNodeData()->offset_mapping_.get();
  }

  const LayoutText* GetLayoutTextUnder(const char* parent_id) {
    Element* parent = GetDocument().getElementById(parent_id);
    return ToLayoutText(parent->firstChild()->GetLayoutObject());
  }

  const NGOffsetMappingUnit* GetUnitForDOMOffset(const Node& node,
                                                 unsigned offset) const {
    return GetOffsetMapping().GetMappingUnitForDOMOffset(node, offset);
  }

  size_t GetTextContentOffset(const Node& node, unsigned offset) const {
    return GetOffsetMapping().GetTextContentOffset(node, offset);
  }

  unsigned StartOfNextNonCollapsedCharacter(const Node& node,
                                            unsigned offset) const {
    return GetOffsetMapping().StartOfNextNonCollapsedCharacter(node, offset);
  }

  unsigned EndOfLastNonCollapsedCharacter(const Node& node,
                                          unsigned offset) const {
    return GetOffsetMapping().EndOfLastNonCollapsedCharacter(node, offset);
  }

  bool IsNonCollapsedCharacter(const Node& node, unsigned offset) const {
    return GetOffsetMapping().IsNonCollapsedCharacter(node, offset);
  }

  bool IsAfterNonCollapsedCharacter(const Node& node, unsigned offset) const {
    return GetOffsetMapping().IsAfterNonCollapsedCharacter(node, offset);
  }

  RefPtr<const ComputedStyle> style_;
  LayoutNGBlockFlow* layout_block_flow_ = nullptr;
  LayoutObject* layout_object_ = nullptr;
  FontCachePurgePreventer purge_preventer_;
};

#define TEST_UNIT(unit, type, owner, dom_start, dom_end, text_content_start, \
                  text_content_end)                                          \
  EXPECT_EQ(type, unit.GetType());                                           \
  EXPECT_EQ(owner, &unit.GetOwner());                                        \
  EXPECT_EQ(dom_start, unit.DOMStart());                                     \
  EXPECT_EQ(dom_end, unit.DOMEnd());                                         \
  EXPECT_EQ(text_content_start, unit.TextContentStart());                    \
  EXPECT_EQ(text_content_end, unit.TextContentEnd())

#define TEST_RANGE(ranges, owner, start, end) \
  ASSERT_TRUE(ranges.Contains(owner));        \
  EXPECT_EQ(start, ranges.at(owner).first);   \
  EXPECT_EQ(end, ranges.at(owner).second)

TEST_F(NGInlineNodeOffsetMappingTest, StoredResult) {
  SetupHtml("t", "<div id=t>foo</div>");
  EXPECT_FALSE(IsOffsetMappingStored());
  GetOffsetMapping();
  EXPECT_TRUE(IsOffsetMappingStored());
}

TEST_F(NGInlineNodeOffsetMappingTest, GetNGInlineNodeForText) {
  SetupHtml("t", "<div id=t>foo</div>");
  Element* div = GetDocument().getElementById("t");
  Node* text = div->firstChild();

  Optional<NGInlineNode> inline_node = GetNGInlineNodeFor(*text);
  ASSERT_TRUE(inline_node.has_value());
  EXPECT_EQ(layout_block_flow_, inline_node->GetLayoutBlockFlow());
}

TEST_F(NGInlineNodeOffsetMappingTest, CantGetNGInlineNodeForBody) {
  SetupHtml("t", "<div id=t>foo</div>");
  Element* div = GetDocument().getElementById("t");

  Optional<NGInlineNode> inline_node = GetNGInlineNodeFor(*div);
  EXPECT_FALSE(inline_node.has_value());
}

TEST_F(NGInlineNodeOffsetMappingTest, OneTextNode) {
  SetupHtml("t", "<div id=t>foo</div>");
  const Node* foo_node = layout_object_->GetNode();
  const NGOffsetMappingResult& result = GetOffsetMapping();

  EXPECT_EQ("foo", result.GetText());

  ASSERT_EQ(1u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 3u, 0u, 3u);

  ASSERT_EQ(1u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 1u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 0));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 1));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 2));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 3));

  EXPECT_EQ(0u, GetTextContentOffset(*foo_node, 0));
  EXPECT_EQ(1u, GetTextContentOffset(*foo_node, 1));
  EXPECT_EQ(2u, GetTextContentOffset(*foo_node, 2));
  EXPECT_EQ(3u, GetTextContentOffset(*foo_node, 3));

  EXPECT_EQ(0u, StartOfNextNonCollapsedCharacter(*foo_node, 0));
  EXPECT_EQ(1u, StartOfNextNonCollapsedCharacter(*foo_node, 1));
  EXPECT_EQ(2u, StartOfNextNonCollapsedCharacter(*foo_node, 2));
  EXPECT_EQ(3u, StartOfNextNonCollapsedCharacter(*foo_node, 3));

  EXPECT_EQ(0u, EndOfLastNonCollapsedCharacter(*foo_node, 0));
  EXPECT_EQ(1u, EndOfLastNonCollapsedCharacter(*foo_node, 1));
  EXPECT_EQ(2u, EndOfLastNonCollapsedCharacter(*foo_node, 2));
  EXPECT_EQ(3u, EndOfLastNonCollapsedCharacter(*foo_node, 3));

  EXPECT_TRUE(IsNonCollapsedCharacter(*foo_node, 0));
  EXPECT_TRUE(IsNonCollapsedCharacter(*foo_node, 1));
  EXPECT_TRUE(IsNonCollapsedCharacter(*foo_node, 2));
  EXPECT_FALSE(IsNonCollapsedCharacter(*foo_node, 3));  // false at node end

  // false at node start
  EXPECT_FALSE(IsAfterNonCollapsedCharacter(*foo_node, 0));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*foo_node, 1));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*foo_node, 2));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*foo_node, 3));
}

TEST_F(NGInlineNodeOffsetMappingTest, TwoTextNodes) {
  SetupHtml("t", "<div id=t>foo<span id=s>bar</span></div>");
  const LayoutText* foo = ToLayoutText(layout_object_);
  const LayoutText* bar = GetLayoutTextUnder("s");
  const Node* foo_node = foo->GetNode();
  const Node* bar_node = bar->GetNode();
  const NGOffsetMappingResult& result = GetOffsetMapping();

  EXPECT_EQ("foobar", result.GetText());

  ASSERT_EQ(2u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 3u, 0u, 3u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, bar_node,
            0u, 3u, 3u, 6u);

  ASSERT_EQ(2u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 1u);
  TEST_RANGE(result.GetRanges(), bar_node, 1u, 2u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 0));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 1));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 2));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 3));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*bar_node, 0));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*bar_node, 1));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*bar_node, 2));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*bar_node, 3));

  EXPECT_EQ(0u, GetTextContentOffset(*foo_node, 0));
  EXPECT_EQ(1u, GetTextContentOffset(*foo_node, 1));
  EXPECT_EQ(2u, GetTextContentOffset(*foo_node, 2));
  EXPECT_EQ(3u, GetTextContentOffset(*foo_node, 3));
  EXPECT_EQ(3u, GetTextContentOffset(*bar_node, 0));
  EXPECT_EQ(4u, GetTextContentOffset(*bar_node, 1));
  EXPECT_EQ(5u, GetTextContentOffset(*bar_node, 2));
  EXPECT_EQ(6u, GetTextContentOffset(*bar_node, 3));

  EXPECT_TRUE(IsNonCollapsedCharacter(*foo_node, 0));
  EXPECT_TRUE(IsNonCollapsedCharacter(*foo_node, 1));
  EXPECT_TRUE(IsNonCollapsedCharacter(*foo_node, 2));
  EXPECT_FALSE(IsNonCollapsedCharacter(*foo_node, 3));  // false at node end

  EXPECT_TRUE(IsNonCollapsedCharacter(*bar_node, 0));
  EXPECT_TRUE(IsNonCollapsedCharacter(*bar_node, 1));
  EXPECT_TRUE(IsNonCollapsedCharacter(*bar_node, 2));
  EXPECT_FALSE(IsNonCollapsedCharacter(*bar_node, 3));  // false at node end

  // false at node start
  EXPECT_FALSE(IsAfterNonCollapsedCharacter(*foo_node, 0));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*foo_node, 1));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*foo_node, 2));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*foo_node, 3));

  // false at node start
  EXPECT_FALSE(IsAfterNonCollapsedCharacter(*bar_node, 0));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*bar_node, 1));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*bar_node, 2));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*bar_node, 3));
}

TEST_F(NGInlineNodeOffsetMappingTest, BRBetweenTextNodes) {
  SetupHtml("t", u"<div id=t>foo<br>bar</div>");
  const LayoutText* foo = ToLayoutText(layout_object_);
  const LayoutText* br = ToLayoutText(foo->NextSibling());
  const LayoutText* bar = ToLayoutText(br->NextSibling());
  const Node* foo_node = foo->GetNode();
  const Node* br_node = br->GetNode();
  const Node* bar_node = bar->GetNode();
  const NGOffsetMappingResult& result = GetOffsetMapping();

  EXPECT_EQ("foo\nbar", result.GetText());

  ASSERT_EQ(3u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 3u, 0u, 3u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, br_node,
            0u, 1u, 3u, 4u);
  TEST_UNIT(result.GetUnits()[2], NGOffsetMappingUnitType::kIdentity, bar_node,
            0u, 3u, 4u, 7u);

  ASSERT_EQ(3u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 1u);
  TEST_RANGE(result.GetRanges(), br_node, 1u, 2u);
  TEST_RANGE(result.GetRanges(), bar_node, 2u, 3u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 0));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 1));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 2));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 3));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 0));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 1));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 2));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 3));

  EXPECT_EQ(0u, GetTextContentOffset(*foo_node, 0));
  EXPECT_EQ(1u, GetTextContentOffset(*foo_node, 1));
  EXPECT_EQ(2u, GetTextContentOffset(*foo_node, 2));
  EXPECT_EQ(3u, GetTextContentOffset(*foo_node, 3));
  EXPECT_EQ(4u, GetTextContentOffset(*bar_node, 0));
  EXPECT_EQ(5u, GetTextContentOffset(*bar_node, 1));
  EXPECT_EQ(6u, GetTextContentOffset(*bar_node, 2));
  EXPECT_EQ(7u, GetTextContentOffset(*bar_node, 3));
}

TEST_F(NGInlineNodeOffsetMappingTest, OneTextNodeWithCollapsedSpace) {
  SetupHtml("t", "<div id=t>foo  bar</div>");
  const Node* node = layout_object_->GetNode();
  const NGOffsetMappingResult& result = GetOffsetMapping();

  EXPECT_EQ("foo bar", result.GetText());

  ASSERT_EQ(3u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, node, 0u,
            4u, 0u, 4u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kCollapsed, node, 4u,
            5u, 4u, 4u);
  TEST_UNIT(result.GetUnits()[2], NGOffsetMappingUnitType::kIdentity, node, 5u,
            8u, 4u, 7u);

  ASSERT_EQ(1u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), node, 0u, 3u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*node, 0));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*node, 1));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*node, 2));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*node, 3));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*node, 4));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*node, 5));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*node, 6));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*node, 7));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*node, 8));

  EXPECT_EQ(0u, GetTextContentOffset(*node, 0));
  EXPECT_EQ(1u, GetTextContentOffset(*node, 1));
  EXPECT_EQ(2u, GetTextContentOffset(*node, 2));
  EXPECT_EQ(3u, GetTextContentOffset(*node, 3));
  EXPECT_EQ(4u, GetTextContentOffset(*node, 4));
  EXPECT_EQ(4u, GetTextContentOffset(*node, 5));
  EXPECT_EQ(5u, GetTextContentOffset(*node, 6));
  EXPECT_EQ(6u, GetTextContentOffset(*node, 7));
  EXPECT_EQ(7u, GetTextContentOffset(*node, 8));

  EXPECT_EQ(3u, StartOfNextNonCollapsedCharacter(*node, 3));
  EXPECT_EQ(5u, StartOfNextNonCollapsedCharacter(*node, 4));
  EXPECT_EQ(5u, StartOfNextNonCollapsedCharacter(*node, 5));

  EXPECT_EQ(3u, EndOfLastNonCollapsedCharacter(*node, 3));
  EXPECT_EQ(4u, EndOfLastNonCollapsedCharacter(*node, 4));
  EXPECT_EQ(4u, EndOfLastNonCollapsedCharacter(*node, 5));

  EXPECT_TRUE(IsNonCollapsedCharacter(*node, 0));
  EXPECT_TRUE(IsNonCollapsedCharacter(*node, 1));
  EXPECT_TRUE(IsNonCollapsedCharacter(*node, 2));
  EXPECT_TRUE(IsNonCollapsedCharacter(*node, 3));
  EXPECT_FALSE(IsNonCollapsedCharacter(*node, 4));
  EXPECT_TRUE(IsNonCollapsedCharacter(*node, 5));
  EXPECT_TRUE(IsNonCollapsedCharacter(*node, 6));
  EXPECT_TRUE(IsNonCollapsedCharacter(*node, 7));
  EXPECT_FALSE(IsNonCollapsedCharacter(*node, 8));

  EXPECT_FALSE(IsAfterNonCollapsedCharacter(*node, 0));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*node, 1));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*node, 2));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*node, 3));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*node, 4));
  EXPECT_FALSE(IsAfterNonCollapsedCharacter(*node, 5));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*node, 6));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*node, 7));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*node, 8));
}

TEST_F(NGInlineNodeOffsetMappingTest, FullyCollapsedWhiteSpaceNode) {
  SetupHtml("t",
            "<div id=t>"
            "<span id=s1>foo </span>"
            " "
            "<span id=s2>bar</span>"
            "</div>");
  const LayoutText* foo = GetLayoutTextUnder("s1");
  const LayoutText* bar = GetLayoutTextUnder("s2");
  const LayoutText* space = ToLayoutText(layout_object_->NextSibling());
  const Node* foo_node = foo->GetNode();
  const Node* bar_node = bar->GetNode();
  const Node* space_node = space->GetNode();
  const NGOffsetMappingResult& result = GetOffsetMapping();

  EXPECT_EQ("foo bar", result.GetText());

  ASSERT_EQ(3u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 4u, 0u, 4u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kCollapsed,
            space_node, 0u, 1u, 4u, 4u);
  TEST_UNIT(result.GetUnits()[2], NGOffsetMappingUnitType::kIdentity, bar_node,
            0u, 3u, 4u, 7u);

  ASSERT_EQ(3u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 1u);
  TEST_RANGE(result.GetRanges(), space_node, 1u, 2u);
  TEST_RANGE(result.GetRanges(), bar_node, 2u, 3u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 0));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 1));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 2));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 3));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 4));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*space_node, 0));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*space_node, 1));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 0));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 1));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 2));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 3));

  EXPECT_EQ(0u, GetTextContentOffset(*foo_node, 0));
  EXPECT_EQ(1u, GetTextContentOffset(*foo_node, 1));
  EXPECT_EQ(2u, GetTextContentOffset(*foo_node, 2));
  EXPECT_EQ(3u, GetTextContentOffset(*foo_node, 3));
  EXPECT_EQ(4u, GetTextContentOffset(*foo_node, 4));
  EXPECT_EQ(4u, GetTextContentOffset(*space_node, 0));
  EXPECT_EQ(4u, GetTextContentOffset(*space_node, 1));
  EXPECT_EQ(4u, GetTextContentOffset(*bar_node, 0));
  EXPECT_EQ(5u, GetTextContentOffset(*bar_node, 1));
  EXPECT_EQ(6u, GetTextContentOffset(*bar_node, 2));
  EXPECT_EQ(7u, GetTextContentOffset(*bar_node, 3));

  EXPECT_EQ(0u, EndOfLastNonCollapsedCharacter(*space_node, 1u));
  EXPECT_EQ(1u, StartOfNextNonCollapsedCharacter(*space_node, 0u));
}

TEST_F(NGInlineNodeOffsetMappingTest, ReplacedElement) {
  SetupHtml("t", "<div id=t>foo <img> bar</div>");
  const LayoutText* foo = ToLayoutText(layout_object_);
  const LayoutText* bar = ToLayoutText(foo->NextSibling()->NextSibling());
  const Node* foo_node = foo->GetNode();
  const Node* bar_node = bar->GetNode();
  const NGOffsetMappingResult& result = GetOffsetMapping();

  ASSERT_EQ(2u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 4u, 0u, 4u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, bar_node,
            0u, 4u, 5u, 9u);

  ASSERT_EQ(2u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 1u);
  TEST_RANGE(result.GetRanges(), bar_node, 1u, 2u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 0));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 1));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 2));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 3));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 4));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*bar_node, 0));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*bar_node, 1));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*bar_node, 2));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*bar_node, 3));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*bar_node, 4));

  EXPECT_EQ(0u, GetTextContentOffset(*foo_node, 0));
  EXPECT_EQ(1u, GetTextContentOffset(*foo_node, 1));
  EXPECT_EQ(2u, GetTextContentOffset(*foo_node, 2));
  EXPECT_EQ(3u, GetTextContentOffset(*foo_node, 3));
  EXPECT_EQ(4u, GetTextContentOffset(*foo_node, 4));
  EXPECT_EQ(5u, GetTextContentOffset(*bar_node, 0));
  EXPECT_EQ(6u, GetTextContentOffset(*bar_node, 1));
  EXPECT_EQ(7u, GetTextContentOffset(*bar_node, 2));
  EXPECT_EQ(8u, GetTextContentOffset(*bar_node, 3));
  EXPECT_EQ(9u, GetTextContentOffset(*bar_node, 4));
}

TEST_F(NGInlineNodeOffsetMappingTest, FirstLetter) {
  SetupHtml("t",
            "<style>div:first-letter{color:red}</style>"
            "<div id=t>foo</div>");
  Element* div = GetDocument().getElementById("t");
  const Node* foo_node = div->firstChild();
  const NGOffsetMappingResult& result = GetOffsetMapping();

  ASSERT_EQ(1u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 3u, 0u, 3u);

  ASSERT_EQ(1u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 1u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 0));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 1));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 2));

  EXPECT_EQ(0u, GetTextContentOffset(*foo_node, 0));
  EXPECT_EQ(1u, GetTextContentOffset(*foo_node, 1));
  EXPECT_EQ(2u, GetTextContentOffset(*foo_node, 2));
}

TEST_F(NGInlineNodeOffsetMappingTest, FirstLetterWithLeadingSpace) {
  SetupHtml("t",
            "<style>div:first-letter{color:red}</style>"
            "<div id=t>  foo</div>");
  Element* div = GetDocument().getElementById("t");
  const Node* foo_node = div->firstChild();
  const NGOffsetMappingResult& result = GetOffsetMapping();

  ASSERT_EQ(2u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kCollapsed, foo_node,
            0u, 2u, 0u, 0u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, foo_node,
            2u, 5u, 0u, 3u);

  ASSERT_EQ(1u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 2u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 0));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 1));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*foo_node, 2));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*foo_node, 3));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*foo_node, 4));

  EXPECT_EQ(0u, GetTextContentOffset(*foo_node, 0));
  EXPECT_EQ(0u, GetTextContentOffset(*foo_node, 1));
  EXPECT_EQ(0u, GetTextContentOffset(*foo_node, 2));
  EXPECT_EQ(1u, GetTextContentOffset(*foo_node, 3));
  EXPECT_EQ(2u, GetTextContentOffset(*foo_node, 4));
}

TEST_F(NGInlineNodeOffsetMappingTest, FirstLetterWithoutRemainingText) {
  SetupHtml("t",
            "<style>div:first-letter{color:red}</style>"
            "<div id=t>  f</div>");
  Element* div = GetDocument().getElementById("t");
  const Node* text_node = div->firstChild();
  const NGOffsetMappingResult& result = GetOffsetMapping();

  ASSERT_EQ(2u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kCollapsed,
            text_node, 0u, 2u, 0u, 0u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, text_node,
            2u, 3u, 0u, 1u);

  ASSERT_EQ(1u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), text_node, 0u, 2u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*text_node, 0));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*text_node, 1));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*text_node, 2));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*text_node, 3));

  EXPECT_EQ(0u, GetTextContentOffset(*text_node, 0));
  EXPECT_EQ(0u, GetTextContentOffset(*text_node, 1));
  EXPECT_EQ(0u, GetTextContentOffset(*text_node, 2));
  EXPECT_EQ(1u, GetTextContentOffset(*text_node, 3));
}

TEST_F(NGInlineNodeOffsetMappingTest, FirstLetterInDifferentBlock) {
  SetupHtml("t",
            "<style>:first-letter{float:right}</style><div id=t>foo</div>");
  Element* div = GetDocument().getElementById("t");
  const Node* text_node = div->firstChild();

  Optional<NGInlineNode> inline_node0 = GetNGInlineNodeFor(*text_node, 0);
  Optional<NGInlineNode> inline_node1 = GetNGInlineNodeFor(*text_node, 1);
  Optional<NGInlineNode> inline_node2 = GetNGInlineNodeFor(*text_node, 2);
  Optional<NGInlineNode> inline_node3 = GetNGInlineNodeFor(*text_node, 3);

  ASSERT_TRUE(inline_node0.has_value());
  ASSERT_TRUE(inline_node1.has_value());
  ASSERT_TRUE(inline_node2.has_value());
  ASSERT_TRUE(inline_node3.has_value());

  // GetNGInlineNodeFor() returns different inline nodes for offset 0 and other
  // offsets, because first-letter is laid out in a different block.
  EXPECT_NE(inline_node0->GetLayoutBlockFlow(),
            inline_node1->GetLayoutBlockFlow());
  EXPECT_EQ(inline_node1->GetLayoutBlockFlow(),
            inline_node2->GetLayoutBlockFlow());
  EXPECT_EQ(inline_node2->GetLayoutBlockFlow(),
            inline_node3->GetLayoutBlockFlow());

  const NGOffsetMappingResult& first_letter_result =
      inline_node0->ComputeOffsetMappingIfNeeded();
  ASSERT_EQ(1u, first_letter_result.GetUnits().size());
  TEST_UNIT(first_letter_result.GetUnits()[0],
            NGOffsetMappingUnitType::kIdentity, text_node, 0u, 1u, 0u, 1u);
  ASSERT_EQ(1u, first_letter_result.GetRanges().size());
  TEST_RANGE(first_letter_result.GetRanges(), text_node, 0u, 1u);

  const NGOffsetMappingResult& remaining_text_result =
      inline_node1->ComputeOffsetMappingIfNeeded();
  ASSERT_EQ(1u, remaining_text_result.GetUnits().size());
  TEST_UNIT(remaining_text_result.GetUnits()[0],
            NGOffsetMappingUnitType::kIdentity, text_node, 1u, 3u, 1u, 3u);
  ASSERT_EQ(1u, remaining_text_result.GetRanges().size());
  TEST_RANGE(remaining_text_result.GetRanges(), text_node, 0u, 1u);

  EXPECT_EQ(&first_letter_result.GetUnits()[0],
            first_letter_result.GetMappingUnitForDOMOffset(*text_node, 0));
  EXPECT_EQ(&remaining_text_result.GetUnits()[0],
            remaining_text_result.GetMappingUnitForDOMOffset(*text_node, 1));
  EXPECT_EQ(&remaining_text_result.GetUnits()[0],
            remaining_text_result.GetMappingUnitForDOMOffset(*text_node, 2));
  EXPECT_EQ(&remaining_text_result.GetUnits()[0],
            remaining_text_result.GetMappingUnitForDOMOffset(*text_node, 3));

  EXPECT_EQ(0u, first_letter_result.GetTextContentOffset(*text_node, 0));
  EXPECT_EQ(1u, remaining_text_result.GetTextContentOffset(*text_node, 1));
  EXPECT_EQ(2u, remaining_text_result.GetTextContentOffset(*text_node, 2));
  EXPECT_EQ(3u, remaining_text_result.GetTextContentOffset(*text_node, 3));
}

TEST_F(NGInlineNodeOffsetMappingTest, WhiteSpaceTextNodeWithoutLayoutText) {
  SetupHtml("t", "<div id=t> <span>foo</span></div>");
  Element* div = GetDocument().getElementById("t");
  const Node* text_node = div->firstChild();

  EXPECT_EQ(0u, EndOfLastNonCollapsedCharacter(*text_node, 1u));
  EXPECT_EQ(1u, StartOfNextNonCollapsedCharacter(*text_node, 0u));
}

}  // namespace blink
