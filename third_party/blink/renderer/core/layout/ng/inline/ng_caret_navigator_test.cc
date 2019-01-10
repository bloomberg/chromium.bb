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

  NGCaretNavigator::VisualCaretMovementResult LeftPositionOf(
      unsigned offset,
      TextAffinity affinity) const {
    return caret_navigator_->LeftPositionOf({offset, affinity});
  }

  NGCaretNavigator::VisualCaretMovementResult RightPositionOf(
      unsigned offset,
      TextAffinity affinity) const {
    return caret_navigator_->RightPositionOf({offset, affinity});
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

  using Position = NGCaretNavigator::Position;

  EXPECT_TRUE(LeftPositionOf(0u, TextAffinity::kDownstream).IsBeforeContext());

  EXPECT_TRUE(LeftPositionOf(1u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({0u, TextAffinity::kDownstream}),
            *LeftPositionOf(1u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(LeftPositionOf(1u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({0u, TextAffinity::kDownstream}),
            *LeftPositionOf(1u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(LeftPositionOf(2u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({1u, TextAffinity::kDownstream}),
            *LeftPositionOf(2u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(LeftPositionOf(2u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({1u, TextAffinity::kDownstream}),
            *LeftPositionOf(2u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(LeftPositionOf(3u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({2u, TextAffinity::kDownstream}),
            *LeftPositionOf(3u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(LeftPositionOf(3u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({4u, TextAffinity::kUpstream}),
            *LeftPositionOf(3u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(LeftPositionOf(4u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({5u, TextAffinity::kUpstream}),
            *LeftPositionOf(4u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(LeftPositionOf(4u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({5u, TextAffinity::kUpstream}),
            *LeftPositionOf(4u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(LeftPositionOf(5u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({6u, TextAffinity::kUpstream}),
            *LeftPositionOf(5u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(LeftPositionOf(5u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({6u, TextAffinity::kUpstream}),
            *LeftPositionOf(5u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(LeftPositionOf(6u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({8u, TextAffinity::kDownstream}),
            *LeftPositionOf(6u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(LeftPositionOf(6u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({2u, TextAffinity::kDownstream}),
            *LeftPositionOf(6u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(LeftPositionOf(7u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({6u, TextAffinity::kDownstream}),
            *LeftPositionOf(7u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(LeftPositionOf(7u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({6u, TextAffinity::kDownstream}),
            *LeftPositionOf(7u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(LeftPositionOf(8u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({7u, TextAffinity::kDownstream}),
            *LeftPositionOf(8u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(LeftPositionOf(8u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({7u, TextAffinity::kDownstream}),
            *LeftPositionOf(8u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(LeftPositionOf(9u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({8u, TextAffinity::kDownstream}),
            *LeftPositionOf(9u, TextAffinity::kUpstream).position);
}

TEST_P(NGCaretNavigatorTest, RightPositionOfBasic) {
  SetupHtml("container",
            "<div id=container>abc&#x05D0;&#x05D1;&#x05D2;123</div>");

  using Position = NGCaretNavigator::Position;

  EXPECT_TRUE(RightPositionOf(0u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({1u, TextAffinity::kUpstream}),
            *RightPositionOf(0u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(RightPositionOf(1u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({2u, TextAffinity::kUpstream}),
            *RightPositionOf(1u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(RightPositionOf(1u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({2u, TextAffinity::kUpstream}),
            *RightPositionOf(1u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(RightPositionOf(2u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({3u, TextAffinity::kUpstream}),
            *RightPositionOf(2u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(RightPositionOf(2u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({3u, TextAffinity::kUpstream}),
            *RightPositionOf(2u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(RightPositionOf(3u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({7u, TextAffinity::kUpstream}),
            *RightPositionOf(3u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(RightPositionOf(3u, TextAffinity::kDownstream).IsAfterContext());

  EXPECT_TRUE(RightPositionOf(4u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({3u, TextAffinity::kDownstream}),
            *RightPositionOf(4u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(RightPositionOf(4u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({3u, TextAffinity::kDownstream}),
            *RightPositionOf(4u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(RightPositionOf(5u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({4u, TextAffinity::kDownstream}),
            *RightPositionOf(5u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(RightPositionOf(5u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({4u, TextAffinity::kDownstream}),
            *RightPositionOf(5u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(RightPositionOf(6u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({5u, TextAffinity::kDownstream}),
            *RightPositionOf(6u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(RightPositionOf(6u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({7u, TextAffinity::kUpstream}),
            *RightPositionOf(6u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(RightPositionOf(7u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({8u, TextAffinity::kUpstream}),
            *RightPositionOf(7u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(RightPositionOf(7u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({8u, TextAffinity::kUpstream}),
            *RightPositionOf(7u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(RightPositionOf(8u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({9u, TextAffinity::kUpstream}),
            *RightPositionOf(8u, TextAffinity::kUpstream).position);

  EXPECT_TRUE(RightPositionOf(8u, TextAffinity::kDownstream).IsWithinContext());
  EXPECT_EQ(Position({9u, TextAffinity::kUpstream}),
            *RightPositionOf(8u, TextAffinity::kDownstream).position);

  EXPECT_TRUE(RightPositionOf(9u, TextAffinity::kUpstream).IsWithinContext());
  EXPECT_EQ(Position({5u, TextAffinity::kDownstream}),
            *RightPositionOf(9u, TextAffinity::kUpstream).position);
}

}  // namespace blink
