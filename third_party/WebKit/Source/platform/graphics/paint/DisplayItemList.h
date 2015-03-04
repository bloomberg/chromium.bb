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
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<DisplayItemList> create() { return adoptPtr(new DisplayItemList); }

    // These methods are called during paint invalidation.
    void invalidate(DisplayItemClient);
    void invalidateAll();

    // These methods are called during painting.
    void add(WTF::PassOwnPtr<DisplayItem>);
    void beginScope(DisplayItemClient);
    void endScope(DisplayItemClient);

    // Nust be called when a painting is finished.
    void endNewPaints() { updatePaintList(); }

    // Get the paint list generated after the last painting.
    const PaintList& paintList() const;

    bool clientCacheIsValid(DisplayItemClient) const;

    // Plays back the current PaintList() into the given context.
    void replay(GraphicsContext*);

#ifndef NDEBUG
    void showDebugData() const;
#endif

protected:
    DisplayItemList() { };

private:
    friend class RenderDrawingRecorderTest;
    friend class ViewDisplayListTest;

    void updatePaintList();

    void checkCachedDisplayItemIsUnchangedFromPreviousPaintList(const DisplayItem&);
    void checkNoRemainingCachedDisplayItems();

#ifndef NDEBUG
    WTF::String paintListAsDebugString(const PaintList&) const;
#endif

    // Indices into PaintList of all DrawingDisplayItems and BeginSubtreeDisplayItems of each client.
    typedef HashMap<DisplayItemClient, Vector<size_t>> DisplayItemIndicesByClientMap;

    static size_t findMatchingItem(const DisplayItem&, DisplayItem::Type, const DisplayItemIndicesByClientMap&, const PaintList&);
    static void appendDisplayItem(PaintList&, DisplayItemIndicesByClientMap&, WTF::PassOwnPtr<DisplayItem>);
    void copyCachedItems(const DisplayItem&, PaintList&, DisplayItemIndicesByClientMap&);

    PaintList m_paintList;
    DisplayItemIndicesByClientMap m_cachedDisplayItemIndicesByClient;
    PaintList m_newPaints;

    // Scope ids are allocated per client to ensure that the ids are stable for non-invalidated
    // clients between frames, so that we can use the id to match new display items to cached
    // display items.
    struct Scope {
        Scope(DisplayItemClient c, int i, bool v) : client(c), id(i), cacheIsValid(v) { }
        DisplayItemClient client;
        int id;
        bool cacheIsValid;
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
