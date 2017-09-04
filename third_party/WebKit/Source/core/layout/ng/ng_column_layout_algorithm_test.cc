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

  iterator.SetParent(fragment);

  // first column fragment
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(100), LayoutUnit(100)), fragment->Size());

  // second column fragment
  const auto* column2 = iterator.NextChild();
  ASSERT_TRUE(column2);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(110), LayoutUnit()), column2->Offset());
  // TODO(mstensho): Make this work.
  // EXPECT_EQ(NGPhysicalSize(LayoutUnit(100), LayoutUnit(100)),
  // column2->Size());
  EXPECT_FALSE(iterator.NextChild());

  // #child fragment in first column
  iterator.SetParent(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(75), LayoutUnit(100)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child fragment in second column
  iterator.SetParent(column2);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(75), LayoutUnit(50)), fragment->Size());
  EXPECT_EQ(0U, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
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

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
  RefPtr<NGConstraintSpace> space = ConstructBlockLayoutTestConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(*space, container);

  FragmentChildIterator iterator(parent_fragment.Get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(320), LayoutUnit(100)), fragment->Size());
  EXPECT_FALSE(iterator.NextChild());

  // first column fragment
  iterator.SetParent(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(100), LayoutUnit(100)), fragment->Size());

  // second column fragment
  const auto* column2 = iterator.NextChild();
  ASSERT_TRUE(column2);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(110), LayoutUnit()), column2->Offset());
  // TODO(mstensho): Make this work.
  // EXPECT_EQ(NGPhysicalSize(LayoutUnit(100), LayoutUnit(100)),
  // column2->Size());

  // third column fragment
  const auto* column3 = iterator.NextChild();
  ASSERT_TRUE(column3);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(220), LayoutUnit()), column3->Offset());
  // TODO(mstensho): Make this work.
  // EXPECT_EQ(NGPhysicalSize(LayoutUnit(100), LayoutUnit(100)),
  // column3->Size());
  EXPECT_FALSE(iterator.NextChild());

  // #child fragment in first column
  iterator.SetParent(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(75), LayoutUnit(100)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());

  // #child fragment in second column
  iterator.SetParent(column2);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(75), LayoutUnit(100)), fragment->Size());
  EXPECT_EQ(0U, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());

  // #child fragment in third column
  iterator.SetParent(column3);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(75), LayoutUnit(50)), fragment->Size());
  EXPECT_EQ(0U, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
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

  iterator.SetParent(fragment);

  // first column fragment
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());

  // second column fragment
  const auto* column2 = iterator.NextChild();
  ASSERT_TRUE(column2);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(110), LayoutUnit()), column2->Offset());

  // third column fragment
  const auto* column3 = iterator.NextChild();
  ASSERT_TRUE(column3);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(220), LayoutUnit()), column3->Offset());
  EXPECT_FALSE(iterator.NextChild());

  // #child fragment in first column
  iterator.SetParent(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(1), LayoutUnit(100)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child fragment in second column
  iterator.SetParent(column2);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(1), LayoutUnit(100)), fragment->Size());
  EXPECT_EQ(0U, fragment->Children().size());

  // #child fragment in third column
  iterator.SetParent(column3);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(1), LayoutUnit(50)), fragment->Size());
  EXPECT_EQ(0U, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
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

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
  RefPtr<NGConstraintSpace> space = ConstructBlockLayoutTestConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(*space, container);

  FragmentChildIterator iterator(parent_fragment.Get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(320), LayoutUnit(100)), fragment->Size());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);

  // first column fragment
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());

  // second column fragment
  const auto* column2 = iterator.NextChild();
  ASSERT_TRUE(column2);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(110), LayoutUnit()), column2->Offset());
  EXPECT_FALSE(iterator.NextChild());

  // #child1 fragment in first column
  iterator.SetParent(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(75), LayoutUnit(60)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());
  // #child2 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit(60)), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(85), LayoutUnit(40)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child2 fragment in second column
  iterator.SetParent(column2);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(85), LayoutUnit(20)), fragment->Size());
  EXPECT_EQ(0U, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
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

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
  RefPtr<NGConstraintSpace> space = ConstructBlockLayoutTestConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(*space, container);

  FragmentChildIterator iterator(parent_fragment.Get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(320), LayoutUnit(100)), fragment->Size());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);

  // first column fragment
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());

  // second column fragment
  const auto* column2 = iterator.NextChild();
  ASSERT_TRUE(column2);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(110), LayoutUnit()), column2->Offset());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child1 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(75), LayoutUnit(60)), fragment->Size());
  FragmentChildIterator grandchild_iterator(fragment);
  // #grandchild1 fragment in first column
  fragment = grandchild_iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(50), LayoutUnit(100)), fragment->Size());
  EXPECT_FALSE(grandchild_iterator.NextChild());
  // #child2 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit(60)), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(85), LayoutUnit(10)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());

  iterator.SetParent(column2);
  // #child1 fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(75), LayoutUnit()), fragment->Size());
  grandchild_iterator.SetParent(fragment);
  // #grandchild1 fragment in second column
  fragment = grandchild_iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(50), LayoutUnit(20)), fragment->Size());
  // #grandchild2 fragment in second column
  fragment = grandchild_iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit(20)), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(40), LayoutUnit(20)), fragment->Size());
  EXPECT_FALSE(grandchild_iterator.NextChild());
  EXPECT_FALSE(iterator.NextChild());
}

TEST_F(NGColumnLayoutAlgorithmTest, DISABLED_FloatInOneColumn) {
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

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
  RefPtr<NGConstraintSpace> space = ConstructBlockLayoutTestConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(*space, container);

  FragmentChildIterator iterator(parent_fragment.Get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(320), LayoutUnit(100)), fragment->Size());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(75), LayoutUnit(100)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

TEST_F(NGColumnLayoutAlgorithmTest, DISABLED_TwoFloatsInOneColumn) {
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

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
  RefPtr<NGConstraintSpace> space = ConstructBlockLayoutTestConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(*space, container);

  FragmentChildIterator iterator(parent_fragment.Get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(320), LayoutUnit(100)), fragment->Size());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child1 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(15), LayoutUnit(100)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());
  // #child2 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(84), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(16), LayoutUnit(100)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

TEST_F(NGColumnLayoutAlgorithmTest, DISABLED_TwoFloatsInTwoColumns) {
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

  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
  RefPtr<NGConstraintSpace> space = ConstructBlockLayoutTestConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(*space, container);

  FragmentChildIterator iterator(parent_fragment.Get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(320), LayoutUnit(100)), fragment->Size());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child1 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(15), LayoutUnit(100)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());
  // #child2 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(84), LayoutUnit()), fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(16), LayoutUnit(100)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child1 fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(110), LayoutUnit()),
            fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(15), LayoutUnit(50)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());
  // #child2 fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(194), LayoutUnit()),
            fragment->Offset());
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(16), LayoutUnit(50)), fragment->Size());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

}  // anonymous namespace
}  // namespace blink
