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
  const NGPaintFragment& FirstLineBoxByElementId(const char* id) {
    const LayoutNGBlockFlow& block_flow =
        ToLayoutNGBlockFlow(*GetLayoutObjectByElementId(id));
    const NGPaintFragment& root_fragment = *block_flow.PaintFragment();
    EXPECT_GE(1u, root_fragment.Children().size());
    const NGPaintFragment& line_box = *root_fragment.Children()[0];
    EXPECT_EQ(NGPhysicalFragment::kFragmentLineBox,
              line_box.PhysicalFragment().Type());
    return line_box;
  }
};

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
