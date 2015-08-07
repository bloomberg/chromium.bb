/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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

#include "config.h"
#include "core/paint/DeprecatedPaintLayerClipper.h"

#include "core/frame/Settings.h"
#include "core/layout/LayoutView.h"
#include "core/paint/DeprecatedPaintLayer.h"

namespace blink {

DeprecatedPaintLayerClipper::DeprecatedPaintLayerClipper(LayoutBoxModelObject& layoutObject)
    : m_layoutObject(layoutObject)
{
}

void DeprecatedPaintLayerClipper::clearClipRectsIncludingDescendants()
{
    for (DeprecatedPaintLayer* layer = m_layoutObject.layer()->firstChild(); layer; layer = layer->nextSibling()) {
        for (int i = 0; i < NumberOfClipRectsCacheSlots; i++)
            layer->clipper().m_clips[i] = nullptr;
        layer->clipper().clearClipRectsIncludingDescendants();
    }
}

void DeprecatedPaintLayerClipper::clearClipRectsIncludingDescendants(ClipRectsCacheSlot cacheSlot)
{
    for (DeprecatedPaintLayer* layer = m_layoutObject.layer()->firstChild(); layer; layer = layer->nextSibling()) {
        layer->clipper().m_clips[cacheSlot] = nullptr;
        layer->clipper().clearClipRectsIncludingDescendants(cacheSlot);
    }
}

LayoutRect DeprecatedPaintLayerClipper::childrenClipRect() const
{
    // FIXME: border-radius not accounted for.
    // FIXME: Flow thread based columns not accounted for.
    DeprecatedPaintLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    // Need to use uncached clip rects, because the value of 'dontClipToOverflow' may be different from the painting path (<rdar://problem/11844909>).
    ClipRectsContext context(clippingRootLayer, UncachedClipRects);
    calculateRects(context, LayoutRect(m_layoutObject.view()->unscaledDocumentRect()), layerBounds, backgroundRect, foregroundRect, outlineRect);
    return LayoutRect(clippingRootLayer->layoutObject()->localToAbsoluteQuad(FloatQuad(foregroundRect.rect())).enclosingBoundingBox());
}

LayoutRect DeprecatedPaintLayerClipper::localClipRect() const
{
    // FIXME: border-radius not accounted for.
    DeprecatedPaintLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    ClipRectsContext context(clippingRootLayer, PaintingClipRects);
    calculateRects(context, LayoutRect(LayoutRect::infiniteIntRect()), layerBounds, backgroundRect, foregroundRect, outlineRect);

    LayoutRect clipRect = backgroundRect.rect();
    if (clipRect == LayoutRect::infiniteIntRect())
        return clipRect;

    LayoutPoint clippingRootOffset;
    m_layoutObject.layer()->convertToLayerCoords(clippingRootLayer, clippingRootOffset);
    clipRect.moveBy(-clippingRootOffset);

    return clipRect;
}

void DeprecatedPaintLayerClipper::calculateRects(const ClipRectsContext& context, const LayoutRect& paintDirtyRect, LayoutRect& layerBounds,
    ClipRect& backgroundRect, ClipRect& foregroundRect, ClipRect& outlineRect, const LayoutPoint* offsetFromRoot) const
{
    bool isClippingRoot = m_layoutObject.layer() == context.rootLayer;

    if (!isClippingRoot && m_layoutObject.layer()->parent()) {
        backgroundRect = backgroundClipRect(context);
        backgroundRect.move(roundedIntSize(context.subPixelAccumulation));
        backgroundRect.intersect(paintDirtyRect);
    } else {
        backgroundRect = paintDirtyRect;
    }

    foregroundRect = backgroundRect;
    outlineRect = backgroundRect;

    LayoutPoint offset;
    if (offsetFromRoot)
        offset = *offsetFromRoot;
    else
        m_layoutObject.layer()->convertToLayerCoords(context.rootLayer, offset);
    layerBounds = LayoutRect(offset, LayoutSize(m_layoutObject.layer()->size()));

    // Update the clip rects that will be passed to child layers.
    if (m_layoutObject.hasOverflowClip()) {
        // This layer establishes a clip of some kind.
        if (shouldRespectOverflowClip(context)) {
            foregroundRect.intersect(toLayoutBox(m_layoutObject).overflowClipRect(offset, context.scrollbarRelevancy));
            if (m_layoutObject.style()->hasBorderRadius())
                foregroundRect.setHasRadius(true);
        }

        // If we establish an overflow clip at all, then go ahead and make sure our background
        // rect is intersected with our layer's bounds including our visual overflow,
        // since any visual overflow like box-shadow or border-outset is not clipped by overflow:auto/hidden.
        if (toLayoutBox(m_layoutObject).hasVisualOverflow()) {
            // FIXME: Perhaps we should be propagating the borderbox as the clip rect for children, even though
            //        we may need to inflate our clip specifically for shadows or outsets.
            // FIXME: Does not do the right thing with columns yet, since we don't yet factor in the
            // individual column boxes as overflow.
            LayoutRect layerBoundsWithVisualOverflow = toLayoutBox(m_layoutObject).visualOverflowRect();
            toLayoutBox(m_layoutObject).flipForWritingMode(layerBoundsWithVisualOverflow); // DeprecatedPaintLayer are in physical coordinates, so the overflow has to be flipped.
            layerBoundsWithVisualOverflow.moveBy(offset);
            if (shouldRespectOverflowClip(context))
                backgroundRect.intersect(layerBoundsWithVisualOverflow);
        } else {
            // The LayoutView is special since its overflow clipping rect may be larger than its box rect (crbug.com/492871).
            LayoutRect bounds = m_layoutObject.isLayoutView() ? toLayoutView(m_layoutObject).viewRect() : toLayoutBox(m_layoutObject).borderBoxRect();
            bounds.moveBy(offset);
            if (shouldRespectOverflowClip(context))
                backgroundRect.intersect(bounds);
        }
    }

    // CSS clip (different than clipping due to overflow) can clip to any box, even if it falls outside of the border box.
    if (m_layoutObject.hasClip()) {
        // Clip applies to *us* as well, so go ahead and update the damageRect.
        LayoutRect newPosClip = toLayoutBox(m_layoutObject).clipRect(offset);
        backgroundRect.intersect(newPosClip);
        foregroundRect.intersect(newPosClip);
        outlineRect.intersect(newPosClip);
    }
}

static void precalculate(const ClipRectsContext& context)
{
    bool isComputingPaintingRect = context.isComputingPaintingRect();
    ClipRectComputationState rects;
    const DeprecatedPaintLayer* rootLayer = context.rootLayer;
    if (isComputingPaintingRect) {
        // Starting arbitrarily in the tree when calculating painting clipRects will
        // not allow you to fill in all layers cache because some intermediate layer
        // may need clips with respect to an ancestor further up. For efficiency we
        // start at the top in order to fill in the cache for all layers.
        rootLayer = context.rootLayer->layoutObject()->view()->layer();
        rects.stackingContextClipRects.setRootLayer(rootLayer);
    }
    rects.currentClipRects.setRootLayer(rootLayer);

    rootLayer->clipper().calculateClipRects(context, rects);
}


// Calculates clipRect for each element in the section of the tree starting with context.rootLayer
// For painting, context.rootLayer is ignored and the entire tree is calculated.
// TODO(chadarmstrong): When using the cache context shouldn't be able to specify the rootLayer. This affects
// what is stored in the 5 caches, and should therefore be enforced internally. Currently
// different callers have different ideas of what the rootLayer should be for a particular
// cacheSlot, which can fill the cache with bad data. If this can be made consistent it should
// be possible to eliminate rootLayer.
ClipRect DeprecatedPaintLayerClipper::backgroundClipRect(const ClipRectsContext& context) const
{
    ASSERT(m_layoutObject.layer()->parent());
    ASSERT(m_layoutObject.view());

    // Ideally backgroundClipRect would not be called with itself as the rootLayer.
    // This behavior can be seen in the following test
    // LayoutTests/compositing/squashing/abspos-under-abspos-overflow-scroll.html
    if (m_layoutObject.layer() == context.rootLayer)
        return LayoutRect(LayoutRect::infiniteIntRect());

    // TODO(chadarmstrong): If possible, all queries should use one of the clipRectsCacheSlots.
    // Without caching this operation involves walking all the way to rootLayer.
    if (!context.usesCache())
        return uncachedBackgroundClipRect(context);

    // TODO(chadarmstrong): precalculation for painting should be moved to updateAllLifecyclePhasesInternal
    // and precalculation could be done for all hit testing. This would let us avoid clearing the cache
    if (!m_clips[context.cacheSlot()])
        precalculate(context);

    // TODO(chadarmstrong): eliminate this if possible.
    // It is necessary only because of a seemingly atypical use of rootLayer that
    // can be seen in LayoutTests/fullscreen/full-screen-line-boxes-crash.html and
    // fast/block/multicol-paint-invalidation-assert.html.
    if (!m_clips[context.cacheSlot()])
        return uncachedBackgroundClipRect(context);
    // As soon as crbug.com/517173 is resolved this assert should be enabled in place of the check
    // ASSERT(m_clips[context.cacheSlot()]->rootLayer() == context.rootLayer);
    if (m_clips[context.cacheSlot()]->rootLayer() != context.rootLayer)
        return uncachedBackgroundClipRect(context);


    return *m_clips[context.cacheSlot()];
}

static bool shouldStopClipRectCalculation(LayoutBoxModelObject& layoutObject, const ClipRectsContext& context)
{
    // The entire tree is calculated for both painting and absolutClipRects
    if (context.cacheSlot() == PaintingClipRectsIgnoringOverflowClip || context.cacheSlot() == PaintingClipRects || context.cacheSlot() == AbsoluteClipRects)
        return false;
    DeprecatedPaintLayer* layer = layoutObject.layer();
    return layer->transform() || layer == layer->enclosingPaginationLayer();
}

// Updates clipRects so that descendants can calculate
// clipping relative to different root layers.
static void resetPaintRects(LayoutBoxModelObject& layoutObject, ClipRectComputationState& rects, ClipRectsCacheSlot slot)
{
    const DeprecatedPaintLayer* layer = layoutObject.layer();
    if (layer->isPaintInvalidationContainer() || layer->transform()) {
        // If this element is hardware accelerated, we let the compositor handle the
        // clipping (in case we want to paint bigger than a scrollable area for smooth
        // scrolling) so we reset the clip rects.
        // If the element has a transform, the clip rect could become a quad so we
        // reset the layer and let the paint code apply the CTM during paint to
        // transform the clip during paint.
        rects.currentClipRects.reset(LayoutRect(LayoutRect::infiniteIntRect()));
        rects.currentClipRects.setRootLayer(layoutObject.layer());
        rects.currentClipRects.setFixed(false);
    }
    if (layer->stackingNode()->isStackingContext()) {
        rects.stackingContextClipRects = rects.currentClipRects;
    }
}

void DeprecatedPaintLayerClipper::addClipsFromThisObject(const ClipRectsContext& context, ClipRects& clipRects) const
{
    LayoutView* view = m_layoutObject.view();
    ASSERT(view);
    // This offset cannot use convertToLayerCoords, because sometimes our rootLayer may be across
    // some transformed layer boundary, for example, in the DeprecatedPaintLayerCompositor overlapMap, where
    // clipRects are needed in view space.
    LayoutPoint offset = roundedLayoutPoint(m_layoutObject.localToContainerPoint(FloatPoint(), context.rootLayer->layoutObject()));
    if (clipRects.fixed() && context.rootLayer->layoutObject() == view)
        offset -= toIntSize(view->frameView()->scrollPosition());

    // The condition for hasClip here seems wrong. See https://crbug.com/504577
    // (overflowClips can be applied even when shouldRespectOverflowClip returns false)
    if (m_layoutObject.hasOverflowClip() && (shouldRespectOverflowClip(context) || m_layoutObject.hasClip())) {
        ClipRect newOverflowClip = toLayoutBox(m_layoutObject).overflowClipRect(offset, context.scrollbarRelevancy);
        newOverflowClip.setHasRadius(m_layoutObject.style()->hasBorderRadius());
        clipRects.setOverflowClipRect(intersection(newOverflowClip, clipRects.overflowClipRect()));
        if (m_layoutObject.isPositioned())
            clipRects.setPosClipRect(intersection(newOverflowClip, clipRects.posClipRect()));
        if (m_layoutObject.isLayoutView())
            clipRects.setFixedClipRect(intersection(newOverflowClip, clipRects.fixedClipRect()));
    }

    if (m_layoutObject.hasClip())
        clipRects.setFixedClipRect(intersection(toLayoutBox(m_layoutObject).clipRect(offset), clipRects.fixedClipRect()));
}

void DeprecatedPaintLayerClipper::calculateClipRects(const ClipRectsContext& context, ClipRectComputationState& rects) const
{
    bool isComputingPaintingRect = context.isComputingPaintingRect();

    // backgroundClipRect does not include clips from yourself
    ClipRectsContext current = context;
    current.rootLayer = rects.currentClipRects.rootLayer();
    addClipsFromThisObject(current, rects.currentClipRects);
    if (isComputingPaintingRect) {
        current.rootLayer = rects.stackingContextClipRects.rootLayer();
        addClipsFromThisObject(current, rects.stackingContextClipRects);
    }

    for (DeprecatedPaintLayer* child = m_layoutObject.layer()->firstChild(); child; child = child->nextSibling()) {
        child->clipper().setClipRect(context, rects);
    }
}

// This function propagate the appropriate clip down to descendants. It is required
// as CSS overflow clips are inherited based on the containing blocks chain:
// ['overflow'] "affects the clipping of all of the element's content
// except any descendant elements (and their respective content and descendants)
// whose containing block is the viewport or an ancestor of the element."
void DeprecatedPaintLayerClipper::updateClipRectBasedOnPosition(ClipRects* clipRects) const
{
    switch (m_layoutObject.style()->position()) {
    case FixedPosition:
        // Clip is applied to all descendants but overflow does not propogate to fixed
        // position elements.
        clipRects->setPosClipRect(ClipRect(LayoutRect(LayoutRect::infiniteIntRect())));
        clipRects->setOverflowClipRect(ClipRect(LayoutRect(LayoutRect::infiniteIntRect())));
        clipRects->setFixed(true);
        break;
    case AbsolutePosition:
        // Overflow clips from staticly positioned elements can be escaped by absolute and fixed
        // position elements.
        clipRects->setOverflowClipRect(clipRects->posClipRect());
        break;
    case RelativePosition:
        // Overflow clips that apply to a relative position element are not escaped by absolute
        // position elements further down the tree.  (Because the relative position element is a
        // containing block.
        clipRects->setPosClipRect(clipRects->overflowClipRect());
        break;
    case StaticPosition:
    case StickyPosition:
        // TODO(flakr): Position sticky should inherit the clip like relative position does (crbug.com/231752).
        break;
    }
}

void DeprecatedPaintLayerClipper::setClipRect(const ClipRectsContext& context, const ClipRectComputationState& parentRects) const
{
    bool isComputingPaintingRect = context.isComputingPaintingRect();
    ClipRectComputationState rects;
    rects.currentClipRects = parentRects.currentClipRects;
    if (isComputingPaintingRect)
        rects.stackingContextClipRects = parentRects.stackingContextClipRects;
    updateClipRectBasedOnPosition(&rects.currentClipRects);
    if (isComputingPaintingRect)
        updateClipRectBasedOnPosition(&rects.stackingContextClipRects);
    ClipRects* clipRects;
    if (isComputingPaintingRect && (m_layoutObject.isPositioned() || m_layoutObject.layer()->transform()))
        clipRects = &rects.stackingContextClipRects;
    else
        clipRects = &rects.currentClipRects;

    m_clips[context.cacheSlot()] = ClipRect::create(intersection(clipRects->overflowClipRect(), clipRects->fixedClipRect()));
    m_clips[context.cacheSlot()]->setRootLayer(clipRects->rootLayer());
    // Note: infinite clipRects should not be scrolled here, otherwise they will accidentally no longer be considered infinite.
    if (clipRects->fixed() && clipRects->rootLayer()->layoutObject() == m_layoutObject.view() && *m_clips[context.cacheSlot()] != LayoutRect(LayoutRect::infiniteIntRect()))
        m_clips[context.cacheSlot()]->move(toIntSize(m_layoutObject.view()->frameView()->scrollPosition()));

    if (shouldStopClipRectCalculation(m_layoutObject, context))
        return;
    if (isComputingPaintingRect)
        resetPaintRects(m_layoutObject, rects, context.cacheSlot());

    calculateClipRects(context, rects);
}

ClipRect DeprecatedPaintLayerClipper::uncachedBackgroundClipRect(const ClipRectsContext& context) const
{
    ClipRects clipRects;
    m_layoutObject.layer()->parent()->clipper().uncachedCalculateClipRects(context, clipRects);

    updateClipRectBasedOnPosition(&clipRects);
    ClipRect result(intersection(clipRects.overflowClipRect(), clipRects.fixedClipRect()));

    // Note: infinite clipRects should not be scrolled here, otherwise they will accidentally no longer be considered infinite.
    if (clipRects.fixed() && context.rootLayer->layoutObject() == m_layoutObject.view() && result != LayoutRect(LayoutRect::infiniteIntRect()))
        result.move(toIntSize(m_layoutObject.view()->frameView()->scrollPosition()));

    return result;
}

void DeprecatedPaintLayerClipper::uncachedCalculateClipRects(const ClipRectsContext& context, ClipRects& clipRects) const
{
    bool rootLayerScrolls = m_layoutObject.document().settings() && m_layoutObject.document().settings()->rootLayerScrolls();
    if (!m_layoutObject.layer()->parent() && !rootLayerScrolls) {
        // The root layer's clip rect is always infinite.
        clipRects.reset(LayoutRect(LayoutRect::infiniteIntRect()));
        return;
    }

    bool isClippingRoot = m_layoutObject.layer() == context.rootLayer;

    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent. We want to cache clip rects with us as the root.
    DeprecatedPaintLayer* parentLayer = !isClippingRoot ? m_layoutObject.layer()->parent() : 0;

    // Ensure that our parent's clip has been calculated so that we can examine the values.
    if (parentLayer) {
        parentLayer->clipper().uncachedCalculateClipRects(context, clipRects);
    } else {
        clipRects.reset(LayoutRect(LayoutRect::infiniteIntRect()));
    }

    updateClipRectBasedOnPosition(&clipRects);
    addClipsFromThisObject(context, clipRects);
}

// TODO(chadarmstrong): Clipping roots should be consistent for a given clipRectsCacheSlot
// As things are, different callers clash over the same slot because they use
// different root layers.
DeprecatedPaintLayer* DeprecatedPaintLayerClipper::clippingRootForPainting() const
{
    const DeprecatedPaintLayer* current = m_layoutObject.layer();
    // FIXME: getting rid of current->hasCompositedDeprecatedPaintLayerMapping() here breaks the
    // compositing/backing/no-backing-for-clip.html layout test, because there is a
    // "composited but paints into ancestor" layer involved. However, it doesn't make sense that
    // that check would be appropriate here but not inside the while loop below.
    if (current->isPaintInvalidationContainer() || current->hasCompositedDeprecatedPaintLayerMapping())
        return const_cast<DeprecatedPaintLayer*>(current);

    while (current) {
        if (current->isRootLayer())
            return const_cast<DeprecatedPaintLayer*>(current);

        current = current->compositingContainer();
        ASSERT(current);
        if (current->transform() || current->isPaintInvalidationContainer())
            return const_cast<DeprecatedPaintLayer*>(current);
    }

    ASSERT_NOT_REACHED();
    return 0;
}

bool DeprecatedPaintLayerClipper::shouldRespectOverflowClip(const ClipRectsContext& context) const
{
    DeprecatedPaintLayer* layer = m_layoutObject.layer();
    if (layer != context.rootLayer)
        return true;

    if (context.respectOverflowClip == IgnoreOverflowClip)
        return false;

    if (layer->isRootLayer() && context.respectOverflowClipForViewport == IgnoreOverflowClip)
        return false;

    return true;
}

} // namespace blink
