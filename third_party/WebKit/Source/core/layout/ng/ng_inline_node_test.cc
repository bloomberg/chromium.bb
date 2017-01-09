// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_inline_node.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_text_fragment.h"
#include "core/layout/ng/ng_text_layout_algorithm.h"
#include "core/layout/ng/ng_line_builder.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class NGInlineNodeForTest : public NGInlineNode {
 public:
  NGInlineNodeForTest(const ComputedStyle* block_style) {
    block_style_ = const_cast<ComputedStyle*>(block_style);
  }
  using NGInlineNode::NGInlineNode;

  String& Text() { return text_content_; }
  Vector<NGLayoutInlineItem>& Items() { return items_; }

  void Append(const String& text, const ComputedStyle* style = nullptr) {
    unsigned start = text_content_.length();
    text_content_.append(text);
    items_.push_back(NGLayoutInlineItem(start, start + text.length(), style));
  }

  void Append(UChar character) {
    text_content_.append(character);
    unsigned end = text_content_.length();
    items_.push_back(NGLayoutInlineItem(end - 1, end, nullptr));
    is_bidi_enabled_ = true;
  }

  void ClearText() {
    text_content_ = String();
    items_.clear();
  }

  void SegmentText() {
    is_bidi_enabled_ = true;
    NGInlineNode::SegmentText();
  }
};

class NGInlineNodeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    style_ = ComputedStyle::create();
    style_->font().update(nullptr);
  }

  void CreateLine(
      NGInlineNode* node,
      HeapVector<Member<const NGPhysicalTextFragment>>* fragments_out) {
    NGConstraintSpace* constraint_space =
        NGConstraintSpaceBuilder(kHorizontalTopBottom).ToConstraintSpace();
    NGLineBuilder line_builder(node, constraint_space);
    NGTextLayoutAlgorithm* algorithm =
        new NGTextLayoutAlgorithm(node, constraint_space);
    algorithm->LayoutInline(&line_builder);

    NGFragmentBuilder fragment_builder(NGPhysicalFragment::kFragmentBox);
    line_builder.CreateFragments(&fragment_builder);
    NGPhysicalBoxFragment* fragment = fragment_builder.ToBoxFragment();
    for (const NGPhysicalFragment* child : fragment->Children()) {
      fragments_out->push_back(toNGPhysicalTextFragment(child));
    }
  }

  RefPtr<ComputedStyle> style_;
};

#define TEST_ITEM_OFFSET_DIR(item, start, end, direction) \
  EXPECT_EQ(start, item.StartOffset());                   \
  EXPECT_EQ(end, item.EndOffset());                       \
  EXPECT_EQ(direction, item.Direction())

TEST_F(NGInlineNodeTest, SegmentASCII) {
  NGInlineNodeForTest* node = new NGInlineNodeForTest(style_.get());
  node->Append("Hello");
  node->SegmentText();
  Vector<NGLayoutInlineItem>& items = node->Items();
  ASSERT_EQ(1u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 5u, TextDirection::kLtr);
}

TEST_F(NGInlineNodeTest, SegmentHebrew) {
  NGInlineNodeForTest* node = new NGInlineNodeForTest(style_.get());
  node->Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA");
  node->SegmentText();
  ASSERT_EQ(1u, node->Items().size());
  Vector<NGLayoutInlineItem>& items = node->Items();
  ASSERT_EQ(1u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 5u, TextDirection::kRtl);
}

TEST_F(NGInlineNodeTest, SegmentSplit1To2) {
  NGInlineNodeForTest* node = new NGInlineNodeForTest(style_.get());
  node->Append(u"Hello \u05E2\u05D1\u05E8\u05D9\u05EA");
  node->SegmentText();
  ASSERT_EQ(2u, node->Items().size());
  Vector<NGLayoutInlineItem>& items = node->Items();
  ASSERT_EQ(2u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 6u, 11u, TextDirection::kRtl);
}

TEST_F(NGInlineNodeTest, SegmentSplit3To4) {
  NGInlineNodeForTest* node = new NGInlineNodeForTest(style_.get());
  node->Append("Hel");
  node->Append(u"lo \u05E2");
  node->Append(u"\u05D1\u05E8\u05D9\u05EA");
  node->SegmentText();
  Vector<NGLayoutInlineItem>& items = node->Items();
  ASSERT_EQ(4u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 3u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 3u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[2], 6u, 7u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[3], 7u, 11u, TextDirection::kRtl);
}

TEST_F(NGInlineNodeTest, SegmentBidiOverride) {
  NGInlineNodeForTest* node = new NGInlineNodeForTest(style_.get());
  node->Append("Hello ");
  node->Append(rightToLeftOverrideCharacter);
  node->Append("ABC");
  node->Append(popDirectionalFormattingCharacter);
  node->SegmentText();
  Vector<NGLayoutInlineItem>& items = node->Items();
  ASSERT_EQ(4u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 6u, 7u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[2], 7u, 10u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[3], 10u, 11u, TextDirection::kLtr);
}

static NGInlineNodeForTest* CreateBidiIsolateNode(const ComputedStyle* style) {
  NGInlineNodeForTest* node = new NGInlineNodeForTest(style);
  node->Append("Hello ", style);
  node->Append(rightToLeftIsolateCharacter);
  node->Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA ", style);
  node->Append(leftToRightIsolateCharacter);
  node->Append("A", style);
  node->Append(popDirectionalIsolateCharacter);
  node->Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA", style);
  node->Append(popDirectionalIsolateCharacter);
  node->Append(" World", style);
  node->SegmentText();
  return node;
}

TEST_F(NGInlineNodeTest, SegmentBidiIsolate) {
  NGInlineNodeForTest* node = CreateBidiIsolateNode(style_.get());
  Vector<NGLayoutInlineItem>& items = node->Items();
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

#define TEST_TEXT_FRAGMENT(fragment, node, start_index, end_index, dir) \
  EXPECT_EQ(node, fragment->Node());                                    \
  EXPECT_EQ(start_index, fragment->StartIndex());                       \
  EXPECT_EQ(end_index, fragment->EndIndex());                           \
  EXPECT_EQ(dir, node->Items()[fragment->StartIndex()].Direction())

TEST_F(NGInlineNodeTest, CreateLineBidiIsolate) {
  NGInlineNodeForTest* node = CreateBidiIsolateNode(style_.get());
  HeapVector<Member<const NGPhysicalTextFragment>> fragments;
  CreateLine(node, &fragments);
  ASSERT_EQ(5u, fragments.size());
  TEST_TEXT_FRAGMENT(fragments[0], node, 0u, 1u, TextDirection::kLtr);
  TEST_TEXT_FRAGMENT(fragments[1], node, 6u, 7u, TextDirection::kRtl);
  TEST_TEXT_FRAGMENT(fragments[2], node, 4u, 5u, TextDirection::kLtr);
  TEST_TEXT_FRAGMENT(fragments[3], node, 2u, 3u, TextDirection::kRtl);
  TEST_TEXT_FRAGMENT(fragments[4], node, 8u, 9u, TextDirection::kLtr);
}

}  // namespace blink
