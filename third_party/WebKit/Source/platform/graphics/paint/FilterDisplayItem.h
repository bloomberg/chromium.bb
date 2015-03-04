// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterDisplayItem_h
#define FilterDisplayItem_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "public/platform/WebFilterOperations.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

class PLATFORM_EXPORT BeginFilterDisplayItem : public PairedBeginDisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<BeginFilterDisplayItem> create(DisplayItemClient client, PassRefPtr<SkImageFilter> imageFilter, const FloatRect& bounds)
    {
        return adoptPtr(new BeginFilterDisplayItem(client, imageFilter, bounds));
    }

    static PassOwnPtr<BeginFilterDisplayItem> create(DisplayItemClient client, PassRefPtr<SkImageFilter> imageFilter, const FloatRect& bounds, PassOwnPtr<WebFilterOperations> filterOperations)
    {
        return adoptPtr(new BeginFilterDisplayItem(client, imageFilter, bounds, filterOperations));
    }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;
    virtual bool drawsContent() const override;

private:
    BeginFilterDisplayItem(DisplayItemClient, PassRefPtr<SkImageFilter>, const FloatRect& bounds);
    BeginFilterDisplayItem(DisplayItemClient, PassRefPtr<SkImageFilter>, const FloatRect& bounds, PassOwnPtr<WebFilterOperations>);

#ifndef NDEBUG
    virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif

    // FIXME: m_imageFilter should be replaced with m_webFilterOperations when copying data to the compositor.
    RefPtr<SkImageFilter> m_imageFilter;
    OwnPtr<WebFilterOperations> m_webFilterOperations;
    const FloatRect m_bounds;
};

class PLATFORM_EXPORT EndFilterDisplayItem : public PairedEndDisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<EndFilterDisplayItem> create(DisplayItemClient client)
    {
        return adoptPtr(new EndFilterDisplayItem(client));
    }

    EndFilterDisplayItem(DisplayItemClient client)
        : PairedEndDisplayItem(client, EndFilter) { }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
#if ENABLE(ASSERT)
    virtual bool isEndAndPairedWith(const DisplayItem& other) const override final { return other.type() == BeginFilter; }
#endif
};

}

#endif // FilterDisplayItem_h
