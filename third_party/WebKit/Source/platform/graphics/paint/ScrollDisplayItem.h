// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollDisplayItem_h
#define ScrollDisplayItem_h

#include "platform/geometry/IntSize.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/FastAllocBase.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT BeginScrollDisplayItem : public PairedBeginDisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<BeginScrollDisplayItem> create(DisplayItemClient client, Type type, const IntSize& currentOffset)
    {
        return adoptPtr(new BeginScrollDisplayItem(client, type, currentOffset));
    }

    BeginScrollDisplayItem(DisplayItemClient client, Type type, const IntSize& currentOffset)
        : PairedBeginDisplayItem(client, type)
        , m_currentOffset(currentOffset)
    {
        ASSERT(isScrollType(type));
    }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
    const IntSize m_currentOffset;
};

class PLATFORM_EXPORT EndScrollDisplayItem : public PairedEndDisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<EndScrollDisplayItem> create(DisplayItemClient client, Type type)
    {
        return adoptPtr(new EndScrollDisplayItem(client, type));
    }

    EndScrollDisplayItem(DisplayItemClient client, Type type)
        : PairedEndDisplayItem(client, type)
    {
        ASSERT(isEndScrollType(type));
    }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
#if ENABLE(ASSERT)
    virtual bool isEndAndPairedWith(const DisplayItem& other) const override final { return other.isScroll(); }
#endif
};

} // namespace blink

#endif // ScrollDisplayItem_h
