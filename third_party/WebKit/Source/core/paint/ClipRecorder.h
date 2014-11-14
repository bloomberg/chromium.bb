// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipRecorder_h
#define ClipRecorder_h

#include "core/paint/ViewDisplayList.h"
#include "core/rendering/PaintPhase.h"
#include "core/rendering/RenderLayerModelObject.h"
#include "platform/geometry/RoundedRect.h"
#include "wtf/Vector.h"

namespace blink {

class ClipRect;
class GraphicsContext;

class ClipDisplayItem : public DisplayItem {
public:
    ClipDisplayItem(const RenderLayerModelObject* renderer, Type type, IntRect clipRect)
        : DisplayItem(renderer, type), m_clipRect(clipRect) { }

    Vector<RoundedRect>& roundedRectClips() { return m_roundedRectClips; }

private:
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

private:
    virtual void replay(GraphicsContext*) override;
};

class ClipRecorder {
public:
    explicit ClipRecorder(const RenderLayerModelObject*, GraphicsContext*, DisplayItem::Type, const ClipRect&);
    void addRoundedRectClip(const RoundedRect&);

    ~ClipRecorder();

private:
    ClipDisplayItem* m_clipDisplayItem;
    GraphicsContext* m_graphicsContext;
    const RenderLayerModelObject* m_renderer;
};

} // namespace blink

#endif // ViewDisplayList_h
