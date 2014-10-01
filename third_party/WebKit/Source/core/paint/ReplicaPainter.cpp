// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ReplicaPainter.h"

#include "core/paint/LayerPainter.h"
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderReplica.h"

namespace blink {

void ReplicaPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderReplica);

    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseMask)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_renderReplica.location();

    if (paintInfo.phase == PaintPhaseForeground) {
        // Turn around and paint the parent layer. Use temporary clipRects, so that the layer doesn't end up caching clip rects
        // computing using the wrong rootLayer
        RenderLayer* rootPaintingLayer = m_renderReplica.layer()->transform() ? m_renderReplica.layer()->parent() : m_renderReplica.layer()->enclosingTransformedAncestor();
        LayerPaintingInfo paintingInfo(rootPaintingLayer, paintInfo.rect, PaintBehaviorNormal, LayoutSize(), 0);
        PaintLayerFlags flags = PaintLayerHaveTransparency | PaintLayerAppliedTransform | PaintLayerUncachedClipRects | PaintLayerPaintingReflection;
        LayerPainter(*m_renderReplica.layer()->parent()).paintLayer(paintInfo.context, paintingInfo, flags);
    } else if (paintInfo.phase == PaintPhaseMask) {
        m_renderReplica.paintMask(paintInfo, adjustedPaintOffset);
    }
}

} // namespace blink
