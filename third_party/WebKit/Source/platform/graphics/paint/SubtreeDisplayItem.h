// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubtreeDisplayItem_h
#define SubtreeDisplayItem_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Assertions.h"

namespace blink {

class PLATFORM_EXPORT SubtreeCachedDisplayItem : public DisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<SubtreeCachedDisplayItem> create(DisplayItemClient client, Type type)
    {
        return adoptPtr(new SubtreeCachedDisplayItem(client, type));
    }

private:
    SubtreeCachedDisplayItem(DisplayItemClient client, Type type)
        : DisplayItem(client, type)
    {
        ASSERT(isSubtreeCachedType(type));
    }

    virtual void replay(GraphicsContext*) override final { ASSERT_NOT_REACHED(); }
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override final { ASSERT_NOT_REACHED(); }
};

class PLATFORM_EXPORT BeginSubtreeDisplayItem : public PairedBeginDisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<BeginSubtreeDisplayItem> create(DisplayItemClient client, Type type)
    {
        return adoptPtr(new BeginSubtreeDisplayItem(client, type));
    }

private:
    BeginSubtreeDisplayItem(DisplayItemClient client, Type type)
        : PairedBeginDisplayItem(client, type)
    {
        ASSERT(isBeginSubtreeType(type));
    }
};

class PLATFORM_EXPORT EndSubtreeDisplayItem : public PairedEndDisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<EndSubtreeDisplayItem> create(DisplayItemClient client, Type type)
    {
        return adoptPtr(new EndSubtreeDisplayItem(client, type));
    }

private:
    EndSubtreeDisplayItem(DisplayItemClient client, Type type)
        : PairedEndDisplayItem(client, type)
    {
        ASSERT(isEndSubtreeType(type));
    }

#if ENABLE(ASSERT)
    virtual bool isEndAndPairedWith(const DisplayItem& other) const override final { return other.isBeginSubtree(); }
#endif
};

} // namespace blink

#endif // SubtreeDisplayItem_h
