// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ViewDisplayList_h
#define ViewDisplayList_h

#include "core/rendering/PaintPhase.h"
#include "wtf/HashSet.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class GraphicsContext;
class RenderObject;
class RenderLayer;

class DisplayItem {
public:
    enum Type {
        // DisplayItem types must be kept in sync with PaintPhase.
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

    const RenderObject* renderer() const { return m_id.renderer; }
    Type type() const { return m_id.type; }
    bool idsEqual(const DisplayItem& other) const { return m_id.renderer == other.m_id.renderer && m_id.type == other.m_id.type; }

#ifndef NDEBUG
    static WTF::String typeAsDebugString(DisplayItem::Type);
    static WTF::String rendererDebugString(const RenderObject*);
    virtual WTF::String asDebugString() const;
#endif

protected:
    DisplayItem(const RenderObject* renderer, Type type)
        : m_id(renderer, type)
    { }

private:
    struct Id {
        Id(const RenderObject* r, Type t)
            : renderer(r)
            , type(t)
        { }

        const RenderObject* renderer;
        const Type type;
    } m_id;
};

typedef Vector<OwnPtr<DisplayItem> > PaintList;

class ViewDisplayList {
public:
    ViewDisplayList() { };

    const PaintList& paintList();
    void add(WTF::PassOwnPtr<DisplayItem>);
    void invalidate(const RenderObject*);

#ifndef NDEBUG
    void showDebugData() const;
#endif

private:
    PaintList::iterator findDisplayItem(PaintList::iterator, const DisplayItem&);
    bool wasInvalidated(const DisplayItem&) const;
    void updatePaintList();

    PaintList m_paintList;
    HashSet<const RenderObject*> m_paintListRenderers;
    HashSet<const RenderObject*> m_invalidated;
    PaintList m_newPaints;
};

} // namespace blink

#endif // ViewDisplayList_h
