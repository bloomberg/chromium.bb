// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ScrollRecorder.h"

#include "core/layout/PaintPhase.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/ScrollDisplayItem.h"

namespace blink {

ScrollRecorder::ScrollRecorder(GraphicsContext* context, DisplayItemClient client, PaintPhase phase, const IntSize& currentOffset)
    : m_client(client)
    , m_beginItemType(DisplayItem::paintPhaseToScrollType(phase))
    , m_context(context)
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context->displayItemList());
        m_context->displayItemList()->add(BeginScrollDisplayItem::create(m_client, m_beginItemType, currentOffset));
    } else {
        BeginScrollDisplayItem scrollDisplayItem(m_client, m_beginItemType, currentOffset);
        scrollDisplayItem.replay(m_context);
    }
}

ScrollRecorder::~ScrollRecorder()
{
    DisplayItem::Type endItemType = DisplayItem::scrollTypeToEndScrollType(m_beginItemType);
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context->displayItemList());
        m_context->displayItemList()->add(EndScrollDisplayItem::create(m_client, endItemType));
    } else {
        EndScrollDisplayItem endScrollDisplayItem(m_client, endItemType);
        endScrollDisplayItem.replay(m_context);
    }
}

} // namespace blink
