// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ViewDisplayList_h
#define ViewDisplayList_h

#include "core/rendering/PaintPhase.h"
#include "platform/graphics/DisplayList.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class GraphicsContext;
class RenderObject;
struct AtomicPaintChunk;

typedef Vector<OwnPtr<AtomicPaintChunk> > PaintCommandList;

struct AtomicPaintChunk {
    AtomicPaintChunk(PassRefPtr<DisplayList> inDisplayList, RenderObject* inRenderer, PaintPhase inPhase)
        : displayList(inDisplayList), renderer(inRenderer), phase(inPhase) { };

    RefPtr<DisplayList> displayList;

    // This auxillary data can be moved off the chunk if needed.
    RenderObject* renderer;
    PaintPhase phase;
};

class PaintCommandRecorder {
public:
    explicit PaintCommandRecorder(GraphicsContext*, RenderObject*, PaintPhase, const FloatRect&);
    ~PaintCommandRecorder();

private:
    GraphicsContext* m_context;
    RenderObject* m_renderer;
    PaintPhase m_phase;
};

class ViewDisplayList {
public:
    ViewDisplayList() { };

    const PaintCommandList& paintCommandList();
    void add(WTF::PassOwnPtr<AtomicPaintChunk>);
    void invalidate(const RenderObject*);

private:
    bool isRepaint(PaintCommandList::iterator, const AtomicPaintChunk&);
    // Update m_paintList with any invalidations or new paints.
    void updatePaintCommandList();

    PaintCommandList m_paintList;
    HashSet<const RenderObject*> m_invalidated;
    PaintCommandList m_newPaints;
};

} // namespace blink

#endif // ViewDisplayList_h
