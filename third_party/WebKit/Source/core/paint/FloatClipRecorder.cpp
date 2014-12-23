// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/FloatClipRecorder.h"

#include "core/rendering/PaintPhase.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/FloatClipDisplayItem.h"

namespace blink {

FloatClipRecorder::FloatClipRecorder(GraphicsContext& context, DisplayItemClient client, const PaintPhase& paintPhase, const FloatRect& clipRect)
    : m_context(context)
    , m_client(client)
{
    DisplayItem::Type type = paintPhaseToFloatClipType(paintPhase);
    OwnPtr<FloatClipDisplayItem> clipDisplayItem = FloatClipDisplayItem::create(m_client, type, clipRect);

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context.displayItemList());
        m_context.displayItemList()->add(clipDisplayItem.release());
    } else {
        clipDisplayItem->replay(&m_context);
    }
}

FloatClipRecorder::~FloatClipRecorder()
{
    OwnPtr<EndFloatClipDisplayItem> endClipDisplayItem = EndFloatClipDisplayItem::create(m_client);
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context.displayItemList());
        m_context.displayItemList()->add(endClipDisplayItem.release());
    } else {
        endClipDisplayItem->replay(&m_context);
    }
}

DisplayItem::Type FloatClipRecorder::paintPhaseToFloatClipType(PaintPhase paintPhase)
{
    switch (paintPhase) {
    case PaintPhaseForeground:
        return DisplayItem::FloatClipForeground;
        break;
    case PaintPhaseSelection:
        return DisplayItem::FloatClipSelection;
        break;
    case PaintPhaseSelfOutline:
        return DisplayItem::FloatClipSelfOutline;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    // This should never happen.
    return DisplayItem::FloatClipForeground;
}

} // namespace blink
