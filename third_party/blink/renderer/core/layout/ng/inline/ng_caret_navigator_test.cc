// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_navigator.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

class NGCaretNavigatorTest : public RenderingTest,
                             public testing::WithParamInterface<bool>,
                             private ScopedLayoutNGForTest,
                             private ScopedBidiCaretAffinityForTest {
 public:
  NGCaretNavigatorTest()
      : ScopedLayoutNGForTest(GetParam()),
        ScopedBidiCaretAffinityForTest(true) {}

  void SetupHtml(const char* id, String html) {
    SetBodyInnerHTML(html);

    LayoutBlockFlow* layout_block_flow =
        ToLayoutBlockFlow(GetLayoutObjectByElementId(id));
    DCHECK(layout_block_flow);
    DCHECK(layout_block_flow->ChildrenInline());
    DCHECK_EQ(layout_block_flow->IsLayoutNGMixin(), GetParam());

    const NGOffsetMapping* mapping =
        NGInlineNode::GetOffsetMapping(layout_block_flow, &mapping_storage_);
    DCHECK(mapping);
    DCHECK_EQ(!mapping_storage_,
              GetParam());  // |storage| is used only for legacy.

    caret_navigator_ = mapping->GetCaretNavigator();
    DCHECK(caret_navigator_);
  }

  UBiDiLevel BidiLevelAt(unsigned index) const {
    return caret_navigator_->BidiLevelAt(index);
  }

  NGCaretNavigator::VisualCharacterMovementResult LeftCharacterOf(
      unsigned index) const {
    return caret_navigator_->LeftCharacterOf(index);
  }

  NGCaretNavigator::VisualCharacterMovementResult RightCharacterOf(
      unsigned index) const {
    return caret_navigator_->RightCharacterOf(index);
  }

  NGCaretNavigator::Position CaretBefore(unsigned index) const {
    return {index, NGCaretNavigator::PositionAnchorType::kBefore};
  }

  NGCaretNavigator::Position CaretAfter(unsigned index) const {
    return {index, NGCaretNavigator::PositionAnchorType::kAfter};
  }

  NGCaretNavigator::VisualCaretMovementResult LeftPositionOf(
      const NGCaretNavigator::Position& position) const {
    return caret_navigator_->LeftPositionOf(position);
  }

  NGCaretNavigator::VisualCaretMovementResult RightPositionOf(
      const NGCaretNavigator::Position& position) const {
    return caret_navigator_->RightPositionOf(position);
  }

 protected:
  std::unique_ptr<NGOffsetMapping> mapping_storage_;
  const NGCaretNavigator* caret_navigator_;
};

INSTANTIATE_TEST_CASE_P(All, NGCaretNavigatorTest, testing::Bool());

TEST_P(NGCaretNavigatorTest, BidiLevelAtBasic) {
  SetupHtml("container",
            "<div id=container>abc&#x05D0;&#x05D1;&#x05D2;123</div>");

  EXPECT_EQ(0u, BidiLevelAt(0));
  EXPECT_EQ(0u, BidiLevelAt(1));
  EXPECT_EQ(0u, BidiLevelAt(2));
  EXPECT_EQ(1u, BidiLevelAt(3));
  EXPECT_EQ(1u, BidiLevelAt(4));
  EXPECT_EQ(1u, BidiLevelAt(5));
  EXPECT_EQ(2u, BidiLevelAt(6));
  EXPECT_EQ(2u, BidiLevelAt(7));
  EXPECT_EQ(2u, BidiLevelAt(8));
}

TEST_P(NGCaretNavigatorTest, LeftCharacterOfBasic) {
  SetupHtml("container",
            "<div id=container>abc&#x05D0;&#x05D1;&#x05D2;123</div>");

  EXPECT_TRUE(LeftCharacterOf(0).IsBeforeContext());

  EXPECT_TRUE(LeftCharacterOf(1).IsWithinContext());
  EXPECT_EQ(0u, *LeftCharacterOf(1).index);

  EXPECT_TRUE(LeftCharacterOf(2).IsWithinContext());
  EXPECT_EQ(1u, *LeftCharacterOf(2).index);

  EXPECT_TRUE(LeftCharacterOf(3).IsWithinContext());
  EXPECT_EQ(4u, *LeftCharacterOf(3).index);

  EXPECT_TRUE(LeftCharacterOf(4).IsWithinContext());
  EXPECT_EQ(5u, *LeftCharacterOf(4).index);

  EXPECT_TRUE(LeftCharacterOf(5).IsWithinContext());
  EXPECT_EQ(8u, *LeftCharacterOf(5).index);

  EXPECT_TRUE(LeftCharacterOf(6).IsWithinContext());
  EXPECT_EQ(2u, *LeftCharacterOf(6).index);

  EXPECT_TRUE(LeftCharacterOf(7).IsWithinContext());
  EXPECT_EQ(6u, *LeftCharacterOf(7).index);

  EXPECT_TRUE(LeftCharacterOf(8).IsWithinContext());
  EXPECT_EQ(7u, *LeftCharacterOf(8).index);
}

TEST_P(NGCaretNavigatorTest, RightCharacterOfBasic) {
  SetupHtml("container",
            "<div id=container>abc&#x05D0;&#x05D1;&#x05D2;123</div>");

  EXPECT_TRUE(RightCharacterOf(0).IsWithinContext());
  EXPECT_EQ(1u, *RightCharacterOf(0).index);

  EXPECT_TRUE(RightCharacterOf(1).IsWithinContext());
  EXPECT_EQ(2u, *RightCharacterOf(1).index);

  EXPECT_TRUE(RightCharacterOf(2).IsWithinContext());
  EXPECT_EQ(6u, *RightCharacterOf(2).index);

  EXPECT_TRUE(RightCharacterOf(3).IsAfterContext());

  EXPECT_TRUE(RightCharacterOf(4).IsWithinContext());
  EXPECT_EQ(3u, *RightCharacterOf(4).index);

  EXPECT_TRUE(RightCharacterOf(5).IsWithinContext());
  EXPECT_EQ(4u, *RightCharacterOf(5).index);

  EXPECT_TRUE(RightCharacterOf(6).IsWithinContext());
  EXPECT_EQ(7u, *RightCharacterOf(6).index);

  EXPECT_TRUE(RightCharacterOf(7).IsWithinContext());
  EXPECT_EQ(8u, *RightCharacterOf(7).index);

  EXPECT_TRUE(RightCharacterOf(8).IsWithinContext());
  EXPECT_EQ(5u, *RightCharacterOf(8).index);
}

TEST_P(NGCaretNavigatorTest, LeftPositionOfBasic) {
  SetupHtml("container",
            "<div id=container>abc&#x05D0;&#x05D1;&#x05D2;123</div>");

  EXPECT_TRUE(LeftPositionOf(CaretBefore(0)).IsBeforeContext());

  EXPECT_TRUE(LeftPositionOf(CaretAfter(0)).IsWithinContext());
  EXPECT_EQ(CaretBefore(0), *LeftPositionOf(CaretAfter(0)).position);

  EXPECT_TRUE(LeftPositionOf(CaretBefore(1)).IsWithinContext());
  EXPECT_EQ(CaretBefore(0), *LeftPositionOf(CaretBefore(1)).position);

  EXPECT_TRUE(LeftPositionOf(CaretAfter(1)).IsWithinContext());
  EXPECT_EQ(CaretBefore(1), *LeftPositionOf(CaretAfter(1)).position);

  EXPECT_TRUE(LeftPositionOf(CaretBefore(2)).IsWithinContext());
  EXPECT_EQ(CaretBefore(1), *LeftPositionOf(CaretBefore(2)).position);

  EXPECT_TRUE(LeftPositionOf(CaretAfter(2)).IsWithinContext());
  EXPECT_EQ(CaretBefore(2), *LeftPositionOf(CaretAfter(2)).position);

  EXPECT_TRUE(LeftPositionOf(CaretBefore(3)).IsWithinContext());
  EXPECT_EQ(CaretAfter(3), *LeftPositionOf(CaretBefore(3)).position);

  EXPECT_TRUE(LeftPositionOf(CaretAfter(3)).IsWithinContext());
  EXPECT_EQ(CaretAfter(4), *LeftPositionOf(CaretAfter(3)).position);

  EXPECT_TRUE(LeftPositionOf(CaretBefore(4)).IsWithinContext());
  EXPECT_EQ(CaretAfter(4), *LeftPositionOf(CaretBefore(4)).position);

  EXPECT_TRUE(LeftPositionOf(CaretAfter(4)).IsWithinContext());
  EXPECT_EQ(CaretAfter(5), *LeftPositionOf(CaretAfter(4)).position);

  EXPECT_TRUE(LeftPositionOf(CaretBefore(5)).IsWithinContext());
  EXPECT_EQ(CaretAfter(5), *LeftPositionOf(CaretBefore(5)).position);

  EXPECT_TRUE(LeftPositionOf(CaretAfter(5)).IsWithinContext());
  EXPECT_EQ(CaretBefore(8), *LeftPositionOf(CaretAfter(5)).position);

  EXPECT_TRUE(LeftPositionOf(CaretBefore(6)).IsWithinContext());
  EXPECT_EQ(CaretBefore(2), *LeftPositionOf(CaretBefore(6)).position);

  EXPECT_TRUE(LeftPositionOf(CaretAfter(6)).IsWithinContext());
  EXPECT_EQ(CaretBefore(6), *LeftPositionOf(CaretAfter(6)).position);

  EXPECT_TRUE(LeftPositionOf(CaretBefore(7)).IsWithinContext());
  EXPECT_EQ(CaretBefore(6), *LeftPositionOf(CaretBefore(7)).position);

  EXPECT_TRUE(LeftPositionOf(CaretAfter(7)).IsWithinContext());
  EXPECT_EQ(CaretBefore(7), *LeftPositionOf(CaretAfter(7)).position);

  EXPECT_TRUE(LeftPositionOf(CaretBefore(8)).IsWithinContext());
  EXPECT_EQ(CaretBefore(7), *LeftPositionOf(CaretBefore(8)).position);

  EXPECT_TRUE(LeftPositionOf(CaretAfter(8)).IsWithinContext());
  EXPECT_EQ(CaretBefore(8), *LeftPositionOf(CaretAfter(8)).position);
}

TEST_P(NGCaretNavigatorTest, RightPositionOfBasic) {
  SetupHtml("container",
            "<div id=container>abc&#x05D0;&#x05D1;&#x05D2;123</div>");

  EXPECT_TRUE(RightPositionOf(CaretBefore(0)).IsWithinContext());
  EXPECT_EQ(CaretAfter(0), *RightPositionOf(CaretBefore(0)).position);

  EXPECT_TRUE(RightPositionOf(CaretAfter(0)).IsWithinContext());
  EXPECT_EQ(CaretAfter(1), *RightPositionOf(CaretAfter(0)).position);

  EXPECT_TRUE(RightPositionOf(CaretBefore(1)).IsWithinContext());
  EXPECT_EQ(CaretAfter(1), *RightPositionOf(CaretBefore(1)).position);

  EXPECT_TRUE(RightPositionOf(CaretAfter(1)).IsWithinContext());
  EXPECT_EQ(CaretAfter(2), *RightPositionOf(CaretAfter(1)).position);

  EXPECT_TRUE(RightPositionOf(CaretBefore(2)).IsWithinContext());
  EXPECT_EQ(CaretAfter(2), *RightPositionOf(CaretBefore(2)).position);

  EXPECT_TRUE(RightPositionOf(CaretAfter(2)).IsWithinContext());
  EXPECT_EQ(CaretAfter(6), *RightPositionOf(CaretAfter(2)).position);

  EXPECT_TRUE(RightPositionOf(CaretBefore(3)).IsAfterContext());

  EXPECT_TRUE(RightPositionOf(CaretAfter(3)).IsWithinContext());
  EXPECT_EQ(CaretBefore(3), *RightPositionOf(CaretAfter(3)).position);

  EXPECT_TRUE(RightPositionOf(CaretBefore(4)).IsWithinContext());
  EXPECT_EQ(CaretBefore(3), *RightPositionOf(CaretBefore(4)).position);

  EXPECT_TRUE(RightPositionOf(CaretAfter(4)).IsWithinContext());
  EXPECT_EQ(CaretBefore(4), *RightPositionOf(CaretAfter(4)).position);

  EXPECT_TRUE(RightPositionOf(CaretBefore(5)).IsWithinContext());
  EXPECT_EQ(CaretBefore(4), *RightPositionOf(CaretBefore(5)).position);

  EXPECT_TRUE(RightPositionOf(CaretAfter(5)).IsWithinContext());
  EXPECT_EQ(CaretBefore(5), *RightPositionOf(CaretAfter(5)).position);

  EXPECT_TRUE(RightPositionOf(CaretBefore(6)).IsWithinContext());
  EXPECT_EQ(CaretAfter(6), *RightPositionOf(CaretBefore(6)).position);

  EXPECT_TRUE(RightPositionOf(CaretAfter(6)).IsWithinContext());
  EXPECT_EQ(CaretAfter(7), *RightPositionOf(CaretAfter(6)).position);

  EXPECT_TRUE(RightPositionOf(CaretBefore(7)).IsWithinContext());
  EXPECT_EQ(CaretAfter(7), *RightPositionOf(CaretBefore(7)).position);

  EXPECT_TRUE(RightPositionOf(CaretAfter(7)).IsWithinContext());
  EXPECT_EQ(CaretAfter(8), *RightPositionOf(CaretAfter(7)).position);

  EXPECT_TRUE(RightPositionOf(CaretBefore(8)).IsWithinContext());
  EXPECT_EQ(CaretAfter(8), *RightPositionOf(CaretBefore(8)).position);

  EXPECT_TRUE(RightPositionOf(CaretAfter(8)).IsWithinContext());
  EXPECT_EQ(CaretBefore(5), *RightPositionOf(CaretAfter(8)).position);
}

}  // namespace blink
