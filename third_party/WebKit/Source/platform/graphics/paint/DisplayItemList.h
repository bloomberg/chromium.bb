// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemList_h
#define DisplayItemList_h

#include "platform/PlatformExport.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/HashSet.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class GraphicsContext;

typedef Vector<OwnPtr<DisplayItem>> PaintList;

class PLATFORM_EXPORT DisplayItemList {
    WTF_MAKE_NONCOPYABLE(DisplayItemList);
public:
    static PassOwnPtr<DisplayItemList> create() { return adoptPtr(new DisplayItemList); }

    const PaintList& paintList();
    void add(WTF::PassOwnPtr<DisplayItem>);

    void invalidate(DisplayItemClient);
    void invalidateAll();
    bool clientCacheIsValid(DisplayItemClient client) const { return m_cachedClients.contains(client); }

    // Plays back the current PaintList() into the given context.
    void replay(GraphicsContext*);

#ifndef NDEBUG
    void showDebugData() const;
#endif

protected:
    DisplayItemList() { };

private:
    PaintList::iterator findNextMatchingCachedItem(PaintList::iterator, const DisplayItem&);
    bool wasInvalidated(const DisplayItem&) const;
    void updatePaintList();

#ifndef NDEBUG
    WTF::String paintListAsDebugString(const PaintList&) const;
#endif

    PaintList m_paintList;
    HashSet<DisplayItemClient> m_cachedClients;
    PaintList m_newPaints;
};

} // namespace blink

#endif // DisplayItemList_h
