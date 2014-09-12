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
    static bool paintNinePieceImage(RenderBoxModelObject&, GraphicsContext*, const LayoutRect&, const RenderStyle*, const NinePieceImage&, CompositeOperator = CompositeSourceOver);
    static void paintBorder(RenderBoxModelObject&, const PaintInfo&, const LayoutRect&, const RenderStyle*, BackgroundBleedAvoidance = BackgroundBleedNone, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
    static void paintBoxShadow(const PaintInfo&, const LayoutRect&, const RenderStyle*, ShadowStyle, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
    static bool shouldAntialiasLines(GraphicsContext*);

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
    static void paintTranslucentBorderSides(RenderObject&, GraphicsContext*, const RenderStyle*, const RoundedRect& outerBorder, const RoundedRect& innerBorder, const IntPoint& innerBorderAdjustment,
        const BorderEdge[], BorderEdgeFlags, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias = false);
    static LayoutRect borderInnerRectAdjustedForBleedAvoidance(GraphicsContext*, const LayoutRect&, BackgroundBleedAvoidance);
    static void paintOneBorderSide(RenderObject&, GraphicsContext*, const RenderStyle*, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
        const IntRect& sideRect, BoxSide, BoxSide adjacentSide1, BoxSide adjacentSide2, const BorderEdge[],
        const Path*, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, const Color* overrideColor = 0);
    static void paintBorderSides(RenderObject&, GraphicsContext*, const RenderStyle*, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
        const IntPoint& innerBorderAdjustment, const BorderEdge[], BorderEdgeFlags, BackgroundBleedAvoidance,
        bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias = false, const Color* overrideColor = 0);
    static void drawBoxSideFromPath(GraphicsContext*, const LayoutRect&, const Path&, const BorderEdge[],
        float thickness, float drawThickness, BoxSide, const RenderStyle*,
        Color, EBorderStyle, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge);
    static void clipBorderSidePolygon(GraphicsContext*, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
        BoxSide, bool firstEdgeMatches, bool secondEdgeMatches);
    static void clipBorderSideForComplexInnerPath(GraphicsContext*, const RoundedRect&, const RoundedRect&, BoxSide, const BorderEdge[]);

    // FIXME: this should be const.
    RenderBox& m_renderBox;
};

} // namespace blink

#endif
