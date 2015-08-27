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
    if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        return false;

    ASSERT(context.displayItemList());

    if (context.displayItemList()->displayItemConstructionIsDisabled() || RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
        return false;

    if (!context.displayItemList()->clientCacheIsValid(client.displayItemClient()))
        return false;

    context.displayItemList()->createAndAppend<CachedDisplayItem>(client, DisplayItem::CachedSubsequence);
    return true;
}

SubsequenceRecorder::SubsequenceRecorder(GraphicsContext& context, const DisplayItemClientWrapper& client)
    : m_displayItemList(context.displayItemList())
    , m_client(client)
{
    if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        return;

    ASSERT(m_displayItemList);
    m_displayItemList->createAndAppend<BeginSubsequenceDisplayItem>(m_client);
}

SubsequenceRecorder::~SubsequenceRecorder()
{
    if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        return;

    // Don't remove no-op BeginSubsequence/EndSubsequence pairs because we need to
    // match them later with CachedSubsequences.
    m_displayItemList->createAndAppend<EndSubsequenceDisplayItem>(m_client);
}

} // namespace blink
