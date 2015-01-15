// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/CompositingRecorder.h"

#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

CompositingRecorder::CompositingRecorder(GraphicsContext* graphicsContext, DisplayItemClient client, const CompositeOperator preCompositeOp, const WebBlendMode& preBlendMode, const float opacity, const CompositeOperator postCompositeOp)
    : m_client(client)
    , m_graphicsContext(graphicsContext)
{
    OwnPtr<BeginCompositingDisplayItem> beginCompositingDisplayItem = BeginCompositingDisplayItem::create(m_client, DisplayItem::BeginCompositing, preCompositeOp, preBlendMode, opacity, postCompositeOp);
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_graphicsContext->displayItemList());
        m_graphicsContext->displayItemList()->add(beginCompositingDisplayItem.release());
    } else {
        beginCompositingDisplayItem->replay(graphicsContext);
    }
}

CompositingRecorder::~CompositingRecorder()
{
    OwnPtr<EndCompositingDisplayItem> endCompositingDisplayItem = EndCompositingDisplayItem::create(m_client, DisplayItem::EndCompositing);
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_graphicsContext->displayItemList());
        m_graphicsContext->displayItemList()->add(endCompositingDisplayItem.release());
    } else {
        endCompositingDisplayItem->replay(m_graphicsContext);
    }
}

} // namespace blink
