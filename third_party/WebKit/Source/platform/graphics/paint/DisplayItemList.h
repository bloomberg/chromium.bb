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

typedef Vector<OwnPtr<DisplayItem> > PaintList;

class PLATFORM_EXPORT DisplayItemList {
    WTF_MAKE_NONCOPYABLE(DisplayItemList);
public:
    DisplayItemList() { };

    const PaintList& paintList();
    void add(WTF::PassOwnPtr<DisplayItem>);
    void invalidate(DisplayItemClient);

#ifndef NDEBUG
    void showDebugData() const;
#endif

private:
    PaintList::iterator findDisplayItem(PaintList::iterator, const DisplayItem&);
    bool wasInvalidated(const DisplayItem&) const;
    void updatePaintList();

    PaintList m_paintList;
    HashSet<DisplayItemClient> m_paintListRenderers;
    HashSet<DisplayItemClient> m_invalidated;
    PaintList m_newPaints;
};

} // namespace blink

#endif // DisplayItemList_h
