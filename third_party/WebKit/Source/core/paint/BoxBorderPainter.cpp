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

bool bleedAvoidanceIsClipping(BackgroundBleedAvoidance bleedAvoidance)
{
    return bleedAvoidance == BackgroundBleedClipOnly || bleedAvoidance == BackgroundBleedClipLayer;
}

} // anonymous namespace

void BoxBorderPainter::drawDoubleBorder(GraphicsContext* context, const LayoutRect& borderRect) const
{
    ASSERT(m_isUniformColor);
    ASSERT(m_isUniformStyle);
    ASSERT(firstEdge().borderStyle() == DOUBLE);
    ASSERT(m_visibleEdgeSet == AllBorderEdges);

    const Color color = firstEdge().color;

    // outer stripe
    const LayoutRectOutsets outerThirdInsets =
        doubleStripeInsets(m_edges, BorderEdge::DoubleBorderStripeOuter);
    const FloatRoundedRect outerThirdRect = m_style.getRoundedInnerBorderFor(borderRect,
        outerThirdInsets, m_includeLogicalLeftEdge, m_includeLogicalRightEdge);
    drawBleedAdjustedDRRect(context, m_bleedAvoidance, m_outer, outerThirdRect, color);

    // inner stripe
    const LayoutRectOutsets innerThirdInsets =
        doubleStripeInsets(m_edges, BorderEdge::DoubleBorderStripeInner);
    const FloatRoundedRect innerThirdRect = m_style.getRoundedInnerBorderFor(borderRect,
        innerThirdInsets, m_includeLogicalLeftEdge, m_includeLogicalRightEdge);
    context->fillDRRect(innerThirdRect, m_inner, color);
}

bool BoxBorderPainter::paintBorderFastPath(GraphicsContext* context, const LayoutRect& borderRect) const
{
    if (!m_isUniformColor || !m_isUniformStyle || !m_inner.isRenderable())
        return false;

    if (firstEdge().borderStyle() != SOLID && firstEdge().borderStyle() != DOUBLE)
        return false;

    if (m_visibleEdgeSet == AllBorderEdges) {
        if (firstEdge().borderStyle() == SOLID) {
            if (m_isUniformWidth && !m_outer.isRounded()) {
                // 4-side, solid, uniform-width, rectangular border => one drawRect()
                drawSolidBorderRect(context, m_outer.rect(), firstEdge().width, firstEdge().color);
            } else {
                // 4-side, solid border => one drawDRRect()
                drawBleedAdjustedDRRect(context, m_bleedAvoidance, m_outer, m_inner, firstEdge().color);
            }
        } else {
            // 4-side, double border => 2x drawDRRect()
            ASSERT(firstEdge().borderStyle() == DOUBLE);
            drawDoubleBorder(context, borderRect);
        }

        return true;
    }

    // This is faster than the normal complex border path only if it avoids creating transparency
    // layers (when the border is translucent).
    if (firstEdge().borderStyle() == SOLID && !m_outer.isRounded() && m_hasAlpha) {
        ASSERT(m_visibleEdgeSet != AllBorderEdges);
        // solid, rectangular border => one drawPath()
        Path path;

        for (int i = BSTop; i <= BSLeft; ++i) {
            const BorderEdge& currEdge = m_edges[i];
            if (currEdge.shouldRender())
                path.addRect(calculateSideRect(m_outer, currEdge, i));
        }

        context->setFillRule(RULE_NONZERO);
        context->setFillColor(firstEdge().color);
        context->fillPath(path);

        return true;
    }

    return false;
}

BoxBorderPainter::BoxBorderPainter(const LayoutRect& borderRect, const ComputedStyle& style,
    const IntRect& clipRect, BackgroundBleedAvoidance bleedAvoidance,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
    : m_style(style)
    , m_bleedAvoidance(bleedAvoidance)
    , m_includeLogicalLeftEdge(includeLogicalLeftEdge)
    , m_includeLogicalRightEdge(includeLogicalRightEdge)
    , m_visibleEdgeCount(0)
    , m_firstVisibleEdge(0)
    , m_visibleEdgeSet(0)
    , m_isUniformStyle(true)
    , m_isUniformWidth(true)
    , m_isUniformColor(true)
    , m_hasAlpha(false)
{
    style.getBorderEdgeInfo(m_edges, includeLogicalLeftEdge, includeLogicalRightEdge);

    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(m_edges); ++i) {
        const BorderEdge& edge = m_edges[i];

        if (!edge.shouldRender()) {
            if (edge.presentButInvisible()) {
                m_isUniformWidth = false;
                m_isUniformColor = false;
            }

            continue;
        }

        m_visibleEdgeCount++;
        m_visibleEdgeSet |= edgeFlagForSide(static_cast<BoxSide>(i));

        m_hasAlpha |= edge.color.hasAlpha();

        if (m_visibleEdgeCount == 1) {
            m_firstVisibleEdge = i;
            continue;
        }

        m_isUniformStyle &= edge.borderStyle() == m_edges[m_firstVisibleEdge].borderStyle();
        m_isUniformWidth &= edge.width == m_edges[m_firstVisibleEdge].width;
        m_isUniformColor &= edge.color == m_edges[m_firstVisibleEdge].color;
    }

    // No need to compute the rrects if we don't have any borders to draw.
    if (!m_visibleEdgeSet)
        return;

    m_outer = style.getRoundedBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);
    m_inner = style.getRoundedInnerBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);

    // If no corner intersects the clip region, we can pretend the outer border is
    // rectangular to improve performance.
    // FIXME: why is this predicated on uniform style & solid edges?
    if (m_isUniformStyle
        && firstEdge().borderStyle() == SOLID
        && m_outer.isRounded()
        && BoxPainter::allCornersClippedOut(m_outer, clipRect))
        m_outer.setRadii(FloatRoundedRect::Radii());
}

void BoxBorderPainter::paintBorder(const PaintInfo& info, const LayoutRect& rect) const
{
    if (!m_visibleEdgeCount || m_outer.rect().isEmpty())
        return;

    GraphicsContext* graphicsContext = info.context;

    if (paintBorderFastPath(graphicsContext, rect))
        return;

    bool clipToOuterBorder = m_outer.isRounded();
    GraphicsContextStateSaver stateSaver(*graphicsContext, clipToOuterBorder);
    if (clipToOuterBorder) {
        // For BackgroundBleedClip{Only,Layer}, the outer rrect clip is already applied.
        if (!bleedAvoidanceIsClipping(m_bleedAvoidance))
            graphicsContext->clipRoundedRect(m_outer);

        // For BackgroundBleedBackgroundOverBorder, we're going to draw an opaque background over
        // the inner rrect - so clipping is not needed (nor desirable due to backdrop bleeding).
        if (m_bleedAvoidance != BackgroundBleedBackgroundOverBorder && m_inner.isRenderable() && !m_inner.isEmpty())
            graphicsContext->clipOutRoundedRect(m_inner);
    }

    // If only one edge visible antialiasing doesn't create seams
    bool antialias = BoxPainter::shouldAntialiasLines(graphicsContext) || m_visibleEdgeCount == 1;
    if (m_hasAlpha) {
        paintTranslucentBorderSides(graphicsContext, antialias);
    } else {
        paintBorderSides(graphicsContext, m_visibleEdgeSet, antialias);
    }
}

void BoxBorderPainter::paintTranslucentBorderSides(GraphicsContext* graphicsContext,
    bool antialias) const
{
    // willBeOverdrawn assumes that we draw in order: top, bottom, left, right.
    // This is different from BoxSide enum order.
    static const BoxSide paintOrder[] = { BSTop, BSBottom, BSLeft, BSRight };

    BorderEdgeFlags edgesToDraw = m_visibleEdgeSet;
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
                commonColor = m_edges[currSide].color;
                includeEdge = true;
            } else {
                includeEdge = m_edges[currSide].color == commonColor;
            }

            if (includeEdge)
                commonColorEdgeSet |= edgeFlagForSide(currSide);
        }

        bool useTransparencyLayer = includesAdjacentEdges(commonColorEdgeSet) && commonColor.hasAlpha();
        if (useTransparencyLayer) {
            graphicsContext->beginLayer(static_cast<float>(commonColor.alpha()) / 255);
            commonColor = Color(commonColor.red(), commonColor.green(), commonColor.blue());
        }

        paintBorderSides(graphicsContext, commonColorEdgeSet, antialias, &commonColor);

        if (useTransparencyLayer)
            graphicsContext->endLayer();

        edgesToDraw &= ~commonColorEdgeSet;
    }
}

void BoxBorderPainter::paintOneBorderSide(GraphicsContext* graphicsContext,
    const FloatRect& sideRect, BoxSide side, BoxSide adjacentSide1, BoxSide adjacentSide2,
    const Path* path, bool antialias, const Color* overrideColor) const
{
    const BorderEdge& edgeToRender = m_edges[side];
    ASSERT(edgeToRender.width);
    const BorderEdge& adjacentEdge1 = m_edges[adjacentSide1];
    const BorderEdge& adjacentEdge2 = m_edges[adjacentSide2];

    const Color& colorToPaint = overrideColor ? *overrideColor : edgeToRender.color;

    if (path) {
        bool adjacentSide1StylesMatch = colorsMatchAtCorner(side, adjacentSide1, m_edges);
        bool adjacentSide2StylesMatch = colorsMatchAtCorner(side, adjacentSide2, m_edges);

        GraphicsContextStateSaver stateSaver(*graphicsContext);
        if (m_inner.isRenderable())
            clipBorderSidePolygon(graphicsContext, side, adjacentSide1StylesMatch, adjacentSide2StylesMatch);
        else
            clipBorderSideForComplexInnerPath(graphicsContext, side);
        float thickness = std::max(std::max(edgeToRender.width, adjacentEdge1.width), adjacentEdge2.width);
        drawBoxSideFromPath(graphicsContext, LayoutRect(m_outer.rect()), *path, edgeToRender.width,
            thickness, side, colorToPaint, edgeToRender.borderStyle());
    } else {
        bool mitreAdjacentSide1 = joinRequiresMitre(side, adjacentSide1, m_edges, !antialias);
        bool mitreAdjacentSide2 = joinRequiresMitre(side, adjacentSide2, m_edges, !antialias);

        bool clipForStyle = styleRequiresClipPolygon(edgeToRender.borderStyle())
            && (mitreAdjacentSide1 || mitreAdjacentSide2);
        bool clipAdjacentSide1 = colorNeedsAntiAliasAtCorner(side, adjacentSide1, m_edges) && mitreAdjacentSide1;
        bool clipAdjacentSide2 = colorNeedsAntiAliasAtCorner(side, adjacentSide2, m_edges) && mitreAdjacentSide2;
        bool shouldClip = clipForStyle || clipAdjacentSide1 || clipAdjacentSide2;

        GraphicsContextStateSaver clipStateSaver(*graphicsContext, shouldClip);
        if (shouldClip) {
            bool aliasAdjacentSide1 = clipAdjacentSide1 || (clipForStyle && mitreAdjacentSide1);
            bool aliasAdjacentSide2 = clipAdjacentSide2 || (clipForStyle && mitreAdjacentSide2);
            clipBorderSidePolygon(graphicsContext, side, !aliasAdjacentSide1, !aliasAdjacentSide2);
            // Since we clipped, no need to draw with a mitre.
            mitreAdjacentSide1 = false;
            mitreAdjacentSide2 = false;
        }

        ObjectPainter::drawLineForBoxSide(graphicsContext, sideRect.x(), sideRect.y(),
            sideRect.maxX(), sideRect.maxY(), side, colorToPaint, edgeToRender.borderStyle(),
            mitreAdjacentSide1 ? adjacentEdge1.width : 0, mitreAdjacentSide2 ? adjacentEdge2.width : 0, antialias);
    }
}

void BoxBorderPainter::paintBorderSides(GraphicsContext* graphicsContext, BorderEdgeFlags edgeSet,
    bool antialias, const Color* overrideColor) const
{
    bool renderRadii = m_outer.isRounded();

    Path roundedPath;
    if (renderRadii)
        roundedPath.addRoundedRect(m_outer);

    // The inner border adjustment for bleed avoidance mode BackgroundBleedBackgroundOverBorder
    // is only applied to sideRect, which is okay since BackgroundBleedBackgroundOverBorder
    // is only to be used for solid borders and the shape of the border painted by drawBoxSideFromPath
    // only depends on sideRect when painting solid borders.

    if (includesEdge(edgeSet, BSTop)) {
        const BorderEdge& edge = m_edges[BSTop];
        ASSERT(edge.shouldRender());

        FloatRect sideRect = m_outer.rect();
        sideRect.setHeight(edge.width);

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edge.borderStyle())
            || borderWillArcInnerEdge(m_inner.radii().topLeft(), m_inner.radii().topRight()));
        paintOneBorderSide(graphicsContext, sideRect, BSTop, BSLeft, BSRight,
            usePath ? &roundedPath : 0, antialias, overrideColor);
    }

    if (includesEdge(edgeSet, BSBottom)) {
        const BorderEdge& edge = m_edges[BSBottom];
        ASSERT(edge.shouldRender());

        FloatRect sideRect = m_outer.rect();
        sideRect.shiftYEdgeTo(sideRect.maxY() - edge.width);

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edge.borderStyle())
            || borderWillArcInnerEdge(m_inner.radii().bottomLeft(), m_inner.radii().bottomRight()));
        paintOneBorderSide(graphicsContext, sideRect, BSBottom, BSLeft, BSRight,
            usePath ? &roundedPath : 0, antialias, overrideColor);
    }

    if (includesEdge(edgeSet, BSLeft)) {
        const BorderEdge& edge = m_edges[BSLeft];
        ASSERT(edge.shouldRender());

        FloatRect sideRect = m_outer.rect();
        sideRect.setWidth(edge.width);

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edge.borderStyle())
            || borderWillArcInnerEdge(m_inner.radii().bottomLeft(), m_inner.radii().topLeft()));
        paintOneBorderSide(graphicsContext, sideRect, BSLeft, BSTop, BSBottom,
            usePath ? &roundedPath : 0, antialias, overrideColor);
    }

    if (includesEdge(edgeSet, BSRight)) {
        const BorderEdge& edge = m_edges[BSRight];
        ASSERT(edge.shouldRender());

        FloatRect sideRect = m_outer.rect();
        sideRect.shiftXEdgeTo(sideRect.maxX() - edge.width);

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edge.borderStyle())
            || borderWillArcInnerEdge(m_inner.radii().bottomRight(), m_inner.radii().topRight()));
        paintOneBorderSide(graphicsContext, sideRect, BSRight, BSTop, BSBottom,
            usePath ? &roundedPath : 0, antialias, overrideColor);
    }
}

void BoxBorderPainter::drawBoxSideFromPath(GraphicsContext* graphicsContext,
    const LayoutRect& borderRect, const Path& borderPath, float thickness, float drawThickness,
    BoxSide side, Color color, EBorderStyle borderStyle) const
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
            const LayoutRectOutsets innerInsets = doubleStripeInsets(m_edges, BorderEdge::DoubleBorderStripeInner);
            FloatRoundedRect innerClip = m_style.getRoundedInnerBorderFor(borderRect, innerInsets,
                m_includeLogicalLeftEdge, m_includeLogicalRightEdge);

            graphicsContext->clipRoundedRect(innerClip);
            drawBoxSideFromPath(graphicsContext, borderRect, borderPath, thickness, drawThickness,
                side, color, SOLID);
        }

        // Draw outer border line
        {
            GraphicsContextStateSaver stateSaver(*graphicsContext);
            LayoutRect outerRect = borderRect;
            LayoutRectOutsets outerInsets = doubleStripeInsets(m_edges, BorderEdge::DoubleBorderStripeOuter);

            if (bleedAvoidanceIsClipping(m_bleedAvoidance)) {
                outerRect.inflate(1);
                outerInsets.setTop(outerInsets.top() - 1);
                outerInsets.setRight(outerInsets.right() - 1);
                outerInsets.setBottom(outerInsets.bottom() - 1);
                outerInsets.setLeft(outerInsets.left() - 1);
            }

            FloatRoundedRect outerClip = m_style.getRoundedInnerBorderFor(outerRect, outerInsets,
                m_includeLogicalLeftEdge, m_includeLogicalRightEdge);
            graphicsContext->clipOutRoundedRect(outerClip);
            drawBoxSideFromPath(graphicsContext, borderRect, borderPath, thickness, drawThickness,
                side, color, SOLID);
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
        drawBoxSideFromPath(graphicsContext, borderRect, borderPath, thickness, drawThickness,
            side, color, s1);

        // Paint inner only
        GraphicsContextStateSaver stateSaver(*graphicsContext);
        LayoutUnit topWidth = m_edges[BSTop].usedWidth() / 2;
        LayoutUnit bottomWidth = m_edges[BSBottom].usedWidth() / 2;
        LayoutUnit leftWidth = m_edges[BSLeft].usedWidth() / 2;
        LayoutUnit rightWidth = m_edges[BSRight].usedWidth() / 2;

        FloatRoundedRect clipRect = m_style.getRoundedInnerBorderFor(borderRect,
            LayoutRectOutsets(-topWidth, -rightWidth, -bottomWidth, -leftWidth),
            m_includeLogicalLeftEdge, m_includeLogicalRightEdge);

        graphicsContext->clipRoundedRect(clipRect);
        drawBoxSideFromPath(graphicsContext, borderRect, borderPath, thickness, drawThickness,
            side, color, s2);
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
    BoxSide side) const
{
    graphicsContext->clip(calculateSideRectIncludingInner(m_outer, m_edges, side));
    FloatRoundedRect adjustedInnerRect = calculateAdjustedInnerBorder(m_inner, side);
    if (!adjustedInnerRect.isEmpty())
        graphicsContext->clipOutRoundedRect(adjustedInnerRect);
}

void BoxBorderPainter::clipBorderSidePolygon(GraphicsContext* graphicsContext, BoxSide side,
    bool firstEdgeMatches, bool secondEdgeMatches) const
{
    FloatPoint quad[4];

    const LayoutRect outerRect(m_outer.rect());
    const LayoutRect innerRect(m_inner.rect());

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

        if (!m_inner.radii().topLeft().isZero()) {
            findIntersection(quad[0], quad[1],
                FloatPoint(
                    quad[1].x() + m_inner.radii().topLeft().width(),
                    quad[1].y()),
                FloatPoint(
                    quad[1].x(),
                    quad[1].y() + m_inner.radii().topLeft().height()),
                quad[1]);
        }

        if (!m_inner.radii().topRight().isZero()) {
            findIntersection(quad[3], quad[2],
                FloatPoint(
                    quad[2].x() - m_inner.radii().topRight().width(),
                    quad[2].y()),
                FloatPoint(
                    quad[2].x(),
                    quad[2].y() + m_inner.radii().topRight().height()),
                quad[2]);
        }
        break;

    case BSLeft:
        quad[0] = FloatPoint(outerRect.minXMinYCorner());
        quad[1] = FloatPoint(innerRect.minXMinYCorner());
        quad[2] = FloatPoint(innerRect.minXMaxYCorner());
        quad[3] = FloatPoint(outerRect.minXMaxYCorner());

        if (!m_inner.radii().topLeft().isZero()) {
            findIntersection(quad[0], quad[1],
                FloatPoint(
                    quad[1].x() + m_inner.radii().topLeft().width(),
                    quad[1].y()),
                FloatPoint(
                    quad[1].x(),
                    quad[1].y() + m_inner.radii().topLeft().height()),
                quad[1]);
        }

        if (!m_inner.radii().bottomLeft().isZero()) {
            findIntersection(quad[3], quad[2],
                FloatPoint(
                    quad[2].x() + m_inner.radii().bottomLeft().width(),
                    quad[2].y()),
                FloatPoint(
                    quad[2].x(),
                    quad[2].y() - m_inner.radii().bottomLeft().height()),
                quad[2]);
        }
        break;

    case BSBottom:
        quad[0] = FloatPoint(outerRect.minXMaxYCorner());
        quad[1] = FloatPoint(innerRect.minXMaxYCorner());
        quad[2] = FloatPoint(innerRect.maxXMaxYCorner());
        quad[3] = FloatPoint(outerRect.maxXMaxYCorner());

        if (!m_inner.radii().bottomLeft().isZero()) {
            findIntersection(quad[0], quad[1],
                FloatPoint(
                    quad[1].x() + m_inner.radii().bottomLeft().width(),
                    quad[1].y()),
                FloatPoint(
                    quad[1].x(),
                    quad[1].y() - m_inner.radii().bottomLeft().height()),
                quad[1]);
        }

        if (!m_inner.radii().bottomRight().isZero()) {
            findIntersection(quad[3], quad[2],
                FloatPoint(
                    quad[2].x() - m_inner.radii().bottomRight().width(),
                    quad[2].y()),
                FloatPoint(
                    quad[2].x(),
                    quad[2].y() - m_inner.radii().bottomRight().height()),
                quad[2]);
        }
        break;

    case BSRight:
        quad[0] = FloatPoint(outerRect.maxXMinYCorner());
        quad[1] = FloatPoint(innerRect.maxXMinYCorner());
        quad[2] = FloatPoint(innerRect.maxXMaxYCorner());
        quad[3] = FloatPoint(outerRect.maxXMaxYCorner());

        if (!m_inner.radii().topRight().isZero()) {
            findIntersection(quad[0], quad[1],
                FloatPoint(
                    quad[1].x() - m_inner.radii().topRight().width(),
                    quad[1].y()),
                FloatPoint(
                    quad[1].x(),
                    quad[1].y() + m_inner.radii().topRight().height()),
                quad[1]);
        }

        if (!m_inner.radii().bottomRight().isZero()) {
            findIntersection(quad[3], quad[2],
                FloatPoint(
                    quad[2].x() - m_inner.radii().bottomRight().width(),
                    quad[2].y()),
                FloatPoint(
                    quad[2].x(),
                    quad[2].y() - m_inner.radii().bottomRight().height()),
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
