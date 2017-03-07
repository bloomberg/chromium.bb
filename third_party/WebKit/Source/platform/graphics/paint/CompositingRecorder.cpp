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

CompositingRecorder::CompositingRecorder(GraphicsContext& graphicsContext,
                                         const DisplayItemClient& client,
                                         const SkBlendMode xferMode,
                                         const float opacity,
                                         const FloatRect* bounds,
                                         ColorFilter colorFilter)
    : m_client(client), m_graphicsContext(graphicsContext) {
  graphicsContext.getPaintController()
      .createAndAppend<BeginCompositingDisplayItem>(m_client, xferMode, opacity,
                                                    bounds, colorFilter);
}

CompositingRecorder::~CompositingRecorder() {
  // If the end of the current display list is of the form
  // [BeginCompositingDisplayItem] [DrawingDisplayItem], then fold the
  // BeginCompositingDisplayItem into a new DrawingDisplayItem that replaces
  // them both. This allows Skia to optimize for the case when the
  // BeginCompositingDisplayItem represents a simple opacity/color that can be
  // merged into the opacity/color of the drawing. See crbug.com/628831 for more
  // details.
  PaintController& paintController = m_graphicsContext.getPaintController();
  const DisplayItem* lastDisplayItem = paintController.lastDisplayItem(0);
  const DisplayItem* secondToLastDisplayItem =
      paintController.lastDisplayItem(1);
  if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled() && lastDisplayItem &&
      secondToLastDisplayItem && lastDisplayItem->drawsContent() &&
      secondToLastDisplayItem->getType() == DisplayItem::kBeginCompositing) {
    FloatRect cullRect(
        ((DrawingDisplayItem*)lastDisplayItem)->GetPaintRecord()->cullRect());
    const DisplayItemClient& displayItemClient = lastDisplayItem->client();
    DisplayItem::Type displayItemType = lastDisplayItem->getType();

    // Re-record the last two DisplayItems into a new drawing. The new item
    // cannot be cached, because it is a mutation of the DisplayItem the client
    // thought it was painting.
    paintController.beginSkippingCache();
    {
#if DCHECK_IS_ON()
      // In the recorder's scope we remove the last two display items which
      // are combined into a new drawing.
      DisableListModificationCheck disabler;
#endif
      DrawingRecorder newRecorder(m_graphicsContext, displayItemClient,
                                  displayItemType, cullRect);
      DCHECK(!DrawingRecorder::useCachedDrawingIfPossible(
          m_graphicsContext, displayItemClient, displayItemType));

      secondToLastDisplayItem->replay(m_graphicsContext);
      lastDisplayItem->replay(m_graphicsContext);
      EndCompositingDisplayItem(m_client).replay(m_graphicsContext);

      // Remove the DrawingDisplayItem.
      paintController.removeLastDisplayItem();
      // Remove the BeginCompositingDisplayItem.
      paintController.removeLastDisplayItem();
    }
    paintController.endSkippingCache();
  } else {
    m_graphicsContext.getPaintController().endItem<EndCompositingDisplayItem>(
        m_client);
  }
}

}  // namespace blink
