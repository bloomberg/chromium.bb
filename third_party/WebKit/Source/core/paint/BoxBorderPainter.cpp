// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxBorderPainter.h"

#include "core/paint/BoxPainter.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/style/BorderEdge.h"
#include "core/style/ComputedStyle.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "wtf/Vector.h"
#include <algorithm>

namespace blink {

namespace {

enum BorderEdgeFlag {
  TopBorderEdge = 1 << BSTop,
  RightBorderEdge = 1 << BSRight,
  BottomBorderEdge = 1 << BSBottom,
  LeftBorderEdge = 1 << BSLeft,
  AllBorderEdges =
      TopBorderEdge | BottomBorderEdge | LeftBorderEdge | RightBorderEdge
};

inline BorderEdgeFlag edgeFlagForSide(BoxSide side) {
  return static_cast<BorderEdgeFlag>(1 << side);
}

inline bool includesEdge(BorderEdgeFlags flags, BoxSide side) {
  return flags & edgeFlagForSide(side);
}

inline bool includesAdjacentEdges(BorderEdgeFlags flags) {
  // The set includes adjacent edges iff it contains at least one horizontal and
  // one vertical edge.
  return (flags & (TopBorderEdge | BottomBorderEdge)) &&
         (flags & (LeftBorderEdge | RightBorderEdge));
}

inline bool styleRequiresClipPolygon(EBorderStyle style) {
  // These are drawn with a stroke, so we have to clip to get corner miters.
  return style == BorderStyleDotted || style == BorderStyleDashed;
}

inline bool borderStyleFillsBorderArea(EBorderStyle style) {
  return !(style == BorderStyleDotted || style == BorderStyleDashed ||
           style == BorderStyleDouble);
}

inline bool borderStyleHasInnerDetail(EBorderStyle style) {
  return style == BorderStyleGroove || style == BorderStyleRidge ||
         style == BorderStyleDouble;
}

inline bool borderStyleIsDottedOrDashed(EBorderStyle style) {
  return style == BorderStyleDotted || style == BorderStyleDashed;
}

// BorderStyleOutset darkens the bottom and right (and maybe lightens the top
// and left) BorderStyleInset darkens the top and left (and maybe lightens the
// bottom and right).
inline bool borderStyleHasUnmatchedColorsAtCorner(EBorderStyle style,
                                                  BoxSide side,
                                                  BoxSide adjacentSide) {
  // These styles match at the top/left and bottom/right.
  if (style == BorderStyleInset || style == BorderStyleGroove ||
      style == BorderStyleRidge || style == BorderStyleOutset) {
    const BorderEdgeFlags topRightFlags =
        edgeFlagForSide(BSTop) | edgeFlagForSide(BSRight);
    const BorderEdgeFlags bottomLeftFlags =
        edgeFlagForSide(BSBottom) | edgeFlagForSide(BSLeft);

    BorderEdgeFlags flags =
        edgeFlagForSide(side) | edgeFlagForSide(adjacentSide);
    return flags == topRightFlags || flags == bottomLeftFlags;
  }
  return false;
}

inline bool colorsMatchAtCorner(BoxSide side,
                                BoxSide adjacentSide,
                                const BorderEdge edges[]) {
  if (!edges[adjacentSide].shouldRender())
    return false;

  if (!edges[side].sharesColorWith(edges[adjacentSide]))
    return false;

  return !borderStyleHasUnmatchedColorsAtCorner(edges[side].borderStyle(), side,
                                                adjacentSide);
}

inline bool borderWillArcInnerEdge(const FloatSize& firstRadius,
                                   const FloatSize& secondRadius) {
  return !firstRadius.isZero() || !secondRadius.isZero();
}

inline bool willOverdraw(BoxSide side,
                         EBorderStyle style,
                         BorderEdgeFlags completedEdges) {
  // If we're done with this side, it will obviously not overdraw any portion of
  // the current edge.
  if (includesEdge(completedEdges, side))
    return false;

  // The side is still to be drawn. It overdraws the current edge iff it has a
  // solid fill style.
  return borderStyleFillsBorderArea(style);
}

inline bool borderStylesRequireMiter(BoxSide side,
                                     BoxSide adjacentSide,
                                     EBorderStyle style,
                                     EBorderStyle adjacentStyle) {
  if (style == BorderStyleDouble || adjacentStyle == BorderStyleDouble ||
      adjacentStyle == BorderStyleGroove || adjacentStyle == BorderStyleRidge)
    return true;

  if (borderStyleIsDottedOrDashed(style) !=
      borderStyleIsDottedOrDashed(adjacentStyle))
    return true;

  if (style != adjacentStyle)
    return true;

  return borderStyleHasUnmatchedColorsAtCorner(style, side, adjacentSide);
}

FloatRect calculateSideRect(const FloatRoundedRect& outerBorder,
                            const BorderEdge& edge,
                            int side) {
  FloatRect sideRect = outerBorder.rect();
  float width = edge.width();

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

FloatRect calculateSideRectIncludingInner(const FloatRoundedRect& outerBorder,
                                          const BorderEdge edges[],
                                          BoxSide side) {
  FloatRect sideRect = outerBorder.rect();
  float width;

  switch (side) {
    case BSTop:
      width = sideRect.height() - edges[BSBottom].width();
      sideRect.setHeight(width);
      break;
    case BSBottom:
      width = sideRect.height() - edges[BSTop].width();
      sideRect.shiftYEdgeTo(sideRect.maxY() - width);
      break;
    case BSLeft:
      width = sideRect.width() - edges[BSRight].width();
      sideRect.setWidth(width);
      break;
    case BSRight:
      width = sideRect.width() - edges[BSLeft].width();
      sideRect.shiftXEdgeTo(sideRect.maxX() - width);
      break;
  }

  return sideRect;
}

FloatRoundedRect calculateAdjustedInnerBorder(
    const FloatRoundedRect& innerBorder,
    BoxSide side) {
  // Expand the inner border as necessary to make it a rounded rect (i.e. radii
  // contained within each edge).  This function relies on the fact we only get
  // radii not contained within each edge if one of the radii for an edge is
  // zero, so we can shift the arc towards the zero radius corner.
  FloatRoundedRect::Radii newRadii = innerBorder.getRadii();
  FloatRect newRect = innerBorder.rect();

  float overshoot;
  float maxRadii;

  switch (side) {
    case BSTop:
      overshoot = newRadii.topLeft().width() + newRadii.topRight().width() -
                  newRect.width();
      // FIXME: once we start pixel-snapping rounded rects after this point, the
      // overshoot concept should disappear.
      if (overshoot > 0.1) {
        newRect.setWidth(newRect.width() + overshoot);
        if (!newRadii.topLeft().width())
          newRect.move(-overshoot, 0);
      }
      newRadii.setBottomLeft(FloatSize(0, 0));
      newRadii.setBottomRight(FloatSize(0, 0));
      maxRadii =
          std::max(newRadii.topLeft().height(), newRadii.topRight().height());
      if (maxRadii > newRect.height())
        newRect.setHeight(maxRadii);
      break;

    case BSBottom:
      overshoot = newRadii.bottomLeft().width() +
                  newRadii.bottomRight().width() - newRect.width();
      if (overshoot > 0.1) {
        newRect.setWidth(newRect.width() + overshoot);
        if (!newRadii.bottomLeft().width())
          newRect.move(-overshoot, 0);
      }
      newRadii.setTopLeft(FloatSize(0, 0));
      newRadii.setTopRight(FloatSize(0, 0));
      maxRadii = std::max(newRadii.bottomLeft().height(),
                          newRadii.bottomRight().height());
      if (maxRadii > newRect.height()) {
        newRect.move(0, newRect.height() - maxRadii);
        newRect.setHeight(maxRadii);
      }
      break;

    case BSLeft:
      overshoot = newRadii.topLeft().height() + newRadii.bottomLeft().height() -
                  newRect.height();
      if (overshoot > 0.1) {
        newRect.setHeight(newRect.height() + overshoot);
        if (!newRadii.topLeft().height())
          newRect.move(0, -overshoot);
      }
      newRadii.setTopRight(FloatSize(0, 0));
      newRadii.setBottomRight(FloatSize(0, 0));
      maxRadii =
          std::max(newRadii.topLeft().width(), newRadii.bottomLeft().width());
      if (maxRadii > newRect.width())
        newRect.setWidth(maxRadii);
      break;

    case BSRight:
      overshoot = newRadii.topRight().height() +
                  newRadii.bottomRight().height() - newRect.height();
      if (overshoot > 0.1) {
        newRect.setHeight(newRect.height() + overshoot);
        if (!newRadii.topRight().height())
          newRect.move(0, -overshoot);
      }
      newRadii.setTopLeft(FloatSize(0, 0));
      newRadii.setBottomLeft(FloatSize(0, 0));
      maxRadii =
          std::max(newRadii.topRight().width(), newRadii.bottomRight().width());
      if (maxRadii > newRect.width()) {
        newRect.move(newRect.width() - maxRadii, 0);
        newRect.setWidth(maxRadii);
      }
      break;
  }

  return FloatRoundedRect(newRect, newRadii);
}

LayoutRectOutsets doubleStripeInsets(const BorderEdge edges[],
                                     BorderEdge::DoubleBorderStripe stripe) {
  // Insets are representes as negative outsets.
  return LayoutRectOutsets(-edges[BSTop].getDoubleBorderStripeWidth(stripe),
                           -edges[BSRight].getDoubleBorderStripeWidth(stripe),
                           -edges[BSBottom].getDoubleBorderStripeWidth(stripe),
                           -edges[BSLeft].getDoubleBorderStripeWidth(stripe));
}

void drawSolidBorderRect(GraphicsContext& context,
                         const FloatRect& borderRect,
                         float borderWidth,
                         const Color& color) {
  FloatRect strokeRect(borderRect);
  borderWidth = roundf(borderWidth);
  strokeRect.inflate(-borderWidth / 2);

  bool wasAntialias = context.shouldAntialias();
  if (!wasAntialias)
    context.setShouldAntialias(true);

  context.setStrokeStyle(SolidStroke);
  context.setStrokeColor(color);
  context.strokeRect(strokeRect, borderWidth);

  if (!wasAntialias)
    context.setShouldAntialias(false);
}

void drawBleedAdjustedDRRect(GraphicsContext& context,
                             BackgroundBleedAvoidance bleedAvoidance,
                             const FloatRoundedRect& outer,
                             const FloatRoundedRect& inner,
                             Color color) {
  switch (bleedAvoidance) {
    case BackgroundBleedClipLayer: {
      // BackgroundBleedClipLayer clips the outer rrect for the whole layer.
      // Based on this, we can avoid background bleeding by filling the
      // *outside* of inner rrect, all the way to the layer bounds (enclosing
      // int rect for the clip, in device space).
      SkPath path;
      path.addRRect(inner);
      path.setFillType(SkPath::kInverseWinding_FillType);

      PaintFlags flags;
      flags.setColor(color.rgb());
      flags.setStyle(PaintFlags::kFill_Style);
      flags.setAntiAlias(true);
      context.drawPath(path, flags);

      break;
    }
    case BackgroundBleedClipOnly:
      if (outer.isRounded()) {
        // BackgroundBleedClipOnly clips the outer rrect corners for us.
        FloatRoundedRect adjustedOuter = outer;
        adjustedOuter.setRadii(FloatRoundedRect::Radii());
        context.fillDRRect(adjustedOuter, inner, color);
        break;
      }
    // fall through
    default:
      context.fillDRRect(outer, inner, color);
      break;
  }
}

bool bleedAvoidanceIsClipping(BackgroundBleedAvoidance bleedAvoidance) {
  return bleedAvoidance == BackgroundBleedClipOnly ||
         bleedAvoidance == BackgroundBleedClipLayer;
}

// The LUTs below assume specific enum values.
static_assert(BorderStyleNone == 0, "unexpected EBorderStyle value");
static_assert(BorderStyleHidden == 1, "unexpected EBorderStyle value");
static_assert(BorderStyleInset == 2, "unexpected EBorderStyle value");
static_assert(BorderStyleGroove == 3, "unexpected EBorderStyle value");
static_assert(BorderStyleOutset == 4, "unexpected EBorderStyle value");
static_assert(BorderStyleRidge == 5, "unexpected EBorderStyle value");
static_assert(BorderStyleDotted == 6, "unexpected EBorderStyle value");
static_assert(BorderStyleDashed == 7, "unexpected EBorderStyle value");
static_assert(BorderStyleSolid == 8, "unexpected EBorderStyle value");
static_assert(BorderStyleDouble == 9, "unexpected EBorderStyle value");

static_assert(BSTop == 0, "unexpected BoxSide value");
static_assert(BSRight == 1, "unexpected BoxSide value");
static_assert(BSBottom == 2, "unexpected BoxSide value");
static_assert(BSLeft == 3, "unexpected BoxSide value");

// Style-based paint order: non-solid edges (dashed/dotted/double) are painted
// before solid edges (inset/outset/groove/ridge/solid) to maximize overdraw
// opportunities.
const unsigned kStylePriority[] = {
    0 /* BorderStyleNone */,   0 /* BorderStyleHidden */,
    2 /* BorderStyleInset */,  2 /* BorderStyleGroove */,
    2 /* BorderStyleOutset */, 2 /* BorderStyleRidge */,
    1 /* BorderStyleDotted */, 1 /* BorderStyleDashed */,
    3 /* BorderStyleSolid */,  1 /* BorderStyleDouble */
};

// Given the same style, prefer drawing in non-adjacent order to minimize the
// number of sides which require miters.
const unsigned kSidePriority[] = {
    0, /* BSTop */
    2, /* BSRight */
    1, /* BSBottom */
    3, /* BSLeft */
};

// Edges sharing the same opacity. Stores both a side list and an edge bitfield
// to support constant time iteration + membership tests.
struct OpacityGroup {
  OpacityGroup(unsigned alpha) : edgeFlags(0), alpha(alpha) {}

  Vector<BoxSide, 4> sides;
  BorderEdgeFlags edgeFlags;
  unsigned alpha;
};

void clipQuad(GraphicsContext& context,
              const FloatPoint quad[],
              bool antialiased) {
  SkPath path;
  path.moveTo(quad[0]);
  path.lineTo(quad[1]);
  path.lineTo(quad[2]);
  path.lineTo(quad[3]);

  context.clipPath(path, antialiased ? AntiAliased : NotAntiAliased);
}

}  // anonymous namespace

// Holds edges grouped by opacity and sorted in paint order.
struct BoxBorderPainter::ComplexBorderInfo {
  ComplexBorderInfo(const BoxBorderPainter& borderPainter, bool antiAlias)
      : antiAlias(antiAlias) {
    Vector<BoxSide, 4> sortedSides;

    // First, collect all visible sides.
    for (unsigned i = borderPainter.m_firstVisibleEdge; i < 4; ++i) {
      BoxSide side = static_cast<BoxSide>(i);

      if (includesEdge(borderPainter.m_visibleEdgeSet, side))
        sortedSides.push_back(side);
    }
    DCHECK(!sortedSides.isEmpty());

    // Then sort them in paint order, based on three (prioritized) criteria:
    // alpha, style, side.
    std::sort(
        sortedSides.begin(), sortedSides.end(),
        [&borderPainter](BoxSide a, BoxSide b) -> bool {
          const BorderEdge& edgeA = borderPainter.m_edges[a];
          const BorderEdge& edgeB = borderPainter.m_edges[b];

          const unsigned alphaA = edgeA.color.alpha();
          const unsigned alphaB = edgeB.color.alpha();
          if (alphaA != alphaB)
            return alphaA < alphaB;

          const unsigned stylePriorityA = kStylePriority[edgeA.borderStyle()];
          const unsigned stylePriorityB = kStylePriority[edgeB.borderStyle()];
          if (stylePriorityA != stylePriorityB)
            return stylePriorityA < stylePriorityB;

          return kSidePriority[a] < kSidePriority[b];
        });

    // Finally, build the opacity group structures.
    buildOpacityGroups(borderPainter, sortedSides);

    if (borderPainter.m_isRounded)
      roundedBorderPath.addRoundedRect(borderPainter.m_outer);
  }

  Vector<OpacityGroup, 4> opacityGroups;

  // Potentially used when drawing rounded borders.
  Path roundedBorderPath;

  bool antiAlias;

 private:
  void buildOpacityGroups(const BoxBorderPainter& borderPainter,
                          const Vector<BoxSide, 4>& sortedSides) {
    unsigned currentAlpha = 0;
    for (BoxSide side : sortedSides) {
      const BorderEdge& edge = borderPainter.m_edges[side];
      const unsigned edgeAlpha = edge.color.alpha();

      DCHECK_GT(edgeAlpha, 0u);
      DCHECK_GE(edgeAlpha, currentAlpha);
      if (edgeAlpha != currentAlpha) {
        opacityGroups.push_back(OpacityGroup(edgeAlpha));
        currentAlpha = edgeAlpha;
      }

      DCHECK(!opacityGroups.isEmpty());
      OpacityGroup& currentGroup = opacityGroups.back();
      currentGroup.sides.push_back(side);
      currentGroup.edgeFlags |= edgeFlagForSide(side);
    }

    DCHECK(!opacityGroups.isEmpty());
  }
};

void BoxBorderPainter::drawDoubleBorder(GraphicsContext& context,
                                        const LayoutRect& borderRect) const {
  DCHECK(m_isUniformColor);
  DCHECK(m_isUniformStyle);
  DCHECK(firstEdge().borderStyle() == BorderStyleDouble);
  DCHECK(m_visibleEdgeSet == AllBorderEdges);

  const Color color = firstEdge().color;

  // outer stripe
  const LayoutRectOutsets outerThirdInsets =
      doubleStripeInsets(m_edges, BorderEdge::DoubleBorderStripeOuter);
  const FloatRoundedRect outerThirdRect = m_style.getRoundedInnerBorderFor(
      borderRect, outerThirdInsets, m_includeLogicalLeftEdge,
      m_includeLogicalRightEdge);
  drawBleedAdjustedDRRect(context, m_bleedAvoidance, m_outer, outerThirdRect,
                          color);

  // inner stripe
  const LayoutRectOutsets innerThirdInsets =
      doubleStripeInsets(m_edges, BorderEdge::DoubleBorderStripeInner);
  const FloatRoundedRect innerThirdRect = m_style.getRoundedInnerBorderFor(
      borderRect, innerThirdInsets, m_includeLogicalLeftEdge,
      m_includeLogicalRightEdge);
  context.fillDRRect(innerThirdRect, m_inner, color);
}

bool BoxBorderPainter::paintBorderFastPath(GraphicsContext& context,
                                           const LayoutRect& borderRect) const {
  if (!m_isUniformColor || !m_isUniformStyle || !m_inner.isRenderable())
    return false;

  if (firstEdge().borderStyle() != BorderStyleSolid &&
      firstEdge().borderStyle() != BorderStyleDouble)
    return false;

  if (m_visibleEdgeSet == AllBorderEdges) {
    if (firstEdge().borderStyle() == BorderStyleSolid) {
      if (m_isUniformWidth && !m_outer.isRounded()) {
        // 4-side, solid, uniform-width, rectangular border => one drawRect()
        drawSolidBorderRect(context, m_outer.rect(), firstEdge().width(),
                            firstEdge().color);
      } else {
        // 4-side, solid border => one drawDRRect()
        drawBleedAdjustedDRRect(context, m_bleedAvoidance, m_outer, m_inner,
                                firstEdge().color);
      }
    } else {
      // 4-side, double border => 2x drawDRRect()
      DCHECK(firstEdge().borderStyle() == BorderStyleDouble);
      drawDoubleBorder(context, borderRect);
    }

    return true;
  }

  // This is faster than the normal complex border path only if it avoids
  // creating transparency layers (when the border is translucent).
  if (firstEdge().borderStyle() == BorderStyleSolid && !m_outer.isRounded() &&
      m_hasAlpha) {
    DCHECK(m_visibleEdgeSet != AllBorderEdges);
    // solid, rectangular border => one drawPath()
    Path path;
    path.setWindRule(RULE_NONZERO);

    for (int i = BSTop; i <= BSLeft; ++i) {
      const BorderEdge& currEdge = m_edges[i];
      if (currEdge.shouldRender())
        path.addRect(calculateSideRect(m_outer, currEdge, i));
    }

    context.setFillColor(firstEdge().color);
    context.fillPath(path);
    return true;
  }

  return false;
}

BoxBorderPainter::BoxBorderPainter(const LayoutRect& borderRect,
                                   const ComputedStyle& style,
                                   BackgroundBleedAvoidance bleedAvoidance,
                                   bool includeLogicalLeftEdge,
                                   bool includeLogicalRightEdge)
    : m_style(style),
      m_bleedAvoidance(bleedAvoidance),
      m_includeLogicalLeftEdge(includeLogicalLeftEdge),
      m_includeLogicalRightEdge(includeLogicalRightEdge),
      m_visibleEdgeCount(0),
      m_firstVisibleEdge(0),
      m_visibleEdgeSet(0),
      m_isUniformStyle(true),
      m_isUniformWidth(true),
      m_isUniformColor(true),
      m_isRounded(false),
      m_hasAlpha(false) {
  style.getBorderEdgeInfo(m_edges, includeLogicalLeftEdge,
                          includeLogicalRightEdge);
  computeBorderProperties();

  // No need to compute the rrects if we don't have any borders to draw.
  if (!m_visibleEdgeSet)
    return;

  m_outer = m_style.getRoundedBorderFor(borderRect, includeLogicalLeftEdge,
                                        includeLogicalRightEdge);
  m_inner = m_style.getRoundedInnerBorderFor(borderRect, includeLogicalLeftEdge,
                                             includeLogicalRightEdge);

  m_isRounded = m_outer.isRounded();
}

BoxBorderPainter::BoxBorderPainter(const ComputedStyle& style,
                                   const LayoutRect& outer,
                                   const LayoutRect& inner,
                                   const BorderEdge& uniformEdgeInfo)
    : m_style(style),
      m_bleedAvoidance(BackgroundBleedNone),
      m_includeLogicalLeftEdge(true),
      m_includeLogicalRightEdge(true),
      m_outer(FloatRect(outer)),
      m_inner(FloatRect(inner)),
      m_visibleEdgeCount(0),
      m_firstVisibleEdge(0),
      m_visibleEdgeSet(0),
      m_isUniformStyle(true),
      m_isUniformWidth(true),
      m_isUniformColor(true),
      m_isRounded(false),
      m_hasAlpha(false) {
  for (auto& edge : m_edges)
    edge = uniformEdgeInfo;

  computeBorderProperties();
}

void BoxBorderPainter::computeBorderProperties() {
  for (unsigned i = 0; i < WTF_ARRAY_LENGTH(m_edges); ++i) {
    const BorderEdge& edge = m_edges[i];

    if (!edge.shouldRender()) {
      if (edge.presentButInvisible()) {
        m_isUniformWidth = false;
        m_isUniformColor = false;
      }

      continue;
    }

    DCHECK_GT(edge.color.alpha(), 0);

    m_visibleEdgeCount++;
    m_visibleEdgeSet |= edgeFlagForSide(static_cast<BoxSide>(i));

    m_hasAlpha |= edge.color.hasAlpha();

    if (m_visibleEdgeCount == 1) {
      m_firstVisibleEdge = i;
      continue;
    }

    m_isUniformStyle &=
        edge.borderStyle() == m_edges[m_firstVisibleEdge].borderStyle();
    m_isUniformWidth &= edge.width() == m_edges[m_firstVisibleEdge].width();
    m_isUniformColor &= edge.color == m_edges[m_firstVisibleEdge].color;
  }
}

void BoxBorderPainter::paintBorder(const PaintInfo& info,
                                   const LayoutRect& rect) const {
  if (!m_visibleEdgeCount || m_outer.rect().isEmpty())
    return;

  GraphicsContext& graphicsContext = info.context;

  if (paintBorderFastPath(graphicsContext, rect))
    return;

  bool clipToOuterBorder = m_outer.isRounded();
  GraphicsContextStateSaver stateSaver(graphicsContext, clipToOuterBorder);
  if (clipToOuterBorder) {
    // For BackgroundBleedClip{Only,Layer}, the outer rrect clip is already
    // applied.
    if (!bleedAvoidanceIsClipping(m_bleedAvoidance))
      graphicsContext.clipRoundedRect(m_outer);

    if (m_inner.isRenderable() && !m_inner.isEmpty())
      graphicsContext.clipOutRoundedRect(m_inner);
  }

  const ComplexBorderInfo borderInfo(*this, true);
  paintOpacityGroup(graphicsContext, borderInfo, 0, 1);
}

// In order to maximize the use of overdraw as a corner seam avoidance
// technique, we draw translucent border sides using the following algorithm:
//
//   1) cluster sides sharing the same opacity into "opacity groups"
//      [ComplexBorderInfo]
//   2) sort groups in increasing opacity order [ComplexBorderInfo]
//   3) reverse-iterate over groups (decreasing opacity order), pushing nested
//      transparency layers with adjusted/relative opacity [paintOpacityGroup]
//   4) iterate over groups (increasing opacity order), painting actual group
//      contents and then ending their corresponding transparency layer
//      [paintOpacityGroup]
//
// Layers are created in decreasing opacity order (top -> bottom), while actual
// border sides are drawn in increasing opacity order (bottom -> top). At each
// level, opacity is adjusted to acount for accumulated/ancestor layer alpha.
// Because opacity is applied via layers, the actual draw paint is opaque.
//
// As an example, let's consider a border with the following sides/opacities:
//
//   top:    1.0
//   right:  0.25
//   bottom: 0.5
//   left:   0.25
//
// These are grouped and sorted in ComplexBorderInfo as follows:
//
//   group[0]: { alpha: 1.0,  sides: top }
//   group[1]: { alpha: 0.5,  sides: bottom }
//   group[2]: { alpha: 0.25, sides: right, left }
//
// Applying the algorithm yields the following paint sequence:
//
//                                // no layer needed for group 0 (alpha = 1)
//   beginLayer(0.5)              // layer for group 1
//     beginLayer(0.5)            // layer for group 2 (alpha: 0.5 * 0.5 = 0.25)
//       paintSides(right, left)  // paint group 2
//     endLayer
//     paintSides(bottom)         // paint group 1
//   endLayer
//   paintSides(top)              // paint group 0
//
// Note that we're always drawing using opaque paints on top of less-opaque
// content - hence we can use overdraw to mask portions of the previous sides.
//
BorderEdgeFlags BoxBorderPainter::paintOpacityGroup(
    GraphicsContext& context,
    const ComplexBorderInfo& borderInfo,
    unsigned index,
    float effectiveOpacity) const {
  DCHECK(effectiveOpacity > 0 && effectiveOpacity <= 1);

  const size_t opacityGroupCount = borderInfo.opacityGroups.size();

  // For overdraw logic purposes, treat missing/transparent edges as completed.
  if (index >= opacityGroupCount)
    return ~m_visibleEdgeSet;

  // Groups are sorted in increasing opacity order, but we need to create layers
  // in decreasing opacity order - hence the reverse iteration.
  const OpacityGroup& group =
      borderInfo.opacityGroups[opacityGroupCount - index - 1];

  // Adjust this group's paint opacity to account for ancestor transparency
  // layers (needed in case we avoid creating a layer below).
  unsigned paintAlpha = group.alpha / effectiveOpacity;
  DCHECK_LE(paintAlpha, 255u);

  // For the last (bottom) group, we can skip the layer even in the presence of
  // opacity iff it contains no adjecent edges (no in-group overdraw
  // possibility).
  bool needsLayer =
      group.alpha != 255 && (includesAdjacentEdges(group.edgeFlags) ||
                             (index + 1 < borderInfo.opacityGroups.size()));

  if (needsLayer) {
    const float groupOpacity = static_cast<float>(group.alpha) / 255;
    DCHECK_LT(groupOpacity, effectiveOpacity);

    context.beginLayer(groupOpacity / effectiveOpacity);
    effectiveOpacity = groupOpacity;

    // Group opacity is applied via a layer => we draw the members using opaque
    // paint.
    paintAlpha = 255;
  }

  // Recursion may seem unpalatable here, but
  //   a) it has an upper bound of 4
  //   b) only triggers at all when mixing border sides with different opacities
  //   c) it allows us to express the layer nesting algorithm more naturally
  BorderEdgeFlags completedEdges =
      paintOpacityGroup(context, borderInfo, index + 1, effectiveOpacity);

  // Paint the actual group edges with an alpha adjusted to account for
  // ancenstor layers opacity.
  for (BoxSide side : group.sides) {
    paintSide(context, borderInfo, side, paintAlpha, completedEdges);
    completedEdges |= edgeFlagForSide(side);
  }

  if (needsLayer)
    context.endLayer();

  return completedEdges;
}

void BoxBorderPainter::paintSide(GraphicsContext& context,
                                 const ComplexBorderInfo& borderInfo,
                                 BoxSide side,
                                 unsigned alpha,
                                 BorderEdgeFlags completedEdges) const {
  const BorderEdge& edge = m_edges[side];
  DCHECK(edge.shouldRender());
  const Color color(edge.color.red(), edge.color.green(), edge.color.blue(),
                    alpha);

  FloatRect sideRect = m_outer.rect();
  const Path* path = nullptr;

  // TODO(fmalita): find a way to consolidate these without sacrificing
  // readability.
  switch (side) {
    case BSTop: {
      bool usePath = m_isRounded &&
                     (borderStyleHasInnerDetail(edge.borderStyle()) ||
                      borderWillArcInnerEdge(m_inner.getRadii().topLeft(),
                                             m_inner.getRadii().topRight()));
      if (usePath)
        path = &borderInfo.roundedBorderPath;
      else
        sideRect.setHeight(roundf(edge.width()));

      paintOneBorderSide(context, sideRect, BSTop, BSLeft, BSRight, path,
                         borderInfo.antiAlias, color, completedEdges);
      break;
    }
    case BSBottom: {
      bool usePath = m_isRounded &&
                     (borderStyleHasInnerDetail(edge.borderStyle()) ||
                      borderWillArcInnerEdge(m_inner.getRadii().bottomLeft(),
                                             m_inner.getRadii().bottomRight()));
      if (usePath)
        path = &borderInfo.roundedBorderPath;
      else
        sideRect.shiftYEdgeTo(sideRect.maxY() - roundf(edge.width()));

      paintOneBorderSide(context, sideRect, BSBottom, BSLeft, BSRight, path,
                         borderInfo.antiAlias, color, completedEdges);
      break;
    }
    case BSLeft: {
      bool usePath = m_isRounded &&
                     (borderStyleHasInnerDetail(edge.borderStyle()) ||
                      borderWillArcInnerEdge(m_inner.getRadii().bottomLeft(),
                                             m_inner.getRadii().topLeft()));
      if (usePath)
        path = &borderInfo.roundedBorderPath;
      else
        sideRect.setWidth(roundf(edge.width()));

      paintOneBorderSide(context, sideRect, BSLeft, BSTop, BSBottom, path,
                         borderInfo.antiAlias, color, completedEdges);
      break;
    }
    case BSRight: {
      bool usePath = m_isRounded &&
                     (borderStyleHasInnerDetail(edge.borderStyle()) ||
                      borderWillArcInnerEdge(m_inner.getRadii().bottomRight(),
                                             m_inner.getRadii().topRight()));
      if (usePath)
        path = &borderInfo.roundedBorderPath;
      else
        sideRect.shiftXEdgeTo(sideRect.maxX() - roundf(edge.width()));

      paintOneBorderSide(context, sideRect, BSRight, BSTop, BSBottom, path,
                         borderInfo.antiAlias, color, completedEdges);
      break;
    }
    default:
      ASSERT_NOT_REACHED();
  }
}

BoxBorderPainter::MiterType BoxBorderPainter::computeMiter(
    BoxSide side,
    BoxSide adjacentSide,
    BorderEdgeFlags completedEdges,
    bool antialias) const {
  const BorderEdge& adjacentEdge = m_edges[adjacentSide];

  // No miters for missing edges.
  if (!adjacentEdge.isPresent)
    return NoMiter;

  // The adjacent edge will overdraw this corner, resulting in a correct miter.
  if (willOverdraw(adjacentSide, adjacentEdge.borderStyle(), completedEdges))
    return NoMiter;

  // Color transitions require miters. Use miters compatible with the AA drawing
  // mode to avoid introducing extra clips.
  if (!colorsMatchAtCorner(side, adjacentSide, m_edges))
    return antialias ? SoftMiter : HardMiter;

  // Non-anti-aliased miters ensure correct same-color seaming when required by
  // style.
  if (borderStylesRequireMiter(side, adjacentSide, m_edges[side].borderStyle(),
                               adjacentEdge.borderStyle()))
    return HardMiter;

  // Overdraw the adjacent edge when the colors match and we have no style
  // restrictions.
  return NoMiter;
}

bool BoxBorderPainter::mitersRequireClipping(MiterType miter1,
                                             MiterType miter2,
                                             EBorderStyle style,
                                             bool antialias) {
  // Clipping is required if any of the present miters doesn't match the current
  // AA mode.
  bool shouldClip = antialias ? miter1 == HardMiter || miter2 == HardMiter
                              : miter1 == SoftMiter || miter2 == SoftMiter;

  // Some styles require clipping for any type of miter.
  shouldClip = shouldClip || ((miter1 != NoMiter || miter2 != NoMiter) &&
                              styleRequiresClipPolygon(style));

  return shouldClip;
}

void BoxBorderPainter::paintOneBorderSide(
    GraphicsContext& graphicsContext,
    const FloatRect& sideRect,
    BoxSide side,
    BoxSide adjacentSide1,
    BoxSide adjacentSide2,
    const Path* path,
    bool antialias,
    Color color,
    BorderEdgeFlags completedEdges) const {
  const BorderEdge& edgeToRender = m_edges[side];
  DCHECK(edgeToRender.width());
  const BorderEdge& adjacentEdge1 = m_edges[adjacentSide1];
  const BorderEdge& adjacentEdge2 = m_edges[adjacentSide2];

  if (path) {
    MiterType miter1 = colorsMatchAtCorner(side, adjacentSide1, m_edges)
                           ? HardMiter
                           : SoftMiter;
    MiterType miter2 = colorsMatchAtCorner(side, adjacentSide2, m_edges)
                           ? HardMiter
                           : SoftMiter;

    GraphicsContextStateSaver stateSaver(graphicsContext);
    if (m_inner.isRenderable())
      clipBorderSidePolygon(graphicsContext, side, miter1, miter2);
    else
      clipBorderSideForComplexInnerPath(graphicsContext, side);
    float thickness =
        std::max(std::max(edgeToRender.width(), adjacentEdge1.width()),
                 adjacentEdge2.width());
    drawBoxSideFromPath(graphicsContext, LayoutRect(m_outer.rect()), *path,
                        edgeToRender.width(), thickness, side, color,
                        edgeToRender.borderStyle());
  } else {
    MiterType miter1 =
        computeMiter(side, adjacentSide1, completedEdges, antialias);
    MiterType miter2 =
        computeMiter(side, adjacentSide2, completedEdges, antialias);
    bool shouldClip = mitersRequireClipping(
        miter1, miter2, edgeToRender.borderStyle(), antialias);

    GraphicsContextStateSaver clipStateSaver(graphicsContext, shouldClip);
    if (shouldClip) {
      clipBorderSidePolygon(graphicsContext, side, miter1, miter2);

      // Miters are applied via clipping, no need to draw them.
      miter1 = miter2 = NoMiter;
    }

    ObjectPainter::drawLineForBoxSide(
        graphicsContext, sideRect.x(), sideRect.y(), sideRect.maxX(),
        sideRect.maxY(), side, color, edgeToRender.borderStyle(),
        miter1 != NoMiter ? roundf(adjacentEdge1.width()) : 0,
        miter2 != NoMiter ? roundf(adjacentEdge2.width()) : 0, antialias);
  }
}

void BoxBorderPainter::drawBoxSideFromPath(GraphicsContext& graphicsContext,
                                           const LayoutRect& borderRect,
                                           const Path& borderPath,
                                           float thickness,
                                           float drawThickness,
                                           BoxSide side,
                                           Color color,
                                           EBorderStyle borderStyle) const {
  if (thickness <= 0)
    return;

  if (borderStyle == BorderStyleDouble && thickness < 3)
    borderStyle = BorderStyleSolid;

  switch (borderStyle) {
    case BorderStyleNone:
    case BorderStyleHidden:
      return;
    case BorderStyleDotted:
    case BorderStyleDashed: {
      drawDashedDottedBoxSideFromPath(graphicsContext, borderPath, thickness,
                                      drawThickness, color, borderStyle);
      return;
    }
    case BorderStyleDouble: {
      drawDoubleBoxSideFromPath(graphicsContext, borderRect, borderPath,
                                thickness, drawThickness, side, color);
      return;
    }
    case BorderStyleRidge:
    case BorderStyleGroove: {
      drawRidgeGrooveBoxSideFromPath(graphicsContext, borderRect, borderPath,
                                     thickness, drawThickness, side, color,
                                     borderStyle);
      return;
    }
    case BorderStyleInset:
      if (side == BSTop || side == BSLeft)
        color = color.dark();
      break;
    case BorderStyleOutset:
      if (side == BSBottom || side == BSRight)
        color = color.dark();
      break;
    default:
      break;
  }

  graphicsContext.setStrokeStyle(NoStroke);
  graphicsContext.setFillColor(color);
  graphicsContext.drawRect(pixelSnappedIntRect(borderRect));
}

void BoxBorderPainter::drawDashedDottedBoxSideFromPath(
    GraphicsContext& graphicsContext,
    const Path& borderPath,
    float thickness,
    float drawThickness,
    Color color,
    EBorderStyle borderStyle) const {
  graphicsContext.setStrokeColor(color);

  // The stroke is doubled here because the provided path is the
  // outside edge of the border so half the stroke is clipped off.
  // The extra multiplier is so that the clipping mask can antialias
  // the edges to prevent jaggies.
  graphicsContext.setStrokeThickness(drawThickness * 2 * 1.1f);
  graphicsContext.setStrokeStyle(
      borderStyle == BorderStyleDashed ? DashedStroke : DottedStroke);

  // If the number of dashes that fit in the path is odd and non-integral
  // then we will have an awkwardly-sized dash at the end of the path. To
  // try to avoid that here, we simply make the whitespace dashes ever so
  // slightly bigger.
  // TODO(schenney): This code for setting up the dash effect is trying to
  // do the same thing as StrokeData::setupPaintDashPathEffect and should be
  // refactored to re-use that code. It would require
  // GraphicsContext::strokePath to take a length parameter.

  float dashLength =
      thickness * ((borderStyle == BorderStyleDashed) ? 3.0f : 1.0f);
  float gapLength = dashLength;
  float numberOfDashes = borderPath.length() / dashLength;
  // Don't try to show dashes if we have less than 2 dashes + 2 gaps.
  // FIXME: should do this test per side.
  if (numberOfDashes >= 4) {
    bool evenNumberOfFullDashes = !((int)numberOfDashes % 2);
    bool integralNumberOfDashes = !(numberOfDashes - (int)numberOfDashes);
    if (!evenNumberOfFullDashes && !integralNumberOfDashes) {
      float numberOfGaps = numberOfDashes / 2;
      gapLength += (dashLength / numberOfGaps);
    }

    DashArray lineDash;
    lineDash.push_back(dashLength);
    lineDash.push_back(gapLength);
    graphicsContext.setLineDash(lineDash, dashLength);
  }

  // FIXME: stroking the border path causes issues with tight corners:
  // https://bugs.webkit.org/show_bug.cgi?id=58711
  // Also, to get the best appearance we should stroke a path between the
  // two borders.
  graphicsContext.strokePath(borderPath);
}

void BoxBorderPainter::drawDoubleBoxSideFromPath(
    GraphicsContext& graphicsContext,
    const LayoutRect& borderRect,
    const Path& borderPath,
    float thickness,
    float drawThickness,
    BoxSide side,
    Color color) const {
  // Draw inner border line
  {
    GraphicsContextStateSaver stateSaver(graphicsContext);
    const LayoutRectOutsets innerInsets =
        doubleStripeInsets(m_edges, BorderEdge::DoubleBorderStripeInner);
    FloatRoundedRect innerClip = m_style.getRoundedInnerBorderFor(
        borderRect, innerInsets, m_includeLogicalLeftEdge,
        m_includeLogicalRightEdge);

    graphicsContext.clipRoundedRect(innerClip);
    drawBoxSideFromPath(graphicsContext, borderRect, borderPath, thickness,
                        drawThickness, side, color, BorderStyleSolid);
  }

  // Draw outer border line
  {
    GraphicsContextStateSaver stateSaver(graphicsContext);
    LayoutRect outerRect = borderRect;
    LayoutRectOutsets outerInsets =
        doubleStripeInsets(m_edges, BorderEdge::DoubleBorderStripeOuter);

    if (bleedAvoidanceIsClipping(m_bleedAvoidance)) {
      outerRect.inflate(1);
      outerInsets.setTop(outerInsets.top() - 1);
      outerInsets.setRight(outerInsets.right() - 1);
      outerInsets.setBottom(outerInsets.bottom() - 1);
      outerInsets.setLeft(outerInsets.left() - 1);
    }

    FloatRoundedRect outerClip = m_style.getRoundedInnerBorderFor(
        outerRect, outerInsets, m_includeLogicalLeftEdge,
        m_includeLogicalRightEdge);
    graphicsContext.clipOutRoundedRect(outerClip);
    drawBoxSideFromPath(graphicsContext, borderRect, borderPath, thickness,
                        drawThickness, side, color, BorderStyleSolid);
  }
}

void BoxBorderPainter::drawRidgeGrooveBoxSideFromPath(
    GraphicsContext& graphicsContext,
    const LayoutRect& borderRect,
    const Path& borderPath,
    float thickness,
    float drawThickness,
    BoxSide side,
    Color color,
    EBorderStyle borderStyle) const {
  EBorderStyle s1;
  EBorderStyle s2;
  if (borderStyle == BorderStyleGroove) {
    s1 = BorderStyleInset;
    s2 = BorderStyleOutset;
  } else {
    s1 = BorderStyleOutset;
    s2 = BorderStyleInset;
  }

  // Paint full border
  drawBoxSideFromPath(graphicsContext, borderRect, borderPath, thickness,
                      drawThickness, side, color, s1);

  // Paint inner only
  GraphicsContextStateSaver stateSaver(graphicsContext);
  int topWidth = m_edges[BSTop].usedWidth() / 2;
  int bottomWidth = m_edges[BSBottom].usedWidth() / 2;
  int leftWidth = m_edges[BSLeft].usedWidth() / 2;
  int rightWidth = m_edges[BSRight].usedWidth() / 2;

  FloatRoundedRect clipRect = m_style.getRoundedInnerBorderFor(
      borderRect,
      LayoutRectOutsets(-topWidth, -rightWidth, -bottomWidth, -leftWidth),
      m_includeLogicalLeftEdge, m_includeLogicalRightEdge);

  graphicsContext.clipRoundedRect(clipRect);
  drawBoxSideFromPath(graphicsContext, borderRect, borderPath, thickness,
                      drawThickness, side, color, s2);
}

void BoxBorderPainter::clipBorderSideForComplexInnerPath(
    GraphicsContext& graphicsContext,
    BoxSide side) const {
  graphicsContext.clip(calculateSideRectIncludingInner(m_outer, m_edges, side));
  FloatRoundedRect adjustedInnerRect =
      calculateAdjustedInnerBorder(m_inner, side);
  if (!adjustedInnerRect.isEmpty())
    graphicsContext.clipOutRoundedRect(adjustedInnerRect);
}

void BoxBorderPainter::clipBorderSidePolygon(GraphicsContext& graphicsContext,
                                             BoxSide side,
                                             MiterType firstMiter,
                                             MiterType secondMiter) const {
  DCHECK(firstMiter != NoMiter || secondMiter != NoMiter);

  FloatPoint edgeQuad[4];  // The boundary of the edge for fill
  FloatPoint boundQuad1;  // Point 1 of the rectilinear bounding box of EdgeQuad
  FloatPoint boundQuad2;  // Point 2 of the rectilinear bounding box of EdgeQuad

  const LayoutRect outerRect(m_outer.rect());
  const LayoutRect innerRect(m_inner.rect());

  // For each side, create a quad that encompasses all parts of that side that
  // may draw, including areas inside the innerBorder.
  //
  //         0----------------3
  //       3  \              /  0
  //       |\  1----------- 2  /|
  //       | 2                1 |
  //       | |                | |
  //       | |                | |
  //       | 1                2 |
  //       |/  2------------1  \|
  //       0  /              \  3
  //         3----------------0

  // Offset size and direction to expand clipping quad
  const static float kExtensionLength = 1e-1f;
  FloatSize extensionOffset;
  switch (side) {
    case BSTop:
      edgeQuad[0] = FloatPoint(outerRect.minXMinYCorner());
      edgeQuad[1] = FloatPoint(innerRect.minXMinYCorner());
      edgeQuad[2] = FloatPoint(innerRect.maxXMinYCorner());
      edgeQuad[3] = FloatPoint(outerRect.maxXMinYCorner());

      DCHECK(edgeQuad[0].y() == edgeQuad[3].y());
      DCHECK(edgeQuad[1].y() == edgeQuad[2].y());

      boundQuad1 = FloatPoint(edgeQuad[0].x(), edgeQuad[1].y());
      boundQuad2 = FloatPoint(edgeQuad[3].x(), edgeQuad[2].y());

      extensionOffset.setWidth(-kExtensionLength);
      extensionOffset.setHeight(0);

      if (!m_inner.getRadii().topLeft().isZero()) {
        findIntersection(
            edgeQuad[0], edgeQuad[1],
            FloatPoint(edgeQuad[1].x() + m_inner.getRadii().topLeft().width(),
                       edgeQuad[1].y()),
            FloatPoint(edgeQuad[1].x(),
                       edgeQuad[1].y() + m_inner.getRadii().topLeft().height()),
            edgeQuad[1]);
        DCHECK(boundQuad1.y() <= edgeQuad[1].y());
        boundQuad1.setY(edgeQuad[1].y());
        boundQuad2.setY(edgeQuad[1].y());
      }

      if (!m_inner.getRadii().topRight().isZero()) {
        findIntersection(
            edgeQuad[3], edgeQuad[2],
            FloatPoint(edgeQuad[2].x() - m_inner.getRadii().topRight().width(),
                       edgeQuad[2].y()),
            FloatPoint(
                edgeQuad[2].x(),
                edgeQuad[2].y() + m_inner.getRadii().topRight().height()),
            edgeQuad[2]);
        if (boundQuad1.y() < edgeQuad[2].y()) {
          boundQuad1.setY(edgeQuad[2].y());
          boundQuad2.setY(edgeQuad[2].y());
        }
      }
      break;

    case BSLeft:
      // Swap the order of adjacent edges to allow common code
      std::swap(firstMiter, secondMiter);
      edgeQuad[0] = FloatPoint(outerRect.minXMaxYCorner());
      edgeQuad[1] = FloatPoint(innerRect.minXMaxYCorner());
      edgeQuad[2] = FloatPoint(innerRect.minXMinYCorner());
      edgeQuad[3] = FloatPoint(outerRect.minXMinYCorner());

      DCHECK(edgeQuad[0].x() == edgeQuad[3].x());
      DCHECK(edgeQuad[1].x() == edgeQuad[2].x());

      boundQuad1 = FloatPoint(edgeQuad[1].x(), edgeQuad[0].y());
      boundQuad2 = FloatPoint(edgeQuad[2].x(), edgeQuad[3].y());

      extensionOffset.setWidth(0);
      extensionOffset.setHeight(kExtensionLength);

      if (!m_inner.getRadii().topLeft().isZero()) {
        findIntersection(
            edgeQuad[3], edgeQuad[2],
            FloatPoint(edgeQuad[2].x() + m_inner.getRadii().topLeft().width(),
                       edgeQuad[2].y()),
            FloatPoint(edgeQuad[2].x(),
                       edgeQuad[2].y() + m_inner.getRadii().topLeft().height()),
            edgeQuad[2]);
        DCHECK(boundQuad2.x() <= edgeQuad[2].x());
        boundQuad1.setX(edgeQuad[2].x());
        boundQuad2.setX(edgeQuad[2].x());
      }

      if (!m_inner.getRadii().bottomLeft().isZero()) {
        findIntersection(
            edgeQuad[0], edgeQuad[1],
            FloatPoint(
                edgeQuad[1].x() + m_inner.getRadii().bottomLeft().width(),
                edgeQuad[1].y()),
            FloatPoint(
                edgeQuad[1].x(),
                edgeQuad[1].y() - m_inner.getRadii().bottomLeft().height()),
            edgeQuad[1]);
        if (boundQuad1.x() < edgeQuad[1].x()) {
          boundQuad1.setX(edgeQuad[1].x());
          boundQuad2.setX(edgeQuad[1].x());
        }
      }
      break;

    case BSBottom:
      // Swap the order of adjacent edges to allow common code
      std::swap(firstMiter, secondMiter);
      edgeQuad[0] = FloatPoint(outerRect.maxXMaxYCorner());
      edgeQuad[1] = FloatPoint(innerRect.maxXMaxYCorner());
      edgeQuad[2] = FloatPoint(innerRect.minXMaxYCorner());
      edgeQuad[3] = FloatPoint(outerRect.minXMaxYCorner());

      DCHECK(edgeQuad[0].y() == edgeQuad[3].y());
      DCHECK(edgeQuad[1].y() == edgeQuad[2].y());

      boundQuad1 = FloatPoint(edgeQuad[0].x(), edgeQuad[1].y());
      boundQuad2 = FloatPoint(edgeQuad[3].x(), edgeQuad[2].y());

      extensionOffset.setWidth(kExtensionLength);
      extensionOffset.setHeight(0);

      if (!m_inner.getRadii().bottomLeft().isZero()) {
        findIntersection(
            edgeQuad[3], edgeQuad[2],
            FloatPoint(
                edgeQuad[2].x() + m_inner.getRadii().bottomLeft().width(),
                edgeQuad[2].y()),
            FloatPoint(
                edgeQuad[2].x(),
                edgeQuad[2].y() - m_inner.getRadii().bottomLeft().height()),
            edgeQuad[2]);
        DCHECK(boundQuad2.y() >= edgeQuad[2].y());
        boundQuad1.setY(edgeQuad[2].y());
        boundQuad2.setY(edgeQuad[2].y());
      }

      if (!m_inner.getRadii().bottomRight().isZero()) {
        findIntersection(
            edgeQuad[0], edgeQuad[1],
            FloatPoint(
                edgeQuad[1].x() - m_inner.getRadii().bottomRight().width(),
                edgeQuad[1].y()),
            FloatPoint(
                edgeQuad[1].x(),
                edgeQuad[1].y() - m_inner.getRadii().bottomRight().height()),
            edgeQuad[1]);
        if (boundQuad1.y() > edgeQuad[1].y()) {
          boundQuad1.setY(edgeQuad[1].y());
          boundQuad2.setY(edgeQuad[1].y());
        }
      }
      break;

    case BSRight:
      edgeQuad[0] = FloatPoint(outerRect.maxXMinYCorner());
      edgeQuad[1] = FloatPoint(innerRect.maxXMinYCorner());
      edgeQuad[2] = FloatPoint(innerRect.maxXMaxYCorner());
      edgeQuad[3] = FloatPoint(outerRect.maxXMaxYCorner());

      DCHECK(edgeQuad[0].x() == edgeQuad[3].x());
      DCHECK(edgeQuad[1].x() == edgeQuad[2].x());

      boundQuad1 = FloatPoint(edgeQuad[1].x(), edgeQuad[0].y());
      boundQuad2 = FloatPoint(edgeQuad[2].x(), edgeQuad[3].y());

      extensionOffset.setWidth(0);
      extensionOffset.setHeight(-kExtensionLength);

      if (!m_inner.getRadii().topRight().isZero()) {
        findIntersection(
            edgeQuad[0], edgeQuad[1],
            FloatPoint(edgeQuad[1].x() - m_inner.getRadii().topRight().width(),
                       edgeQuad[1].y()),
            FloatPoint(
                edgeQuad[1].x(),
                edgeQuad[1].y() + m_inner.getRadii().topRight().height()),
            edgeQuad[1]);
        DCHECK(boundQuad1.x() >= edgeQuad[1].x());
        boundQuad1.setX(edgeQuad[1].x());
        boundQuad2.setX(edgeQuad[1].x());
      }

      if (!m_inner.getRadii().bottomRight().isZero()) {
        findIntersection(
            edgeQuad[3], edgeQuad[2],
            FloatPoint(
                edgeQuad[2].x() - m_inner.getRadii().bottomRight().width(),
                edgeQuad[2].y()),
            FloatPoint(
                edgeQuad[2].x(),
                edgeQuad[2].y() - m_inner.getRadii().bottomRight().height()),
            edgeQuad[2]);
        if (boundQuad1.x() > edgeQuad[2].x()) {
          boundQuad1.setX(edgeQuad[2].x());
          boundQuad2.setX(edgeQuad[2].x());
        }
      }
      break;
  }

  if (firstMiter == secondMiter) {
    clipQuad(graphicsContext, edgeQuad, firstMiter == SoftMiter);
    return;
  }

  // If antialiasing settings for the first edge and second edge are different,
  // they have to be addressed separately. We do this by applying 2 clips, one
  // for each miter, with the appropriate anti-aliasing setting. Each clip uses
  // 3 sides of the quad rectilinear bounding box and a 4th side aligned with
  // the miter edge. We extend the clip in the miter direction to ensure overlap
  // as each edge is drawn.
  if (firstMiter != NoMiter) {
    FloatPoint clippingQuad[4];

    clippingQuad[0] = edgeQuad[0] + extensionOffset;
    findIntersection(edgeQuad[0], edgeQuad[1], boundQuad1, boundQuad2,
                     clippingQuad[1]);
    clippingQuad[1] += extensionOffset;
    clippingQuad[2] = boundQuad2;
    clippingQuad[3] = edgeQuad[3];

    clipQuad(graphicsContext, clippingQuad, firstMiter == SoftMiter);
  }

  if (secondMiter != NoMiter) {
    FloatPoint clippingQuad[4];

    clippingQuad[0] = edgeQuad[0];
    clippingQuad[1] = boundQuad1;
    findIntersection(edgeQuad[2], edgeQuad[3], boundQuad1, boundQuad2,
                     clippingQuad[2]);
    clippingQuad[2] -= extensionOffset;
    clippingQuad[3] = edgeQuad[3] - extensionOffset;

    clipQuad(graphicsContext, clippingQuad, secondMiter == SoftMiter);
  }
}

}  // namespace blink
