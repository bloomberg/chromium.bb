// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_column_layout_algorithm.h"

#include "core/layout/ng/ng_base_layout_algorithm_test.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"

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

  RefPtr<NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(
      const NGConstraintSpace& space,
      NGBlockNode node) {
    RefPtr<NGLayoutResult> result =
        NGBlockLayoutAlgorithm(node, space).Layout();

    return ToNGPhysicalBoxFragment(result->PhysicalFragment().Get());
  }

  RefPtr<NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(Element* element) {
    NGBlockNode container(ToLayoutBox(element->GetLayoutObject()));
    RefPtr<NGConstraintSpace> space = ConstructBlockLayoutTestConstraintSpace(
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
    return DumpFragmentTree(fragment.Get());
  }

  RefPtr<ComputedStyle> style_;
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
  RefPtr<NGConstraintSpace> space = ConstructBlockLayoutTestConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(*space, container);
  FragmentChildIterator iterator(parent_fragment.Get());
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
  RefPtr<NGConstraintSpace> space = ConstructBlockLayoutTestConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(*space, container);
  FragmentChildIterator iterator(parent_fragment.Get());
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
  RefPtr<NGConstraintSpace> space = ConstructBlockLayoutTestConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(*space, container);

  FragmentChildIterator iterator(parent_fragment.Get());
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

}  // anonymous namespace
}  // namespace blink
