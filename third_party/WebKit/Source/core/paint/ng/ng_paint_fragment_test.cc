// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_paint_fragment.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class NGPaintFragmentTest : public RenderingTest,
                            private ScopedLayoutNGForTest,
                            private ScopedLayoutNGPaintFragmentsForTest {
 public:
  NGPaintFragmentTest(LocalFrameClient* local_frame_client = nullptr)
      : RenderingTest(local_frame_client),
        ScopedLayoutNGForTest(true),
        ScopedLayoutNGPaintFragmentsForTest(true) {}

 protected:
  const NGPaintFragment& RootPaintFragmentByElementId(const char* id) {
    const LayoutNGBlockFlow& block_flow =
        ToLayoutNGBlockFlow(*GetLayoutObjectByElementId(id));
    const NGPaintFragment& root = *block_flow.PaintFragment();
    return root;
  }

  const NGPaintFragment& FirstLineBoxByElementId(const char* id) {
    const NGPaintFragment& root = RootPaintFragmentByElementId(id);
    EXPECT_GE(1u, root.Children().size());
    const NGPaintFragment& line_box = *root.Children()[0];
    EXPECT_EQ(NGPhysicalFragment::kFragmentLineBox,
              line_box.PhysicalFragment().Type());
    return line_box;
  }
};

TEST_F(NGPaintFragmentTest, InlineFragmentsFor) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px/1 Ahem; width: 10ch; }
    span { background: yellow; }
    </style>
    <body>
      <div id="container">12345 <span id="box">789 123456789 123<span> 567</div>
    </body>
  )HTML");
  LayoutBlockFlow* container =
      ToLayoutBlockFlow(GetLayoutObjectByElementId("container"));
  ASSERT_TRUE(container);
  LayoutObject* text1 = container->FirstChild();
  ASSERT_TRUE(text1 && text1->IsText());
  LayoutObject* box = text1->NextSibling();
  ASSERT_TRUE(box && box->IsLayoutInline());

  Vector<NGPaintFragment*> results;
  auto it = NGPaintFragment::InlineFragmentsFor(text1);
  results.AppendRange(it.begin(), it.end());
  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(text1, results[0]->GetLayoutObject());
  EXPECT_EQ(NGPhysicalOffset(), results[0]->InlineOffsetToContainerBox());

  results.clear();
  it = NGPaintFragment::InlineFragmentsFor(box);
  results.AppendRange(it.begin(), it.end());
  EXPECT_EQ(3u, results.size());
  EXPECT_EQ(box, results[0]->GetLayoutObject());
  EXPECT_EQ(box, results[1]->GetLayoutObject());
  EXPECT_EQ(box, results[2]->GetLayoutObject());

  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(60), LayoutUnit()),
            results[0]->InlineOffsetToContainerBox());
  EXPECT_EQ("789", ToNGPhysicalTextFragment(
                       results[0]->Children()[0]->PhysicalFragment())
                       .Text());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit(10)),
            results[1]->InlineOffsetToContainerBox());
  EXPECT_EQ("123456789", ToNGPhysicalTextFragment(
                             results[1]->Children()[0]->PhysicalFragment())
                             .Text());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit(20)),
            results[2]->InlineOffsetToContainerBox());
  EXPECT_EQ("123", ToNGPhysicalTextFragment(
                       results[2]->Children()[0]->PhysicalFragment())
                       .Text());
}

TEST_F(NGPaintFragmentTest, InlineBox) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; }
    </style>
    <body>
      <div id="container">1234567890<span id="box">XXX<span></div>
    </body>
  )HTML");
  const NGPaintFragment& line_box = FirstLineBoxByElementId("container");
  EXPECT_EQ(2u, line_box.Children().size());

  // Inline boxes without box decorations (border, background, etc.) do not
  // generate box fragments and that their child fragments are placed directly
  // under the line box.
  const NGPaintFragment& outer_text = *line_box.Children()[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 0, 100, 10), outer_text.VisualRect());

  const NGPaintFragment& inner_text = *line_box.Children()[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(100, 0, 30, 10), inner_text.VisualRect());
}

TEST_F(NGPaintFragmentTest, InlineBoxWithDecorations) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; }
    #box { background: blue; }
    </style>
    <body>
      <div id="container">1234567890<span id="box">XXX<span></div>
    </body>
  )HTML");
  const NGPaintFragment& line_box = FirstLineBoxByElementId("container");
  EXPECT_EQ(2u, line_box.Children().size());

  const NGPaintFragment& outer_text = *line_box.Children()[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 0, 100, 10), outer_text.VisualRect());

  // Inline boxes with box decorations generate box fragments.
  const NGPaintFragment& inline_box = *line_box.Children()[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(100, 0, 30, 10), inline_box.VisualRect());

  EXPECT_EQ(1u, inline_box.Children().size());
  const NGPaintFragment& text = *inline_box.Children()[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText, text.PhysicalFragment().Type());

  // VisualRect of descendants of inline boxes are relative to its inline
  // formatting context, not to its parent inline box.
  EXPECT_EQ(LayoutRect(100, 0, 30, 10), text.VisualRect());
}

TEST_F(NGPaintFragmentTest, RelativeBlock) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; }
    #container { position: relative; top: 10px; }
    </style>
    <body>
      <div id="container">1234567890<span id="box">XXX<span></div>
    </body>
  )HTML");
  const NGPaintFragment& line_box = FirstLineBoxByElementId("container");
  EXPECT_EQ(2u, line_box.Children().size());

  const NGPaintFragment& outer_text = *line_box.Children()[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 10, 100, 10), outer_text.VisualRect());

  const NGPaintFragment& inner_text = *line_box.Children()[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(100, 10, 30, 10), inner_text.VisualRect());
}

// TODO(kojii): VisualRect for inline box with 'position: relative' is not
// correct. Disabled for now.
TEST_F(NGPaintFragmentTest, DISABLED_RelativeInline) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; }
    #box { position: relative; top: 10px; }
    </style>
    <body>
      <div id="container">1234567890<span id="box">XXX<span></div>
    </body>
  )HTML");
  const NGPaintFragment& line_box = FirstLineBoxByElementId("container");
  EXPECT_EQ(2u, line_box.Children().size());

  const NGPaintFragment& outer_text = *line_box.Children()[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 0, 100, 10), outer_text.VisualRect());

  // Inline boxes with box decorations generate box fragments.
  const NGPaintFragment& inline_box = *line_box.Children()[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(100, 10, 30, 10), inline_box.VisualRect());

  EXPECT_EQ(1u, inline_box.Children().size());
  const NGPaintFragment& inner_text = *inline_box.Children()[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text.PhysicalFragment().Type());

  // VisualRect of descendants of inline boxes are relative to its inline
  // formatting context, not to its parent inline box.
  EXPECT_EQ(LayoutRect(100, 10, 30, 10), inner_text.VisualRect());
}

// TODO(kojii): VisualRect for inline box with 'position: relative' is not
// correct. Disabled for now.
TEST_F(NGPaintFragmentTest, DISABLED_RelativeBlockAndInline) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; }
    #container, #box { position: relative; top: 10px; }
    </style>
    <body>
      <div id="container">1234567890<span id="box">XXX<span></div>
    </body>
  )HTML");
  const NGPaintFragment& line_box = FirstLineBoxByElementId("container");
  EXPECT_EQ(2u, line_box.Children().size());

  const NGPaintFragment& outer_text = *line_box.Children()[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 10, 100, 10), outer_text.VisualRect());

  // Inline boxes with box decorations generate box fragments.
  const NGPaintFragment& inline_box = *line_box.Children()[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(100, 20, 30, 10), inline_box.VisualRect());

  EXPECT_EQ(1u, inline_box.Children().size());
  const NGPaintFragment& inner_text = *inline_box.Children()[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text.PhysicalFragment().Type());

  // VisualRect of descendants of inline boxes are relative to its inline
  // formatting context, not to its parent inline box.
  EXPECT_EQ(LayoutRect(100, 20, 30, 10), inner_text.VisualRect());
}

}  // namespace blink
