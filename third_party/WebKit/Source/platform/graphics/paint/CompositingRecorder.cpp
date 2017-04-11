// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/CompositingRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

CompositingRecorder::CompositingRecorder(GraphicsContext& graphics_context,
                                         const DisplayItemClient& client,
                                         const SkBlendMode xfer_mode,
                                         const float opacity,
                                         const FloatRect* bounds,
                                         ColorFilter color_filter)
    : client_(client), graphics_context_(graphics_context) {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;
  graphics_context.GetPaintController()
      .CreateAndAppend<BeginCompositingDisplayItem>(client_, xfer_mode, opacity,
                                                    bounds, color_filter);
}

CompositingRecorder::~CompositingRecorder() {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;
  // If the end of the current display list is of the form
  // [BeginCompositingDisplayItem] [DrawingDisplayItem], then fold the
  // BeginCompositingDisplayItem into a new DrawingDisplayItem that replaces
  // them both. This allows Skia to optimize for the case when the
  // BeginCompositingDisplayItem represents a simple opacity/color that can be
  // merged into the opacity/color of the drawing. See crbug.com/628831 for more
  // details.
  PaintController& paint_controller = graphics_context_.GetPaintController();
  const DisplayItem* last_display_item = paint_controller.LastDisplayItem(0);
  const DisplayItem* second_to_last_display_item =
      paint_controller.LastDisplayItem(1);
  // TODO(chrishtr): remove the call to LastDisplayItemIsSubsequenceEnd when
  // https://codereview.chromium.org/2768143002 lands.
  if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled() && last_display_item &&
      second_to_last_display_item && last_display_item->DrawsContent() &&
      second_to_last_display_item->GetType() ==
          DisplayItem::kBeginCompositing &&
      !paint_controller.LastDisplayItemIsSubsequenceEnd()) {
    FloatRect cull_rect(
        ((DrawingDisplayItem*)last_display_item)->GetPaintRecord()->cullRect());
    const DisplayItemClient& display_item_client = last_display_item->Client();
    DisplayItem::Type display_item_type = last_display_item->GetType();

    // Re-record the last two DisplayItems into a new drawing. The new item
    // cannot be cached, because it is a mutation of the DisplayItem the client
    // thought it was painting.
    paint_controller.BeginSkippingCache();
    {
#if DCHECK_IS_ON()
      // In the recorder's scope we remove the last two display items which
      // are combined into a new drawing.
      DisableListModificationCheck disabler;
#endif
      DrawingRecorder new_recorder(graphics_context_, display_item_client,
                                   display_item_type, cull_rect);
      DCHECK(!DrawingRecorder::UseCachedDrawingIfPossible(
          graphics_context_, display_item_client, display_item_type));

      second_to_last_display_item->Replay(graphics_context_);
      last_display_item->Replay(graphics_context_);
      EndCompositingDisplayItem(client_).Replay(graphics_context_);

      // Remove the DrawingDisplayItem.
      paint_controller.RemoveLastDisplayItem();
      // Remove the BeginCompositingDisplayItem.
      paint_controller.RemoveLastDisplayItem();
    }
    paint_controller.EndSkippingCache();
  } else {
    graphics_context_.GetPaintController().EndItem<EndCompositingDisplayItem>(
        client_);
  }
}

}  // namespace blink
