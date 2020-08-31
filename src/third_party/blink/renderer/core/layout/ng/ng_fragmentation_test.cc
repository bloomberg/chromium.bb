// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

class NGFragmentationTest : public NGBaseLayoutAlgorithmTest,
                            private ScopedLayoutNGBlockFragmentationForTest {
 protected:
  NGFragmentationTest() : ScopedLayoutNGBlockFragmentationForTest(true) {}

  scoped_refptr<const NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(
      Element* element) {
    NGBlockNode container(ToLayoutBox(element->GetLayoutObject()));
    NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
        WritingMode::kHorizontalTb, TextDirection::kLtr,
        LogicalSize(LayoutUnit(1000), kIndefiniteSize));
    return NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(container, space);
  }
};

TEST_F(NGFragmentationTest, MultipleFragments) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div style="columns:3; width:620px; column-fill:auto; height:100px; column-gap:10px;">
        <div id="outer1" style="height:150px;">
          <div id="inner1" style="height:250px;"></div>
          <div id="inner2" style="height:10px;"></div>
        </div>
        <div id="outer2" style="height:90px;"></div>
      </div>
    </div>
  )HTML");

  RunBlockLayoutAlgorithm(GetElementById("container"));
  const LayoutBox* outer1 = ToLayoutBox(GetLayoutObjectByElementId("outer1"));
  const LayoutBox* outer2 = ToLayoutBox(GetLayoutObjectByElementId("outer2"));
  const LayoutBox* inner1 = ToLayoutBox(GetLayoutObjectByElementId("inner1"));
  const LayoutBox* inner2 = ToLayoutBox(GetLayoutObjectByElementId("inner2"));

  EXPECT_EQ(outer1->PhysicalFragmentCount(), 3u);
  EXPECT_EQ(outer2->PhysicalFragmentCount(), 2u);
  EXPECT_EQ(inner1->PhysicalFragmentCount(), 3u);
  EXPECT_EQ(inner2->PhysicalFragmentCount(), 1u);

  // While the #outer1 box itself only needs two fragments, we need to create a
  // third fragment to hold the overflowing children in the third column.
  EXPECT_EQ(outer1->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 100));
  EXPECT_EQ(outer1->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 50));
  EXPECT_EQ(outer1->GetPhysicalFragment(2)->Size(), PhysicalSize(200, 0));

  // #inner1 overflows its parent and uses three columns.
  EXPECT_EQ(inner1->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 100));
  EXPECT_EQ(inner1->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 100));
  EXPECT_EQ(inner1->GetPhysicalFragment(2)->Size(), PhysicalSize(200, 50));

  // #inner2 is tiny, and only needs some space in one column (the third one).
  EXPECT_EQ(inner2->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 10));

  // #outer2 starts in the second column and ends in the third.
  EXPECT_EQ(outer2->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 50));
  EXPECT_EQ(outer2->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 40));
}

TEST_F(NGFragmentationTest, MultipleFragmentsAndColumnSpanner) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="multicol" style="columns:3; width:620px; column-gap:10px; orphans:1; widows:1; line-height:20px;">
        <div id="outer">
          <div id="inner1"><br><br><br><br></div>
          <div id="spanner1" style="column-span:all;"></div>
          <div id="inner2"><br><br><br><br><br></div>
          <div id="spanner2" style="column-span:all;"></div>
          <div id="inner3"><br><br><br><br><br><br><br></div>
        </div>
      </div>
    </div>
  )HTML");

  RunBlockLayoutAlgorithm(GetElementById("container"));
  const LayoutBox* multicol =
      ToLayoutBox(GetLayoutObjectByElementId("multicol"));
  const LayoutBox* outer = ToLayoutBox(GetLayoutObjectByElementId("outer"));
  const LayoutBox* inner1 = ToLayoutBox(GetLayoutObjectByElementId("inner1"));
  const LayoutBox* inner2 = ToLayoutBox(GetLayoutObjectByElementId("inner2"));
  const LayoutBox* inner3 = ToLayoutBox(GetLayoutObjectByElementId("inner3"));
  const LayoutBox* spanner1 =
      ToLayoutBox(GetLayoutObjectByElementId("spanner1"));
  const LayoutBox* spanner2 =
      ToLayoutBox(GetLayoutObjectByElementId("spanner2"));

  EXPECT_EQ(multicol->PhysicalFragmentCount(), 1u);

  // #outer will create 8 fragments: 2 for the 2 columns before the first
  // spanner, 3 for the 3 columns between the two spanners, and 3 for the 3
  // columns after the last spanner.
  EXPECT_EQ(outer->PhysicalFragmentCount(), 8u);

  // #inner1 has 4 lines split into 2 columns.
  EXPECT_EQ(inner1->PhysicalFragmentCount(), 2u);

  // #inner2 has 5 lines split into 3 columns.
  EXPECT_EQ(inner2->PhysicalFragmentCount(), 3u);

  // #inner3 has 8 lines split into 3 columns.
  EXPECT_EQ(inner3->PhysicalFragmentCount(), 3u);

  EXPECT_EQ(spanner1->PhysicalFragmentCount(), 1u);
  EXPECT_EQ(spanner2->PhysicalFragmentCount(), 1u);

  EXPECT_EQ(multicol->GetPhysicalFragment(0)->Size(), PhysicalSize(620, 140));
  EXPECT_EQ(outer->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(outer->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(outer->GetPhysicalFragment(2)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(outer->GetPhysicalFragment(3)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(outer->GetPhysicalFragment(4)->Size(), PhysicalSize(200, 20));
  EXPECT_EQ(outer->GetPhysicalFragment(5)->Size(), PhysicalSize(200, 60));
  EXPECT_EQ(outer->GetPhysicalFragment(6)->Size(), PhysicalSize(200, 60));
  EXPECT_EQ(outer->GetPhysicalFragment(7)->Size(), PhysicalSize(200, 20));
  EXPECT_EQ(inner1->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(inner1->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(inner2->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(inner2->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 40));
  EXPECT_EQ(inner2->GetPhysicalFragment(2)->Size(), PhysicalSize(200, 20));
  EXPECT_EQ(inner3->GetPhysicalFragment(0)->Size(), PhysicalSize(200, 60));
  EXPECT_EQ(inner3->GetPhysicalFragment(1)->Size(), PhysicalSize(200, 60));
  EXPECT_EQ(inner3->GetPhysicalFragment(2)->Size(), PhysicalSize(200, 20));
  EXPECT_EQ(spanner1->GetPhysicalFragment(0)->Size(), PhysicalSize(620, 0));
  EXPECT_EQ(spanner2->GetPhysicalFragment(0)->Size(), PhysicalSize(620, 0));
}

TEST_F(NGFragmentationTest, MultipleFragmentsNestedMulticol) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="outer_multicol" style="columns:3; column-fill:auto; height:100px; width:620px; column-gap:10px;">
        <div id="inner_multicol" style="columns:2;">
          <div id="child1" style="width:11px; height:350px;"></div>
          <div id="child2" style="width:22px; height:350px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  RunBlockLayoutAlgorithm(GetElementById("container"));
  const LayoutBox* outer_multicol =
      ToLayoutBox(GetLayoutObjectByElementId("outer_multicol"));
  const LayoutBox* inner_multicol =
      ToLayoutBox(GetLayoutObjectByElementId("inner_multicol"));
  const LayoutBox* child1 = ToLayoutBox(GetLayoutObjectByElementId("child1"));
  const LayoutBox* child2 = ToLayoutBox(GetLayoutObjectByElementId("child2"));

  EXPECT_EQ(outer_multicol->PhysicalFragmentCount(), 1u);

  // The content is too tall (350px + 350px, column height 100px, 2*3 columns =
  // 600px) and will use one more column than we have specified.
  EXPECT_EQ(inner_multicol->PhysicalFragmentCount(), 4u);

  // 350px tall content with a column height of 100px will require 4 fragments.
  EXPECT_EQ(child1->PhysicalFragmentCount(), 4u);
  EXPECT_EQ(child2->PhysicalFragmentCount(), 4u);

  EXPECT_EQ(outer_multicol->GetPhysicalFragment(0)->Size(),
            PhysicalSize(620, 100));

  EXPECT_EQ(inner_multicol->GetPhysicalFragment(0)->Size(),
            PhysicalSize(200, 100));
  EXPECT_EQ(inner_multicol->GetPhysicalFragment(1)->Size(),
            PhysicalSize(200, 100));
  EXPECT_EQ(inner_multicol->GetPhysicalFragment(2)->Size(),
            PhysicalSize(200, 100));
  EXPECT_EQ(inner_multicol->GetPhysicalFragment(3)->Size(),
            PhysicalSize(200, 100));

  // #child1 starts at the beginning of a column, so the last fragment will be
  // shorter than the rest.
  EXPECT_EQ(child1->GetPhysicalFragment(0)->Size(), PhysicalSize(11, 100));
  EXPECT_EQ(child1->GetPhysicalFragment(1)->Size(), PhysicalSize(11, 100));
  EXPECT_EQ(child1->GetPhysicalFragment(2)->Size(), PhysicalSize(11, 100));
  EXPECT_EQ(child1->GetPhysicalFragment(3)->Size(), PhysicalSize(11, 50));

  // #child2 starts in the middle of a column, so the first fragment will be
  // shorter than the rest.
  EXPECT_EQ(child2->GetPhysicalFragment(0)->Size(), PhysicalSize(22, 50));
  EXPECT_EQ(child2->GetPhysicalFragment(1)->Size(), PhysicalSize(22, 100));
  EXPECT_EQ(child2->GetPhysicalFragment(2)->Size(), PhysicalSize(22, 100));
  EXPECT_EQ(child2->GetPhysicalFragment(3)->Size(), PhysicalSize(22, 100));
}

}  // anonymous namespace
}  // namespace blink
