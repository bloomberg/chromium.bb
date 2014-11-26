// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransparencyDisplayItem_h
#define TransparencyDisplayItem_h

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "public/platform/WebBlendMode.h"
#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

class PLATFORM_EXPORT BeginTransparencyDisplayItem : public DisplayItem {
public:
    BeginTransparencyDisplayItem(DisplayItemClient client, Type type, const WebBlendMode& blendMode, const float opacity)
        : DisplayItem(client, type)
        , m_blendMode(blendMode)
        , m_opacity(opacity) { }
    virtual void replay(GraphicsContext*) override;

private:
#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif

    bool hasBlendMode() const { return m_blendMode != WebBlendModeNormal; }

    const WebBlendMode m_blendMode;
    const float m_opacity;
};

class PLATFORM_EXPORT EndTransparencyDisplayItem : public DisplayItem {
public:
    EndTransparencyDisplayItem(DisplayItemClient client, Type type)
        : DisplayItem(client, type) { }
    virtual void replay(GraphicsContext*) override;

private:
#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif
};

} // namespace blink

#endif // TransparencyDisplayItem_h
