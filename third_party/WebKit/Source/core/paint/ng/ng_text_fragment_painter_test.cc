// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_text_fragment_painter.h"

#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/paint/PaintControllerPaintTest.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayerPainter.h"
#include "core/style/ComputedStyle.h"
#include "core/testing/CoreUnitTestHelper.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/runtime_enabled_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class NGTextFragmentPainterTest : public PaintControllerPaintTest,
                                  private ScopedLayoutNGForTest {
 public:
  NGTextFragmentPainterTest(LocalFrameClient* local_frame_client = nullptr)
      : PaintControllerPaintTest(local_frame_client),
        ScopedLayoutNGForTest(true) {}
};

INSTANTIATE_TEST_CASE_P(All,
                        NGTextFragmentPainterTest,
                        ::testing::Values(0, kRootLayerScrolling));

TEST_P(NGTextFragmentPainterTest, TestTextStyle) {
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

  const NGPaintFragment& root_fragment = *block_flow.PaintFragment();
  EXPECT_EQ(1u, root_fragment.Children().size());
  const NGPaintFragment& line_box_fragment = *root_fragment.Children()[0];
  EXPECT_EQ(1u, line_box_fragment.Children().size());
  const NGPaintFragment& text_fragment = *line_box_fragment.Children()[0];

  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 2,
      TestDisplayItem(ViewBackgroundClient(), DisplayItem::kDocumentBackground),
      TestDisplayItem(text_fragment, kForegroundType));
}

}  // namespace blink
