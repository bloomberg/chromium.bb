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

TransparencyRecorder::TransparencyRecorder(GraphicsContext* graphicsContext, DisplayItemClient client, const CompositeOperator preTransparencyLayerCompositeOp, const WebBlendMode& preTransparencyLayerBlendMode, const float opacity, const CompositeOperator postTransparencyLayerCompositeOp)
    : m_client(client)
    , m_graphicsContext(graphicsContext)
{
    OwnPtr<BeginTransparencyDisplayItem> beginTransparencyDisplayItem = BeginTransparencyDisplayItem::create(m_client, DisplayItem::BeginTransparency, preTransparencyLayerCompositeOp, preTransparencyLayerBlendMode, opacity, postTransparencyLayerCompositeOp);
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_graphicsContext->displayItemList());
        m_graphicsContext->displayItemList()->add(beginTransparencyDisplayItem.release());
    } else {
        beginTransparencyDisplayItem->replay(graphicsContext);
    }
}

TransparencyRecorder::~TransparencyRecorder()
{
    OwnPtr<EndTransparencyDisplayItem> endTransparencyDisplayItem = EndTransparencyDisplayItem::create(m_client, DisplayItem::EndTransparency);
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_graphicsContext->displayItemList());
        m_graphicsContext->displayItemList()->add(endTransparencyDisplayItem.release());
    } else {
        endTransparencyDisplayItem->replay(m_graphicsContext);
    }
}

} // namespace blink
