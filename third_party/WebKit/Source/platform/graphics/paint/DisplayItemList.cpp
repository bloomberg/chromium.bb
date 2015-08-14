// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItemList.h"

#include "platform/NotImplemented.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

#if ENABLE(ASSERT)
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#endif

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

bool DisplayItemList::lastDisplayItemIsNoopBegin() const
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    if (m_newDisplayItems.isEmpty())
        return false;

    const auto& lastDisplayItem = m_newDisplayItems.last();
    return lastDisplayItem.isBegin() && !lastDisplayItem.drawsContent();
}

void DisplayItemList::removeLastDisplayItem()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
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

void DisplayItemList::processNewItem(DisplayItem* displayItem)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    ASSERT(!m_constructionDisabled);
    ASSERT(!skippingCache() || !displayItem->isCached());

    if (displayItem->isCached())
        ++m_numCachedItems;

#if ENABLE(ASSERT)
    // Verify noop begin/end pairs have been removed.
    if (m_newDisplayItems.size() >= 2 && displayItem->isEnd()) {
        const auto& beginDisplayItem = m_newDisplayItems[m_newDisplayItems.size() - 2];
        if (beginDisplayItem.isBegin() && !beginDisplayItem.drawsContent())
            ASSERT(!displayItem->isEndAndPairedWith(beginDisplayItem.type()));
    }
#endif

    if (!m_scopeStack.isEmpty())
        displayItem->setScope(m_scopeStack.last());

#if ENABLE(ASSERT)
    size_t index = findMatchingItemFromIndex(displayItem->nonCachedId(), m_newDisplayItemIndicesByClient, m_newDisplayItems);
    if (index != kNotFound) {
#ifndef NDEBUG
        showDebugData();
        WTFLogAlways("DisplayItem %s has duplicated id with previous %s (index=%d)\n",
            displayItem->asDebugString().utf8().data(), m_newDisplayItems[index].asDebugString().utf8().data(), static_cast<int>(index));
#endif
        ASSERT_NOT_REACHED();
    }
    addItemToIndexIfNeeded(*displayItem, m_newDisplayItems.size() - 1, m_newDisplayItemIndicesByClient);
#endif // ENABLE(ASSERT)

    ASSERT(!displayItem->skippedCache()); // Only DisplayItemList can set the flag.
    if (skippingCache())
        displayItem->setSkippedCache();
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
    if (skippingCache())
        return false;
    updateValidlyCachedClientsIfNeeded();
    return m_validlyCachedClients.contains(client);
}

size_t DisplayItemList::findMatchingItemFromIndex(const DisplayItem::Id& id, const DisplayItemIndicesByClientMap& displayItemIndicesByClient, const DisplayItems& list)
{
    DisplayItemIndicesByClientMap::const_iterator it = displayItemIndicesByClient.find(id.client);
    if (it == displayItemIndicesByClient.end())
        return kNotFound;

    const Vector<size_t>& indices = it->value;
    for (size_t index : indices) {
        // TODO(pdr): elementAt is not cheap so this should be refactored (See crbug.com/505965).
        const DisplayItem& existingItem = list[index];
        ASSERT(existingItem.ignoreFromDisplayList() || existingItem.client() == id.client);
        if (!existingItem.ignoreFromDisplayList() && id.matches(existingItem))
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

DisplayItems::iterator DisplayItemList::findOutOfOrderCachedItem(DisplayItems::iterator currentIt, const DisplayItem::Id& id, DisplayItemIndicesByClientMap& displayItemIndicesByClient)
{
    ASSERT(clientCacheIsValid(id.client));

    size_t foundIndex = findMatchingItemFromIndex(id, displayItemIndicesByClient, m_currentDisplayItems);
    if (foundIndex != kNotFound)
        return m_currentDisplayItems.begin() + foundIndex;

    return findOutOfOrderCachedItemForward(currentIt, id, displayItemIndicesByClient);
}

// Find forward for the item and index all skipped indexable items.
DisplayItems::iterator DisplayItemList::findOutOfOrderCachedItemForward(DisplayItems::iterator currentIt, const DisplayItem::Id& id, DisplayItemIndicesByClientMap& displayItemIndicesByClient)
{
    DisplayItems::iterator currentEnd = m_currentDisplayItems.end();
    for (; currentIt != currentEnd; ++currentIt) {
        const DisplayItem& item = *currentIt;
        if (!item.ignoreFromDisplayList()
            && item.isCacheable()
            && m_validlyCachedClients.contains(item.client())) {
            if (id.matches(item))
                return currentIt;

            addItemToIndexIfNeeded(item, currentIt - m_currentDisplayItems.begin(), displayItemIndicesByClient);
        }
    }
    return currentEnd;
}

void DisplayItemList::copyCachedSubtree(DisplayItems::iterator& currentIt, DisplayItems& updatedList)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    ASSERT(currentIt->isBeginSubtree());
    ASSERT(!currentIt->scope());
    DisplayItem::Id endSubtreeId(currentIt->client(), DisplayItem::beginSubtreeTypeToEndSubtreeType(currentIt->type()), 0);
    while (true) {
        updatedList.appendByMoving(*currentIt, currentIt->derivedSize());
        if (endSubtreeId.matches(updatedList.last()))
            break;
        ++currentIt;
        // We should always find the EndSubtree display item.
        ASSERT(currentIt != m_currentDisplayItems.end());
    }
}

// Update the existing display items by removing invalidated entries, updating
// repainted ones, and appending new items.
// - For CachedDisplayItem, copy the corresponding cached DrawingDisplayItem;
// - For SubtreeCachedDisplayItem, copy the cached display items between the
//   corresponding BeginSubtreeDisplayItem and EndSubtreeDisplayItem (incl.);
// - Otherwise, copy the new display item.
//
// The algorithm is O(|m_currentDisplayItems| + |m_newDisplayItems|).
// Coefficients are related to the ratio of out-of-order [Subtree]CachedDisplayItems
// and the average number of (Drawing|BeginSubtree)DisplayItems per client.
void DisplayItemList::commitNewDisplayItems()
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
    DisplayItemIndicesByClientMap displayItemIndicesByClient;

#if ENABLE(ASSERT)
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
        // Under-invalidation checking requires a full index of m_currentDisplayItems.
        size_t i = 0;
        for (const auto& item : m_currentDisplayItems) {
            addItemToIndexIfNeeded(item, i, displayItemIndicesByClient);
            ++i;
        }
    }
#endif // ENABLE(ASSERT)

    // TODO(jbroman): Consider revisiting this heuristic.
    DisplayItems updatedList(
        kMaximumDisplayItemSize,
        std::max(m_currentDisplayItems.usedCapacityInBytes(), m_newDisplayItems.usedCapacityInBytes()));
    DisplayItems::iterator currentIt = m_currentDisplayItems.begin();
    DisplayItems::iterator currentEnd = m_currentDisplayItems.end();
    for (DisplayItems::iterator newIt = m_newDisplayItems.begin(); newIt != m_newDisplayItems.end(); ++newIt) {
        const DisplayItem& newDisplayItem = *newIt;
        const DisplayItem::Id newDisplayItemId = newDisplayItem.nonCachedId();
        bool newDisplayItemHasCachedType = newDisplayItem.type() != newDisplayItemId.type;

        bool isSynchronized = currentIt != currentEnd
            && !currentIt->ignoreFromDisplayList()
            && newDisplayItemId.matches(*currentIt);

        if (newDisplayItemHasCachedType) {
            ASSERT(!RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());
            ASSERT(newDisplayItem.isCached());
            ASSERT(clientCacheIsValid(newDisplayItem.client()));
            if (!isSynchronized) {
                DisplayItems::iterator foundIt = findOutOfOrderCachedItem(currentIt, newDisplayItemId, displayItemIndicesByClient);
                ASSERT(foundIt != currentIt);

                if (foundIt == currentEnd) {
#ifndef NDEBUG
                    showDebugData();
                    WTFLogAlways("%s not found in m_currentDisplayItems\n", newDisplayItem.asDebugString().utf8().data());
#endif
                    ASSERT_NOT_REACHED();

                    // If foundIt == currentEnd, it means that we did not find the cached display item. This should be impossible, but may occur
                    // if there is a bug in the system, such as under-invalidation, incorrect cache checking or duplicate display ids. In this case,
                    // attempt to recover rather than crashing or bailing on display of the rest of the display list.
                    continue;
                }
                currentIt = foundIt;
            }

            if (newDisplayItem.isCachedDrawing()) {
                updatedList.appendByMoving(*currentIt, currentIt->derivedSize());
            } else {
                ASSERT(newDisplayItem.isCachedSubtree());
                copyCachedSubtree(currentIt, updatedList);
                ASSERT(updatedList.last().isEndSubtree());
            }
        } else {
#if ENABLE(ASSERT)
            if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
                checkCachedDisplayItemIsUnchanged(newDisplayItem, displayItemIndicesByClient);
            else
                ASSERT(!newDisplayItem.isDrawing() || newDisplayItem.skippedCache() || !clientCacheIsValid(newDisplayItem.client()));
#endif // ENABLE(ASSERT)
            updatedList.appendByMoving(*newIt, newIt->derivedSize());
        }

        if (isSynchronized)
            ++currentIt;
    }

#if ENABLE(ASSERT)
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
        checkNoRemainingCachedDisplayItems();
#endif // ENABLE(ASSERT)

    m_newDisplayItems.clear();
    m_validlyCachedClientsDirty = true;
    m_currentDisplayItems.swap(updatedList);
    m_numCachedItems = 0;
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

    DisplayItemClient lastClient = nullptr;
    for (const DisplayItem& displayItem : m_currentDisplayItems) {
        if (displayItem.client() == lastClient)
            continue;
        lastClient = displayItem.client();
        if (!displayItem.skippedCache())
            m_validlyCachedClients.add(lastClient);
    }
}

void DisplayItemList::commitNewDisplayItemsAndAppendToWebDisplayItemList(WebDisplayItemList* list)
{
    commitNewDisplayItems();
    for (const DisplayItem& item : m_currentDisplayItems)
        item.appendToWebDisplayItemList(list);
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

static bool bitmapIsAllZero(const SkBitmap& bitmap)
{
    bitmap.lockPixels();
    bool result = true;
    for (int x = 0; result && x < bitmap.width(); ++x) {
        for (int y = 0; result && y < bitmap.height(); ++y) {
            if (SkColorSetA(bitmap.getColor(x, y), 0) != SK_ColorTRANSPARENT)
                result = false;
        }
    }
    bitmap.unlockPixels();
    return result;
}

void DisplayItemList::checkCachedDisplayItemIsUnchanged(const DisplayItem& displayItem, DisplayItemIndicesByClientMap& displayItemIndicesByClient)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());

    if (!displayItem.isDrawing() || displayItem.skippedCache() || !clientCacheIsValid(displayItem.client()))
        return;

    // If checking under-invalidation, we always generate new display item even if the client is not invalidated.
    // Checks if the new picture is the same as the cached old picture. If the new picture is different but
    // the client is not invalidated, issue error about under-invalidation.
    size_t index = findMatchingItemFromIndex(displayItem.nonCachedId(), displayItemIndicesByClient, m_currentDisplayItems);
    if (index == kNotFound) {
        showUnderInvalidationError("ERROR: under-invalidation: no cached display item", displayItem);
        ASSERT_NOT_REACHED();
        return;
    }

    DisplayItems::iterator foundItem = m_currentDisplayItems.begin() + index;
    RefPtr<const SkPicture> newPicture = static_cast<const DrawingDisplayItem&>(displayItem).picture();
    RefPtr<const SkPicture> oldPicture = static_cast<const DrawingDisplayItem&>(*foundItem).picture();
    // Mark the display item as ignored so that we can check if there are any remaining cached display items after merging.
    foundItem->setIgnoredFromDisplayList();

    if (!newPicture && !oldPicture)
        return;
    if (newPicture && oldPicture) {
        switch (static_cast<const DrawingDisplayItem&>(displayItem).underInvalidationCheckingMode()) {
        case DrawingDisplayItem::CheckPicture:
            if (newPicture->approximateOpCount() == oldPicture->approximateOpCount()) {
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
            break;
        case DrawingDisplayItem::CheckBitmap:
            if (newPicture->cullRect() == oldPicture->cullRect()) {
                SkBitmap bitmap;
                SkRect rect = newPicture->cullRect();
                bitmap.allocPixels(SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
                SkCanvas canvas(bitmap);
                canvas.translate(-rect.x(), -rect.y());
                canvas.drawPicture(oldPicture.get());
                SkPaint diffPaint;
                diffPaint.setXfermodeMode(SkXfermode::kDifference_Mode);
                canvas.drawPicture(newPicture.get(), nullptr, &diffPaint);
                if (bitmapIsAllZero(bitmap)) // Contents are the same.
                    return;
            }
        default:
            ASSERT_NOT_REACHED();
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

    for (const auto& displayItem : m_currentDisplayItems) {
        if (displayItem.ignoreFromDisplayList() || !displayItem.isDrawing() || !clientCacheIsValid(displayItem.client()))
            continue;
        showUnderInvalidationError("May be under-invalidation: no new display item", displayItem);
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
        if (displayItem.ignoreFromDisplayList()) {
            stringBuilder.append("null");
            continue;
        }
        stringBuilder.append(String::format("{index: %d, ", (int)i));
        displayItem.dumpPropertiesAsDebugString(stringBuilder);
        stringBuilder.append(", cacheIsValid: ");
        stringBuilder.append(clientCacheIsValid(displayItem.client()) ? "true" : "false");
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
