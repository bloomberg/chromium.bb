// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ReplicaPainter.h"

#include "core/layout/LayoutReplica.h"
#include "core/layout/PaintInfo.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "core/paint/DeprecatedPaintLayerPainter.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"

namespace blink {

void ReplicaPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderReplica);

    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseMask)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_renderReplica.location();

    if (paintInfo.phase == PaintPhaseForeground) {
        // Turn around and paint the parent layer. Use temporary clipRects, so that the layer doesn't end up caching clip rects
        // computing using the wrong rootLayer
        DeprecatedPaintLayer* rootPaintingLayer = m_renderReplica.layer()->transform() ? m_renderReplica.layer()->parent() : m_renderReplica.layer()->enclosingTransformedAncestor();
        DeprecatedPaintLayerPaintingInfo paintingInfo(rootPaintingLayer, LayoutRect(paintInfo.rect), PaintBehaviorNormal, LayoutSize(), 0);
        PaintLayerFlags flags = PaintLayerHaveTransparency | PaintLayerAppliedTransform | PaintLayerUncachedClipRects | PaintLayerPaintingReflection;
        DeprecatedPaintLayerPainter(*m_renderReplica.layer()->parent()).paintLayer(paintInfo.context, paintingInfo, flags);
    } else if (paintInfo.phase == PaintPhaseMask) {
        LayoutRect paintRect(adjustedPaintOffset, m_renderReplica.size());
        LayoutObjectDrawingRecorder drawingRecorder(paintInfo.context, m_renderReplica, paintInfo.phase, paintRect);
        if (!drawingRecorder.canUseCachedDrawing())
            m_renderReplica.paintMask(paintInfo, adjustedPaintOffset);
    }
}

} // namespace blink
