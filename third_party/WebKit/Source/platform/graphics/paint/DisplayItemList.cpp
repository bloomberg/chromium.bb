// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItemList.h"

#include "platform/NotImplemented.h"
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
        if (beginDisplayItem.isBegin() && beginDisplayItem.type() != DisplayItem::BeginSubsequence && !beginDisplayItem.drawsContent())
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
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());

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
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    return m_clientsWithPaintOffsetInvalidations.contains(client);
}
#endif

void DisplayItemList::recordPaintOffset(DisplayItemClient client, const LayoutPoint& paintOffset)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    m_previousPaintOffsets.set(client, paintOffset);
}

bool DisplayItemList::paintOffsetIsUnchanged(DisplayItemClient client, const LayoutPoint& paintOffset) const
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    PreviousPaintOffsets::const_iterator it = m_previousPaintOffsets.find(client);
    if (it == m_previousPaintOffsets.end())
        return false;
    return paintOffset == it->value;
}

void DisplayItemList::removeUnneededPaintOffsetEntries()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());

    // This function is only needed temporarily while paint offsets are stored
    // in a map on the list itself. Because we don't always get notified when
    // a display item client is removed, we need to infer it to prevent the
    // paint offset map from growing indefinitely. This is achieved by just
    // removing any paint offset clients that are no longer in the full list.

    HashSet<DisplayItemClient> paintOffsetClientsToRemove;
    for (auto& client : m_previousPaintOffsets.keys())
        paintOffsetClientsToRemove.add(client);
    for (auto& item : m_currentDisplayItems)
        paintOffsetClientsToRemove.remove(item.client());

    for (auto& client : paintOffsetClientsToRemove)
        m_previousPaintOffsets.remove(client);
}

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
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    ASSERT(currentIt->type() == DisplayItem::BeginSubsequence);
    ASSERT(!currentIt->scope());
    DisplayItem::Id endSubsequenceId(currentIt->client(), DisplayItem::EndSubsequence, 0);
    do {
        // We should always find the EndSubsequence display item.
        ASSERT(currentIt != m_currentDisplayItems.end());
        updatedList.appendByMoving(*currentIt);
        ++currentIt;
    } while (!endSubsequenceId.matches(updatedList.last()));
}

// Update the existing display items by removing invalidated entries, updating
// repainted ones, and appending new items.
// - For CachedDisplayItem, copy the corresponding cached DrawingDisplayItem;
// - For SubsequenceCachedDisplayItem, copy the cached display items between the
//   corresponding BeginSubsequenceDisplayItem and EndSubsequenceDisplayItem (incl.);
// - Otherwise, copy the new display item.
//
// The algorithm is O(|m_currentDisplayItems| + |m_newDisplayItems|).
// Coefficients are related to the ratio of out-of-order [Subsequence]CachedDisplayItems
// and the average number of (Drawing|BeginSubsequence)DisplayItems per client.
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
        if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
            removeUnneededPaintOffsetEntries();
        return;
    }

    updateValidlyCachedClientsIfNeeded();

    // Stores indices to valid DrawingDisplayItems in m_currentDisplayItems that have not been matched
    // by CachedDisplayItems during synchronized matching. The indexed items will be matched
    // by later out-of-order CachedDisplayItems in m_newDisplayItems. This ensures that when
    // out-of-order CachedDisplayItems occur, we only traverse at most once over m_currentDisplayItems
    // looking for potential matches. Thus we can ensure that the algorithm runs in linear time.
    OutOfOrderIndexContext outOfOrderIndexContext(m_currentDisplayItems.begin());

#if ENABLE(ASSERT)
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
        // Under-invalidation checking requires a full index of m_currentDisplayItems.
        size_t i = 0;
        for (const auto& item : m_currentDisplayItems) {
            addItemToIndexIfNeeded(item, i, outOfOrderIndexContext.displayItemIndicesByClient);
            ++i;
        }
    }
#endif // ENABLE(ASSERT)

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
            ASSERT(!RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());
            ASSERT(newDisplayItem.isCached());
            ASSERT(clientCacheIsValid(newDisplayItem.client()) || (RuntimeEnabledFeatures::slimmingPaintV2Enabled() && !paintOffsetWasInvalidated(newDisplayItem.client())));
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

            if (newDisplayItem.isCachedDrawing()) {
                updatedList.appendByMoving(*currentIt);
                ++currentIt;
            } else {
                ASSERT(newDisplayItem.type() == DisplayItem::CachedSubsequence);
                copyCachedSubsequence(currentIt, updatedList);
                ASSERT(updatedList.last().type() == DisplayItem::EndSubsequence);
            }
        } else {
#if ENABLE(ASSERT)
            if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
                checkCachedDisplayItemIsUnchanged(newDisplayItem, outOfOrderIndexContext.displayItemIndicesByClient);
            } else {
                ASSERT(!newDisplayItem.isDrawing()
                    || newDisplayItem.skippedCache()
                    || !clientCacheIsValid(newDisplayItem.client())
                    || (RuntimeEnabledFeatures::slimmingPaintV2Enabled() && paintOffsetWasInvalidated(newDisplayItem.client())));
            }
#endif
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

    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        removeUnneededPaintOffsetEntries();

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
    // Invalidate the display item so that we can check if there are any remaining cached display items after merging.
    foundItem->clearClientForUnderInvalidationChecking();

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
        if (!displayItem.isValid() || !displayItem.isDrawing() || !clientCacheIsValid(displayItem.client()))
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
