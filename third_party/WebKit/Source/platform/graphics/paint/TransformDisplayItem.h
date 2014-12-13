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
public:
    static PassOwnPtr<BeginTransformDisplayItem> create(DisplayItemClient client, const AffineTransform& transform) { return adoptPtr(new BeginTransformDisplayItem(client, transform)); }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

protected:
    BeginTransformDisplayItem(DisplayItemClient, const AffineTransform&);

private:
#ifndef NDEBUG
    virtual const char* name() const override { return "BeginTransform"; }
#endif

    const AffineTransform m_transform;
};

class PLATFORM_EXPORT EndTransformDisplayItem : public DisplayItem {
public:
    static PassOwnPtr<EndTransformDisplayItem> create(DisplayItemClient client) { return adoptPtr(new EndTransformDisplayItem(client)); }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

protected:
    EndTransformDisplayItem(DisplayItemClient client)
        : DisplayItem(client, EndTransform) { }

private:
#ifndef NDEBUG
    virtual const char* name() const override { return "EndTransform"; }
#endif
};

} // namespace blink

#endif // TransformDisplayItem_h
