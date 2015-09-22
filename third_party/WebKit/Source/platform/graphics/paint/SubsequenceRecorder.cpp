// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/SubsequenceRecorder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/CachedDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/SubsequenceDisplayItem.h"

namespace blink {

bool SubsequenceRecorder::useCachedSubsequenceIfPossible(GraphicsContext& context, const DisplayItemClientWrapper& client)
{
    if (!RuntimeEnabledFeatures::slimmingPaintSubsequenceCachingEnabled())
        return false;

    ASSERT(context.displayItemList());

    if (context.displayItemList()->displayItemConstructionIsDisabled())
        return false;

    if (!context.displayItemList()->clientCacheIsValid(client.displayItemClient()))
        return false;

    context.displayItemList()->createAndAppend<CachedDisplayItem>(client, DisplayItem::CachedSubsequence);

#if ENABLE(ASSERT)
    // When under-invalidation checking is enabled, we output CachedSubsequence display item
    // followed by forced painting of the subsequence.
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
        return false;
#endif

    return true;
}

SubsequenceRecorder::SubsequenceRecorder(GraphicsContext& context, const DisplayItemClientWrapper& client)
    : m_displayItemList(context.displayItemList())
    , m_client(client)
    , m_beginSubsequenceIndex(0)
{
    if (!RuntimeEnabledFeatures::slimmingPaintSubsequenceCachingEnabled())
        return;

    ASSERT(m_displayItemList);
    if (m_displayItemList->displayItemConstructionIsDisabled())
        return;

    m_beginSubsequenceIndex = m_displayItemList->newDisplayItems().size();
    m_displayItemList->createAndAppend<BeginSubsequenceDisplayItem>(m_client);
}

SubsequenceRecorder::~SubsequenceRecorder()
{
    if (!RuntimeEnabledFeatures::slimmingPaintSubsequenceCachingEnabled())
        return;

    if (m_displayItemList->displayItemConstructionIsDisabled())
        return;

    if (m_displayItemList->lastDisplayItemIsNoopBegin()) {
        ASSERT(m_beginSubsequenceIndex == m_displayItemList->newDisplayItems().size() - 1);
        // Remove uncacheable no-op BeginSubsequence/EndSubsequence pairs.
        // Don't remove cacheable no-op pairs because we need to match them later with CachedSubsequences.
        if (m_displayItemList->newDisplayItems().last().skippedCache()) {
            m_displayItemList->removeLastDisplayItem();
            return;
        }
    }

    m_displayItemList->createAndAppend<EndSubsequenceDisplayItem>(m_client);
}

void SubsequenceRecorder::setUncacheable()
{
    if (!RuntimeEnabledFeatures::slimmingPaintSubsequenceCachingEnabled())
        return;

    if (m_displayItemList->displayItemConstructionIsDisabled())
        return;

    ASSERT(m_displayItemList->newDisplayItems()[m_beginSubsequenceIndex].type() == DisplayItem::BeginSubsequence);
    m_displayItemList->newDisplayItems()[m_beginSubsequenceIndex].setSkippedCache();
}

} // namespace blink
