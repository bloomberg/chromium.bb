// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterDisplayItem_h
#define FilterDisplayItem_h

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/ImageFilter.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/PassRefPtr.h"
#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

class PLATFORM_EXPORT BeginFilterDisplayItem : public DisplayItem {
public:
    BeginFilterDisplayItem(DisplayItemClient client, Type type, PassRefPtr<ImageFilter> imageFilter, const LayoutRect& bounds)
        : DisplayItem(client, type), m_imageFilter(imageFilter), m_bounds(bounds) { }
    virtual void replay(GraphicsContext*) override;

#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif

    RefPtr<ImageFilter> m_imageFilter;
    const LayoutRect m_bounds;
};

class PLATFORM_EXPORT EndFilterDisplayItem : public DisplayItem {
public:
    EndFilterDisplayItem(DisplayItemClient client)
        : DisplayItem(client, EndFilter) { }
    virtual void replay(GraphicsContext*) override;

#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif
};

}

#endif // FilterDisplayItem_h
