// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FixedPositionDisplayItem_h
#define FixedPositionDisplayItem_h

#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/FastAllocBase.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT BeginFixedPositionDisplayItem : public PairedBeginDisplayItem {
    WTF_MAKE_FAST_ALLOCATED(BeginFixedPositionDisplayItem);
public:
    static PassOwnPtr<BeginFixedPositionDisplayItem> create(const DisplayItemClientWrapper& client)
    {
        return adoptPtr(new BeginFixedPositionDisplayItem(client));
    }

    BeginFixedPositionDisplayItem(const DisplayItemClientWrapper& client)
        : PairedBeginDisplayItem(client, BeginFixedPosition)
    {
    }

    virtual void replay(GraphicsContext&) override final { }
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override final;
};

class PLATFORM_EXPORT EndFixedPositionDisplayItem : public PairedEndDisplayItem {
    WTF_MAKE_FAST_ALLOCATED(EndFixedPositionDisplayItem);
public:
    static PassOwnPtr<EndFixedPositionDisplayItem> create(const DisplayItemClientWrapper& client)
    {
        return adoptPtr(new EndFixedPositionDisplayItem(client));
    }

    EndFixedPositionDisplayItem(const DisplayItemClientWrapper& client)
        : PairedEndDisplayItem(client, EndFixedPosition)
    {
    }

    virtual void replay(GraphicsContext&) override final { }
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override final;

private:
#if ENABLE(ASSERT)
    virtual bool isEndAndPairedWith(const DisplayItem& other) const override final { return other.type() == BeginFixedPosition; }
#endif
};

} // namespace blink

#endif // FixedPositionDisplayItem_h
