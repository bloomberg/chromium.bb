// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_fieldset_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {
namespace {

class NGFieldsetLayoutAlgorithmTest : public NGBaseLayoutAlgorithmTest {
 protected:
  void SetUp() override {
    NGBaseLayoutAlgorithmTest::SetUp();
    style_ = ComputedStyle::Create();
    was_fieldset_enabled_ = RuntimeEnabledFeatures::LayoutNGFieldsetEnabled();
    was_block_fragmentation_enabled_ =
        RuntimeEnabledFeatures::LayoutNGBlockFragmentationEnabled();
    RuntimeEnabledFeatures::SetLayoutNGFieldsetEnabled(true);
    RuntimeEnabledFeatures::SetLayoutNGBlockFragmentationEnabled(true);
  }

  void TearDown() override {
    RuntimeEnabledFeatures::SetLayoutNGFieldsetEnabled(was_fieldset_enabled_);
    RuntimeEnabledFeatures::SetLayoutNGBlockFragmentationEnabled(
        was_block_fragmentation_enabled_);
  }

  scoped_refptr<const NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(
      const NGConstraintSpace& space,
      NGBlockNode node) {
    scoped_refptr<NGLayoutResult> result =
        NGBlockLayoutAlgorithm(node, space).Layout();

    return ToNGPhysicalBoxFragment(result->PhysicalFragment().get());
  }

  scoped_refptr<const NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(
      Element* element) {
    NGBlockNode container(ToLayoutBox(element->GetLayoutObject()));
    NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
        WritingMode::kHorizontalTb, TextDirection::kLtr,
        NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
    return RunBlockLayoutAlgorithm(space, container);
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
  bool was_fieldset_enabled_ = false;
  bool was_block_fragmentation_enabled_ = false;
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
        offset:50,0 size:0x0
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
      offset:3,30 size:120x0
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

}  // anonymous namespace
}  // namespace blink
