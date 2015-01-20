// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/ClipPathRecorder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

ClipPathRecorder::ClipPathRecorder(GraphicsContext& context, DisplayItemClient client, const Path& clipPath, WindRule windRule)
    : m_context(context)
    , m_client(client)
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context.displayItemList());
        m_context.displayItemList()->add(BeginClipPathDisplayItem::create(m_client, clipPath, windRule));
    } else {
        BeginClipPathDisplayItem clipPathDisplayItem(m_client, clipPath, windRule);
        clipPathDisplayItem.replay(&m_context);
    }
}

ClipPathRecorder::~ClipPathRecorder()
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context.displayItemList());
        m_context.displayItemList()->add(EndClipPathDisplayItem::create(m_client));
    } else {
        EndClipPathDisplayItem endClipPathDisplayItem(m_client);
        endClipPathDisplayItem.replay(&m_context);
    }
}

} // namespace blink
