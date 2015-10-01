// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItemList.h"

#include "platform/NotImplemented.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

#ifndef NDEBUG
#include "platform/graphics/LoggingCanvas.h"
#include "wtf/text/StringBuilder.h"
#include <stdio.h>
#endif

namespace blink {

const DisplayItems& DisplayItemList::displayItems() const
{
    ASSERT(m_newDisplayItems.isEmpty());
    return m_currentDisplayItems;
}

bool DisplayItemList::lastDisplayItemIsNoopBegin() const
{
    if (m_newDisplayItems.isEmpty())
        return false;

    const auto& lastDisplayItem = m_newDisplayItems.last();
    return lastDisplayItem.isBegin() && !lastDisplayItem.drawsContent();
}

void DisplayItemList::removeLastDisplayItem()
{
    if (m_newDisplayItems.isEmpty())
        return;

#if ENABLE(ASSERT)
    // Also remove the index pointing to the removed display item.
    DisplayItemIndicesByClientMap::iterator it = m_newDisplayItemIndicesByClient.find(m_newDisplayItems.last().client());
    if (it != m_newDisplayItemIndicesByClient.end()) {
        Vector<size_t>& indices = it->value;
        if (!indices.isEmpty() && indices.last() == (m_newDisplayItems.size() - 1))
            indices.removeLast();
    }
#endif
    m_newDisplayItems.removeLast();
}

void DisplayItemList::processNewItem(DisplayItem& displayItem)
{
    ASSERT(!m_constructionDisabled);
    ASSERT(!skippingCache() || !displayItem.isCached());

    if (displayItem.isCached())
        ++m_numCachedItems;

#if ENABLE(ASSERT)
    // Verify noop begin/end pairs have been removed.
    if (m_newDisplayItems.size() >= 2 && displayItem.isEnd()) {
        const auto& beginDisplayItem = m_newDisplayItems[m_newDisplayItems.size() - 2];
        if (beginDisplayItem.isBegin() && !beginDisplayItem.isSubsequence() && !beginDisplayItem.drawsContent())
            ASSERT(!displayItem.isEndAndPairedWith(beginDisplayItem.type()));
    }
#endif

    if (!m_scopeStack.isEmpty())
        displayItem.setScope(m_scopeStack.last());

#if ENABLE(ASSERT)
    size_t index = findMatchingItemFromIndex(displayItem.nonCachedId(), m_newDisplayItemIndicesByClient, m_newDisplayItems);
    if (index != kNotFound) {
#ifndef NDEBUG
        showDebugData();
        WTFLogAlways("DisplayItem %s has duplicated id with previous %s (index=%d)\n",
            displayItem.asDebugString().utf8().data(), m_newDisplayItems[index].asDebugString().utf8().data(), static_cast<int>(index));
#endif
        ASSERT_NOT_REACHED();
    }
    addItemToIndexIfNeeded(displayItem, m_newDisplayItems.size() - 1, m_newDisplayItemIndicesByClient);
#endif // ENABLE(ASSERT)

    if (skippingCache())
        displayItem.setSkippedCache();
}

void DisplayItemList::beginScope()
{
    ASSERT_WITH_SECURITY_IMPLICATION(m_nextScope < UINT_MAX);
    m_scopeStack.append(m_nextScope++);
    beginSkippingCache();
}

void DisplayItemList::endScope()
{
    m_scopeStack.removeLast();
    endSkippingCache();
}

void DisplayItemList::invalidate(const DisplayItemClientWrapper& client)
{
    invalidateUntracked(client.displayItemClient());

    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled() && m_trackedPaintInvalidationObjects)
        m_trackedPaintInvalidationObjects->append(client.debugName());
}

void DisplayItemList::invalidateUntracked(DisplayItemClient client)
{
    // Can only be called during layout/paintInvalidation, not during painting.
    ASSERT(m_newDisplayItems.isEmpty());
    updateValidlyCachedClientsIfNeeded();
    m_validlyCachedClients.remove(client);
}

void DisplayItemList::invalidateAll()
{
    // Can only be called during layout/paintInvalidation, not during painting.
    ASSERT(m_newDisplayItems.isEmpty());
    m_currentDisplayItems.clear();
    m_validlyCachedClients.clear();
    m_validlyCachedClientsDirty = false;

    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled() && m_trackedPaintInvalidationObjects)
        m_trackedPaintInvalidationObjects->append("##ALL##");
}

bool DisplayItemList::clientCacheIsValid(DisplayItemClient client) const
{
    if (skippingCache())
        return false;
    updateValidlyCachedClientsIfNeeded();
    return m_validlyCachedClients.contains(client);
}

void DisplayItemList::invalidatePaintOffset(const DisplayItemClientWrapper& client)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintOffsetCachingEnabled());

    updateValidlyCachedClientsIfNeeded();
    m_validlyCachedClients.remove(client.displayItemClient());

    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled() && m_trackedPaintInvalidationObjects)
        m_trackedPaintInvalidationObjects->append(client.debugName());

#if ENABLE(ASSERT)
    m_clientsWithPaintOffsetInvalidations.add(client.displayItemClient());

    // Ensure no phases slipped in using the old paint offset which would indicate
    // different phases used different paint offsets, which should not happen.
    for (const auto& item : m_newDisplayItems)
        ASSERT(!item.isCached() || item.client() != client.displayItemClient());
#endif
}

#if ENABLE(ASSERT)
bool DisplayItemList::paintOffsetWasInvalidated(DisplayItemClient client) const
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintOffsetCachingEnabled());
    return m_clientsWithPaintOffsetInvalidations.contains(client);
}
#endif

size_t DisplayItemList::findMatchingItemFromIndex(const DisplayItem::Id& id, const DisplayItemIndicesByClientMap& displayItemIndicesByClient, const DisplayItems& list)
{
    DisplayItemIndicesByClientMap::const_iterator it = displayItemIndicesByClient.find(id.client);
    if (it == displayItemIndicesByClient.end())
        return kNotFound;

    const Vector<size_t>& indices = it->value;
    for (size_t index : indices) {
        const DisplayItem& existingItem = list[index];
        ASSERT(!existingItem.isValid() || existingItem.client() == id.client);
        if (existingItem.isValid() && id.matches(existingItem))
            return index;
    }

    return kNotFound;
}

void DisplayItemList::addItemToIndexIfNeeded(const DisplayItem& displayItem, size_t index, DisplayItemIndicesByClientMap& displayItemIndicesByClient)
{
    if (!displayItem.isCacheable())
        return;

    DisplayItemIndicesByClientMap::iterator it = displayItemIndicesByClient.find(displayItem.client());
    Vector<size_t>& indices = it == displayItemIndicesByClient.end() ?
        displayItemIndicesByClient.add(displayItem.client(), Vector<size_t>()).storedValue->value : it->value;
    indices.append(index);
}

struct DisplayItemList::OutOfOrderIndexContext {
    OutOfOrderIndexContext(DisplayItems::iterator begin) : nextItemToIndex(begin) { }

    DisplayItems::iterator nextItemToIndex;
    DisplayItemIndicesByClientMap displayItemIndicesByClient;
};

DisplayItems::iterator DisplayItemList::findOutOfOrderCachedItem(const DisplayItem::Id& id, OutOfOrderIndexContext& context)
{
    ASSERT(clientCacheIsValid(id.client));

    size_t foundIndex = findMatchingItemFromIndex(id, context.displayItemIndicesByClient, m_currentDisplayItems);
    if (foundIndex != kNotFound)
        return m_currentDisplayItems.begin() + foundIndex;

    return findOutOfOrderCachedItemForward(id, context);
}

// Find forward for the item and index all skipped indexable items.
DisplayItems::iterator DisplayItemList::findOutOfOrderCachedItemForward(const DisplayItem::Id& id, OutOfOrderIndexContext& context)
{
    DisplayItems::iterator currentEnd = m_currentDisplayItems.end();
    for (; context.nextItemToIndex != currentEnd; ++context.nextItemToIndex) {
        const DisplayItem& item = *context.nextItemToIndex;
        ASSERT(item.isValid());
        if (item.isCacheable() && clientCacheIsValid(item.client())) {
            if (id.matches(item))
                return context.nextItemToIndex++;

            addItemToIndexIfNeeded(item, context.nextItemToIndex - m_currentDisplayItems.begin(), context.displayItemIndicesByClient);
        }
    }
    return currentEnd;
}

void DisplayItemList::copyCachedSubsequence(DisplayItems::iterator& currentIt, DisplayItems& updatedList)
{
    ASSERT(currentIt->isSubsequence());
    ASSERT(!currentIt->scope());
    DisplayItem::Id endSubsequenceId(currentIt->client(), DisplayItem::subsequenceTypeToEndSubsequenceType(currentIt->type()), 0);
    do {
        // We should always find the EndSubsequence display item.
        ASSERT(currentIt != m_currentDisplayItems.end());
        ASSERT(currentIt->isValid());
        updatedList.appendByMoving(*currentIt);
        ++currentIt;
    } while (!endSubsequenceId.matches(updatedList.last()));
}

// Update the existing display items by removing invalidated entries, updating
// repainted ones, and appending new items.
// - For cached drawing display item, copy the corresponding cached DrawingDisplayItem;
// - For cached subsequence display item, copy the cached display items between the
//   corresponding SubsequenceDisplayItem and EndSubsequenceDisplayItem (incl.);
// - Otherwise, copy the new display item.
//
// The algorithm is O(|m_currentDisplayItems| + |m_newDisplayItems|).
// Coefficients are related to the ratio of out-of-order CachedDisplayItems
// and the average number of (Drawing|Subsequence)DisplayItems per client.
//
// TODO(pdr): Implement the DisplayListDiff algorithm for SlimmingPaintV2.
void DisplayItemList::commitNewDisplayItems(DisplayListDiff*)
{
    TRACE_EVENT2("blink,benchmark", "DisplayItemList::commitNewDisplayItems", "current_display_list_size", (int)m_currentDisplayItems.size(),
        "num_non_cached_new_items", (int)m_newDisplayItems.size() - m_numCachedItems);

    // These data structures are used during painting only.
    ASSERT(m_scopeStack.isEmpty());
    m_scopeStack.clear();
    m_nextScope = 1;
    ASSERT(!skippingCache());
#if ENABLE(ASSERT)
    m_newDisplayItemIndicesByClient.clear();
#endif

    if (m_currentDisplayItems.isEmpty()) {
#if ENABLE(ASSERT)
        for (const auto& item : m_newDisplayItems)
            ASSERT(!item.isCached());
#endif
        m_currentDisplayItems.swap(m_newDisplayItems);
        m_validlyCachedClientsDirty = true;
        m_numCachedItems = 0;
        return;
    }

    updateValidlyCachedClientsIfNeeded();

    // Stores indices to valid DrawingDisplayItems in m_currentDisplayItems that have not been matched
    // by CachedDisplayItems during synchronized matching. The indexed items will be matched
    // by later out-of-order CachedDisplayItems in m_newDisplayItems. This ensures that when
    // out-of-order CachedDisplayItems occur, we only traverse at most once over m_currentDisplayItems
    // looking for potential matches. Thus we can ensure that the algorithm runs in linear time.
    OutOfOrderIndexContext outOfOrderIndexContext(m_currentDisplayItems.begin());

    // TODO(jbroman): Consider revisiting this heuristic.
    DisplayItems updatedList(std::max(m_currentDisplayItems.usedCapacityInBytes(), m_newDisplayItems.usedCapacityInBytes()));
    DisplayItems::iterator currentIt = m_currentDisplayItems.begin();
    DisplayItems::iterator currentEnd = m_currentDisplayItems.end();
    for (DisplayItems::iterator newIt = m_newDisplayItems.begin(); newIt != m_newDisplayItems.end(); ++newIt) {
        const DisplayItem& newDisplayItem = *newIt;
        const DisplayItem::Id newDisplayItemId = newDisplayItem.nonCachedId();
        bool newDisplayItemHasCachedType = newDisplayItem.type() != newDisplayItemId.type;

        bool isSynchronized = currentIt != currentEnd && newDisplayItemId.matches(*currentIt);

        if (newDisplayItemHasCachedType) {
            ASSERT(newDisplayItem.isCached());
            ASSERT(clientCacheIsValid(newDisplayItem.client()) || (RuntimeEnabledFeatures::slimmingPaintOffsetCachingEnabled() && !paintOffsetWasInvalidated(newDisplayItem.client())));
            if (!isSynchronized) {
                currentIt = findOutOfOrderCachedItem(newDisplayItemId, outOfOrderIndexContext);

                if (currentIt == currentEnd) {
#ifndef NDEBUG
                    showDebugData();
                    WTFLogAlways("%s not found in m_currentDisplayItems\n", newDisplayItem.asDebugString().utf8().data());
#endif
                    ASSERT_NOT_REACHED();
                    // We did not find the cached display item. This should be impossible, but may occur if there is a bug
                    // in the system, such as under-invalidation, incorrect cache checking or duplicate display ids.
                    // In this case, attempt to recover rather than crashing or bailing on display of the rest of the display list.
                    continue;
                }
            }
#if ENABLE(ASSERT)
            if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
                DisplayItems::iterator temp = currentIt;
                checkUnderInvalidation(newIt, temp);
            }
#endif
            if (newDisplayItem.isCachedDrawing()) {
                updatedList.appendByMoving(*currentIt);
                ++currentIt;
            } else {
                ASSERT(newDisplayItem.isCachedSubsequence());
                copyCachedSubsequence(currentIt, updatedList);
                ASSERT(updatedList.last().isEndSubsequence());
            }
        } else {
            ASSERT(!newDisplayItem.isDrawing()
                || newDisplayItem.skippedCache()
                || !clientCacheIsValid(newDisplayItem.client())
                || (RuntimeEnabledFeatures::slimmingPaintOffsetCachingEnabled() && paintOffsetWasInvalidated(newDisplayItem.client())));

            updatedList.appendByMoving(*newIt);

            if (isSynchronized)
                ++currentIt;
        }
        // Items before currentIt should have been copied so we don't need to index them.
        if (currentIt - outOfOrderIndexContext.nextItemToIndex > 0)
            outOfOrderIndexContext.nextItemToIndex = currentIt;
    }

#if ENABLE(ASSERT)
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
        checkNoRemainingCachedDisplayItems();
#endif // ENABLE(ASSERT)

    m_newDisplayItems.clear();
    m_validlyCachedClientsDirty = true;
    m_currentDisplayItems.swap(updatedList);
    m_numCachedItems = 0;

#if ENABLE(ASSERT)
    m_clientsWithPaintOffsetInvalidations.clear();
#endif
}

size_t DisplayItemList::approximateUnsharedMemoryUsage() const
{
    size_t memoryUsage = sizeof(*this);

    // Memory outside this class due to m_currentDisplayItems.
    memoryUsage += m_currentDisplayItems.memoryUsageInBytes();

    // TODO(jbroman): If display items begin to have significant external memory
    // usage that's not shared with the embedder, we should account for it here.
    //
    // External objects, shared with the embedder, such as SkPicture, should be
    // excluded to avoid double counting. It is the embedder's responsibility to
    // count such objects.
    //
    // At time of writing, the only known case of unshared external memory was
    // the rounded clips vector in ClipDisplayItem, which is not expected to
    // contribute significantly to memory usage.

    // Memory outside this class due to m_newDisplayItems.
    ASSERT(m_newDisplayItems.isEmpty());
    memoryUsage += m_newDisplayItems.memoryUsageInBytes();

    return memoryUsage;
}

void DisplayItemList::updateValidlyCachedClientsIfNeeded() const
{
    if (!m_validlyCachedClientsDirty)
        return;

    m_validlyCachedClients.clear();
    m_validlyCachedClientsDirty = false;

    DisplayItemClient lastAddedClient = nullptr;
    for (const DisplayItem& displayItem : m_currentDisplayItems) {
        if (displayItem.client() == lastAddedClient)
            continue;
        if (displayItem.isCacheable()) {
            lastAddedClient = displayItem.client();
            m_validlyCachedClients.add(lastAddedClient);
        }
    }
}

void DisplayItemList::appendToWebDisplayItemList(WebDisplayItemList* list)
{
    for (const DisplayItem& item : m_currentDisplayItems)
        item.appendToWebDisplayItemList(list);
}

void DisplayItemList::commitNewDisplayItemsAndAppendToWebDisplayItemList(WebDisplayItemList* list)
{
    commitNewDisplayItems();
    appendToWebDisplayItemList(list);
}

#if ENABLE(ASSERT)

void DisplayItemList::checkUnderInvalidation(DisplayItems::iterator& newIt, DisplayItems::iterator& currentIt)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());
    ASSERT(newIt->isCached());

    // When under-invalidation-checking is enabled, the forced painting is following the cached display item.
    DisplayItem::Type nextItemType = DisplayItem::nonCachedType(newIt->type());
    ++newIt;
    ASSERT(newIt->type() == nextItemType);

    if (newIt->isDrawing()) {
        checkCachedDisplayItemIsUnchanged("", *newIt, *currentIt);
        return;
    }

    ASSERT(newIt->isSubsequence());

#ifndef NDEBUG
    CString messagePrefix = String::format("(In CachedSubsequence of %s)", newIt->clientDebugString().utf8().data()).utf8();
#else
    CString messagePrefix = "(In CachedSubsequence)";
#endif

    DisplayItem::Id endSubsequenceId(newIt->client(), DisplayItem::subsequenceTypeToEndSubsequenceType(newIt->type()), 0);
    while (true) {
        ASSERT(newIt != m_newDisplayItems.end());
        if (newIt->isCached())
            checkUnderInvalidation(newIt, currentIt);
        else
            checkCachedDisplayItemIsUnchanged(messagePrefix.data(), *newIt, *currentIt);

        if (endSubsequenceId.matches(*newIt))
            break;

        ++newIt;
        ++currentIt;
    }
}

static void showUnderInvalidationError(const char* messagePrefix, const char* reason, const DisplayItem* newItem, const DisplayItem* oldItem)
{
#ifndef NDEBUG
    WTFLogAlways("%s %s:\nNew display item: %s\nOld display item: %s\nSee http://crbug.com/450725.", messagePrefix, reason,
        newItem ? newItem->asDebugString().utf8().data() : "None",
        oldItem ? oldItem->asDebugString().utf8().data() : "None");
#else
    WTFLogAlways("%s %s. Run debug build to get more details\nSee http://crbug.com/450725.", messagePrefix, reason);
#endif // NDEBUG
}

void DisplayItemList::checkCachedDisplayItemIsUnchanged(const char* messagePrefix, const DisplayItem& newItem, const DisplayItem& oldItem)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());
    ASSERT(!newItem.isCached());
    ASSERT(!oldItem.isCached());

    if (newItem.skippedCache()) {
        showUnderInvalidationError(messagePrefix, "ERROR: under-invalidation: skipped-cache in cached subsequence", &newItem, &oldItem);
        ASSERT_NOT_REACHED();
    }

    if (newItem.isCacheable() && !m_validlyCachedClients.contains(newItem.client())) {
        showUnderInvalidationError(messagePrefix, "ERROR: under-invalidation: invalidated in cached subsequence", &newItem, &oldItem);
        ASSERT_NOT_REACHED();
    }

    if (newItem.equals(oldItem))
        return;

    showUnderInvalidationError(messagePrefix, "ERROR: under-invalidation: display item changed", &newItem, &oldItem);

#ifndef NDEBUG
    if (newItem.isDrawing()) {
        RefPtr<const SkPicture> newPicture = static_cast<const DrawingDisplayItem&>(newItem).picture();
        RefPtr<const SkPicture> oldPicture = static_cast<const DrawingDisplayItem&>(oldItem).picture();
        String oldPictureDebugString = oldPicture ? pictureAsDebugString(oldPicture.get()) : "None";
        String newPictureDebugString = newPicture ? pictureAsDebugString(newPicture.get()) : "None";
        WTFLogAlways("old picture:\n%s\n", oldPictureDebugString.utf8().data());
        WTFLogAlways("new picture:\n%s\n", newPictureDebugString.utf8().data());
    }
#endif // NDEBUG

    ASSERT_NOT_REACHED();
}

void DisplayItemList::checkNoRemainingCachedDisplayItems()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());

    for (const auto& displayItem : m_currentDisplayItems) {
        if (!displayItem.isValid() || !displayItem.isCacheable() || !clientCacheIsValid(displayItem.client()))
            continue;
        showUnderInvalidationError("", "May be under-invalidation: no new display item", nullptr, &displayItem);
    }
}

#endif // ENABLE(ASSERT)

#ifndef NDEBUG

WTF::String DisplayItemList::displayItemsAsDebugString(const DisplayItems& list) const
{
    StringBuilder stringBuilder;
    size_t i = 0;
    for (auto it = list.begin(); it != list.end(); ++it, ++i) {
        const DisplayItem& displayItem = *it;
        if (i)
            stringBuilder.append(",\n");
        stringBuilder.append(String::format("{index: %d, ", (int)i));
        displayItem.dumpPropertiesAsDebugString(stringBuilder);
        if (displayItem.isValid()) {
            stringBuilder.append(", cacheIsValid: ");
            stringBuilder.append(clientCacheIsValid(displayItem.client()) ? "true" : "false");
        }
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

void DisplayItemList::replay(GraphicsContext& context)
{
    TRACE_EVENT0("blink,benchmark", "DisplayItemList::replay");
    ASSERT(m_newDisplayItems.isEmpty());
    for (DisplayItem& displayItem : m_currentDisplayItems)
        displayItem.replay(context);
}

} // namespace blink
