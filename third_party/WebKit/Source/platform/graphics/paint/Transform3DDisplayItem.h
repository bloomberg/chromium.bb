// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Transform3DDisplayItem_h
#define Transform3DDisplayItem_h

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT BeginTransform3DDisplayItem : public DisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<BeginTransform3DDisplayItem> create(DisplayItemClient client, const TransformationMatrix& transform)
    {
        return adoptPtr(new BeginTransform3DDisplayItem(client, transform));
    }

    BeginTransform3DDisplayItem(DisplayItemClient client, const TransformationMatrix& transform)
        : DisplayItem(client, BeginTransform)
        , m_transform(transform) { }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
#ifndef NDEBUG
    virtual const char* name() const override { return "BeginTransform3D"; }
#endif

    const TransformationMatrix m_transform;
    FloatPoint3D m_transformOrigin;
};

class PLATFORM_EXPORT EndTransform3DDisplayItem : public DisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<EndTransform3DDisplayItem> create(DisplayItemClient client)
    {
        return adoptPtr(new EndTransform3DDisplayItem(client));
    }

    EndTransform3DDisplayItem(DisplayItemClient client)
        : DisplayItem(client, EndTransform) { }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
#ifndef NDEBUG
    virtual const char* name() const override { return "EndTransform3D"; }
#endif
};

} // namespace blink

#endif // Transform3DDisplayItem_h
