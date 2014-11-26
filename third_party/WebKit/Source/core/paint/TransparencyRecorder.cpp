// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/TransparencyRecorder.h"

#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/TransparencyDisplayItem.h"

namespace blink {

TransparencyRecorder::TransparencyRecorder(GraphicsContext* graphicsContext, const RenderObject* renderer, const WebBlendMode& blendMode, const float opacity)
    : m_renderer(renderer)
    , m_graphicsContext(graphicsContext)
{
    OwnPtr<BeginTransparencyDisplayItem> beginTransparencyDisplayItem = adoptPtr(new BeginTransparencyDisplayItem(renderer ? renderer->displayItemClient() : nullptr, DisplayItem::BeginTransparency, blendMode, opacity));
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        if (RenderLayer* container = renderer->enclosingLayer()->enclosingLayerForPaintInvalidationCrossingFrameBoundaries())
            container->graphicsLayerBacking()->displayItemList().add(beginTransparencyDisplayItem.release());
    } else {
        beginTransparencyDisplayItem->replay(graphicsContext);
    }
}

TransparencyRecorder::~TransparencyRecorder()
{
    OwnPtr<EndTransparencyDisplayItem> endTransparencyDisplayItem = adoptPtr(new EndTransparencyDisplayItem(m_renderer ? m_renderer->displayItemClient() : nullptr, DisplayItem::EndTransparency));
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        if (RenderLayer* container = m_renderer->enclosingLayer()->enclosingLayerForPaintInvalidationCrossingFrameBoundaries())
            container->graphicsLayerBacking()->displayItemList().add(endTransparencyDisplayItem.release());
    } else {
        endTransparencyDisplayItem->replay(m_graphicsContext);
    }
}

} // namespace blink
