// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CachedDisplayItem_h
#define CachedDisplayItem_h

#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Assertions.h"

namespace blink {

// A placeholder of DisplayItem in the new paint list of DisplayItemList, to indicate that
// the DisplayItem has not been changed and should be replaced with the cached DisplayItem
// when merging new paint list to cached paint list.
class PLATFORM_EXPORT CachedDisplayItem : public DisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<CachedDisplayItem> create(DisplayItemClient client, Type type)
    {
        return adoptPtr(new CachedDisplayItem(client, type));
    }

private:
    CachedDisplayItem(DisplayItemClient client, Type type)
        : DisplayItem(client, type)
    {
        ASSERT(isCachedType(type));
    }

    // CachedDisplayItem is never replayed or appended to WebDisplayItemList.
    virtual void replay(GraphicsContext*) override final { ASSERT_NOT_REACHED(); }
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override final { ASSERT_NOT_REACHED(); }
};

} // namespace blink

#endif // CachedDisplayItem_h
