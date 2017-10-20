// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_column_layout_algorithm.h"

#include "core/layout/ng/ng_base_layout_algorithm_test.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_column_layout_algorithm.h"

namespace blink {
namespace {

class NGColumnLayoutAlgorithmTest : public NGBaseLayoutAlgorithmTest {
 protected:
  void SetUp() override {
    // Make sure to reset this, except for the one test that needs it.
    RuntimeEnabledFeatures::SetLayoutNGFragmentCachingEnabled(false);

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

  scoped_refptr<NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(
      Element* element) {
    NGBlockNode container(ToLayoutBox(element->GetLayoutObject()));
    scoped_refptr<NGConstraintSpace> space =
        ConstructBlockLayoutTestConstraintSpace(
            kHorizontalTopBottom, TextDirection::kLtr,
            NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
    return RunBlockLayoutAlgorithm(*space, container);
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

TEST_F(NGColumnLayoutAlgorithmTest, EmptyMulticol) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 210px;
      }
    </style>
    <div id="container">
      <div id="parent"></div>
    </div>
  )HTML");

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          kHorizontalTopBottom, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  scoped_refptr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(*space, container);
  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(210), LayoutUnit(100)), fragment->Size());
  EXPECT_FALSE(iterator.NextChild());

  // There should be nothing inside the multicol container.
  // TODO(mstensho): Get rid of this column fragment. It shouldn't be here.
  fragment = FragmentChildIterator(fragment).NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(100), LayoutUnit()), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());

  EXPECT_FALSE(FragmentChildIterator(fragment).NextChild());
}

TEST_F(NGColumnLayoutAlgorithmTest, EmptyBlock) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 210px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child"></div>
      </div>
    </div>
  )HTML");

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          kHorizontalTopBottom, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  scoped_refptr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(*space, container);
  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(210), LayoutUnit(100)), fragment->Size());
  ASSERT_TRUE(fragment);
  EXPECT_FALSE(iterator.NextChild());
  iterator.SetParent(fragment);

  // first column fragment
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(100), LayoutUnit()), fragment->Size());
  EXPECT_FALSE(iterator.NextChild());

  // #child fragment in first column
  iterator.SetParent(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(100), LayoutUnit()), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

TEST_F(NGColumnLayoutAlgorithmTest, BlockInOneColumn) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 310px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="width:60%; height:100%"></div>
      </div>
    </div>
  )HTML");

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          kHorizontalTopBottom, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  scoped_refptr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(*space, container);

  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(310), LayoutUnit(100)), fragment->Size());
  EXPECT_FALSE(iterator.NextChild());
  iterator.SetParent(fragment);

  // first column fragment
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(100)), fragment->Size());
  EXPECT_FALSE(iterator.NextChild());

  // #child fragment in first column
  iterator.SetParent(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(90), LayoutUnit(100)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

TEST_F(NGColumnLayoutAlgorithmTest, BlockInTwoColumns) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 210px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="width:75%; height:150px"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:210x100
      offset:0,0 size:100x100
        offset:0,0 size:75x100
      offset:110,0 size:100x50
        offset:0,0 size:75x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BlockInThreeColumns) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="width:75%; height:250px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:75x100
      offset:110,0 size:100x100
        offset:0,0 size:75x100
      offset:220,0 size:100x50
        offset:0,0 size:75x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ActualColumnCountGreaterThanSpecified) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 210px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="width:1px; height:250px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:210x100
      offset:0,0 size:100x100
        offset:0,0 size:1x100
      offset:110,0 size:100x100
        offset:0,0 size:1x100
      offset:220,0 size:100x50
        offset:0,0 size:1x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, TwoBlocksInTwoColumns) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child1" style="width:75%; height:60px;"></div>
        <div id="child2" style="width:85%; height:60px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:75x60
        offset:0,60 size:85x40
      offset:110,0 size:100x20
        offset:0,0 size:85x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ZeroHeight) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 0;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent"></div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x0
    offset:0,0 size:320x0
      offset:0,0 size:100x0
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ZeroHeightWithContent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 0;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:20px; height:5px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x0
    offset:0,0 size:320x0
      offset:0,0 size:100x1
        offset:0,0 size:20x1
      offset:110,0 size:100x1
        offset:0,0 size:20x1
      offset:220,0 size:100x1
        offset:0,0 size:20x1
      offset:330,0 size:100x1
        offset:0,0 size:20x1
      offset:440,0 size:100x1
        offset:0,0 size:20x1
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, OverflowedBlock) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child1" style="width:75%; height:60px;">
          <div id="grandchild1" style="width:50px; height:120px;"></div>
          <div id="grandchild2" style="width:40px; height:20px;"></div>
        </div>
        <div id="child2" style="width:85%; height:10px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x70
        offset:0,0 size:75x60
          offset:0,0 size:50x100
        offset:0,60 size:85x10
      offset:110,0 size:100x0
        offset:0,0 size:75x0
          offset:0,0 size:50x20
          offset:0,20 size:40x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, FloatInOneColumn) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="float:left; width:75%; height:100px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:75x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, TwoFloatsInOneColumn) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child1" style="float:left; width:15%; height:100px;"></div>
        <div id="child2" style="float:right; width:16%; height:100px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:15x100
        offset:84,0 size:16x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, TwoFloatsInTwoColumns) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child1" style="float:left; width:15%; height:150px;"></div>
        <div id="child2" style="float:right; width:16%; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:15x100
        offset:84,0 size:16x100
      offset:110,0 size:100x50
        offset:0,0 size:15x50
        offset:84,0 size:16x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BlockWithTopMarginInThreeColumns) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:50px; height:70px;"></div>
        <div style="margin-top:10px; width:60px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:50x70
        offset:0,80 size:60x20
      offset:110,0 size:100x100
        offset:0,0 size:60x100
      offset:220,0 size:100x30
        offset:0,0 size:60x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BlockStartAtColumnBoundary) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:50px; height:100px;"></div>
        <div style="width:60px; height:100px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:50x100
        offset:0,100 size:60x0
      offset:110,0 size:100x100
        offset:0,0 size:60x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedBlockAfterBlock) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:10px;"></div>
        <div>
          <div style="width:60px; height:120px;"></div>
          <div style="width:50px; height:20px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:100x10
        offset:0,10 size:100x90
          offset:0,0 size:60x90
      offset:110,0 size:100x50
        offset:0,0 size:100x50
          offset:0,0 size:60x30
          offset:0,30 size:50x20
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakInsideAvoid) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:10px; height:50px;"></div>
        <div style="break-inside:avoid; width:20px; height:70px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:10x50
      offset:110,0 size:100x70
        offset:0,0 size:20x70
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BreakInsideAvoidTallBlock) {
  // The block that has break-inside:avoid is too tall to fit in one
  // fragmentainer. So a break is unavoidable. Let's check that:
  // 1. The block is still shifted to the start of the next fragmentainer
  // 2. We give up shifting it any further (would cause infinite an loop)
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:10px; height:50px;"></div>
        <div style="break-inside:avoid; width:20px; height:170px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:10x50
      offset:110,0 size:100x100
        offset:0,0 size:20x100
      offset:220,0 size:100x70
        offset:0,0 size:20x70
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedBreakInsideAvoid) {
  // If there were no break-inside:avoid on the outer DIV here, there'd be a
  // break between the two inner ones, since they wouldn't both fit in the first
  // column. However, since the outer DIV does have such a declaration,
  // everything is supposed to be pushed to the second column, with no space
  // between the children.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:10px; height:50px;"></div>
        <div style="break-inside:avoid; width:30px;">
          <div style="break-inside:avoid; width:21px; height:30px;"></div>
          <div style="break-inside:avoid; width:22px; height:30px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:10x50
      offset:110,0 size:100x60
        offset:0,0 size:30x60
          offset:0,0 size:21x30
          offset:0,30 size:22x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, NestedBreakInsideAvoidTall) {
  // Here the outer DIV with break-inside:avoid is too tall to fit where it
  // occurs naturally, so it needs to be pushed to the second column. It's not
  // going to fit fully there either, though, since its two children don't fit
  // together. Its second child wants to avoid breaks inside, so it will be
  // moved to the third column.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:10px; height:50px;"></div>
        <div style="break-inside:avoid; width:30px;">
          <div style="width:21px; height:30px;"></div>
          <div style="break-inside:avoid; width:22px; height:80px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:320x100
      offset:0,0 size:100x100
        offset:0,0 size:10x50
      offset:110,0 size:100x100
        offset:0,0 size:30x100
          offset:0,0 size:21x30
      offset:220,0 size:100x80
        offset:0,0 size:30x80
          offset:0,0 size:22x80
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, BorderAndPadding) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x110
    offset:0,0 size:330x110
      offset:5,5 size:100x100
        offset:0,0 size:30x100
      offset:115,5 size:100x50
        offset:0,0 size:30x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, MinMax) {
  // The multicol container here contains two inline-blocks with a line break
  // opportunity between them. We'll test what min/max values we get for the
  // multicol container when specifying both column-count and column-width, only
  // column-count, and only column-width.
  SetBodyInnerHTML(R"HTML(
    <style>
      #multicol {
        column-gap: 10px;
        width: fit-content;
      }
      #multicol span { display:inline-block; width:50px; height:50px; }
    </style>
    <div id="container">
      <div id="multicol">
        <div>
          <span></span><wbr><span></span>
        </div>
      </div>
    </div>
  )HTML");

  LayoutObject* layout_object = GetLayoutObjectByElementId("multicol");
  ASSERT_TRUE(layout_object);
  ASSERT_TRUE(layout_object->IsBox());
  NGBlockNode node = NGBlockNode(ToLayoutBox(layout_object));
  ComputedStyle* style = layout_object->MutableStyle();
  scoped_refptr<NGConstraintSpace> space =
      ConstructBlockLayoutTestConstraintSpace(
          kHorizontalTopBottom, TextDirection::kLtr,
          NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  NGColumnLayoutAlgorithm algorithm(node, *space.get());
  Optional<MinMaxSize> size;

  // Both column-count and column-width set.
  style->SetColumnCount(3);
  style->SetColumnWidth(80);
  size = algorithm.ComputeMinMaxSize();
  ASSERT_TRUE(size.has_value());
  EXPECT_EQ(LayoutUnit(260), size->min_size);
  EXPECT_EQ(LayoutUnit(320), size->max_size);

  // Only column-count set.
  style->SetHasAutoColumnWidth();
  size = algorithm.ComputeMinMaxSize();
  ASSERT_TRUE(size.has_value());
  EXPECT_EQ(LayoutUnit(170), size->min_size);
  EXPECT_EQ(LayoutUnit(320), size->max_size);

  // Only column-width set.
  style->SetColumnWidth(80);
  style->SetHasAutoColumnCount();
  size = algorithm.ComputeMinMaxSize();
  ASSERT_TRUE(size.has_value());
  EXPECT_EQ(LayoutUnit(80), size->min_size);
  EXPECT_EQ(LayoutUnit(100), size->max_size);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancing) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x60
    offset:0,0 size:330x60
      offset:5,5 size:100x50
        offset:0,0 size:30x50
      offset:115,5 size:100x50
        offset:0,0 size:30x50
      offset:225,5 size:100x50
        offset:0,0 size:30x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingFixedHeightExactMatch) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        height: 50px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x60
    offset:0,0 size:330x60
      offset:5,5 size:100x50
        offset:0,0 size:30x50
      offset:115,5 size:100x50
        offset:0,0 size:30x50
      offset:225,5 size:100x50
        offset:0,0 size:30x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingFixedHeightLessContent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x110
    offset:0,0 size:330x110
      offset:5,5 size:100x50
        offset:0,0 size:30x50
      offset:115,5 size:100x50
        offset:0,0 size:30x50
      offset:225,5 size:100x50
        offset:0,0 size:30x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest,
       ColumnBalancingFixedHeightOverflowingContent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        height: 35px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x45
    offset:0,0 size:330x45
      offset:5,5 size:100x35
        offset:0,0 size:30x35
      offset:115,5 size:100x35
        offset:0,0 size:30x35
      offset:225,5 size:100x35
        offset:0,0 size:30x35
      offset:335,5 size:100x35
        offset:0,0 size:30x35
      offset:445,5 size:100x10
        offset:0,0 size:30x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingMinHeight) {
  // Min-height has no effect on the columns, only on the multicol
  // container. Balanced columns should never be taller than they have to be.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        min-height:70px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x60
    offset:0,0 size:330x60
      offset:5,5 size:100x50
        offset:0,0 size:30x50
      offset:115,5 size:100x50
        offset:0,0 size:30x50
      offset:225,5 size:100x50
        offset:0,0 size:30x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingMaxHeight) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        max-height:40px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:330x50
      offset:5,5 size:100x40
        offset:0,0 size:30x40
      offset:115,5 size:100x40
        offset:0,0 size:30x40
      offset:225,5 size:100x40
        offset:0,0 size:30x40
      offset:335,5 size:100x30
        offset:0,0 size:30x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest,
       ColumnBalancingMinHeightLargerThanMaxHeight) {
  // Min-height has no effect on the columns, only on the multicol
  // container. Balanced columns should never be taller than they have to be.
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        min-height:70px;
        max-height:50px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x60
    offset:0,0 size:330x60
      offset:5,5 size:100x50
        offset:0,0 size:30x50
      offset:115,5 size:100x50
        offset:0,0 size:30x50
      offset:225,5 size:100x50
        offset:0,0 size:30x50
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingFixedHeightMinHeight) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
        height:40px;
        max-height:30px;
      }
    </style>
    <div id="container">
      <div id="parent" style="border:3px solid; padding:2px;">
        <div style="width:30px; height:150px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x40
    offset:0,0 size:330x40
      offset:5,5 size:100x30
        offset:0,0 size:30x30
      offset:115,5 size:100x30
        offset:0,0 size:30x30
      offset:225,5 size:100x30
        offset:0,0 size:30x30
      offset:335,5 size:100x30
        offset:0,0 size:30x30
      offset:445,5 size:100x30
        offset:0,0 size:30x30
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancing100By3) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent { columns: 3; }
    </style>
    <div id="container">
      <div id="parent">
        <div style="height:100px;"></div>
      </div>
    </div>
  )HTML");

  scoped_refptr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(GetElementById("container"));

  FragmentChildIterator iterator(parent_fragment.get());
  const auto* multicol = iterator.NextChild();
  ASSERT_TRUE(multicol);

  // Actual column-count should be 3. I.e. no overflow columns.
  EXPECT_EQ(3U, multicol->Children().size());
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingEmpty) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent"></div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x0
    offset:0,0 size:320x0
      offset:0,0 size:100x0
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGColumnLayoutAlgorithmTest, ColumnBalancingEmptyBlock) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        columns: 3;
        column-gap: 10px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div style="width:20px;"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpFragmentTree(GetElementById("container"));
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x0
    offset:0,0 size:320x0
      offset:0,0 size:100x0
        offset:0,0 size:20x0
)DUMP";
  EXPECT_EQ(expectation, dump);
}

}  // anonymous namespace
}  // namespace blink
