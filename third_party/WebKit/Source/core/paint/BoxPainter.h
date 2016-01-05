// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxPainter_h
#define BoxPainter_h

#include "core/layout/LayoutBoxModelObject.h"
#include "core/paint/ObjectPainter.h"
#include "wtf/Allocator.h"

namespace blink {

class BackgroundImageGeometry;
class FloatRoundedRect;
class LayoutPoint;
struct PaintInfo;
class LayoutBox;
class LayoutObject;

class BoxPainter {
    STACK_ALLOCATED();
public:
    BoxPainter(const LayoutBox& layoutBox) : m_layoutBox(layoutBox) { }
    void paint(const PaintInfo&, const LayoutPoint&);

    void paintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&);
    void paintMask(const PaintInfo&, const LayoutPoint&);
    void paintClippingMask(const PaintInfo&, const LayoutPoint&);

    typedef Vector<const FillLayer*, 8> FillLayerOcclusionOutputList;
    // Returns true if the result fill layers have non-associative blending or compositing mode.
    // (i.e. The rendering will be different without creating isolation group by context.saveLayer().)
    // Note that the output list will be in top-bottom order.
    bool calculateFillLayerOcclusionCulling(FillLayerOcclusionOutputList &reversedPaintList, const FillLayer&);

    void paintFillLayers(const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance = BackgroundBleedNone, SkXfermode::Mode = SkXfermode::kSrcOver_Mode, const LayoutObject* backgroundObject = nullptr);
    void paintMaskImages(const PaintInfo&, const LayoutRect&);
    void paintBoxDecorationBackgroundWithRect(const PaintInfo&, const LayoutPoint&, const LayoutRect&);
    static void paintFillLayer(const LayoutBoxModelObject&, const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance, const InlineFlowBox* = nullptr, const LayoutSize& = LayoutSize(), SkXfermode::Mode = SkXfermode::kSrcOver_Mode, const LayoutObject* backgroundObject = nullptr);
    static InterpolationQuality chooseInterpolationQuality(const LayoutObject&, Image*, const void*, const LayoutSize&);
    static bool paintNinePieceImage(const LayoutBoxModelObject&, GraphicsContext&, const LayoutRect&, const ComputedStyle&, const NinePieceImage&, SkXfermode::Mode = SkXfermode::kSrcOver_Mode);
    static void paintBorder(const LayoutBoxModelObject&, const PaintInfo&, const LayoutRect&, const ComputedStyle&, BackgroundBleedAvoidance = BackgroundBleedNone, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
    static void paintBoxShadow(const PaintInfo&, const LayoutRect&, const ComputedStyle&, ShadowStyle, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
    static bool shouldForceWhiteBackgroundForPrintEconomy(const ComputedStyle&, const Document&);

private:
    void paintBackground(const PaintInfo&, const LayoutRect&, const Color& backgroundColor, BackgroundBleedAvoidance = BackgroundBleedNone);
    static FloatRoundedRect backgroundRoundedRectAdjustedForBleedAvoidance(const LayoutObject&, const LayoutRect&, BackgroundBleedAvoidance, const InlineFlowBox*, const LayoutSize&, bool includeLogicalLeftEdge, bool includeLogicalRightEdge);
    static FloatRoundedRect getBackgroundRoundedRect(const LayoutObject&, const LayoutRect&, const InlineFlowBox*, LayoutUnit inlineBoxWidth, LayoutUnit inlineBoxHeight,
        bool includeLogicalLeftEdge, bool includeLogicalRightEdge);
    static void applyBoxShadowForBackground(GraphicsContext&, const LayoutObject&);

    LayoutRect boundsForDrawingRecorder(const LayoutPoint& paintOffset);

    const LayoutBox& m_layoutBox;
};

} // namespace blink

#endif
