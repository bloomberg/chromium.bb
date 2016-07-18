// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintController.h"

#include "platform/TraceEvent.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "third_party/skia/include/core/SkPictureAnalyzer.h"

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
    DCHECK(m_newDisplayItemList.isEmpty());
    DCHECK(m_newPaintChunks.isInInitialState());
    return m_currentPaintArtifact;
}

bool PaintController::useCachedDrawingIfPossible(const DisplayItemClient& client, DisplayItem::Type type)
{
    DCHECK(DisplayItem::isDrawingType(type));

    if (displayItemConstructionIsDisabled())
        return false;

    if (!clientCacheIsValid(client))
        return false;

#if DCHECK_IS_ON()
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled() && isCheckingUnderInvalidation()) {
        // We are checking under-invalidation of a subsequence enclosing this display item.
        // Let the client continue to actually paint the display item.
        return false;
    }
#endif

    DisplayItemList::iterator cachedItem = findCachedItem(DisplayItem::Id(client, type));
    if (cachedItem == m_currentPaintArtifact.getDisplayItemList().end()) {
        NOTREACHED();
        return false;
    }

    ++m_numCachedNewItems;
    ensureNewDisplayItemListInitialCapacity();
    if (!DCHECK_IS_ON() || !RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
        processNewItem(m_newDisplayItemList.appendByMoving(*cachedItem));

    m_nextItemToMatch = cachedItem + 1;
    // Items before m_nextItemToMatch have been copied so we don't need to index them.
    if (m_nextItemToMatch - m_nextItemToIndex > 0)
        m_nextItemToIndex = m_nextItemToMatch;

#if DCHECK_IS_ON()
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
        if (!isCheckingUnderInvalidation()) {
            m_underInvalidationCheckingBegin = cachedItem;
            m_underInvalidationCheckingEnd = cachedItem + 1;
            m_underInvalidationMessagePrefix = "";
        }
        // Return false to let the painter actually paint, and we will check if the new painting
        // is the same as the cached.
        return false;
    }
#endif

    return true;
}

bool PaintController::useCachedSubsequenceIfPossible(const DisplayItemClient& client)
{
    // TODO(crbug.com/596983): Implement subsequence caching for spv2.
    // The problem is in copyCachedSubsequence() which fails to handle PaintChunkProperties
    // of chunks containing cached display items. We need to find the previous
    // PaintChunkProperties and ensure they are valid in the current paint property tree.
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        return false;

    if (displayItemConstructionIsDisabled() || subsequenceCachingIsDisabled())
        return false;

    if (!clientCacheIsValid(client))
        return false;

#if DCHECK_IS_ON()
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled() && isCheckingUnderInvalidation()) {
        // We are checking under-invalidation of an ancestor subsequence enclosing this one.
        // The ancestor subsequence is supposed to have already "copied", so we should let the
        // client continue to actually paint the descendant subsequences without "copying".
        return false;
    }
#endif

    DisplayItemList::iterator cachedItem = findCachedItem(DisplayItem::Id(client, DisplayItem::Subsequence));
    if (cachedItem == m_currentPaintArtifact.getDisplayItemList().end()) {
        NOTREACHED();
        return false;
    }

    // |cachedItem| will point to the first item after the subsequence or end of the current list.
    ensureNewDisplayItemListInitialCapacity();
    copyCachedSubsequence(cachedItem);

    m_nextItemToMatch = cachedItem;
    // Items before |cachedItem| have been copied so we don't need to index them.
    if (cachedItem - m_nextItemToIndex > 0)
        m_nextItemToIndex = cachedItem;

#if DCHECK_IS_ON()
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
        // Return false to let the painter actually paint, and we will check if the new painting
        // is the same as the cached.
        return false;
    }
#endif

    return true;
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

#if DCHECK_IS_ON()
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
    DCHECK(!m_constructionDisabled);

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    if (!skippingCache()) {
        if (displayItem.isCacheable()) {
            // Mark the client shouldKeepAlive under this PaintController.
            // The status will end after the new display items are committed.
            displayItem.client().beginShouldKeepAlive(this);

            if (!m_currentSubsequenceClients.isEmpty()) {
                // Mark the client shouldKeepAlive under the current subsequence.
                // The status will end when the subsequence owner is invalidated or deleted.
                displayItem.client().beginShouldKeepAlive(m_currentSubsequenceClients.last());
            }
        }

        if (displayItem.getType() == DisplayItem::Subsequence) {
            m_currentSubsequenceClients.append(&displayItem.client());
        } else if (displayItem.getType() == DisplayItem::EndSubsequence) {
            CHECK(m_currentSubsequenceClients.last() == &displayItem.client());
            m_currentSubsequenceClients.removeLast();
        }
    }
#endif

#if DCHECK_IS_ON()
    // Verify noop begin/end pairs have been removed.
    if (m_newDisplayItemList.size() >= 2 && displayItem.isEnd()) {
        const auto& beginDisplayItem = m_newDisplayItemList[m_newDisplayItemList.size() - 2];
        if (beginDisplayItem.isBegin() && beginDisplayItem.getType() != DisplayItem::Subsequence && !beginDisplayItem.drawsContent())
            DCHECK(!displayItem.isEndAndPairedWith(beginDisplayItem.getType()));
    }
#endif

    if (skippingCache())
        displayItem.setSkippedCache();

#if DCHECK_IS_ON()
    size_t index = findMatchingItemFromIndex(displayItem.getId(), m_newDisplayItemIndicesByClient, m_newDisplayItemList);
    if (index != kNotFound) {
#ifndef NDEBUG
        showDebugData();
        WTFLogAlways("DisplayItem %s has duplicated id with previous %s (index=%d)\n",
            displayItem.asDebugString().utf8().data(), m_newDisplayItemList[index].asDebugString().utf8().data(), static_cast<int>(index));
#endif
        NOTREACHED();
    }
    addItemToIndexIfNeeded(displayItem, m_newDisplayItemList.size() - 1, m_newDisplayItemIndicesByClient);

    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
        checkUnderInvalidation();
#endif // DCHECK_IS_ON()

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

void PaintController::invalidateAll()
{
    // Can only be called during layout/paintInvalidation, not during painting.
    DCHECK(m_newDisplayItemList.isEmpty());
    m_currentPaintArtifact.reset();
    m_currentCacheGeneration.invalidate();
}

bool PaintController::clientCacheIsValid(const DisplayItemClient& client) const
{
#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    CHECK(client.isAlive());
#endif
    if (skippingCache())
        return false;
    return client.displayItemsAreCached(m_currentCacheGeneration);
}

size_t PaintController::findMatchingItemFromIndex(const DisplayItem::Id& id, const DisplayItemIndicesByClientMap& displayItemIndicesByClient, const DisplayItemList& list)
{
    DisplayItemIndicesByClientMap::const_iterator it = displayItemIndicesByClient.find(&id.client);
    if (it == displayItemIndicesByClient.end())
        return kNotFound;

    const Vector<size_t>& indices = it->value;
    for (size_t index : indices) {
        const DisplayItem& existingItem = list[index];
        DCHECK(!existingItem.hasValidClient() || existingItem.client() == id.client);
        if (id == existingItem.getId())
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

DisplayItemList::iterator PaintController::findCachedItem(const DisplayItem::Id& id)
{
    DCHECK(clientCacheIsValid(id.client));

    // Try to find the item sequentially first. This is fast if the current list and the new list are in
    // the same order around the new item. If found, we don't need to update and lookup the index.
    DisplayItemList::iterator end = m_currentPaintArtifact.getDisplayItemList().end();
    DisplayItemList::iterator it = m_nextItemToMatch;
    for (; it != end; ++it) {
        // We encounter an item that has already been copied which indicates we can't do sequential matching.
        if (!it->hasValidClient())
            break;
        if (id == it->getId()) {
#if DCHECK_IS_ON()
            ++m_numSequentialMatches;
#endif
            return it;
        }
        // We encounter a different cacheable item which also indicates we can't do sequential matching.
        if (it->isCacheable())
            break;
    }

    size_t foundIndex = findMatchingItemFromIndex(id, m_outOfOrderItemIndices, m_currentPaintArtifact.getDisplayItemList());
    if (foundIndex != kNotFound) {
#if DCHECK_IS_ON()
        ++m_numOutOfOrderMatches;
#endif
        return m_currentPaintArtifact.getDisplayItemList().begin() + foundIndex;
    }

    return findOutOfOrderCachedItemForward(id);
}

// Find forward for the item and index all skipped indexable items.
DisplayItemList::iterator PaintController::findOutOfOrderCachedItemForward(const DisplayItem::Id& id)
{
    DisplayItemList::iterator end = m_currentPaintArtifact.getDisplayItemList().end();
    for (DisplayItemList::iterator it = m_nextItemToIndex; it != end; ++it) {
        const DisplayItem& item = *it;
        DCHECK(item.hasValidClient());
        if (id == item.getId()) {
#if DCHECK_IS_ON()
            ++m_numSequentialMatches;
#endif
            return it;
        }
        if (item.isCacheable()) {
#if DCHECK_IS_ON()
            ++m_numIndexedItems;
#endif
            addItemToIndexIfNeeded(item, it - m_currentPaintArtifact.getDisplayItemList().begin(), m_outOfOrderItemIndices);
        }
    }

#ifndef NDEBUG
    showDebugData();
    LOG(ERROR) << id.client.debugName() << ":" << DisplayItem::typeAsDebugString(id.type) << " not found in current display item list";
#endif
    NOTREACHED();
    // We did not find the cached display item. This should be impossible, but may occur if there is a bug
    // in the system, such as under-invalidation, incorrect cache checking or duplicate display ids.
    // In this case, the caller should fall back to repaint the display item.
    return end;
}

// Copies a cached subsequence from current list to the new list.
// On return, |it| points to the item after the EndSubsequence item of the subsequence.
// When paintUnderInvaldiationCheckingEnabled() we'll not actually copy the subsequence,
// but mark the begin and end of the subsequence for under-invalidation checking.
void PaintController::copyCachedSubsequence(DisplayItemList::iterator& it)
{
#if DCHECK_IS_ON()
    DCHECK(it->getType() == DisplayItem::Subsequence);
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
        DCHECK(!isCheckingUnderInvalidation());
        m_underInvalidationCheckingBegin = it;
#ifndef NDEBUG
        m_underInvalidationMessagePrefix = "(In CachedSubsequence of " + it->clientDebugString() + ")";
#else
        m_underInvalidationMessagePrefix = "(In CachedSubsequence)";
#endif
    }
#endif

    DisplayItem::Id endSubsequenceId(it->client(), DisplayItem::EndSubsequence);
    bool metEndSubsequence = false;
    do {
        // We should always find the EndSubsequence display item.
        DCHECK(it != m_currentPaintArtifact.getDisplayItemList().end());
        DCHECK(it->hasValidClient());
#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
        CHECK(it->client().isAlive());
#endif
        ++m_numCachedNewItems;
        if (it->getId() == endSubsequenceId)
            metEndSubsequence = true;
        if (!DCHECK_IS_ON() || !RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
            processNewItem(m_newDisplayItemList.appendByMoving(*it));
        ++it;
    } while (!metEndSubsequence);

#if DCHECK_IS_ON()
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
        m_underInvalidationCheckingEnd = it;
        DCHECK(isCheckingUnderInvalidation());
    }
#endif
}

static IntRect visualRectForDisplayItem(const DisplayItem& displayItem, const LayoutSize& offsetFromLayoutObject)
{
    LayoutRect visualRect = displayItem.client().visualRect();
    visualRect.move(-offsetFromLayoutObject);
    return enclosingIntRect(visualRect);
}

void PaintController::resetCurrentListIterators()
{
    m_nextItemToMatch = m_currentPaintArtifact.getDisplayItemList().begin();
    m_nextItemToIndex = m_nextItemToMatch;
#if DCHECK_IS_ON()
    m_underInvalidationCheckingBegin = m_currentPaintArtifact.getDisplayItemList().end();
    m_underInvalidationCheckingEnd = m_currentPaintArtifact.getDisplayItemList().begin();
#endif
}

void PaintController::commitNewDisplayItems(const LayoutSize& offsetFromLayoutObject)
{
    TRACE_EVENT2("blink,benchmark", "PaintController::commitNewDisplayItems",
        "current_display_list_size", (int)m_currentPaintArtifact.getDisplayItemList().size(),
        "num_non_cached_new_items", (int)m_newDisplayItemList.size() - m_numCachedNewItems);
    m_numCachedNewItems = 0;

    // These data structures are used during painting only.
    DCHECK(!skippingCache());
#if DCHECK_IS_ON()
    m_newDisplayItemIndicesByClient.clear();
#endif

    SkPictureGpuAnalyzer gpuAnalyzer;

    m_currentCacheGeneration = DisplayItemClient::CacheGenerationOrInvalidationReason::next();
    for (const auto& item : m_newDisplayItemList) {
        // No reason to continue the analysis once we have a veto.
        if (gpuAnalyzer.suitableForGpuRasterization())
            item.analyzeForGpuRasterization(gpuAnalyzer);

        m_newDisplayItemList.appendVisualRect(visualRectForDisplayItem(item, offsetFromLayoutObject));

        if (item.isCacheable())
            item.client().setDisplayItemsCached(m_currentCacheGeneration);
    }

    // The new list will not be appended to again so we can release unused memory.
    m_newDisplayItemList.shrinkToFit();
    m_currentPaintArtifact = PaintArtifact(std::move(m_newDisplayItemList), m_newPaintChunks.releasePaintChunks(), gpuAnalyzer.suitableForGpuRasterization());
    resetCurrentListIterators();
    m_outOfOrderItemIndices.clear();

    // We'll allocate the initial buffer when we start the next paint.
    m_newDisplayItemList = DisplayItemList(0);

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    CHECK(m_currentSubsequenceClients.isEmpty());
    DisplayItemClient::endShouldKeepAliveAllClients(this);
#endif

#if DCHECK_IS_ON()
    m_numSequentialMatches = 0;
    m_numOutOfOrderMatches = 0;
    m_numIndexedItems = 0;
#endif
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
    DCHECK(m_newDisplayItemList.isEmpty());
    memoryUsage += m_newDisplayItemList.memoryUsageInBytes();

    return memoryUsage;
}

void PaintController::appendDebugDrawingAfterCommit(const DisplayItemClient& displayItemClient, PassRefPtr<SkPicture> picture, const LayoutSize& offsetFromLayoutObject)
{
    DCHECK(m_newDisplayItemList.isEmpty());
    DrawingDisplayItem& displayItem = m_currentPaintArtifact.getDisplayItemList().allocateAndConstruct<DrawingDisplayItem>(displayItemClient, DisplayItem::DebugDrawing, picture);
    displayItem.setSkippedCache();
    m_currentPaintArtifact.getDisplayItemList().appendVisualRect(visualRectForDisplayItem(displayItem, offsetFromLayoutObject));

    // Need to reset the iterators after mutation of the DisplayItemList.
    resetCurrentListIterators();
}

#if DCHECK_IS_ON()

void PaintController::showUnderInvalidationError(const char* reason, const DisplayItem& newItem, const DisplayItem& oldItem) const
{
    LOG(ERROR) << m_underInvalidationMessagePrefix << " " << reason;
#ifndef NDEBUG
    LOG(ERROR) << "New display item: " << newItem.asDebugString();
    LOG(ERROR) << "Old display item: " << oldItem.asDebugString();
#else
    LOG(ERROR) << "Run debug build to get more details.";
#endif
    LOG(ERROR) << "See http://crbug.com/619103.";

#ifndef NDEBUG
    if (newItem.isDrawing()) {
        RefPtr<const SkPicture> newPicture = static_cast<const DrawingDisplayItem&>(newItem).picture();
        RefPtr<const SkPicture> oldPicture = static_cast<const DrawingDisplayItem&>(oldItem).picture();
        LOG(INFO) << "new picture:\n" << (newPicture ? pictureAsDebugString(newPicture.get()) : "None");
        LOG(INFO) << "old picture:\n" << (oldPicture ? pictureAsDebugString(oldPicture.get()) : "None");
    }
#endif // NDEBUG
}

void PaintController::checkUnderInvalidation()
{
    DCHECK(RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());

    if (!isCheckingUnderInvalidation())
        return;

    const DisplayItem& newItem = m_newDisplayItemList.last();
    const DisplayItem& oldItem = *m_underInvalidationCheckingBegin++;

    if (newItem.isCacheable() && !clientCacheIsValid(newItem.client())) {
        showUnderInvalidationError("under-invalidation of PaintLayer: invalidated in cached subsequence", newItem, oldItem);
        NOTREACHED();
    }
    if (!newItem.equals(oldItem)) {
        showUnderInvalidationError("under-invalidation: display item changed", newItem, oldItem);
        NOTREACHED();
    }
}

#endif // DCHECK_IS_ON()

#ifndef NDEBUG

String PaintController::displayItemListAsDebugString(const DisplayItemList& list) const
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
        if (list.hasVisualRect(i)) {
            IntRect visualRect = list.visualRect(i);
            stringBuilder.append(String::format(", visualRect: [%d,%d %dx%d]",
                visualRect.x(), visualRect.y(),
                visualRect.width(), visualRect.height()));
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
