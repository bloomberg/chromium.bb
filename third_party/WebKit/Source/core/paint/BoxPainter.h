// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxPainter_h
#define BoxPainter_h

#include "core/layout/LayoutBoxModelObject.h"
#include "core/paint/ObjectPainter.h"

namespace blink {

class BackgroundImageGeometry;
class FloatRoundedRect;
class LayoutPoint;
struct PaintInfo;
class LayoutBox;
class LayoutObject;

class BoxPainter {
public:
    BoxPainter(LayoutBox& layoutBox) : m_layoutBox(layoutBox) { }
    void paint(const PaintInfo&, const LayoutPoint&);

    void paintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&);
    void paintMask(const PaintInfo&, const LayoutPoint&);
    void paintClippingMask(const PaintInfo&, const LayoutPoint&);
    void paintFillLayers(const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance = BackgroundBleedNone, SkXfermode::Mode = SkXfermode::kSrcOver_Mode, LayoutObject* backgroundObject = 0);
    void paintMaskImages(const PaintInfo&, const LayoutRect&);
    void paintBoxDecorationBackgroundWithRect(const PaintInfo&, const LayoutPoint&, const LayoutRect&);
    static void paintFillLayerExtended(LayoutBoxModelObject&, const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance, InlineFlowBox* = 0, const LayoutSize& = LayoutSize(), SkXfermode::Mode = SkXfermode::kSrcOver_Mode, LayoutObject* backgroundObject = 0, bool skipBaseColor = false);
    static void calculateBackgroundImageGeometry(LayoutBoxModelObject&, const LayoutBoxModelObject* paintContainer, const FillLayer&, const LayoutRect& paintRect, BackgroundImageGeometry&, LayoutObject* = 0);
    static InterpolationQuality chooseInterpolationQuality(LayoutBoxModelObject&, GraphicsContext*, Image*, const void*, const LayoutSize&);
    static bool paintNinePieceImage(LayoutBoxModelObject&, GraphicsContext*, const LayoutRect&, const LayoutStyle&, const NinePieceImage&, SkXfermode::Mode = SkXfermode::kSrcOver_Mode);
    static void paintBorder(LayoutBoxModelObject&, const PaintInfo&, const LayoutRect&, const LayoutStyle&, BackgroundBleedAvoidance = BackgroundBleedNone, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
    static void paintBoxShadow(const PaintInfo&, const LayoutRect&, const LayoutStyle&, ShadowStyle, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
    static bool shouldAntialiasLines(GraphicsContext*);

private:
    void paintBackground(const PaintInfo&, const LayoutRect&, const Color& backgroundColor, BackgroundBleedAvoidance = BackgroundBleedNone);
    void paintRootBoxFillLayers(const PaintInfo&);
    void paintFillLayer(const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance, SkXfermode::Mode, LayoutObject* backgroundObject, bool skipBaseColor = false);
    static void paintRootBackgroundColor(LayoutObject&, const PaintInfo&, const LayoutRect&, const Color&);
    static FloatRoundedRect backgroundRoundedRectAdjustedForBleedAvoidance(LayoutObject&, GraphicsContext*, const LayoutRect&, BackgroundBleedAvoidance, InlineFlowBox*, const LayoutSize&, bool includeLogicalLeftEdge, bool includeLogicalRightEdge);
    static FloatRoundedRect getBackgroundRoundedRect(LayoutObject&, const LayoutRect&, InlineFlowBox*, LayoutUnit inlineBoxWidth, LayoutUnit inlineBoxHeight,
        bool includeLogicalLeftEdge, bool includeLogicalRightEdge);
    static bool isDocumentElementWithOpaqueBackground(LayoutObject&);
    static void applyBoxShadowForBackground(GraphicsContext*, LayoutObject&);
    static bool fixedBackgroundPaintsInLocalCoordinates(const LayoutObject&);
    static IntSize calculateFillTileSize(const LayoutBoxModelObject&, const FillLayer&, const IntSize& scaledPositioningAreaSize);
    static void paintTranslucentBorderSides(GraphicsContext*, const LayoutStyle&, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder, const IntPoint& innerBorderAdjustment,
        const BorderEdge[], BorderEdgeFlags, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias = false);
    static LayoutRect borderInnerRectAdjustedForBleedAvoidance(GraphicsContext*, const LayoutRect&, BackgroundBleedAvoidance);
    static void paintOneBorderSide(GraphicsContext*, const LayoutStyle&, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder,
        const FloatRect& sideRect, BoxSide, BoxSide adjacentSide1, BoxSide adjacentSide2, const BorderEdge[],
        const Path*, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, const Color* overrideColor = 0);
    static void paintBorderSides(GraphicsContext*, const LayoutStyle&, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder,
        const IntPoint& innerBorderAdjustment, const BorderEdge[], BorderEdgeFlags, BackgroundBleedAvoidance,
        bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias = false, const Color* overrideColor = 0);
    static void drawBoxSideFromPath(GraphicsContext*, const LayoutRect&, const Path&, const BorderEdge[],
        float thickness, float drawThickness, BoxSide, const LayoutStyle&,
        Color, EBorderStyle, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge);
    static void clipBorderSidePolygon(GraphicsContext*, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder,
        BoxSide, bool firstEdgeMatches, bool secondEdgeMatches);
    static void clipBorderSideForComplexInnerPath(GraphicsContext*, const FloatRoundedRect&, const FloatRoundedRect&, BoxSide, const BorderEdge[]);

    LayoutRect boundsForDrawingRecorder(const LayoutPoint& paintOffset);
    LayoutRect scrolledBackgroundRect();

    // FIXME: this should be const.
    LayoutBox& m_layoutBox;
};

} // namespace blink

#endif
