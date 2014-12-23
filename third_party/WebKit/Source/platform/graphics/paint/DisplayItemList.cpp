// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItemList.h"

#include "platform/NotImplemented.h"
#include "platform/RuntimeEnabledFeatures.h"
#ifndef NDEBUG
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/text/StringBuilder.h"
#include <stdio.h>
#endif

namespace blink {

const PaintList& DisplayItemList::paintList()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());

    updatePaintList();
    return m_paintList;
}

void DisplayItemList::add(WTF::PassOwnPtr<DisplayItem> displayItem)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    m_newPaints.append(displayItem);
}

void DisplayItemList::invalidate(DisplayItemClient client)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    m_cachedClients.remove(client);
}

void DisplayItemList::invalidateAll()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    // Can only be called during layout/paintInvalidation, not during painting.
    ASSERT(m_newPaints.isEmpty());
    m_paintList.clear();
    m_cachedClients.clear();
}

PaintList::iterator DisplayItemList::findNextMatchingCachedItem(PaintList::iterator begin, const DisplayItem& displayItem)
{
    PaintList::iterator end = m_paintList.end();

    if (!clientCacheIsValid(displayItem.client()))
        return end;

    for (PaintList::iterator it = begin; it != end; ++it) {
        DisplayItem& existing = **it;
        if (existing.idsEqual(displayItem))
            return it;
    }

    ASSERT_NOT_REACHED();
    return end;
}

static void appendDisplayItem(PaintList& list, HashSet<DisplayItemClient>& clients, WTF::PassOwnPtr<DisplayItem> displayItem)
{
    clients.add(displayItem->client());
    list.append(displayItem);
}

// Update the existing paintList by removing invalidated entries, updating
// repainted ones, and appending new items.
//
// The algorithm is O(|existing paint list| + |newly painted list|): by using
// the ordering implied by the existing paint list, extra treewalks are avoided.
void DisplayItemList::updatePaintList()
{
    PaintList updatedList;
    HashSet<DisplayItemClient> newCachedClients;

    PaintList::iterator paintListIt = m_paintList.begin();
    PaintList::iterator paintListEnd = m_paintList.end();

    for (OwnPtr<DisplayItem>& newDisplayItem : m_newPaints) {
        PaintList::iterator cachedItemIt = findNextMatchingCachedItem(paintListIt, *newDisplayItem);
        if (cachedItemIt != paintListEnd) {
            // Copy all of the existing items over until we hit the matching cached item.
            for (; paintListIt != cachedItemIt; ++paintListIt) {
                if (clientCacheIsValid((*paintListIt)->client()))
                    appendDisplayItem(updatedList, newCachedClients, paintListIt->release());
            }

            // Use the cached item for the new display item.
            appendDisplayItem(updatedList, newCachedClients, cachedItemIt->release());
            ++paintListIt;
        } else {
            // If the new display item is a cached placeholder, we should have found
            // the cached display item.
            ASSERT(!newDisplayItem->isCached());

            // Copy over the new item.
            appendDisplayItem(updatedList, newCachedClients, newDisplayItem.release());
        }
    }

    // Copy over any remaining items that are validly cached.
    for (; paintListIt != paintListEnd; ++paintListIt) {
        if (clientCacheIsValid((*paintListIt)->client()))
            appendDisplayItem(updatedList, newCachedClients, paintListIt->release());
    }

    m_newPaints.clear();
    m_paintList.clear();
    m_paintList.swap(updatedList);
    m_cachedClients.clear();
    m_cachedClients.swap(newCachedClients);
}

#ifndef NDEBUG

WTF::String DisplayItemList::paintListAsDebugString(const PaintList& list) const
{
    StringBuilder stringBuilder;
    bool isFirst = true;
    for (auto& displayItem : list) {
        if (!isFirst)
            stringBuilder.append(",\n");
        isFirst = false;
        stringBuilder.append('{');
        displayItem->dumpPropertiesAsDebugString(stringBuilder);
        stringBuilder.append(", cacheIsValid: ");
        stringBuilder.append(clientCacheIsValid(displayItem->client()) ? "true" : "false");
        stringBuilder.append('}');
    }
    return stringBuilder.toString();
}

void DisplayItemList::showDebugData() const
{
    fprintf(stderr, "paint list: [%s]\n", paintListAsDebugString(m_paintList).utf8().data());
    fprintf(stderr, "new paints: [%s]\n", paintListAsDebugString(m_newPaints).utf8().data());
}

#endif

void DisplayItemList::replay(GraphicsContext* context)
{
    updatePaintList();
    for (auto& displayItem : m_paintList)
        displayItem->replay(context);
}

} // namespace blink
