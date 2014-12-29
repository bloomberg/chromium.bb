// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/ClipRecorder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

ClipRecorder::ClipRecorder(DisplayItemClient client, GraphicsContext* context, DisplayItem::Type type, const LayoutRect& clipRect, SkRegion::Op operation)
    : m_client(client)
    , m_context(context)
{
    OwnPtr<ClipDisplayItem> clipDisplayItem = ClipDisplayItem::create(m_client, type, pixelSnappedIntRect(clipRect), operation);

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context->displayItemList());
        m_context->displayItemList()->add(clipDisplayItem.release());
    } else {
        clipDisplayItem->replay(m_context);
    }
}

ClipRecorder::~ClipRecorder()
{
    OwnPtr<EndClipDisplayItem> endClipDisplayItem = EndClipDisplayItem::create(m_client);

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context->displayItemList());
        m_context->displayItemList()->add(endClipDisplayItem.release());
    } else {
        endClipDisplayItem->replay(m_context);
    }
}

} // namespace blink
