// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipDisplayItem_h
#define ClipDisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class RoundedRect;

class PLATFORM_EXPORT ClipDisplayItem : public DisplayItem {
public:
    static PassOwnPtr<ClipDisplayItem> create(DisplayItemClient client, Type type, const IntRect& clipRect) { return adoptPtr(new ClipDisplayItem(client, type, clipRect)); }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

    Vector<RoundedRect>& roundedRectClips() { return m_roundedRectClips; }

protected:
    ClipDisplayItem(DisplayItemClient client, Type type, const IntRect& clipRect)
        : DisplayItem(client, type), m_clipRect(clipRect) { }

private:
#ifndef NDEBUG
    virtual const char* name() const override { return "Clip"; }
    virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif
    IntRect m_clipRect;
    Vector<RoundedRect> m_roundedRectClips;
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
