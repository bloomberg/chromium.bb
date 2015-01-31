// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformDisplayItem_h
#define TransformDisplayItem_h

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT BeginTransformDisplayItem : public DisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<BeginTransformDisplayItem> create(DisplayItemClient client, const AffineTransform& transform)
    {
        return adoptPtr(new BeginTransformDisplayItem(client, transform));
    }

    BeginTransformDisplayItem(DisplayItemClient client, const AffineTransform& transform)
        : DisplayItem(client, BeginTransform)
        , m_transform(transform) { }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
    const AffineTransform m_transform;
};

class PLATFORM_EXPORT EndTransformDisplayItem : public DisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<EndTransformDisplayItem> create(DisplayItemClient client)
    {
        return adoptPtr(new EndTransformDisplayItem(client));
    }

    EndTransformDisplayItem(DisplayItemClient client)
        : DisplayItem(client, EndTransform) { }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;
};

} // namespace blink

#endif // TransformDisplayItem_h
