// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/ClipRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

ClipRecorder::ClipRecorder(GraphicsContext& context, const DisplayItemClientWrapper& client, DisplayItem::Type type, const LayoutRect& clipRect)
    : m_client(client)
    , m_context(context)
    , m_type(type)
{
    ASSERT(m_context.displayItemList());
    if (m_context.displayItemList()->displayItemConstructionIsDisabled())
        return;
    m_context.displayItemList()->createAndAppend<ClipDisplayItem>(m_client, type, pixelSnappedIntRect(clipRect));
}

ClipRecorder::~ClipRecorder()
{
    ASSERT(m_context.displayItemList());
    if (!m_context.displayItemList()->displayItemConstructionIsDisabled()) {
        if (m_context.displayItemList()->lastDisplayItemIsNoopBegin())
            m_context.displayItemList()->removeLastDisplayItem();
        else
            m_context.displayItemList()->createAndAppend<EndClipDisplayItem>(m_client, DisplayItem::clipTypeToEndClipType(m_type));
    }
}

} // namespace blink
