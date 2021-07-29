// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {
namespace {

class NGOutOfFlowLayoutPartTest
    : public NGBaseLayoutAlgorithmTest,
      private ScopedLayoutNGBlockFragmentationForTest {
 protected:
  NGOutOfFlowLayoutPartTest() : ScopedLayoutNGBlockFragmentationForTest(true) {}

  scoped_refptr<const NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(
      Element* element) {
    NGBlockNode container(element->GetLayoutBox());
    NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
        {WritingMode::kHorizontalTb, TextDirection::kLtr},
        LogicalSize(LayoutUnit(1000), kIndefiniteSize));
    return NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(container, space);
  }

  String DumpFragmentTree(Element* element) {
    auto fragment = RunBlockLayoutAlgorithm(element);
    return DumpFragmentTree(fragment.get());
  }

  String DumpFragmentTree(const blink::NGPhysicalBoxFragment* fragment) {
    NGPhysicalFragment::DumpFlags flags =
        NGPhysicalFragment::DumpHeaderText | NGPhysicalFragment::DumpSubtree |
        NGPhysicalFragment::DumpIndentation | NGPhysicalFragment::DumpOffset |
        NGPhysicalFragment::DumpSize;

    return fragment->DumpFragmentTree(flags);
  }
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
  auto* block_flow = To<LayoutBlockFlow>(rel->GetLayoutObject());
  scoped_refptr<const NGLayoutResult> result =
      block_flow->GetCachedLayoutResult();
  EXPECT_TRUE(result);
  EXPECT_EQ(result->PhysicalFragment().OutOfFlowPositionedDescendants().size(),
            2u);

  // Test the final result.
  Element* fixed_1 = GetDocument().getElementById("fixed1");
  Element* fixed_2 = GetDocument().getElementById("fixed2");
  // fixed1 top is static: #abs.top + #pad.height
  EXPECT_EQ(fixed_1->OffsetTop(), LayoutUnit(99));
  // fixed2 top is positioned: #fixed2.top
  EXPECT_EQ(fixed_2->OffsetTop(), LayoutUnit(9));
}

// Tests non-fragmented positioned nodes inside a multi-column.
TEST_F(NGOutOfFlowLayoutPartTest, PositionedInMulticol) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count: 2; height: 40px; column-fill: auto; column-gap: 16px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position: absolute;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div style="width:100px; height:50px;"></div>
          <div class="rel" style="width:30px;">
            <div class="abs" style="width:5px; top:10px; height:5px;">
            </div>
            <div class="rel" style="width:35px; padding-top:8px;">
              <div class="abs" style="width:10px; top:20px; height:10px;">
              </div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:100x40
      offset:508,0 size:492x40
        offset:0,0 size:100x10
        offset:0,10 size:30x8
          offset:0,0 size:35x8
        offset:0,30 size:10x10
        offset:0,20 size:5x5
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that positioned nodes fragment correctly.
TEST_F(NGOutOfFlowLayoutPartTest, SimplePositionedFragmentation) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; top:0px; width:5px; height:50px;
          border:solid 2px; margin-top:5px; padding:5px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div style="width:100px; height:50px;"></div>
          <div class="rel">
            <div class="abs"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:100x40
      offset:508,0 size:492x40
        offset:0,0 size:100x10
        offset:0,10 size:30x0
        offset:0,15 size:19x25
      offset:1016,0 size:492x40
        offset:0,0 size:19x39
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests fragmentation when a positioned node's child overflows.
TEST_F(NGOutOfFlowLayoutPartTest, PositionedFragmentationWithOverflow) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; top:10px; width:5px; height:10px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div class="abs">
              <div style="width:100px; height:50px;"></div>
            </div>
          </div>
          <div style="width:20px; height:100px;"></div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:30x0
        offset:0,0 size:20x40
        offset:0,10 size:5x10
          offset:0,0 size:100x30
      offset:508,0 size:492x40
        offset:0,0 size:20x40
        offset:0,0 size:5x0
          offset:0,0 size:100x20
      offset:1016,0 size:492x40
        offset:0,0 size:20x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that new column fragments are added correctly if a positioned node
// fragments beyond the last fragmentainer in a context.
TEST_F(NGOutOfFlowLayoutPartTest, PositionedFragmentationWithNewColumns) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; width:5px; height:120px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div class="abs"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:30x0
        offset:0,0 size:5x40
      offset:508,0 size:492x40
        offset:0,0 size:5x40
      offset:1016,0 size:492x40
        offset:0,0 size:5x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that empty column fragments are added if an OOF element begins layout
// in a fragmentainer that is more than one index beyond the last existing
// column fragmentainer.
TEST_F(NGOutOfFlowLayoutPartTest, PositionedFragmentationWithNewEmptyColumns) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; top:80px; width:5px; height:120px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div class="abs"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:30x0
      offset:508,0 size:492x40
      offset:1016,0 size:492x40
        offset:0,0 size:5x40
      offset:1524,0 size:492x40
        offset:0,0 size:5x40
      offset:2032,0 size:492x40
        offset:0,0 size:5x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Break-inside does not apply to absolute positioned elements.
TEST_F(NGOutOfFlowLayoutPartTest, BreakInsideAvoid) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position:relative;
        }
        .abs {
          position:absolute; break-inside:avoid;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div style="width:20px; height:10px;"></div>
          <div class="rel" style="width:30px;">
            <div class="abs" style="width:40px; height:40px;"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:20x10
        offset:0,10 size:30x0
        offset:0,10 size:40x30
      offset:508,0 size:492x40
        offset:0,0 size:40x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Break-before does not apply to absolute positioned elements.
TEST_F(NGOutOfFlowLayoutPartTest, BreakBeforeColumn) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute; break-before:column;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div style="width:10px; height:30px;"></div>
          <div class="rel" style="width:30px;">
            <div class="abs" style="width:40px; height:30px;"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:10x30
        offset:0,30 size:30x0
        offset:0,30 size:40x10
      offset:508,0 size:492x40
        offset:0,0 size:40x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Break-after does not apply to absolute positioned elements.
TEST_F(NGOutOfFlowLayoutPartTest, BreakAfterColumn) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute; break-after:column;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div style="width:10px; height:20px;"></div>
          <div class="rel" style="width:30px; height:10px;">
            <div class="abs" style="width:40px; height:10px;"></div>
          </div>
          <div style="width:20px; height:10px;"></div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:10x20
        offset:0,20 size:30x10
        offset:0,30 size:20x10
        offset:0,20 size:40x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Break-inside should still apply to children of absolute positioned elements.
TEST_F(NGOutOfFlowLayoutPartTest, ChildBreakInsideAvoid) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:100px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel" style="width:30px;">
            <div class="abs" style="width:40px; height:150px;">
              <div style="width:15px; height:50px;"></div>
              <div style="break-inside:avoid; width:20px; height:100px;"></div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:1000x100
      offset:0,0 size:492x100
        offset:0,0 size:30x0
        offset:0,0 size:40x100
          offset:0,0 size:15x50
      offset:508,0 size:492x100
        offset:0,0 size:40x50
          offset:0,0 size:20x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Break-before should still apply to children of absolute positioned elements.
TEST_F(NGOutOfFlowLayoutPartTest, ChildBreakBeforeAvoid) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:100px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel" style="width:30px;">
            <div class="abs" style="width:40px; height:150px;">
              <div style="width:15px; height:50px;"></div>
              <div style="width:20px; height:50px;"></div>
              <div style="break-before:avoid; width:10px; height:20px;"></div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:1000x100
      offset:0,0 size:492x100
        offset:0,0 size:30x0
        offset:0,0 size:40x100
          offset:0,0 size:15x50
      offset:508,0 size:492x100
        offset:0,0 size:40x50
          offset:0,0 size:20x50
          offset:0,50 size:10x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Break-after should still apply to children of absolute positioned elements.
TEST_F(NGOutOfFlowLayoutPartTest, ChildBreakAfterAvoid) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:100px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel" style="width:30px;">
            <div class="abs" style="width:40px; height:150px;">
              <div style="width:15px; height:50px;"></div>
              <div style="break-after:avoid; width:20px; height:50px;"></div>
              <div style="width:10px; height:20px;"></div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:1000x100
      offset:0,0 size:492x100
        offset:0,0 size:30x0
        offset:0,0 size:40x100
          offset:0,0 size:15x50
      offset:508,0 size:492x100
        offset:0,0 size:40x50
          offset:0,0 size:20x50
          offset:0,50 size:10x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that a positioned element with a negative top property moves the OOF
// node to the previous fragmentainer and spans 3 columns.
TEST_F(NGOutOfFlowLayoutPartTest,
       PositionedFragmentationWithNegativeTopPropertyAndNewEmptyColumn) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; top:-40px; width:5px; height:80px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div style="height: 60px; width: 32px;"></div>
          <div class="rel">
            <div class="abs"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:32x40
        offset:0,20 size:5x20
      offset:508,0 size:492x40
        offset:0,0 size:32x20
        offset:0,0 size:5x40
        offset:0,20 size:30x0
      offset:1016,0 size:492x40
        offset:0,0 size:5x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGOutOfFlowLayoutPartTest, PositionedFragmentationWithBottomProperty) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute; bottom:10px; width:5px; height:40px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel" style="height: 60px; width: 32px;">
            <div class="abs"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:32x40
        offset:0,10 size:5x30
      offset:508,0 size:492x40
        offset:0,0 size:32x20
        offset:0,0 size:5x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that a positioned element without a top or bottom property stays in
// flow - even though it's treated as an OOF element.
TEST_F(NGOutOfFlowLayoutPartTest,
       PositionedFragmentationInFlowWithAddedColumns) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position:relative; width:30px;
        }
        .abs {
          position:absolute; width:5px; height:80px;
        }
       </style>
       <div id="container">
         <div id="multicol">
           <div class="rel">
             <div style="height: 60px; width: 32px;"></div>
             <div class="abs"></div>
           </div>
         </div>
       </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:30x40
          offset:0,0 size:32x40
      offset:508,0 size:492x40
        offset:0,0 size:30x20
          offset:0,0 size:32x20
        offset:0,20 size:5x20
      offset:1016,0 size:492x40
        offset:0,0 size:5x40
      offset:1524,0 size:492x40
        offset:0,0 size:5x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that the fragments of a positioned element are added to the right
// fragmentainer despite the presence of column spanners.
TEST_F(NGOutOfFlowLayoutPartTest, PositionedFragmentationAndColumnSpanners) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position:relative; width:30px;
        }
        .abs {
          position:absolute; width:5px; height:20px;
        }
       </style>
       <div id="container">
         <div id="multicol">
           <div class="rel">
             <div style="column-span:all;"></div>
             <div style="height: 60px; width: 32px;"></div>
             <div style="column-span:all;"></div>
             <div class="abs"></div>
           </div>
         </div>
       </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x0
        offset:0,0 size:30x0
      offset:0,0 size:1000x0
      offset:0,0 size:492x30
        offset:0,0 size:30x30
          offset:0,0 size:32x30
      offset:508,0 size:492x30
        offset:0,0 size:30x30
          offset:0,0 size:32x30
      offset:0,30 size:1000x0
      offset:0,30 size:492x10
        offset:0,0 size:30x0
        offset:0,0 size:5x10
      offset:508,30 size:492x10
        offset:0,0 size:5x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that column spanners are skipped over when laying out fragmented abspos
// elements.
TEST_F(NGOutOfFlowLayoutPartTest, PositionedFragmentationWithNestedSpanner) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; width:5px; height:50px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div style="column-span:all;"></div>
            <div class="abs"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x0
        offset:0,0 size:30x0
      offset:0,0 size:1000x0
      offset:0,0 size:492x40
        offset:0,0 size:30x0
        offset:0,0 size:5x40
      offset:508,0 size:492x40
        offset:0,0 size:5x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that column spanners are skipped over when laying out fragmented abspos
// elements.
TEST_F(NGOutOfFlowLayoutPartTest, PositionedFragmentationWithNestedSpanners) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; width:5px; height:50px;
        }
        .content { height:20px; }
      </style>
      <div id="container">
        <div id="multicol">
          <div style="column-span:all;"></div>
          <div class="rel">
            <div class="content"></div>
            <div style="column-span:all;"></div>
            <div style="column-span:all;"></div>
            <div style="column-span:all;"></div>
            <div class="abs"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x0
      offset:0,0 size:1000x0
      offset:0,0 size:492x10
        offset:0,0 size:30x10
          offset:0,0 size:30x10
      offset:508,0 size:492x10
        offset:0,0 size:30x10
          offset:0,0 size:30x10
      offset:0,10 size:1000x0
      offset:0,10 size:1000x0
      offset:0,10 size:1000x0
      offset:0,10 size:492x30
        offset:0,0 size:30x0
        offset:0,0 size:5x30
      offset:508,10 size:492x30
        offset:0,0 size:5x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that abspos elements bubble up to their containing block when nested
// inside of a spanner.
TEST_F(NGOutOfFlowLayoutPartTest, AbsposInSpanner) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute; width:5px; height:50px; top:5px;
        }
      </style>
      <div id="container">
        <div class="rel" style="width:50px;">
          <div id="multicol">
            <div class="rel" style="width:30px;">
              <div style="width:10px; height:30px;"></div>
              <div>
                <div style="column-span:all;">
                  <div class="abs"></div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:50x40
      offset:0,0 size:50x40
        offset:0,0 size:17x15
          offset:0,0 size:30x15
            offset:0,0 size:10x15
        offset:33,0 size:17x15
          offset:0,0 size:30x15
            offset:0,0 size:10x15
            offset:0,15 size:30x0
        offset:0,15 size:50x0
        offset:0,15 size:17x25
          offset:0,0 size:30x0
            offset:0,0 size:30x0
      offset:0,5 size:5x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that abspos elements bubble up to their containing block when nested
// inside of a spanner and get the correct static position.
TEST_F(NGOutOfFlowLayoutPartTest, AbsposInSpannerStaticPos) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative;
        }
        .abs {
          position:absolute; width:5px; height:50px;
        }
      </style>
      <div id="container">
        <div class="rel" style="width:50px;">
          <div id="multicol">
            <div class="rel" style="width:30px;">
              <div style="width:10px; height:30px;"></div>
              <div style="column-span:all; margin-top:5px;">
                <div style="width:20px; height:5px;"></div>
                <div class="abs"></div>
              </div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:50x40
      offset:0,0 size:50x40
        offset:0,0 size:17x15
          offset:0,0 size:30x15
            offset:0,0 size:10x15
        offset:33,0 size:17x15
          offset:0,0 size:30x15
            offset:0,0 size:10x15
        offset:0,20 size:50x5
          offset:0,0 size:20x5
        offset:0,25 size:17x15
          offset:0,0 size:30x0
      offset:0,25 size:5x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests fragmented abspos elements with a spanner nested inside.
TEST_F(NGOutOfFlowLayoutPartTest, SpannerInAbspos) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; width:5px; height:50px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div class="abs">
              <div style="column-span:all;"></div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:30x0
        offset:0,0 size:5x40
          offset:0,0 size:5x0
      offset:508,0 size:492x40
        offset:0,0 size:5x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that new column fragments are added correctly if a positioned node
// fragments beyond the last fragmentainer in a context in the presence of a
// spanner.
TEST_F(NGOutOfFlowLayoutPartTest,
       PositionedFragmentationWithNewColumnsAndSpanners) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; width:5px; height:120px; top:0px;
        }
        .content { height:20px; }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div class="content"></div>
            <div class="abs"></div>
          </div>
          <div style="column-span:all;"></div>
          <div style="column-span:all;"></div>
          <div style="column-span:all;"></div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x10
        offset:0,0 size:30x10
          offset:0,0 size:30x10
        offset:0,0 size:5x10
      offset:508,0 size:492x10
        offset:0,0 size:30x10
          offset:0,0 size:30x10
        offset:0,0 size:5x10
      offset:0,10 size:1000x0
      offset:0,10 size:1000x0
      offset:0,10 size:1000x0
      offset:0,10 size:492x30
        offset:0,0 size:5x30
      offset:508,10 size:492x30
        offset:0,0 size:5x30
      offset:1016,10 size:492x30
        offset:0,0 size:5x30
      offset:1524,10 size:492x30
        offset:0,0 size:5x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that new column fragments are added correctly if a positioned node
// fragments beyond the last fragmentainer in a context directly after a
// spanner.
TEST_F(NGOutOfFlowLayoutPartTest,
       PositionedFragmentationWithNewColumnsAfterSpanner) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; width:5px; height:50px; top:25px;
        }
        .content { height:20px; }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div class="content"></div>
            <div class="abs"></div>
          </div>
          <div style="column-span:all;"></div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x10
        offset:0,0 size:30x10
          offset:0,0 size:30x10
      offset:508,0 size:492x10
        offset:0,0 size:30x10
          offset:0,0 size:30x10
      offset:0,10 size:1000x0
      offset:0,10 size:492x30
        offset:0,5 size:5x25
      offset:508,10 size:492x30
        offset:0,0 size:5x25
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that new column fragments are added correctly if a positioned node
// fragments beyond the last fragmentainer in a context in the presence of a
// spanner.
TEST_F(NGOutOfFlowLayoutPartTest,
       AbsposFragWithSpannerAndNewColumnsAutoHeight) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; width:5px; height:4px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div class="abs"></div>
          </div>
          <div style="column-span:all;"></div>
          <div style="column-span:all;"></div>
          <div style="column-span:all;"></div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x0
    offset:0,0 size:1000x0
      offset:0,0 size:492x0
        offset:0,0 size:30x0
        offset:0,0 size:5x1
      offset:0,0 size:1000x0
      offset:0,0 size:1000x0
      offset:0,0 size:1000x0
      offset:0,0 size:492x0
        offset:0,0 size:5x1
      offset:508,0 size:492x0
        offset:0,0 size:5x1
      offset:1016,0 size:492x0
        offset:0,0 size:5x1
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that empty column fragments are added if an OOF element begins layout
// in a fragmentainer that is more than one index beyond the last existing
// column fragmentainer in the presence of a spanner.
TEST_F(NGOutOfFlowLayoutPartTest, AbsposFragWithSpannerAndNewEmptyColumns) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; top:80px; width:5px; height:120px;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div class="abs"></div>
          </div>
          <div style="column-span:all;"></div>
          <div style="column-span:all;"></div>
          <div style="column-span:all;"></div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x0
        offset:0,0 size:30x0
      offset:0,0 size:1000x0
      offset:0,0 size:1000x0
      offset:0,0 size:1000x0
      offset:0,0 size:492x40
      offset:508,0 size:492x40
        offset:0,39 size:5x1
      offset:1016,0 size:492x40
        offset:0,0 size:5x40
      offset:1524,0 size:492x40
        offset:0,0 size:5x40
      offset:2032,0 size:492x40
        offset:0,0 size:5x39
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Fragmented OOF element with block-size percentage resolution.
TEST_F(NGOutOfFlowLayoutPartTest, AbsposFragmentationPctResolution) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position: relative; width:30px;
        }
        .abs {
          position:absolute; top:30px; width:5px; height:100%;
        }
        .spanner {
          column-span:all; height:25%;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div class="abs"></div>
            <div style="width: 10px; height:30px;"></div>
          </div>
          <div class="spanner"></div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x15
        offset:0,0 size:30x15
          offset:0,0 size:10x15
      offset:508,0 size:492x15
        offset:0,0 size:30x15
          offset:0,0 size:10x15
      offset:0,15 size:1000x10
      offset:0,25 size:492x15
        offset:0,0 size:5x15
      offset:508,25 size:492x15
        offset:0,0 size:5x15
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Fragmented OOF element with block-size percentage resolution and overflow.
TEST_F(NGOutOfFlowLayoutPartTest,
       AbsposFragmentationPctResolutionWithOverflow) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          columns:5; column-fill:auto; column-gap:0px; height:100px;
        }
        .rel {
          position: relative; width:55px;
        }
        .abs {
          position:absolute; top:0px; width:5px; height:100%;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div style="height:30px;"></div>
          <div class="rel">
            <div class="abs"></div>
            <div style="width:44px; height:200px;">
              <div style="width:33px; height:400px;"></div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:1000x100
      offset:0,0 size:200x100
        offset:0,0 size:200x30
        offset:0,30 size:55x70
          offset:0,0 size:44x70
            offset:0,0 size:33x70
        offset:0,30 size:5x70
      offset:200,0 size:200x100
        offset:0,0 size:55x100
          offset:0,0 size:44x100
            offset:0,0 size:33x100
        offset:0,0 size:5x100
      offset:400,0 size:200x100
        offset:0,0 size:55x30
          offset:0,0 size:44x30
            offset:0,0 size:33x100
        offset:0,0 size:5x30
      offset:600,0 size:200x100
        offset:0,0 size:55x0
          offset:0,0 size:44x0
            offset:0,0 size:33x100
      offset:800,0 size:200x100
        offset:0,0 size:55x0
          offset:0,0 size:44x0
            offset:0,0 size:33x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Fragmented OOF element inside a nested multi-column.
TEST_F(NGOutOfFlowLayoutPartTest, SimpleAbsposNestedFragmentation) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        .multicol {
          columns:2; column-fill:auto; column-gap:0px;
        }
        .rel {
          position: relative; width:55px; height:80px;
        }
        .abs {
          position:absolute; top:0px; width:5px; height:80px;
        }
      </style>
      <div id="container">
        <div class="multicol" id="outer" style="height:100px;">
          <div style="height:40px; width:40px;"></div>
          <div class="multicol" id="inner">
            <div class="rel">
              <div class="abs"></div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:1000x100
      offset:0,0 size:500x100
        offset:0,0 size:40x40
        offset:0,40 size:500x60
          offset:0,0 size:250x60
            offset:0,0 size:55x60
            offset:0,0 size:5x60
          offset:250,0 size:250x60
            offset:0,0 size:55x20
            offset:0,0 size:5x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Fragmented OOF element inside a nested multi-column with new columns.
TEST_F(NGOutOfFlowLayoutPartTest, AbsposNestedFragmentationNewColumns) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        .multicol {
          columns:2; column-fill:auto; column-gap:0px;
        }
        #inner {
          column-gap:16px; height:40px; padding:10px;
        }
        .rel {
          position: relative; width:55px; height:20px;
        }
        .abs {
          position:absolute; top:0px; width:5px; height:40px;
        }
      </style>
      <div id="container">
        <div class="multicol" id="outer" style="height:100px;">
          <div style="height:40px; width:40px;"></div>
          <div class="multicol" id="inner">
            <div class="rel">
              <div class="abs"></div>
            </div>
            <div style="column-span:all;"></div>
            <div style="column-span:all;"></div>
            <div style="column-span:all;"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  // Note that it's not obvious that the block-size of the last inner
  // fragmentainer (after the spanners) is correct; see crbug.com/1224337
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:1000x100
      offset:0,0 size:500x100
        offset:0,0 size:40x40
        offset:0,40 size:500x60
          offset:10,10 size:232x10
            offset:0,0 size:55x10
            offset:0,0 size:5x10
          offset:258,10 size:232x10
            offset:0,0 size:55x10
            offset:0,0 size:5x10
          offset:10,20 size:480x0
          offset:10,20 size:480x0
          offset:10,20 size:480x0
          offset:10,20 size:232x40
            offset:0,0 size:5x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Fragmented OOF element inside a nested multi-column starting at a
// fragmentainer index beyond the last existing fragmentainer.
TEST_F(NGOutOfFlowLayoutPartTest, AbsposNestedFragmentationNewEmptyColumns) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        .multicol {
          columns:2; column-fill:auto; column-gap:0px;
        }
        .rel {
          position: relative; width:55px; height:80px;
        }
        .abs {
          position:absolute; top:120px; width:5px; height:120px;
        }
      </style>
      <div id="container">
        <div class="multicol" id="outer" style="height:100px;">
          <div style="height:40px; width:40px;"></div>
          <div class="multicol" id="inner" style="column-gap:16px;">
            <div class="rel">
              <div class="abs"></div>
            </div>
            <div style="column-span:all;"></div>
            <div style="column-span:all;"></div>
            <div style="column-span:all;"></div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  // Note that it's not obvious that the block-size of the last inner
  // fragmentainers (after the spanners) are correct; see crbug.com/1224337
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:1000x100
      offset:0,0 size:500x100
        offset:0,0 size:40x40
        offset:0,40 size:500x40
          offset:0,0 size:242x40
            offset:0,0 size:55x40
          offset:258,0 size:242x40
            offset:0,0 size:55x40
          offset:0,40 size:500x0
          offset:0,40 size:500x0
          offset:0,40 size:500x0
          offset:0,40 size:242x40
          offset:258,40 size:242x40
            offset:0,0 size:5x40
          offset:516,40 size:242x40
            offset:0,0 size:5x40
          offset:774,40 size:242x40
            offset:0,0 size:5x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Fragmented OOF with `height: auto` and positioned with the bottom property.
TEST_F(NGOutOfFlowLayoutPartTest,
       PositionedFragmentationWithBottomPropertyAndHeightAuto) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count:2; column-fill:auto; column-gap:16px; height:40px;
        }
        .rel {
          position:relative; height:60px; width:32px;
        }
        .abs {
          position:absolute; bottom:0; width:5px; height:auto;
        }
      </style>
      <div id="container">
        <div id="multicol">
          <div class="rel">
            <div class="abs">
              <div style="width: 2px; height: 10px"></div>
              <div style="width: 3px; height: 20px"></div>
              <div style="width: 4px; height: 10px"></div>
            </div>
          </div>
        </div>
      </div>
      )HTML");
  String dump = DumpFragmentTree(GetElementById("container"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:1000x40
      offset:0,0 size:492x40
        offset:0,0 size:32x40
        offset:0,20 size:5x20
          offset:0,0 size:2x10
          offset:0,10 size:3x10
      offset:508,0 size:492x40
        offset:0,0 size:32x20
        offset:0,0 size:5x20
          offset:0,0 size:3x10
          offset:0,10 size:4x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGOutOfFlowLayoutPartTest, PositionedObjectsInMulticol) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        .multicol {
          column-count: 2; column-fill: auto; column-gap: 0px;
        }
      </style>
      <div class="multicol" id="outer">
        <div class="multicol" id="inner" style="position:relative;">
          <div id="abs1" style="position:absolute;"></div>
          <div id="rel" style="position:relative;">
            <div id="abs2" style="position:absolute;"></div>
          </div>
        </div>
      </div>
      )HTML");
  Element* outer_multicol = GetDocument().getElementById("outer");
  auto* multicol = To<LayoutBlockFlow>(outer_multicol->GetLayoutObject());
  EXPECT_FALSE(multicol->PositionedObjects());

  Element* inner_multicol = GetDocument().getElementById("inner");
  multicol = To<LayoutBlockFlow>(inner_multicol->GetLayoutObject());
  auto* abs = GetLayoutBoxByElementId("abs1");
  EXPECT_TRUE(multicol->PositionedObjects()->Contains(abs));
  EXPECT_EQ(multicol->PositionedObjects()->size(), 1u);

  Element* rel_element = GetDocument().getElementById("rel");
  auto* rel = To<LayoutBlockFlow>(rel_element->GetLayoutObject());
  abs = GetLayoutBoxByElementId("abs2");
  EXPECT_TRUE(rel->PositionedObjects()->Contains(abs));
  EXPECT_EQ(rel->PositionedObjects()->size(), 1u);
}

TEST_F(NGOutOfFlowLayoutPartTest, PositionedObjectsInMulticolWithInline) {
  SetBodyInnerHTML(
      R"HTML(
      <style>
        #multicol {
          column-count: 2; column-fill: auto; column-gap: 0px;
        }
      </style>
      <div id="multicol">
        <div id="target">
          <span style="position: relative;">
            <div id="abs1" style="position:absolute;"></div>
            <div id="abs2" style="position:absolute;"></div>
          </span>
        </div>
      </div>
      )HTML");
  Element* multicol_element = GetDocument().getElementById("multicol");
  auto* multicol = To<LayoutBlockFlow>(multicol_element->GetLayoutObject());
  EXPECT_FALSE(multicol->PositionedObjects());

  Element* target_element = GetDocument().getElementById("target");
  auto* target = To<LayoutBlockFlow>(target_element->GetLayoutObject());
  auto* abs1 = GetLayoutBoxByElementId("abs1");
  auto* abs2 = GetLayoutBoxByElementId("abs2");
  EXPECT_TRUE(target->PositionedObjects()->Contains(abs1));
  EXPECT_TRUE(target->PositionedObjects()->Contains(abs2));
  EXPECT_EQ(target->PositionedObjects()->size(), 2u);
}
}  // namespace
}  // namespace blink
