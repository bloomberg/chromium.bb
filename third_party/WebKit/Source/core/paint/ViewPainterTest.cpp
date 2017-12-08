// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ViewPainter.h"

#include <gtest/gtest.h>
#include "core/paint/PaintControllerPaintTest.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

namespace blink {

class ViewPainterTest : public PaintControllerPaintTest {
 protected:
  void RunFixedBackgroundTest(bool prefer_compositing_to_lcd_text);
};

INSTANTIATE_TEST_CASE_P(All,
                        ViewPainterTest,
                        ::testing::Values(0,
                                          kSlimmingPaintV175,
                                          kRootLayerScrolling,
                                          kSlimmingPaintV175 |
                                              kRootLayerScrolling));

void ViewPainterTest::RunFixedBackgroundTest(
    bool prefer_compositing_to_lcd_text) {
  if (prefer_compositing_to_lcd_text) {
    Settings* settings = GetDocument().GetFrame()->GetSettings();
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body {
        margin: 0;
        width: 1200px;
        height: 900px;
        background: radial-gradient(
          circle at 100px 100px, blue, transparent 200px) fixed;
      }
    </style>
  )HTML");

  LocalFrameView* frame_view = GetDocument().View();
  ScrollableArea* layout_viewport = frame_view->LayoutViewportScrollableArea();

  ScrollOffset scroll_offset(200, 150);
  layout_viewport->SetScrollOffset(scroll_offset, kUserScroll);
  frame_view->UpdateAllLifecyclePhases();

  bool rls = RuntimeEnabledFeatures::RootLayerScrollingEnabled();
  bool v175 = RuntimeEnabledFeatures::SlimmingPaintV175Enabled();
  CompositedLayerMapping* clm =
      GetLayoutView().Layer()->GetCompositedLayerMapping();

  // If we prefer compositing to LCD text, the fixed background should go in a
  // different layer from the scrolling content; otherwise, it should go in the
  // same layer.  With RLS, the scrolling content is in the scrolling contents
  // layer; without RLS, the scrolling content is in the main GraphicsLayer.
  GraphicsLayer* layer_for_background;
  if (prefer_compositing_to_lcd_text) {
    layer_for_background =
        rls ? clm->MainGraphicsLayer() : clm->BackgroundLayer();
  } else {
    layer_for_background =
        rls ? clm->ScrollingContentsLayer() : clm->MainGraphicsLayer();
  }
  const DisplayItemList& display_items =
      layer_for_background->GetPaintController().GetDisplayItemList();
  const DisplayItem& background =
      display_items[rls && !prefer_compositing_to_lcd_text && !v175 ? 2 : 0];
  EXPECT_EQ(background.GetType(), kDocumentBackgroundType);
  DisplayItemClient* expected_client;
  if (rls && !prefer_compositing_to_lcd_text)
    expected_client = GetLayoutView().Layer()->GraphicsLayerBacking();
  else
    expected_client = &GetLayoutView();
  EXPECT_EQ(&background.Client(), expected_client);

  sk_sp<const PaintRecord> record =
      static_cast<const DrawingDisplayItem&>(background).GetPaintRecord();
  ASSERT_EQ(record->size(), 2u);
  cc::PaintOpBuffer::Iterator it(record.get());
  ASSERT_EQ((*++it)->GetType(), cc::PaintOpType::DrawRect);

  // This is the dest_rect_ calculated by BackgroundImageGeometry.  For a fixed
  // background in scrolling contents layer, its location is the scroll offset.
  SkRect rect = static_cast<const cc::DrawRectOp*>(*it)->rect;
  ASSERT_EQ(prefer_compositing_to_lcd_text ? ScrollOffset() : scroll_offset,
            ScrollOffset(rect.fLeft, rect.fTop));
}

TEST_P(ViewPainterTest, DocumentFixedBackgroundLowDPI) {
  RunFixedBackgroundTest(false);
}

TEST_P(ViewPainterTest, DocumentFixedBackgroundHighDPI) {
  RunFixedBackgroundTest(true);
}

TEST_P(ViewPainterTest, DocumentBackgroundWithScroll) {
  SetBodyInnerHTML("<div style='height: 5000px'></div>");

  const DisplayItemClient* background_item_client;
  const DisplayItemClient* background_chunk_client;
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    background_item_client = GetLayoutView().Layer()->GraphicsLayerBacking();
    background_chunk_client = background_item_client;
  } else {
    background_item_client = &GetLayoutView();
    background_chunk_client = GetLayoutView().Layer();
  }

  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 1,
      TestDisplayItem(*background_item_client, kDocumentBackgroundType));

  if (!RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;

  const auto& chunks = RootPaintController().GetPaintArtifact().PaintChunks();
  EXPECT_EQ(1u, chunks.size());
  const auto& chunk = chunks[0];
  EXPECT_EQ(background_chunk_client, &chunk.id.client);

  const auto& tree_state = chunk.properties.property_tree_state;
  EXPECT_EQ(EffectPaintPropertyNode::Root(), tree_state.Effect());
  const auto* properties = GetLayoutView().FirstFragment().PaintProperties();
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    EXPECT_EQ(properties->ScrollTranslation(), tree_state.Transform());
    EXPECT_EQ(properties->OverflowClip(), tree_state.Clip());
  } else {
    EXPECT_EQ(nullptr, properties);
    const auto* frame_view = GetDocument().View();
    EXPECT_EQ(frame_view->ScrollTranslation(), tree_state.Transform());
    EXPECT_EQ(frame_view->ContentClip(), tree_state.Clip());
  }
}

}  // namespace blink
