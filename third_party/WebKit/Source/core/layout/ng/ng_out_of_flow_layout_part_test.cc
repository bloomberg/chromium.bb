// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_out_of_flow_layout_part.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_layout_result.h"

namespace blink {
namespace {

class NGOutOfFlowLayoutPartTest : public RenderingTest {
 public:
  NGOutOfFlowLayoutPartTest() {
    RuntimeEnabledFeatures::SetLayoutNGEnabled(true);
  };
  ~NGOutOfFlowLayoutPartTest() {
    RuntimeEnabledFeatures::SetLayoutNGEnabled(false);
  };
};

// Fixed blocks inside absolute blocks trigger otherwise unused while loop
// inside NGOutOfFlowLayoutPart::Run.
// This test exercises this loop by placing two fixed elements inside abs.
TEST_F(NGOutOfFlowLayoutPartTest, FixedInsideAbs) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        body{ padding:0px; margin:0px}
        #rel { position:relative }
        #abs {
          position: absolute;
          top:49px;
          left:0px;
        }
        #pad {
          width:100px;
          height:50px;
        }
        #fixed1 {
          position:fixed;
          width:50px;
        }
        #fixed2 {
          position:fixed;
          top:9px;
          left:7px;
        }
      </style>
      <div id='rel'>
        <div id='abs'>
          <div id='pad'></div>
          <div id='fixed1'>
            <p>fixed static</p>
          </div>
          <div id='fixed2'>
            <p>fixed plain</p>
          </div>
        </div>
      </div>
      )HTML");

  // Test whether the oof fragments have been collected at NG->Legacy boundary.
  Element* rel = GetDocument().getElementById("rel");
  LayoutNGBlockFlow* block_flow = ToLayoutNGBlockFlow(rel->GetLayoutObject());
  RefPtr<NGConstraintSpace> space =
      NGConstraintSpace::CreateFromLayoutObject(*block_flow);
  NGBlockNode node(block_flow);
  RefPtr<NGLayoutResult> result = node.Layout(space.Get());
  EXPECT_EQ(result->OutOfFlowPositionedDescendants().size(), (size_t)2);

  // Test the final result.
  Element* fixed_1 = GetDocument().getElementById("fixed1");
  Element* fixed_2 = GetDocument().getElementById("fixed2");
  // fixed1 top is static: #abs.top + #pad.height
  EXPECT_EQ(fixed_1->OffsetTop(), LayoutUnit(99));
  // fixed2 top is positioned: #fixed2.top
  EXPECT_EQ(fixed_2->OffsetTop(), LayoutUnit(9));
};

TEST_F(NGOutOfFlowLayoutPartTest, OrthogonalWritingMode1) {
  SetBodyInnerHTML(
      R"HTML(
    <style>
      #container {
        position: relative;
        writing-mode: horizontal-tb;
        width: 200px;
        height: 400px;
      }
      #abs-child {
        position: absolute;
        top: 10px;
        writing-mode: vertical-rl;
        width: auto;
        height: 30px;
      }
    </style>
    <div id="container">
      <div id="abs-child"></div>
    </div>
    )HTML");

  LayoutNGBlockFlow* block_flow =
      ToLayoutNGBlockFlow(GetLayoutObjectByElementId("container"));
  NGBlockNode node(block_flow);
  RefPtr<NGConstraintSpace> space =
      NGConstraintSpace::CreateFromLayoutObject(*block_flow);

  RefPtr<const NGPhysicalFragment> fragment =
      node.Layout(space.Get())->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(200), LayoutUnit(400)), fragment->Size());

  fragment = ToNGPhysicalBoxFragment(fragment.Get())->Children()[0];
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(0), LayoutUnit(30)), fragment->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(10)),
            fragment->Offset());
};

TEST_F(NGOutOfFlowLayoutPartTest, OrthogonalWritingMode2) {
  SetBodyInnerHTML(
      R"HTML(
    <style>
      #container {
        position: relative;
        writing-mode: horizontal-tb;
        width: 200px;
        height: 400px;
      }
      #abs-child {
        position: absolute;
        top: 10px;
        writing-mode: vertical-rl;
        width: 20%;
        height: 30px;
      }
    </style>
    <div id="container">
      <div id="abs-child"></div>
    </div>
    )HTML");

  LayoutNGBlockFlow* block_flow =
      ToLayoutNGBlockFlow(GetLayoutObjectByElementId("container"));
  NGBlockNode node(block_flow);
  RefPtr<NGConstraintSpace> space =
      NGConstraintSpace::CreateFromLayoutObject(*block_flow);

  RefPtr<const NGPhysicalFragment> fragment =
      node.Layout(space.Get())->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(200), LayoutUnit(400)), fragment->Size());

  fragment = ToNGPhysicalBoxFragment(fragment.Get())->Children()[0];
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(40), LayoutUnit(30)), fragment->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(10)),
            fragment->Offset());
};

}  // namespace
}  // namespace blink
