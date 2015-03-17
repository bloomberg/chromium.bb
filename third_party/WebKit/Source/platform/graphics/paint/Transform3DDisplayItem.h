// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Transform3DDisplayItem_h
#define Transform3DDisplayItem_h

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/Assertions.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT BeginTransform3DDisplayItem : public PairedBeginDisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<BeginTransform3DDisplayItem> create(DisplayItemClient client, DisplayItem::Type type, const TransformationMatrix& transform)
    {
        return adoptPtr(new BeginTransform3DDisplayItem(client, type, transform));
    }

    BeginTransform3DDisplayItem(DisplayItemClient client, DisplayItem::Type type, const TransformationMatrix& transform)
        : PairedBeginDisplayItem(client, type)
        , m_transform(transform)
    {
        ASSERT(DisplayItem::isTransform3DType(type));
    }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

    const TransformationMatrix& transform() const { return m_transform; }

private:
    const TransformationMatrix m_transform;
    FloatPoint3D m_transformOrigin;
};

class PLATFORM_EXPORT EndTransform3DDisplayItem : public PairedEndDisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<EndTransform3DDisplayItem> create(DisplayItemClient client, DisplayItem::Type type)
    {
        return adoptPtr(new EndTransform3DDisplayItem(client, type));
    }

    EndTransform3DDisplayItem(DisplayItemClient client, DisplayItem::Type type)
        : PairedEndDisplayItem(client, type)
    {
        ASSERT(DisplayItem::isEndTransform3DType(type));
    }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
#if ENABLE(ASSERT)
    virtual bool isEndAndPairedWith(const DisplayItem& other) const override final
    {
        return DisplayItem::transform3DTypeToEndTransform3DType(other.type()) == type();
    }
#endif
};

} // namespace blink

#endif // Transform3DDisplayItem_h
