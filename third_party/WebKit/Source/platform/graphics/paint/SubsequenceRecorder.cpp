// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/SubsequenceRecorder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/CachedDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/SubsequenceDisplayItem.h"

namespace blink {

bool SubsequenceRecorder::useCachedSubsequenceIfPossible(GraphicsContext& context, const DisplayItemClientWrapper& client, DisplayItem::Type type)
{
    if (!RuntimeEnabledFeatures::slimmingPaintSubsequenceCachingEnabled())
        return false;

    ASSERT(context.paintController());

    if (context.paintController()->displayItemConstructionIsDisabled())
        return false;

    if (!context.paintController()->clientCacheIsValid(client.displayItemClient()))
        return false;

    context.paintController()->createAndAppend<CachedDisplayItem>(client, DisplayItem::subsequenceTypeToCachedSubsequenceType(type));

#if ENABLE(ASSERT)
    // When under-invalidation checking is enabled, we output CachedSubsequence display item
    // followed by forced painting of the subsequence.
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
        return false;
#endif

    return true;
}

SubsequenceRecorder::SubsequenceRecorder(GraphicsContext& context, const DisplayItemClientWrapper& client, DisplayItem::Type type)
    : m_paintController(context.paintController())
    , m_client(client)
    , m_beginSubsequenceIndex(0)
    , m_type(type)
{
    if (!RuntimeEnabledFeatures::slimmingPaintSubsequenceCachingEnabled())
        return;

    ASSERT(m_paintController);
    if (m_paintController->displayItemConstructionIsDisabled())
        return;

    m_beginSubsequenceIndex = m_paintController->newDisplayItemList().size();
    m_paintController->createAndAppend<BeginSubsequenceDisplayItem>(m_client, type);
}

SubsequenceRecorder::~SubsequenceRecorder()
{
    if (!RuntimeEnabledFeatures::slimmingPaintSubsequenceCachingEnabled())
        return;

    if (m_paintController->displayItemConstructionIsDisabled())
        return;

    if (m_paintController->lastDisplayItemIsNoopBegin()) {
        ASSERT(m_beginSubsequenceIndex == m_paintController->newDisplayItemList().size() - 1);
        // Remove uncacheable no-op BeginSubsequence/EndSubsequence pairs.
        // Don't remove cacheable no-op pairs because we need to match them later with CachedSubsequences.
        if (m_paintController->newDisplayItemList().last().skippedCache()) {
            m_paintController->removeLastDisplayItem();
            return;
        }
    }

    m_paintController->createAndAppend<EndSubsequenceDisplayItem>(m_client, DisplayItem::subsequenceTypeToEndSubsequenceType(m_type));
}

void SubsequenceRecorder::setUncacheable()
{
    if (!RuntimeEnabledFeatures::slimmingPaintSubsequenceCachingEnabled())
        return;

    if (m_paintController->displayItemConstructionIsDisabled())
        return;

    ASSERT(m_paintController->newDisplayItemList()[m_beginSubsequenceIndex].isSubsequence());
    m_paintController->newDisplayItemList()[m_beginSubsequenceIndex].setSkippedCache();
}

} // namespace blink
