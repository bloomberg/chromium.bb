// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_fieldset_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

class NGFieldsetLayoutAlgorithmTest
    : public NGBaseLayoutAlgorithmTest,
      private ScopedLayoutNGFieldsetForTest,
      private ScopedLayoutNGBlockFragmentationForTest {
 protected:
  NGFieldsetLayoutAlgorithmTest()
      : ScopedLayoutNGFieldsetForTest(true),
        ScopedLayoutNGBlockFragmentationForTest(true) {}
  void SetUp() override {
    NGBaseLayoutAlgorithmTest::SetUp();
    style_ = ComputedStyle::Create();
  }

  scoped_refptr<const NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(
      Element* element) {
    NGBlockNode container(ToLayoutBox(element->GetLayoutObject()));
    NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
        WritingMode::kHorizontalTb, TextDirection::kLtr,
        LogicalSize(LayoutUnit(1000), kIndefiniteSize));
    return NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(container, space);
  }

  MinMaxSizes RunComputeMinMaxSizes(NGBlockNode node) {
    NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
        WritingMode::kHorizontalTb, TextDirection::kLtr,
        LogicalSize(LayoutUnit(), LayoutUnit()), false,
        node.CreatesNewFormattingContext());
    NGFragmentGeometry fragment_geometry =
        CalculateInitialMinMaxFragmentGeometry(space, node);

    NGFieldsetLayoutAlgorithm algorithm({node, fragment_geometry, space});
    MinMaxSizesInput input(
        /* percentage_resolution_block_size */ (LayoutUnit()));
    return algorithm.ComputeMinMaxSizes(input).sizes;
  }

  MinMaxSizes RunComputeMinMaxSizes(const char* element_id) {
    Element* element = GetDocument().getElementById(element_id);
    NGBlockNode node(ToLayoutBox(element->GetLayoutObject()));
    return RunComputeMinMaxSizes(node);
  }

  String DumpFragmentTree(const NGPhysicalBoxFragment* fragment) {
    NGPhysicalFragment::DumpFlags flags =
        NGPhysicalFragment::DumpHeaderText | NGPhysicalFragment::DumpSubtree |
        NGPhysicalFragment::DumpIndentation | NGPhysicalFragment::DumpOffset |
        NGPhysicalFragment::DumpSize;

    return fragment->DumpFragmentTree(flags);
  }

  String DumpFragmentTree(Element* element) {
    auto fragment = RunBlockLayoutAlgorithm(element);
    return DumpFragmentTree(fragment.get());
  }

  scoped_refptr<ComputedStyle> style_;
};

TEST_F(NGFieldsetLayoutAlgorithmTest, Empty) {
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset { margin:0; border:3px solid; padding:10px; width:100px; }
    </style>
    <div id="container">
      <fieldset></fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x26
    offset:0,0 size:126x26
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, NoLegend) {
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset { margin:0; border:3px solid; padding:10px; width:100px; }
    </style>
    <div id="container">
      <fieldset>
        <div style="height:100px;"></div>
      </fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x126
    offset:0,0 size:126x126
      offset:3,3 size:120x120
        offset:10,10 size:100x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, Legend) {
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset { margin:0; border:3px solid; padding:10px; width:100px; }
    </style>
    <div id="container">
      <fieldset>
        <legend style="padding:0; width:50px; height:200px;"></legend>
        <div style="height:100px;"></div>
      </fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x323
    offset:0,0 size:126x323
      offset:13,0 size:50x200
      offset:3,200 size:120x120
        offset:10,10 size:100x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, SmallLegendLargeBorder) {
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset { margin:0; border:40px solid; padding:10px; width:100px; }
      legend { padding:0; width:10px; height:10px;
               margin-top:5px; margin-bottom:15px; }
    </style>
    <div id="container">
      <fieldset>
        <legend></legend>
        <div style="height:100px;"></div>
      </fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x200
    offset:0,0 size:200x200
      offset:50,10 size:10x10
      offset:40,40 size:120x120
        offset:10,10 size:100x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, LegendOrthogonalWritingMode) {
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset { margin:0; border:3px solid; padding:10px; width:100px; }
      legend { writing-mode:vertical-rl; padding:0; margin:10px 15px 20px 30px;
               width:10px; height:50px; }
    </style>
    <div id="container">
      <fieldset>
        <legend></legend>
        <div style="height:100px;"></div>
      </fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x203
    offset:0,0 size:126x203
      offset:43,10 size:10x50
      offset:3,80 size:120x120
        offset:10,10 size:100x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, VerticalLr) {
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset { writing-mode:vertical-lr; margin:0; border:3px solid;
                 padding:10px; height:100px; }
      legend { padding:0; margin:10px 15px 20px 30px; width:10px; height:50px; }
    </style>
    <div id="container">
      <fieldset>
        <legend></legend>
        <div style="width:100px;"></div>
      </fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x126
    offset:0,0 size:178x126
      offset:30,23 size:10x50
      offset:55,3 size:120x120
        offset:10,10 size:100x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, VerticalRl) {
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset { writing-mode:vertical-rl; margin:0; border:3px solid;
                 padding:10px; height:100px; }
      legend { padding:0; margin:10px 15px 20px 30px; width:10px; height:50px; }
    </style>
    <div id="container">
      <fieldset>
        <legend></legend>
        <div style="width:100px;"></div>
      </fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x126
    offset:0,0 size:178x126
      offset:153,23 size:10x50
      offset:3,3 size:120x120
        offset:10,10 size:100x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, LegendAutoSize) {
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset { margin:0; border:3px solid; padding:10px; width:100px; }
    </style>
    <div id="container">
      <fieldset>
        <legend style="padding:0;">
          <div style="float:left; width:25px; height:200px;"></div>
          <div style="float:left; width:25px; height:200px;"></div>
        </legend>
        <div style="height:100px;"></div>
      </fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x323
    offset:0,0 size:126x323
      offset:13,0 size:50x200
        offset:0,0 size:25x200
        offset:25,0 size:25x200
      offset:3,200 size:120x120
        offset:10,10 size:100x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, PercentageHeightChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset {
        margin:0; border:3px solid; padding:10px; width:100px; height:100px;
      }
    </style>
    <div id="container">
      <fieldset>
        <legend style="padding:0; width:30px; height:30px;"></legend>
        <div style="height:100%;"></div>
      </fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x126
    offset:0,0 size:126x126
      offset:13,0 size:30x30
      offset:3,30 size:120x93
        offset:10,10 size:100x73
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Disabled because out-of-flow positioned objects enter legacy code (regardless
// of the out-of-flow positioned object being laid out by NG or not). Invoking
// layout on our own outside of the lifecycle machinery will eventually fail a
// CHECK in SubtreeLayoutScope::SubtreeLayoutScope().
TEST_F(NGFieldsetLayoutAlgorithmTest, DISABLED_AbsposChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset {
        position:relative; margin:0; border:3px solid; padding:10px;
        width:100px; height:100px;
      }
    </style>
    <div id="container">
      <fieldset>
        <legend style="padding:0; width:30px; height:30px;"></legend>
        <div style="position:absolute; top:0; right:0; bottom:0; left:0;"></div>
      </fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x126
    offset:0,0 size:126x126
      offset:13,0 size:30x30
      offset:3,30 size:120x93
      offset:3,30 size:120x93
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Used height needs to be adjusted to encompass the legend, if specified height
// requests a lower height than that.
TEST_F(NGFieldsetLayoutAlgorithmTest, ZeroHeight) {
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset {
        margin:0; border:3px solid; padding:10px; width:100px; height:0;
      }
    </style>
    <div id="container">
      <fieldset>
        <legend style="padding:0; width:30px; height:30px;"></legend>
        <div style="height:200px;"></div>
      </fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x53
    offset:0,0 size:126x53
      offset:13,0 size:30x30
      offset:3,30 size:120x20
        offset:10,10 size:100x200
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Used height needs to be adjusted to encompass the legend, if specified height
// requests a lower max-height than that.
TEST_F(NGFieldsetLayoutAlgorithmTest, ZeroMaxHeight) {
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset {
        margin:0; border:3px solid; padding:10px; width:100px; max-height:0;
      }
    </style>
    <div id="container">
      <fieldset>
        <legend style="padding:0; width:30px; height:30px;"></legend>
        <div style="height:200px;"></div>
      </fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x53
    offset:0,0 size:126x53
      offset:13,0 size:30x30
      offset:3,30 size:120x220
        offset:10,10 size:100x200
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Things inside legends and fieldsets are treated as if there was no fieldsets
// and legends involved, as far as the percentage height quirk is concerned.
TEST_F(NGFieldsetLayoutAlgorithmTest, PercentHeightQuirks) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset {
        margin:0; border:3px solid; padding:10px; width:100px;
      }
    </style>
    <div id="container" style="height:200px;">
      <fieldset>
        <legend style="padding:0;">
          <div style="width:100px; height:50%;"></div>
        </legend>
        <div style="width:40px; height:20%;"></div>
      </fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x200
    offset:0,0 size:126x163
      offset:13,0 size:100x100
        offset:0,0 size:100x100
      offset:3,100 size:120x60
        offset:10,10 size:40x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Legends are treated as regular elements, as far as the percentage height
// quirk is concerned.
TEST_F(NGFieldsetLayoutAlgorithmTest, LegendPercentHeightQuirks) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset {
        margin:0; border:3px solid; padding:10px; width:100px;
      }
    </style>
    <div id="container" style="height:200px;">
      <fieldset>
        <legend style="padding:0; width:100px; height:50%;"></legend>
        <div style="width:40px; height:20%;"></div>
      </fieldset>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x200
    offset:0,0 size:126x163
      offset:13,0 size:100x100
      offset:3,100 size:120x60
        offset:10,10 size:40x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// This test makes sure that the fieldset content handles fieldset padding
// when the fieldset is expanded to encompass the legend.
TEST_F(NGFieldsetLayoutAlgorithmTest, FieldsetPaddingWithLegend) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #fieldset {
          border:none; margin:0; padding:10px; width: 150px; height: 100px;
        }
        #legend {
          padding:0px; margin:0; width: 50px; height: 120px;
        }
        #child {
          width: 100px; height: 40px;
        }
      </style>
      <fieldset id="fieldset">
        <legend id="legend"></legend>
        <div id="child"></div>
      </fieldset>
  )HTML");

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext());

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:170x140
    offset:10,0 size:50x120
    offset:0,120 size:170x20
      offset:10,10 size:100x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, MinMax) {
  SetBodyInnerHTML(R"HTML(
    <style>
      fieldset { margin:123px; border:3px solid; padding:10px; width:100px; }
      legend { margin:20px; border:11px solid; padding:7px; }
      .float { float:left; width:50px; height:50px; }
    </style>
    <div id="container">
      <fieldset id="fieldset1"></fieldset>
      <fieldset id="fieldset2">
        <legend></legend>
      </fieldset>
      <fieldset id="fieldset3">
        <legend></legend>
        <div class="float"></div>
        <div class="float"></div>
      </fieldset>
      <fieldset id="fieldset4">
        <legend>
          <div class="float"></div>
          <div class="float"></div>
        </legend>
        <div class="float"></div>
      </fieldset>
      <fieldset id="fieldset5">
        <legend>
          <div class="float"></div>
        </legend>
        <div class="float"></div>
        <div class="float"></div>
        <div class="float"></div>
      </fieldset>
      <fieldset id="fieldset6">
        <div class="float"></div>
        <div class="float"></div>
      </fieldset>
    </div>
  )HTML");

  MinMaxSizes sizes;

  sizes = RunComputeMinMaxSizes("fieldset1");
  EXPECT_EQ(sizes.min_size, LayoutUnit(26));
  EXPECT_EQ(sizes.max_size, LayoutUnit(26));

  sizes = RunComputeMinMaxSizes("fieldset2");
  EXPECT_EQ(sizes.min_size, LayoutUnit(102));
  EXPECT_EQ(sizes.max_size, LayoutUnit(102));

  sizes = RunComputeMinMaxSizes("fieldset3");
  EXPECT_EQ(sizes.min_size, LayoutUnit(102));
  EXPECT_EQ(sizes.max_size, LayoutUnit(126));

  sizes = RunComputeMinMaxSizes("fieldset4");
  EXPECT_EQ(sizes.min_size, LayoutUnit(152));
  EXPECT_EQ(sizes.max_size, LayoutUnit(202));

  sizes = RunComputeMinMaxSizes("fieldset5");
  EXPECT_EQ(sizes.min_size, LayoutUnit(152));
  EXPECT_EQ(sizes.max_size, LayoutUnit(176));

  sizes = RunComputeMinMaxSizes("fieldset6");
  EXPECT_EQ(sizes.min_size, LayoutUnit(76));
  EXPECT_EQ(sizes.max_size, LayoutUnit(126));
}

// Tests that a fieldset won't fragment if it doesn't reach the fragmentation
// line.
TEST_F(NGFieldsetLayoutAlgorithmTest, NoFragmentation) {
  SetBodyInnerHTML(R"HTML(
      <style>
        fieldset {
          border:3px solid; margin:0; padding:10px; width: 150px; height: 100px;
        }
      </style>
      <fieldset id="fieldset"></fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  // We should only have one 176x126 fragment with no fragmentation.
  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  EXPECT_EQ(PhysicalSize(176, 126), fragment->Size());
  ASSERT_FALSE(fragment->BreakToken());
}

// Tests that a fieldset will fragment if it reaches the fragmentation line.
TEST_F(NGFieldsetLayoutAlgorithmTest, SimpleFragmentation) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #fieldset {
          border:3px solid; margin:0; padding:10px; width: 150px; height: 500px;
        }
      </style>
      <fieldset id="fieldset"></fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  EXPECT_EQ(PhysicalSize(176, 200), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken());

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  EXPECT_EQ(PhysicalSize(176, 200), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken());

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  EXPECT_EQ(PhysicalSize(176, 126), fragment->Size());
  ASSERT_FALSE(fragment->BreakToken());
}

// Tests that a fieldset with no content or padding will fragment if it reaches
// the fragmentation line.
TEST_F(NGFieldsetLayoutAlgorithmTest, FragmentationNoPadding) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset { margin:0; border:10px solid; padding:0px; width:100px; }
    </style>
    <fieldset id="fieldset"></fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(10);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:120x10
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:120x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that a fieldset with auto height will fragment when its content reaches
// the fragmentation line.
TEST_F(NGFieldsetLayoutAlgorithmTest, FieldsetContentFragmentationAutoHeight) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #fieldset {
          border:3px solid; margin:0; padding:10px; width: 150px;
        }
        #child {
          margin:0; width: 50px; height: 500px;
        }
      </style>
      <fieldset id="fieldset">
        <div id="child"></div>
      </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x200
    offset:3,3 size:170x197
      offset:10,10 size:50x187
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x200
    offset:3,0 size:170x200
      offset:10,0 size:50x200
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x126
    offset:3,0 size:170x123
      offset:10,0 size:50x113
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that a fieldset with a set height will fragment when its content
// reaches the fragmentation line.
TEST_F(NGFieldsetLayoutAlgorithmTest, FieldsetContentFragmentation) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #fieldset {
          border:3px solid; margin:0; padding:10px; width: 150px; height: 100px;
        }
        #child {
          margin:0; width: 50px; height: 500px;
        }
      </style>
      <fieldset id="fieldset">
        <div id="child"></div>
      </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x126
    offset:3,3 size:170x120
      offset:10,10 size:50x187
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x0
    offset:3,0 size:170x0
      offset:10,0 size:50x200
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x0
    offset:3,0 size:170x0
      offset:10,0 size:50x113
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that a fieldset with auto height will fragment when its legend reaches
// the fragmentation line.
TEST_F(NGFieldsetLayoutAlgorithmTest, LegendFragmentationAutoHeight) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #fieldset {
          border:3px solid; margin:0; padding:10px; width: 150px;
        }
        #legend {
          padding:0px; margin:0; width: 50px; height: 500px;
        }
      </style>
      <fieldset id="fieldset">
        <legend id="legend"></legend>
      </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x200
    offset:13,0 size:50x200
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x200
    offset:13,0 size:50x200
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x123
    offset:13,0 size:50x100
    offset:3,100 size:170x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that a fieldset with a set height will fragment when its legend
// reaches the fragmentation line. The used height should also be extended to
// encompass the legend.
TEST_F(NGFieldsetLayoutAlgorithmTest, LegendFragmentation) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #fieldset {
          border:3px solid; margin:0; padding:10px; width: 150px; height: 100px;
        }
        #legend {
          padding:0px; margin:0; width: 50px; height: 500px;
        }
      </style>
      <fieldset id="fieldset">
        <legend id="legend"></legend>
      </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x200
    offset:13,0 size:50x200
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x200
    offset:13,0 size:50x200
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x123
    offset:13,0 size:50x100
    offset:3,100 size:170x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that a fieldset with auto height will fragment when its legend/content
// reaches the fragmentation line.
TEST_F(NGFieldsetLayoutAlgorithmTest, LegendAndContentFragmentationAutoHeight) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #fieldset {
          border:3px solid; margin:0; padding:10px; width: 150px;
        }
        #legend {
          padding:0px; margin:0; width: 50px; height: 500px;
        }
        #child {
          margin:0; width: 100px; height: 200px;
        }
      </style>
      <fieldset id="fieldset">
        <legend id="legend"></legend>
        <div id="child"></div>
      </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x200
    offset:13,0 size:50x200
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x200
    offset:13,0 size:50x200
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x200
    offset:13,0 size:50x100
    offset:3,100 size:170x100
      offset:10,10 size:100x90
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x123
    offset:3,0 size:170x120
      offset:10,0 size:100x110
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that a fieldset with a set height will fragment when its legend/content
// reaches the fragmentation line.
TEST_F(NGFieldsetLayoutAlgorithmTest, LegendAndContentFragmentation) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #fieldset {
          border:3px solid; margin:0; padding:10px; width: 150px; height: 100px;
        }
        #legend {
          padding:0px; margin:0; width: 50px; height: 500px;
        }
        #child {
          margin:0; width: 100px; height: 200px;
        }
      </style>
      <fieldset id="fieldset">
        <legend id="legend"></legend>
        <div id="child"></div>
      </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x200
    offset:13,0 size:50x200
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x200
    offset:13,0 size:50x200
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x123
    offset:13,0 size:50x100
    offset:3,100 size:170x20
      offset:10,10 size:100x90
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:176x0
    offset:3,0 size:170x0
      offset:10,0 size:100x110
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests fragmentation when a legend's child content overflows.
TEST_F(NGFieldsetLayoutAlgorithmTest, LegendFragmentationWithOverflow) {
  SetBodyInnerHTML(R"HTML(
      <style>
        fieldset, legend { margin:0; border:none; padding:0; }
      </style>
      <fieldset id="fieldset">
        <legend style="height:30px;">
          <div style="width:55px; height:150px;"></div>
        </legend>
        <div style="width:44px; height:150px;"></div>
      </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(100);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:55x30
      offset:0,0 size:55x100
    offset:0,30 size:1000x70
      offset:0,0 size:44x70
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x80
    offset:0,0 size:55x0
      offset:0,0 size:55x50
    offset:0,0 size:1000x80
      offset:0,0 size:44x80
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that fragmentation works as expected when the fieldset content has a
// negative margin block start.
TEST_F(NGFieldsetLayoutAlgorithmTest,
       LegendAndContentFragmentationNegativeMargin) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #fieldset {
          border:none; margin:0; padding:0px; width: 150px; height: 100px;
        }
        #legend {
          padding:0px; margin:0; width: 50px; height: 100px;
        }
        #child {
          margin-top: -20px; width: 100px; height: 40px;
        }
      </style>
      <fieldset id="fieldset">
        <legend id="legend"></legend>
        <div id="child"></div>
      </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(100);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:150x100
    offset:0,0 size:50x100
    offset:0,100 size:150x0
      offset:0,-20 size:100x20
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:150x0
    offset:0,0 size:150x0
      offset:0,0 size:100x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, OverflowedLegend) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset {
        border:none; margin:0; padding:0px; width: 100px; height: 100px;
      }
      #legend {
        padding:0px; margin:0px;
      }
    </style>
    <fieldset id="fieldset">
      <legend id="legend" style="width:75%; height:60px;">
        <div id="grandchild1" style="width:50px; height:120px;"></div>
        <div id="grandchild2" style="width:40px; height:20px;"></div>
      </legend>
      <div id="child" style="width:85%; height:10px;"></div>
    </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(100);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x100
    offset:0,0 size:75x60
      offset:0,0 size:50x100
    offset:0,60 size:100x40
      offset:0,0 size:85x10
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x0
    offset:0,0 size:75x0
      offset:0,0 size:50x20
      offset:0,20 size:40x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, OverflowedFieldsetContent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset {
        border:none; margin:0; padding:0px; width: 100px; height: 100px;
      }
      #legend {
        padding:0px; margin:0px;
      }
    </style>
    <fieldset id="fieldset">
      <legend id="legend" style="width:75%; height:10px;">
        <div style="width:50px; height:220px;"></div>
      </legend>
      <div style="width:85%; height:10px;"></div>
      <div id="child" style="width:65%; height:10px;">
        <div style="width:51px; height:220px;"></div>
      </div>
    </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(100);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x100
    offset:0,0 size:75x10
      offset:0,0 size:50x100
    offset:0,10 size:100x90
      offset:0,0 size:85x10
      offset:0,10 size:65x10
        offset:0,0 size:51x80
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x0
    offset:0,0 size:75x0
      offset:0,0 size:50x100
    offset:0,0 size:100x0
      offset:0,0 size:65x0
        offset:0,0 size:51x100
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x0
    offset:0,0 size:75x0
      offset:0,0 size:50x20
    offset:0,0 size:100x0
      offset:0,0 size:65x0
        offset:0,0 size:51x40
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, BreakInsideAvoid) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset {
        border:none; margin:0; padding:0px; width: 100px; height: 100px;
      }
      #legend {
        padding:0px; margin:0px;
      }
    </style>
     <fieldset id="fieldset">
      <legend id="legend" style="width:10px; height:50px;"></legend>
      <div style="break-inside:avoid; width:20px; height:70px;"></div>
    </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(100);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x100
    offset:0,0 size:10x50
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x0
    offset:0,0 size:100x0
      offset:0,0 size:20x70
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, BreakInsideAvoidTallBlock) {
  // The block that has break-inside:avoid is too tall to fit in one
  // fragmentainer. So a break is unavoidable. Let's check that:
  // 1. The block is still shifted to the start of the next fragmentainer
  // 2. We give up shifting it any further (would cause infinite an loop)
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset {
        border:none; margin:0; padding:0px; width: 100px; height: 100px;
      }
      #legend {
        padding:0px; margin:0px;
      }
    </style>
     <fieldset id="fieldset">
      <legend id="legend" style="width:10px; height:50px;"></legend>
      <div style="break-inside:avoid; width:20px; height:170px;"></div>
    </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(100);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x100
    offset:0,0 size:10x50
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x0
    offset:0,0 size:100x0
      offset:0,0 size:20x100
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x0
    offset:0,0 size:100x0
      offset:0,0 size:20x70
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, LegendBreakInsideAvoid) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset {
        border:none; margin:0; padding:0px; width: 100px; height: 50px;
      }
      #legend {
        padding:0px; margin:0px;
      }
    </style>
    <div id="container">
      <div style="width:20px; height:50px;"></div>
      <fieldset id="fieldset">
        <legend id="legend" style="break-inside:avoid; width:10px; height:60px;">
        </legend>
      </fieldset>
    </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(100);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("container")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:20x50
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x60
    offset:0,0 size:100x60
      offset:0,0 size:10x60
      offset:0,60 size:100x0
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, BreakBeforeAvoid) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset {
        border:none; margin:0; padding:0px; width: 100px;
      }
      #legend {
        padding:0px; margin:0px;
      }
    </style>
    <div id="container">
      <div style="width:20px; height:50px;"></div>
      <fieldset id="fieldset">
        <legend id="legend" style="width:10px; height:25px;"></legend>
        <div style="width:30px; height:25px;"></div>
        <div style="break-before:avoid; width:15px; height:25px;"></div>
      </fieldset>
    </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(100);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("container")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x75
    offset:0,0 size:20x50
    offset:0,50 size:100x25
      offset:0,0 size:10x25
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:100x50
      offset:0,0 size:100x50
        offset:0,0 size:30x25
        offset:0,25 size:15x25
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, LegendBreakBeforeAvoid) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset {
        border:10px solid; margin:0; padding:0px; width: 100px;
      }
      #legend {
        padding:0px; margin:10px; width:10px; height:25px;
      }
    </style>
    <div id="container">
      <div style="width:20px; height:90px;"></div>
      <fieldset id="fieldset">
        <legend id="legend" style="break-before:avoid;"></legend>
      </fieldset>
    </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(100);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("container")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:20x90
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x55
    offset:0,0 size:120x55
      offset:20,10 size:10x25
      offset:10,45 size:100x0
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, BreakAfterAvoid) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset {
        border:none; margin:0; padding:0px; width: 100px;
      }
      #legend {
        padding:0px; margin:0px;
      }
    </style>
    <div id="container">
      <div style="width:20px; height:50px;"></div>
      <fieldset id="fieldset">
        <legend id="legend" style="width:10px; height:25px;"></legend>
        <div style="break-after:avoid; width:30px; height:25px;"></div>
        <div style="width:15px; height:25px;"></div>
      </fieldset>
    </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(100);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("container")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x75
    offset:0,0 size:20x50
    offset:0,50 size:100x25
      offset:0,0 size:10x25
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:100x50
      offset:0,0 size:100x50
        offset:0,0 size:30x25
        offset:0,25 size:15x25
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, LegendBreakAfterAvoid) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset {
        border:0px solid; margin:0; padding:0px; width: 100px;
      }
      #legend {
        padding:0px; margin:0px; width:10px; height:50px;
      }
    </style>
    <div id="container">
      <div style="width:20px; height:50px;"></div>
      <fieldset id="fieldset">
        <legend id="legend" style="break-after:avoid;"></legend>
        <div style="width:15px; height:25px;"></div>
      </fieldset>
    </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(100);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("container")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:20x50
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x75
    offset:0,0 size:100x75
      offset:0,0 size:10x50
      offset:0,50 size:100x25
        offset:0,0 size:15x25
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, MarginTopPastEndOfFragmentainer) {
  // A block whose border box would start past the end of the current
  // fragmentainer should start exactly at the start of the next fragmentainer,
  // discarding what's left of the margin.
  // https://www.w3.org/TR/css-break-3/#break-margins
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset {
        border:none; margin:0; padding:0px; width: 100px; height: 100px;
      }
      #legend {
        padding:0px; margin:0px;
      }
    </style>
     <fieldset id="fieldset">
      <legend id="legend" style="margin-top:60px; width:10px; height:20px;"></legend>
      <div style="width:20px; height:20px;"></div>
    </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(50);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x50
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x50
    offset:0,0 size:10x20
    offset:0,20 size:100x30
      offset:0,0 size:20x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGFieldsetLayoutAlgorithmTest, MarginBottomPastEndOfFragmentainer) {
  // A block whose border box would start past the end of the current
  // fragmentainer should start exactly at the start of the next fragmentainer,
  // discarding what's left of the margin.
  // https://www.w3.org/TR/css-break-3/#break-margins
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset {
        border:none; margin:0; padding:0px; width: 100px; height: 100px;
      }
      #legend {
        padding:0px; margin:0px;
      }
    </style>
     <fieldset id="fieldset">
      <legend id="legend" style="margin-bottom:20px; height:90px;"></legend>
      <div style="width:20px; height:20px;"></div>
    </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(100);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x100
    offset:0,0 size:0x90
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:100x0
    offset:0,0 size:100x0
      offset:0,0 size:20x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that a fieldset with a large border and a small legend fragment
// correctly. In this case, the legend block offset is not adjusted because the
// legend is broken across multiple fragments.
TEST_F(NGFieldsetLayoutAlgorithmTest, SmallLegendLargeBorderFragmentation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset { margin:0; border:60px solid; padding:0px; width:100px;
                  height:10px; }
      #legend { padding:0; width:10px; height:50px; }
    </style>
    <fieldset id="fieldset">
      <legend id="legend"></legend>
    </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(40);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:220x40
    offset:60,0 size:10x40
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:220x40
    offset:60,0 size:10x10
    offset:60,20 size:100x10
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:220x40
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:220x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that a fieldset with a large border and a small legend fragment
// correctly. In this case, the legend block offset is adjusted because the
// legend fits inside the first fragment.
TEST_F(NGFieldsetLayoutAlgorithmTest, SmallerLegendLargeBorderFragmentation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset { margin:0; border:60px solid; padding:0px; width:100px;
                  height:10px; }
      #legend { padding:0; width:10px; height:5px; }
    </style>
    <fieldset id="fieldset">
      <legend id="legend"></legend>
    </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(40);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:220x40
    offset:60,27.5 size:10x5
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:220x40
    offset:60,20 size:100x10
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:220x40
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:220x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// Tests that a fieldset with a large border and a small legend fragment
// correctly. In this case, the legend block offset is not adjusted because the
// legend breaks after attempting to adjust the offset.
TEST_F(NGFieldsetLayoutAlgorithmTest, SmallerLegendLargeBorderWithBreak) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fieldset { margin:0; border:60px solid; padding:0px; width:100px;
                  height:10px; }
      #legend { padding:0; width:10px; height:5px; margin-top:16px; }
    </style>
    <fieldset id="fieldset">
      <legend id="legend"></legend>
    </fieldset>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(40);

  NGBlockNode node(ToLayoutBox(GetLayoutObjectByElementId("fieldset")));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      WritingMode::kHorizontalTb, TextDirection::kLtr,
      LogicalSize(LayoutUnit(1000), kIndefiniteSize), false,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(node, space);
  ASSERT_TRUE(fragment->BreakToken());

  String dump = DumpFragmentTree(fragment.get());
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:220x40
    offset:60,16 size:10x5
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:220x40
    offset:60,20 size:100x10
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_TRUE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:220x40
)DUMP";
  EXPECT_EQ(expectation, dump);

  fragment = NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
      node, space, fragment->BreakToken());
  ASSERT_FALSE(fragment->BreakToken());

  dump = DumpFragmentTree(fragment.get());
  expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:220x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

}  // anonymous namespace
}  // namespace blink
