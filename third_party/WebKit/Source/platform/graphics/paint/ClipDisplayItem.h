// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipDisplayItem_h
#define ClipDisplayItem_h

#include "SkRegion.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class PLATFORM_EXPORT ClipDisplayItem : public PairedBeginDisplayItem {
    WTF_MAKE_FAST_ALLOCATED(ClipDisplayItem);
public:
    static PassOwnPtr<ClipDisplayItem> create(const DisplayItemClientWrapper& client, Type type, const IntRect& clipRect, PassOwnPtr<Vector<FloatRoundedRect>> roundedRectClips = nullptr)
    {
        return adoptPtr(new ClipDisplayItem(client, type, clipRect, roundedRectClips));
    }

    ClipDisplayItem(const DisplayItemClientWrapper& client, Type type, const IntRect& clipRect, PassOwnPtr<Vector<FloatRoundedRect>> roundedRectClips = nullptr)
        : PairedBeginDisplayItem(client, type)
        , m_clipRect(clipRect)
        , m_roundedRectClips(roundedRectClips)
    {
        ASSERT(isClipType(type));
    }

    virtual void replay(GraphicsContext&) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
#ifndef NDEBUG
    virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif
    IntRect m_clipRect;
    OwnPtr<Vector<FloatRoundedRect>> m_roundedRectClips;
};

class PLATFORM_EXPORT EndClipDisplayItem : public PairedEndDisplayItem {
    WTF_MAKE_FAST_ALLOCATED(EndClipDisplayItem);
public:
    static PassOwnPtr<EndClipDisplayItem> create(const DisplayItemClientWrapper& client, Type type)
    {
        return adoptPtr(new EndClipDisplayItem(client, type));
    }

    EndClipDisplayItem(const DisplayItemClientWrapper& client, Type type)
        : PairedEndDisplayItem(client, type)
    {
        ASSERT(isEndClipType(type));
    }

    virtual void replay(GraphicsContext&) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
#if ENABLE(ASSERT)
    virtual bool isEndAndPairedWith(DisplayItem::Type otherType) const override final { return DisplayItem::isClipType(otherType); }
#endif
};

} // namespace blink

#endif // ClipDisplayItem_h
