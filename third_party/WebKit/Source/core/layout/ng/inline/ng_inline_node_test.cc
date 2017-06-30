// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_node.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/ng/inline/ng_inline_layout_algorithm.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/inline/ng_text_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class NGInlineNodeForTest : public NGInlineNode {
 public:
  using NGInlineNode::NGInlineNode;

  String& Text() { return MutableData().text_content_; }
  Vector<NGInlineItem>& Items() { return MutableData().items_; }

  void Append(const String& text,
              const ComputedStyle* style = nullptr,
              LayoutObject* layout_object = nullptr) {
    unsigned start = Data().text_content_.length();
    MutableData().text_content_.append(text);
    MutableData().items_.push_back(NGInlineItem(NGInlineItem::kText, start,
                                                start + text.length(), style,
                                                layout_object));
  }

  void Append(UChar character) {
    MutableData().text_content_.append(character);
    unsigned end = Data().text_content_.length();
    MutableData().items_.push_back(
        NGInlineItem(NGInlineItem::kBidiControl, end - 1, end, nullptr));
    MutableData().is_bidi_enabled_ = true;
  }

  void ClearText() {
    MutableData().text_content_ = String();
    MutableData().items_.clear();
  }

  void SegmentText() {
    MutableData().is_bidi_enabled_ = true;
    NGInlineNode::SegmentText();
  }

  using NGInlineNode::CollectInlines;
  using NGInlineNode::ShapeText;
};

class NGInlineNodeTest : public RenderingTest {
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

  void UseLayoutObjectAndAhem() {
    // Get Ahem from document. Loading "Ahem.woff" using |createTestFont| fails
    // on linux_chromium_asan_rel_ng.
    LoadAhem();
    SetupHtml("t", "<div id=t style='font:10px Ahem'>test</div>");
  }

  NGInlineNodeForTest CreateInlineNode() {
    if (!layout_block_flow_)
      SetupHtml("t", "<div id=t style='font:10px'>test</div>");
    NGInlineNodeForTest node(layout_block_flow_);
    node.InvalidatePrepareLayout();
    return node;
  }

  void CreateLine(NGInlineNode node,
                  Vector<RefPtr<const NGPhysicalTextFragment>>* fragments_out) {
    RefPtr<NGConstraintSpace> constraint_space =
        NGConstraintSpaceBuilder(kHorizontalTopBottom)
            .ToConstraintSpace(kHorizontalTopBottom);
    RefPtr<NGLayoutResult> result =
        NGInlineLayoutAlgorithm(node, constraint_space.Get()).Layout();

    const NGPhysicalBoxFragment* container =
        ToNGPhysicalBoxFragment(result->PhysicalFragment().Get());
    EXPECT_EQ(container->Children().size(), 1u);
    const NGPhysicalLineBoxFragment* line =
        ToNGPhysicalLineBoxFragment(container->Children()[0].Get());
    for (const auto& child : line->Children()) {
      fragments_out->push_back(ToNGPhysicalTextFragment(child.Get()));
    }
  }

  RefPtr<const ComputedStyle> style_;
  LayoutNGBlockFlow* layout_block_flow_ = nullptr;
  LayoutObject* layout_object_ = nullptr;
  FontCachePurgePreventer purge_preventer_;
};

#define TEST_ITEM_TYPE_OFFSET(item, type, start, end) \
  EXPECT_EQ(NGInlineItem::type, item.Type());         \
  EXPECT_EQ(start, item.StartOffset());               \
  EXPECT_EQ(end, item.EndOffset())

#define TEST_ITEM_TYPE_OFFSET_LEVEL(item, type, start, end, level) \
  EXPECT_EQ(NGInlineItem::type, item.Type());                      \
  EXPECT_EQ(start, item.StartOffset());                            \
  EXPECT_EQ(end, item.EndOffset());                                \
  EXPECT_EQ(level, item.BidiLevel())

#define TEST_ITEM_OFFSET_DIR(item, start, end, direction) \
  EXPECT_EQ(start, item.StartOffset());                   \
  EXPECT_EQ(end, item.EndOffset());                       \
  EXPECT_EQ(direction, item.Direction())

TEST_F(NGInlineNodeTest, CollectInlinesText) {
  SetupHtml("t", "<div id=t>Hello <span>inline</span> world.</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  Vector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 6u);
  TEST_ITEM_TYPE_OFFSET(items[1], kOpenTag, 6u, 6u);
  TEST_ITEM_TYPE_OFFSET(items[2], kText, 6u, 12u);
  TEST_ITEM_TYPE_OFFSET(items[3], kCloseTag, 12u, 12u);
  TEST_ITEM_TYPE_OFFSET(items[4], kText, 12u, 19u);
  EXPECT_EQ(5u, items.size());
}

TEST_F(NGInlineNodeTest, CollectInlinesBR) {
  SetupHtml("t", u"<div id=t>Hello<br>World</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_EQ("Hello\nWorld", node.Text());
  Vector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 5u);
  TEST_ITEM_TYPE_OFFSET(items[1], kControl, 5u, 6u);
  TEST_ITEM_TYPE_OFFSET(items[2], kText, 6u, 11u);
  EXPECT_EQ(3u, items.size());
}

TEST_F(NGInlineNodeTest, CollectInlinesRtlText) {
  SetupHtml("t", u"<div id=t dir=rtl>\u05E2 <span>\u05E2</span> \u05E2</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_TRUE(node.IsBidiEnabled());
  node.SegmentText();
  EXPECT_TRUE(node.IsBidiEnabled());
  Vector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[0], kText, 0u, 2u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[1], kOpenTag, 2u, 2u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[2], kText, 2u, 3u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[3], kCloseTag, 3u, 3u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[4], kText, 3u, 5u, 1u);
  EXPECT_EQ(5u, items.size());
}

TEST_F(NGInlineNodeTest, CollectInlinesMixedText) {
  SetupHtml("t", u"<div id=t>Hello, \u05E2 <span>\u05E2</span></div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_TRUE(node.IsBidiEnabled());
  node.SegmentText();
  EXPECT_TRUE(node.IsBidiEnabled());
  Vector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[0], kText, 0u, 7u, 0u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[1], kText, 7u, 9u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[2], kOpenTag, 9u, 9u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[3], kText, 9u, 10u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[4], kCloseTag, 10u, 10u, 1u);
  EXPECT_EQ(5u, items.size());
}

TEST_F(NGInlineNodeTest, CollectInlinesMixedTextEndWithON) {
  SetupHtml("t", u"<div id=t>Hello, \u05E2 <span>\u05E2!</span></div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_TRUE(node.IsBidiEnabled());
  node.SegmentText();
  EXPECT_TRUE(node.IsBidiEnabled());
  Vector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[0], kText, 0u, 7u, 0u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[1], kText, 7u, 9u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[2], kOpenTag, 9u, 9u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[3], kText, 9u, 10u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[4], kText, 10u, 11u, 0u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[5], kCloseTag, 11u, 11u, 0u);
  EXPECT_EQ(6u, items.size());
}

TEST_F(NGInlineNodeTest, SegmentASCII) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append("Hello");
  node.SegmentText();
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(1u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 5u, TextDirection::kLtr);
}

TEST_F(NGInlineNodeTest, SegmentHebrew) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA");
  node.SegmentText();
  ASSERT_EQ(1u, node.Items().size());
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(1u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 5u, TextDirection::kRtl);
}

TEST_F(NGInlineNodeTest, SegmentSplit1To2) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append(u"Hello \u05E2\u05D1\u05E8\u05D9\u05EA");
  node.SegmentText();
  ASSERT_EQ(2u, node.Items().size());
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(2u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 6u, 11u, TextDirection::kRtl);
}

TEST_F(NGInlineNodeTest, SegmentSplit3To4) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append("Hel");
  node.Append(u"lo \u05E2");
  node.Append(u"\u05D1\u05E8\u05D9\u05EA");
  node.SegmentText();
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(4u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 3u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 3u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[2], 6u, 7u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[3], 7u, 11u, TextDirection::kRtl);
}

TEST_F(NGInlineNodeTest, SegmentBidiOverride) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append("Hello ");
  node.Append(kRightToLeftOverrideCharacter);
  node.Append("ABC");
  node.Append(kPopDirectionalFormattingCharacter);
  node.SegmentText();
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(4u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 6u, 7u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[2], 7u, 10u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[3], 10u, 11u, TextDirection::kLtr);
}

static NGInlineNodeForTest CreateBidiIsolateNode(NGInlineNodeForTest node,
                                                 const ComputedStyle* style,
                                                 LayoutObject* layout_object) {
  node.Append("Hello ", style, layout_object);
  node.Append(kRightToLeftIsolateCharacter);
  node.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA ", style, layout_object);
  node.Append(kLeftToRightIsolateCharacter);
  node.Append("A", style, layout_object);
  node.Append(kPopDirectionalIsolateCharacter);
  node.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA", style, layout_object);
  node.Append(kPopDirectionalIsolateCharacter);
  node.Append(" World", style, layout_object);
  node.SegmentText();
  return node;
}

TEST_F(NGInlineNodeTest, SegmentBidiIsolate) {
  NGInlineNodeForTest node = CreateInlineNode();
  node = CreateBidiIsolateNode(node, style_.Get(), layout_object_);
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(9u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 6u, 7u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[2], 7u, 13u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[3], 13u, 14u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[4], 14u, 15u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[5], 15u, 16u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[6], 16u, 21u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[7], 21u, 22u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[8], 22u, 28u, TextDirection::kLtr);
}

#define TEST_TEXT_FRAGMENT(fragment, node, index, start_offset, end_offset, \
                           dir)                                             \
  EXPECT_EQ(node, fragment->Node());                                        \
  EXPECT_EQ(index, fragment->ItemIndex());                                  \
  EXPECT_EQ(start_offset, fragment->StartOffset());                         \
  EXPECT_EQ(end_offset, fragment->EndOffset());                             \
  EXPECT_EQ(dir, node.Items()[fragment->ItemIndex()].Direction())

TEST_F(NGInlineNodeTest, CreateLineBidiIsolate) {
  UseLayoutObjectAndAhem();
  RefPtr<ComputedStyle> style = ComputedStyle::Create();
  style->SetLineHeight(Length(1, kFixed));
  style->GetFont().Update(nullptr);
  NGInlineNodeForTest node = CreateInlineNode();
  node = CreateBidiIsolateNode(node, style.Get(), layout_object_);
  node.ShapeText();
  Vector<RefPtr<const NGPhysicalTextFragment>> fragments;
  CreateLine(node, &fragments);
  ASSERT_EQ(5u, fragments.size());
  TEST_TEXT_FRAGMENT(fragments[0], node, 0u, 0u, 6u, TextDirection::kLtr);
  TEST_TEXT_FRAGMENT(fragments[1], node, 6u, 16u, 21u, TextDirection::kRtl);
  TEST_TEXT_FRAGMENT(fragments[2], node, 4u, 14u, 15u, TextDirection::kLtr);
  TEST_TEXT_FRAGMENT(fragments[3], node, 2u, 7u, 13u, TextDirection::kRtl);
  TEST_TEXT_FRAGMENT(fragments[4], node, 8u, 22u, 28u, TextDirection::kLtr);
}

TEST_F(NGInlineNodeTest, MinMaxContentSize) {
  LoadAhem();
  SetupHtml("t", "<div id=t style='font:10px Ahem'>AB CDEF</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  MinMaxContentSize sizes = node.ComputeMinMaxContentSize();
  EXPECT_EQ(40, sizes.min_content);
  EXPECT_EQ(70, sizes.max_content);
}

TEST_F(NGInlineNodeTest, MinMaxContentSizeElementBoundary) {
  LoadAhem();
  SetupHtml("t", "<div id=t style='font:10px Ahem'>A B<span>C D</span></div>");
  NGInlineNodeForTest node = CreateInlineNode();
  MinMaxContentSize sizes = node.ComputeMinMaxContentSize();
  // |min_content| should be the width of "BC" because there is an element
  // boundary between "B" and "C" but no break opportunities.
  EXPECT_EQ(20, sizes.min_content);
  EXPECT_EQ(60, sizes.max_content);
}

}  // namespace blink
