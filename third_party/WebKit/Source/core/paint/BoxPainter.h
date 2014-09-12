// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxPainter_h
#define BoxPainter_h

#include "core/rendering/RenderBoxModelObject.h"

namespace blink {

class LayoutPoint;
struct PaintInfo;
class RenderBox;
class RenderObject;

class BoxPainter {
public:
    BoxPainter(RenderBox& renderBox) : m_renderBox(renderBox) { }
    void paint(PaintInfo&, const LayoutPoint&);

    void paintBoxDecorationBackground(PaintInfo&, const LayoutPoint&);
    void paintMask(PaintInfo&, const LayoutPoint&);
    void paintClippingMask(PaintInfo&, const LayoutPoint&);
    void paintFillLayers(const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance = BackgroundBleedNone, CompositeOperator = CompositeSourceOver, RenderObject* backgroundObject = 0);
    void paintMaskImages(const PaintInfo&, const LayoutRect&);
    void paintBoxDecorationBackgroundWithRect(PaintInfo&, const LayoutPoint&, const LayoutRect&);
    static void paintFillLayerExtended(RenderBoxModelObject&, const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance, InlineFlowBox* = 0, const LayoutSize& = LayoutSize(), CompositeOperator = CompositeSourceOver, RenderObject* backgroundObject = 0, bool skipBaseColor = false);
    static void calculateBackgroundImageGeometry(RenderBoxModelObject&, const RenderLayerModelObject* paintContainer, const FillLayer&, const LayoutRect& paintRect, BackgroundImageGeometry&, RenderObject* = 0);
    static InterpolationQuality chooseInterpolationQuality(RenderBoxModelObject&, GraphicsContext*, Image*, const void*, const LayoutSize&);
    static void clipRoundedInnerRect(GraphicsContext*, const LayoutRect&, const RoundedRect& clipRect);

private:
    void paintBackground(const PaintInfo&, const LayoutRect&, const Color& backgroundColor, BackgroundBleedAvoidance = BackgroundBleedNone);
    void paintRootBoxFillLayers(const PaintInfo&);
    void paintFillLayer(const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance, CompositeOperator, RenderObject* backgroundObject, bool skipBaseColor = false);
    static void paintRootBackgroundColor(RenderObject&, const PaintInfo&, const LayoutRect&, const Color&);
    static RoundedRect backgroundRoundedRectAdjustedForBleedAvoidance(RenderObject&, GraphicsContext*, const LayoutRect&, BackgroundBleedAvoidance, InlineFlowBox*, const LayoutSize&, bool includeLogicalLeftEdge, bool includeLogicalRightEdge);
    static RoundedRect getBackgroundRoundedRect(RenderObject&, const LayoutRect&, InlineFlowBox*, LayoutUnit inlineBoxWidth, LayoutUnit inlineBoxHeight,
        bool includeLogicalLeftEdge, bool includeLogicalRightEdge);
    static bool isDocumentElementWithOpaqueBackground(RenderObject&);
    static void applyBoxShadowForBackground(GraphicsContext*, RenderObject&);
    static bool fixedBackgroundPaintsInLocalCoordinates(const RenderObject&);
    static IntSize calculateFillTileSize(const RenderBoxModelObject&, const FillLayer&, const IntSize& scaledPositioningAreaSize);

    // FIXME: this should be const.
    RenderBox& m_renderBox;
};

} // namespace blink

#endif
