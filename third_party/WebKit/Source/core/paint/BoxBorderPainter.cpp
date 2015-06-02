// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/BoxBorderPainter.h"

#include "core/paint/BoxPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/style/BorderEdge.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

namespace {

enum BorderEdgeFlag {
    TopBorderEdge = 1 << BSTop,
    RightBorderEdge = 1 << BSRight,
    BottomBorderEdge = 1 << BSBottom,
    LeftBorderEdge = 1 << BSLeft,
    AllBorderEdges = TopBorderEdge | BottomBorderEdge | LeftBorderEdge | RightBorderEdge
};

inline BorderEdgeFlag edgeFlagForSide(BoxSide side)
{
    return static_cast<BorderEdgeFlag>(1 << side);
}

inline bool includesEdge(BorderEdgeFlags flags, BoxSide side)
{
    return flags & edgeFlagForSide(side);
}

inline bool includesAdjacentEdges(BorderEdgeFlags flags)
{
    // The set includes adjacent edges iff it contains at least one horizontal and one vertical edge.
    return (flags & (TopBorderEdge | BottomBorderEdge))
        && (flags & (LeftBorderEdge | RightBorderEdge));
}

inline bool styleRequiresClipPolygon(EBorderStyle style)
{
    // These are drawn with a stroke, so we have to clip to get corner miters.
    return style == DOTTED || style == DASHED;
}

inline bool borderStyleFillsBorderArea(EBorderStyle style)
{
    return !(style == DOTTED || style == DASHED || style == DOUBLE);
}

inline bool borderStyleHasInnerDetail(EBorderStyle style)
{
    return style == GROOVE || style == RIDGE || style == DOUBLE;
}

inline bool borderStyleIsDottedOrDashed(EBorderStyle style)
{
    return style == DOTTED || style == DASHED;
}

// OUTSET darkens the bottom and right (and maybe lightens the top and left)
// INSET darkens the top and left (and maybe lightens the bottom and right)
inline bool borderStyleHasUnmatchedColorsAtCorner(EBorderStyle style, BoxSide side, BoxSide adjacentSide)
{
    // These styles match at the top/left and bottom/right.
    if (style == INSET || style == GROOVE || style == RIDGE || style == OUTSET) {
        const BorderEdgeFlags topRightFlags = edgeFlagForSide(BSTop) | edgeFlagForSide(BSRight);
        const BorderEdgeFlags bottomLeftFlags = edgeFlagForSide(BSBottom) | edgeFlagForSide(BSLeft);

        BorderEdgeFlags flags = edgeFlagForSide(side) | edgeFlagForSide(adjacentSide);
        return flags == topRightFlags || flags == bottomLeftFlags;
    }
    return false;
}

inline bool colorsMatchAtCorner(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[])
{
    if (edges[side].shouldRender() != edges[adjacentSide].shouldRender())
        return false;

    if (!edges[side].sharesColorWith(edges[adjacentSide]))
        return false;

    return !borderStyleHasUnmatchedColorsAtCorner(edges[side].borderStyle(), side, adjacentSide);
}

inline bool colorNeedsAntiAliasAtCorner(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[])
{
    if (!edges[side].color.hasAlpha())
        return false;

    if (edges[side].shouldRender() != edges[adjacentSide].shouldRender())
        return false;

    if (!edges[side].sharesColorWith(edges[adjacentSide]))
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(edges[side].borderStyle(), side, adjacentSide);
}

inline bool borderWillArcInnerEdge(const FloatSize& firstRadius, const FloatSize& secondRadius)
{
    return !firstRadius.isZero() || !secondRadius.isZero();
}

// This assumes that we draw in order: top, bottom, left, right.
inline bool willBeOverdrawn(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[])
{
    switch (side) {
    case BSTop:
    case BSBottom:
        if (edges[adjacentSide].presentButInvisible())
            return false;

        if (!edges[side].sharesColorWith(edges[adjacentSide]) && edges[adjacentSide].color.hasAlpha())
            return false;

        if (!borderStyleFillsBorderArea(edges[adjacentSide].borderStyle()))
            return false;

        return true;

    case BSLeft:
    case BSRight:
        // These draw last, so are never overdrawn.
        return false;
    }
    return false;
}

inline bool borderStylesRequireMitre(BoxSide side, BoxSide adjacentSide, EBorderStyle style, EBorderStyle adjacentStyle)
{
    if (style == DOUBLE || adjacentStyle == DOUBLE || adjacentStyle == GROOVE || adjacentStyle == RIDGE)
        return true;

    if (borderStyleIsDottedOrDashed(style) != borderStyleIsDottedOrDashed(adjacentStyle))
        return true;

    if (style != adjacentStyle)
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(style, side, adjacentSide);
}

inline bool joinRequiresMitre(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[], bool allowOverdraw)
{
    if ((edges[side].isTransparent && edges[adjacentSide].isTransparent) || !edges[adjacentSide].isPresent)
        return false;

    if (allowOverdraw && willBeOverdrawn(side, adjacentSide, edges))
        return false;

    if (!edges[side].sharesColorWith(edges[adjacentSide]))
        return true;

    if (borderStylesRequireMitre(side, adjacentSide, edges[side].borderStyle(), edges[adjacentSide].borderStyle()))
        return true;

    return false;
}

FloatRect calculateSideRect(const FloatRoundedRect& outerBorder, const BorderEdge& edge, int side)
{
    FloatRect sideRect = outerBorder.rect();
    int width = edge.width;

    if (side == BSTop)
        sideRect.setHeight(width);
    else if (side == BSBottom)
        sideRect.shiftYEdgeTo(sideRect.maxY() - width);
    else if (side == BSLeft)
        sideRect.setWidth(width);
    else
        sideRect.shiftXEdgeTo(sideRect.maxX() - width);

    return sideRect;
}

FloatRect calculateSideRectIncludingInner(const FloatRoundedRect& outerBorder, const BorderEdge edges[], BoxSide side)
{
    FloatRect sideRect = outerBorder.rect();
    int width;

    switch (side) {
    case BSTop:
        width = sideRect.height() - edges[BSBottom].width;
        sideRect.setHeight(width);
        break;
    case BSBottom:
        width = sideRect.height() - edges[BSTop].width;
        sideRect.shiftYEdgeTo(sideRect.maxY() - width);
        break;
    case BSLeft:
        width = sideRect.width() - edges[BSRight].width;
        sideRect.setWidth(width);
        break;
    case BSRight:
        width = sideRect.width() - edges[BSLeft].width;
        sideRect.shiftXEdgeTo(sideRect.maxX() - width);
        break;
    }

    return sideRect;
}

FloatRoundedRect calculateAdjustedInnerBorder(const FloatRoundedRect& innerBorder, BoxSide side)
{
    // Expand the inner border as necessary to make it a rounded rect (i.e. radii contained within each edge).
    // This function relies on the fact we only get radii not contained within each edge if one of the radii
    // for an edge is zero, so we can shift the arc towards the zero radius corner.
    FloatRoundedRect::Radii newRadii = innerBorder.radii();
    FloatRect newRect = innerBorder.rect();

    float overshoot;
    float maxRadii;

    switch (side) {
    case BSTop:
        overshoot = newRadii.topLeft().width() + newRadii.topRight().width() - newRect.width();
        // FIXME: once we start pixel-snapping rounded rects after this point, the overshoot concept
        // should disappear.
        if (overshoot > 0.1) {
            newRect.setWidth(newRect.width() + overshoot);
            if (!newRadii.topLeft().width())
                newRect.move(-overshoot, 0);
        }
        newRadii.setBottomLeft(IntSize(0, 0));
        newRadii.setBottomRight(IntSize(0, 0));
        maxRadii = std::max(newRadii.topLeft().height(), newRadii.topRight().height());
        if (maxRadii > newRect.height())
            newRect.setHeight(maxRadii);
        break;

    case BSBottom:
        overshoot = newRadii.bottomLeft().width() + newRadii.bottomRight().width() - newRect.width();
        if (overshoot > 0.1) {
            newRect.setWidth(newRect.width() + overshoot);
            if (!newRadii.bottomLeft().width())
                newRect.move(-overshoot, 0);
        }
        newRadii.setTopLeft(IntSize(0, 0));
        newRadii.setTopRight(IntSize(0, 0));
        maxRadii = std::max(newRadii.bottomLeft().height(), newRadii.bottomRight().height());
        if (maxRadii > newRect.height()) {
            newRect.move(0, newRect.height() - maxRadii);
            newRect.setHeight(maxRadii);
        }
        break;

    case BSLeft:
        overshoot = newRadii.topLeft().height() + newRadii.bottomLeft().height() - newRect.height();
        if (overshoot > 0.1) {
            newRect.setHeight(newRect.height() + overshoot);
            if (!newRadii.topLeft().height())
                newRect.move(0, -overshoot);
        }
        newRadii.setTopRight(IntSize(0, 0));
        newRadii.setBottomRight(IntSize(0, 0));
        maxRadii = std::max(newRadii.topLeft().width(), newRadii.bottomLeft().width());
        if (maxRadii > newRect.width())
            newRect.setWidth(maxRadii);
        break;

    case BSRight:
        overshoot = newRadii.topRight().height() + newRadii.bottomRight().height() - newRect.height();
        if (overshoot > 0.1) {
            newRect.setHeight(newRect.height() + overshoot);
            if (!newRadii.topRight().height())
                newRect.move(0, -overshoot);
        }
        newRadii.setTopLeft(IntSize(0, 0));
        newRadii.setBottomLeft(IntSize(0, 0));
        maxRadii = std::max(newRadii.topRight().width(), newRadii.bottomRight().width());
        if (maxRadii > newRect.width()) {
            newRect.move(newRect.width() - maxRadii, 0);
            newRect.setWidth(maxRadii);
        }
        break;
    }

    return FloatRoundedRect(newRect, newRadii);
}

struct BoxBorderInfo {
    STACK_ALLOCATED();
public:
    BoxBorderInfo(const ComputedStyle& style, BackgroundBleedAvoidance bleedAvoidance,
        bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
        : style(style)
        , bleedAvoidance(bleedAvoidance)
        , includeLogicalLeftEdge(includeLogicalLeftEdge)
        , includeLogicalRightEdge(includeLogicalRightEdge)
        , visibleEdgeCount(0)
        , firstVisibleEdge(0)
        , visibleEdgeSet(0)
        , isUniformStyle(true)
        , isUniformWidth(true)
        , isUniformColor(true)
        , hasAlpha(false)
    {

        style.getBorderEdgeInfo(edges, includeLogicalLeftEdge, includeLogicalRightEdge);

        for (unsigned i = 0; i < WTF_ARRAY_LENGTH(edges); ++i) {
            const BorderEdge& edge = edges[i];

            if (!edge.shouldRender()) {
                if (edge.presentButInvisible()) {
                    isUniformWidth = false;
                    isUniformColor = false;
                }

                continue;
            }

            visibleEdgeCount++;
            visibleEdgeSet |= edgeFlagForSide(static_cast<BoxSide>(i));

            hasAlpha = hasAlpha || edge.color.hasAlpha();

            if (visibleEdgeCount == 1) {
                firstVisibleEdge = i;
                continue;
            }

            isUniformStyle = isUniformStyle && (edge.borderStyle() == edges[firstVisibleEdge].borderStyle());
            isUniformWidth = isUniformWidth && (edge.width == edges[firstVisibleEdge].width);
            isUniformColor = isUniformColor && (edge.color == edges[firstVisibleEdge].color);
        }
    }

    const ComputedStyle& style;
    const BackgroundBleedAvoidance bleedAvoidance;
    const bool includeLogicalLeftEdge;
    const bool includeLogicalRightEdge;

    BorderEdge edges[4];

    unsigned visibleEdgeCount;
    unsigned firstVisibleEdge;
    BorderEdgeFlags visibleEdgeSet;

    bool isUniformStyle;
    bool isUniformWidth;
    bool isUniformColor;
    bool hasAlpha;
};

LayoutRectOutsets doubleStripeInsets(const BorderEdge edges[], BorderEdge::DoubleBorderStripe stripe)
{
    // Insets are representes as negative outsets.
    return LayoutRectOutsets(
        -edges[BSTop].getDoubleBorderStripeWidth(stripe),
        -edges[BSRight].getDoubleBorderStripeWidth(stripe),
        -edges[BSBottom].getDoubleBorderStripeWidth(stripe),
        -edges[BSLeft].getDoubleBorderStripeWidth(stripe));
}

void drawSolidBorderRect(GraphicsContext* context, const FloatRect& borderRect,
    float borderWidth, const Color& color)
{
    FloatRect strokeRect(borderRect);
    strokeRect.inflate(-borderWidth / 2);

    bool antialias = BoxPainter::shouldAntialiasLines(context);
    bool wasAntialias = context->shouldAntialias();
    if (antialias != wasAntialias)
        context->setShouldAntialias(antialias);

    context->setStrokeStyle(SolidStroke);
    context->setStrokeColor(color);
    context->strokeRect(strokeRect, borderWidth);

    if (antialias != wasAntialias)
        context->setShouldAntialias(wasAntialias);
}

void drawBleedAdjustedDRRect(GraphicsContext* context, BackgroundBleedAvoidance bleedAvoidance,
    const FloatRoundedRect& outer, const FloatRoundedRect& inner, Color color)
{
    switch (bleedAvoidance) {
    case BackgroundBleedBackgroundOverBorder:
        // BackgroundBleedBackgroundOverBorder draws an opaque background over the inner rrect,
        // so we can simply fill the outer rect here to avoid backdrop bleeding.
        context->fillRoundedRect(outer, color);
        break;
    case BackgroundBleedClipLayer: {
        // BackgroundBleedClipLayer clips the outer rrect for the whole layer. Based on this,
        // we can avoid background bleeding by filling the *outside* of inner rrect, all the
        // way to the layer bounds (enclosing int rect for the clip, in device space).
        ASSERT(outer.isRounded());

        SkPath path;
        path.addRRect(inner);
        path.setFillType(SkPath::kInverseWinding_FillType);

        SkPaint paint;
        paint.setColor(color.rgb());
        paint.setStyle(SkPaint::kFill_Style);
        paint.setAntiAlias(true);
        context->drawPath(path, paint);

        break;
    }
    case BackgroundBleedClipOnly:
        if (outer.isRounded()) {
            // BackgroundBleedClipOnly clips the outer rrect corners for us.
            FloatRoundedRect adjustedOuter = outer;
            adjustedOuter.setRadii(FloatRoundedRect::Radii());
            context->fillDRRect(adjustedOuter, inner, color);
            break;
        }
        // fall through
    default:
        context->fillDRRect(outer, inner, color);
        break;
    }
}

void drawDoubleBorder(GraphicsContext* context, const BoxBorderInfo& borderInfo, const LayoutRect& borderRect,
    const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder)
{
    ASSERT(borderInfo.isUniformColor);
    ASSERT(borderInfo.isUniformStyle);
    ASSERT(borderInfo.edges[borderInfo.firstVisibleEdge].borderStyle() == DOUBLE);
    ASSERT(borderInfo.visibleEdgeSet == AllBorderEdges);

    const Color color = borderInfo.edges[borderInfo.firstVisibleEdge].color;

    // outer stripe
    const LayoutRectOutsets outerThirdInsets =
        doubleStripeInsets(borderInfo.edges, BorderEdge::DoubleBorderStripeOuter);
    const FloatRoundedRect outerThirdRect = borderInfo.style.getRoundedInnerBorderFor(borderRect,
        outerThirdInsets, borderInfo.includeLogicalLeftEdge, borderInfo.includeLogicalRightEdge);
    drawBleedAdjustedDRRect(context, borderInfo.bleedAvoidance, outerBorder, outerThirdRect, color);

    // inner stripe
    const LayoutRectOutsets innerThirdInsets =
        doubleStripeInsets(borderInfo.edges, BorderEdge::DoubleBorderStripeInner);
    const FloatRoundedRect innerThirdRect = borderInfo.style.getRoundedInnerBorderFor(borderRect,
        innerThirdInsets, borderInfo.includeLogicalLeftEdge, borderInfo.includeLogicalRightEdge);
    context->fillDRRect(innerThirdRect, innerBorder, color);
}

bool paintBorderFastPath(GraphicsContext* context, const BoxBorderInfo& info,
    const LayoutRect& borderRect, const FloatRoundedRect& outer, const FloatRoundedRect& inner)
{
    if (!info.isUniformColor || !info.isUniformStyle || !inner.isRenderable())
        return false;

    const BorderEdge& firstEdge = info.edges[info.firstVisibleEdge];
    if (firstEdge.borderStyle() != SOLID && firstEdge.borderStyle() != DOUBLE)
        return false;

    if (info.visibleEdgeSet == AllBorderEdges) {
        if (firstEdge.borderStyle() == SOLID) {
            if (info.isUniformWidth && !outer.isRounded()) {
                // 4-side, solid, uniform-width, rectangular border => one drawRect()
                drawSolidBorderRect(context, outer.rect(), firstEdge.width, firstEdge.color);
            } else {
                // 4-side, solid border => one drawDRRect()
                drawBleedAdjustedDRRect(context, info.bleedAvoidance, outer, inner, firstEdge.color);
            }
        } else {
            // 4-side, double border => 2x drawDRRect()
            ASSERT(firstEdge.borderStyle() == DOUBLE);
            drawDoubleBorder(context, info, borderRect, outer, inner);
        }

        return true;
    }

    // This is faster than the normal complex border path only if it avoids creating transparency
    // layers (when the border is translucent).
    if (firstEdge.borderStyle() == SOLID && !outer.isRounded() && info.hasAlpha) {
        ASSERT(info.visibleEdgeSet != AllBorderEdges);
        // solid, rectangular border => one drawPath()
        Path path;

        for (int i = BSTop; i <= BSLeft; ++i) {
            const BorderEdge& currEdge = info.edges[i];
            if (currEdge.shouldRender())
                path.addRect(calculateSideRect(outer, currEdge, i));
        }

        context->setFillRule(RULE_NONZERO);
        context->setFillColor(firstEdge.color);
        context->fillPath(path);

        return true;
    }

    return false;
}

bool bleedAvoidanceIsClipping(BackgroundBleedAvoidance bleedAvoidance)
{
    return bleedAvoidance == BackgroundBleedClipOnly || bleedAvoidance == BackgroundBleedClipLayer;
}

} // anonymous namespace

void BoxBorderPainter::paintBorder(LayoutBoxModelObject& obj, const PaintInfo& info,
    const LayoutRect& rect, const ComputedStyle& style, BackgroundBleedAvoidance bleedAvoidance,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    GraphicsContext* graphicsContext = info.context;
    // border-image is not affected by border-radius.
    if (BoxPainter::paintNinePieceImage(obj, graphicsContext, rect, style, style.borderImage()))
        return;

    const BoxBorderInfo borderInfo(style, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
    FloatRoundedRect outerBorder = style.getRoundedBorderFor(rect, includeLogicalLeftEdge, includeLogicalRightEdge);
    FloatRoundedRect innerBorder = style.getRoundedInnerBorderFor(rect, includeLogicalLeftEdge, includeLogicalRightEdge);

    if (outerBorder.rect().isEmpty() || !borderInfo.visibleEdgeCount)
        return;

    const BorderEdge& firstEdge = borderInfo.edges[borderInfo.firstVisibleEdge];
    bool haveAllSolidEdges = borderInfo.isUniformStyle && firstEdge.borderStyle() == SOLID;

    // If no corner intersects the clip region, we can pretend outerBorder is
    // rectangular to improve performance.
    if (haveAllSolidEdges && outerBorder.isRounded() && BoxPainter::allCornersClippedOut(outerBorder, info.rect))
        outerBorder.setRadii(FloatRoundedRect::Radii());

    if (paintBorderFastPath(graphicsContext, borderInfo, rect, outerBorder, innerBorder))
        return;

    bool clipToOuterBorder = outerBorder.isRounded();
    GraphicsContextStateSaver stateSaver(*graphicsContext, clipToOuterBorder);
    if (clipToOuterBorder) {
        // For BackgroundBleedClip{Only,Layer}, the outer rrect clip is already applied.
        if (!bleedAvoidanceIsClipping(bleedAvoidance))
            graphicsContext->clipRoundedRect(outerBorder);

        // For BackgroundBleedBackgroundOverBorder, we're going to draw an opaque background over
        // the inner rrect - so clipping is not needed (nor desirable due to backdrop bleeding).
        if (bleedAvoidance != BackgroundBleedBackgroundOverBorder && innerBorder.isRenderable() && !innerBorder.isEmpty())
            graphicsContext->clipOutRoundedRect(innerBorder);
    }

    // If only one edge visible antialiasing doesn't create seams
    bool antialias = BoxPainter::shouldAntialiasLines(graphicsContext) || borderInfo.visibleEdgeCount == 1;
    if (borderInfo.hasAlpha) {
        paintTranslucentBorderSides(graphicsContext, style, outerBorder, innerBorder, borderInfo.edges,
        borderInfo.visibleEdgeSet, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias);
    } else {
        paintBorderSides(graphicsContext, style, outerBorder, innerBorder, borderInfo.edges,
        borderInfo.visibleEdgeSet, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias);
    }
}

void BoxBorderPainter::paintTranslucentBorderSides(GraphicsContext* graphicsContext,
    const ComputedStyle& style, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder,
    const BorderEdge edges[], BorderEdgeFlags edgesToDraw, BackgroundBleedAvoidance bleedAvoidance,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias)
{
    // willBeOverdrawn assumes that we draw in order: top, bottom, left, right.
    // This is different from BoxSide enum order.
    static const BoxSide paintOrder[] = { BSTop, BSBottom, BSLeft, BSRight };

    while (edgesToDraw) {
        // Find undrawn edges sharing a color.
        Color commonColor;

        BorderEdgeFlags commonColorEdgeSet = 0;
        for (size_t i = 0; i < sizeof(paintOrder) / sizeof(paintOrder[0]); ++i) {
            BoxSide currSide = paintOrder[i];
            if (!includesEdge(edgesToDraw, currSide))
                continue;

            bool includeEdge;
            if (!commonColorEdgeSet) {
                commonColor = edges[currSide].color;
                includeEdge = true;
            } else {
                includeEdge = edges[currSide].color == commonColor;
            }

            if (includeEdge)
                commonColorEdgeSet |= edgeFlagForSide(currSide);
        }

        bool useTransparencyLayer = includesAdjacentEdges(commonColorEdgeSet) && commonColor.hasAlpha();
        if (useTransparencyLayer) {
            graphicsContext->beginLayer(static_cast<float>(commonColor.alpha()) / 255);
            commonColor = Color(commonColor.red(), commonColor.green(), commonColor.blue());
        }

        paintBorderSides(graphicsContext, style, outerBorder, innerBorder, edges, commonColorEdgeSet,
            bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, &commonColor);

        if (useTransparencyLayer)
            graphicsContext->endLayer();

        edgesToDraw &= ~commonColorEdgeSet;
    }
}

void BoxBorderPainter::paintOneBorderSide(GraphicsContext* graphicsContext, const ComputedStyle& style,
    const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder, const FloatRect& sideRect,
    BoxSide side, BoxSide adjacentSide1, BoxSide adjacentSide2, const BorderEdge edges[], const Path* path,
    BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge,
    bool antialias, const Color* overrideColor)
{
    const BorderEdge& edgeToRender = edges[side];
    ASSERT(edgeToRender.width);
    const BorderEdge& adjacentEdge1 = edges[adjacentSide1];
    const BorderEdge& adjacentEdge2 = edges[adjacentSide2];

    bool mitreAdjacentSide1 = joinRequiresMitre(side, adjacentSide1, edges, !antialias);
    bool mitreAdjacentSide2 = joinRequiresMitre(side, adjacentSide2, edges, !antialias);

    bool adjacentSide1StylesMatch = colorsMatchAtCorner(side, adjacentSide1, edges);
    bool adjacentSide2StylesMatch = colorsMatchAtCorner(side, adjacentSide2, edges);

    const Color& colorToPaint = overrideColor ? *overrideColor : edgeToRender.color;

    if (path) {
        GraphicsContextStateSaver stateSaver(*graphicsContext);
        if (innerBorder.isRenderable())
            clipBorderSidePolygon(graphicsContext, outerBorder, innerBorder, side, adjacentSide1StylesMatch, adjacentSide2StylesMatch);
        else
            clipBorderSideForComplexInnerPath(graphicsContext, outerBorder, innerBorder, side, edges);
        float thickness = std::max(std::max(edgeToRender.width, adjacentEdge1.width), adjacentEdge2.width);
        drawBoxSideFromPath(graphicsContext, LayoutRect(outerBorder.rect()), *path, edges, edgeToRender.width, thickness, side, style,
            colorToPaint, edgeToRender.borderStyle(), bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
    } else {
        bool clipForStyle = styleRequiresClipPolygon(edgeToRender.borderStyle()) && (mitreAdjacentSide1 || mitreAdjacentSide2);
        bool clipAdjacentSide1 = colorNeedsAntiAliasAtCorner(side, adjacentSide1, edges) && mitreAdjacentSide1;
        bool clipAdjacentSide2 = colorNeedsAntiAliasAtCorner(side, adjacentSide2, edges) && mitreAdjacentSide2;
        bool shouldClip = clipForStyle || clipAdjacentSide1 || clipAdjacentSide2;

        GraphicsContextStateSaver clipStateSaver(*graphicsContext, shouldClip);
        if (shouldClip) {
            bool aliasAdjacentSide1 = clipAdjacentSide1 || (clipForStyle && mitreAdjacentSide1);
            bool aliasAdjacentSide2 = clipAdjacentSide2 || (clipForStyle && mitreAdjacentSide2);
            clipBorderSidePolygon(graphicsContext, outerBorder, innerBorder, side, !aliasAdjacentSide1, !aliasAdjacentSide2);
            // Since we clipped, no need to draw with a mitre.
            mitreAdjacentSide1 = false;
            mitreAdjacentSide2 = false;
        }

        ObjectPainter::drawLineForBoxSide(graphicsContext, sideRect.x(), sideRect.y(), sideRect.maxX(), sideRect.maxY(), side, colorToPaint, edgeToRender.borderStyle(),
            mitreAdjacentSide1 ? adjacentEdge1.width : 0, mitreAdjacentSide2 ? adjacentEdge2.width : 0, antialias);
    }
}

void BoxBorderPainter::paintBorderSides(GraphicsContext* graphicsContext, const ComputedStyle& style,
    const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder, const BorderEdge edges[],
    BorderEdgeFlags edgeSet, BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge,
    bool includeLogicalRightEdge, bool antialias, const Color* overrideColor)
{
    bool renderRadii = outerBorder.isRounded();

    Path roundedPath;
    if (renderRadii)
        roundedPath.addRoundedRect(outerBorder);

    // The inner border adjustment for bleed avoidance mode BackgroundBleedBackgroundOverBorder
    // is only applied to sideRect, which is okay since BackgroundBleedBackgroundOverBorder
    // is only to be used for solid borders and the shape of the border painted by drawBoxSideFromPath
    // only depends on sideRect when painting solid borders.

    if (edges[BSTop].shouldRender() && includesEdge(edgeSet, BSTop)) {
        FloatRect sideRect = outerBorder.rect();
        sideRect.setHeight(edges[BSTop].width);

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSTop].borderStyle()) || borderWillArcInnerEdge(innerBorder.radii().topLeft(), innerBorder.radii().topRight()));
        paintOneBorderSide(graphicsContext, style, outerBorder, innerBorder, sideRect, BSTop, BSLeft, BSRight, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }

    if (edges[BSBottom].shouldRender() && includesEdge(edgeSet, BSBottom)) {
        FloatRect sideRect = outerBorder.rect();
        sideRect.shiftYEdgeTo(sideRect.maxY() - edges[BSBottom].width);

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSBottom].borderStyle()) || borderWillArcInnerEdge(innerBorder.radii().bottomLeft(), innerBorder.radii().bottomRight()));
        paintOneBorderSide(graphicsContext, style, outerBorder, innerBorder, sideRect, BSBottom, BSLeft, BSRight, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }

    if (edges[BSLeft].shouldRender() && includesEdge(edgeSet, BSLeft)) {
        FloatRect sideRect = outerBorder.rect();
        sideRect.setWidth(edges[BSLeft].width);

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSLeft].borderStyle()) || borderWillArcInnerEdge(innerBorder.radii().bottomLeft(), innerBorder.radii().topLeft()));
        paintOneBorderSide(graphicsContext, style, outerBorder, innerBorder, sideRect, BSLeft, BSTop, BSBottom, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }

    if (edges[BSRight].shouldRender() && includesEdge(edgeSet, BSRight)) {
        FloatRect sideRect = outerBorder.rect();
        sideRect.shiftXEdgeTo(sideRect.maxX() - edges[BSRight].width);

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSRight].borderStyle()) || borderWillArcInnerEdge(innerBorder.radii().bottomRight(), innerBorder.radii().topRight()));
        paintOneBorderSide(graphicsContext, style, outerBorder, innerBorder, sideRect, BSRight, BSTop, BSBottom, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }
}

void BoxBorderPainter::drawBoxSideFromPath(GraphicsContext* graphicsContext,
    const LayoutRect& borderRect, const Path& borderPath, const BorderEdge edges[], float thickness,
    float drawThickness, BoxSide side, const ComputedStyle& style, Color color, EBorderStyle borderStyle,
    BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    if (thickness <= 0)
        return;

    if (borderStyle == DOUBLE && thickness < 3)
        borderStyle = SOLID;

    switch (borderStyle) {
    case BNONE:
    case BHIDDEN:
        return;
    case DOTTED:
    case DASHED: {
        graphicsContext->setStrokeColor(color);

        // The stroke is doubled here because the provided path is the
        // outside edge of the border so half the stroke is clipped off.
        // The extra multiplier is so that the clipping mask can antialias
        // the edges to prevent jaggies.
        graphicsContext->setStrokeThickness(drawThickness * 2 * 1.1f);
        graphicsContext->setStrokeStyle(borderStyle == DASHED ? DashedStroke : DottedStroke);

        // If the number of dashes that fit in the path is odd and non-integral then we
        // will have an awkwardly-sized dash at the end of the path. To try to avoid that
        // here, we simply make the whitespace dashes ever so slightly bigger.
        // FIXME: This could be even better if we tried to manipulate the dash offset
        // and possibly the gapLength to get the corners dash-symmetrical.
        float dashLength = thickness * ((borderStyle == DASHED) ? 3.0f : 1.0f);
        float gapLength = dashLength;
        float numberOfDashes = borderPath.length() / dashLength;
        // Don't try to show dashes if we have less than 2 dashes + 2 gaps.
        // FIXME: should do this test per side.
        if (numberOfDashes >= 4) {
            bool evenNumberOfFullDashes = !((int)numberOfDashes % 2);
            bool integralNumberOfDashes = !(numberOfDashes - (int)numberOfDashes);
            if (!evenNumberOfFullDashes && !integralNumberOfDashes) {
                float numberOfGaps = numberOfDashes / 2;
                gapLength += (dashLength  / numberOfGaps);
            }

            DashArray lineDash;
            lineDash.append(dashLength);
            lineDash.append(gapLength);
            graphicsContext->setLineDash(lineDash, dashLength);
        }

        // FIXME: stroking the border path causes issues with tight corners:
        // https://bugs.webkit.org/show_bug.cgi?id=58711
        // Also, to get the best appearance we should stroke a path between the two borders.
        graphicsContext->strokePath(borderPath);
        return;
    }
    case DOUBLE: {
        // Draw inner border line
        {
            GraphicsContextStateSaver stateSaver(*graphicsContext);
            const LayoutRectOutsets innerInsets = doubleStripeInsets(edges, BorderEdge::DoubleBorderStripeInner);
            FloatRoundedRect innerClip = style.getRoundedInnerBorderFor(borderRect,
                innerInsets, includeLogicalLeftEdge, includeLogicalRightEdge);

            graphicsContext->clipRoundedRect(innerClip);
            drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, SOLID, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        }

        // Draw outer border line
        {
            GraphicsContextStateSaver stateSaver(*graphicsContext);
            LayoutRect outerRect = borderRect;
            LayoutRectOutsets outerInsets = doubleStripeInsets(edges, BorderEdge::DoubleBorderStripeOuter);

            if (bleedAvoidanceIsClipping(bleedAvoidance)) {
                outerRect.inflate(1);
                outerInsets.setTop(outerInsets.top() - 1);
                outerInsets.setRight(outerInsets.right() - 1);
                outerInsets.setBottom(outerInsets.bottom() - 1);
                outerInsets.setLeft(outerInsets.left() - 1);
            }

            FloatRoundedRect outerClip = style.getRoundedInnerBorderFor(outerRect, outerInsets,
                includeLogicalLeftEdge, includeLogicalRightEdge);
            graphicsContext->clipOutRoundedRect(outerClip);
            drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, SOLID, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        }
        return;
    }
    case RIDGE:
    case GROOVE:
    {
        EBorderStyle s1;
        EBorderStyle s2;
        if (borderStyle == GROOVE) {
            s1 = INSET;
            s2 = OUTSET;
        } else {
            s1 = OUTSET;
            s2 = INSET;
        }

        // Paint full border
        drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, s1, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);

        // Paint inner only
        GraphicsContextStateSaver stateSaver(*graphicsContext);
        LayoutUnit topWidth = edges[BSTop].usedWidth() / 2;
        LayoutUnit bottomWidth = edges[BSBottom].usedWidth() / 2;
        LayoutUnit leftWidth = edges[BSLeft].usedWidth() / 2;
        LayoutUnit rightWidth = edges[BSRight].usedWidth() / 2;

        FloatRoundedRect clipRect = style.getRoundedInnerBorderFor(borderRect,
            LayoutRectOutsets(-topWidth, -rightWidth, -bottomWidth, -leftWidth),
            includeLogicalLeftEdge, includeLogicalRightEdge);

        graphicsContext->clipRoundedRect(clipRect);
        drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, s2, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        return;
    }
    case INSET:
        if (side == BSTop || side == BSLeft)
            color = color.dark();
        break;
    case OUTSET:
        if (side == BSBottom || side == BSRight)
            color = color.dark();
        break;
    default:
        break;
    }

    graphicsContext->setStrokeStyle(NoStroke);
    graphicsContext->setFillColor(color);
    graphicsContext->drawRect(pixelSnappedIntRect(borderRect));
}

void BoxBorderPainter::clipBorderSideForComplexInnerPath(GraphicsContext* graphicsContext,
    const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder,
    BoxSide side, const BorderEdge edges[])
{
    graphicsContext->clip(calculateSideRectIncludingInner(outerBorder, edges, side));
    FloatRoundedRect adjustedInnerRect = calculateAdjustedInnerBorder(innerBorder, side);
    if (!adjustedInnerRect.isEmpty())
        graphicsContext->clipOutRoundedRect(adjustedInnerRect);
}

void BoxBorderPainter::clipBorderSidePolygon(GraphicsContext* graphicsContext,
    const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder, BoxSide side,
    bool firstEdgeMatches, bool secondEdgeMatches)
{
    FloatPoint quad[4];

    const LayoutRect outerRect(outerBorder.rect());
    const LayoutRect innerRect(innerBorder.rect());

    FloatPoint centerPoint(innerRect.location().x().toFloat() + innerRect.width().toFloat() / 2, innerRect.location().y().toFloat() + innerRect.height().toFloat() / 2);

    // For each side, create a quad that encompasses all parts of that side that may draw,
    // including areas inside the innerBorder.
    //
    //         0----------------3
    //       0  \              /  0
    //       |\  1----------- 2  /|
    //       | 1                1 |
    //       | |                | |
    //       | |                | |
    //       | 2                2 |
    //       |/  1------------2  \|
    //       3  /              \  3
    //         0----------------3
    //
    switch (side) {
    case BSTop:
        quad[0] = FloatPoint(outerRect.minXMinYCorner());
        quad[1] = FloatPoint(innerRect.minXMinYCorner());
        quad[2] = FloatPoint(innerRect.maxXMinYCorner());
        quad[3] = FloatPoint(outerRect.maxXMinYCorner());

        if (!innerBorder.radii().topLeft().isZero()) {
            findIntersection(quad[0], quad[1],
                FloatPoint(
                    quad[1].x() + innerBorder.radii().topLeft().width(),
                    quad[1].y()),
                FloatPoint(
                    quad[1].x(),
                    quad[1].y() + innerBorder.radii().topLeft().height()),
                quad[1]);
        }

        if (!innerBorder.radii().topRight().isZero()) {
            findIntersection(quad[3], quad[2],
                FloatPoint(
                    quad[2].x() - innerBorder.radii().topRight().width(),
                    quad[2].y()),
                FloatPoint(
                    quad[2].x(),
                    quad[2].y() + innerBorder.radii().topRight().height()),
                quad[2]);
        }
        break;

    case BSLeft:
        quad[0] = FloatPoint(outerRect.minXMinYCorner());
        quad[1] = FloatPoint(innerRect.minXMinYCorner());
        quad[2] = FloatPoint(innerRect.minXMaxYCorner());
        quad[3] = FloatPoint(outerRect.minXMaxYCorner());

        if (!innerBorder.radii().topLeft().isZero()) {
            findIntersection(quad[0], quad[1],
                FloatPoint(
                    quad[1].x() + innerBorder.radii().topLeft().width(),
                    quad[1].y()),
                FloatPoint(
                    quad[1].x(),
                    quad[1].y() + innerBorder.radii().topLeft().height()),
                quad[1]);
        }

        if (!innerBorder.radii().bottomLeft().isZero()) {
            findIntersection(quad[3], quad[2],
                FloatPoint(
                    quad[2].x() + innerBorder.radii().bottomLeft().width(),
                    quad[2].y()),
                FloatPoint(
                    quad[2].x(),
                    quad[2].y() - innerBorder.radii().bottomLeft().height()),
                quad[2]);
        }
        break;

    case BSBottom:
        quad[0] = FloatPoint(outerRect.minXMaxYCorner());
        quad[1] = FloatPoint(innerRect.minXMaxYCorner());
        quad[2] = FloatPoint(innerRect.maxXMaxYCorner());
        quad[3] = FloatPoint(outerRect.maxXMaxYCorner());

        if (!innerBorder.radii().bottomLeft().isZero()) {
            findIntersection(quad[0], quad[1],
                FloatPoint(
                    quad[1].x() + innerBorder.radii().bottomLeft().width(),
                    quad[1].y()),
                FloatPoint(
                    quad[1].x(),
                    quad[1].y() - innerBorder.radii().bottomLeft().height()),
                quad[1]);
        }

        if (!innerBorder.radii().bottomRight().isZero()) {
            findIntersection(quad[3], quad[2],
                FloatPoint(
                    quad[2].x() - innerBorder.radii().bottomRight().width(),
                    quad[2].y()),
                FloatPoint(
                    quad[2].x(),
                    quad[2].y() - innerBorder.radii().bottomRight().height()),
                quad[2]);
        }
        break;

    case BSRight:
        quad[0] = FloatPoint(outerRect.maxXMinYCorner());
        quad[1] = FloatPoint(innerRect.maxXMinYCorner());
        quad[2] = FloatPoint(innerRect.maxXMaxYCorner());
        quad[3] = FloatPoint(outerRect.maxXMaxYCorner());

        if (!innerBorder.radii().topRight().isZero()) {
            findIntersection(quad[0], quad[1],
                FloatPoint(
                    quad[1].x() - innerBorder.radii().topRight().width(),
                    quad[1].y()),
                FloatPoint(
                    quad[1].x(),
                    quad[1].y() + innerBorder.radii().topRight().height()),
                quad[1]);
        }

        if (!innerBorder.radii().bottomRight().isZero()) {
            findIntersection(quad[3], quad[2],
                FloatPoint(
                    quad[2].x() - innerBorder.radii().bottomRight().width(),
                    quad[2].y()),
                FloatPoint(
                    quad[2].x(),
                    quad[2].y() - innerBorder.radii().bottomRight().height()),
                quad[2]);
        }
        break;
    }

    // If the border matches both of its adjacent sides, don't anti-alias the clip, and
    // if neither side matches, anti-alias the clip.
    if (firstEdgeMatches == secondEdgeMatches) {
        graphicsContext->clipPolygon(4, quad, !firstEdgeMatches);
        return;
    }

    // If antialiasing settings for the first edge and second edge is different,
    // they have to be addressed separately. We do this by breaking the quad into
    // two parallelograms, made by moving quad[1] and quad[2].
    float ax = quad[1].x() - quad[0].x();
    float ay = quad[1].y() - quad[0].y();
    float bx = quad[2].x() - quad[1].x();
    float by = quad[2].y() - quad[1].y();
    float cx = quad[3].x() - quad[2].x();
    float cy = quad[3].y() - quad[2].y();

    const static float kEpsilon = 1e-2f;
    float r1, r2;
    if (fabsf(bx) < kEpsilon && fabsf(by) < kEpsilon) {
        // The quad was actually a triangle.
        r1 = r2 = 1.0f;
    } else {
        // Extend parallelogram a bit to hide calculation error
        const static float kExtendFill = 1e-2f;

        r1 = (-ax * by + ay * bx) / (cx * by - cy * bx) + kExtendFill;
        r2 = (-cx * by + cy * bx) / (ax * by - ay * bx) + kExtendFill;
    }

    FloatPoint firstQuad[4];
    firstQuad[0] = quad[0];
    firstQuad[1] = quad[1];
    firstQuad[2] = FloatPoint(quad[3].x() + r2 * ax, quad[3].y() + r2 * ay);
    firstQuad[3] = quad[3];
    graphicsContext->clipPolygon(4, firstQuad, !firstEdgeMatches);

    FloatPoint secondQuad[4];
    secondQuad[0] = quad[0];
    secondQuad[1] = FloatPoint(quad[0].x() - r1 * cx, quad[0].y() - r1 * cy);
    secondQuad[2] = quad[2];
    secondQuad[3] = quad[3];
    graphicsContext->clipPolygon(4, secondQuad, !secondEdgeMatches);
}

} // namespace blink
