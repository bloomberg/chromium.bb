// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/SubtreeRecorder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/CachedDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/SubtreeDisplayItem.h"

namespace blink {

SubtreeRecorder::SubtreeRecorder(GraphicsContext& context, const DisplayItemClientWrapper& client, int paintPhase)
    : m_displayItemList(context.displayItemList())
    , m_client(client)
    , m_paintPhase(paintPhase)
    , m_canUseCache(false)
#if ENABLE(ASSERT)
    , m_checkedCanUseCache(false)
#endif
{
    if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        return;

    ASSERT(m_displayItemList);

    // TODO(wangxianzhu): Implement subtree caching.

    if (!m_canUseCache)
        m_displayItemList->createAndAppend<BeginSubtreeDisplayItem>(m_client, DisplayItem::paintPhaseToBeginSubtreeType(paintPhase));
}

SubtreeRecorder::~SubtreeRecorder()
{
    if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        return;

    ASSERT(m_checkedCanUseCache);
    if (m_canUseCache)
        m_displayItemList->createAndAppend<CachedDisplayItem>(m_client, DisplayItem::paintPhaseToCachedSubtreeType(m_paintPhase));
    else if (m_displayItemList->lastDisplayItemIsNoopBegin())
        m_displayItemList->removeLastDisplayItem();
    else
        m_displayItemList->createAndAppend<EndSubtreeDisplayItem>(m_client, DisplayItem::paintPhaseToEndSubtreeType(m_paintPhase));
}

bool SubtreeRecorder::canUseCache() const
{
#if ENABLE(ASSERT)
    m_checkedCanUseCache = true;
#endif
    return m_canUseCache;
}

} // namespace blink
