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

    size_t cachedItem = findCachedItem(DisplayItem::Id(client, type));
    if (cachedItem == kNotFound) {
        NOTREACHED();
        return false;
    }

    ++m_numCachedNewItems;
    ensureNewDisplayItemListInitialCapacity();
    if (!DCHECK_IS_ON() || !RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
        processNewItem(m_newDisplayItemList.appendByMoving(m_currentPaintArtifact.getDisplayItemList()[cachedItem]));

    m_nextItemToMatch = cachedItem + 1;
    // Items before m_nextItemToMatch have been copied so we don't need to index them.
    if (m_nextItemToMatch > m_nextItemToIndex)
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

    size_t cachedItem = findCachedItem(DisplayItem::Id(client, DisplayItem::Subsequence));
    if (cachedItem == kNotFound) {
        NOTREACHED();
        return false;
    }

    // |cachedItem| will point to the first item after the subsequence or end of the current list.
    ensureNewDisplayItemListInitialCapacity();
    copyCachedSubsequence(cachedItem);

    m_nextItemToMatch = cachedItem;
    // Items before |cachedItem| have been copied so we don't need to index them.
    if (cachedItem > m_nextItemToIndex)
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

    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled() && isCheckingUnderInvalidation()) {
        if (m_skippedProbableUnderInvalidationCount) {
            --m_skippedProbableUnderInvalidationCount;
        } else {
            DCHECK(m_underInvalidationCheckingBegin);
            --m_underInvalidationCheckingBegin;
        }
    }
#endif
    m_newDisplayItemList.removeLast();

    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        m_newPaintChunks.decrementDisplayItemIndex();
}

const DisplayItem* PaintController::lastDisplayItem(unsigned offset)
{
    if (offset < m_newDisplayItemList.size())
        return &m_newDisplayItemList[m_newDisplayItemList.size() - offset - 1];
    return nullptr;
}

void PaintController::processNewItem(DisplayItem& displayItem)
{
    DCHECK(!m_constructionDisabled);

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    if (!isSkippingCache()) {
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

    if (isSkippingCache())
        displayItem.setSkippedCache();

#if DCHECK_IS_ON()
    // Verify noop begin/end pairs have been removed.
    if (m_newDisplayItemList.size() >= 2 && displayItem.isEnd()) {
        const auto& beginDisplayItem = m_newDisplayItemList[m_newDisplayItemList.size() - 2];
        if (beginDisplayItem.isBegin() && beginDisplayItem.getType() != DisplayItem::Subsequence && !beginDisplayItem.drawsContent())
            DCHECK(!displayItem.isEndAndPairedWith(beginDisplayItem.getType()));
    }

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
        m_newPaintChunks.incrementDisplayItemIndex(displayItem);
}

void PaintController::updateCurrentPaintChunkProperties(const PaintChunk::Id* id, const PaintChunkProperties& newProperties)
{
    m_newPaintChunks.updateCurrentPaintChunkProperties(id, newProperties);
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
    if (isSkippingCache())
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

size_t PaintController::findCachedItem(const DisplayItem::Id& id)
{
    DCHECK(clientCacheIsValid(id.client));

    // Try to find the item sequentially first. This is fast if the current list and the new list are in
    // the same order around the new item. If found, we don't need to update and lookup the index.
    for (size_t i = m_nextItemToMatch; i < m_currentPaintArtifact.getDisplayItemList().size(); ++i) {
        // We encounter an item that has already been copied which indicates we can't do sequential matching.
        const DisplayItem& item = m_currentPaintArtifact.getDisplayItemList()[i];
        if (!item.hasValidClient())
            break;
        if (id == item.getId()) {
#if DCHECK_IS_ON()
            ++m_numSequentialMatches;
#endif
            return i;
        }
        // We encounter a different cacheable item which also indicates we can't do sequential matching.
        if (item.isCacheable())
            break;
    }

    size_t foundIndex = findMatchingItemFromIndex(id, m_outOfOrderItemIndices, m_currentPaintArtifact.getDisplayItemList());
    if (foundIndex != kNotFound) {
#if DCHECK_IS_ON()
        ++m_numOutOfOrderMatches;
#endif
        return foundIndex;
    }

    return findOutOfOrderCachedItemForward(id);
}

// Find forward for the item and index all skipped indexable items.
size_t PaintController::findOutOfOrderCachedItemForward(const DisplayItem::Id& id)
{
    for (size_t i = m_nextItemToIndex; i < m_currentPaintArtifact.getDisplayItemList().size(); ++i) {
        const DisplayItem& item = m_currentPaintArtifact.getDisplayItemList()[i];
        DCHECK(item.hasValidClient());
        if (id == item.getId()) {
#if DCHECK_IS_ON()
            ++m_numSequentialMatches;
#endif
            return i;
        }
        if (item.isCacheable()) {
#if DCHECK_IS_ON()
            ++m_numIndexedItems;
#endif
            addItemToIndexIfNeeded(item, i, m_outOfOrderItemIndices);
        }
    }

#ifndef NDEBUG
    showDebugData();
    LOG(ERROR) << id.client.debugName() << ":" << DisplayItem::typeAsDebugString(id.type);
#endif
    NOTREACHED() << "Can't find cached display item";
    // We did not find the cached display item. This should be impossible, but may occur if there is a bug
    // in the system, such as under-invalidation, incorrect cache checking or duplicate display ids.
    // In this case, the caller should fall back to repaint the display item.
    return kNotFound;
}

// Copies a cached subsequence from current list to the new list. On return,
// |cachedItemIndex| points to the item after the EndSubsequence item of the subsequence.
// When paintUnderInvaldiationCheckingEnabled() we'll not actually copy the subsequence,
// but mark the begin and end of the subsequence for under-invalidation checking.
void PaintController::copyCachedSubsequence(size_t& cachedItemIndex)
{
    DisplayItem* cachedItem = &m_currentPaintArtifact.getDisplayItemList()[cachedItemIndex];
#if DCHECK_IS_ON()
    DCHECK(cachedItem->getType() == DisplayItem::Subsequence);
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
        DCHECK(!isCheckingUnderInvalidation());
        m_underInvalidationCheckingBegin = cachedItemIndex;
        m_underInvalidationMessagePrefix = "(In cached subsequence of " + cachedItem->client().debugName() + ")";
    }
#endif

    DisplayItem::Id endSubsequenceId(cachedItem->client(), DisplayItem::EndSubsequence);
    Vector<PaintChunk>::const_iterator cachedChunk;
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        cachedChunk = m_currentPaintArtifact.findChunkByDisplayItemIndex(cachedItemIndex);
        updateCurrentPaintChunkProperties(cachedChunk->id ? &*cachedChunk->id : nullptr, cachedChunk->properties);
    } else {
        // This is to avoid compilation error about uninitialized variable on Windows.
        cachedChunk = m_currentPaintArtifact.paintChunks().begin();
    }

    while (true) {
        DCHECK(cachedItem->hasValidClient());
#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
        CHECK(cachedItem->client().isAlive());
#endif
        ++m_numCachedNewItems;
        bool metEndSubsequence = cachedItem->getId() == endSubsequenceId;
        if (!DCHECK_IS_ON() || !RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
            if (RuntimeEnabledFeatures::slimmingPaintV2Enabled() && cachedItemIndex == cachedChunk->endIndex) {
                ++cachedChunk;
                updateCurrentPaintChunkProperties(cachedChunk->id ? &*cachedChunk->id : nullptr, cachedChunk->properties);
            }
            processNewItem(m_newDisplayItemList.appendByMoving(*cachedItem));
            if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
                DCHECK((!m_newPaintChunks.lastChunk().id && !cachedChunk->id) || m_newPaintChunks.lastChunk().matches(*cachedChunk));
        }

        ++cachedItemIndex;
        if (metEndSubsequence)
            break;

        // We should always be able to find the EndSubsequence display item.
        DCHECK(cachedItemIndex < m_currentPaintArtifact.getDisplayItemList().size());
        cachedItem = &m_currentPaintArtifact.getDisplayItemList()[cachedItemIndex];
    }

#if DCHECK_IS_ON()
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
        m_underInvalidationCheckingEnd = cachedItemIndex;
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

void PaintController::resetCurrentListIndices()
{
    m_nextItemToMatch = 0;
    m_nextItemToIndex = 0;
#if DCHECK_IS_ON()
    m_underInvalidationCheckingBegin = 0;
    m_underInvalidationCheckingEnd = 0;
    m_skippedProbableUnderInvalidationCount = 0;
#endif
}

void PaintController::commitNewDisplayItems(const LayoutSize& offsetFromLayoutObject)
{
    TRACE_EVENT2("blink,benchmark", "PaintController::commitNewDisplayItems",
        "current_display_list_size", (int)m_currentPaintArtifact.getDisplayItemList().size(),
        "num_non_cached_new_items", (int)m_newDisplayItemList.size() - m_numCachedNewItems);
    m_numCachedNewItems = 0;

    // These data structures are used during painting only.
    DCHECK(!isSkippingCache());
#if DCHECK_IS_ON()
    m_newDisplayItemIndicesByClient.clear();
#endif

    SkPictureGpuAnalyzer gpuAnalyzer;

    m_currentCacheGeneration = DisplayItemClient::CacheGenerationOrInvalidationReason::next();
    Vector<const DisplayItemClient*> skippedCacheClients;
    for (const auto& item : m_newDisplayItemList) {
        // No reason to continue the analysis once we have a veto.
        if (gpuAnalyzer.suitableForGpuRasterization())
            item.analyzeForGpuRasterization(gpuAnalyzer);

        m_newDisplayItemList.appendVisualRect(visualRectForDisplayItem(item, offsetFromLayoutObject));

        if (item.isCacheable()) {
            item.client().setDisplayItemsCached(m_currentCacheGeneration);
        } else {
            if (item.client().isJustCreated())
                item.client().clearIsJustCreated();
            if (item.skippedCache())
                skippedCacheClients.append(&item.client());
        }
    }

    for (auto* client : skippedCacheClients)
        client->setDisplayItemsUncached();

    // The new list will not be appended to again so we can release unused memory.
    m_newDisplayItemList.shrinkToFit();
    m_currentPaintArtifact = PaintArtifact(std::move(m_newDisplayItemList), m_newPaintChunks.releasePaintChunks(), gpuAnalyzer.suitableForGpuRasterization());
    resetCurrentListIndices();
    m_outOfOrderItemIndices.clear();

    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        for (const auto& chunk : m_currentPaintArtifact.paintChunks()) {
            if (chunk.id && chunk.id->client.isJustCreated())
                chunk.id->client.clearIsJustCreated();
        }
    }

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
}

#if DCHECK_IS_ON()

void PaintController::showUnderInvalidationError(const char* reason, const DisplayItem& newItem, const DisplayItem* oldItem) const
{
    LOG(ERROR) << m_underInvalidationMessagePrefix << " " << reason;
#ifndef NDEBUG
    LOG(ERROR) << "New display item: " << newItem.asDebugString();
    LOG(ERROR) << "Old display item: " << (oldItem ? oldItem->asDebugString() : "None");
#else
    LOG(ERROR) << "Run debug build to get more details.";
#endif
    LOG(ERROR) << "See http://crbug.com/619103.";

#ifndef NDEBUG
    const SkPicture* newPicture = newItem.isDrawing() ? static_cast<const DrawingDisplayItem&>(newItem).picture() : nullptr;
    const SkPicture* oldPicture = oldItem && oldItem->isDrawing() ? static_cast<const DrawingDisplayItem*>(oldItem)->picture() : nullptr;
    LOG(INFO) << "new picture:\n" << (newPicture ? pictureAsDebugString(newPicture) : "None");
    LOG(INFO) << "old picture:\n" << (oldPicture ? pictureAsDebugString(oldPicture) : "None");

    showDebugData();
#endif // NDEBUG
}

void PaintController::checkUnderInvalidation()
{
    DCHECK(RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled());

    if (!isCheckingUnderInvalidation())
        return;

    const DisplayItem& newItem = m_newDisplayItemList.last();
    size_t oldItemIndex = m_underInvalidationCheckingBegin + m_skippedProbableUnderInvalidationCount;
    const DisplayItem* oldItem = oldItemIndex < m_currentPaintArtifact.getDisplayItemList().size() ? &m_currentPaintArtifact.getDisplayItemList()[oldItemIndex] : nullptr;

    if (newItem.isCacheable() && !clientCacheIsValid(newItem.client())) {
        showUnderInvalidationError("under-invalidation of PaintLayer: invalidated in cached subsequence", newItem, oldItem);
        NOTREACHED();
    }

    bool oldAndNewEqual = oldItem && newItem.equals(*oldItem);
    if (!oldAndNewEqual) {
        if (newItem.isBegin()) {
            // Temporarily skip mismatching begin display item which may be removed when we remove a no-op pair.
            ++m_skippedProbableUnderInvalidationCount;
            return;
        }
        if (newItem.isDrawing() && m_skippedProbableUnderInvalidationCount == 1) {
            DCHECK_GE(m_newDisplayItemList.size(), 2u);
            if (m_newDisplayItemList[m_newDisplayItemList.size() - 2].getType() == DisplayItem::BeginCompositing) {
                // This might be a drawing item between a pair of begin/end compositing display items that will be folded
                // into a single drawing display item.
                ++m_skippedProbableUnderInvalidationCount;
                return;
            }
        }
    }

    if (m_skippedProbableUnderInvalidationCount || !oldAndNewEqual) {
        // If we ever skipped reporting any under-invalidations, report the earliest one.
        showUnderInvalidationError("under-invalidation: display item changed",
            m_newDisplayItemList[m_newDisplayItemList.size() - m_skippedProbableUnderInvalidationCount - 1],
            &m_currentPaintArtifact.getDisplayItemList()[m_underInvalidationCheckingBegin]);
        NOTREACHED();
    }

    ++m_underInvalidationCheckingBegin;
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
