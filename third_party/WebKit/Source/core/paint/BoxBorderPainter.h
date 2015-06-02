// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxBorderPainter_h
#define BoxBorderPainter_h

#include "core/layout/LayoutBoxModelObject.h"
#include "platform/heap/Heap.h"

namespace blink {

class ComputedStyle;
class LayoutBox;
class LayoutRect;
struct PaintInfo;

// TODO(fmalita): this class will evolve into a stateful painter (merged w/ BoxBorderInfo), with no
// static methods.
class BoxBorderPainter {
    STACK_ALLOCATED();
public:
    void paintBorder(LayoutBoxModelObject&, const PaintInfo&, const LayoutRect&, const ComputedStyle&,
        BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const;

private:
    static void paintTranslucentBorderSides(GraphicsContext*, const ComputedStyle&, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder,
        const BorderEdge[], BorderEdgeFlags, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias = false);
    static void paintOneBorderSide(GraphicsContext*, const ComputedStyle&, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder,
        const FloatRect& sideRect, BoxSide, BoxSide adjacentSide1, BoxSide adjacentSide2, const BorderEdge[],
        const Path*, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, const Color* overrideColor = 0);
    static void paintBorderSides(GraphicsContext*, const ComputedStyle&, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder,
        const BorderEdge[], BorderEdgeFlags, BackgroundBleedAvoidance, bool includeLogicalLeftEdge,
        bool includeLogicalRightEdge, bool antialias = false, const Color* overrideColor = 0);
    static void drawBoxSideFromPath(GraphicsContext*, const LayoutRect&, const Path&, const BorderEdge[],
        float thickness, float drawThickness, BoxSide, const ComputedStyle&,
        Color, EBorderStyle, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge);
    static void clipBorderSidePolygon(GraphicsContext*, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder,
        BoxSide, bool firstEdgeMatches, bool secondEdgeMatches);
    static void clipBorderSideForComplexInnerPath(GraphicsContext*, const FloatRoundedRect&, const FloatRoundedRect&, BoxSide, const BorderEdge[]);
};

} // namespace blink

#endif
