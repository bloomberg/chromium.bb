// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintControllerPaintTest.h"

#include "core/editing/FrameCaret.h"
#include "core/editing/FrameSelection.h"
#include "core/layout/LayoutText.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/page/FocusController.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayerPainter.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

namespace blink {

INSTANTIATE_TEST_CASE_P(All,
                        PaintControllerPaintTestForSlimmingPaintV1AndV2,
                        ::testing::Bool());

INSTANTIATE_TEST_CASE_P(All,
                        PaintControllerPaintTestForSlimmingPaintV2,
                        ::testing::Bool());

TEST_P(PaintControllerPaintTestForSlimmingPaintV1AndV2,
       FullDocumentPaintingWithCaret) {
  SetBodyInnerHTML(
      "<div id='div' contentEditable='true' style='outline:none'>XYZ</div>");
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  Element& div = *ToElement(GetDocument().body()->firstChild());
  InlineTextBox& text_inline_box =
      *ToLayoutText(div.firstChild()->GetLayoutObject())->FirstTextBox();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 2,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(text_inline_box, kForegroundType));
  } else {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 2,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(text_inline_box, kForegroundType));
  }

  div.focus();
  GetDocument().View()->UpdateAllLifecyclePhases();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 3,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(text_inline_box, kForegroundType),
        TestDisplayItem(GetDocument()
                            .GetFrame()
                            ->Selection()
                            .CaretDisplayItemClientForTesting(),
                        DisplayItem::kCaret));  // New!
  } else {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 3,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(text_inline_box, kForegroundType),
        TestDisplayItem(GetDocument()
                            .GetFrame()
                            ->Selection()
                            .CaretDisplayItemClientForTesting(),
                        DisplayItem::kCaret));  // New!
  }
}

TEST_P(PaintControllerPaintTestForSlimmingPaintV1AndV2, InlineRelayout) {
  SetBodyInnerHTML(
      "<div id='div' style='width:100px; height: 200px'>AAAAAAAAAA "
      "BBBBBBBBBB</div>");
  Element& div = *ToElement(GetDocument().body()->firstChild());
  LayoutBlock& div_block =
      *ToLayoutBlock(GetDocument().body()->firstChild()->GetLayoutObject());
  LayoutText& text = *ToLayoutText(div_block.FirstChild());
  InlineTextBox& first_text_box = *text.FirstTextBox();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 2,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(first_text_box, kForegroundType));
  } else {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 2,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(first_text_box, kForegroundType));
  }

  div.setAttribute(HTMLNames::styleAttr, "width: 10px; height: 200px");
  GetDocument().View()->UpdateAllLifecyclePhases();

  LayoutText& new_text = *ToLayoutText(div_block.FirstChild());
  InlineTextBox& new_first_text_box = *new_text.FirstTextBox();
  InlineTextBox& second_text_box = *new_text.FirstTextBox()->NextTextBox();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 3,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(new_first_text_box, kForegroundType),
        TestDisplayItem(second_text_box, kForegroundType));
  } else {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 3,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(new_first_text_box, kForegroundType),
        TestDisplayItem(second_text_box, kForegroundType));
  }
}

TEST_P(PaintControllerPaintTestForSlimmingPaintV2, ChunkIdClientCacheFlag) {
  SetBodyInnerHTML(
      "<div id='div' style='width: 200px; height: 200px; opacity: 0.5'>"
      "  <div style='width: 100px; height: 100px; background-color: "
      "blue'></div>"
      "  <div style='width: 100px; height: 100px; background-color: "
      "blue'></div>"
      "</div>");
  LayoutBlock& div = *ToLayoutBlock(GetLayoutObjectByElementId("div"));
  LayoutObject& sub_div = *div.FirstChild();
  LayoutObject& sub_div2 = *sub_div.NextSibling();
  EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
                      TestDisplayItem(sub_div, kBackgroundType),
                      TestDisplayItem(sub_div2, kBackgroundType));

  // Verify that the background does not scroll.
  const PaintChunk& background_chunk = RootPaintController().PaintChunks()[0];
  EXPECT_FALSE(background_chunk.properties.property_tree_state.Transform()
                   ->IsScrollTranslation());

  const EffectPaintPropertyNode* effect_node = div.PaintProperties()->Effect();
  EXPECT_EQ(0.5f, effect_node->Opacity());

  const PaintChunk& chunk = RootPaintController().PaintChunks()[1];
  EXPECT_EQ(*div.Layer(), chunk.id->client);
  EXPECT_EQ(effect_node, chunk.properties.property_tree_state.Effect());

  EXPECT_FALSE(div.Layer()->IsJustCreated());
  // Client used by only paint chunks and non-cachaeable display items but not
  // by any cacheable display items won't be marked as validly cached.
  EXPECT_FALSE(RootPaintController().ClientCacheIsValid(*div.Layer()));
  EXPECT_FALSE(RootPaintController().ClientCacheIsValid(div));
  EXPECT_TRUE(RootPaintController().ClientCacheIsValid(sub_div));
}

TEST_P(PaintControllerPaintTestForSlimmingPaintV2, CompositingNoFold) {
  SetBodyInnerHTML(
      "<div id='div' style='width: 200px; height: 200px; opacity: 0.5'>"
      "  <div style='width: 100px; height: 100px; background-color: "
      "blue'></div>"
      "</div>");
  LayoutBlock& div = *ToLayoutBlock(GetLayoutObjectByElementId("div"));
  LayoutObject& sub_div = *div.FirstChild();

  EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
                      TestDisplayItem(sub_div, kBackgroundType));
}

}  // namespace blink
