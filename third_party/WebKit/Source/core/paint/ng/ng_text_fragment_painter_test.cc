// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_text_fragment_painter.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/paint/PaintControllerPaintTest.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayerPainter.h"
#include "core/style/ComputedStyle.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/runtime_enabled_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using NGTextFragmentPainterTest = PaintControllerPaintTest;
INSTANTIATE_TEST_CASE_P(All,
                        NGTextFragmentPainterTest,
                        ::testing::Values(0, kRootLayerScrolling));

class EnableLayoutNGForScope {
 public:
  EnableLayoutNGForScope() {
    layout_ng_ = RuntimeEnabledFeatures::LayoutNGEnabled();
    paint_fragments_ = RuntimeEnabledFeatures::LayoutNGPaintFragmentsEnabled();
    RuntimeEnabledFeatures::SetLayoutNGEnabled(true);
    RuntimeEnabledFeatures::SetLayoutNGPaintFragmentsEnabled(true);
  }
  ~EnableLayoutNGForScope() {
    RuntimeEnabledFeatures::SetLayoutNGEnabled(layout_ng_);
    RuntimeEnabledFeatures::SetLayoutNGPaintFragmentsEnabled(paint_fragments_);
  }

 private:
  bool layout_ng_;
  bool paint_fragments_;
};

TEST_P(NGTextFragmentPainterTest, TestTextStyle) {
  EnableLayoutNGForScope enable_layout_ng;

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <body>
      <div id="container">Hello World!</div>
    </body>
  )HTML");

  LayoutObject& container = *GetLayoutObjectByElementId("container");

  const LayoutNGBlockFlow& block_flow = ToLayoutNGBlockFlow(container);

  RootPaintController().InvalidateAll();
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  IntRect interest_rect(0, 0, 640, 480);
  Paint(&interest_rect);

  DisplayItemClient* background_client = nullptr;
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    // With SPv1 and RLS, the document background uses the scrolling contents
    // layer as its DisplayItemClient.
    background_client = GetLayoutView().Layer()->GraphicsLayerBacking();
  } else {
    background_client = &GetLayoutView();
  }

  const NGPaintFragment& root_fragment = *block_flow.PaintFragment();
  EXPECT_EQ(1u, root_fragment.Children().size());
  const NGPaintFragment& line_box_fragment = *root_fragment.Children()[0];
  EXPECT_EQ(1u, line_box_fragment.Children().size());
  const NGPaintFragment& text_fragment = *line_box_fragment.Children()[0];

  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 3,
      TestDisplayItem(*background_client, DisplayItem::kDocumentBackground),
      TestDisplayItem(root_fragment, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(text_fragment, kForegroundType));
}

}  // namespace blink
