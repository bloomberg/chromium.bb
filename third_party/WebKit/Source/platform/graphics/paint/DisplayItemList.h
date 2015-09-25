// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemList_h
#define DisplayItemList_h

#include "platform/PlatformExport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/ContiguousContainer.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/Transform3DDisplayItem.h"
#include "wtf/Alignment.h"
#include "wtf/HashMap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Utility.h"
#include "wtf/Vector.h"

namespace blink {

class GraphicsContext;

// kDisplayItemAlignment must be a multiple of alignof(derived display item) for
// each derived display item; the ideal value is the least common multiple.
// Currently the limiting factor is TransformtionMatrix (in
// BeginTransform3DDisplayItem), which requests 16-byte alignment.
static const size_t kDisplayItemAlignment = WTF_ALIGN_OF(BeginTransform3DDisplayItem);
static const size_t kInitialDisplayItemsCapacity = 64;
static const size_t kMaximumDisplayItemSize = sizeof(BeginTransform3DDisplayItem);

// Map from SimpleLayer.startPoint to the DrawingDisplayItems within its range
// which were invalidated on this frame and do not change SimpleLayers.
using DisplayListDiff = HashMap<DisplayItemClient, DisplayItem*>;

class DisplayItems : public ContiguousContainer<DisplayItem, kDisplayItemAlignment> {
public:
    DisplayItems(size_t initialSizeBytes)
        : ContiguousContainer(kMaximumDisplayItemSize, initialSizeBytes) {}

    DisplayItem& appendByMoving(DisplayItem& item)
    {
#ifndef NDEBUG
        WTF::String originalDebugString = item.asDebugString();
#endif
        ASSERT(item.isValid());
        DisplayItem& result = ContiguousContainer::appendByMoving(item, item.derivedSize());
        // ContiguousContainer::appendByMoving() called in-place constructor on item, which invalidated it.
        ASSERT(!item.isValid());
#ifndef NDEBUG
        // Save original debug string in the old item to help debugging.
        item.setClientDebugString(originalDebugString);
#endif
        return result;
    }
};

class PLATFORM_EXPORT DisplayItemList {
    WTF_MAKE_NONCOPYABLE(DisplayItemList);
    WTF_MAKE_FAST_ALLOCATED(DisplayItemList);
public:
    static PassOwnPtr<DisplayItemList> create()
    {
        return adoptPtr(new DisplayItemList());
    }

    // These methods are called during paint invalidation.
    void invalidate(const DisplayItemClientWrapper&);
    void invalidateUntracked(DisplayItemClient);
    void invalidateAll();

    // Record when paint offsets change during paint.
    void invalidatePaintOffset(const DisplayItemClientWrapper&);
#if ENABLE(ASSERT)
    bool paintOffsetWasInvalidated(DisplayItemClient) const;
#endif

    // These methods are called during painting.
    template <typename DisplayItemClass, typename... Args>
    DisplayItemClass& createAndAppend(Args&&... args)
    {
        static_assert(WTF::IsSubclass<DisplayItemClass, DisplayItem>::value,
            "Can only createAndAppend subclasses of DisplayItem.");
        static_assert(sizeof(DisplayItemClass) <= kMaximumDisplayItemSize,
            "DisplayItem subclass is larger than kMaximumDisplayItemSize.");

        DisplayItemClass& displayItem = m_newDisplayItems.allocateAndConstruct<DisplayItemClass>(WTF::forward<Args>(args)...);
        processNewItem(displayItem);
        return displayItem;
    }

    // Scopes must be used to avoid duplicated display item ids when we paint some object
    // multiple times and generate multiple display items with the same type.
    // We don't cache display items added in scopes.
    void beginScope();
    void endScope();

    // True if the last display item is a begin that doesn't draw content.
    bool lastDisplayItemIsNoopBegin() const;
    void removeLastDisplayItem();

    void beginSkippingCache() { ++m_skippingCacheCount; }
    void endSkippingCache() { ASSERT(m_skippingCacheCount > 0); --m_skippingCacheCount; }
    bool skippingCache() const { return m_skippingCacheCount; }

    // Must be called when a painting is finished. If passed, a DisplayListDiff
    // is initialized and created.
    void commitNewDisplayItems(DisplayListDiff* = 0);

    // Returns the approximate memory usage, excluding memory likely to be
    // shared with the embedder after copying to WebDisplayItemList.
    // Should only be called right after commitNewDisplayItems.
    size_t approximateUnsharedMemoryUsage() const;

    // Get the paint list generated after the last painting.
    const DisplayItems& displayItems() const;

    bool clientCacheIsValid(DisplayItemClient) const;

    // Commits the new display items and plays back the updated display items into the given context.
    void commitNewDisplayItemsAndReplay(GraphicsContext& context)
    {
        commitNewDisplayItems();
        replay(context);
    }

    void appendToWebDisplayItemList(WebDisplayItemList*);
    void commitNewDisplayItemsAndAppendToWebDisplayItemList(WebDisplayItemList*);

    bool displayItemConstructionIsDisabled() const { return m_constructionDisabled; }
    void setDisplayItemConstructionIsDisabled(const bool disable) { m_constructionDisabled = disable; }

    // Returns displayItems added using createAndAppend() since beginning or the last
    // commitNewDisplayItems(). Use with care.
    DisplayItems& newDisplayItems() { return m_newDisplayItems; }

#ifndef NDEBUG
    void showDebugData() const;
#endif

    void startTrackingPaintInvalidationObjects()
    {
        ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
        m_trackedPaintInvalidationObjects = adoptPtr(new Vector<String>());
    }
    void stopTrackingPaintInvalidationObjects()
    {
        ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
        m_trackedPaintInvalidationObjects = nullptr;
    }
    Vector<String> trackedPaintInvalidationObjects()
    {
        ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
        return m_trackedPaintInvalidationObjects ? *m_trackedPaintInvalidationObjects : Vector<String>();
    }

protected:
    DisplayItemList()
        : m_currentDisplayItems(0)
        , m_newDisplayItems(kInitialDisplayItemsCapacity * kMaximumDisplayItemSize)
        , m_validlyCachedClientsDirty(false)
        , m_constructionDisabled(false)
        , m_skippingCacheCount(0)
        , m_numCachedItems(0)
        , m_nextScope(1) { }

private:
    // Set new item state (scopes, cache skipping, etc) for a new item.
    void processNewItem(DisplayItem&);

    void updateValidlyCachedClientsIfNeeded() const;

#ifndef NDEBUG
    WTF::String displayItemsAsDebugString(const DisplayItems&) const;
#endif

    // Indices into PaintList of all DrawingDisplayItems and BeginSubsequenceDisplayItems of each client.
    // Temporarily used during merge to find out-of-order display items.
    using DisplayItemIndicesByClientMap = HashMap<DisplayItemClient, Vector<size_t>>;

    static size_t findMatchingItemFromIndex(const DisplayItem::Id&, const DisplayItemIndicesByClientMap&, const DisplayItems&);
    static void addItemToIndexIfNeeded(const DisplayItem&, size_t index, DisplayItemIndicesByClientMap&);

    struct OutOfOrderIndexContext;
    DisplayItems::iterator findOutOfOrderCachedItem(const DisplayItem::Id&, OutOfOrderIndexContext&);
    DisplayItems::iterator findOutOfOrderCachedItemForward(const DisplayItem::Id&, OutOfOrderIndexContext&);
    void copyCachedSubsequence(DisplayItems::iterator& currentIt, DisplayItems& updatedList);

#if ENABLE(ASSERT)
    // The following two methods are for checking under-invalidations
    // (when RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled).
    void checkUnderInvalidation(DisplayItems::iterator& newIt, DisplayItems::iterator& currentIt);
    void checkCachedDisplayItemIsUnchanged(const char* messagePrefix, const DisplayItem& newItem, const DisplayItem& oldItem);
    void checkNoRemainingCachedDisplayItems();
#endif

    void replay(GraphicsContext&);

    DisplayItems m_currentDisplayItems;
    DisplayItems m_newDisplayItems;

    // Contains all clients having valid cached paintings if updated.
    // It's lazily updated in updateValidlyCachedClientsIfNeeded().
    // FIXME: In the future we can replace this with client-side repaint flags
    // to avoid the cost of building and querying the hash table.
    mutable HashSet<DisplayItemClient> m_validlyCachedClients;
    mutable bool m_validlyCachedClientsDirty;

#if ENABLE(ASSERT)
    // Set of clients which had paint offset changes since the last commit. This is used for
    // ensuring paint offsets are only updated once and are the same in all phases.
    HashSet<DisplayItemClient> m_clientsWithPaintOffsetInvalidations;
#endif

    // Allow display item construction to be disabled to isolate the costs of construction
    // in performance metrics.
    bool m_constructionDisabled;

    int m_skippingCacheCount;

    int m_numCachedItems;

    unsigned m_nextScope;
    Vector<unsigned> m_scopeStack;

#if ENABLE(ASSERT)
    // This is used to check duplicated ids during add(). We could also check during
    // updatePaintList(), but checking during add() helps developer easily find where
    // the duplicated ids are from.
    DisplayItemIndicesByClientMap m_newDisplayItemIndicesByClient;
#endif

    OwnPtr<Vector<String>> m_trackedPaintInvalidationObjects;
};

} // namespace blink

#endif // DisplayItemList_h
