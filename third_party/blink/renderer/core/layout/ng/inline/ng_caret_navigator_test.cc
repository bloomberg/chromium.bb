// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_navigator.h"

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

class NGCaretNavigatorTest : public RenderingTest,
                             private ScopedBidiCaretAffinityForTest {
 public:
  NGCaretNavigatorTest() : ScopedBidiCaretAffinityForTest(true) {}

  void SetupHtml(const char* id, String html) {
    SetBodyInnerHTML(html);

    block_flow_ = ToLayoutBlockFlow(GetLayoutObjectByElementId(id));
    DCHECK(block_flow_);
    DCHECK(block_flow_->IsLayoutNGMixin());
    DCHECK(block_flow_->ChildrenInline());
  }

  UBiDiLevel BidiLevelAt(unsigned index) const {
    return NGCaretNavigator(*block_flow_).BidiLevelAt(index);
  }

  NGCaretNavigator::VisualCharacterMovementResult LeftCharacterOf(
      unsigned index) const {
    return NGCaretNavigator(*block_flow_).LeftCharacterOf(index);
  }

  NGCaretNavigator::VisualCharacterMovementResult RightCharacterOf(
      unsigned index) const {
    return NGCaretNavigator(*block_flow_).RightCharacterOf(index);
  }

  NGCaretNavigator::Position CaretBefore(unsigned index) const {
    return {index, NGCaretNavigator::PositionAnchorType::kBefore};
  }

  NGCaretNavigator::Position CaretAfter(unsigned index) const {
    return {index, NGCaretNavigator::PositionAnchorType::kAfter};
  }

  NGCaretNavigator::VisualCaretMovementResult LeftPositionOf(
      const NGCaretNavigator::Position& position) const {
    return NGCaretNavigator(*block_flow_).LeftPositionOf(position);
  }

  NGCaretNavigator::VisualCaretMovementResult RightPositionOf(
      const NGCaretNavigator::Position& position) const {
    return NGCaretNavigator(*block_flow_).RightPositionOf(position);
  }

 protected:
  const LayoutBlockFlow* block_flow_;
};

TEST_F(NGCaretNavigatorTest, BidiLevelAtBasic) {
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

TEST_F(NGCaretNavigatorTest, LeftCharacterOfBasic) {
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

TEST_F(NGCaretNavigatorTest, RightCharacterOfBasic) {
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

TEST_F(NGCaretNavigatorTest, LeftPositionOfBasic) {
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

TEST_F(NGCaretNavigatorTest, RightPositionOfBasic) {
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

// Tests below check caret movement crossing line boundaries

TEST_F(NGCaretNavigatorTest, HardLineBreak) {
  SetupHtml("container", "<div id=container>abc<br>def</div>");

  EXPECT_TRUE(LeftPositionOf(CaretBefore(0)).IsBeforeContext());

  EXPECT_TRUE(RightPositionOf(CaretAfter(2)).IsWithinContext());
  EXPECT_EQ(CaretBefore(4), *RightPositionOf(CaretAfter(2)).position);

  EXPECT_TRUE(RightPositionOf(CaretBefore(3)).IsWithinContext());
  EXPECT_EQ(CaretBefore(4), *RightPositionOf(CaretBefore(3)).position);

  EXPECT_TRUE(LeftPositionOf(CaretBefore(4)).IsWithinContext());
  EXPECT_EQ(CaretBefore(3), *LeftPositionOf(CaretBefore(4)).position);

  EXPECT_TRUE(RightPositionOf(CaretAfter(6)).IsAfterContext());
}

TEST_F(NGCaretNavigatorTest, SoftLineWrapAtSpace) {
  SetupHtml("container", "<div id=container style=\"width:0\">abc def</div>");

  EXPECT_TRUE(LeftPositionOf(CaretBefore(0)).IsBeforeContext());

  EXPECT_TRUE(RightPositionOf(CaretAfter(2)).IsWithinContext());
  EXPECT_EQ(CaretBefore(4), *RightPositionOf(CaretAfter(2)).position);

  EXPECT_TRUE(RightPositionOf(CaretBefore(3)).IsWithinContext());
  EXPECT_EQ(CaretBefore(4), *RightPositionOf(CaretBefore(3)).position);

  EXPECT_TRUE(LeftPositionOf(CaretBefore(4)).IsWithinContext());
  EXPECT_EQ(CaretAfter(2), *LeftPositionOf(CaretBefore(4)).position);

  EXPECT_TRUE(RightPositionOf(CaretAfter(6)).IsAfterContext());
}

TEST_F(NGCaretNavigatorTest, BidiAndSoftLineWrapAtSpaceLtr) {
  LoadAhem();
  SetupHtml("container",
            "<div id=container style='font: 10px/10px Ahem; width: 100px'>"
            "before    &#x05D0;&#x05D1;&#x05D2;&#x05D3; "
            "&#x05D4;&#x05D5;&#x05D6;&#x05D7;&#x05D8;&#x05D9;"
            "&#x05DA;&#x05DB;&#x05DC;&#x05DD;&#x05DE;&#x05DF;"
            "&#x05E0;&#x05E1;&#x05E2;&#x05E3;&#x05E4;&#x05E5;"
            "</div>");

  // Moving left from "|before DCBA" should be before context
  EXPECT_TRUE(LeftPositionOf(CaretBefore(0)).IsBeforeContext());

  // Moving right from "before |DCBA" should yield "before D|CBA"
  EXPECT_TRUE(RightPositionOf(CaretAfter(10)).IsWithinContext());
  EXPECT_EQ(CaretBefore(10), *RightPositionOf(CaretAfter(10)).position);
  EXPECT_TRUE(RightPositionOf(CaretAfter(6)).IsWithinContext());
  EXPECT_EQ(CaretBefore(10), *RightPositionOf(CaretAfter(6)).position);

  // Moving left from "before |DCBA" should yield "before| DCBA"
  EXPECT_TRUE(LeftPositionOf(CaretAfter(10)).IsWithinContext());
  EXPECT_EQ(CaretBefore(6), *LeftPositionOf(CaretAfter(10)).position);
  EXPECT_TRUE(LeftPositionOf(CaretAfter(6)).IsWithinContext());
  EXPECT_EQ(CaretBefore(6), *LeftPositionOf(CaretAfter(6)).position);

  // Moving right from "before DCBA|" should yield "V|UTSRQPONMLKJIHGFE"
  EXPECT_TRUE(RightPositionOf(CaretBefore(7)).IsWithinContext());
  EXPECT_EQ(CaretBefore(29), *RightPositionOf(CaretBefore(7)).position);

  // Moving left from "|VUTSRQPONMLKJIHGFE" should yield "before DCB|A"
  EXPECT_TRUE(LeftPositionOf(CaretAfter(29)).IsWithinContext());
  EXPECT_EQ(CaretAfter(7), *LeftPositionOf(CaretAfter(29)).position);

  // Moving right from "VUTSRQPONMLKJIHGFE|" should be after context
  EXPECT_TRUE(RightPositionOf(CaretBefore(12)).IsAfterContext());
}

TEST_F(NGCaretNavigatorTest, BidiAndSoftLineWrapAtSpaceRtl) {
  LoadAhem();
  SetupHtml(
      "container",
      "<div dir=rtl id=container style='font: 10px/10px Ahem; width: 120px'>"
      "&#x05D0;&#x05D1;&#x05D2;&#x05D3;    after encyclopedia"
      "</div>");

  // Moving right from "after DCBA|" should be before context
  EXPECT_TRUE(RightPositionOf(CaretBefore(0)).IsBeforeContext());

  // Moving left from "after| DCBA" should yield "afte|r DCBA"
  EXPECT_TRUE(LeftPositionOf(CaretAfter(4)).IsWithinContext());
  EXPECT_EQ(CaretBefore(9), *LeftPositionOf(CaretAfter(4)).position);
  EXPECT_TRUE(LeftPositionOf(CaretAfter(9)).IsWithinContext());
  EXPECT_EQ(CaretBefore(9), *LeftPositionOf(CaretAfter(9)).position);

  // Moving right from "after| DCBA" should yield "after |DCBA"
  EXPECT_TRUE(RightPositionOf(CaretAfter(4)).IsWithinContext());
  EXPECT_EQ(CaretBefore(4), *RightPositionOf(CaretAfter(4)).position);
  EXPECT_TRUE(RightPositionOf(CaretAfter(9)).IsWithinContext());
  EXPECT_EQ(CaretBefore(4), *RightPositionOf(CaretAfter(9)).position);

  // Moving left from "|after DCBA" should yield "encyclopedi|a"
  EXPECT_TRUE(LeftPositionOf(CaretBefore(5)).IsWithinContext());
  EXPECT_EQ(CaretBefore(22), *LeftPositionOf(CaretBefore(5)).position);

  // Moving right from "encyclopedia|" should yield "a|fter DCBA"
  EXPECT_TRUE(RightPositionOf(CaretAfter(22)).IsWithinContext());
  EXPECT_EQ(CaretAfter(5), *RightPositionOf(CaretAfter(22)).position);

  // Moving left from "|encyclopedia" should be after context
  EXPECT_TRUE(LeftPositionOf(CaretBefore(11)).IsAfterContext());
}

TEST_F(NGCaretNavigatorTest, SoftLineWrapAtHyphen) {
  SetupHtml("container", "<div id=container style=\"width:0\">abc-def</div>");

  EXPECT_TRUE(LeftPositionOf(CaretBefore(0)).IsBeforeContext());

  // 3 -> 4
  EXPECT_TRUE(RightPositionOf(CaretAfter(2)).IsWithinContext());
  EXPECT_EQ(CaretAfter(3), *RightPositionOf(CaretAfter(2)).position);
  EXPECT_TRUE(RightPositionOf(CaretBefore(3)).IsWithinContext());
  EXPECT_EQ(CaretAfter(3), *RightPositionOf(CaretBefore(3)).position);

  // 4 -> 5
  EXPECT_TRUE(RightPositionOf(CaretAfter(3)).IsWithinContext());
  EXPECT_EQ(CaretAfter(4), *RightPositionOf(CaretAfter(3)).position);
  EXPECT_TRUE(RightPositionOf(CaretBefore(4)).IsWithinContext());
  EXPECT_EQ(CaretAfter(4), *RightPositionOf(CaretBefore(4)).position);

  // 5 -> 4
  EXPECT_TRUE(LeftPositionOf(CaretBefore(5)).IsWithinContext());
  EXPECT_EQ(CaretBefore(4), *LeftPositionOf(CaretBefore(5)).position);
  EXPECT_TRUE(LeftPositionOf(CaretAfter(4)).IsWithinContext());
  EXPECT_EQ(CaretBefore(4), *LeftPositionOf(CaretAfter(4)).position);

  // 4 -> 3
  EXPECT_TRUE(LeftPositionOf(CaretBefore(4)).IsWithinContext());
  EXPECT_EQ(CaretBefore(3), *LeftPositionOf(CaretBefore(4)).position);
  EXPECT_TRUE(LeftPositionOf(CaretAfter(3)).IsWithinContext());
  EXPECT_EQ(CaretBefore(3), *LeftPositionOf(CaretAfter(3)).position);

  EXPECT_TRUE(RightPositionOf(CaretAfter(6)).IsAfterContext());
}

TEST_F(NGCaretNavigatorTest, MoveOverPseudoElementInBidi) {
  SetupHtml("container",
            "<style>.bidi::before,.bidi::after{content:'a\\05D0 b'}</style>"
            "<div id=container>&#x05D0;&#x05D1; &#x05D2;&#x05D3; "
            "<span class=bidi>&#x05D4;&#x05D5;</span>"
            " &#x05D6;&#x05D7; &#x05D8;&#x05D9;</div>");

  // Text: "AB CD aAbEFaAb GH IJ"
  // Rendered as: "DC BA aAbFEaAb JI HG"

  // Moving right from "BA |" should arrive at "F|E"
  EXPECT_TRUE(RightPositionOf(CaretAfter(5)).IsWithinContext());
  EXPECT_EQ(CaretBefore(10), *RightPositionOf(CaretAfter(5)).position);

  // Moving left from "|FE" should arrive at "BA| "
  EXPECT_TRUE(LeftPositionOf(CaretAfter(10)).IsWithinContext());
  EXPECT_EQ(CaretBefore(5), *LeftPositionOf(CaretAfter(10)).position);

  // Moving right from "FE|" should arrive at " |JI"
  EXPECT_TRUE(RightPositionOf(CaretBefore(9)).IsWithinContext());
  EXPECT_EQ(CaretAfter(14), *RightPositionOf(CaretBefore(9)).position);

  // Moving left from "| JI" should arrive at "F|E"
  EXPECT_TRUE(LeftPositionOf(CaretBefore(14)).IsWithinContext());
  EXPECT_EQ(CaretAfter(9), *LeftPositionOf(CaretBefore(14)).position);
}

TEST_F(NGCaretNavigatorTest, EnterableInlineBlock) {
  SetupHtml("container",
            "<div id=container>foo"
            "<span style='display:inline-block'>bar</span>"
            "baz</div>");

  // Moving right from "foo|" should enter the span from front.
  EXPECT_TRUE(RightPositionOf(CaretAfter(2)).HasEnteredChildContext());
  EXPECT_EQ(CaretBefore(3), *RightPositionOf(CaretAfter(2)).position);
  EXPECT_TRUE(RightPositionOf(CaretBefore(3)).HasEnteredChildContext());
  EXPECT_EQ(CaretBefore(3), *RightPositionOf(CaretBefore(3)).position);

  // Moving left from "|baz" should enter the span from behind.
  EXPECT_TRUE(LeftPositionOf(CaretBefore(4)).HasEnteredChildContext());
  EXPECT_EQ(CaretAfter(3), *LeftPositionOf(CaretBefore(4)).position);
  EXPECT_TRUE(LeftPositionOf(CaretAfter(3)).HasEnteredChildContext());
  EXPECT_EQ(CaretAfter(3), *LeftPositionOf(CaretAfter(3)).position);
}

TEST_F(NGCaretNavigatorTest, UnenterableInlineBlock) {
  SetupHtml("container",
            "<div id=container>foo"
            "<input value=bar>"
            "baz</div>");

  // Moving right from "foo|" should reach "<input>|".
  EXPECT_TRUE(RightPositionOf(CaretAfter(2)).IsWithinContext());
  EXPECT_EQ(CaretAfter(3), *RightPositionOf(CaretAfter(2)).position);
  EXPECT_TRUE(RightPositionOf(CaretBefore(3)).IsWithinContext());
  EXPECT_EQ(CaretAfter(3), *RightPositionOf(CaretBefore(3)).position);

  // Moving left from "|baz" should reach "|<input>".
  EXPECT_TRUE(LeftPositionOf(CaretBefore(4)).IsWithinContext());
  EXPECT_EQ(CaretBefore(3), *LeftPositionOf(CaretBefore(4)).position);
  EXPECT_TRUE(LeftPositionOf(CaretAfter(3)).IsWithinContext());
  EXPECT_EQ(CaretBefore(3), *LeftPositionOf(CaretAfter(3)).position);
}

}  // namespace blink
