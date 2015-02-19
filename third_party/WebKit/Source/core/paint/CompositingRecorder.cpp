// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/CompositingRecorder.h"

#include "core/layout/Layer.h"
#include "core/layout/LayoutObject.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

CompositingRecorder::CompositingRecorder(GraphicsContext* graphicsContext, DisplayItemClient client, const CompositeOperator compositeOp, const WebBlendMode& blendMode, const float opacity)
    : m_client(client)
    , m_graphicsContext(graphicsContext)
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_graphicsContext->displayItemList());
        m_graphicsContext->displayItemList()->add(BeginCompositingDisplayItem::create(m_client, compositeOp, blendMode, opacity));
    } else {
        BeginCompositingDisplayItem beginCompositingDisplayItem(m_client, compositeOp, blendMode, opacity);
        beginCompositingDisplayItem.replay(graphicsContext);
    }
}

CompositingRecorder::~CompositingRecorder()
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_graphicsContext->displayItemList());
        m_graphicsContext->displayItemList()->add(EndCompositingDisplayItem::create(m_client));
    } else {
        EndCompositingDisplayItem endCompositingDisplayItem(m_client);
        endCompositingDisplayItem.replay(m_graphicsContext);
    }
}

} // namespace blink
