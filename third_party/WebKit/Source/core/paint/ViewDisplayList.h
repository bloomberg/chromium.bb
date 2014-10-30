// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ViewDisplayList_h
#define ViewDisplayList_h

#include "core/rendering/PaintPhase.h"
#include "platform/geometry/RoundedRect.h"
#include "platform/graphics/DisplayList.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class ClipRect;
class GraphicsContext;
class RenderObject;
class RenderLayer;

class DisplayItem {
public:
    enum Type {
        // The Drawing types must be kept in sync with PaintPhase.
        DrawingPaintPhaseBlockBackground = 0,
        DrawingPaintPhaseChildBlockBackground = 1,
        DrawingPaintPhaseChildBlockBackgrounds = 2,
        DrawingPaintPhaseFloat = 3,
        DrawingPaintPhaseForeground = 4,
        DrawingPaintPhaseOutline = 5,
        DrawingPaintPhaseChildOutlines = 6,
        DrawingPaintPhaseSelfOutline = 7,
        DrawingPaintPhaseSelection = 8,
        DrawingPaintPhaseCollapsedTableBorders = 9,
        DrawingPaintPhaseTextClip = 10,
        DrawingPaintPhaseMask = 11,
        DrawingPaintPhaseClippingMask = 12,
        ClipLayerOverflowControls = 13,
        ClipLayerBackground = 14,
        ClipLayerParent = 15,
        ClipLayerFilter = 16,
        ClipLayerForeground = 17,
        ClipLayerFragmentFloat = 18,
        ClipLayerFragmentForeground = 19,
        ClipLayerFragmentChildOutline = 20,
        ClipLayerFragmentOutline = 21,
        ClipLayerFragmentMask = 22,
        ClipLayerFragmentClippingMask = 23,
        ClipLayerFragmentParent = 24,
        ClipLayerFragmentSelection = 25,
        ClipLayerFragmentChildBlockBackgrounds = 26,
        EndClip = 27,
    };

    virtual ~DisplayItem() { }

    virtual void replay(GraphicsContext*) = 0;

    RenderObject* renderer() const { return m_id.renderer; }
    Type type() const { return m_id.type; }
    bool idsEqual(const DisplayItem& other) const { return m_id.renderer == other.m_id.renderer && m_id.type == other.m_id.type; }

protected:
    DisplayItem(RenderObject* renderer, Type type)
    {
        m_id.renderer = renderer;
        m_id.type = type;
    }

private:
    struct Id {
        RenderObject* renderer;
        Type type;
    } m_id;
};

class ClipDisplayItem : public DisplayItem {
public:
    ClipDisplayItem(RenderObject* renderer, RenderLayer*, Type type, IntRect clipRect)
        : DisplayItem(renderer, type), m_clipRect(clipRect) { }

    Vector<RoundedRect>& roundedRectClips() { return m_roundedRectClips; }

private:
    virtual void replay(GraphicsContext*) override;

    IntRect m_clipRect;
    Vector<RoundedRect> m_roundedRectClips;
};

class EndClipDisplayItem : public DisplayItem {
public:
    EndClipDisplayItem() : DisplayItem(0, EndClip) { }

private:
    virtual void replay(GraphicsContext*) override;
};

class DrawingDisplayItem : public DisplayItem {
public:
    DrawingDisplayItem(PassRefPtr<SkPicture> picture, const FloatRect& bounds, PaintPhase phase, RenderObject* renderer)
        : DisplayItem(renderer, (Type)phase), m_picture(picture), m_bounds(bounds) { }

private:
    virtual void replay(GraphicsContext*) override;

    RefPtr<SkPicture> m_picture;
    FloatRect m_bounds;
};

class DrawingRecorder {
public:
    explicit DrawingRecorder(GraphicsContext*, RenderObject*, PaintPhase, const FloatRect&);
    ~DrawingRecorder();

private:
    GraphicsContext* m_context;
    RenderObject* m_renderer;
    PaintPhase m_phase;
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

typedef Vector<OwnPtr<DisplayItem> > PaintList;

class ViewDisplayList {
public:
    ViewDisplayList() { };

    const PaintList& paintList();
    void add(WTF::PassOwnPtr<DisplayItem>);
    void invalidate(const RenderObject*);

private:
    bool isRepaint(PaintList::iterator, const DisplayItem&);
    // Update m_paintList with any invalidations or new paints.
    void updatePaintList();

    PaintList m_paintList;
    HashSet<const RenderObject*> m_invalidated;
    PaintList m_newPaints;
};

} // namespace blink

#endif // ViewDisplayList_h
