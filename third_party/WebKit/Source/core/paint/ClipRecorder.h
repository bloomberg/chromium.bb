// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipRecorder_h
#define ClipRecorder_h

#include "core/paint/ViewDisplayList.h"
#include "core/rendering/PaintPhase.h"
#include "platform/geometry/RoundedRect.h"
#include "wtf/Vector.h"

namespace blink {

class ClipRect;
class GraphicsContext;
class RenderObject;
class RenderLayer;

class ClipDisplayItem : public DisplayItem {
public:
    ClipDisplayItem(RenderObject* renderer, RenderLayer*, Type type, IntRect clipRect)
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
    EndClipDisplayItem() : DisplayItem(0, EndClip) { }

private:
    virtual void replay(GraphicsContext*) override;
};

class ClipRecorder {
public:
    explicit ClipRecorder(RenderLayer*, GraphicsContext*, DisplayItem::Type, const ClipRect&);
    void addRoundedRectClip(const RoundedRect&);

    ~ClipRecorder();

private:
    ClipDisplayItem* m_clipDisplayItem;
    GraphicsContext* m_graphicsContext;
    RenderLayer* m_renderLayer;
};

} // namespace blink

#endif // ViewDisplayList_h
