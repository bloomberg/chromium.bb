/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "core/paint/PaintLayerClipper.h"

#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutView.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/paint/GeometryMapper.h"

namespace blink {

static void adjustClipRectsForChildren(const LayoutBoxModelObject& layoutObject,
                                       ClipRects& clipRects) {
  EPosition position = layoutObject.styleRef().position();
  // A fixed object is essentially the root of its containing block hierarchy,
  // so when we encounter such an object, we reset our clip rects to the
  // fixedClipRect.
  if (position == EPosition::kFixed) {
    clipRects.setPosClipRect(clipRects.fixedClipRect());
    clipRects.setOverflowClipRect(clipRects.fixedClipRect());
    clipRects.setFixed(true);
  } else if (position == EPosition::kRelative) {
    clipRects.setPosClipRect(clipRects.overflowClipRect());
  } else if (position == EPosition::kAbsolute) {
    clipRects.setOverflowClipRect(clipRects.posClipRect());
  }
}

static void applyClipRects(const ClipRectsContext& context,
                           const LayoutBoxModelObject& layoutObject,
                           LayoutPoint offset,
                           ClipRects& clipRects) {
  DCHECK(layoutObject.isBox());
  const LayoutBox& box = *toLayoutBox(&layoutObject);

  DCHECK(box.shouldClipOverflow() || box.hasClip());
  LayoutView* view = box.view();
  DCHECK(view);
  if (clipRects.fixed() && &context.rootLayer->layoutObject() == view)
    offset -= LayoutSize(view->frameView()->getScrollOffset());

  if (box.shouldClipOverflow()) {
    ClipRect newOverflowClip =
        box.overflowClipRect(offset, context.overlayScrollbarClipBehavior);
    newOverflowClip.setHasRadius(box.styleRef().hasBorderRadius());
    clipRects.setOverflowClipRect(
        intersection(newOverflowClip, clipRects.overflowClipRect()));
    if (box.isPositioned())
      clipRects.setPosClipRect(
          intersection(newOverflowClip, clipRects.posClipRect()));
    if (box.canContainFixedPositionObjects())
      clipRects.setFixedClipRect(
          intersection(newOverflowClip, clipRects.fixedClipRect()));
    if (box.styleRef().containsPaint())
      clipRects.setPosClipRect(
          intersection(newOverflowClip, clipRects.posClipRect()));
  }
  if (box.hasClip()) {
    LayoutRect newClip = box.clipRect(offset);
    clipRects.setPosClipRect(intersection(newClip, clipRects.posClipRect()));
    clipRects.setOverflowClipRect(
        intersection(newClip, clipRects.overflowClipRect()));
    clipRects.setFixedClipRect(
        intersection(newClip, clipRects.fixedClipRect()));
  }
}

PaintLayerClipper::PaintLayerClipper(const PaintLayer& layer,
                                     bool usegeometryMapper)
    : m_layer(layer), m_useGeometryMapper(usegeometryMapper) {}

ClipRects* PaintLayerClipper::clipRectsIfCached(
    const ClipRectsContext& context) const {
  DCHECK(context.usesCache());
  if (!m_layer.clipRectsCache())
    return nullptr;
  ClipRectsCache::Entry& entry =
      m_layer.clipRectsCache()->get(context.cacheSlot());
  // FIXME: We used to ASSERT that we always got a consistent root layer.
  // We should add a test that has an inconsistent root. See
  // http://crbug.com/366118 for an example.
  if (context.rootLayer != entry.root)
    return 0;
#if DCHECK_IS_ON()
  DCHECK(entry.overlayScrollbarClipBehavior ==
         context.overlayScrollbarClipBehavior);
#endif
  return entry.clipRects.get();
}

ClipRects& PaintLayerClipper::storeClipRectsInCache(
    const ClipRectsContext& context,
    ClipRects* parentClipRects,
    const ClipRects& clipRects) const {
  ClipRectsCache::Entry& entry =
      m_layer.ensureClipRectsCache().get(context.cacheSlot());
  entry.root = context.rootLayer;
#if DCHECK_IS_ON()
  entry.overlayScrollbarClipBehavior = context.overlayScrollbarClipBehavior;
#endif
  if (parentClipRects) {
    // If our clip rects match the clip rects of our parent, we share storage.
    if (clipRects == *parentClipRects) {
      entry.clipRects = parentClipRects;
      return *parentClipRects;
    }
  }
  entry.clipRects = ClipRects::create(clipRects);
  return *entry.clipRects;
}

ClipRects& PaintLayerClipper::getClipRects(
    const ClipRectsContext& context) const {
  DCHECK(!m_useGeometryMapper);
  if (ClipRects* result = clipRectsIfCached(context))
    return *result;
  // Note that it's important that we call getClipRects on our parent
  // before we call calculateClipRects so that calculateClipRects will hit
  // the cache.
  ClipRects* parentClipRects = nullptr;
  if (context.rootLayer != &m_layer && m_layer.parent()) {
    parentClipRects =
        &PaintLayerClipper(*m_layer.parent(), false).getClipRects(context);
  }
  RefPtr<ClipRects> clipRects = ClipRects::create();
  calculateClipRects(context, *clipRects);
  return storeClipRectsInCache(context, parentClipRects, *clipRects);
}

void PaintLayerClipper::clearCache(ClipRectsCacheSlot cacheSlot) {
  if (cacheSlot == NumberOfClipRectsCacheSlots)
    m_layer.clearClipRectsCache();
  else if (ClipRectsCache* cache = m_layer.clipRectsCache())
    cache->clear(cacheSlot);
}

void PaintLayerClipper::clearClipRectsIncludingDescendants() {
  clearClipRectsIncludingDescendants(NumberOfClipRectsCacheSlots);
}

void PaintLayerClipper::clearClipRectsIncludingDescendants(
    ClipRectsCacheSlot cacheSlot) {
  std::stack<const PaintLayer*> layers;
  layers.push(&m_layer);

  while (!layers.empty()) {
    const PaintLayer* currentLayer = layers.top();
    layers.pop();
    PaintLayerClipper(*currentLayer, m_useGeometryMapper).clearCache(cacheSlot);
    for (const PaintLayer* layer = currentLayer->firstChild(); layer;
         layer = layer->nextSibling())
      layers.push(layer);
  }
}

LayoutRect PaintLayerClipper::localClipRect(
    const PaintLayer& clippingRootLayer) const {
  ClipRectsContext context(&clippingRootLayer, PaintingClipRects);
  if (m_useGeometryMapper) {
    ClipRect clipRect;
    calculateBackgroundClipRectWithGeometryMapper(context, clipRect);
    LayoutRect premappedRect = clipRect.rect();

    // The rect now needs to be transformed to the local space of this
    // PaintLayer.
    premappedRect.moveBy(context.rootLayer->layoutObject().paintOffset());

    const auto* clipRootLayerTransform = clippingRootLayer.layoutObject()
                                             .localBorderBoxProperties()
                                             ->transform();
    const auto* layerTransform =
        m_layer.layoutObject().localBorderBoxProperties()->transform();
    FloatRect clippedRectInLocalSpace(premappedRect);
    GeometryMapper::sourceToDestinationRect(
        clipRootLayerTransform, layerTransform, clippedRectInLocalSpace);
    clippedRectInLocalSpace.moveBy(
        -FloatPoint(m_layer.layoutObject().paintOffset()));

    return LayoutRect(clippedRectInLocalSpace);
  }

  LayoutRect layerBounds;
  ClipRect backgroundRect, foregroundRect;
  calculateRects(context, LayoutRect(LayoutRect::infiniteIntRect()),
                 layerBounds, backgroundRect, foregroundRect);

  LayoutRect clipRect = backgroundRect.rect();
  // TODO(chrishtr): avoid converting to IntRect and back.
  if (clipRect == LayoutRect(LayoutRect::infiniteIntRect()))
    return clipRect;

  LayoutPoint clippingRootOffset;
  m_layer.convertToLayerCoords(&clippingRootLayer, clippingRootOffset);
  clipRect.moveBy(-clippingRootOffset);

  return clipRect;
}

#ifdef CHECK_CLIP_RECTS
#define CHECK_RECTS_EQ(expected, actual)                                \
  do {                                                                  \
    bool matches =                                                      \
        (expected.isEmpty() && actual.isEmpty()) || expected == actual; \
    if (!matches) {                                                     \
      LOG(ERROR) << "Rects don't match for m_layer="                    \
                 << m_layer.layoutObject()->debugName()                 \
                 << " expected=" << expected.toString()                 \
                 << " actual=" << actual.toString();                    \
    }                                                                   \
  } while (false);
#endif

void PaintLayerClipper::calculateRectsWithGeometryMapper(
    const ClipRectsContext& context,
    const LayoutRect& paintDirtyRect,
    LayoutRect& layerBounds,
    ClipRect& backgroundRect,
    ClipRect& foregroundRect,
    const LayoutPoint* offsetFromRoot) const {
  // TODO(chrishtr): fix the underlying bug that causes this situation.
  if (!m_layer.layoutObject().paintProperties() &&
      !m_layer.layoutObject().localBorderBoxProperties()) {
    backgroundRect = ClipRect(LayoutRect(LayoutRect::infiniteIntRect()));
    foregroundRect = ClipRect(LayoutRect(LayoutRect::infiniteIntRect()));
  } else {
    calculateBackgroundClipRectWithGeometryMapper(context, backgroundRect);
    backgroundRect.move(context.subPixelAccumulation);
    backgroundRect.intersect(paintDirtyRect);

    calculateForegroundClipRectWithGeometryMapper(context, foregroundRect);
    foregroundRect.move(context.subPixelAccumulation);
    foregroundRect.intersect(paintDirtyRect);
  }

  LayoutPoint offset;
  if (offsetFromRoot)
    offset = *offsetFromRoot;
  else
    m_layer.convertToLayerCoords(context.rootLayer, offset);
  layerBounds = LayoutRect(offset, LayoutSize(m_layer.size()));

#ifdef CHECK_CLIP_RECTS
  ClipRect testBackgroundRect, testForegroundRect;
  LayoutRect testLayerBounds;
  PaintLayerClipper(m_layer, nullptr)
      .calculateRects(context, paintDirtyRect, testLayerBounds,
                      testBackgroundRect, testForegroundRect, offsetFromRoot);
  CHECK_RECTS_EQ(testBackgroundRect, backgroundRect);
  CHECK_RECTS_EQ(testForegroundRect, foregroundRect);
  CHECK_RECTS_EQ(testLayerBounds, layerBounds);
#endif
}

void PaintLayerClipper::calculateRects(
    const ClipRectsContext& context,
    const LayoutRect& paintDirtyRect,
    LayoutRect& layerBounds,
    ClipRect& backgroundRect,
    ClipRect& foregroundRect,
    const LayoutPoint* offsetFromRoot) const {
  if (m_useGeometryMapper) {
    calculateRectsWithGeometryMapper(context, paintDirtyRect, layerBounds,
                                     backgroundRect, foregroundRect,
                                     offsetFromRoot);
    return;
  }

  bool isClippingRoot = &m_layer == context.rootLayer;
  LayoutBoxModelObject& layoutObject = m_layer.layoutObject();

  if (!isClippingRoot && m_layer.parent()) {
    calculateBackgroundClipRect(context, backgroundRect);
    backgroundRect.move(context.subPixelAccumulation);
    backgroundRect.intersect(paintDirtyRect);
  } else {
    backgroundRect = paintDirtyRect;
  }

  foregroundRect = backgroundRect;

  LayoutPoint offset;
  if (offsetFromRoot)
    offset = *offsetFromRoot;
  else
    m_layer.convertToLayerCoords(context.rootLayer, offset);
  layerBounds = LayoutRect(offset, LayoutSize(m_layer.size()));

  // Update the clip rects that will be passed to child layers.
  if (shouldClipOverflow(context)) {
    LayoutRect overflowAndClipRect =
        toLayoutBox(layoutObject)
            .overflowClipRect(offset, context.overlayScrollbarClipBehavior);
    foregroundRect.intersect(overflowAndClipRect);
    if (layoutObject.styleRef().hasBorderRadius())
      foregroundRect.setHasRadius(true);

    // FIXME: Does not do the right thing with columns yet, since we don't yet
    // factor in the individual column boxes as overflow.

    LayoutRect layerBoundsWithVisualOverflow = localVisualRect();
    layerBoundsWithVisualOverflow.moveBy(offset);
    backgroundRect.intersect(layerBoundsWithVisualOverflow);
  }

  // CSS clip (different than clipping due to overflow) can clip to any box,
  // even if it falls outside of the border box.
  if (layoutObject.hasClip()) {
    // Clip applies to *us* as well, so go ahead and update the damageRect.
    LayoutRect newPosClip = toLayoutBox(layoutObject).clipRect(offset);
    backgroundRect.intersect(newPosClip);
    foregroundRect.intersect(newPosClip);
  }
}

void PaintLayerClipper::calculateClipRects(const ClipRectsContext& context,
                                           ClipRects& clipRects) const {
  const LayoutBoxModelObject& layoutObject = m_layer.layoutObject();
  if (!m_layer.parent() &&
      !RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    // The root layer's clip rect is always infinite.
    clipRects.reset(LayoutRect(LayoutRect::infiniteIntRect()));
    return;
  }

  bool isClippingRoot = &m_layer == context.rootLayer;

  // For transformed layers, the root layer was shifted to be us, so there is no
  // need to examine the parent. We want to cache clip rects with us as the
  // root.
  PaintLayer* parentLayer = !isClippingRoot ? m_layer.parent() : nullptr;
  // Ensure that our parent's clip has been calculated so that we can examine
  // the values.
  if (parentLayer) {
    PaintLayerClipper(*parentLayer, m_useGeometryMapper)
        .getOrCalculateClipRects(context, clipRects);
  } else {
    clipRects.reset(LayoutRect(LayoutRect::infiniteIntRect()));
  }

  adjustClipRectsForChildren(layoutObject, clipRects);

  if (shouldClipOverflow(context) || layoutObject.hasClip()) {
    // This offset cannot use convertToLayerCoords, because sometimes our
    // rootLayer may be across some transformed layer boundary, for example, in
    // the PaintLayerCompositor overlapMap, where clipRects are needed in view
    // space.
    applyClipRects(context, layoutObject,
                   LayoutPoint(layoutObject.localToAncestorPoint(
                       FloatPoint(), &context.rootLayer->layoutObject())),
                   clipRects);
  }
}

static ClipRect backgroundClipRectForPosition(const ClipRects& parentRects,
                                              EPosition position) {
  if (position == EPosition::kFixed)
    return parentRects.fixedClipRect();

  if (position == EPosition::kAbsolute)
    return parentRects.posClipRect();

  return parentRects.overflowClipRect();
}

void PaintLayerClipper::calculateBackgroundClipRectWithGeometryMapper(
    const ClipRectsContext& context,
    ClipRect& output) const {
  DCHECK(m_useGeometryMapper);
  PropertyTreeState sourcePropertyTreeState(nullptr, nullptr, nullptr);
  PropertyTreeState destinationPropertyTreeState(nullptr, nullptr, nullptr);
  initializeCommonClipRectState(context, sourcePropertyTreeState,
                                destinationPropertyTreeState);

  if (&m_layer != context.rootLayer) {
    auto* ancestorProperties =
        context.rootLayer->layoutObject().paintProperties();
    const auto* ancestorOverflowClip =
        ancestorProperties ? ancestorProperties->overflowClip() : nullptr;
    // Set the clip of |destinationPropertyTreeState| to be inside the
    // ancestor's overflow clip, so that that clip is not applied.
    if (context.respectOverflowClip == IgnoreOverflowClip &&
        ancestorOverflowClip)
      destinationPropertyTreeState.setClip(ancestorOverflowClip);
  }

  // The background rect applies all clips *above* m_layer, but not the overflow
  // clip of m_layer. It also applies a clip to the total painting bounds
  // of m_layer, because nothing in m_layer or its children within the clip can
  // paint outside of those bounds.
  // The total painting bounds includes any visual overflow (such as shadow) and
  // filter bounds.
  //
  // TODO(chrishtr): sourceToDestinationVisualRect and
  // sourceToDestinationClipRect may not compute tight results in the presence
  // of transforms. Tight results are required for most use cases of these
  // rects, so we should add methods to GeometryMapper that guarantee there
  // are tight results, or else signal an error.
  if (shouldClipOverflow(context)) {
    FloatClipRect clipRect((FloatRect(localVisualRect())));
    clipRect.moveBy(FloatPoint(m_layer.layoutObject().paintOffset()));
    GeometryMapper::sourceToDestinationVisualRect(
        sourcePropertyTreeState, destinationPropertyTreeState, clipRect);
    output.setRect(clipRect);
  } else {
    const FloatClipRect& clippedRectInRootLayerSpace =
        GeometryMapper::sourceToDestinationClipRect(
            sourcePropertyTreeState, destinationPropertyTreeState);
    output.setRect(clippedRectInRootLayerSpace);
  }

  output.moveBy(-context.rootLayer->layoutObject().paintOffset());
}

void PaintLayerClipper::initializeCommonClipRectState(
    const ClipRectsContext& context,
    PropertyTreeState& sourcePropertyTreeState,
    PropertyTreeState& destinationPropertyTreeState) const {
  DCHECK(m_useGeometryMapper);
  DCHECK(m_layer.layoutObject().localBorderBoxProperties());

  sourcePropertyTreeState = *m_layer.layoutObject().localBorderBoxProperties();
  DCHECK(context.rootLayer->layoutObject().localBorderBoxProperties());
  destinationPropertyTreeState =
      *context.rootLayer->layoutObject().localBorderBoxProperties();

  auto* ancestorProperties =
      context.rootLayer->layoutObject().paintProperties();
  const auto* ancestorCssClip =
      ancestorProperties ? ancestorProperties->cssClip() : nullptr;
  // CSS clip of the root is always applied.
  if (ancestorCssClip) {
    DCHECK(destinationPropertyTreeState.clip() ==
           ancestorProperties->cssClip());
    destinationPropertyTreeState.setClip(ancestorCssClip->parent());
  }
}

void PaintLayerClipper::calculateForegroundClipRectWithGeometryMapper(
    const ClipRectsContext& context,
    ClipRect& output) const {
  DCHECK(m_useGeometryMapper);
  PropertyTreeState sourcePropertyTreeState(nullptr, nullptr, nullptr);
  PropertyTreeState destinationPropertyTreeState(nullptr, nullptr, nullptr);
  initializeCommonClipRectState(context, sourcePropertyTreeState,
                                destinationPropertyTreeState);
  const auto* properties = m_layer.layoutObject().paintProperties();

  if (&m_layer == context.rootLayer) {
    // Set the overflow clip for |sourcePropertyTreeState| so that it differs
    // from |destinationPropertyTreeState| in its clip.
    if (context.respectOverflowClip == RespectOverflowClip && properties &&
        properties->overflowClip())
      sourcePropertyTreeState.setClip(properties->overflowClip());
  } else {
    const auto* ancestorProperties =
        context.rootLayer->layoutObject().paintProperties();

    // Set the clip of |destinationPropertyTreeState| to be inside the
    // ancestor's overflow clip, so that that clip is not applied.
    if (context.respectOverflowClip == IgnoreOverflowClip &&
        ancestorProperties && ancestorProperties->overflowClip())
      destinationPropertyTreeState.setClip(ancestorProperties->overflowClip());

    // Set the overflow clip for |sourcePropertyTreeState| so that it differs
    // from destinationPropertyTreeState| in its clip.
    if (properties && properties->overflowClip())
      sourcePropertyTreeState.setClip(properties->overflowClip());
  }

  const FloatClipRect& clippedRectInRootLayerSpace =
      GeometryMapper::sourceToDestinationClipRect(sourcePropertyTreeState,
                                                  destinationPropertyTreeState);
  output.setRect(clippedRectInRootLayerSpace);

  output.moveBy(-context.rootLayer->layoutObject().paintOffset());
}

LayoutRect PaintLayerClipper::localVisualRect() const {
  const LayoutObject& layoutObject = m_layer.layoutObject();
  // The LayoutView is special since its overflow clipping rect may be larger
  // than its box rect (crbug.com/492871).
  LayoutRect layerBoundsWithVisualOverflow =
      layoutObject.isLayoutView()
          ? toLayoutView(layoutObject).viewRect()
          : toLayoutBox(layoutObject).visualOverflowRect();
  toLayoutBox(layoutObject)
      .flipForWritingMode(
          // PaintLayer are in physical coordinates, so the overflow has to be
          // flipped.
          layerBoundsWithVisualOverflow);
  if (m_layer.paintsWithFilters()) {
    layerBoundsWithVisualOverflow =
        m_layer.mapLayoutRectForFilter(layerBoundsWithVisualOverflow);
  }
  return layerBoundsWithVisualOverflow;
}

void PaintLayerClipper::calculateBackgroundClipRect(
    const ClipRectsContext& context,
    ClipRect& output) const {
  if (m_useGeometryMapper) {
    // TODO(chrishtr): fix the underlying bug that causes this situation.
    if (!m_layer.layoutObject().paintProperties() &&
        !m_layer.layoutObject().localBorderBoxProperties()) {
      output.setRect(FloatClipRect());
      return;
    }

    calculateBackgroundClipRectWithGeometryMapper(context, output);
#ifdef CHECK_CLIP_RECTS
    ClipRect testBackgroundClipRect =
        PaintLayerClipper(m_layer, nullptr).backgroundClipRect(context);
    CHECK_RECTS_EQ(testBackgroundClipRect, output);
#endif
    return;
  }
  DCHECK(m_layer.parent());
  LayoutView* layoutView = m_layer.layoutObject().view();
  DCHECK(layoutView);

  RefPtr<ClipRects> parentClipRects = ClipRects::create();
  if (&m_layer == context.rootLayer) {
    parentClipRects->reset(LayoutRect(LayoutRect::infiniteIntRect()));
  } else {
    PaintLayerClipper(*m_layer.parent(), m_useGeometryMapper)
        .getOrCalculateClipRects(context, *parentClipRects);
  }

  output = backgroundClipRectForPosition(
      *parentClipRects, m_layer.layoutObject().styleRef().position());

  // Note: infinite clipRects should not be scrolled here, otherwise they will
  // accidentally no longer be considered infinite.
  if (parentClipRects->fixed() &&
      &context.rootLayer->layoutObject() == layoutView &&
      output != LayoutRect(LayoutRect::infiniteIntRect()))
    output.move(LayoutSize(layoutView->frameView()->getScrollOffset()));
}

void PaintLayerClipper::getOrCalculateClipRects(const ClipRectsContext& context,
                                                ClipRects& clipRects) const {
  DCHECK(!m_useGeometryMapper);

  if (context.usesCache())
    clipRects = getClipRects(context);
  else
    calculateClipRects(context, clipRects);
}

bool PaintLayerClipper::shouldClipOverflow(
    const ClipRectsContext& context) const {
  if (!m_layer.layoutObject().isBox())
    return false;
  const LayoutBox& box = toLayoutBox(m_layer.layoutObject());

  if (!shouldRespectOverflowClip(context))
    return false;

  return box.shouldClipOverflow();
}

bool PaintLayerClipper::shouldRespectOverflowClip(
    const ClipRectsContext& context) const {
  if (&m_layer != context.rootLayer)
    return true;

  if (context.respectOverflowClip == IgnoreOverflowClip)
    return false;

  if (m_layer.isRootLayer() &&
      context.respectOverflowClipForViewport == IgnoreOverflowClip)
    return false;

  return true;
}

ClipRects& PaintLayerClipper::paintingClipRects(
    const PaintLayer* rootLayer,
    ShouldRespectOverflowClipType respectOverflowClip,
    const LayoutSize& subpixelAccumulation) const {
  DCHECK(!m_useGeometryMapper);
  ClipRectsContext context(rootLayer, PaintingClipRects,
                           IgnorePlatformOverlayScrollbarSize,
                           subpixelAccumulation);
  if (respectOverflowClip == IgnoreOverflowClip)
    context.setIgnoreOverflowClip();
  return getClipRects(context);
}

}  // namespace blink
