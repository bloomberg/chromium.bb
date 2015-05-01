// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FixedPositionContainerDisplayItem_h
#define FixedPositionContainerDisplayItem_h

#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/FastAllocBase.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT BeginFixedPositionContainerDisplayItem : public PairedBeginDisplayItem {
    WTF_MAKE_FAST_ALLOCATED(BeginFixedPositionContainerDisplayItem);
public:
    static PassOwnPtr<BeginFixedPositionContainerDisplayItem> create(const DisplayItemClientWrapper& client)
    {
        return adoptPtr(new BeginFixedPositionContainerDisplayItem(client));
    }

    BeginFixedPositionContainerDisplayItem(const DisplayItemClientWrapper& client)
        : PairedBeginDisplayItem(client, BeginFixedPositionContainer)
    {
    }

    virtual void replay(GraphicsContext&) override final { }
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override final;
};

class PLATFORM_EXPORT EndFixedPositionContainerDisplayItem : public PairedEndDisplayItem {
    WTF_MAKE_FAST_ALLOCATED(EndFixedPositionContainerDisplayItem);
public:
    static PassOwnPtr<EndFixedPositionContainerDisplayItem> create(const DisplayItemClientWrapper& client)
    {
        return adoptPtr(new EndFixedPositionContainerDisplayItem(client));
    }

    EndFixedPositionContainerDisplayItem(const DisplayItemClientWrapper& client)
        : PairedEndDisplayItem(client, EndFixedPositionContainer)
    {
    }

    virtual void replay(GraphicsContext&) override final { }
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override final;

private:
#if ENABLE(ASSERT)
    virtual bool isEndAndPairedWith(const DisplayItem& other) const override final { return other.type() == BeginFixedPositionContainer; }
#endif
};

} // namespace blink

#endif // FixedPositionContainerDisplayItem_h
