/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GraphicsLayerUpdater_h
#define GraphicsLayerUpdater_h

#include "wtf/Vector.h"

namespace WebCore {

class GraphicsLayer;
class RenderLayer;
class RenderPart;
class RenderView;

class GraphicsLayerUpdater {
public:
    explicit GraphicsLayerUpdater(RenderView&);
    ~GraphicsLayerUpdater();

    void updateRecursive(RenderLayer&);
    void rebuildTree(RenderLayer&, Vector<GraphicsLayer*>& childLayersOfEnclosingLayer, int depth);

private:
    void update(RenderLayer&);

    RenderView& m_renderView;

    // Used for gathering UMA data about the effect on memory usage of promoting all layers
    // that have a webkit-transition on opacity or transform and intersect the viewport.
    double m_pixelsWithoutPromotingAllTransitions;
    double m_pixelsAddedByPromotingAllTransitions;
};

} // namespace WebCore

#endif // GraphicsLayerUpdater_h
