// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintController.h"

#include "platform/TraceEvent.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

#ifndef NDEBUG
#include "platform/graphics/LoggingCanvas.h"
#include "wtf/text/StringBuilder.h"
#include <stdio.h>
#endif

namespace blink {

static PaintChunker::ItemBehavior behaviorOfItemType(DisplayItem::Type type)
{
    if (DisplayItem::isForeignLayerType(type))
        return PaintChunker::RequiresSeparateChunk;
    return PaintChunker::DefaultBehavior;
}

const PaintArtifact& PaintController::paintArtifact() const
{
    ASSERT(m_newDisplayItemList.isEmpty());
    ASSERT(m_newPaintChunks.isInInitialState());
    return m_currentPaintArtifact;
}

bool PaintController::lastDisplayItemIsNoopBegin() const
{
    if (m_newDisplayItemList.isEmpty())
        return false;

    const auto& lastDisplayItem = m_newDisplayItemList.last();
    return lastDisplayItem.isBegin() && !lastDisplayItem.drawsContent();
}

void PaintController::removeLastDisplayItem()
{
    if (m_newDisplayItemList.isEmpty())
        return;

#if ENABLE(ASSERT)
    // Also remove the index pointing to the removed display item.
    DisplayItemIndicesByClientMap::iterator it = m_newDisplayItemIndicesByClient.find(&m_newDisplayItemList.last().client());
    if (it != m_newDisplayItemIndicesByClient.end()) {
        Vector<size_t>& indices = it->value;
        if (!indices.isEmpty() && indices.last() == (m_newDisplayItemList.size() - 1))
            indices.removeLast();
    }
#endif
    m_newDisplayItemList.removeLast();

    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        m_newPaintChunks.decrementDisplayItemIndex();
}

void PaintController::processNewItem(DisplayItem& displayItem)
{
    ASSERT(!m_constructionDisabled);
    ASSERT(!skippingCache() || !displayItem.isCached());

    if (displayItem.isCached())
        ++m_numCachedNewItems;

#if ENABLE(ASSERT)
    // Verify noop begin/end pairs have been removed.
    if (m_newDisplayItemList.size() >= 2 && displayItem.isEnd()) {
        const auto& beginDisplayItem = m_newDisplayItemList[m_newDisplayItemList.size() - 2];
        if (beginDisplayItem.isBegin() && beginDisplayItem.getType() != DisplayItem::Subsequence && !beginDisplayItem.drawsContent())
            ASSERT(!displayItem.isEndAndPairedWith(beginDisplayItem.getType()));
    }
#endif

    if (!m_scopeStack.isEmpty())
        displayItem.setScope(m_scopeStack.last());

#if ENABLE(ASSERT)
    size_t index = findMatchingItemFromIndex(displayItem.nonCachedId(), m_newDisplayItemIndicesByClient, m_newDisplayItemList);
    if (index != kNotFound) {
#ifndef NDEBUG
        showDebugData();
        WTFLogAlways("DisplayItem %s has duplicated id with previous %s (index=%d)\n",
            displayItem.asDebugString().utf8().data(), m_newDisplayItemList[index].asDebugString().utf8().data(), static_cast<int>(index));
#endif
        ASSERT_NOT_REACHED();
    }
    addItemToIndexIfNeeded(displayItem, m_newDisplayItemList.size() - 1, m_newDisplayItemIndicesByClient);
#endif // ENABLE(ASSERT)

    if (skippingCache())
        displayItem.setSkippedCache();

    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        m_newPaintChunks.incrementDisplayItemIndex(behaviorOfItemType(displayItem.getType()));
}

void PaintController::updateCurrentPaintChunkProperties(const PaintChunkProperties& newProperties)
{
    m_newPaintChunks.updateCurrentPaintChunkProperties(newProperties);
}

const PaintChunkProperties& PaintController::currentPaintChunkProperties() const
{
    return m_newPaintChunks.currentPaintChunkProperties();
}

void PaintController::beginScope()
{
    ASSERT_WITH_SECURITY_IMPLICATION(m_nextScope < UINT_MAX);
    m_scopeStack.append(m_nextScope++);
    beginSkippingCache();
}

void PaintController::endScope()
{
    m_scopeStack.removeLast();
    endSkippingCache();
}

void PaintController::invalidate(const DisplayItemClient& client)
{
#if ENABLE(ASSERT)
    // Slimming paint v1 CompositedLayerMapping may invalidate client on extra layers.
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled() || clientCacheIsValid(client))
        m_invalidations.append(client.debugName());
#endif

    invalidateUntracked(client);
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled() && m_trackedPaintInvalidationObjects)
        m_trackedPaintInvalidationObjects->append(client.debugName());
}

void PaintController::invalidateUntracked(const DisplayItemClient& client)
{
    // This can be called during painting, but we can't invalidate already painted clients.
    client.setDisplayItemsUncached();
    ASSERT(!m_newDisplayItemIndicesByClient.contains(&client));
}

void PaintController::invalidateAll()
{
    // Can only be called during layout/paintInvalidation, not during painting.
    ASSERT(m_newDisplayItemList.isEmpty());
    m_currentPaintArtifact.reset();
    m_currentCacheGeneration.invalidate();

    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled() && m_trackedPaintInvalidationObjects)
        m_trackedPaintInvalidationObjects->append("##ALL##");
}

bool PaintController::clientCacheIsValid(const DisplayItemClient& client) const
{
    ASSERT(DisplayItemClient::isAlive(client));
    if (skippingCache())
        return false;
    return client.displayItemsAreCached(m_currentCacheGeneration);
}

void PaintController::invalidatePaintOffset(const DisplayItemClient& client)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());
    invalidate(client);

#if ENABLE(ASSERT)
    ASSERT(!paintOffsetWasInvalidated(client));
    m_clientsWithPaintOffsetInvalidations.add(&client);
#endif
}

#if ENABLE(ASSERT)
bool PaintController::paintOffsetWasInvalidated(const DisplayItemClient& client) const
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());
    return m_clientsWithPaintOffsetInvalidations.contains(&client);
}
#endif

size_t PaintController::findMatchingItemFromIndex(const DisplayItem::Id& id, const DisplayItemIndicesByClientMap& displayItemIndicesByClient, const DisplayItemList& list)
{
    DisplayItemIndicesByClientMap::const_iterator it = displayItemIndicesByClient.find(&id.client);
    if (it == displayItemIndicesByClient.end())
        return kNotFound;

    const Vector<size_t>& indices = it->value;
    for (size_t index : indices) {
        const DisplayItem& existingItem = list[index];
        ASSERT(!existingItem.hasValidClient() || existingItem.client() == id.client);
        if (id.matches(existingItem))
            return index;
    }

    return kNotFound;
}

void PaintController::addItemToIndexIfNeeded(const DisplayItem& displayItem, size_t index, DisplayItemIndicesByClientMap& displayItemIndicesByClient)
{
    if (!displayItem.isCacheable())
        return;

    DisplayItemIndicesByClientMap::iterator it = displayItemIndicesByClient.find(&displayItem.client());
    Vector<size_t>& indices = it == displayItemIndicesByClient.end() ?
        displayItemIndicesByClient.add(&displayItem.client(), Vector<size_t>()).storedValue->value : it->value;
    indices.append(index);
}

struct PaintController::OutOfOrderIndexContext {
    STACK_ALLOCATED();
    OutOfOrderIndexContext(DisplayItemList::iterator begin) : nextItemToIndex(begin) { }

    DisplayItemList::iterator nextItemToIndex;
    DisplayItemIndicesByClientMap displayItemIndicesByClient;
};

DisplayItemList::iterator PaintController::findOutOfOrderCachedItem(const DisplayItem::Id& id, OutOfOrderIndexContext& context)
{
    ASSERT(clientCacheIsValid(id.client));

    size_t foundIndex = findMatchingItemFromIndex(id, context.displayItemIndicesByClient, m_currentPaintArtifact.getDisplayItemList());
    if (foundIndex != kNotFound)
        return m_currentPaintArtifact.getDisplayItemList().begin() + foundIndex;

    return findOutOfOrderCachedItemForward(id, context);
}

// Find forward for the item and index all skipped indexable items.
DisplayItemList::iterator PaintController::findOutOfOrderCachedItemForward(const DisplayItem::Id& id, OutOfOrderIndexContext& context)
{
    DisplayItemList::iterator currentEnd = m_currentPaintArtifact.getDisplayItemList().end();
    for (; context.nextItemToIndex != currentEnd; ++context.nextItemToIndex) {
        const DisplayItem& item = *context.nextItemToIndex;
        ASSERT(item.hasValidClient());
        if (id.matches(item))
            return context.nextItemToIndex++;
        if (item.isCacheable())
            addItemToIndexIfNeeded(item, context.nextItemToIndex - m_currentPaintArtifact.getDisplayItemList().begin(), context.displayItemIndicesByClient);
    }
    return currentEnd;
}

void PaintController::copyCachedSubsequence(const DisplayItemList& currentList, DisplayItemList::iterator& currentIt, DisplayItemList& updatedList)
{
    ASSERT(currentIt->getType() == DisplayItem::Subsequence);
    ASSERT(!currentIt->scope());
    DisplayItem::Id endSubsequenceId(currentIt->client(), DisplayItem::EndSubsequence, 0);
    do {
        // We should always find the EndSubsequence display item.
        ASSERT(currentIt != m_currentPaintArtifact.getDisplayItemList().end());
        ASSERT(currentIt->hasValidClient());
        updatedList.appendByMoving(*currentIt, currentList.visualRect(currentIt - m_currentPaintArtifact.getDisplayItemList().begin()));
        ++currentIt;
    } while (!endSubsequenceId.matches(updatedList.last()));
}

void PaintController::commitNewDisplayItems(const LayoutSize& offsetFromLayoutObject)
{
#if ENABLE(ASSERT)
    m_newDisplayItemList.assertDisplayItemClientsAreAlive();
#endif
    commitNewDisplayItemsInternal(offsetFromLayoutObject);
#if ENABLE(ASSERT)
    m_currentPaintArtifact.getDisplayItemList().assertDisplayItemClientsAreAlive();
#endif
}

static IntRect visualRectForDisplayItem(const DisplayItem& displayItem, const LayoutSize& offsetFromLayoutObject)
{
    LayoutRect visualRect = displayItem.client().visualRect();
    visualRect.move(-offsetFromLayoutObject);
    return enclosingIntRect(visualRect);
}

// Update the existing display items by removing invalidated entries, updating
// repainted ones, and appending new items.
// - For cached drawing display item, copy the corresponding cached DrawingDisplayItem;
// - For cached subsequence display item, copy the cached display items between the
//   corresponding SubsequenceDisplayItem and EndSubsequenceDisplayItem (incl.);
// - Otherwise, copy the new display item.
//
// The algorithm is O(|m_currentDisplayItemList| + |m_newDisplayItemList|).
// Coefficients are related to the ratio of out-of-order CachedDisplayItems
// and the average number of (Drawing|Subsequence)DisplayItems per client.
//
void PaintController::commitNewDisplayItemsInternal(const LayoutSize& offsetFromLayoutObject)
{
    TRACE_EVENT2("blink,benchmark", "PaintController::commitNewDisplayItems",
        "current_display_list_size", (int)m_currentPaintArtifact.getDisplayItemList().size(),
        "num_non_cached_new_items", (int)m_newDisplayItemList.size() - m_numCachedNewItems);
    m_numCachedNewItems = 0;

    // These data structures are used during painting only.
    ASSERT(m_scopeStack.isEmpty());
    m_scopeStack.clear();
    m_nextScope = 1;
    ASSERT(!skippingCache());
#if ENABLE(ASSERT)
    m_newDisplayItemIndicesByClient.clear();
    m_clientsWithPaintOffsetInvalidations.clear();
    m_invalidations.clear();
#endif

    if (m_currentPaintArtifact.isEmpty()) {
#if ENABLE(ASSERT)
        for (const auto& item : m_newDisplayItemList)
            ASSERT(!item.isCached());
#endif
        for (const auto& item : m_newDisplayItemList)
            m_newDisplayItemList.appendVisualRect(visualRectForDisplayItem(item, offsetFromLayoutObject));

        m_currentPaintArtifact = PaintArtifact(std::move(m_newDisplayItemList), m_newPaintChunks.releasePaintChunks());
        m_newDisplayItemList = DisplayItemList(kInitialDisplayItemListCapacityBytes);
        updateCacheGeneration();
        return;
    }

    // Stores indices to valid DrawingDisplayItems in m_currentDisplayItems that have not been matched
    // by CachedDisplayItems during synchronized matching. The indexed items will be matched
    // by later out-of-order CachedDisplayItems in m_newDisplayItemList. This ensures that when
    // out-of-order CachedDisplayItems occur, we only traverse at most once over m_currentDisplayItems
    // looking for potential matches. Thus we can ensure that the algorithm runs in linear time.
    OutOfOrderIndexContext outOfOrderIndexContext(m_currentPaintArtifact.getDisplayItemList().begin());

    // TODO(jbroman): Consider revisiting this heuristic.
    DisplayItemList updatedList(std::max(m_currentPaintArtifact.getDisplayItemList().usedCapacityInBytes(), m_newDisplayItemList.usedCapacityInBytes()));
    Vector<PaintChunk> updatedPaintChunks;
    DisplayItemList::iterator currentIt = m_currentPaintArtifact.getDisplayItemList().begin();
    DisplayItemList::iterator currentEnd = m_currentPaintArtifact.getDisplayItemList().end();
    for (DisplayItemList::iterator newIt = m_newDisplayItemList.begin(); newIt != m_newDisplayItemList.end(); ++newIt) {
        const DisplayItem& newDisplayItem = *newIt;
        const DisplayItem::Id newDisplayItemId = newDisplayItem.nonCachedId();
        bool newDisplayItemHasCachedType = newDisplayItem.getType() != newDisplayItemId.type;

        bool isSynchronized = currentIt != currentEnd && newDisplayItemId.matches(*currentIt);

        if (newDisplayItemHasCachedType) {
            ASSERT(newDisplayItem.isCached());
            ASSERT(clientCacheIsValid(newDisplayItem.client()) || (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() && !paintOffsetWasInvalidated(newDisplayItem.client())));
            if (!isSynchronized) {
                currentIt = findOutOfOrderCachedItem(newDisplayItemId, outOfOrderIndexContext);

                if (currentIt == currentEnd) {
#ifndef NDEBUG
                    showDebugData();
                    WTFLogAlways("%s not found in m_currentDisplayItemList\n", newDisplayItem.asDebugString().utf8().data());
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
                DisplayItemList::iterator temp = currentIt;
                checkUnderInvalidation(newIt, temp);
            }
#endif
            if (newDisplayItem.isCachedDrawing()) {
                updatedList.appendByMoving(*currentIt, m_currentPaintArtifact.getDisplayItemList().visualRect(currentIt - m_currentPaintArtifact.getDisplayItemList().begin()));
                ++currentIt;
            } else {
                ASSERT(newDisplayItem.getType() == DisplayItem::CachedSubsequence);
                copyCachedSubsequence(m_currentPaintArtifact.getDisplayItemList(), currentIt, updatedList);
                ASSERT(updatedList.last().getType() == DisplayItem::EndSubsequence);
            }
        } else {
            ASSERT(!newDisplayItem.isDrawing()
                || newDisplayItem.skippedCache()
                || !clientCacheIsValid(newDisplayItem.client())
                || (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() && paintOffsetWasInvalidated(newDisplayItem.client())));

            updatedList.appendByMoving(*newIt, visualRectForDisplayItem(*newIt, offsetFromLayoutObject));

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

    // TODO(jbroman): When subsequence caching applies to SPv2, we'll need to
    // merge the paint chunks as well.
    m_currentPaintArtifact = PaintArtifact(std::move(updatedList), m_newPaintChunks.releasePaintChunks());

    m_newDisplayItemList = DisplayItemList(kInitialDisplayItemListCapacityBytes);
    updateCacheGeneration();
}

size_t PaintController::approximateUnsharedMemoryUsage() const
{
    size_t memoryUsage = sizeof(*this);

    // Memory outside this class due to m_currentPaintArtifact.
    memoryUsage += m_currentPaintArtifact.approximateUnsharedMemoryUsage() - sizeof(m_currentPaintArtifact);

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

    // Memory outside this class due to m_newDisplayItemList.
    ASSERT(m_newDisplayItemList.isEmpty());
    memoryUsage += m_newDisplayItemList.memoryUsageInBytes();

    return memoryUsage;
}

void PaintController::updateCacheGeneration()
{
    m_currentCacheGeneration = DisplayItemCacheGeneration::next();
    for (const DisplayItem& displayItem : m_currentPaintArtifact.getDisplayItemList()) {
        if (displayItem.isCacheable())
            displayItem.client().setDisplayItemsCached(m_currentCacheGeneration);
    }
}

#if ENABLE(ASSERT)

void PaintController::checkUnderInvalidation(DisplayItemList::iterator& newIt, DisplayItemList::iterator& currentIt)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());
    ASSERT(newIt->isCached());

    // When under-invalidation-checking is enabled, the forced painting is following the cached display item.
    DisplayItem::Type nextItemType = DisplayItem::nonCachedType(newIt->getType());
    ++newIt;
    ASSERT(newIt->getType() == nextItemType);

    if (newIt->isDrawing()) {
        checkCachedDisplayItemIsUnchanged("", *newIt, *currentIt);
        return;
    }

    ASSERT(newIt->getType() == DisplayItem::Subsequence);

#ifndef NDEBUG
    CString messagePrefix = String::format("(In CachedSubsequence of %s)", newIt->clientDebugString().utf8().data()).utf8();
#else
    CString messagePrefix = "(In CachedSubsequence)";
#endif

    DisplayItem::Id endSubsequenceId(newIt->client(), DisplayItem::EndSubsequence, 0);
    while (true) {
        ASSERT(newIt != m_newDisplayItemList.end());
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

void PaintController::checkCachedDisplayItemIsUnchanged(const char* messagePrefix, const DisplayItem& newItem, const DisplayItem& oldItem)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());
    ASSERT(!newItem.isCached());
    ASSERT(!oldItem.isCached());

    if (newItem.skippedCache()) {
        showUnderInvalidationError(messagePrefix, "ERROR: under-invalidation: skipped-cache in cached subsequence", &newItem, &oldItem);
        ASSERT_NOT_REACHED();
    }

    if (newItem.isCacheable() && !clientCacheIsValid(newItem.client())) {
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

void PaintController::checkNoRemainingCachedDisplayItems()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());

    for (const auto& displayItem : m_currentPaintArtifact.getDisplayItemList()) {
        if (!displayItem.hasValidClient() || !displayItem.isCacheable() || !clientCacheIsValid(displayItem.client()))
            continue;
        showUnderInvalidationError("", "May be under-invalidation: no new display item", nullptr, &displayItem);
    }
}

#endif // ENABLE(ASSERT)

#ifndef NDEBUG

WTF::String PaintController::displayItemListAsDebugString(const DisplayItemList& list) const
{
    StringBuilder stringBuilder;
    size_t i = 0;
    for (auto it = list.begin(); it != list.end(); ++it, ++i) {
        const DisplayItem& displayItem = *it;
        if (i)
            stringBuilder.append(",\n");
        stringBuilder.append(String::format("{index: %d, ", (int)i));
        displayItem.dumpPropertiesAsDebugString(stringBuilder);
        if (displayItem.hasValidClient()) {
            stringBuilder.append(", cacheIsValid: ");
            stringBuilder.append(clientCacheIsValid(displayItem.client()) ? "true" : "false");
        }
        stringBuilder.append('}');
    }
    return stringBuilder.toString();
}

void PaintController::showDebugData() const
{
    WTFLogAlways("current display item list: [%s]\n", displayItemListAsDebugString(m_currentPaintArtifact.getDisplayItemList()).utf8().data());
    WTFLogAlways("new display item list: [%s]\n", displayItemListAsDebugString(m_newDisplayItemList).utf8().data());
}

#endif // ifndef NDEBUG

} // namespace blink
