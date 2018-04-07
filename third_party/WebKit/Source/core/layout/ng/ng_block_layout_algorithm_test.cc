// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/tag_collection.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

using testing::ElementsAre;
using testing::Pointee;

class NGBlockLayoutAlgorithmTest : public NGBaseLayoutAlgorithmTest {
 protected:
  void SetUp() override {
    NGBaseLayoutAlgorithmTest::SetUp();
    style_ = ComputedStyle::Create();
  }

  scoped_refptr<NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(
      const NGConstraintSpace& space,
      NGBlockNode node) {
    scoped_refptr<NGLayoutResult> result =
        NGBlockLayoutAlgorithm(node, space).Layout();

    return ToNGPhysicalBoxFragment(result->PhysicalFragment().get());
  }

  MinMaxSize RunComputeMinAndMax(NGBlockNode node) {
    // The constraint space is not used for min/max computation, but we need
    // it to create the algorithm.
    scoped_refptr<NGConstraintSpace> space =
        ConstructBlockLayoutTestConstraintSpace(
            WritingMode::kHorizontalTb, TextDirection::kLtr,
            NGLogicalSize(LayoutUnit(), LayoutUnit()));

    NGBlockLayoutAlgorithm algorithm(node, *space);
    MinMaxSizeInput input;
    auto min_max = algorithm.ComputeMinMaxSize(input);
    EXPECT_TRUE(min_max.has_value());
    return *min_max;
  }

  String DumpFragmentTree(const NGPhysicalBoxFragment* fragment) {
    NGPhysicalFragment::DumpFlags flags =
        NGPhysicalFragment::DumpHeaderText | NGPhysicalFragment::DumpSubtree |
        NGPhysicalFragment::DumpIndentation | NGPhysicalFragment::DumpOffset |
        NGPhysicalFragment::DumpSize;

    return fragment->DumpFragmentTree(flags);
  }

  scoped_refptr<ComputedStyle> style_;
};

TEST_F(NGBlockLayoutAlgorithmTest, FixedSize) {
  SetBodyInnerHTML(R"HTML(
    <div id="box" style="width:30px; height:40px"></div>
  )HTML");

  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));

  NGBlockNode box(ToLayoutBox(GetLayoutObjectByElementId("box")));

  scoped_refptr<NGPhysicalFragment> frag = RunBlockLayoutAlgorithm(*space, box);

  EXPECT_EQ(NGPhysicalSize(LayoutUnit(30), LayoutUnit(40)), frag->Size());
}

TEST_F(NGBlockLayoutAlgorithmTest, Caching) {
  ScopedLayoutNGFragmentCachingForTest layout_ng_fragment_caching(true);

  SetBodyInnerHTML(R"HTML(
    <div id="box" style="width:30px; height:40px"></div>
  )HTML");

  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));

  LayoutBlockFlow* block_flow =
      ToLayoutBlockFlow(GetLayoutObjectByElementId("box"));
  NGBlockNode node(block_flow);

  scoped_refptr<NGLayoutResult> result(node.Layout(*space, nullptr));
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(30), LayoutUnit(40)),
            result->PhysicalFragment()->Size());

  // Test pointer-equal constraint space
  result = block_flow->CachedLayoutResult(*space, nullptr);
  EXPECT_NE(result.get(), nullptr);

  // Test identical, but not pointer-equal, constraint space
  space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  result = block_flow->CachedLayoutResult(*space, nullptr);
  EXPECT_NE(result.get(), nullptr);

  // Test different constraint space
  space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(200), NGSizeIndefinite));
  result = block_flow->CachedLayoutResult(*space, nullptr);
  EXPECT_EQ(result.get(), nullptr);

  // Test layout invalidation
  block_flow->SetNeedsLayout("");
  result = block_flow->CachedLayoutResult(*space, nullptr);
  EXPECT_EQ(result.get(), nullptr);
}

// Verifies that two children are laid out with the correct size and position.
TEST_F(NGBlockLayoutAlgorithmTest, LayoutBlockChildren) {
  SetBodyInnerHTML(R"HTML(
    <div id="container" style="width: 30px">
      <div style="height: 20px">
      </div>
      <div style="height: 30px; margin-top: 5px; margin-bottom: 20px">
      </div>
    </div>
  )HTML");
  const int kWidth = 30;
  const int kHeight1 = 20;
  const int kHeight2 = 30;
  const int kMarginTop = 5;

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));

  scoped_refptr<NGPhysicalBoxFragment> frag =
      RunBlockLayoutAlgorithm(*space, container);

  EXPECT_EQ(LayoutUnit(kWidth), frag->Size().width);
  EXPECT_EQ(LayoutUnit(kHeight1 + kHeight2 + kMarginTop), frag->Size().height);
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, frag->Type());
  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragment* first_child_fragment = frag->Children()[0].get();
  EXPECT_EQ(kHeight1, first_child_fragment->Size().height);
  EXPECT_EQ(0, first_child_fragment->Offset().top);

  const NGPhysicalFragment* second_child_fragment = frag->Children()[1].get();
  EXPECT_EQ(kHeight2, second_child_fragment->Size().height);
  EXPECT_EQ(kHeight1 + kMarginTop, second_child_fragment->Offset().top);
}

// Verifies that a child is laid out correctly if it's writing mode is different
// from the parent's one.
TEST_F(NGBlockLayoutAlgorithmTest, LayoutBlockChildrenWithWritingMode) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #div2 {
        width: 50px;
        height: 50px;
        margin-left: 100px;
        writing-mode: horizontal-tb;
      }
    </style>
    <div id="container">
      <div id="div1" style="writing-mode: vertical-lr;">
        <div id="div2">
        </div>
      </div>
    </div>
  )HTML");
  const int kHeight = 50;
  const int kMarginLeft = 100;

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(500), LayoutUnit(500)));
  scoped_refptr<NGPhysicalBoxFragment> frag =
      RunBlockLayoutAlgorithm(*space, container);

  const NGPhysicalFragment* child = frag->Children()[0].get();
  // DIV2
  child = static_cast<const NGPhysicalBoxFragment*>(child)->Children()[0].get();

  EXPECT_EQ(kHeight, child->Size().height);
  EXPECT_EQ(0, child->Offset().top);
  EXPECT_EQ(kMarginLeft, child->Offset().left);
}

// Verifies that floats are positioned at the top of the first child that can
// determine its position after margins collapsed.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase1WithFloats) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #container {
          height: 200px;
          width: 200px;
          margin-top: 10px;
          padding: 0 7px;
          background-color: red;
        }
        #first-child {
          margin-top: 20px;
          height: 10px;
          background-color: blue;
        }
        #float-child-left {
          float: left;
          height: 10px;
          width: 10px;
          padding: 10px;
          margin: 10px;
          background-color: green;
        }
        #float-child-right {
          float: right;
          height: 30px;
          width: 30px;
          background-color: pink;
        }
      </style>
      <div id='container'>
        <div id='float-child-left'></div>
        <div id='float-child-right'></div>
        <div id='first-child'></div>
      </div>
    )HTML");

  // ** Run LayoutNG algorithm **
  scoped_refptr<NGConstraintSpace> space;
  scoped_refptr<NGPhysicalBoxFragment> fragment;
  std::tie(fragment, space) = RunBlockLayoutAlgorithmForElement(
      GetDocument().getElementsByTagName("html")->item(0));
  ASSERT_EQ(fragment->Children().size(), 1UL);
  auto* body_fragment = ToNGPhysicalBoxFragment(fragment->Children()[0].get());
  // 20 = max(first child's margin top, containers's margin top)
  int body_top_offset = 20;
  EXPECT_THAT(LayoutUnit(body_top_offset), body_fragment->Offset().top);
  // 8 = body's margin
  int body_left_offset = 8;
  EXPECT_THAT(LayoutUnit(body_left_offset), body_fragment->Offset().left);
  ASSERT_EQ(1UL, body_fragment->Children().size());
  auto* container_fragment =
      ToNGPhysicalBoxFragment(body_fragment->Children()[0].get());
  // 0 = collapsed with body's margin
  EXPECT_THAT(LayoutUnit(0), container_fragment->Offset().top);
  ASSERT_EQ(3UL, container_fragment->Children().size());
  auto* first_child_fragment =
      ToNGPhysicalBoxFragment(container_fragment->Children()[2].get());
  // 0 = collapsed with container's margin
  EXPECT_THAT(LayoutUnit(0), first_child_fragment->Offset().top);
}

// Verifies the collapsing margins case for the next pairs:
// - bottom margin of box and top margin of its next in-flow following sibling.
// - top and bottom margins of a box that does not establish a new block
//   formatting context and that has zero computed 'min-height', zero or 'auto'
//   computed 'height', and no in-flow children
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase2WithFloats) {
  SetBodyInnerHTML(R"HTML(
      <style>
      #first-child {
        background-color: red;
        height: 50px;
        margin-bottom: 20px;
      }
      #float-between-empties {
        background-color: green;
        float: left;
        height: 30px;
        width: 30px;
      }
      #float-between-nonempties {
        background-color: lightgreen;
        float: left;
        height: 40px;
        width: 40px;
      }
      #float-top-align {
        background-color: seagreen;
        float: left;
        height: 50px;
        width: 50px;
      }
      #second-child {
        background-color: blue;
        height: 50px;
        margin-top: 10px;
      }
      </style>
      <div id='first-child'>
        <div id='empty1' style='margin-bottom: -15px'></div>
        <div id='float-between-empties'></div>
        <div id='empty2'></div>
      </div>
      <div id='float-between-nonempties'></div>
      <div id='second-child'>
        <div id='float-top-align'></div>
        <div id='empty3'></div>
        <div id='empty4' style='margin-top: -30px'></div>
      </div>
      <div id='empty5'></div>
    )HTML");

  // ** Run LayoutNG algorithm **
  scoped_refptr<NGConstraintSpace> space;
  scoped_refptr<NGPhysicalBoxFragment> fragment;
  std::tie(fragment, space) = RunBlockLayoutAlgorithmForElement(
      GetDocument().getElementsByTagName("html")->item(0));

  auto* body_fragment = ToNGPhysicalBoxFragment(fragment->Children()[0].get());
  // -7 = empty1's margin(-15) + body's margin(8)
  EXPECT_THAT(LayoutUnit(-7), body_fragment->Offset().top);
  ASSERT_EQ(4UL, body_fragment->Children().size());

  FragmentChildIterator iterator(body_fragment);
  const auto* first_child_fragment = iterator.NextChild();
  EXPECT_THAT(LayoutUnit(), first_child_fragment->Offset().top);

  const auto* float_nonempties_fragment = iterator.NextChild();
  // 70 = first_child's height(50) + first child's margin-bottom(20)
  EXPECT_THAT(float_nonempties_fragment->Offset().top, LayoutUnit(70));
  EXPECT_THAT(float_nonempties_fragment->Offset().left, LayoutUnit(0));

  const auto* second_child_fragment = iterator.NextChild();
  // 40 = first_child's height(50) - margin's collapsing result(10)
  EXPECT_THAT(LayoutUnit(40), second_child_fragment->Offset().top);

  const auto* empty5_fragment = iterator.NextChild();
  // 90 = first_child's height(50) + collapsed margins(-10) +
  // second child's height(50)
  EXPECT_THAT(LayoutUnit(90), empty5_fragment->Offset().top);

  // ** Verify layout tree **
  Element* first_child = GetDocument().getElementById("first-child");
  // -7 = body_top_offset
  EXPECT_EQ(-7, first_child->OffsetTop());
}

// Verifies the collapsing margins case for the next pair:
// - bottom margin of a last in-flow child and bottom margin of its parent if
//   the parent has 'auto' computed height
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase3) {
  SetBodyInnerHTML(R"HTML(
      <style>
       #container {
         margin-bottom: 20px;
       }
       #child {
         margin-bottom: 200px;
         height: 50px;
       }
      </style>
      <div id='container'>
        <div id='child'></div>
      </div>
    )HTML");

  const NGPhysicalBoxFragment* body_fragment;
  const NGPhysicalBoxFragment* container_fragment;
  const NGPhysicalBoxFragment* child_fragment;
  scoped_refptr<const NGPhysicalBoxFragment> fragment;
  auto run_test = [&](const Length& container_height) {
    Element* container = GetDocument().getElementById("container");
    container->MutableComputedStyle()->SetHeight(container_height);
    container->GetLayoutObject()->SetNeedsLayout("");
    std::tie(fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
        GetDocument().getElementsByTagName("html")->item(0));
    ASSERT_EQ(1UL, fragment->Children().size());
    body_fragment = ToNGPhysicalBoxFragment(fragment->Children()[0].get());
    container_fragment =
        ToNGPhysicalBoxFragment(body_fragment->Children()[0].get());
    ASSERT_EQ(1UL, container_fragment->Children().size());
    child_fragment =
        ToNGPhysicalBoxFragment(container_fragment->Children()[0].get());
  };

  // height == auto
  run_test(Length(kAuto));
  // Margins are collapsed with the result 200 = std::max(20, 200)
  // The fragment size 258 == body's margin 8 + child's height 50 + 200
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(800), LayoutUnit(258)), fragment->Size());

  // height == fixed
  run_test(Length(50, kFixed));
  // Margins are not collapsed, so fragment still has margins == 20.
  // The fragment size 78 == body's margin 8 + child's height 50 + 20
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(800), LayoutUnit(78)), fragment->Size());
}

// Verifies that 2 adjoining margins are not collapsed if there is padding or
// border that separates them.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase4) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #container {
          margin: 30px 0px;
          width: 200px;
        }
        #child {
         margin: 200px 0px;
          height: 50px;
          background-color: blue;
        }
      </style>
      <div id='container'>
        <div id='child'></div>
      </div>
    )HTML");

  const NGPhysicalBoxFragment* body_fragment;
  const NGPhysicalBoxFragment* container_fragment;
  const NGPhysicalBoxFragment* child_fragment;
  scoped_refptr<const NGPhysicalBoxFragment> fragment;
  auto run_test = [&](const Length& container_padding_top) {
    Element* container = GetDocument().getElementById("container");
    container->MutableComputedStyle()->SetPaddingTop(container_padding_top);
    container->GetLayoutObject()->SetNeedsLayout("");
    std::tie(fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
        GetDocument().getElementsByTagName("html")->item(0));
    ASSERT_EQ(1UL, fragment->Children().size());
    body_fragment = ToNGPhysicalBoxFragment(fragment->Children()[0].get());
    container_fragment =
        ToNGPhysicalBoxFragment(body_fragment->Children()[0].get());
    ASSERT_EQ(1UL, container_fragment->Children().size());
    child_fragment =
        ToNGPhysicalBoxFragment(container_fragment->Children()[0].get());
  };

  // with padding
  run_test(Length(20, kFixed));
  // 500 = child's height 50 + 2xmargin 400 + paddint-top 20 +
  // container's margin 30
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(800), LayoutUnit(500)), fragment->Size());
  // 30 = max(body's margin 8, container margin 30)
  EXPECT_EQ(LayoutUnit(30), body_fragment->Offset().top);
  // 220 = container's padding top 20 + child's margin
  EXPECT_EQ(LayoutUnit(220), child_fragment->Offset().top);

  // without padding
  run_test(Length(0, kFixed));
  // 450 = 2xmax(body's margin 8, container's margin 30, child's margin 200) +
  //       child's height 50
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(800), LayoutUnit(450)), fragment->Size());
  // 200 = (body's margin 8, container's margin 30, child's margin 200)
  EXPECT_EQ(LayoutUnit(200), body_fragment->Offset().top);
  // 0 = collapsed margins
  EXPECT_EQ(LayoutUnit(0), child_fragment->Offset().top);
}

// Verifies that margins of 2 adjoining blocks with different writing modes
// get collapsed.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase5) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #container {
          margin-top: 10px;
          writing-mode: vertical-lr;
        }
        #vertical {
          margin-right: 90px;
          background-color: red;
          height: 70px;
          width: 30px;
        }
        #horizontal {
         background-color: blue;
          margin-left: 100px;
          writing-mode: horizontal-tb;
          height: 60px;
          width: 30px;
        }
      </style>
      <div id='container'>
        <div id='vertical'></div>
        <div id='horizontal'></div>
      </div>
    )HTML");
  scoped_refptr<NGPhysicalBoxFragment> fragment;
  std::tie(fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
      GetDocument().getElementsByTagName("html")->item(0));

  // body
  auto* body_fragment = ToNGPhysicalBoxFragment(fragment->Children()[0].get());
  // 10 = std::max(body's margin 8, container's margin top)
  int body_top_offset = 10;
  EXPECT_THAT(body_fragment->Offset().top, LayoutUnit(body_top_offset));
  int body_left_offset = 8;
  EXPECT_THAT(body_fragment->Offset().left, LayoutUnit(body_left_offset));

  // height = 70. std::max(vertical height's 70, horizontal's height's 60)
  ASSERT_EQ(NGPhysicalSize(LayoutUnit(784), LayoutUnit(70)),
            body_fragment->Size());
  ASSERT_EQ(1UL, body_fragment->Children().size());

  // container
  auto* container_fragment =
      ToNGPhysicalBoxFragment(body_fragment->Children()[0].get());
  // Container's margins are collapsed with body's fragment.
  EXPECT_THAT(container_fragment->Offset().top, LayoutUnit());
  EXPECT_THAT(container_fragment->Offset().left, LayoutUnit());
  ASSERT_EQ(2UL, container_fragment->Children().size());

  // vertical
  auto* vertical_fragment =
      ToNGPhysicalBoxFragment(container_fragment->Children()[0].get());
  EXPECT_THAT(vertical_fragment->Offset().top, LayoutUnit());
  EXPECT_THAT(vertical_fragment->Offset().left, LayoutUnit());

  // horizontal
  auto* horizontal_fragment =
      ToNGPhysicalBoxFragment(container_fragment->Children()[1].get());
  EXPECT_THAT(horizontal_fragment->Offset().top, LayoutUnit());
  // 130 = vertical's width 30 +
  //       std::max(vertical's margin right 90, horizontal's margin-left 100)
  EXPECT_THAT(horizontal_fragment->Offset().left, LayoutUnit(130));
}

// Verifies that margins collapsing logic works with Layout Inline.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsWithText) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
          margin: 10px;
        }
        p {
          margin: 20px;
        }
      </style>
      <p>Some text</p>
    )HTML");
  scoped_refptr<NGPhysicalBoxFragment> html_fragment;
  std::tie(html_fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
      GetDocument().getElementsByTagName("html")->item(0));

  auto* body_fragment =
      ToNGPhysicalBoxFragment(html_fragment->Children()[0].get());
  // 20 = std::max(body's margin, p's margin)
  EXPECT_THAT(body_fragment->Offset(),
              NGPhysicalOffset(LayoutUnit(10), LayoutUnit(20)));

  auto* p_fragment =
      ToNGPhysicalBoxFragment(body_fragment->Children()[0].get());
  // Collapsed margins with result = 0.
  EXPECT_THAT(p_fragment->Offset(),
              NGPhysicalOffset(LayoutUnit(20), LayoutUnit(0)));
}

// Verifies that the margin strut of a child with a different writing mode does
// not get used in the collapsing margins calculation.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase6) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #div1 {
        margin-bottom: 10px;
        width: 10px;
        height: 60px;
        writing-mode: vertical-rl;
      }
      #div2 { margin-left: -20px; width: 10px; }
      #div3 { margin-top: 40px; height: 60px; }
    </style>
    <div id="container" style="width:500px;height:500px">
      <div id="div1">
         <div id="div2">vertical</div>
      </div>
      <div id="div3"></div>
    </div>
  )HTML");
  const int kHeight = 60;
  const int kMarginBottom = 10;
  const int kMarginTop = 40;

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(500), LayoutUnit(500)));
  scoped_refptr<NGPhysicalBoxFragment> frag =
      RunBlockLayoutAlgorithm(*space, container);

  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragment* child1 = frag->Children()[0].get();
  EXPECT_EQ(0, child1->Offset().top);
  EXPECT_EQ(kHeight, child1->Size().height);

  const NGPhysicalFragment* child2 = frag->Children()[1].get();
  EXPECT_EQ(kHeight + std::max(kMarginBottom, kMarginTop),
            child2->Offset().top);
}

// Verifies that a child with clearance - which does nothing - still shifts its
// parent's offset.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase7) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      outline: solid purple 1px;
      width: 200px;
    }
    #zero {
      outline: solid red 1px;
      margin-top: 10px;
    }
    #float {
      background: yellow;
      float: right;
      width: 20px;
      height: 20px;
    }
    #inflow {
      background: blue;
      clear: left;
      height: 20px;
      margin-top: 20px;
    }
    </style>
    <div id="zero">
      <div id="float"></div>
    </div>
    <div id="inflow"></div>
  )HTML");

  scoped_refptr<NGPhysicalBoxFragment> fragment;
  std::tie(fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
      GetDocument().getElementsByTagName("html")->item(0));

  FragmentChildIterator iterator(fragment.get());

  // body
  const NGPhysicalBoxFragment* child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(200), LayoutUnit(20)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(8), LayoutUnit(20)), child->Offset());

  // #zero
  iterator.SetParent(child);
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(200), LayoutUnit(0)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(0)), child->Offset());

  // #inflow
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(200), LayoutUnit(20)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(0)), child->Offset());
}

// An empty block level element (with margins collapsing through it) has
// non-trivial behaviour with margins collapsing.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsEmptyBlockWithClearance) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      position: relative;
      outline: solid purple 1px;
      display: flow-root;
      width: 200px;
    }
    #float {
      background: orange;
      float: left;
      width: 50px;
      height: 50px;
    }
    #zero {
      outline: solid red 1px;
      clear: left;
    }
    #abs {
      background: cyan;
      position: absolute;
      width: 20px;
      height: 20px;
    }
    #inflow {
      background: green;
      height: 20px;
    }
    </style>
    <div id="float"></div>
    <div id="zero-top"></div>
    <div id="zero">
      <!-- This exists to produce complex margin struts. -->
      <div id="zero-inner"></div>
    </div>
    <div id="abs"></div>
    <div id="inflow"></div>
  )HTML");

  const LayoutNGBlockFlow* zero;
  const LayoutNGBlockFlow* abs;
  const LayoutNGBlockFlow* inflow;
  scoped_refptr<const NGPhysicalBoxFragment> fragment;
  auto run_test = [&](const Length& zero_top_margin_bottom,
                      const Length& zero_inner_margin_top,
                      const Length& zero_inner_margin_bottom,
                      const Length& zero_margin_bottom,
                      const Length& inflow_margin_top) {
    // Set the style of the elements we care about.
    Element* zero_top = GetDocument().getElementById("zero-top");
    zero_top->MutableComputedStyle()->SetMarginBottom(zero_top_margin_bottom);
    zero_top->GetLayoutObject()->SetNeedsLayout("");
    Element* zero_inner = GetDocument().getElementById("zero-inner");
    zero_inner->MutableComputedStyle()->SetMarginTop(zero_inner_margin_top);
    zero_inner->MutableComputedStyle()->SetMarginBottom(
        zero_inner_margin_bottom);
    zero_inner->GetLayoutObject()->SetNeedsLayout("");
    Element* zero_element = GetDocument().getElementById("zero");
    zero_element->MutableComputedStyle()->SetMarginBottom(zero_margin_bottom);
    zero_element->GetLayoutObject()->SetNeedsLayout("");

    Element* inflow_element = GetDocument().getElementById("inflow");
    inflow_element->MutableComputedStyle()->SetMarginTop(inflow_margin_top);
    inflow_element->GetLayoutObject()->SetNeedsLayout("");

    GetDocument().View()->UpdateAllLifecyclePhases();

    LayoutNGBlockFlow* child;
    // #float
    child = ToLayoutNGBlockFlow(
        GetDocument().getElementById("float")->GetLayoutObject());
    EXPECT_EQ(LayoutSize(LayoutUnit(50), LayoutUnit(50)), child->Size());
    EXPECT_EQ(LayoutPoint(LayoutUnit(0), LayoutUnit(0)), child->Location());

    // We need to manually test the position of #zero, #abs, #inflow.
    zero = ToLayoutNGBlockFlow(
        GetDocument().getElementById("zero")->GetLayoutObject());
    inflow = ToLayoutNGBlockFlow(
        GetDocument().getElementById("inflow")->GetLayoutObject());
    abs = ToLayoutNGBlockFlow(
        GetDocument().getElementById("abs")->GetLayoutObject());
  };

  // Base case of no margins.
  run_test(
      /* #zero-top margin-bottom */ Length(0, kFixed),
      /* #zero-inner margin-top */ Length(0, kFixed),
      /* #zero-inner margin-bottom */ Length(0, kFixed),
      /* #zero margin-bottom */ Length(0, kFixed),
      /* #inflow margin-top */ Length(0, kFixed));

  // #zero, #abs, #inflow should all be positioned at the float.
  EXPECT_EQ(LayoutUnit(50), zero->Location().Y());
  EXPECT_EQ(LayoutUnit(50), abs->Location().Y());
  EXPECT_EQ(LayoutUnit(50), inflow->Location().Y());

  // A margin strut which resolves to -50 (-70 + 20) adjusts the position of
  // #zero to the float clearance.
  run_test(
      /* #zero-top margin-bottom */ Length(0, kFixed),
      /* #zero-inner margin-top */ Length(-60, kFixed),
      /* #zero-inner margin-bottom */ Length(20, kFixed),
      /* #zero margin-bottom */ Length(-70, kFixed),
      /* #inflow margin-top */ Length(50, kFixed));

  // #zero is placed at the float, the margin strut is at:
  // 90 = (50 - (-60 + 20)).
  EXPECT_EQ(LayoutUnit(50), zero->Location().Y());

  // #abs estimates its position with the margin strut:
  // 40 = (90 + (-70 + 20)).
  EXPECT_EQ(LayoutUnit(40), abs->Location().Y());

  // #inflow has similar behaviour to #abs, but includes its margin.
  // 70 = (90 + (-70 + 50))
  EXPECT_EQ(LayoutUnit(70), inflow->Location().Y());

  // A margin strut which resolves to 60 (-10 + 70) means that #zero doesn't
  // get adjusted to clear the float, and we have normal behaviour.
  //
  // NOTE: This case below has wildly different results on different browsers,
  // we may have to change the behaviour here in the future for web compat.
  run_test(
      /* #zero-top margin-bottom */ Length(0, kFixed),
      /* #zero-inner margin-top */ Length(70, kFixed),
      /* #zero-inner margin-bottom */ Length(-10, kFixed),
      /* #zero margin-bottom */ Length(-20, kFixed),
      /* #inflow margin-top */ Length(80, kFixed));

  // #zero is placed at 60 (-10 + 70).
  EXPECT_EQ(LayoutUnit(60), zero->Location().Y());

  // #abs estimates its position with the margin strut:
  // 50 = (0 + (-20 + 70)).
  EXPECT_EQ(LayoutUnit(50), abs->Location().Y());

  // #inflow has similar behaviour to #abs, but includes its margin.
  // 60 = (0 + (-20 + 80))
  EXPECT_EQ(LayoutUnit(60), inflow->Location().Y());

  // #zero-top produces a margin which needs to be ignored, as #zero is
  // affected by clearance, it needs to have layout performed again, starting
  // with an empty margin strut.
  run_test(
      /* #zero-top margin-bottom */ Length(30, kFixed),
      /* #zero-inner margin-top */ Length(20, kFixed),
      /* #zero-inner margin-bottom */ Length(-10, kFixed),
      /* #zero margin-bottom */ Length(0, kFixed),
      /* #inflow margin-top */ Length(25, kFixed));

  // #zero is placed at the float, the margin strut is at:
  // 40 = (50 - (-10 + 20)).
  EXPECT_EQ(LayoutUnit(50), zero->Location().Y());

  // The margin strut is now disjoint, this is placed at:
  // 55 = (40 + (-10 + 25))
  EXPECT_EQ(LayoutUnit(55), inflow->Location().Y());
}

// Tests that when auto margins are applied to a new formatting context, they
// are applied within the layout opportunity.
TEST_F(NGBlockLayoutAlgorithmTest, NewFormattingContextAutoMargins) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container { width: 200px; direction: rtl; }
        #float { width: 100px; height: 60px; background: hotpink; float: left; }
        #newfc { direction: rtl; width: 50px; height: 20px; background: green; overflow: hidden; }
      </style>
      <div id="container">
        <div id="float"></div>
        <div id="newfc" style="margin-right: auto;"></div>
        <div id="newfc" style="margin-left: auto; margin-right: auto;"></div>
        <div id="newfc" style="margin-left: auto;"></div>
      </div>
    )HTML");

  scoped_refptr<const NGPhysicalBoxFragment> fragment;
  std::tie(fragment, std::ignore) =
      RunBlockLayoutAlgorithmForElement(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:200x60
    offset:0,0 size:100x60
    offset:100,0 size:50x20
    offset:125,20 size:50x20
    offset:150,40 size:50x20
)DUMP";
  EXPECT_EQ(expectation, DumpFragmentTree(fragment.get()));
}

// Verifies that a box's size includes its borders and padding, and that
// children are positioned inside the content box.
TEST_F(NGBlockLayoutAlgorithmTest, BorderAndPadding) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #div1 {
        width: 100px;
        height: 100px;
        border-style: solid;
        border-width: 1px 2px 3px 4px;
        padding: 5px 6px 7px 8px;
      }
    </style>
    <div id="container">
      <div id="div1">
         <div id="div2"></div>
      </div>
    </div>
  )HTML");
  const int kWidth = 100;
  const int kHeight = 100;
  const int kBorderTop = 1;
  const int kBorderRight = 2;
  const int kBorderBottom = 3;
  const int kBorderLeft = 4;
  const int kPaddingTop = 5;
  const int kPaddingRight = 6;
  const int kPaddingBottom = 7;
  const int kPaddingLeft = 8;

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));

  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));

  scoped_refptr<NGPhysicalBoxFragment> frag =
      RunBlockLayoutAlgorithm(*space, container);

  ASSERT_EQ(frag->Children().size(), 1UL);

  // div1
  const NGPhysicalFragment* child = frag->Children()[0].get();
  EXPECT_EQ(kBorderLeft + kPaddingLeft + kWidth + kPaddingRight + kBorderRight,
            child->Size().width);
  EXPECT_EQ(kBorderTop + kPaddingTop + kHeight + kPaddingBottom + kBorderBottom,
            child->Size().height);

  ASSERT_TRUE(child->IsBox());
  ASSERT_EQ(static_cast<const NGPhysicalBoxFragment*>(child)->Children().size(),
            1UL);

  // div2
  child = static_cast<const NGPhysicalBoxFragment*>(child)->Children()[0].get();
  EXPECT_EQ(kBorderTop + kPaddingTop, child->Offset().top);
  EXPECT_EQ(kBorderLeft + kPaddingLeft, child->Offset().left);
}

TEST_F(NGBlockLayoutAlgorithmTest, PercentageResolutionSize) {
  SetBodyInnerHTML(R"HTML(
    <div id="container" style="width: 30px; padding-left: 10px">
      <div id="div1" style="width: 40%"></div>
    </div>
  )HTML");
  const int kPaddingLeft = 10;
  const int kWidth = 30;

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));

  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  scoped_refptr<NGPhysicalBoxFragment> frag =
      RunBlockLayoutAlgorithm(*space, container);

  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), frag->Size().width);
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, frag->Type());
  ASSERT_EQ(frag->Children().size(), 1UL);

  const NGPhysicalFragment* child = frag->Children()[0].get();
  EXPECT_EQ(LayoutUnit(12), child->Size().width);
}

// A very simple auto margin case. We rely on the tests in ng_length_utils_test
// for the more complex cases; just make sure we handle auto at all here.
TEST_F(NGBlockLayoutAlgorithmTest, AutoMargin) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #first { width: 10px; margin-left: auto; margin-right: auto; }
    </style>
    <div id="container" style="width: 30px; padding-left: 10px">
      <div id="first">
      </div>
    </div>
  )HTML");
  const int kPaddingLeft = 10;
  const int kWidth = 30;
  const int kChildWidth = 10;

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));

  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  scoped_refptr<NGPhysicalBoxFragment> frag =
      RunBlockLayoutAlgorithm(*space, container);

  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), frag->Size().width);
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, frag->Type());
  ASSERT_EQ(1UL, frag->Children().size());

  const NGPhysicalFragment* child = frag->Children()[0].get();
  EXPECT_EQ(LayoutUnit(kChildWidth), child->Size().width);
  EXPECT_EQ(LayoutUnit(kPaddingLeft + 10), child->Offset().left);
  EXPECT_EQ(LayoutUnit(0), child->Offset().top);
}

// Verifies that floats can be correctly positioned if they are inside of nested
// empty blocks.
TEST_F(NGBlockLayoutAlgorithmTest, PositionFloatInsideEmptyBlocks) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #container {
          height: 300px;
          width: 300px;
          outline: blue solid;
        }
        #empty1 {
          margin: 20px;
          padding: 0 20px;
        }
        #empty2 {
          margin: 15px;
          padding: 0 15px;
        }
        #left-float {
          float: left;
          height: 5px;
          width: 5px;
          padding: 10px;
          margin: 10px;
          background-color: green;
        }
        #right-float {
          float: right;
          height: 15px;
          width: 15px;
          margin: 15px 10px;
          background-color: red;
        }
      </style>
      <div id='container'>
        <div id='empty1'>
          <div id='empty2'>
            <div id='left-float'></div>
            <div id='right-float'></div>
          </div>
        </div>
      </div>
    )HTML");

  // ** Run LayoutNG algorithm **
  scoped_refptr<NGConstraintSpace> space;
  scoped_refptr<NGPhysicalBoxFragment> fragment;
  std::tie(fragment, space) = RunBlockLayoutAlgorithmForElement(
      GetDocument().getElementsByTagName("html")->item(0));

  const auto* body_fragment =
      ToNGPhysicalBoxFragment(fragment->Children()[0].get());
  FragmentChildIterator iterator(body_fragment);
  // 20 = std::max(empty1's margin, empty2's margin, body's margin)
  int body_top_offset = 20;
  EXPECT_THAT(body_fragment->Offset().top, LayoutUnit(body_top_offset));
  ASSERT_EQ(1UL, body_fragment->Children().size());

  const auto* container_fragment = iterator.NextChild();
  ASSERT_EQ(1UL, container_fragment->Children().size());

  iterator.SetParent(container_fragment);
  const auto* empty1_fragment = iterator.NextChild();
  // 0, vertical margins got collapsed
  EXPECT_THAT(empty1_fragment->Offset().top, LayoutUnit());
  // 20 empty1's margin
  EXPECT_THAT(empty1_fragment->Offset().left, LayoutUnit(20));
  ASSERT_EQ(empty1_fragment->Children().size(), 1UL);

  iterator.SetParent(empty1_fragment);
  const auto* empty2_fragment = iterator.NextChild();
  // 0, vertical margins got collapsed
  EXPECT_THAT(LayoutUnit(), empty2_fragment->Offset().top);
  // 35 = empty1's padding(20) + empty2's padding(15)
  EXPECT_THAT(empty2_fragment->Offset().left, LayoutUnit(35));

  iterator.SetParent(empty2_fragment);
  const auto* left_float_fragment = iterator.NextChild();
  // inline 25 = empty2's padding(15) + left float's margin(10)
  // block 10 = left float's margin
  EXPECT_THAT(left_float_fragment->Offset(),
              NGPhysicalOffset(LayoutUnit(25), LayoutUnit(10)));

  const auto* right_float_fragment = iterator.NextChild();
  // inline offset 150 = empty2's padding(15) + right float's margin(10) + right
  // float offset(125)
  // block offset 15 = right float's margin
  LayoutUnit right_float_offset = LayoutUnit(125);
  EXPECT_THAT(
      right_float_fragment->Offset(),
      NGPhysicalOffset(LayoutUnit(25) + right_float_offset, LayoutUnit(15)));

  // ** Verify layout tree **
  Element* left_float = GetDocument().getElementById("left-float");
  // 88 = body's margin(8) +
  // empty1's padding and margin + empty2's padding and margins + float's
  // padding
  EXPECT_THAT(left_float->OffsetLeft(), 88);
  // 30 = body_top_offset(collapsed margins result) + float's padding
  EXPECT_THAT(left_float->OffsetTop(), body_top_offset + 10);

  // ** Legacy Floating objects **
  Element* empty2 = GetDocument().getElementById("empty2");
  auto& floating_objects =
      const_cast<FloatingObjects*>(
          ToLayoutBlockFlow(empty2->GetLayoutObject())->GetFloatingObjects())
          ->MutableSet();
  ASSERT_EQ(2UL, floating_objects.size());
  auto left_floating_object = floating_objects.TakeFirst();
  ASSERT_TRUE(left_floating_object->IsPlaced());
  // 80 = float_inline_offset(25) + accumulative offset of empty blocks(35 + 20)
  EXPECT_THAT(left_floating_object->X(), LayoutUnit(15));
  // 10 = left float's margin
  EXPECT_THAT(left_floating_object->Y(), LayoutUnit());

  auto right_floating_object = floating_objects.TakeFirst();
  ASSERT_TRUE(right_floating_object->IsPlaced());
  // 150 = float_inline_offset(25) +
  //       right float offset(125)
  EXPECT_THAT(right_floating_object->X(), LayoutUnit(140));
  // 15 = right float's margin
  EXPECT_THAT(right_floating_object->Y(), LayoutUnit(0));
}

// Verifies that left/right floating and regular blocks can be positioned
// correctly by the algorithm.
TEST_F(NGBlockLayoutAlgorithmTest, PositionFloatFragments) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #container {
          height: 200px;
          width: 200px;
        }
        #left-float {
          background-color: red;
          float: left;
          height: 30px;
          width: 30px;
        }
        #left-wide-float {
          background-color: greenyellow;
          float: left;
          height: 30px;
          width: 180px;
        }
        #regular {
          width: 40px;
          height: 40px;
          background-color: green;
        }
        #right-float {
          background-color: cyan;
          float: right;
          width: 50px;
          height: 50px;
        }
        #left-float-with-margin {
          background-color: black;
          float: left;
          height: 120px;
          margin: 10px;
          width: 120px;
        }
      </style>
      <div id='container'>
        <div id='left-float'></div>
        <div id='left-wide-float'></div>
        <div id='regular'></div>
        <div id='right-float'></div>
        <div id='left-float-with-margin'></div>
      </div>
      )HTML");

  // ** Run LayoutNG algorithm **
  scoped_refptr<NGConstraintSpace> space;
  scoped_refptr<NGPhysicalBoxFragment> fragment;
  std::tie(fragment, space) = RunBlockLayoutAlgorithmForElement(
      GetDocument().getElementsByTagName("html")->item(0));

  // ** Verify LayoutNG fragments and the list of positioned floats **
  ASSERT_EQ(1UL, fragment->Children().size());
  const auto* body_fragment =
      ToNGPhysicalBoxFragment(fragment->Children()[0].get());
  EXPECT_THAT(LayoutUnit(8), body_fragment->Offset().top);

  FragmentChildIterator iterator(body_fragment);
  const auto* container_fragment = iterator.NextChild();
  ASSERT_EQ(5UL, container_fragment->Children().size());

  // ** Verify layout tree **
  Element* left_float = GetDocument().getElementById("left-float");
  // 8 = body's margin-top
  EXPECT_EQ(8, left_float->OffsetTop());

  iterator.SetParent(container_fragment);
  const auto* left_float_fragment = iterator.NextChild();
  EXPECT_THAT(LayoutUnit(), left_float_fragment->Offset().top);

  Element* left_wide_float = GetDocument().getElementById("left-wide-float");
  // left-wide-float is positioned right below left-float as it's too wide.
  // 38 = left_float_block_offset 8 +
  //      left-float's height 30
  EXPECT_EQ(38, left_wide_float->OffsetTop());

  const auto* left_wide_float_fragment = iterator.NextChild();
  // 30 = left-float's height.
  EXPECT_THAT(LayoutUnit(30), left_wide_float_fragment->Offset().top);

  Element* regular = GetDocument().getElementById("regular");
  // regular_block_offset = body's margin-top 8
  EXPECT_EQ(8, regular->OffsetTop());

  const auto* regular_block_fragment = iterator.NextChild();
  EXPECT_THAT(LayoutUnit(), regular_block_fragment->Offset().top);

  Element* right_float = GetDocument().getElementById("right-float");
  // 158 = body's margin-left 8 + container's width 200 - right_float's width 50
  // it's positioned right after our left_wide_float
  // 68 = left_wide_float_block_offset 38 + left-wide-float's height 30
  EXPECT_EQ(158, right_float->OffsetLeft());
  EXPECT_EQ(68, right_float->OffsetTop());

  const auto* right_float_fragment = iterator.NextChild();
  // 60 = right_float_block_offset(68) - body's margin(8)
  EXPECT_THAT(LayoutUnit(60), right_float_fragment->Offset().top);
  // 150 = right_float_inline_offset(158) - body's margin(8)
  EXPECT_THAT(LayoutUnit(150), right_float_fragment->Offset().left);

  Element* left_float_with_margin =
      GetDocument().getElementById("left-float-with-margin");
  // 18 = body's margin(8) + left-float-with-margin's margin(10)
  EXPECT_EQ(18, left_float_with_margin->OffsetLeft());
  // 78 = left_wide_float_block_offset 38 + left-wide-float's height 30 +
  //      left-float-with-margin's margin(10)
  EXPECT_EQ(78, left_float_with_margin->OffsetTop());

  const auto* left_float_with_margin_fragment = iterator.NextChild();
  // 70 = left_float_with_margin_block_offset(78) - body's margin(8)
  EXPECT_THAT(LayoutUnit(70), left_float_with_margin_fragment->Offset().top);
  // 10 = left_float_with_margin_inline_offset(18) - body's margin(8)
  EXPECT_THAT(LayoutUnit(10), left_float_with_margin_fragment->Offset().left);
}

// Verifies that NG block layout algorithm respects "clear" CSS property.
TEST_F(NGBlockLayoutAlgorithmTest, PositionFragmentsWithClear) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #container {
          height: 200px;
          width: 200px;
        }
        #float-left {
          background-color: red;
          float: left;
          height: 30px;
          width: 30px;
        }
        #float-right {
          background-color: blue;
          float: right;
          height: 170px;
          width: 40px;
        }
        #clearance {
          background-color: yellow;
          height: 60px;
          width: 60px;
          margin: 20px;
        }
        #block {
          margin: 40px;
          background-color: black;
          height: 60px;
          width: 60px;
        }
        #adjoining-clearance {
          background-color: green;
          clear: left;
          height: 20px;
          width: 20px;
          margin: 30px;
        }
      </style>
      <div id='container'>
        <div id='float-left'></div>
        <div id='float-right'></div>
        <div id='clearance'></div>
        <div id='block'></div>
        <div id='adjoining-clearance'></div>
      </div>
    )HTML");

  const NGPhysicalBoxFragment* clerance_fragment;
  const NGPhysicalBoxFragment* body_fragment;
  const NGPhysicalBoxFragment* container_fragment;
  const NGPhysicalBoxFragment* block_fragment;
  const NGPhysicalBoxFragment* adjoining_clearance_fragment;
  scoped_refptr<const NGPhysicalBoxFragment> fragment;
  auto run_with_clearance = [&](EClear clear_value) {
    Element* el_with_clear = GetDocument().getElementById("clearance");
    el_with_clear->MutableComputedStyle()->SetClear(clear_value);
    el_with_clear->GetLayoutObject()->SetNeedsLayout("");
    std::tie(fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
        GetDocument().getElementsByTagName("html")->item(0));
    ASSERT_EQ(1UL, fragment->Children().size());
    body_fragment = ToNGPhysicalBoxFragment(fragment->Children()[0].get());
    container_fragment =
        ToNGPhysicalBoxFragment(body_fragment->Children()[0].get());
    ASSERT_EQ(5UL, container_fragment->Children().size());
    clerance_fragment =
        ToNGPhysicalBoxFragment(container_fragment->Children()[2].get());
    block_fragment =
        ToNGPhysicalBoxFragment(container_fragment->Children()[3].get());
    adjoining_clearance_fragment =
        ToNGPhysicalBoxFragment(container_fragment->Children()[4].get());
  };

  // clear: none
  run_with_clearance(EClear::kNone);
  // 20 = std::max(body's margin 8, clearance's margins 20)
  EXPECT_EQ(LayoutUnit(20), body_fragment->Offset().top);
  EXPECT_EQ(LayoutUnit(0), container_fragment->Offset().top);
  // 0 = collapsed margins
  EXPECT_EQ(LayoutUnit(0), clerance_fragment->Offset().top);
  // 100 = clearance's height 60 +
  //       std::max(clearance's margins 20, block's margins 40)
  EXPECT_EQ(LayoutUnit(100), block_fragment->Offset().top);
  // 200 = 100 + block's height 60 + max(adjoining_clearance's margins 30,
  //                                     block's margins 40)
  EXPECT_EQ(LayoutUnit(200), adjoining_clearance_fragment->Offset().top);

  // clear: right
  run_with_clearance(EClear::kRight);
  // 8 = body's margin. This doesn't collapse its margins with 'clearance' block
  // as it's not an adjoining block to body.
  EXPECT_EQ(LayoutUnit(8), body_fragment->Offset().top);
  EXPECT_EQ(LayoutUnit(0), container_fragment->Offset().top);
  // 170 = float-right's height
  EXPECT_EQ(LayoutUnit(170), clerance_fragment->Offset().top);
  // 270 = float-right's height + clearance's height 60 +
  //       max(clearance's margin 20, block margin 40)
  EXPECT_EQ(LayoutUnit(270), block_fragment->Offset().top);
  // 370 = block's offset 270 + block's height 60 +
  //       std::max(block's margin 40, adjoining_clearance's margin 30)
  EXPECT_EQ(LayoutUnit(370), adjoining_clearance_fragment->Offset().top);

  // clear: left
  run_with_clearance(EClear::kLeft);
  // 8 = body's margin. This doesn't collapse its margins with 'clearance' block
  // as it's not an adjoining block to body.
  EXPECT_EQ(LayoutUnit(8), body_fragment->Offset().top);
  EXPECT_EQ(LayoutUnit(0), container_fragment->Offset().top);
  // 30 = float_left's height
  EXPECT_EQ(LayoutUnit(30), clerance_fragment->Offset().top);
  // 130 = float_left's height + clearance's height 60 +
  //       max(clearance's margin 20, block margin 40)
  EXPECT_EQ(LayoutUnit(130), block_fragment->Offset().top);
  // 230 = block's offset 130 + block's height 60 +
  //       std::max(block's margin 40, adjoining_clearance's margin 30)
  EXPECT_EQ(LayoutUnit(230), adjoining_clearance_fragment->Offset().top);

  // clear: both
  // same as clear: right
  run_with_clearance(EClear::kBoth);
  EXPECT_EQ(LayoutUnit(8), body_fragment->Offset().top);
  EXPECT_EQ(LayoutUnit(0), container_fragment->Offset().top);
  EXPECT_EQ(LayoutUnit(170), clerance_fragment->Offset().top);
  EXPECT_EQ(LayoutUnit(270), block_fragment->Offset().top);
  EXPECT_EQ(LayoutUnit(370), adjoining_clearance_fragment->Offset().top);
}

// Verifies that we compute the right min and max-content size.
TEST_F(NGBlockLayoutAlgorithmTest, ComputeMinMaxContent) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="first-child" style="width: 20px"></div>
      <div id="second-child" style="width: 30px"></div>
    </div>
  )HTML");

  const int kSecondChildWidth = 30;

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));

  MinMaxSize sizes = RunComputeMinAndMax(container);
  EXPECT_EQ(kSecondChildWidth, sizes.min_size);
  EXPECT_EQ(kSecondChildWidth, sizes.max_size);
}

TEST_F(NGBlockLayoutAlgorithmTest, ComputeMinMaxContentFloats) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #f1 { float: left; width: 20px; }
      #f2 { float: left; width: 30px; }
      #f3 { float: right; width: 40px; }
    </style>
    <div id="container">
      <div id="f1"></div>
      <div id="f2"></div>
      <div id="f3"></div>
    </div>
  )HTML");

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));

  MinMaxSize sizes = RunComputeMinAndMax(container);
  EXPECT_EQ(LayoutUnit(40), sizes.min_size);
  EXPECT_EQ(LayoutUnit(90), sizes.max_size);
}

TEST_F(NGBlockLayoutAlgorithmTest, ComputeMinMaxContentFloatsClearance) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #f1 { float: left; width: 20px; }
      #f2 { float: left; width: 30px; }
      #f3 { float: right; width: 40px; clear: left; }
    </style>
    <div id="container">
      <div id="f1"></div>
      <div id="f2"></div>
      <div id="f3"></div>
    </div>
  )HTML");

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));

  MinMaxSize sizes = RunComputeMinAndMax(container);
  EXPECT_EQ(LayoutUnit(40), sizes.min_size);
  EXPECT_EQ(LayoutUnit(50), sizes.max_size);
}

TEST_F(NGBlockLayoutAlgorithmTest, ComputeMinMaxContentNewFormattingContext) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #f1 { float: left; width: 20px; }
      #f2 { float: left; width: 30px; }
      #fc { display: flex; width: 40px; margin-left: 60px; }
    </style>
    <div id="container">
      <div id="f1"></div>
      <div id="f2"></div>
      <div id="fc"></div>
    </div>
  )HTML");

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));

  MinMaxSize sizes = RunComputeMinAndMax(container);
  EXPECT_EQ(LayoutUnit(100), sizes.min_size);
  EXPECT_EQ(LayoutUnit(100), sizes.max_size);
}

TEST_F(NGBlockLayoutAlgorithmTest,
       ComputeMinMaxContentNewFormattingContextNegativeMargins) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #f1 { float: left; width: 20px; }
      #f2 { float: left; width: 30px; }
      #fc { display: flex; width: 40px; margin-left: -20px; }
    </style>
    <div id="container">
      <div id="f1"></div>
      <div id="f2"></div>
      <div id="fc"></div>
    </div>
  )HTML");

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));

  MinMaxSize sizes = RunComputeMinAndMax(container);
  EXPECT_EQ(LayoutUnit(30), sizes.min_size);
  EXPECT_EQ(LayoutUnit(70), sizes.max_size);
}

TEST_F(NGBlockLayoutAlgorithmTest,
       ComputeMinMaxContentSingleNewFormattingContextNegativeMargins) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fc { display: flex; width: 20px; margin-left: -40px; }
    </style>
    <div id="container">
      <div id="fc"></div>
    </div>
  )HTML");

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));

  MinMaxSize sizes = RunComputeMinAndMax(container);
  EXPECT_EQ(LayoutUnit(), sizes.min_size);
  EXPECT_EQ(LayoutUnit(), sizes.max_size);
}

// Tests that we correctly handle shrink-to-fit
TEST_F(NGBlockLayoutAlgorithmTest, ShrinkToFit) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="first-child" style="width: 20px"></div>
      <div id="second-child" style="width: 30px"></div>
    </div>
  )HTML");
  const int kWidthChild2 = 30;

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));

  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(100), NGSizeIndefinite), true);
  scoped_refptr<NGPhysicalFragment> frag =
      RunBlockLayoutAlgorithm(*space, container);

  EXPECT_EQ(LayoutUnit(kWidthChild2), frag->Size().width);
}

// Verifies that we position empty blocks and floats correctly inside of the
// block that establishes new BFC.
TEST_F(NGBlockLayoutAlgorithmTest, PositionEmptyBlocksInNewBfc) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        overflow: hidden;
      }
      #empty-block1 {
        margin: 8px;
      }
      #left-float {
        float: left;
        background: red;
        height: 20px;
        width: 10px;
        margin: 15px;
      }
      #empty-block2 {
        margin-top: 50px;
      }
    </style>
    <div id="container">
      <div id="left-float"></div>
      <div id="empty-block1"></div>
      <div id="empty-block2"></div>
    </div>
  )HTML");
  // #container is the new parent for our float because it's height != 0.
  Element* container = GetDocument().getElementById("container");
  auto& floating_objects =
      const_cast<FloatingObjects*>(
          ToLayoutBlockFlow(container->GetLayoutObject())->GetFloatingObjects())
          ->MutableSet();
  ASSERT_EQ(1UL, floating_objects.size());
  auto floating_object = floating_objects.TakeFirst();
  // left-float's margin = 15.
  EXPECT_THAT(floating_object->X(), LayoutUnit());
  EXPECT_THAT(floating_object->Y(), LayoutUnit());

  scoped_refptr<const NGPhysicalBoxFragment> html_fragment;
  std::tie(html_fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
      GetDocument().getElementsByTagName("html")->item(0));
  auto* body_fragment =
      ToNGPhysicalBoxFragment(html_fragment->Children()[0].get());
  auto* container_fragment =
      ToNGPhysicalBoxFragment(body_fragment->Children()[0].get());
  auto* empty_block1 =
      ToNGPhysicalBoxFragment(container_fragment->Children()[1].get());
  // empty-block1's margin == 8
  EXPECT_THAT(empty_block1->Offset(),
              NGPhysicalOffset(LayoutUnit(8), LayoutUnit(8)));

  auto* empty_block2 =
      ToNGPhysicalBoxFragment(container_fragment->Children()[2].get());
  // empty-block2's margin == 50
  EXPECT_THAT(empty_block2->Offset(),
              NGPhysicalOffset(LayoutUnit(0), LayoutUnit(50)));
}

// Verifies that we can correctly position blocks with clearance and
// intruding floats.
TEST_F(NGBlockLayoutAlgorithmTest,
       PositionBlocksWithClearanceAndIntrudingFloats) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    body { margin: 80px; }
    #left-float {
      background: green;
      float: left;
      width: 50px;
      height: 50px;
    }
    #right-float {
      background: red;
      float: right;
      margin: 0 80px 0 10px;
      width: 50px;
      height: 80px;
    }
    #block1 {
      outline: purple solid;
      height: 30px;
      margin: 130px 0 20px 0;
    }
    #zero {
     margin-top: 30px;
    }
    #container-clear {
      clear: left;
      outline: orange solid;
    }
    #clears-right {
      clear: right;
      height: 20px;
      background: lightblue;
    }
    </style>

    <div id="left-float"></div>
    <div id="right-float"></div>
    <div id="block1"></div>
    <div id="container-clear">
      <div id="zero"></div>
      <div id="clears-right"></div>
    </div>
  )HTML");

  // Run LayoutNG algorithm.
  scoped_refptr<NGPhysicalBoxFragment> html_fragment;
  std::tie(html_fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
      GetDocument().getElementsByTagName("html")->item(0));
  auto* body_fragment =
      ToNGPhysicalBoxFragment(html_fragment->Children()[0].get());
  ASSERT_EQ(4UL, body_fragment->Children().size());

  // Verify #container-clear block
  auto* container_clear_fragment =
      ToNGPhysicalBoxFragment(body_fragment->Children()[3].get());
  // 60 = block1's height 30 + std::max(block1's margin 20, zero's margin 30)
  EXPECT_THAT(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(60)),
              container_clear_fragment->Offset());
  Element* container_clear = GetDocument().getElementById("container-clear");
  // 190 = block1's margin 130 + block1's height 30 +
  //       std::max(block1's margin 20, zero's margin 30)
  EXPECT_THAT(container_clear->OffsetTop(), 190);

  // Verify #clears-right block
  ASSERT_EQ(2UL, container_clear_fragment->Children().size());
  auto* clears_right_fragment =
      ToNGPhysicalBoxFragment(container_clear_fragment->Children()[1].get());
  // 20 = right-float's block end offset (130 + 80) -
  //      container_clear->offsetTop() 190
  EXPECT_THAT(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(20)),
              clears_right_fragment->Offset());
}

// Tests that a block won't fragment if it doesn't reach the fragmentation line.
TEST_F(NGBlockLayoutAlgorithmTest, NoFragmentation) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          height: 200px;
        }
      </style>
      <div id='container'></div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false, true,
          kFragmentainerSpaceAvailable);

  // We should only have one 150x200 fragment with no fragmentation.
  scoped_refptr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, *space).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(200)), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken()->IsFinished());
}

// Tests that a block will fragment if it reaches the fragmentation line.
TEST_F(NGBlockLayoutAlgorithmTest, SimpleFragmentation) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          height: 300px;
        }
      </style>
      <div id='container'></div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false, true,
          kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, *space).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(200)), fragment->Size());
  ASSERT_FALSE(fragment->BreakToken()->IsFinished());

  fragment = NGBlockLayoutAlgorithm(node, *space,
                                    ToNGBlockBreakToken(fragment->BreakToken()))
                 .Layout()
                 ->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(100)), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken()->IsFinished());
}

// Tests that children inside the same block formatting context fragment when
// reaching a fragmentation line.
TEST_F(NGBlockLayoutAlgorithmTest, InnerChildrenFragmentation) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          padding-top: 20px;
        }
        #child1 {
          height: 200px;
          margin-bottom: 20px;
        }
        #child2 {
          height: 100px;
          margin-top: 20px;
        }
      </style>
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
      </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false, true,
          kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, *space).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(200)), fragment->Size());
  ASSERT_FALSE(fragment->BreakToken()->IsFinished());

  FragmentChildIterator iterator(ToNGPhysicalBoxFragment(fragment.get()));
  const NGPhysicalBoxFragment* child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(180)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(20)), child->Offset());

  EXPECT_FALSE(iterator.NextChild());

  fragment = NGBlockLayoutAlgorithm(node, *space,
                                    ToNGBlockBreakToken(fragment->BreakToken()))
                 .Layout()
                 ->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(140)), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken()->IsFinished());

  iterator.SetParent(ToNGPhysicalBoxFragment(fragment.get()));
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(20)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(0)), child->Offset());

  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(100)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(40)), child->Offset());

  EXPECT_FALSE(iterator.NextChild());
}

// Tests that children which establish new formatting contexts fragment
// correctly.
TEST_F(NGBlockLayoutAlgorithmTest,
       InnerFormattingContextChildrenFragmentation) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          padding-top: 20px;
        }
        #child1 {
          height: 200px;
          margin-bottom: 20px;
          contain: paint;
        }
        #child2 {
          height: 100px;
          margin-top: 20px;
          contain: paint;
        }
      </style>
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
      </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false, true,
          kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, *space).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(200)), fragment->Size());
  ASSERT_FALSE(fragment->BreakToken()->IsFinished());

  FragmentChildIterator iterator(ToNGPhysicalBoxFragment(fragment.get()));
  const NGPhysicalBoxFragment* child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(180)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(20)), child->Offset());

  EXPECT_FALSE(iterator.NextChild());

  fragment = NGBlockLayoutAlgorithm(node, *space,
                                    ToNGBlockBreakToken(fragment->BreakToken()))
                 .Layout()
                 ->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(140)), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken()->IsFinished());

  iterator.SetParent(ToNGPhysicalBoxFragment(fragment.get()));
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(20)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(0)), child->Offset());

  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(100)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(40)), child->Offset());

  EXPECT_FALSE(iterator.NextChild());
}

// Tests that children inside a block container will fragment if the container
// doesn't reach the fragmentation line.
TEST_F(NGBlockLayoutAlgorithmTest, InnerChildrenFragmentationSmallHeight) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          padding-top: 20px;
          height: 50px;
        }
        #child1 {
          height: 200px;
          margin-bottom: 20px;
        }
        #child2 {
          height: 100px;
          margin-top: 20px;
        }
      </style>
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
      </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false, true,
          kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, *space).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(70)), fragment->Size());
  ASSERT_FALSE(fragment->BreakToken()->IsFinished());

  FragmentChildIterator iterator(ToNGPhysicalBoxFragment(fragment.get()));
  const NGPhysicalBoxFragment* child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(180)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(20)), child->Offset());

  EXPECT_FALSE(iterator.NextChild());

  fragment = NGBlockLayoutAlgorithm(node, *space,
                                    ToNGBlockBreakToken(fragment->BreakToken()))
                 .Layout()
                 ->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(0)), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken()->IsFinished());

  iterator.SetParent(ToNGPhysicalBoxFragment(fragment.get()));
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(20)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(0)), child->Offset());

  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(100)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(40)), child->Offset());

  EXPECT_FALSE(iterator.NextChild());
}

// Tests that float children fragment correctly inside a parallel flow.
TEST_F(NGBlockLayoutAlgorithmTest, FloatFragmentationParallelFlows) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        width: 150px;
        height: 50px;
      }
      #float1 {
        width: 50px;
        height: 200px;
        float: left;
      }
      #float2 {
        width: 75px;
        height: 250px;
        float: right;
        margin: 10px;
      }
    </style>
    <div id='container'>
      <div id='float1'></div>
      <div id='float2'></div>
    </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(150);

  NGBlockNode node(ToLayoutBlockFlow(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false, true,
          kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, *space).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(50)), fragment->Size());
  ASSERT_FALSE(fragment->BreakToken()->IsFinished());

  FragmentChildIterator iterator(ToNGPhysicalBoxFragment(fragment.get()));

  // First fragment of float1.
  const auto* child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(50), LayoutUnit(150)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(0)), child->Offset());

  // First fragment of float2.
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(75), LayoutUnit(150)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(65), LayoutUnit(10)), child->Offset());

  space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false, true,
      kFragmentainerSpaceAvailable);
  fragment = NGBlockLayoutAlgorithm(node, *space,
                                    ToNGBlockBreakToken(fragment->BreakToken()))
                 .Layout()
                 ->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(0)), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken()->IsFinished());

  iterator.SetParent(ToNGPhysicalBoxFragment(fragment.get()));

  // Second fragment of float1.
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(50), LayoutUnit(50)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(0)), child->Offset());

  // Second fragment of float2.
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(75), LayoutUnit(100)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(65), LayoutUnit()), child->Offset());
}

// Tests that float children don't fragment if they aren't in the same writing
// mode as their parent.
TEST_F(NGBlockLayoutAlgorithmTest, FloatFragmentationOrthogonalFlows) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        width: 150px;
        height: 60px;
        overflow: hidden;
      }
      #float1 {
        width: 100px;
        height: 50px;
        float: left;
      }
      #float2 {
        width: 60px;
        height: 200px;
        float: right;
        writing-mode: vertical-rl;
      }
    </style>
    <div id='container'>
      <div id='float1'></div>
      <div id='float2'></div>
    </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(150);

  NGBlockNode node(ToLayoutBlockFlow(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false, true,
          kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, *space).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(60)), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken()->IsFinished());

  // float2 should only have one fragment.
  FragmentChildIterator iterator(ToNGPhysicalBoxFragment(fragment.get()));
  const auto* child = iterator.NextChild();
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(60), LayoutUnit(200)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(90), LayoutUnit(50)), child->Offset());
  ASSERT_TRUE(child->BreakToken()->IsFinished());
}

// Tests that a float child inside a zero height block fragments correctly.
TEST_F(NGBlockLayoutAlgorithmTest, FloatFragmentationZeroHeight) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        width: 150px;
        height: 50px;
      }
      #float {
        width: 75px;
        height: 200px;
        float: left;
        margin: 10px;
      }
    </style>
    <div id='container'>
      <div id='zero'>
        <div id='float'></div>
      </div>
    </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(150);

  NGBlockNode node(ToLayoutBlockFlow(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false, true,
          kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, *space).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(50)), fragment->Size());
  ASSERT_FALSE(fragment->BreakToken()->IsFinished());

  FragmentChildIterator iterator(ToNGPhysicalBoxFragment(fragment.get()));
  const auto* child = iterator.NextChild();

  // First fragment of float.
  iterator.SetParent(child);
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(75), LayoutUnit(150)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(10), LayoutUnit(10)), child->Offset());

  space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false, true,
      kFragmentainerSpaceAvailable);
  fragment = NGBlockLayoutAlgorithm(node, *space,
                                    ToNGBlockBreakToken(fragment->BreakToken()))
                 .Layout()
                 ->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(0)), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken()->IsFinished());

  iterator.SetParent(ToNGPhysicalBoxFragment(fragment.get()));
  child = iterator.NextChild();

  // Second fragment of float.
  iterator.SetParent(child);
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(75), LayoutUnit(50)), child->Size());
  // TODO(ikilpatrick): Don't include the block-start margin of a float which
  // has fragmented.
  // EXPECT_EQ(NGPhysicalOffset(LayoutUnit(10), LayoutUnit(0)),
  // child->Offset());
}

// Verifies that we correctly position a new FC block with the Layout
// Opportunity iterator.
TEST_F(NGBlockLayoutAlgorithmTest,
       NewFcBlockWithAdjoiningFloatCollapsesMargins) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        width: 200px; outline: solid purple 1px;
      }
      #float {
        float: left; width: 100px; height: 30px; background: red;
      }
      #new-fc {
        contain: paint; margin-top: 20px; background: purple;
        height: 50px;
      }
    </style>
    <div id="container">
      <div id="float"></div>
      <div id="new-fc"></div>
    </div>
  )HTML");

  const NGPhysicalBoxFragment* body_fragment;
  const NGPhysicalBoxFragment* container_fragment;
  const NGPhysicalBoxFragment* new_fc_fragment;
  scoped_refptr<const NGPhysicalBoxFragment> fragment;
  auto run_test = [&](const Length& block_width) {
    Element* new_fc_block = GetDocument().getElementById("new-fc");
    new_fc_block->MutableComputedStyle()->SetWidth(block_width);
    new_fc_block->GetLayoutObject()->SetNeedsLayout("");
    std::tie(fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
        GetDocument().getElementsByTagName("html")->item(0));
    ASSERT_EQ(1UL, fragment->Children().size());
    body_fragment = ToNGPhysicalBoxFragment(fragment->Children()[0].get());
    container_fragment =
        ToNGPhysicalBoxFragment(body_fragment->Children()[0].get());
    ASSERT_EQ(2UL, container_fragment->Children().size());
    new_fc_fragment =
        ToNGPhysicalBoxFragment(container_fragment->Children()[1].get());
  };

  // #new-fc is small enough to fit on the same line with #float.
  run_test(Length(80, kFixed));
  // 100 = float's width, 0 = no margin collapsing
  EXPECT_THAT(new_fc_fragment->Offset(),
              NGPhysicalOffset(LayoutUnit(100), LayoutUnit(0)));
  // 8 = body's margins, 20 = new-fc's margin top(20) collapses with
  // body's margin(8)
  EXPECT_THAT(body_fragment->Offset(),
              NGPhysicalOffset(LayoutUnit(8), LayoutUnit(20)));

  // #new-fc is too wide to be positioned on the same line with #float
  run_test(Length(120, kFixed));
  // 30 = #float's height
  EXPECT_THAT(new_fc_fragment->Offset(),
              NGPhysicalOffset(LayoutUnit(0), LayoutUnit(30)));
  // 8 = body's margins, no margin collapsing
  EXPECT_THAT(body_fragment->Offset(),
              NGPhysicalOffset(LayoutUnit(8), LayoutUnit(8)));
}

TEST_F(NGBlockLayoutAlgorithmTest, NewFcAvoidsFloats) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        width: 200px;
      }
      #float {
        float: left; width: 100px; height: 30px; background: red;
      }
      #fc {
        width: 150px; height: 120px; display: flow-root;
      }
    </style>
    <div id="container">
      <div id="float"></div>
      <div id="fc"></div>
    </div>
  )HTML");

  NGBlockNode node(ToLayoutBlockFlow(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));

  scoped_refptr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, *space).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(200), LayoutUnit(150)), fragment->Size());

  FragmentChildIterator iterator(ToNGPhysicalBoxFragment(fragment.get()));

  const NGPhysicalBoxFragment* child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(100), LayoutUnit(30)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(0)), child->Offset());

  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(120)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(30)), child->Offset());
}

TEST_F(NGBlockLayoutAlgorithmTest, ZeroBlockSizeAboveEdge) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container { width: 200px; display: flow-root; }
      #inflow { width: 50px; height: 50px; background: red; margin-top: -70px; }
      #zero { width: 70px; margin: 10px 0 30px 0; }
    </style>
    <div id="container">
      <div id="inflow"></div>
      <div id="zero"></div>
    </div>
  )HTML");

  NGBlockNode node(ToLayoutBlockFlow(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false, true);

  scoped_refptr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, *space).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(200), LayoutUnit(10)), fragment->Size());

  FragmentChildIterator iterator(ToNGPhysicalBoxFragment(fragment.get()));

  const NGPhysicalBoxFragment* child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(50), LayoutUnit(50)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(-70)), child->Offset());

  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(70), LayoutUnit(0)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(-10)), child->Offset());
}

TEST_F(NGBlockLayoutAlgorithmTest, NewFcFirstChildIsZeroBlockSize) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container { width: 200px; display: flow-root; }
      #zero1 { width: 50px; margin-top: -30px; margin-bottom: 10px; }
      #zero2 { width: 70px; margin-top: 20px; margin-bottom: -40px; }
      #inflow { width: 90px; height: 20px; margin-top: 30px; }
    </style>
    <div id="container">
      <div id="zero1"></div>
      <div id="zero2"></div>
      <div id="inflow"></div>
    </div>
  )HTML");

  NGBlockNode node(ToLayoutBlockFlow(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          WritingMode::kHorizontalTb, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false, true);

  scoped_refptr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, *space).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(200), LayoutUnit(10)), fragment->Size());

  FragmentChildIterator iterator(ToNGPhysicalBoxFragment(fragment.get()));

  const NGPhysicalBoxFragment* child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(50), LayoutUnit(0)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(-30)), child->Offset());

  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(70), LayoutUnit(0)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(-10)), child->Offset());

  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(90), LayoutUnit(20)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(-10)), child->Offset());
}

// This test assumes that tables are not yet implemented in LayoutNG.
TEST_F(NGBlockLayoutAlgorithmTest, RootFragmentOffsetInsideLegacy) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div style="display:table-cell;">
      <div id="innerNGRoot" style="margin-top:10px; margin-left:20px;"></div>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  const LayoutObject* innerNGRoot = GetLayoutObjectByElementId("innerNGRoot");

  ASSERT_TRUE(innerNGRoot->IsLayoutNGMixin());
  const NGPhysicalBoxFragment* fragment =
      CurrentFragmentFor(ToLayoutNGBlockFlow(innerNGRoot));

  ASSERT_TRUE(fragment);
  // TODO(crbug.com/781241: Re-enable when we calculate inline offset at
  // the right time.
  // EXPECT_EQ(NGPhysicalOffset(LayoutUnit(20), LayoutUnit(10)),
  //          fragment->Offset());
}

}  // namespace
}  // namespace blink
