// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterDisplayItem_h
#define FilterDisplayItem_h

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/ImageFilter.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

class PLATFORM_EXPORT BeginFilterDisplayItem : public DisplayItem {
public:
    static PassOwnPtr<BeginFilterDisplayItem> create(DisplayItemClient client, Type type, PassRefPtr<ImageFilter> imageFilter, const LayoutRect& bounds) { return adoptPtr(new BeginFilterDisplayItem(client, type, imageFilter, bounds)); }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

protected:
    BeginFilterDisplayItem(DisplayItemClient client, Type type, PassRefPtr<ImageFilter> imageFilter, const LayoutRect& bounds)
        : DisplayItem(client, type), m_imageFilter(imageFilter), m_bounds(bounds) { }

private:
#ifndef NDEBUG
    virtual const char* name() const override { return "BeginFilter"; }
    virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif

    RefPtr<ImageFilter> m_imageFilter;
    const LayoutRect m_bounds;
};

class PLATFORM_EXPORT EndFilterDisplayItem : public DisplayItem {
public:
    static PassOwnPtr<EndFilterDisplayItem> create(DisplayItemClient client) { return adoptPtr(new EndFilterDisplayItem(client)); }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

protected:
    EndFilterDisplayItem(DisplayItemClient client)
        : DisplayItem(client, EndFilter) { }

private:
#ifndef NDEBUG
    virtual const char* name() const override { return "EndFilter"; }
#endif
};

}

#endif // FilterDisplayItem_h
