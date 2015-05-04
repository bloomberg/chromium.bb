// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItemList.h"

#include "platform/NotImplemented.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"

#ifndef NDEBUG
#include "platform/graphics/LoggingCanvas.h"
#include "wtf/text/StringBuilder.h"
#include <stdio.h>
#endif

namespace blink {

const DisplayItems& DisplayItemList::displayItems() const
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    ASSERT(m_newDisplayItems.isEmpty());
    return m_currentDisplayItems;
}

void DisplayItemList::add(WTF::PassOwnPtr<DisplayItem> displayItem)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    ASSERT(!m_constructionDisabled);

    if (displayItem->isEnd()) {
        ASSERT(!m_newDisplayItems.isEmpty());
        if (m_newDisplayItems.last()->isBegin() && !m_newDisplayItems.last()->drawsContent()) {
            ASSERT(displayItem->isEndAndPairedWith(*m_newDisplayItems.last()));
            // Remove the beginning display item of this empty pair.
            m_newDisplayItems.removeLast();
#if ENABLE(ASSERT)
            // Also remove the index pointing to the removed display item.
            DisplayItemIndicesByClientMap::iterator it = m_newDisplayItemIndicesByClient.find(displayItem->client());
            if (it != m_newDisplayItemIndicesByClient.end()) {
                Vector<size_t>& indices = it->value;
                if (!indices.isEmpty() && indices.last() == m_newDisplayItems.size())
                    indices.removeLast();
            }
#endif
            return;
        }
    }

    if (!m_scopeStack.isEmpty())
        displayItem->setScope(m_scopeStack.last().client, m_scopeStack.last().id);

#if ENABLE(ASSERT)
    size_t index = findMatchingItemFromIndex(*displayItem, displayItem->type(), m_newDisplayItemIndicesByClient, m_newDisplayItems);
    if (index != kNotFound) {
#ifndef NDEBUG
        showDebugData();
        WTFLogAlways("DisplayItem %s has duplicated id with previous %s (index=%d)\n",
            displayItem->asDebugString().utf8().data(), m_newDisplayItems[index]->asDebugString().utf8().data(), static_cast<int>(index));
#endif
        ASSERT_NOT_REACHED();
    }
    addItemToIndex(*displayItem, m_newDisplayItems.size(), m_newDisplayItemIndicesByClient);
#endif

    m_newDisplayItems.append(displayItem);
}

void DisplayItemList::beginScope(DisplayItemClient client)
{
    ClientScopeIdMap::iterator it = m_clientScopeIdMap.find(client);
    int scopeId;
    if (it == m_clientScopeIdMap.end()) {
        m_clientScopeIdMap.add(client, 0);
        scopeId = 0;
    } else {
        scopeId = ++it->value;
    }
    m_scopeStack.append(Scope(client, scopeId));
}

void DisplayItemList::endScope(DisplayItemClient client)
{
    m_scopeStack.removeLast();
}

void DisplayItemList::invalidate(DisplayItemClient client)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    // Can only be called during layout/paintInvalidation, not during painting.
    ASSERT(m_newDisplayItems.isEmpty());
    updateValidlyCachedClientsIfNeeded();
    m_validlyCachedClients.remove(client);
}

void DisplayItemList::invalidateAll()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    // Can only be called during layout/paintInvalidation, not during painting.
    ASSERT(m_newDisplayItems.isEmpty());
    m_currentDisplayItems.clear();
    m_validlyCachedClients.clear();
    m_validlyCachedClientsDirty = false;
}

bool DisplayItemList::clientCacheIsValid(DisplayItemClient client) const
{
    updateValidlyCachedClientsIfNeeded();
    return m_validlyCachedClients.contains(client);
}

size_t DisplayItemList::findMatchingItemFromIndex(const DisplayItem& displayItem, DisplayItem::Type matchingType, const DisplayItemIndicesByClientMap& displayItemIndicesByClient, const DisplayItems& list)
{
    DisplayItemIndicesByClientMap::const_iterator it = displayItemIndicesByClient.find(displayItem.client());
    if (it == displayItemIndicesByClient.end())
        return kNotFound;

    const Vector<size_t>& indices = it->value;
    for (size_t index : indices) {
        const OwnPtr<DisplayItem>& existingItem = list[index];
        ASSERT(!existingItem || existingItem->client() == displayItem.client());
        if (existingItem && existingItem->idsEqual(displayItem, matchingType))
            return index;
    }

    return kNotFound;
}

void DisplayItemList::addItemToIndex(const DisplayItem& displayItem, size_t index, DisplayItemIndicesByClientMap& displayItemIndicesByClient)
{
    // Only need to index DrawingDisplayItems and FIXME: BeginSubtreeDisplayItems.
    if (!displayItem.isDrawing())
        return;

    DisplayItemIndicesByClientMap::iterator it = displayItemIndicesByClient.find(displayItem.client());
    Vector<size_t>& indices = it == displayItemIndicesByClient.end() ?
        displayItemIndicesByClient.add(displayItem.client(), Vector<size_t>()).storedValue->value : it->value;
    indices.append(index);
}

size_t DisplayItemList::findOutOfOrderCachedItem(size_t& currentDisplayItemsIndex, const DisplayItem& displayItem, DisplayItem::Type matchingType, DisplayItemIndicesByClientMap& displayItemIndicesByClient)
{
    ASSERT(displayItem.isCached());
    ASSERT(clientCacheIsValid(displayItem.client()));

    size_t foundIndex = findMatchingItemFromIndex(displayItem, matchingType, displayItemIndicesByClient, m_currentDisplayItems);
    if (foundIndex != kNotFound)
        return foundIndex;

    return findOutOfOrderCachedItemForward(currentDisplayItemsIndex, displayItem, matchingType, displayItemIndicesByClient);
}

// Find forward for the item and index all skipped indexable items.
size_t DisplayItemList::findOutOfOrderCachedItemForward(size_t& currentDisplayItemsIndex, const DisplayItem& displayItem, DisplayItem::Type matchingType, DisplayItemIndicesByClientMap& displayItemIndicesByClient)
{
    size_t currentDisplayItemsSize = m_currentDisplayItems.size();
    for (; currentDisplayItemsIndex < currentDisplayItemsSize; ++currentDisplayItemsIndex) {
        const DisplayItem* item = m_currentDisplayItems[currentDisplayItemsIndex].get();
        if (item && item->isDrawing() && m_validlyCachedClients.contains(item->client())) {
            if (item->idsEqual(displayItem, matchingType))
                return currentDisplayItemsIndex;

            addItemToIndex(*item, currentDisplayItemsIndex, displayItemIndicesByClient);
        }
    }
    return kNotFound;
}

// Update the existing display items by removing invalidated entries, updating
// repainted ones, and appending new items.
// - For CachedDisplayItem, copy the corresponding cached DrawingDisplayItem;
// - FIXME: Re-enable SubtreeCachedDisplayItem:
//   For SubtreeCachedDisplayItem, copy the cached display items between the
//   corresponding BeginSubtreeDisplayItem and EndSubtreeDisplayItem (incl.);
// - Otherwise, copy the new display item.
//
// The algorithm is O(|m_currentDisplayItems| + |m_newDisplayItems|).
// Coefficients are related to the ratio of out-of-order [Subtree]CachedDisplayItems
// and the average number of (Drawing|BeginSubtree)DisplayItems per client.
void DisplayItemList::commitNewDisplayItems()
{
    TRACE_EVENT0("blink,benchmark", "DisplayItemList::commitNewDisplayItems");

    // These data structures are used during painting only.
    m_clientScopeIdMap.clear();
    ASSERT(m_scopeStack.isEmpty());
    m_scopeStack.clear();
#if ENABLE(ASSERT)
    m_newDisplayItemIndicesByClient.clear();
#endif

    if (m_currentDisplayItems.isEmpty()) {
#if ENABLE(ASSERT)
        for (auto& item : m_newDisplayItems)
            ASSERT(!item->isCached() && !item->isSubtreeCached());
#endif
        m_currentDisplayItems.swap(m_newDisplayItems);
        m_validlyCachedClientsDirty = true;
        return;
    }

    updateValidlyCachedClientsIfNeeded();

    // Stores indices to valid DrawingDisplayItems in m_currentDisplayItems that have not been matched
    // by CachedDisplayItems during synchronized matching. The indexed items will be matched
    // by later out-of-order CachedDisplayItems in m_newDisplayItems. This ensures that when
    // out-of-order CachedDisplayItems occur, we only traverse at most once over m_currentDisplayItems
    // looking for potential matches. Thus we can ensure that the algorithm runs in linear time.
    DisplayItemIndicesByClientMap displayItemIndicesByClient;

#if ENABLE(ASSERT)
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
        // Under-invalidation checking requires a full index of m_currentDisplayItems.
        for (size_t i = 0; i < m_currentDisplayItems.size(); ++i)
            addItemToIndex(*m_currentDisplayItems[i], i, displayItemIndicesByClient);
    }
#endif // ENABLE(ASSERT)

    DisplayItems updatedList;
    size_t currentDisplayItemsIndex = 0;
    for (auto& newDisplayItem : m_newDisplayItems) {
        DisplayItem::Type matchingType = newDisplayItem->type();
        if (newDisplayItem->isCached())
            matchingType = DisplayItem::cachedTypeToDrawingType(matchingType);
        bool isSynchronized = currentDisplayItemsIndex < m_currentDisplayItems.size()
            && m_currentDisplayItems[currentDisplayItemsIndex]
            && m_currentDisplayItems[currentDisplayItemsIndex]->idsEqual(*newDisplayItem, matchingType);

        if (newDisplayItem->isCached()) {
            ASSERT(!RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());
            ASSERT(clientCacheIsValid(newDisplayItem->client()));
            if (isSynchronized) {
                updatedList.append(m_currentDisplayItems[currentDisplayItemsIndex].release());
            } else {
                size_t foundIndex = findOutOfOrderCachedItem(currentDisplayItemsIndex, *newDisplayItem, matchingType, displayItemIndicesByClient);
                if (foundIndex != kNotFound) {
                    isSynchronized = (foundIndex == currentDisplayItemsIndex);
                    updatedList.append(m_currentDisplayItems[foundIndex].release());
                }
                // else: do nothing because previously the client generated a empty picture which is not stored in the cache.
            }
        } else {
#if ENABLE(ASSERT)
            if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
                checkCachedDisplayItemIsUnchanged(*newDisplayItem, displayItemIndicesByClient);
            else
                ASSERT(!newDisplayItem->isDrawing() || !clientCacheIsValid(newDisplayItem->client()));
#endif // ENABLE(ASSERT)
            updatedList.append(newDisplayItem.release());
        }

        if (isSynchronized)
            ++currentDisplayItemsIndex;
    }

#if ENABLE(ASSERT)
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
        checkNoRemainingCachedDisplayItems();
#endif // ENABLE(ASSERT)

    m_newDisplayItems.clear();
    m_validlyCachedClientsDirty = true;
    m_currentDisplayItems.clear();
    m_currentDisplayItems.swap(updatedList);
}

void DisplayItemList::updateValidlyCachedClientsIfNeeded() const
{
    if (!m_validlyCachedClientsDirty)
        return;

    m_validlyCachedClients.clear();
    m_validlyCachedClientsDirty = false;

    DisplayItemClient lastClient = nullptr;
    for (const auto& displayItem : m_currentDisplayItems) {
        if (displayItem->client() == lastClient)
            continue;
        lastClient = displayItem->client();
        m_validlyCachedClients.add(lastClient);
    }
}

#if ENABLE(ASSERT)

static void showUnderInvalidationError(const char* reason, const DisplayItem& displayItem)
{
#ifndef NDEBUG
    WTFLogAlways("%s: %s\nSee http://crbug.com/450725.", reason, displayItem.asDebugString().utf8().data());
#else
    WTFLogAlways("%s. Run debug build to get more details\nSee http://crbug.com/450725.", reason);
#endif // NDEBUG
}

void DisplayItemList::checkCachedDisplayItemIsUnchanged(const DisplayItem& displayItem, DisplayItemIndicesByClientMap& displayItemIndicesByClient)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());

    if (!displayItem.isDrawing() || !clientCacheIsValid(displayItem.client()))
        return;

    if (static_cast<const DrawingDisplayItem&>(displayItem).skipUnderInvalidationChecking())
        return;

    // If checking under-invalidation, we always generate new display item even if the client is not invalidated.
    // Checks if the new picture is the same as the cached old picture. If the new picture is different but
    // the client is not invalidated, issue error about under-invalidation.
    size_t index = findMatchingItemFromIndex(displayItem, displayItem.type(), displayItemIndicesByClient, m_currentDisplayItems);
    if (index == kNotFound) {
        showUnderInvalidationError("ERROR: under-invalidation: no cached display item", displayItem);
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr<const SkPicture> newPicture = static_cast<const DrawingDisplayItem&>(displayItem).picture();
    RefPtr<const SkPicture> oldPicture = static_cast<const DrawingDisplayItem&>(*m_currentDisplayItems[index]).picture();
    // Remove the display item from cache so that we can check if there are any remaining cached display items after merging.
    m_currentDisplayItems[index] = nullptr;

    if (!newPicture && !oldPicture)
        return;
    if (newPicture && oldPicture && newPicture->approximateOpCount() == oldPicture->approximateOpCount()) {
        SkDynamicMemoryWStream newPictureSerialized;
        newPicture->serialize(&newPictureSerialized);
        SkDynamicMemoryWStream oldPictureSerialized;
        oldPicture->serialize(&oldPictureSerialized);

        if (newPictureSerialized.bytesWritten() == oldPictureSerialized.bytesWritten()) {
            RefPtr<SkData> oldData = adoptRef(oldPictureSerialized.copyToData());
            RefPtr<SkData> newData = adoptRef(newPictureSerialized.copyToData());
            if (oldData->equals(newData.get()))
                return;
        }
    }

    showUnderInvalidationError("ERROR: under-invalidation: display item changed", displayItem);
#ifndef NDEBUG
    String oldPictureDebugString = oldPicture ? pictureAsDebugString(oldPicture.get()) : "None";
    String newPictureDebugString = newPicture ? pictureAsDebugString(newPicture.get()) : "None";
    WTFLogAlways("old picture:\n%s\n", oldPictureDebugString.utf8().data());
    WTFLogAlways("new picture:\n%s\n", newPictureDebugString.utf8().data());
#endif // NDEBUG

    ASSERT_NOT_REACHED();
}

void DisplayItemList::checkNoRemainingCachedDisplayItems()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());

    for (OwnPtr<DisplayItem>& displayItem : m_currentDisplayItems) {
        if (!displayItem || !displayItem->isDrawing() || !clientCacheIsValid(displayItem->client()))
            continue;
        showUnderInvalidationError("May be under-invalidation: no new display item", *displayItem);
    }
}

#endif // ENABLE(ASSERT)

#ifndef NDEBUG

WTF::String DisplayItemList::displayItemsAsDebugString(const DisplayItems& list) const
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
    WTFLogAlways("current display items: [%s]\n", displayItemsAsDebugString(m_currentDisplayItems).utf8().data());
    WTFLogAlways("new display items: [%s]\n", displayItemsAsDebugString(m_newDisplayItems).utf8().data());
}

#endif // ifndef NDEBUG

void DisplayItemList::replay(GraphicsContext& context) const
{
    TRACE_EVENT0("blink,benchmark", "DisplayItemList::replay");
    ASSERT(m_newDisplayItems.isEmpty());
    for (auto& displayItem : m_currentDisplayItems)
        displayItem->replay(context);
}

} // namespace blink
