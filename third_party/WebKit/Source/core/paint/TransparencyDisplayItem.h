// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransparencyDisplayItem_h
#define TransparencyDisplayItem_h

#include "core/paint/ViewDisplayList.h"
#include "platform/geometry/LayoutRect.h"
#include "public/platform/WebBlendMode.h"

namespace blink {

class BeginTransparencyDisplayItem : public DisplayItem {
public:
    BeginTransparencyDisplayItem(const RenderObject* renderer, Type type, const LayoutRect& clipRect, const WebBlendMode& blendMode, const float opacity)
        : DisplayItem(renderer, type)
        , m_clipRect(clipRect)
        , m_blendMode(blendMode)
        , m_opacity(opacity) { }
    virtual void replay(GraphicsContext*) override;

private:
#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif

    bool hasBlendMode() const { return m_blendMode != WebBlendModeNormal; }

    const LayoutRect m_clipRect;
    const WebBlendMode m_blendMode;
    const float m_opacity;
};

class EndTransparencyDisplayItem : public DisplayItem {
public:
    EndTransparencyDisplayItem(const RenderObject* renderer, Type type)
        : DisplayItem(renderer, type) { }
    virtual void replay(GraphicsContext*) override;

private:
#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif
};

} // namespace blink

#endif // TransparencyDisplayItem_h
