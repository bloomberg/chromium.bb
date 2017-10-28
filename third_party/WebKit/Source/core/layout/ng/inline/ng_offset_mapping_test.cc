// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_offset_mapping.h"

#include "core/dom/FirstLetterPseudoElement.h"
#include "core/editing/Position.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class NGOffsetMappingTest : public RenderingTest {
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

  const NGOffsetMapping& GetOffsetMapping() const {
    const NGOffsetMapping* map =
        NGInlineNode(layout_block_flow_).ComputeOffsetMappingIfNeeded();
    CHECK(map);
    return *map;
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

  Optional<unsigned> GetTextContentOffset(const Position& position) const {
    return GetOffsetMapping().GetTextContentOffset(position);
  }

  Optional<unsigned> StartOfNextNonCollapsedCharacter(const Node& node,
                                                      unsigned offset) const {
    return GetOffsetMapping().StartOfNextNonCollapsedCharacter(node, offset);
  }

  Optional<unsigned> EndOfLastNonCollapsedCharacter(const Node& node,
                                                    unsigned offset) const {
    return GetOffsetMapping().EndOfLastNonCollapsedCharacter(node, offset);
  }

  bool IsBeforeNonCollapsedCharacter(const Node& node, unsigned offset) const {
    return GetOffsetMapping().IsBeforeNonCollapsedCharacter(node, offset);
  }

  bool IsAfterNonCollapsedCharacter(const Node& node, unsigned offset) const {
    return GetOffsetMapping().IsAfterNonCollapsedCharacter(node, offset);
  }

  Position GetFirstPosition(unsigned offset) const {
    return GetOffsetMapping().GetFirstPosition(offset);
  }

  Position GetLastPosition(unsigned offset) const {
    return GetOffsetMapping().GetLastPosition(offset);
  }

  scoped_refptr<const ComputedStyle> style_;
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

TEST_F(NGOffsetMappingTest, StoredResult) {
  SetupHtml("t", "<div id=t>foo</div>");
  EXPECT_FALSE(IsOffsetMappingStored());
  GetOffsetMapping();
  EXPECT_TRUE(IsOffsetMappingStored());
}

TEST_F(NGOffsetMappingTest, OneTextNode) {
  SetupHtml("t", "<div id=t>foo</div>");
  const Node* foo_node = layout_object_->GetNode();
  const NGOffsetMapping& result = GetOffsetMapping();

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

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 2)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(foo_node, 3)));

  EXPECT_EQ(Position(foo_node, 0), GetFirstPosition(0));
  EXPECT_EQ(Position(foo_node, 1), GetFirstPosition(1));
  EXPECT_EQ(Position(foo_node, 2), GetFirstPosition(2));
  EXPECT_EQ(Position(foo_node, 3), GetFirstPosition(3));

  EXPECT_EQ(Position(foo_node, 0), GetLastPosition(0));
  EXPECT_EQ(Position(foo_node, 1), GetLastPosition(1));
  EXPECT_EQ(Position(foo_node, 2), GetLastPosition(2));
  EXPECT_EQ(Position(foo_node, 3), GetLastPosition(3));

  EXPECT_EQ(0u, *StartOfNextNonCollapsedCharacter(*foo_node, 0));
  EXPECT_EQ(1u, *StartOfNextNonCollapsedCharacter(*foo_node, 1));
  EXPECT_EQ(2u, *StartOfNextNonCollapsedCharacter(*foo_node, 2));
  EXPECT_FALSE(StartOfNextNonCollapsedCharacter(*foo_node, 3));

  EXPECT_FALSE(EndOfLastNonCollapsedCharacter(*foo_node, 0));
  EXPECT_EQ(1u, *EndOfLastNonCollapsedCharacter(*foo_node, 1));
  EXPECT_EQ(2u, *EndOfLastNonCollapsedCharacter(*foo_node, 2));
  EXPECT_EQ(3u, *EndOfLastNonCollapsedCharacter(*foo_node, 3));

  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*foo_node, 0));
  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*foo_node, 1));
  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*foo_node, 2));
  EXPECT_FALSE(
      IsBeforeNonCollapsedCharacter(*foo_node, 3));  // false at node end

  // false at node start
  EXPECT_FALSE(IsAfterNonCollapsedCharacter(*foo_node, 0));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*foo_node, 1));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*foo_node, 2));
  EXPECT_TRUE(IsAfterNonCollapsedCharacter(*foo_node, 3));
}

TEST_F(NGOffsetMappingTest, TwoTextNodes) {
  SetupHtml("t", "<div id=t>foo<span id=s>bar</span></div>");
  const LayoutText* foo = ToLayoutText(layout_object_);
  const LayoutText* bar = GetLayoutTextUnder("s");
  const Node* foo_node = foo->GetNode();
  const Node* bar_node = bar->GetNode();
  const NGOffsetMapping& result = GetOffsetMapping();

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

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 2)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(foo_node, 3)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(bar_node, 0)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(bar_node, 1)));
  EXPECT_EQ(5u, *GetTextContentOffset(Position(bar_node, 2)));
  EXPECT_EQ(6u, *GetTextContentOffset(Position(bar_node, 3)));

  EXPECT_EQ(Position(foo_node, 3), GetFirstPosition(3));
  EXPECT_EQ(Position(bar_node, 0), GetLastPosition(3));

  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*foo_node, 0));
  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*foo_node, 1));
  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*foo_node, 2));
  EXPECT_FALSE(
      IsBeforeNonCollapsedCharacter(*foo_node, 3));  // false at node end

  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*bar_node, 0));
  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*bar_node, 1));
  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*bar_node, 2));
  EXPECT_FALSE(
      IsBeforeNonCollapsedCharacter(*bar_node, 3));  // false at node end

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

TEST_F(NGOffsetMappingTest, BRBetweenTextNodes) {
  SetupHtml("t", u"<div id=t>foo<br>bar</div>");
  const LayoutText* foo = ToLayoutText(layout_object_);
  const LayoutText* br = ToLayoutText(foo->NextSibling());
  const LayoutText* bar = ToLayoutText(br->NextSibling());
  const Node* foo_node = foo->GetNode();
  const Node* br_node = br->GetNode();
  const Node* bar_node = bar->GetNode();
  const NGOffsetMapping& result = GetOffsetMapping();

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

  // TODO(xiaochengh): Add test cases for BR@BeforeNode and BR@AfterNode

  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 0));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 1));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 2));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 3));

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 2)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(foo_node, 3)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position::BeforeNode(*br_node)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position::AfterNode(*br_node)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(bar_node, 0)));
  EXPECT_EQ(5u, *GetTextContentOffset(Position(bar_node, 1)));
  EXPECT_EQ(6u, *GetTextContentOffset(Position(bar_node, 2)));
  EXPECT_EQ(7u, *GetTextContentOffset(Position(bar_node, 3)));

  EXPECT_EQ(Position(foo_node, 3), GetFirstPosition(3));
  EXPECT_EQ(Position::BeforeNode(*br_node), GetLastPosition(3));
  EXPECT_EQ(Position::AfterNode(*br_node), GetFirstPosition(4));
  EXPECT_EQ(Position(bar_node, 0), GetLastPosition(4));
}

TEST_F(NGOffsetMappingTest, OneTextNodeWithCollapsedSpace) {
  SetupHtml("t", "<div id=t>foo  bar</div>");
  const Node* node = layout_object_->GetNode();
  const NGOffsetMapping& result = GetOffsetMapping();

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

  EXPECT_EQ(0u, *GetTextContentOffset(Position(node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(node, 2)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(node, 3)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(node, 4)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(node, 5)));
  EXPECT_EQ(5u, *GetTextContentOffset(Position(node, 6)));
  EXPECT_EQ(6u, *GetTextContentOffset(Position(node, 7)));
  EXPECT_EQ(7u, *GetTextContentOffset(Position(node, 8)));

  EXPECT_EQ(Position(node, 4), GetFirstPosition(4));
  EXPECT_EQ(Position(node, 5), GetLastPosition(4));

  EXPECT_EQ(3u, *StartOfNextNonCollapsedCharacter(*node, 3));
  EXPECT_EQ(5u, *StartOfNextNonCollapsedCharacter(*node, 4));
  EXPECT_EQ(5u, *StartOfNextNonCollapsedCharacter(*node, 5));

  EXPECT_EQ(3u, *EndOfLastNonCollapsedCharacter(*node, 3));
  EXPECT_EQ(4u, *EndOfLastNonCollapsedCharacter(*node, 4));
  EXPECT_EQ(4u, *EndOfLastNonCollapsedCharacter(*node, 5));

  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*node, 0));
  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*node, 1));
  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*node, 2));
  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*node, 3));
  EXPECT_FALSE(IsBeforeNonCollapsedCharacter(*node, 4));
  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*node, 5));
  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*node, 6));
  EXPECT_TRUE(IsBeforeNonCollapsedCharacter(*node, 7));
  EXPECT_FALSE(IsBeforeNonCollapsedCharacter(*node, 8));

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

TEST_F(NGOffsetMappingTest, FullyCollapsedWhiteSpaceNode) {
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
  const NGOffsetMapping& result = GetOffsetMapping();

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

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 2)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(foo_node, 3)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(foo_node, 4)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(space_node, 0)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(space_node, 1)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(bar_node, 0)));
  EXPECT_EQ(5u, *GetTextContentOffset(Position(bar_node, 1)));
  EXPECT_EQ(6u, *GetTextContentOffset(Position(bar_node, 2)));
  EXPECT_EQ(7u, *GetTextContentOffset(Position(bar_node, 3)));

  EXPECT_EQ(Position(foo_node, 4), GetFirstPosition(4));
  EXPECT_EQ(Position(bar_node, 0), GetLastPosition(4));

  EXPECT_FALSE(EndOfLastNonCollapsedCharacter(*space_node, 1u));
  EXPECT_FALSE(StartOfNextNonCollapsedCharacter(*space_node, 0u));
}

TEST_F(NGOffsetMappingTest, ReplacedElement) {
  SetupHtml("t", "<div id=t>foo <img> bar</div>");
  const LayoutText* foo = ToLayoutText(layout_object_);
  const LayoutObject* img = foo->NextSibling();
  const LayoutText* bar = ToLayoutText(img->NextSibling());
  const Node* foo_node = foo->GetNode();
  const Node* img_node = img->GetNode();
  const Node* bar_node = bar->GetNode();
  const NGOffsetMapping& result = GetOffsetMapping();

  ASSERT_EQ(3u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 4u, 0u, 4u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, img_node,
            0u, 1u, 4u, 5u);
  TEST_UNIT(result.GetUnits()[2], NGOffsetMappingUnitType::kIdentity, bar_node,
            0u, 4u, 5u, 9u);

  ASSERT_EQ(3u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 1u);
  TEST_RANGE(result.GetRanges(), img_node, 1u, 2u);
  TEST_RANGE(result.GetRanges(), bar_node, 2u, 3u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 0));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 1));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 2));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 3));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 4));

  // TODO(xiaochengh): Pass positions IMG@BeforeNode and IMG@AfterNode instead
  // of (node, offset) pairs.
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*img_node, 0));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForDOMOffset(*img_node, 1));

  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 0));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 1));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 2));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 3));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForDOMOffset(*bar_node, 4));

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 2)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(foo_node, 3)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(foo_node, 4)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position::BeforeNode(*img_node)));
  EXPECT_EQ(5u, *GetTextContentOffset(Position::AfterNode(*img_node)));
  EXPECT_EQ(5u, *GetTextContentOffset(Position(bar_node, 0)));
  EXPECT_EQ(6u, *GetTextContentOffset(Position(bar_node, 1)));
  EXPECT_EQ(7u, *GetTextContentOffset(Position(bar_node, 2)));
  EXPECT_EQ(8u, *GetTextContentOffset(Position(bar_node, 3)));
  EXPECT_EQ(9u, *GetTextContentOffset(Position(bar_node, 4)));

  EXPECT_EQ(Position(foo_node, 4), GetFirstPosition(4));
  EXPECT_EQ(Position::BeforeNode(*img_node), GetLastPosition(4));
  EXPECT_EQ(Position::AfterNode(*img_node), GetFirstPosition(5));
  EXPECT_EQ(Position(bar_node, 0), GetLastPosition(5));
}

TEST_F(NGOffsetMappingTest, FirstLetter) {
  SetupHtml("t",
            "<style>div:first-letter{color:red}</style>"
            "<div id=t>foo</div>");
  Element* div = GetDocument().getElementById("t");
  const Node* foo_node = div->firstChild();
  const NGOffsetMapping& result = GetOffsetMapping();

  ASSERT_EQ(1u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 3u, 0u, 3u);

  ASSERT_EQ(1u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 1u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 0));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 1));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForDOMOffset(*foo_node, 2));

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 2)));

  EXPECT_EQ(Position(foo_node, 1), GetFirstPosition(1));
  EXPECT_EQ(Position(foo_node, 1), GetLastPosition(1));
}

TEST_F(NGOffsetMappingTest, FirstLetterWithLeadingSpace) {
  SetupHtml("t",
            "<style>div:first-letter{color:red}</style>"
            "<div id=t>  foo</div>");
  Element* div = GetDocument().getElementById("t");
  const Node* foo_node = div->firstChild();
  const NGOffsetMapping& result = GetOffsetMapping();

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

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 2)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 3)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 4)));

  EXPECT_EQ(Position(foo_node, 0), GetFirstPosition(0));
  EXPECT_EQ(Position(foo_node, 2), GetLastPosition(0));
}

TEST_F(NGOffsetMappingTest, FirstLetterWithoutRemainingText) {
  SetupHtml("t",
            "<style>div:first-letter{color:red}</style>"
            "<div id=t>  f</div>");
  Element* div = GetDocument().getElementById("t");
  const Node* text_node = div->firstChild();
  const NGOffsetMapping& result = GetOffsetMapping();

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

  EXPECT_EQ(0u, *GetTextContentOffset(Position(text_node, 0)));
  EXPECT_EQ(0u, *GetTextContentOffset(Position(text_node, 1)));
  EXPECT_EQ(0u, *GetTextContentOffset(Position(text_node, 2)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(text_node, 3)));

  EXPECT_EQ(Position(text_node, 0), GetFirstPosition(0));
  EXPECT_EQ(Position(text_node, 2), GetLastPosition(0));
}

TEST_F(NGOffsetMappingTest, FirstLetterInDifferentBlock) {
  SetupHtml("t",
            "<style>:first-letter{float:right}</style><div id=t>foo</div>");
  Element* div = GetDocument().getElementById("t");
  const Node* text_node = div->firstChild();

  auto* mapping0 = NGOffsetMapping::GetFor(Position(text_node, 0));
  auto* mapping1 = NGOffsetMapping::GetFor(Position(text_node, 1));
  auto* mapping2 = NGOffsetMapping::GetFor(Position(text_node, 2));
  auto* mapping3 = NGOffsetMapping::GetFor(Position(text_node, 3));

  ASSERT_TRUE(mapping0);
  ASSERT_TRUE(mapping1);
  ASSERT_TRUE(mapping2);
  ASSERT_TRUE(mapping3);

  // GetNGOffsetmappingFor() returns different mappings for offset 0 and other
  // offsets, because first-letter is laid out in a different block.
  EXPECT_NE(mapping0, mapping1);
  EXPECT_EQ(mapping1, mapping2);
  EXPECT_EQ(mapping2, mapping3);

  const NGOffsetMapping& first_letter_result = *mapping0;
  ASSERT_EQ(1u, first_letter_result.GetUnits().size());
  TEST_UNIT(first_letter_result.GetUnits()[0],
            NGOffsetMappingUnitType::kIdentity, text_node, 0u, 1u, 0u, 1u);
  ASSERT_EQ(1u, first_letter_result.GetRanges().size());
  TEST_RANGE(first_letter_result.GetRanges(), text_node, 0u, 1u);

  const NGOffsetMapping& remaining_text_result = *mapping1;
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

  EXPECT_EQ(0u,
            *first_letter_result.GetTextContentOffset(Position(text_node, 0)));
  EXPECT_EQ(
      1u, *remaining_text_result.GetTextContentOffset(Position(text_node, 1)));
  EXPECT_EQ(
      2u, *remaining_text_result.GetTextContentOffset(Position(text_node, 2)));
  EXPECT_EQ(
      3u, *remaining_text_result.GetTextContentOffset(Position(text_node, 3)));

  EXPECT_EQ(Position(text_node, 1), first_letter_result.GetFirstPosition(1));
  EXPECT_EQ(Position(text_node, 1), first_letter_result.GetLastPosition(1));
  EXPECT_EQ(Position(text_node, 1), remaining_text_result.GetFirstPosition(1));
  EXPECT_EQ(Position(text_node, 1), remaining_text_result.GetLastPosition(1));
}

TEST_F(NGOffsetMappingTest, WhiteSpaceTextNodeWithoutLayoutText) {
  SetupHtml("t", "<div id=t> <span>foo</span></div>");
  Element* div = GetDocument().getElementById("t");
  const Node* text_node = div->firstChild();

  EXPECT_FALSE(EndOfLastNonCollapsedCharacter(*text_node, 1u));
  EXPECT_FALSE(StartOfNextNonCollapsedCharacter(*text_node, 0u));
}

}  // namespace blink
