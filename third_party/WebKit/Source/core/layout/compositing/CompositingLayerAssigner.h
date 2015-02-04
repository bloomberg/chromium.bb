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

#ifndef CompositingLayerAssigner_h
#define CompositingLayerAssigner_h

#include "core/layout/compositing/LayerCompositor.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutPoint.h"

namespace blink {

class Layer;

class CompositingLayerAssigner {
public:
    explicit CompositingLayerAssigner(LayerCompositor*);
    ~CompositingLayerAssigner();

    void assign(Layer* updateRoot, Vector<Layer*>& layersNeedingPaintInvalidation);

    bool layersChanged() const { return m_layersChanged; }

    // FIXME: This function should be private. We should remove the one caller
    // once we've fixed the compositing chicken/egg issues.
    CompositingStateTransitionType computeCompositedLayerUpdate(Layer*);

private:
    struct SquashingState {
        SquashingState()
            : mostRecentMapping(0)
            , hasMostRecentMapping(false)
            , haveAssignedBackingsToEntireSquashingLayerSubtree(false)
            , nextSquashedLayerIndex(0)
            , totalAreaOfSquashedRects(0) { }

        void updateSquashingStateForNewMapping(CompositedLayerMapping*, bool hasNewCompositedLayerMapping);

        // The most recent composited backing that the layer should squash onto if needed.
        CompositedLayerMapping* mostRecentMapping;
        bool hasMostRecentMapping;

        // Whether all Layers in the stacking subtree rooted at the most recent mapping's
        // owning layer have had CompositedLayerMappings assigned. Layers cannot squash into a
        // CompositedLayerMapping owned by a stacking ancestor, since this changes paint order.
        bool haveAssignedBackingsToEntireSquashingLayerSubtree;

        // Counter that tracks what index the next Layer would be if it gets squashed to the current squashing layer.
        size_t nextSquashedLayerIndex;

        // The absolute bounding rect of all the squashed layers.
        IntRect boundingRect;

        // This is simply the sum of the areas of the squashed rects. This can be very skewed if the rects overlap,
        // but should be close enough to drive a heuristic.
        uint64_t totalAreaOfSquashedRects;
    };

    void assignLayersToBackingsInternal(Layer*, SquashingState&, Vector<Layer*>& layersNeedingPaintInvalidation);
    void assignLayersToBackingsForReflectionLayer(Layer* reflectionLayer, Vector<Layer*>& layersNeedingPaintInvalidation);
    CompositingReasons getReasonsPreventingSquashing(const Layer*, const SquashingState&);
    bool squashingWouldExceedSparsityTolerance(const Layer* candidate, const SquashingState&);
    void updateSquashingAssignment(Layer*, SquashingState&, CompositingStateTransitionType, Vector<Layer*>& layersNeedingPaintInvalidation);
    bool needsOwnBacking(const Layer*) const;

    LayerCompositor* m_compositor;
    bool m_layersChanged;
};

} // namespace blink

#endif // CompositingLayerAssigner_h
