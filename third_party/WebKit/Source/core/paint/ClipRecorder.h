// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipRecorder_h
#define ClipRecorder_h

#include "core/paint/ViewDisplayList.h"
#include "core/rendering/RenderLayerModelObject.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/RoundedRect.h"

namespace blink {

class RenderLayerModelObject;
struct PaintInfo;

class ClipDisplayItem : public DisplayItem {
public:
    ClipDisplayItem(const RenderLayerModelObject* renderer, Type type, IntRect clipRect)
        : DisplayItem(renderer, type), m_clipRect(clipRect) { }

    Vector<RoundedRect>& roundedRectClips() { return m_roundedRectClips; }
    virtual void replay(GraphicsContext*) override;

    IntRect m_clipRect;
    Vector<RoundedRect> m_roundedRectClips;
#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif
};

class EndClipDisplayItem : public DisplayItem {
public:
    EndClipDisplayItem(const RenderLayerModelObject* renderer) : DisplayItem(renderer, EndClip) { }

    virtual void replay(GraphicsContext*) override;
};

class ClipRecorder {
public:
    ClipRecorder(RenderLayerModelObject&, const PaintInfo&, const LayoutRect& clipRect);
    ~ClipRecorder();

    static DisplayItem::Type paintPhaseToClipType(PaintPhase);
private:
    LayoutRect m_clipRect;
    const PaintInfo& m_paintInfo;
    RenderLayerModelObject& m_canvas;
};

} // namespace blink

#endif // ClipRecorder_h
