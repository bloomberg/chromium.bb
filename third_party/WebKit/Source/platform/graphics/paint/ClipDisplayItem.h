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

class PLATFORM_EXPORT ClipDisplayItem : public DisplayItem {
public:
    static PassOwnPtr<ClipDisplayItem> create(DisplayItemClient client, Type type, const IntRect& clipRect, SkRegion::Op operation = SkRegion::kIntersect_Op)
    {
        return adoptPtr(new ClipDisplayItem(client, type, clipRect, operation));
    }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

    Vector<FloatRoundedRect>& roundedRectClips() { return m_roundedRectClips; }

protected:
    ClipDisplayItem(DisplayItemClient client, Type type, const IntRect& clipRect, SkRegion::Op operation)
        : DisplayItem(client, type), m_clipRect(clipRect), m_operation(operation) { }

private:
#ifndef NDEBUG
    virtual const char* name() const override { return "Clip"; }
    virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif
    IntRect m_clipRect;
    Vector<FloatRoundedRect> m_roundedRectClips;
    SkRegion::Op m_operation;
};

class PLATFORM_EXPORT EndClipDisplayItem : public DisplayItem {
public:
    static PassOwnPtr<EndClipDisplayItem> create(DisplayItemClient client) { return adoptPtr(new EndClipDisplayItem(client)); }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

protected:
    EndClipDisplayItem(DisplayItemClient client) : DisplayItem(client, EndClip) { }

private:
#ifndef NDEBUG
    virtual const char* name() const override { return "EndClip"; }
#endif
};

} // namespace blink

#endif // ClipDisplayItem_h
