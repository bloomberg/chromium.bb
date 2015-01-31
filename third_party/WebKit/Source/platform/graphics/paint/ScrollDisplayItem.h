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

class PLATFORM_EXPORT BeginScrollDisplayItem : public DisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<BeginScrollDisplayItem> create(DisplayItemClient client, Type type, const IntSize& currentOffset)
    {
        return adoptPtr(new BeginScrollDisplayItem(client, type, currentOffset));
    }

    BeginScrollDisplayItem(DisplayItemClient client, Type type, const IntSize& currentOffset)
        : DisplayItem(client, type)
        , m_currentOffset(currentOffset) { }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
    const IntSize m_currentOffset;
};

class PLATFORM_EXPORT EndScrollDisplayItem : public DisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<EndScrollDisplayItem> create(DisplayItemClient client, Type type)
    {
        return adoptPtr(new EndScrollDisplayItem(client, type));
    }

    EndScrollDisplayItem(DisplayItemClient client, Type type)
        : DisplayItem(client, type) { }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;
};

} // namespace blink

#endif // ScrollDisplayItem_h
