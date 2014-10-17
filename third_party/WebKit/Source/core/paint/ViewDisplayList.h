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
struct AtomicPaintChunk;


struct DisplayItem {
    virtual void replay(GraphicsContext*) = 0;

    virtual ~DisplayItem() { }
};

struct ClipDisplayItem : DisplayItem {
    RenderLayer* layer;
    enum ClipType {
        LayerOverflowControls,
        LayerBackground,
        LayerParent,
        LayerFilter,
        LayerForeground,
        LayerFragmentFloat,
        LayerFragmentForeground,
        LayerFragmentChildOutline,
        LayerFragmentOutline,
        LayerFragmentMask,
        LayerFragmentClippingMask,
        LayerFragmentParent,
        LayerFragmentSelection,
        LayerFragmentChildBlockBackgrounds
    };
    ClipType clipType;
    IntRect clipRect;
    Vector<RoundedRect> roundedRectClips;

    virtual void replay(GraphicsContext*);
};

struct EndClipDisplayItem : DisplayItem {
    RenderLayer* layer;
    virtual void replay(GraphicsContext*);
};

struct AtomicPaintChunk : DisplayItem {
    AtomicPaintChunk(PassRefPtr<DisplayList> inDisplayList, RenderObject* inRenderer, PaintPhase inPhase)
        : displayList(inDisplayList), renderer(inRenderer), phase(inPhase) { };

    RefPtr<DisplayList> displayList;

    // This auxillary data can be moved off the chunk if needed.
    RenderObject* renderer;
    PaintPhase phase;

    virtual void replay(GraphicsContext*);
};

typedef Vector<OwnPtr<DisplayItem> > PaintList;

class PaintCommandRecorder {
public:
    explicit PaintCommandRecorder(GraphicsContext*, RenderObject*, PaintPhase, const FloatRect&);
    ~PaintCommandRecorder();

private:
    GraphicsContext* m_context;
    RenderObject* m_renderer;
    PaintPhase m_phase;
};

class ClipRecorder {
public:
    explicit ClipRecorder(RenderLayer*, GraphicsContext*, ClipDisplayItem::ClipType, const ClipRect&);
    void addRoundedRectClip(const RoundedRect&);

    ~ClipRecorder();

private:
    OwnPtr<ClipDisplayItem> m_clipDisplayItem;
    GraphicsContext* m_graphicsContext;
    RenderLayer* m_renderLayer;
};

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
