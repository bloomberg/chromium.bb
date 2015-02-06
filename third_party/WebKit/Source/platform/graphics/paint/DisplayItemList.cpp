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
    ASSERT(m_newPaints.isEmpty());
    return m_paintList;
}

void DisplayItemList::add(WTF::PassOwnPtr<DisplayItem> displayItem)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());

    if (displayItem->isEnd()) {
        ASSERT(!m_newPaints.isEmpty());
        if (m_newPaints.last()->isBegin()) {
            ASSERT(displayItem->isEndAndPairedWith(*m_newPaints.last()));
            // Remove empty pairs.
            m_newPaints.removeLast();
            return;
        }
    }
    m_newPaints.append(displayItem);
}

void DisplayItemList::invalidate(DisplayItemClient client)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    // Can only be called during layout/paintInvalidation, not during painting.
    ASSERT(m_newPaints.isEmpty());
    m_cachedDisplayItemIndicesByClient.remove(client);
}

void DisplayItemList::invalidateAll()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    // Can only be called during layout/paintInvalidation, not during painting.
    ASSERT(m_newPaints.isEmpty());
    m_paintList.clear();
    m_cachedDisplayItemIndicesByClient.clear();
}

bool DisplayItemList::clientCacheIsValid(DisplayItemClient client) const
{
    return RuntimeEnabledFeatures::slimmingPaintDisplayItemCacheEnabled() && m_cachedDisplayItemIndicesByClient.contains(client);
}

size_t DisplayItemList::findMatchingCachedItem(const DisplayItem& displayItem)
{
    ASSERT(displayItem.isCached() || displayItem.isSubtreeCached());
    ASSERT(clientCacheIsValid(displayItem.client()));

    Vector<size_t>& indices = m_cachedDisplayItemIndicesByClient.find(displayItem.client())->value;
    DisplayItem::Type matchingType = displayItem.isCached()
        ? DisplayItem::cachedTypeToDrawingType(displayItem.type())
        : DisplayItem::subtreeCachedTypeToBeginSubtreeType(displayItem.type());

    for (size_t index : indices) {
        OwnPtr<DisplayItem>& existingItem = m_paintList[index];
        ASSERT(!existingItem || existingItem->client() == displayItem.client());
        if (existingItem && existingItem->type() == matchingType)
            return index;
    }

    // Previously the client generated a empty picture or an empty subtree
    // which is not stored in the cache.
    return kNotFound;
}

void DisplayItemList::appendDisplayItem(PaintList& list, DisplayItemIndicesByClientMap& displayItemIndicesByClient, WTF::PassOwnPtr<DisplayItem> displayItem)
{
    DisplayItemIndicesByClientMap::iterator it = displayItemIndicesByClient.find(displayItem->client());
    Vector<size_t>& indices = it == displayItemIndicesByClient.end() ?
        displayItemIndicesByClient.add(displayItem->client(), Vector<size_t>()).storedValue->value : it->value;
    indices.append(list.size());

    list.append(displayItem);
}

void DisplayItemList::copyCachedItems(const DisplayItem& displayItem, PaintList& list, DisplayItemIndicesByClientMap& displayItemIndicesByClient)
{
    size_t index = findMatchingCachedItem(displayItem);
    if (index == kNotFound)
        return;

    if (displayItem.isCached()) {
        appendDisplayItem(list, displayItemIndicesByClient, m_paintList[index].release());
        return;
    }

    ASSERT(m_paintList[index]->isBeginSubtree());
    DisplayItem* beginSubtree = m_paintList[index].get();
    DisplayItem::Type endSubtreeType = DisplayItem::beginSubtreeTypeToEndSubtreeType(beginSubtree->type());
    do {
        if (clientCacheIsValid(m_paintList[index]->client()))
            appendDisplayItem(list, displayItemIndicesByClient, m_paintList[index].release());
        ++index;
    } while (list.last()->client() != beginSubtree->client() || list.last()->type() != endSubtreeType);
}

// Update the existing paintList by removing invalidated entries, updating
// repainted ones, and appending new items.
// - For CachedDisplayItem, copy the corresponding cached DrawingDisplayItem;
// - For SubtreeCachedDisplayItem, copy the cached display items between the
//   corresponding BeginSubtreeDisplayItem and EndSubtreeDisplayItem (incl.);
// - Otherwise, copy the new display item.
//
// The algorithm is O(|existing paint list| + |newly painted list|).
// Coefficients are related to the ratio of [Subtree]CachedDisplayItems
// and the average number of (Drawing|BeginSubtree)DisplayItems per client.
void DisplayItemList::updatePaintList()
{
    if (!RuntimeEnabledFeatures::slimmingPaintDisplayItemCacheEnabled()) {
        m_paintList.clear();
        m_paintList.swap(m_newPaints);
        m_cachedDisplayItemIndicesByClient.clear();
        return;
    }

    PaintList updatedList;
    DisplayItemIndicesByClientMap newCachedDisplayItemIndicesByClient;

    for (OwnPtr<DisplayItem>& newDisplayItem : m_newPaints) {
        if (newDisplayItem->isCached() || newDisplayItem->isSubtreeCached())
            copyCachedItems(*newDisplayItem, updatedList, newCachedDisplayItemIndicesByClient);
        else
            appendDisplayItem(updatedList, newCachedDisplayItemIndicesByClient, newDisplayItem.release());
    }

    m_newPaints.clear();
    m_paintList.clear();
    m_paintList.swap(updatedList);
    m_cachedDisplayItemIndicesByClient.clear();
    m_cachedDisplayItemIndicesByClient.swap(newCachedDisplayItemIndicesByClient);
}

#ifndef NDEBUG

WTF::String DisplayItemList::paintListAsDebugString(const PaintList& list) const
{
    StringBuilder stringBuilder;
    for (size_t i = 0; i < list.size(); ++i) {
        const OwnPtr<DisplayItem>& displayItem = list[i];
        if (i)
            stringBuilder.append(",\n");
        if (!displayItem) {
            stringBuilder.append("null");
            continue;
        }
        stringBuilder.append(String::format("{index: %d, ", (int)i));
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

#endif // ifndef NDEBUG

void DisplayItemList::replay(GraphicsContext* context)
{
    updatePaintList();
    for (auto& displayItem : m_paintList)
        displayItem->replay(context);
}

} // namespace blink
