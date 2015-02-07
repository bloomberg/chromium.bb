// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/FloatClipRecorder.h"

#include "core/layout/PaintPhase.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/FloatClipDisplayItem.h"

namespace blink {

FloatClipRecorder::FloatClipRecorder(GraphicsContext& context, DisplayItemClient client, PaintPhase paintPhase, const FloatRect& clipRect)
    : m_context(context)
    , m_client(client)
    , m_clipType(DisplayItem::paintPhaseToFloatClipType(paintPhase))
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context.displayItemList());
        m_context.displayItemList()->add(FloatClipDisplayItem::create(m_client, m_clipType, clipRect));
    } else {
        FloatClipDisplayItem clipDisplayItem(m_client, m_clipType, clipRect);
        clipDisplayItem.replay(&m_context);
    }
}

FloatClipRecorder::~FloatClipRecorder()
{
    DisplayItem::Type endType = DisplayItem::floatClipTypeToEndFloatClipType(m_clipType);
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context.displayItemList());
        m_context.displayItemList()->add(EndFloatClipDisplayItem::create(m_client, endType));
    } else {
        EndFloatClipDisplayItem endClipDisplayItem(m_client, endType);
        endClipDisplayItem.replay(&m_context);
    }
}

} // namespace blink
