// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

class NGPhysicalBoxFragmentTest : public NGLayoutTest {
 public:
  NGPhysicalBoxFragmentTest() : NGLayoutTest() {}

  const NGPhysicalBoxFragment& GetBodyFragment() const {
    return *ToLayoutBlockFlow(GetDocument().body()->GetLayoutObject())
                ->CurrentFragment();
  }

  const NGPhysicalBoxFragment& GetPhysicalBoxFragmentByElementId(
      const char* id) {
    LayoutBlockFlow* layout_object =
        ToLayoutBlockFlow(GetLayoutObjectByElementId(id));
    DCHECK(layout_object);
    const NGPhysicalBoxFragment* fragment = layout_object->CurrentFragment();
    DCHECK(fragment);
    return *fragment;
  }
};

TEST_F(NGPhysicalBoxFragmentTest, FloatingDescendantsInlineChlidren) {
  SetBodyInnerHTML(R"HTML(
    <div id="hasfloats">
      text
      <div style="float: left"></div>
    </div>
    <div id="nofloats">
      text
    </div>
  )HTML");

  const NGPhysicalBoxFragment& has_floats =
      GetPhysicalBoxFragmentByElementId("hasfloats");
  EXPECT_TRUE(has_floats.HasFloatingDescendants());
  const NGPhysicalBoxFragment& no_floats =
      GetPhysicalBoxFragmentByElementId("nofloats");
  EXPECT_FALSE(no_floats.HasFloatingDescendants());
}

TEST_F(NGPhysicalBoxFragmentTest, FloatingDescendantsBlockChlidren) {
  SetBodyInnerHTML(R"HTML(
    <div id="hasfloats">
      <div></div>
      <div style="float: left"></div>
    </div>
    <div id="nofloats">
      <div></div>
    </div>
  )HTML");

  const NGPhysicalBoxFragment& has_floats =
      GetPhysicalBoxFragmentByElementId("hasfloats");
  EXPECT_TRUE(has_floats.HasFloatingDescendants());
  const NGPhysicalBoxFragment& no_floats =
      GetPhysicalBoxFragmentByElementId("nofloats");
  EXPECT_FALSE(no_floats.HasFloatingDescendants());
}

// HasFloatingDescendants() should be set for each inline formatting context and
// should not be propagated across inline formatting context.
TEST_F(NGPhysicalBoxFragmentTest, FloatingDescendantsInlineBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id="nofloats">
      text
      <span id="hasfloats" style="display: inline-block">
        <div style="float: left"></div>
      </span>
    </div>
  )HTML");

  const NGPhysicalBoxFragment& has_floats =
      GetPhysicalBoxFragmentByElementId("hasfloats");
  EXPECT_TRUE(has_floats.HasFloatingDescendants());
  const NGPhysicalBoxFragment& no_floats =
      GetPhysicalBoxFragmentByElementId("nofloats");
  EXPECT_FALSE(no_floats.HasFloatingDescendants());
}

// TODO(layout-dev): Design more straightforward way to ensure old layout
// instead of using |contenteditable|.

// Tests that a normal old layout root box fragment has correct box type.
TEST_F(NGPhysicalBoxFragmentTest, DISABLED_NormalOldLayoutRoot) {
  SetBodyInnerHTML("<div contenteditable>X</div>");
  const NGPhysicalFragment* fragment =
      GetBodyFragment().Children().front().get();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsBox());
  EXPECT_EQ(NGPhysicalFragment::kNormalBox, fragment->BoxType());
  EXPECT_TRUE(fragment->IsOldLayoutRoot());
  EXPECT_TRUE(fragment->IsBlockFormattingContextRoot());
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
// Tests that a float old layout root box fragment has correct box type.
TEST_F(NGPhysicalBoxFragmentTest, DISABLED_FloatOldLayoutRoot) {
  SetBodyInnerHTML("<span contenteditable style='float:left'>X</span>foo");
  const NGPhysicalFragment* fragment =
      GetBodyFragment().Children().front().get();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsBox());
  EXPECT_EQ(NGPhysicalFragment::kFloating, fragment->BoxType());
  EXPECT_TRUE(fragment->IsOldLayoutRoot());
  EXPECT_TRUE(fragment->IsBlockFormattingContextRoot());
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
// Tests that an inline block old layout root box fragment has correct box type.
TEST_F(NGPhysicalBoxFragmentTest, DISABLED_InlineBlockOldLayoutRoot) {
  SetBodyInnerHTML(
      "<span contenteditable style='display:inline-block'>X</span>foo");
  const NGPhysicalContainerFragment* line_box =
      ToNGPhysicalContainerFragment(GetBodyFragment().Children().front().get());
  const NGPhysicalFragment* fragment = line_box->Children().front().get();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsBox());
  EXPECT_EQ(NGPhysicalFragment::kAtomicInline, fragment->BoxType());
  EXPECT_TRUE(fragment->IsOldLayoutRoot());
  EXPECT_TRUE(fragment->IsBlockFormattingContextRoot());
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
// Tests that an out-of-flow positioned old layout root box fragment has correct
// box type.
TEST_F(NGPhysicalBoxFragmentTest, DISABLED_OutOfFlowPositionedOldLayoutRoot) {
  SetBodyInnerHTML(
      "<style>body {position: absolute}</style>"
      "<div contenteditable style='position: absolute'>X</div>");
  const NGPhysicalFragment* fragment =
      GetBodyFragment().Children().front().get();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsBox());
  EXPECT_EQ(NGPhysicalFragment::kOutOfFlowPositioned, fragment->BoxType());
  EXPECT_TRUE(fragment->IsOldLayoutRoot());
  EXPECT_TRUE(fragment->IsBlockFormattingContextRoot());
}

}  // namespace blink
