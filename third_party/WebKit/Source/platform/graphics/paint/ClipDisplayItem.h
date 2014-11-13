// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipDisplayItem_h
#define ClipDisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Vector.h"

namespace blink {

class RoundedRect;

class PLATFORM_EXPORT ClipDisplayItem : public DisplayItem {
public:
    ClipDisplayItem(DisplayItemClient client, Type type, IntRect clipRect)
        : DisplayItem(client, type), m_clipRect(clipRect) { }

    Vector<RoundedRect>& roundedRectClips() { return m_roundedRectClips; }

private:
    virtual void replay(GraphicsContext*) override;

    IntRect m_clipRect;
    Vector<RoundedRect> m_roundedRectClips;
#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif
};

class PLATFORM_EXPORT EndClipDisplayItem : public DisplayItem {
public:
    EndClipDisplayItem() : DisplayItem(0, EndClip) { }

private:
    virtual void replay(GraphicsContext*) override;
};

} // namespace blink

#endif // ClipDisplayItem_h
