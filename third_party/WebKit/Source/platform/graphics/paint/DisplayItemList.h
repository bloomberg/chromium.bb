// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemList_h
#define DisplayItemList_h

#include "platform/PlatformExport.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/HashMap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class GraphicsContext;

typedef Vector<OwnPtr<DisplayItem>> PaintList;

class PLATFORM_EXPORT DisplayItemList {
    WTF_MAKE_NONCOPYABLE(DisplayItemList);
    WTF_MAKE_FAST_ALLOCATED(DisplayItemList);
public:
    static PassOwnPtr<DisplayItemList> create() { return adoptPtr(new DisplayItemList); }

    // These methods are called during paint invalidation.
    void invalidate(DisplayItemClient);
    void invalidateAll();

    // These methods are called during painting.
    void add(WTF::PassOwnPtr<DisplayItem>);
    void beginScope(DisplayItemClient);
    void endScope(DisplayItemClient);

    // Must be called when a painting is finished.
    void endNewPaints() { updatePaintList(); }

    // Get the paint list generated after the last painting.
    const PaintList& paintList() const;

    bool clientCacheIsValid(DisplayItemClient) const;

    // Plays back the current PaintList() into the given context.
    void replay(GraphicsContext&);

#if ENABLE(ASSERT)
    size_t newPaintsSize() const { return m_newPaints.size(); }
#endif

#ifndef NDEBUG
    void showDebugData() const;
#endif

protected:
    DisplayItemList() : m_validlyCachedClientsDirty(false) { }

private:
    friend class LayoutObjectDrawingRecorderTest;
    friend class ViewDisplayListTest;

    void updatePaintList();

    void updateValidlyCachedClientsIfNeeded() const;

#ifndef NDEBUG
    WTF::String paintListAsDebugString(const PaintList&) const;
#endif

    // Indices into PaintList of all DrawingDisplayItems and BeginSubtreeDisplayItems of each client.
    // Temporarily used during merge to find out-of-order display items.
    using DisplayItemIndicesByClientMap = HashMap<DisplayItemClient, Vector<size_t>>;

    static size_t findMatchingItemFromIndex(const DisplayItem&, DisplayItem::Type matchingType, const DisplayItemIndicesByClientMap&, const PaintList&);
    static void addItemToIndex(const DisplayItem&, size_t index, DisplayItemIndicesByClientMap&);
    size_t findOutOfOrderCachedItem(size_t& currentPaintListIndex, const DisplayItem&, DisplayItem::Type, DisplayItemIndicesByClientMap&);
    size_t findOutOfOrderCachedItemForward(size_t& currentPaintListIndex, const DisplayItem&, DisplayItem::Type, DisplayItemIndicesByClientMap&);

    // The following two methods are for checking under-invalidations
    // (when RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled).
    void checkCachedDisplayItemIsUnchangedFromPreviousPaintList(const DisplayItem&, DisplayItemIndicesByClientMap&);
    void checkNoRemainingCachedDisplayItems();

    PaintList m_paintList;
    PaintList m_newPaints;

    // Contains all clients having valid cached paintings if updated.
    // It's lazily updated in updateValidlyCachedClientsIfNeeded().
    // FIXME: In the future we can replace this with client-side repaint flags
    // to avoid the cost of building and querying the hash table.
    mutable HashSet<DisplayItemClient> m_validlyCachedClients;
    mutable bool m_validlyCachedClientsDirty;

    // Scope ids are allocated per client to ensure that the ids are stable for non-invalidated
    // clients between frames, so that we can use the id to match new display items to cached
    // display items.
    struct Scope {
        Scope(DisplayItemClient c, int i) : client(c), id(i) { }
        DisplayItemClient client;
        int id;
    };
    typedef HashMap<DisplayItemClient, int> ClientScopeIdMap;
    ClientScopeIdMap m_clientScopeIdMap;
    Vector<Scope> m_scopeStack;

#if ENABLE(ASSERT)
    // This is used to check duplicated ids during add(). We could also check during
    // updatePaintList(), but checking during add() helps developer easily find where
    // the duplicated ids are from.
    DisplayItemIndicesByClientMap m_newDisplayItemIndicesByClient;
#endif
};

} // namespace blink

#endif // DisplayItemList_h
