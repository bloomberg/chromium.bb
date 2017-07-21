// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_node.h"

#include "core/dom/FirstLetterPseudoElement.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/ng/inline/ng_offset_mapping_builder.h"
#include "core/layout/ng/inline/ng_offset_mapping_result.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

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

  const NGOffsetMappingResult& GetOffsetMapping() {
    return NGInlineNode(layout_block_flow_).ComputeOffsetMappingIfNeeded();
  }

  bool IsOffsetMappingStored() const {
    return layout_block_flow_->GetNGInlineNodeData().offset_mapping_.get();
  }

  const LayoutText* GetLayoutTextUnder(const char* parent_id) {
    Element* parent = GetDocument().getElementById(parent_id);
    return ToLayoutText(parent->firstChild()->GetLayoutObject());
  }

  RefPtr<const ComputedStyle> style_;
  LayoutNGBlockFlow* layout_block_flow_ = nullptr;
  LayoutObject* layout_object_ = nullptr;
  FontCachePurgePreventer purge_preventer_;
};

#define TEST_UNIT(unit, _type, _owner, domstart, domend, textcontentstart, \
                  textcontentend)                                          \
  EXPECT_EQ(_type, unit.type);                                             \
  EXPECT_EQ(_owner, unit.owner);                                           \
  EXPECT_EQ(domstart, unit.dom_start);                                     \
  EXPECT_EQ(domend, unit.dom_end);                                         \
  EXPECT_EQ(textcontentstart, unit.text_content_start);                    \
  EXPECT_EQ(textcontentend, unit.text_content_end)

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

TEST_F(NGInlineNodeOffsetMappingTest, OneTextNode) {
  SetupHtml("t", "<div id=t>foo</div>");
  const NGOffsetMappingResult& result = GetOffsetMapping();

  ASSERT_EQ(1u, result.units.size());
  TEST_UNIT(result.units[0], NGOffsetMappingUnitType::kIdentity, layout_object_,
            0u, 3u, 0u, 3u);

  ASSERT_EQ(1u, result.ranges.size());
  TEST_RANGE(result.ranges, ToLayoutText(layout_object_), 0u, 1u);
}

TEST_F(NGInlineNodeOffsetMappingTest, TwoTextNodes) {
  SetupHtml("t", "<div id=t>foo<span id=s>bar</span></div>");
  const LayoutText* foo = ToLayoutText(layout_object_);
  const LayoutText* bar = GetLayoutTextUnder("s");
  const NGOffsetMappingResult& result = GetOffsetMapping();

  ASSERT_EQ(2u, result.units.size());
  TEST_UNIT(result.units[0], NGOffsetMappingUnitType::kIdentity, foo, 0u, 3u,
            0u, 3u);
  TEST_UNIT(result.units[1], NGOffsetMappingUnitType::kIdentity, bar, 0u, 3u,
            3u, 6u);

  ASSERT_EQ(2u, result.ranges.size());
  TEST_RANGE(result.ranges, foo, 0u, 1u);
  TEST_RANGE(result.ranges, bar, 1u, 2u);
}

TEST_F(NGInlineNodeOffsetMappingTest, BRBetweenTextNodes) {
  SetupHtml("t", u"<div id=t>foo<br>bar</div>");
  const LayoutText* foo = ToLayoutText(layout_object_);
  const LayoutText* br = ToLayoutText(foo->NextSibling());
  const LayoutText* bar = ToLayoutText(br->NextSibling());
  const NGOffsetMappingResult& result = GetOffsetMapping();

  ASSERT_EQ(3u, result.units.size());
  TEST_UNIT(result.units[0], NGOffsetMappingUnitType::kIdentity, foo, 0u, 3u,
            0u, 3u);
  TEST_UNIT(result.units[1], NGOffsetMappingUnitType::kIdentity, br, 0u, 1u, 3u,
            4u);
  TEST_UNIT(result.units[2], NGOffsetMappingUnitType::kIdentity, bar, 0u, 3u,
            4u, 7u);

  ASSERT_EQ(3u, result.ranges.size());
  TEST_RANGE(result.ranges, foo, 0u, 1u);
  TEST_RANGE(result.ranges, br, 1u, 2u);
  TEST_RANGE(result.ranges, bar, 2u, 3u);
}

TEST_F(NGInlineNodeOffsetMappingTest, OneTextNodeWithCollapsedSpace) {
  SetupHtml("t", "<div id=t>foo  bar</div>");
  const NGOffsetMappingResult& result = GetOffsetMapping();

  ASSERT_EQ(3u, result.units.size());
  TEST_UNIT(result.units[0], NGOffsetMappingUnitType::kIdentity, layout_object_,
            0u, 4u, 0u, 4u);
  TEST_UNIT(result.units[1], NGOffsetMappingUnitType::kCollapsed,
            layout_object_, 4u, 5u, 4u, 4u);
  TEST_UNIT(result.units[2], NGOffsetMappingUnitType::kIdentity, layout_object_,
            5u, 8u, 4u, 7u);

  ASSERT_EQ(1u, result.ranges.size());
  TEST_RANGE(result.ranges, ToLayoutText(layout_object_), 0u, 3u);
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
  const NGOffsetMappingResult& result = GetOffsetMapping();

  ASSERT_EQ(3u, result.units.size());
  TEST_UNIT(result.units[0], NGOffsetMappingUnitType::kIdentity, foo, 0u, 4u,
            0u, 4u);
  TEST_UNIT(result.units[1], NGOffsetMappingUnitType::kCollapsed, space, 0u, 1u,
            4u, 4u);
  TEST_UNIT(result.units[2], NGOffsetMappingUnitType::kIdentity, bar, 0u, 3u,
            4u, 7u);

  ASSERT_EQ(3u, result.ranges.size());
  TEST_RANGE(result.ranges, foo, 0u, 1u);
  TEST_RANGE(result.ranges, space, 1u, 2u);
  TEST_RANGE(result.ranges, bar, 2u, 3u);
}

TEST_F(NGInlineNodeOffsetMappingTest, ReplacedElement) {
  SetupHtml("t", "<div id=t>foo <img> bar</div>");
  const LayoutText* foo = ToLayoutText(layout_object_);
  const LayoutText* bar = ToLayoutText(foo->NextSibling()->NextSibling());
  const NGOffsetMappingResult& result = GetOffsetMapping();

  ASSERT_EQ(2u, result.units.size());
  TEST_UNIT(result.units[0], NGOffsetMappingUnitType::kIdentity, foo, 0u, 4u,
            0u, 4u);
  TEST_UNIT(result.units[1], NGOffsetMappingUnitType::kIdentity, bar, 0u, 4u,
            5u, 9u);

  ASSERT_EQ(2u, result.ranges.size());
  TEST_RANGE(result.ranges, foo, 0u, 1u);
  TEST_RANGE(result.ranges, bar, 1u, 2u);
}

TEST_F(NGInlineNodeOffsetMappingTest, FirstLetter) {
  SetupHtml("t",
            "<style>div:first-letter{color:red}</style>"
            "<div id=t>foo</div>");
  Element* div = GetDocument().getElementById("t");
  const LayoutText* remaining_text =
      ToLayoutText(div->firstChild()->GetLayoutObject());
  const LayoutText* first_letter =
      ToLayoutText(ToLayoutTextFragment(remaining_text)
                       ->GetFirstLetterPseudoElement()
                       ->GetLayoutObject()
                       ->SlowFirstChild());
  const NGOffsetMappingResult& result = GetOffsetMapping();

  ASSERT_EQ(2u, result.units.size());
  TEST_UNIT(result.units[0], NGOffsetMappingUnitType::kIdentity, first_letter,
            0u, 1u, 0u, 1u);
  TEST_UNIT(result.units[1], NGOffsetMappingUnitType::kIdentity, remaining_text,
            1u, 3u, 1u, 3u);

  ASSERT_EQ(2u, result.ranges.size());
  TEST_RANGE(result.ranges, first_letter, 0u, 1u);
  TEST_RANGE(result.ranges, remaining_text, 1u, 2u);
}

TEST_F(NGInlineNodeOffsetMappingTest, FirstLetterWithLeadingSpace) {
  SetupHtml("t",
            "<style>div:first-letter{color:red}</style>"
            "<div id=t>  foo</div>");
  Element* div = GetDocument().getElementById("t");
  const LayoutText* remaining_text =
      ToLayoutText(div->firstChild()->GetLayoutObject());
  const LayoutText* first_letter =
      ToLayoutText(ToLayoutTextFragment(remaining_text)
                       ->GetFirstLetterPseudoElement()
                       ->GetLayoutObject()
                       ->SlowFirstChild());
  const NGOffsetMappingResult& result = GetOffsetMapping();

  ASSERT_EQ(3u, result.units.size());
  TEST_UNIT(result.units[0], NGOffsetMappingUnitType::kCollapsed, first_letter,
            0u, 2u, 0u, 0u);
  TEST_UNIT(result.units[1], NGOffsetMappingUnitType::kIdentity, first_letter,
            2u, 3u, 0u, 1u);
  TEST_UNIT(result.units[2], NGOffsetMappingUnitType::kIdentity, remaining_text,
            3u, 5u, 1u, 3u);

  ASSERT_EQ(2u, result.ranges.size());
  TEST_RANGE(result.ranges, first_letter, 0u, 2u);
  TEST_RANGE(result.ranges, remaining_text, 2u, 3u);
}

}  // namespace blink
