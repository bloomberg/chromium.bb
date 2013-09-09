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
#include "core/rendering/RenderLayer.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "core/css/PseudoStyleRequest.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentEventQueue.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLFrameElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/page/UseCounter.h"
#include "core/page/animation/AnimationController.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/platform/HistogramSupport.h"
#include "core/platform/Partitions.h"
#include "core/platform/PlatformGestureEvent.h"
#include "core/platform/PlatformMouseEvent.h"
#include "core/platform/ScrollAnimator.h"
#include "core/platform/Scrollbar.h"
#include "core/platform/ScrollbarTheme.h"
#include "core/platform/chromium/TraceEvent.h"
#include "core/platform/graphics/FloatPoint3D.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/GraphicsContextStateSaver.h"
#include "core/platform/graphics/filters/ReferenceFilter.h"
#include "core/platform/graphics/filters/SourceGraphic.h"
#include "core/platform/graphics/filters/custom/CustomFilterGlobalContext.h"
#include "core/platform/graphics/filters/custom/CustomFilterOperation.h"
#include "core/platform/graphics/filters/custom/CustomFilterValidatedProgram.h"
#include "core/platform/graphics/filters/custom/ValidatedCustomFilterOperation.h"
#include "core/platform/graphics/transforms/ScaleTransformOperation.h"
#include "core/platform/graphics/transforms/TransformationMatrix.h"
#include "core/platform/graphics/transforms/TranslateTransformOperation.h"
#include "core/rendering/ColumnInfo.h"
#include "core/rendering/FilterEffectRenderer.h"
#include "core/rendering/HitTestRequest.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/HitTestingTransformState.h"
#include "core/rendering/RenderFlowThread.h"
#include "core/rendering/RenderGeometryMap.h"
#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderLayerBacking.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderReplica.h"
#include "core/rendering/RenderScrollbar.h"
#include "core/rendering/RenderScrollbarPart.h"
#include "core/rendering/RenderTreeAsText.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/svg/ReferenceFilterBuilder.h"
#include "core/rendering/svg/RenderSVGResourceClipper.h"
#include "wtf/StdLibExtras.h"
#include "wtf/UnusedParam.h"
#include "wtf/text/CString.h"

#define MIN_INTERSECT_FOR_REVEAL 32

using namespace std;

namespace WebCore {

using namespace HTMLNames;

const int MinimumWidthWhileResizing = 100;
const int MinimumHeightWhileResizing = 40;
const int ResizerControlExpandRatioForTouch = 2;

bool ClipRect::intersects(const HitTestLocation& hitTestLocation) const
{
    return hitTestLocation.intersects(m_rect);
}

RenderLayer::RenderLayer(RenderLayerModelObject* renderer)
    : m_inResizeMode(false)
    , m_normalFlowListDirty(true)
    , m_hasSelfPaintingLayerDescendant(false)
    , m_hasSelfPaintingLayerDescendantDirty(false)
    , m_hasOutOfFlowPositionedDescendant(false)
    , m_hasOutOfFlowPositionedDescendantDirty(true)
    , m_hasUnclippedDescendant(false)
    , m_isUnclippedDescendant(false)
    , m_needsCompositedScrolling(false)
    , m_needsCompositedScrollingHasBeenRecorded(false)
    , m_willUseCompositedScrollingHasBeenRecorded(false)
    , m_isScrollableAreaHasBeenRecorded(false)
    , m_canBePromotedToStackingContainer(false)
    , m_canBePromotedToStackingContainerDirty(true)
    , m_isRootLayer(renderer->isRenderView())
    , m_usedTransparency(false)
    , m_paintingInsideReflection(false)
    , m_inOverflowRelayout(false)
    , m_repaintStatus(NeedsNormalRepaint)
    , m_visibleContentStatusDirty(true)
    , m_hasVisibleContent(false)
    , m_visibleDescendantStatusDirty(false)
    , m_hasVisibleDescendant(false)
    , m_isPaginated(false)
    , m_3DTransformedDescendantStatusDirty(true)
    , m_has3DTransformedDescendant(false)
    , m_containsDirtyOverlayScrollbars(false)
#if !ASSERT_DISABLED
    , m_layerListMutationAllowed(true)
#endif
    , m_canSkipRepaintRectsUpdateOnScroll(renderer->isTableCell())
    , m_hasFilterInfo(false)
    , m_blendMode(BlendModeNormal)
    , m_renderer(renderer)
    , m_parent(0)
    , m_previous(0)
    , m_next(0)
    , m_first(0)
    , m_last(0)
    , m_staticInlinePosition(0)
    , m_staticBlockPosition(0)
    , m_reflection(0)
    , m_scrollCorner(0)
    , m_resizer(0)
    , m_enclosingPaginationLayer(0)
    , m_forceNeedsCompositedScrolling(DoNotForceCompositedScrolling)
    // FIXME: We could lazily allocate our ScrollableArea based on style properties
    // ('overflow', ...) but for now, we are always allocating it as it's safer.
    , m_scrollableArea(adoptPtr(new RenderLayerScrollableArea(this)))

{
    m_isNormalFlowOnly = shouldBeNormalFlowOnly();
    m_isSelfPaintingLayer = shouldBeSelfPaintingLayer();

    // Non-stacking containers should have empty z-order lists. As this is already the case,
    // there is no need to dirty / recompute these lists.
    m_zOrderListsDirty = isStackingContainer();

    if (!renderer->firstChild() && renderer->style()) {
        m_visibleContentStatusDirty = false;
        m_hasVisibleContent = renderer->style()->visibility() == VISIBLE;
    }

    updateResizerAreaSet();
}

RenderLayer::~RenderLayer()
{
    if (inResizeMode() && !renderer()->documentBeingDestroyed()) {
        if (Frame* frame = renderer()->frame())
            frame->eventHandler()->resizeLayerDestroyed();
    }

    if (Frame* frame = renderer()->frame()) {
        if (FrameView* frameView = frame->view())
            frameView->removeResizerArea(this);
    }

    if (!m_renderer->documentBeingDestroyed())
        compositor()->removeOutOfFlowPositionedLayer(this);

    destroyScrollbar(HorizontalScrollbar);
    destroyScrollbar(VerticalScrollbar);

    if (renderer()->frame() && renderer()->frame()->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = renderer()->frame()->page()->scrollingCoordinator())
            scrollingCoordinator->willDestroyRenderLayer(this);
    }

    if (m_reflection)
        removeReflection();

    removeFilterInfoIfNeeded();

    // Child layers will be deleted by their corresponding render objects, so
    // we don't need to delete them ourselves.

    clearBacking(true);

    if (m_scrollCorner)
        m_scrollCorner->destroy();
    if (m_resizer)
        m_resizer->destroy();
}

String RenderLayer::debugName() const
{
    String name = renderer()->debugName();
    if (!isReflection())
        return name;
    return name + " (reflection)";
}

RenderLayerCompositor* RenderLayer::compositor() const
{
    if (!renderer()->view())
        return 0;
    return renderer()->view()->compositor();
}

void RenderLayer::contentChanged(ContentChangeType changeType)
{
    // This can get called when video becomes accelerated, so the layers may change.
    if ((changeType == CanvasChanged || changeType == VideoChanged || changeType == FullScreenChanged) && compositor()->updateLayerCompositingState(this))
        compositor()->setCompositingLayersNeedRebuild();

    if (m_backing)
        m_backing->contentChanged(changeType);
}

bool RenderLayer::canRender3DTransforms() const
{
    return compositor()->canRender3DTransforms();
}

bool RenderLayer::paintsWithFilters() const
{
    // FIXME: Eventually there will be more factors than isComposited() to decide whether or not to render the filter
    if (!renderer()->hasFilter())
        return false;

    if (!isComposited())
        return true;

    if (!m_backing || !m_backing->canCompositeFilters())
        return true;

    return false;
}

bool RenderLayer::requiresFullLayerImageForFilters() const
{
    if (!paintsWithFilters())
        return false;
    FilterEffectRenderer* filter = filterRenderer();
    return filter ? filter->hasFilterThatMovesPixels() : false;
}

LayoutPoint RenderLayer::computeOffsetFromRoot(bool& hasLayerOffset) const
{
    hasLayerOffset = true;

    if (!parent())
        return LayoutPoint();

    // This is similar to root() but we check if an ancestor layer would
    // prevent the optimization from working.
    const RenderLayer* rootLayer = 0;
    for (const RenderLayer* parentLayer = parent(); parentLayer; rootLayer = parentLayer, parentLayer = parentLayer->parent()) {
        hasLayerOffset = parentLayer->canUseConvertToLayerCoords();
        if (!hasLayerOffset)
            return LayoutPoint();
    }
    ASSERT(rootLayer == root());

    LayoutPoint offset;
    parent()->convertToLayerCoords(rootLayer, offset);
    return offset;
}

void RenderLayer::updateLayerPositionsAfterLayout(const RenderLayer* rootLayer, UpdateLayerPositionsFlags flags)
{
    RenderGeometryMap geometryMap(UseTransforms);
    if (this != rootLayer)
        geometryMap.pushMappingsToAncestor(parent(), 0);
    updateLayerPositions(&geometryMap, flags);
}

void RenderLayer::updateLayerPositions(RenderGeometryMap* geometryMap, UpdateLayerPositionsFlags flags)
{
    updateLayerPosition(); // For relpositioned layers or non-positioned layers,
                           // we need to keep in sync, since we may have shifted relative
                           // to our parent layer.
    if (geometryMap)
        geometryMap->pushMappingsToAncestor(this, parent());

    // Clear our cached clip rect information.
    clearClipRects();

    if (hasOverflowControls()) {
        LayoutPoint offsetFromRoot;
        if (geometryMap)
            offsetFromRoot = LayoutPoint(geometryMap->absolutePoint(FloatPoint()));
        else {
            // FIXME: It looks suspicious to call convertToLayerCoords here
            // as canUseConvertToLayerCoords may be true for an ancestor layer.
            convertToLayerCoords(root(), offsetFromRoot);
        }
        positionOverflowControls(toIntSize(roundedIntPoint(offsetFromRoot)));
    }

    updateDescendantDependentFlags();

    if (flags & UpdatePagination)
        updatePagination();
    else {
        m_isPaginated = false;
        m_enclosingPaginationLayer = 0;
    }

    if (m_hasVisibleContent) {
        RenderView* view = renderer()->view();
        ASSERT(view);
        // FIXME: LayoutState does not work with RenderLayers as there is not a 1-to-1
        // mapping between them and the RenderObjects. It would be neat to enable
        // LayoutState outside the layout() phase and use it here.
        ASSERT(!view->layoutStateEnabled());

        RenderLayerModelObject* repaintContainer = renderer()->containerForRepaint();
        LayoutRect oldRepaintRect = m_repaintRect;
        LayoutRect oldOutlineBox = m_outlineBox;
        computeRepaintRects(repaintContainer, geometryMap);

        // FIXME: Should ASSERT that value calculated for m_outlineBox using the cached offset is the same
        // as the value not using the cached offset, but we can't due to https://bugs.webkit.org/show_bug.cgi?id=37048
        if (flags & CheckForRepaint) {
            if (view && !view->printing()) {
                if (m_repaintStatus & NeedsFullRepaint) {
                    renderer()->repaintUsingContainer(repaintContainer, pixelSnappedIntRect(oldRepaintRect));
                    if (m_repaintRect != oldRepaintRect)
                        renderer()->repaintUsingContainer(repaintContainer, pixelSnappedIntRect(m_repaintRect));
                } else if (shouldRepaintAfterLayout())
                    renderer()->repaintAfterLayoutIfNeeded(repaintContainer, oldRepaintRect, oldOutlineBox, &m_repaintRect, &m_outlineBox);
            }
        }
    } else
        clearRepaintRects();

    m_repaintStatus = NeedsNormalRepaint;

    // Go ahead and update the reflection's position and size.
    if (m_reflection)
        m_reflection->layout();

    // Clear the IsCompositingUpdateRoot flag once we've found the first compositing layer in this update.
    bool isUpdateRoot = (flags & IsCompositingUpdateRoot);
    if (isComposited())
        flags &= ~IsCompositingUpdateRoot;

    if (useRegionBasedColumns() && renderer()->isInFlowRenderFlowThread()) {
        updatePagination();
        flags |= UpdatePagination;
    }

    if (renderer()->hasColumns())
        flags |= UpdatePagination;

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositions(geometryMap, flags);

    if ((flags & UpdateCompositingLayers) && isComposited()) {
        RenderLayerBacking::UpdateAfterLayoutFlags updateFlags = RenderLayerBacking::CompositingChildrenOnly;
        if (flags & NeedsFullRepaintInBacking)
            updateFlags |= RenderLayerBacking::NeedsFullRepaint;
        if (isUpdateRoot)
            updateFlags |= RenderLayerBacking::IsUpdateRoot;
        backing()->updateAfterLayout(updateFlags);
    }

    if (geometryMap)
        geometryMap->popMappingsToAncestor(parent());
}

LayoutRect RenderLayer::repaintRectIncludingNonCompositingDescendants() const
{
    LayoutRect repaintRect = m_repaintRect;
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        // Don't include repaint rects for composited child layers; they will paint themselves and have a different origin.
        if (child->isComposited())
            continue;

        repaintRect.unite(child->repaintRectIncludingNonCompositingDescendants());
    }
    return repaintRect;
}

void RenderLayer::setAncestorChainHasSelfPaintingLayerDescendant()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (!layer->m_hasSelfPaintingLayerDescendantDirty && layer->hasSelfPaintingLayerDescendant())
            break;

        layer->m_hasSelfPaintingLayerDescendantDirty = false;
        layer->m_hasSelfPaintingLayerDescendant = true;
    }
}

void RenderLayer::dirtyAncestorChainHasSelfPaintingLayerDescendantStatus()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        layer->m_hasSelfPaintingLayerDescendantDirty = true;
        // If we have reached a self-painting layer, we know our parent should have a self-painting descendant
        // in this case, there is no need to dirty our ancestors further.
        if (layer->isSelfPaintingLayer()) {
            ASSERT(!parent() || parent()->m_hasSelfPaintingLayerDescendantDirty || parent()->hasSelfPaintingLayerDescendant());
            break;
        }
    }
}

void RenderLayer::setAncestorChainHasOutOfFlowPositionedDescendant()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (!layer->m_hasOutOfFlowPositionedDescendantDirty && layer->hasOutOfFlowPositionedDescendant())
            break;

        layer->setHasOutOfFlowPositionedDescendantDirty(false);
        layer->setHasOutOfFlowPositionedDescendant(true);
    }
}

void RenderLayer::dirtyAncestorChainHasOutOfFlowPositionedDescendantStatus()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        layer->setHasOutOfFlowPositionedDescendantDirty(true);

        // We may or may not have an unclipped descendant. If we do, we'll reset
        // this to true the next time composited scrolling state is updated.
        layer->setHasUnclippedDescendant(false);

        // If we have reached an out of flow positioned layer, we know our parent should have an out-of-flow positioned descendant.
        // In this case, there is no need to dirty our ancestors further.
        if (layer->renderer()->isOutOfFlowPositioned()) {
            ASSERT(!parent() || parent()->m_hasOutOfFlowPositionedDescendantDirty || parent()->hasOutOfFlowPositionedDescendant());
            break;
        }
    }
}

bool RenderLayer::acceleratedCompositingForOverflowScrollEnabled() const
{
    const Settings* settings = renderer()->document().settings();
    return settings && settings->acceleratedCompositingForOverflowScrollEnabled();
}

// FIXME: This is a temporary flag and should be removed once accelerated
// overflow scroll is ready (crbug.com/254111).
bool RenderLayer::compositorDrivenAcceleratedScrollingEnabled() const
{
    if (!acceleratedCompositingForOverflowScrollEnabled())
        return false;

    const Settings* settings = renderer()->document().settings();
    return settings && settings->isCompositorDrivenAcceleratedScrollingEnabled();
}

// Determine whether the current layer can be promoted to a stacking container.
// We do this by computing what positive and negative z-order lists would look
// like before and after promotion, and ensuring that proper stacking order is
// preserved between the two sets of lists.
void RenderLayer::updateCanBeStackingContainer()
{
    TRACE_EVENT0("blink_rendering,comp-scroll", "RenderLayer::updateCanBeStackingContainer");

    if (isStackingContext() || !m_canBePromotedToStackingContainerDirty || !acceleratedCompositingForOverflowScrollEnabled())
        return;

    FrameView* frameView = renderer()->view()->frameView();
    if (!frameView || !frameView->containsScrollableArea(scrollableArea()))
        return;

    RenderLayer* ancestorStackingContext = this->ancestorStackingContext();
    if (!ancestorStackingContext)
        return;

    OwnPtr<Vector<RenderLayer*> > posZOrderListBeforePromote = adoptPtr(new Vector<RenderLayer*>);
    OwnPtr<Vector<RenderLayer*> > negZOrderListBeforePromote = adoptPtr(new Vector<RenderLayer*>);
    OwnPtr<Vector<RenderLayer*> > posZOrderListAfterPromote = adoptPtr(new Vector<RenderLayer*>);
    OwnPtr<Vector<RenderLayer*> > negZOrderListAfterPromote = adoptPtr(new Vector<RenderLayer*>);

    collectBeforePromotionZOrderList(ancestorStackingContext, posZOrderListBeforePromote, negZOrderListBeforePromote);
    collectAfterPromotionZOrderList(ancestorStackingContext, posZOrderListAfterPromote, negZOrderListAfterPromote);

    size_t maxIndex = std::min(posZOrderListAfterPromote->size() + negZOrderListAfterPromote->size(), posZOrderListBeforePromote->size() + negZOrderListBeforePromote->size());

    m_canBePromotedToStackingContainerDirty = false;
    m_canBePromotedToStackingContainer = false;

    const RenderLayer* layerAfterPromote = 0;
    for (size_t i = 0; i < maxIndex && layerAfterPromote != this; ++i) {
        const RenderLayer* layerBeforePromote = i < negZOrderListBeforePromote->size()
            ? negZOrderListBeforePromote->at(i)
            : posZOrderListBeforePromote->at(i - negZOrderListBeforePromote->size());
        layerAfterPromote = i < negZOrderListAfterPromote->size()
            ? negZOrderListAfterPromote->at(i)
            : posZOrderListAfterPromote->at(i - negZOrderListAfterPromote->size());

        if (layerBeforePromote != layerAfterPromote && (layerAfterPromote != this || renderer()->hasBackground()))
            return;
    }

    layerAfterPromote = 0;
    for (size_t i = 0; i < maxIndex && layerAfterPromote != this; ++i) {
        const RenderLayer* layerBeforePromote = i < posZOrderListBeforePromote->size()
            ? posZOrderListBeforePromote->at(posZOrderListBeforePromote->size() - i - 1)
            : negZOrderListBeforePromote->at(negZOrderListBeforePromote->size() + posZOrderListBeforePromote->size() - i - 1);
        layerAfterPromote = i < posZOrderListAfterPromote->size()
            ? posZOrderListAfterPromote->at(posZOrderListAfterPromote->size() - i - 1)
            : negZOrderListAfterPromote->at(negZOrderListAfterPromote->size() + posZOrderListAfterPromote->size() - i - 1);

        if (layerBeforePromote != layerAfterPromote && layerAfterPromote != this)
            return;
    }

    m_canBePromotedToStackingContainer = true;
}

static inline bool isPositionedContainer(const RenderLayer* layer)
{
    // FIXME: This is not in sync with containingBlock.
    // RenderObject::canContainFixedPositionedObject() should probably be used
    // instead.
    RenderLayerModelObject* layerRenderer = layer->renderer();
    return layer->isRootLayer() || layerRenderer->isPositioned() || layer->hasTransform();
}

void RenderLayer::collectBeforePromotionZOrderList(RenderLayer* ancestorStackingContext, OwnPtr<Vector<RenderLayer*> >& posZOrderListBeforePromote, OwnPtr<Vector<RenderLayer*> >& negZOrderListBeforePromote)
{
    ancestorStackingContext->rebuildZOrderLists(posZOrderListBeforePromote, negZOrderListBeforePromote, this, OnlyStackingContextsCanBeStackingContainers);

    const RenderLayer* positionedAncestor = parent();
    while (positionedAncestor && !isPositionedContainer(positionedAncestor) && !positionedAncestor->isStackingContext())
        positionedAncestor = positionedAncestor->parent();
    if (positionedAncestor && (!isPositionedContainer(positionedAncestor) || positionedAncestor->isStackingContext()))
        positionedAncestor = 0;

    if (!posZOrderListBeforePromote)
        posZOrderListBeforePromote = adoptPtr(new Vector<RenderLayer*>());
    else if (posZOrderListBeforePromote->find(this) != notFound)
        return;

    // The current layer will appear in the z-order lists after promotion, so
    // for a meaningful comparison, we must insert it in the z-order lists
    // before promotion if it does not appear there already.
    if (!positionedAncestor) {
        posZOrderListBeforePromote->prepend(this);
        return;
    }

    for (size_t index = 0; index < posZOrderListBeforePromote->size(); index++) {
        if (posZOrderListBeforePromote->at(index) == positionedAncestor) {
            posZOrderListBeforePromote->insert(index + 1, this);
            return;
        }
    }
}

void RenderLayer::collectAfterPromotionZOrderList(RenderLayer* ancestorStackingContext, OwnPtr<Vector<RenderLayer*> >& posZOrderListAfterPromote, OwnPtr<Vector<RenderLayer*> >& negZOrderListAfterPromote)
{
    ancestorStackingContext->rebuildZOrderLists(posZOrderListAfterPromote, negZOrderListAfterPromote, this, ForceLayerToStackingContainer);
}

// Compute what positive and negative z-order lists would look like before and
// after promotion, so we can later ensure that proper stacking order is
// preserved between the two sets of lists.
//
// A few examples:
// c = currentLayer
// - = negative z-order child of currentLayer
// + = positive z-order child of currentLayer
// a = positioned ancestor of currentLayer
// x = any other RenderLayer in the list
//
// (a) xxxxx-----++a+++x
// (b) xxx-----c++++++xx
//
// Normally the current layer would be painted in the normal flow list if it
// doesn't already appear in the positive z-order list. However, in the case
// that the layer has a positioned ancestor, it will paint directly after the
// positioned ancestor. In example (a), the current layer would be painted in
// the middle of its own positive z-order children, so promoting would cause a
// change in paint order (since a promoted layer will paint all of its positive
// z-order children strictly after it paints itself).
//
// In example (b), it is ok to promote the current layer only if it does not
// have a background. If it has a background, the background gets painted before
// the layer's negative z-order children, so again, a promotion would cause a
// change in paint order (causing the background to get painted after the
// negative z-order children instead of before).
//
void RenderLayer::computePaintOrderList(PaintOrderListType type, Vector<RefPtr<Node> >& list)
{
    OwnPtr<Vector<RenderLayer*> > posZOrderList;
    OwnPtr<Vector<RenderLayer*> > negZOrderList;

    RenderLayer* stackingContext = ancestorStackingContext();

    if (!stackingContext)
        return;

    switch (type) {
    case BeforePromote:
        collectBeforePromotionZOrderList(stackingContext, posZOrderList, negZOrderList);
        break;
    case AfterPromote:
        collectAfterPromotionZOrderList(stackingContext, posZOrderList, negZOrderList);
        break;
    }

    if (negZOrderList) {
        for (size_t index = 0; index < negZOrderList->size(); ++index)
            list.append(negZOrderList->at(index)->renderer()->node());
    }

    if (posZOrderList) {
        for (size_t index = 0; index < posZOrderList->size(); ++index)
            list.append(posZOrderList->at(index)->renderer()->node());
    }
}

bool RenderLayer::scrollsWithRespectTo(const RenderLayer* other) const
{
    const EPosition position = renderer()->style()->position();
    const EPosition otherPosition = other->renderer()->style()->position();
    const RenderObject* containingBlock = renderer()->containingBlock();
    const RenderObject* otherContainingBlock = other->renderer()->containingBlock();
    const RenderLayer* rootLayer = renderer()->view()->compositor()->rootRenderLayer();

    // Fixed-position elements are a special case. They are static with respect
    // to the viewport, which is not represented by any RenderObject, and their
    // containingBlock() method returns the root HTML element (while its true
    // containingBlock should really be the viewport). The real measure for a
    // non-transformed fixed-position element is as follows: any fixed position
    // element, A, scrolls with respect an element, B, if and only if B is not
    // fixed position.
    //
    // Unfortunately, it gets a bit more complicated - a fixed-position element
    // which has a transform acts exactly as an absolute-position element
    // (including having a real, non-viewport containing block).
    //
    // Below, a "root" fixed position element is defined to be one whose
    // containing block is the root. These root-fixed-position elements are
    // the only ones that need this special case code - other fixed position
    // elements, as well as all absolute, relative, and static elements use the
    // logic below.
    const bool isRootFixedPos = position == FixedPosition && containingBlock->enclosingLayer() == rootLayer;
    const bool otherIsRootFixedPos = otherPosition == FixedPosition && otherContainingBlock->enclosingLayer() == rootLayer;

    if (isRootFixedPos && otherIsRootFixedPos)
        return false;
    if (isRootFixedPos || otherIsRootFixedPos)
        return true;

    FrameView* frameView = renderer()->view()->frameView();

    if (containingBlock == otherContainingBlock)
        return false;

    // Maintain a set of containing blocks between the first layer and its
    // closest scrollable ancestor.
    HashSet<const RenderObject*> containingBlocks;
    while (containingBlock) {
        if (frameView && frameView->containsScrollableArea(containingBlock->enclosingLayer()->scrollableArea()))
            break;
        containingBlocks.add(containingBlock);
        containingBlock = containingBlock->containingBlock();
    }

    // Do the same for the 2nd layer, but if we find a common containing block,
    // it means both layers are contained within a single non-scrolling subtree.
    // Hence, they will not scroll with respect to each other.
    while (otherContainingBlock) {
        if (containingBlocks.contains(otherContainingBlock))
            return false;
        if (frameView && frameView->containsScrollableArea(otherContainingBlock->enclosingLayer()->scrollableArea()))
            break;
        otherContainingBlock = otherContainingBlock->containingBlock();
    }

    return true;
}

void RenderLayer::computeRepaintRects(const RenderLayerModelObject* repaintContainer, const RenderGeometryMap* geometryMap)
{
    ASSERT(!m_visibleContentStatusDirty);

    m_repaintRect = renderer()->clippedOverflowRectForRepaint(repaintContainer);
    m_outlineBox = renderer()->outlineBoundsForRepaint(repaintContainer, geometryMap);
}


void RenderLayer::computeRepaintRectsIncludingDescendants()
{
    // FIXME: computeRepaintRects() has to walk up the parent chain for every layer to compute the rects.
    // We should make this more efficient.
    // FIXME: it's wrong to call this when layout is not up-to-date, which we do.
    computeRepaintRects(renderer()->containerForRepaint());

    for (RenderLayer* layer = firstChild(); layer; layer = layer->nextSibling())
        layer->computeRepaintRectsIncludingDescendants();
}

void RenderLayer::clearRepaintRects()
{
    ASSERT(!m_hasVisibleContent);
    ASSERT(!m_visibleContentStatusDirty);

    m_repaintRect = IntRect();
    m_outlineBox = IntRect();
}

void RenderLayer::updateLayerPositionsAfterDocumentScroll()
{
    ASSERT(this == renderer()->view()->layer());

    RenderGeometryMap geometryMap(UseTransforms);
    updateLayerPositionsAfterScroll(&geometryMap);
}

void RenderLayer::updateLayerPositionsAfterOverflowScroll()
{
    RenderGeometryMap geometryMap(UseTransforms);
    RenderView* view = renderer()->view();
    if (this != view->layer())
        geometryMap.pushMappingsToAncestor(parent(), 0);

    // FIXME: why is it OK to not check the ancestors of this layer in order to
    // initialize the HasSeenViewportConstrainedAncestor and HasSeenAncestorWithOverflowClip flags?
    updateLayerPositionsAfterScroll(&geometryMap, IsOverflowScroll);
}

void RenderLayer::updateLayerPositionsAfterScroll(RenderGeometryMap* geometryMap, UpdateLayerPositionsAfterScrollFlags flags)
{
    // FIXME: This shouldn't be needed, but there are some corner cases where
    // these flags are still dirty. Update so that the check below is valid.
    updateDescendantDependentFlags();

    // If we have no visible content and no visible descendants, there is no point recomputing
    // our rectangles as they will be empty. If our visibility changes, we are expected to
    // recompute all our positions anyway.
    if (!m_hasVisibleDescendant && !m_hasVisibleContent)
        return;

    bool positionChanged = updateLayerPosition();
    if (positionChanged)
        flags |= HasChangedAncestor;

    if (geometryMap)
        geometryMap->pushMappingsToAncestor(this, parent());

    if (flags & HasChangedAncestor || flags & HasSeenViewportConstrainedAncestor || flags & IsOverflowScroll)
        clearClipRects();

    if (renderer()->style()->hasViewportConstrainedPosition())
        flags |= HasSeenViewportConstrainedAncestor;

    if (renderer()->hasOverflowClip())
        flags |= HasSeenAncestorWithOverflowClip;

    if (flags & HasSeenViewportConstrainedAncestor
        || (flags & IsOverflowScroll && flags & HasSeenAncestorWithOverflowClip && !m_canSkipRepaintRectsUpdateOnScroll)) {
        // FIXME: We could track the repaint container as we walk down the tree.
        computeRepaintRects(renderer()->containerForRepaint(), geometryMap);
    } else {
        // Check that our cached rects are correct.
        // FIXME: re-enable these assertions when the issue with table cells is resolved: https://bugs.webkit.org/show_bug.cgi?id=103432
        // ASSERT(m_repaintRect == renderer()->clippedOverflowRectForRepaint(renderer()->containerForRepaint()));
        // ASSERT(m_outlineBox == renderer()->outlineBoundsForRepaint(renderer()->containerForRepaint(), geometryMap));
    }

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositionsAfterScroll(geometryMap, flags);

    // We don't update our reflection as scrolling is a translation which does not change the size()
    // of an object, thus RenderReplica will still repaint itself properly as the layer position was
    // updated above.

    if (geometryMap)
        geometryMap->popMappingsToAncestor(parent());
}

void RenderLayer::positionNewlyCreatedOverflowControls()
{
    if (!backing()->hasUnpositionedOverflowControlsLayers())
        return;

    RenderGeometryMap geometryMap(UseTransforms);
    RenderView* view = renderer()->view();
    if (this != view->layer() && parent())
        geometryMap.pushMappingsToAncestor(parent(), 0);

    LayoutPoint offsetFromRoot = LayoutPoint(geometryMap.absolutePoint(FloatPoint()));
    positionOverflowControls(toIntSize(roundedIntPoint(offsetFromRoot)));
}

bool RenderLayer::hasBlendMode() const
{
    return RuntimeEnabledFeatures::cssCompositingEnabled() && renderer()->hasBlendMode();
}

void RenderLayer::updateBlendMode()
{
    if (!RuntimeEnabledFeatures::cssCompositingEnabled())
        return;

    BlendMode newBlendMode = renderer()->style()->blendMode();
    if (newBlendMode != m_blendMode) {
        m_blendMode = newBlendMode;
        if (backing())
            backing()->setBlendMode(newBlendMode);
    }
}

void RenderLayer::updateTransform()
{
    // hasTransform() on the renderer is also true when there is transform-style: preserve-3d or perspective set,
    // so check style too.
    bool hasTransform = renderer()->hasTransform() && renderer()->style()->hasTransform();
    bool had3DTransform = has3DTransform();

    bool hadTransform = m_transform;
    if (hasTransform != hadTransform) {
        if (hasTransform)
            m_transform = adoptPtr(new TransformationMatrix);
        else
            m_transform.clear();

        // Layers with transforms act as clip rects roots, so clear the cached clip rects here.
        clearClipRectsIncludingDescendants();
    }

    if (hasTransform) {
        RenderBox* box = renderBox();
        ASSERT(box);
        m_transform->makeIdentity();
        box->style()->applyTransform(*m_transform, box->pixelSnappedBorderBoxRect().size(), RenderStyle::IncludeTransformOrigin);
        makeMatrixRenderable(*m_transform, canRender3DTransforms());
    }

    if (had3DTransform != has3DTransform())
        dirty3DTransformedDescendantStatus();
}

TransformationMatrix RenderLayer::currentTransform(RenderStyle::ApplyTransformOrigin applyOrigin) const
{
    if (!m_transform)
        return TransformationMatrix();

    if (renderer()->style()->isRunningAcceleratedAnimation()) {
        TransformationMatrix currTransform;
        RefPtr<RenderStyle> style = renderer()->animation()->getAnimatedStyleForRenderer(renderer());
        style->applyTransform(currTransform, renderBox()->pixelSnappedBorderBoxRect().size(), applyOrigin);
        makeMatrixRenderable(currTransform, canRender3DTransforms());
        return currTransform;
    }

    // m_transform includes transform-origin, so we need to recompute the transform here.
    if (applyOrigin == RenderStyle::ExcludeTransformOrigin) {
        RenderBox* box = renderBox();
        TransformationMatrix currTransform;
        box->style()->applyTransform(currTransform, box->pixelSnappedBorderBoxRect().size(), RenderStyle::ExcludeTransformOrigin);
        makeMatrixRenderable(currTransform, canRender3DTransforms());
        return currTransform;
    }

    return *m_transform;
}

TransformationMatrix RenderLayer::renderableTransform(PaintBehavior paintBehavior) const
{
    if (!m_transform)
        return TransformationMatrix();

    if (paintBehavior & PaintBehaviorFlattenCompositingLayers) {
        TransformationMatrix matrix = *m_transform;
        makeMatrixRenderable(matrix, false /* flatten 3d */);
        return matrix;
    }

    return *m_transform;
}

static bool checkContainingBlockChainForPagination(RenderLayerModelObject* renderer, RenderBox* ancestorColumnsRenderer)
{
    RenderView* view = renderer->view();
    RenderLayerModelObject* prevBlock = renderer;
    RenderBlock* containingBlock;
    for (containingBlock = renderer->containingBlock();
         containingBlock && containingBlock != view && containingBlock != ancestorColumnsRenderer;
         containingBlock = containingBlock->containingBlock())
        prevBlock = containingBlock;

    // If the columns block wasn't in our containing block chain, then we aren't paginated by it.
    if (containingBlock != ancestorColumnsRenderer)
        return false;

    // If the previous block is absolutely positioned, then we can't be paginated by the columns block.
    if (prevBlock->isOutOfFlowPositioned())
        return false;

    // Otherwise we are paginated by the columns block.
    return true;
}

bool RenderLayer::useRegionBasedColumns() const
{
    const Settings* settings = renderer()->document().settings();
    return settings && settings->regionBasedColumnsEnabled();
}

void RenderLayer::updatePagination()
{
    m_isPaginated = false;
    m_enclosingPaginationLayer = 0;

    if (isComposited() || !parent())
        return; // FIXME: We will have to deal with paginated compositing layers someday.
                // FIXME: For now the RenderView can't be paginated.  Eventually printing will move to a model where it is though.

    // The main difference between the paginated booleans for the old column code and the new column code
    // is that each paginated layer has to paint on its own with the new code. There is no
    // recurring into child layers. This means that the m_isPaginated bits for the new column code can't just be set on
    // "roots" that get split and paint all their descendants. Instead each layer has to be checked individually and
    // genuinely know if it is going to have to split itself up when painting only its contents (and not any other descendant
    // layers). We track an enclosingPaginationLayer instead of using a simple bit, since we want to be able to get back
    // to that layer easily.
    bool regionBasedColumnsUsed = useRegionBasedColumns();
    if (regionBasedColumnsUsed && renderer()->isInFlowRenderFlowThread()) {
        m_enclosingPaginationLayer = this;
        return;
    }

    if (isNormalFlowOnly()) {
        if (regionBasedColumnsUsed) {
            // Content inside a transform is not considered to be paginated, since we simply
            // paint the transform multiple times in each column, so we don't have to use
            // fragments for the transformed content.
            m_enclosingPaginationLayer = parent()->enclosingPaginationLayer();
            if (m_enclosingPaginationLayer && m_enclosingPaginationLayer->hasTransform())
                m_enclosingPaginationLayer = 0;
        } else
            m_isPaginated = parent()->renderer()->hasColumns();
        return;
    }

    // For the new columns code, we want to walk up our containing block chain looking for an enclosing layer. Once
    // we find one, then we just check its pagination status.
    if (regionBasedColumnsUsed) {
        RenderView* view = renderer()->view();
        RenderBlock* containingBlock;
        for (containingBlock = renderer()->containingBlock();
             containingBlock && containingBlock != view;
             containingBlock = containingBlock->containingBlock()) {
            if (containingBlock->hasLayer()) {
                // Content inside a transform is not considered to be paginated, since we simply
                // paint the transform multiple times in each column, so we don't have to use
                // fragments for the transformed content.
                m_enclosingPaginationLayer = containingBlock->layer()->enclosingPaginationLayer();
                if (m_enclosingPaginationLayer && m_enclosingPaginationLayer->hasTransform())
                    m_enclosingPaginationLayer = 0;
                return;
            }
        }
        return;
    }

    // If we're not normal flow, then we need to look for a multi-column object between us and our stacking container.
    RenderLayer* ancestorStackingContainer = this->ancestorStackingContainer();
    for (RenderLayer* curr = parent(); curr; curr = curr->parent()) {
        if (curr->renderer()->hasColumns()) {
            m_isPaginated = checkContainingBlockChainForPagination(renderer(), curr->renderBox());
            return;
        }
        if (curr == ancestorStackingContainer)
            return;
    }
}

bool RenderLayer::canBeStackingContainer() const
{
    if (isStackingContext() || !ancestorStackingContainer())
        return true;

    ASSERT(!m_canBePromotedToStackingContainerDirty);
    return m_canBePromotedToStackingContainer;
}

void RenderLayer::setHasVisibleContent()
{
    if (m_hasVisibleContent && !m_visibleContentStatusDirty) {
        ASSERT(!parent() || parent()->hasVisibleDescendant());
        return;
    }

    m_visibleContentStatusDirty = false;
    m_hasVisibleContent = true;
    computeRepaintRects(renderer()->containerForRepaint());
    if (!isNormalFlowOnly()) {
        // We don't collect invisible layers in z-order lists if we are not in compositing mode.
        // As we became visible, we need to dirty our stacking containers ancestors to be properly
        // collected. FIXME: When compositing, we could skip this dirtying phase.
        for (RenderLayer* sc = ancestorStackingContainer(); sc; sc = sc->ancestorStackingContainer()) {
            sc->dirtyZOrderLists();
            if (sc->hasVisibleContent())
                break;
        }
    }

    if (parent())
        parent()->setAncestorChainHasVisibleDescendant();
}

void RenderLayer::dirtyVisibleContentStatus()
{
    m_visibleContentStatusDirty = true;
    if (parent())
        parent()->dirtyAncestorChainVisibleDescendantStatus();
}

void RenderLayer::dirtyAncestorChainVisibleDescendantStatus()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (layer->m_visibleDescendantStatusDirty)
            break;

        layer->m_visibleDescendantStatusDirty = true;
    }
}

void RenderLayer::setAncestorChainHasVisibleDescendant()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (!layer->m_visibleDescendantStatusDirty && layer->hasVisibleDescendant())
            break;

        layer->m_hasVisibleDescendant = true;
        layer->m_visibleDescendantStatusDirty = false;
    }
}

void RenderLayer::updateHasUnclippedDescendant()
{
    TRACE_EVENT0("blink_rendering", "RenderLayer::updateHasUnclippedDescendant");
    ASSERT(renderer()->isOutOfFlowPositioned());
    if (!m_hasVisibleContent && !m_hasVisibleDescendant)
        return;

    const RenderObject* containingBlock = renderer()->containingBlock();
    setIsUnclippedDescendant(false);
    for (RenderLayer* ancestor = parent(); ancestor && ancestor->renderer() != containingBlock; ancestor = ancestor->parent()) {
        // TODO(vollick): This isn't quite right. Whenever ancestor is composited and clips
        // overflow, we're technically unclipped. However, this will currently cause a huge
        // number of layers to report that they are unclipped. Eventually, when we've formally
        // separated the clipping, transform, opacity, and stacking trees here and in the
        // compositor, we will be able to relax this restriction without it being prohibitively
        // expensive (currently, we have to do a lot of work in the compositor to honor a
        // clip child/parent relationship).
        if (ancestor->needsCompositedScrolling())
            setIsUnclippedDescendant(true);
        ancestor->setHasUnclippedDescendant(true);
    }
}

static bool subtreeContainsOutOfFlowPositionedLayer(const RenderLayer* subtreeRoot)
{
    return (subtreeRoot->renderer() && subtreeRoot->renderer()->isOutOfFlowPositioned()) || subtreeRoot->hasOutOfFlowPositionedDescendant();
}

void RenderLayer::updateDescendantDependentFlags()
{
    if (m_visibleDescendantStatusDirty || m_hasSelfPaintingLayerDescendantDirty || m_hasOutOfFlowPositionedDescendantDirty) {
        m_hasVisibleDescendant = false;
        m_hasSelfPaintingLayerDescendant = false;
        m_hasOutOfFlowPositionedDescendant = false;

        for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
            child->updateDescendantDependentFlags();

            bool hasVisibleDescendant = child->m_hasVisibleContent || child->m_hasVisibleDescendant;
            bool hasSelfPaintingLayerDescendant = child->isSelfPaintingLayer() || child->hasSelfPaintingLayerDescendant();
            bool hasOutOfFlowPositionedDescendant = subtreeContainsOutOfFlowPositionedLayer(child);

            m_hasVisibleDescendant |= hasVisibleDescendant;
            m_hasSelfPaintingLayerDescendant |= hasSelfPaintingLayerDescendant;
            m_hasOutOfFlowPositionedDescendant |= hasOutOfFlowPositionedDescendant;

            if (m_hasVisibleDescendant && m_hasSelfPaintingLayerDescendant && hasOutOfFlowPositionedDescendant)
                break;
        }

        m_visibleDescendantStatusDirty = false;
        m_hasSelfPaintingLayerDescendantDirty = false;
        m_hasOutOfFlowPositionedDescendantDirty = false;
    }

    if (m_visibleContentStatusDirty) {
        if (renderer()->style()->visibility() == VISIBLE)
            m_hasVisibleContent = true;
        else {
            // layer may be hidden but still have some visible content, check for this
            m_hasVisibleContent = false;
            RenderObject* r = renderer()->firstChild();
            while (r) {
                if (r->style()->visibility() == VISIBLE && !r->hasLayer()) {
                    m_hasVisibleContent = true;
                    break;
                }
                if (r->firstChild() && !r->hasLayer())
                    r = r->firstChild();
                else if (r->nextSibling())
                    r = r->nextSibling();
                else {
                    do {
                        r = r->parent();
                        if (r == renderer())
                            r = 0;
                    } while (r && !r->nextSibling());
                    if (r)
                        r = r->nextSibling();
                }
            }
        }
        m_visibleContentStatusDirty = false;
    }
}

void RenderLayer::dirty3DTransformedDescendantStatus()
{
    RenderLayer* curr = ancestorStackingContainer();
    if (curr)
        curr->m_3DTransformedDescendantStatusDirty = true;

    // This propagates up through preserve-3d hierarchies to the enclosing flattening layer.
    // Note that preserves3D() creates stacking context, so we can just run up the stacking containers.
    while (curr && curr->preserves3D()) {
        curr->m_3DTransformedDescendantStatusDirty = true;
        curr = curr->ancestorStackingContainer();
    }
}

// Return true if this layer or any preserve-3d descendants have 3d.
bool RenderLayer::update3DTransformedDescendantStatus()
{
    if (m_3DTransformedDescendantStatusDirty) {
        m_has3DTransformedDescendant = false;

        updateZOrderLists();

        // Transformed or preserve-3d descendants can only be in the z-order lists, not
        // in the normal flow list, so we only need to check those.
        if (Vector<RenderLayer*>* positiveZOrderList = posZOrderList()) {
            for (unsigned i = 0; i < positiveZOrderList->size(); ++i)
                m_has3DTransformedDescendant |= positiveZOrderList->at(i)->update3DTransformedDescendantStatus();
        }

        // Now check our negative z-index children.
        if (Vector<RenderLayer*>* negativeZOrderList = negZOrderList()) {
            for (unsigned i = 0; i < negativeZOrderList->size(); ++i)
                m_has3DTransformedDescendant |= negativeZOrderList->at(i)->update3DTransformedDescendantStatus();
        }

        m_3DTransformedDescendantStatusDirty = false;
    }

    // If we live in a 3d hierarchy, then the layer at the root of that hierarchy needs
    // the m_has3DTransformedDescendant set.
    if (preserves3D())
        return has3DTransform() || m_has3DTransformedDescendant;

    return has3DTransform();
}

bool RenderLayer::updateLayerPosition()
{
    LayoutPoint localPoint;
    LayoutSize inlineBoundingBoxOffset; // We don't put this into the RenderLayer x/y for inlines, so we need to subtract it out when done.
    if (renderer()->isInline() && renderer()->isRenderInline()) {
        RenderInline* inlineFlow = toRenderInline(renderer());
        IntRect lineBox = inlineFlow->linesBoundingBox();
        setSize(lineBox.size());
        inlineBoundingBoxOffset = toSize(lineBox.location());
        localPoint += inlineBoundingBoxOffset;
    } else if (RenderBox* box = renderBox()) {
        // FIXME: Is snapping the size really needed here for the RenderBox case?
        setSize(pixelSnappedIntSize(box->size(), box->location()));
        localPoint += box->topLeftLocationOffset();
    }

    if (!renderer()->isOutOfFlowPositioned() && renderer()->parent()) {
        // We must adjust our position by walking up the render tree looking for the
        // nearest enclosing object with a layer.
        RenderObject* curr = renderer()->parent();
        while (curr && !curr->hasLayer()) {
            if (curr->isBox() && !curr->isTableRow()) {
                // Rows and cells share the same coordinate space (that of the section).
                // Omit them when computing our xpos/ypos.
                localPoint += toRenderBox(curr)->topLeftLocationOffset();
            }
            curr = curr->parent();
        }
        if (curr->isBox() && curr->isTableRow()) {
            // Put ourselves into the row coordinate space.
            localPoint -= toRenderBox(curr)->topLeftLocationOffset();
        }
    }

    // Subtract our parent's scroll offset.
    if (renderer()->isOutOfFlowPositioned() && enclosingPositionedAncestor()) {
        RenderLayer* positionedParent = enclosingPositionedAncestor();

        // For positioned layers, we subtract out the enclosing positioned layer's scroll offset.
        if (positionedParent->renderer()->hasOverflowClip()) {
            LayoutSize offset = positionedParent->scrolledContentOffset();
            localPoint -= offset;
        }

        if (renderer()->isOutOfFlowPositioned() && positionedParent->renderer()->isInFlowPositioned() && positionedParent->renderer()->isRenderInline()) {
            LayoutSize offset = toRenderInline(positionedParent->renderer())->offsetForInFlowPositionedInline(toRenderBox(renderer()));
            localPoint += offset;
        }
    } else if (parent()) {
        if (isComposited()) {
            // FIXME: Composited layers ignore pagination, so about the best we can do is make sure they're offset into the appropriate column.
            // They won't split across columns properly.
            LayoutSize columnOffset;
            if (!parent()->renderer()->hasColumns() && parent()->renderer()->isRoot() && renderer()->view()->hasColumns())
                renderer()->view()->adjustForColumns(columnOffset, localPoint);
            else
                parent()->renderer()->adjustForColumns(columnOffset, localPoint);

            localPoint += columnOffset;
        }

        if (parent()->renderer()->hasOverflowClip()) {
            IntSize scrollOffset = parent()->scrolledContentOffset();
            localPoint -= scrollOffset;
        }
    }

    bool positionOrOffsetChanged = false;
    if (renderer()->isInFlowPositioned()) {
        LayoutSize newOffset = toRenderBoxModelObject(renderer())->offsetForInFlowPosition();
        positionOrOffsetChanged = newOffset != m_offsetForInFlowPosition;
        m_offsetForInFlowPosition = newOffset;
        localPoint.move(m_offsetForInFlowPosition);
    } else {
        m_offsetForInFlowPosition = LayoutSize();
    }

    // FIXME: We'd really like to just get rid of the concept of a layer rectangle and rely on the renderers.
    localPoint -= inlineBoundingBoxOffset;

    positionOrOffsetChanged |= location() != localPoint;
    setLocation(localPoint);
    return positionOrOffsetChanged;
}

TransformationMatrix RenderLayer::perspectiveTransform() const
{
    if (!renderer()->hasTransform())
        return TransformationMatrix();

    RenderStyle* style = renderer()->style();
    if (!style->hasPerspective())
        return TransformationMatrix();

    // Maybe fetch the perspective from the backing?
    const IntRect borderBox = toRenderBox(renderer())->pixelSnappedBorderBoxRect();
    const float boxWidth = borderBox.width();
    const float boxHeight = borderBox.height();

    float perspectiveOriginX = floatValueForLength(style->perspectiveOriginX(), boxWidth);
    float perspectiveOriginY = floatValueForLength(style->perspectiveOriginY(), boxHeight);

    // A perspective origin of 0,0 makes the vanishing point in the center of the element.
    // We want it to be in the top-left, so subtract half the height and width.
    perspectiveOriginX -= boxWidth / 2.0f;
    perspectiveOriginY -= boxHeight / 2.0f;

    TransformationMatrix t;
    t.translate(perspectiveOriginX, perspectiveOriginY);
    t.applyPerspective(style->perspective());
    t.translate(-perspectiveOriginX, -perspectiveOriginY);

    return t;
}

FloatPoint RenderLayer::perspectiveOrigin() const
{
    if (!renderer()->hasTransform())
        return FloatPoint();

    const LayoutRect borderBox = toRenderBox(renderer())->borderBoxRect();
    RenderStyle* style = renderer()->style();

    return FloatPoint(floatValueForLength(style->perspectiveOriginX(), borderBox.width()),
                      floatValueForLength(style->perspectiveOriginY(), borderBox.height()));
}

RenderLayer* RenderLayer::ancestorStackingContainer() const
{
    RenderLayer* ancestor = parent();
    while (ancestor && !ancestor->isStackingContainer())
        ancestor = ancestor->parent();
    return ancestor;
}

RenderLayer* RenderLayer::ancestorStackingContext() const
{
    RenderLayer* ancestor = parent();
    while (ancestor && !ancestor->isStackingContext())
        ancestor = ancestor->parent();
    return ancestor;
}

static inline bool isFixedPositionedContainer(RenderLayer* layer)
{
    return layer->isRootLayer() || layer->hasTransform();
}

RenderLayer* RenderLayer::enclosingPositionedAncestor() const
{
    RenderLayer* curr = parent();
    while (curr && !isPositionedContainer(curr))
        curr = curr->parent();

    return curr;
}

RenderLayer* RenderLayer::enclosingScrollableLayer() const
{
    for (RenderLayer* nextLayer = parent(); nextLayer; nextLayer = nextLayer->parent()) {
        if (nextLayer->renderer()->isBox() && toRenderBox(nextLayer->renderer())->canBeScrolledAndHasScrollableArea())
            return nextLayer;
    }

    return 0;
}

IntRect RenderLayer::scrollableAreaBoundingBox() const
{
    return renderer()->absoluteBoundingBoxRect();
}

bool RenderLayer::userInputScrollable(ScrollbarOrientation orientation) const
{
    RenderBox* box = renderBox();
    ASSERT(box);

    if (box->isIntristicallyScrollable(orientation))
        return true;

    EOverflow overflowStyle = (orientation == HorizontalScrollbar) ?
        renderer()->style()->overflowX() : renderer()->style()->overflowY();
    return (overflowStyle == OSCROLL || overflowStyle == OAUTO || overflowStyle == OOVERLAY);
}

bool RenderLayer::shouldPlaceVerticalScrollbarOnLeft() const
{
    return renderer()->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft();
}

int RenderLayer::pageStep(ScrollbarOrientation orientation) const
{
    RenderBox* box = renderBox();
    ASSERT(box);

    int length = (orientation == HorizontalScrollbar) ?
        box->pixelSnappedClientWidth() : box->pixelSnappedClientHeight();
    int minPageStep = static_cast<float>(length) * ScrollableArea::minFractionToStepWhenPaging();
    int pageStep = max(minPageStep, length - ScrollableArea::maxOverlapBetweenPages());

    return max(pageStep, 1);
}

RenderLayer* RenderLayer::enclosingTransformedAncestor() const
{
    RenderLayer* curr = parent();
    while (curr && !curr->isRootLayer() && !curr->transform())
        curr = curr->parent();

    return curr;
}

static inline const RenderLayer* compositingContainer(const RenderLayer* layer)
{
    return layer->isNormalFlowOnly() ? layer->parent() : layer->ancestorStackingContainer();
}

inline bool RenderLayer::shouldRepaintAfterLayout() const
{
    if (m_repaintStatus == NeedsNormalRepaint)
        return true;

    // Composited layers that were moved during a positioned movement only
    // layout, don't need to be repainted. They just need to be recomposited.
    ASSERT(m_repaintStatus == NeedsFullRepaintForPositionedMovementLayout);
    return !isComposited();
}

RenderLayer* RenderLayer::enclosingCompositingLayer(bool includeSelf) const
{
    if (includeSelf && isComposited())
        return const_cast<RenderLayer*>(this);

    for (const RenderLayer* curr = compositingContainer(this); curr; curr = compositingContainer(curr)) {
        if (curr->isComposited())
            return const_cast<RenderLayer*>(curr);
    }

    return 0;
}

RenderLayer* RenderLayer::enclosingCompositingLayerForRepaint(bool includeSelf) const
{
    if (includeSelf && isComposited() && !backing()->paintsIntoCompositedAncestor())
        return const_cast<RenderLayer*>(this);

    for (const RenderLayer* curr = compositingContainer(this); curr; curr = compositingContainer(curr)) {
        if (curr->isComposited() && !curr->backing()->paintsIntoCompositedAncestor())
            return const_cast<RenderLayer*>(curr);
    }

    return 0;
}

RenderLayer* RenderLayer::ancestorScrollingLayer() const
{
    if (!acceleratedCompositingForOverflowScrollEnabled())
        return 0;

    RenderObject* containingBlock = renderer()->containingBlock();
    if (!containingBlock)
        return 0;

    for (RenderLayer* ancestorLayer = containingBlock->enclosingLayer(); ancestorLayer; ancestorLayer = ancestorLayer->parent()) {
        if (ancestorLayer->needsCompositedScrolling())
            return ancestorLayer;
    }

    return 0;
}

RenderLayer* RenderLayer::enclosingFilterLayer(bool includeSelf) const
{
    const RenderLayer* curr = includeSelf ? this : parent();
    for (; curr; curr = curr->parent()) {
        if (curr->requiresFullLayerImageForFilters())
            return const_cast<RenderLayer*>(curr);
    }

    return 0;
}

RenderLayer* RenderLayer::enclosingFilterRepaintLayer() const
{
    for (const RenderLayer* curr = this; curr; curr = curr->parent()) {
        if ((curr != this && curr->requiresFullLayerImageForFilters()) || curr->isComposited() || curr->isRootLayer())
            return const_cast<RenderLayer*>(curr);
    }
    return 0;
}

void RenderLayer::setFilterBackendNeedsRepaintingInRect(const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;

    LayoutRect rectForRepaint = rect;
    renderer()->style()->filterOutsets().expandRect(rectForRepaint);

    RenderLayerFilterInfo* filterInfo = this->filterInfo();
    ASSERT(filterInfo);
    filterInfo->expandDirtySourceRect(rectForRepaint);

    ASSERT(filterInfo->renderer());
    if (filterInfo->renderer()->hasCustomShaderFilter()) {
        // If we have at least one custom shader, we need to update the whole bounding box of the layer, because the
        // shader can address any ouput pixel.
        // Note: This is only for output rect, so there's no need to expand the dirty source rect.
        rectForRepaint.unite(calculateLayerBounds(this));
    }

    RenderLayer* parentLayer = enclosingFilterRepaintLayer();
    ASSERT(parentLayer);
    FloatQuad repaintQuad(rectForRepaint);
    LayoutRect parentLayerRect = renderer()->localToContainerQuad(repaintQuad, parentLayer->renderer()).enclosingBoundingBox();

    if (parentLayer->isComposited()) {
        parentLayer->setBackingNeedsRepaintInRect(parentLayerRect);
        return;
    }

    if (parentLayer->paintsWithFilters()) {
        parentLayer->setFilterBackendNeedsRepaintingInRect(parentLayerRect);
        return;
    }

    if (parentLayer->isRootLayer()) {
        RenderView* view = toRenderView(parentLayer->renderer());
        view->repaintViewRectangle(parentLayerRect);
        return;
    }

    ASSERT_NOT_REACHED();
}

bool RenderLayer::hasAncestorWithFilterOutsets() const
{
    for (const RenderLayer* curr = this; curr; curr = curr->parent()) {
        RenderLayerModelObject* renderer = curr->renderer();
        if (renderer->style()->hasFilterOutsets())
            return true;
    }
    return false;
}

RenderLayer* RenderLayer::clippingRootForPainting() const
{
    if (isComposited())
        return const_cast<RenderLayer*>(this);

    const RenderLayer* current = this;
    while (current) {
        if (current->isRootLayer())
            return const_cast<RenderLayer*>(current);

        current = compositingContainer(current);
        ASSERT(current);
        if (current->transform()
            || (current->isComposited() && !current->backing()->paintsIntoCompositedAncestor())
        )
            return const_cast<RenderLayer*>(current);
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutPoint RenderLayer::absoluteToContents(const LayoutPoint& absolutePoint) const
{
    // We don't use convertToLayerCoords because it doesn't know about transforms
    return roundedLayoutPoint(renderer()->absoluteToLocal(absolutePoint, UseTransforms));
}

bool RenderLayer::cannotBlitToWindow() const
{
    if (isTransparent() || hasReflection() || hasTransform())
        return true;
    if (!parent())
        return false;
    return parent()->cannotBlitToWindow();
}

bool RenderLayer::isTransparent() const
{
    // FIXME: This seems incorrect; why would SVG layers be opaque?
    if (renderer()->node() && renderer()->node()->namespaceURI() == SVGNames::svgNamespaceURI)
        return false;

    return renderer()->isTransparent() || renderer()->hasMask();
}

RenderLayer* RenderLayer::transparentPaintingAncestor()
{
    if (isComposited())
        return 0;

    for (RenderLayer* curr = parent(); curr; curr = curr->parent()) {
        if (curr->isComposited())
            return 0;
        if (curr->isTransparent())
            return curr;
    }
    return 0;
}

enum TransparencyClipBoxBehavior {
    PaintingTransparencyClipBox,
    HitTestingTransparencyClipBox
};

enum TransparencyClipBoxMode {
    DescendantsOfTransparencyClipBox,
    RootOfTransparencyClipBox
};

static LayoutRect transparencyClipBox(const RenderLayer*, const RenderLayer* rootLayer, TransparencyClipBoxBehavior, TransparencyClipBoxMode, PaintBehavior = 0);

static void expandClipRectForDescendantsAndReflection(LayoutRect& clipRect, const RenderLayer* layer, const RenderLayer* rootLayer,
    TransparencyClipBoxBehavior transparencyBehavior, PaintBehavior paintBehavior)
{
    // If we have a mask, then the clip is limited to the border box area (and there is
    // no need to examine child layers).
    if (!layer->renderer()->hasMask()) {
        // Note: we don't have to walk z-order lists since transparent elements always establish
        // a stacking container. This means we can just walk the layer tree directly.
        for (RenderLayer* curr = layer->firstChild(); curr; curr = curr->nextSibling()) {
            if (!layer->reflection() || layer->reflectionLayer() != curr)
                clipRect.unite(transparencyClipBox(curr, rootLayer, transparencyBehavior, DescendantsOfTransparencyClipBox, paintBehavior));
        }
    }

    // If we have a reflection, then we need to account for that when we push the clip.  Reflect our entire
    // current transparencyClipBox to catch all child layers.
    // FIXME: Accelerated compositing will eventually want to do something smart here to avoid incorporating this
    // size into the parent layer.
    if (layer->renderer()->hasReflection()) {
        LayoutPoint delta;
        layer->convertToLayerCoords(rootLayer, delta);
        clipRect.move(-delta.x(), -delta.y());
        clipRect.unite(layer->renderBox()->reflectedRect(clipRect));
        clipRect.moveBy(delta);
    }
}

static LayoutRect transparencyClipBox(const RenderLayer* layer, const RenderLayer* rootLayer, TransparencyClipBoxBehavior transparencyBehavior,
    TransparencyClipBoxMode transparencyMode, PaintBehavior paintBehavior)
{
    // FIXME: Although this function completely ignores CSS-imposed clipping, we did already intersect with the
    // paintDirtyRect, and that should cut down on the amount we have to paint.  Still it
    // would be better to respect clips.

    if (rootLayer != layer && ((transparencyBehavior == PaintingTransparencyClipBox && layer->paintsWithTransform(paintBehavior))
        || (transparencyBehavior == HitTestingTransparencyClipBox && layer->hasTransform()))) {
        // The best we can do here is to use enclosed bounding boxes to establish a "fuzzy" enough clip to encompass
        // the transformed layer and all of its children.
        const RenderLayer* paginationLayer = transparencyMode == DescendantsOfTransparencyClipBox ? layer->enclosingPaginationLayer() : 0;
        const RenderLayer* rootLayerForTransform = paginationLayer ? paginationLayer : rootLayer;
        LayoutPoint delta;
        layer->convertToLayerCoords(rootLayerForTransform, delta);

        TransformationMatrix transform;
        transform.translate(delta.x(), delta.y());
        transform = transform * *layer->transform();

        // We don't use fragment boxes when collecting a transformed layer's bounding box, since it always
        // paints unfragmented.
        LayoutRect clipRect = layer->boundingBox(layer);
        expandClipRectForDescendantsAndReflection(clipRect, layer, layer, transparencyBehavior, paintBehavior);
        layer->renderer()->style()->filterOutsets().expandRect(clipRect);
        LayoutRect result = transform.mapRect(clipRect);
        if (!paginationLayer)
            return result;

        // We have to break up the transformed extent across our columns.
        // Split our box up into the actual fragment boxes that render in the columns/pages and unite those together to
        // get our true bounding box.
        RenderFlowThread* enclosingFlowThread = toRenderFlowThread(paginationLayer->renderer());
        result = enclosingFlowThread->fragmentsBoundingBox(result);

        LayoutPoint rootLayerDelta;
        paginationLayer->convertToLayerCoords(rootLayer, rootLayerDelta);
        result.moveBy(rootLayerDelta);
        return result;
    }

    LayoutRect clipRect = layer->boundingBox(rootLayer, RenderLayer::UseFragmentBoxes);
    expandClipRectForDescendantsAndReflection(clipRect, layer, rootLayer, transparencyBehavior, paintBehavior);
    layer->renderer()->style()->filterOutsets().expandRect(clipRect);
    return clipRect;
}

LayoutRect RenderLayer::paintingExtent(const RenderLayer* rootLayer, const LayoutRect& paintDirtyRect, PaintBehavior paintBehavior)
{
    return intersection(transparencyClipBox(this, rootLayer, PaintingTransparencyClipBox, RootOfTransparencyClipBox, paintBehavior), paintDirtyRect);
}

void RenderLayer::beginTransparencyLayers(GraphicsContext* context, const RenderLayer* rootLayer, const LayoutRect& paintDirtyRect, PaintBehavior paintBehavior)
{
    if (context->paintingDisabled() || (paintsWithTransparency(paintBehavior) && m_usedTransparency))
        return;

    RenderLayer* ancestor = transparentPaintingAncestor();
    if (ancestor)
        ancestor->beginTransparencyLayers(context, rootLayer, paintDirtyRect, paintBehavior);

    if (paintsWithTransparency(paintBehavior)) {
        m_usedTransparency = true;
        context->save();
        LayoutRect clipRect = paintingExtent(rootLayer, paintDirtyRect, paintBehavior);
        context->clip(clipRect);
        context->beginTransparencyLayer(renderer()->opacity());
#ifdef REVEAL_TRANSPARENCY_LAYERS
        context->setFillColor(Color(0.0f, 0.0f, 0.5f, 0.2f));
        context->fillRect(clipRect);
#endif
    }
}

void* RenderLayer::operator new(size_t sz)
{
    return partitionAlloc(Partitions::getRenderingPartition(), sz);
}

void RenderLayer::operator delete(void* ptr)
{
    partitionFree(ptr);
}

void RenderLayer::addChild(RenderLayer* child, RenderLayer* beforeChild)
{
    RenderLayer* prevSibling = beforeChild ? beforeChild->previousSibling() : lastChild();
    if (prevSibling) {
        child->setPreviousSibling(prevSibling);
        prevSibling->setNextSibling(child);
        ASSERT(prevSibling != child);
    } else
        setFirstChild(child);

    if (beforeChild) {
        beforeChild->setPreviousSibling(child);
        child->setNextSibling(beforeChild);
        ASSERT(beforeChild != child);
    } else
        setLastChild(child);

    child->setParent(this);

    if (child->isNormalFlowOnly())
        dirtyNormalFlowList();

    if (!child->isNormalFlowOnly() || child->firstChild()) {
        // Dirty the z-order list in which we are contained. The ancestorStackingContainer() can be null in the
        // case where we're building up generated content layers. This is ok, since the lists will start
        // off dirty in that case anyway.
        child->dirtyStackingContainerZOrderLists();
    }

    child->updateDescendantDependentFlags();
    if (child->m_hasVisibleContent || child->m_hasVisibleDescendant)
        setAncestorChainHasVisibleDescendant();

    if (child->isSelfPaintingLayer() || child->hasSelfPaintingLayerDescendant())
        setAncestorChainHasSelfPaintingLayerDescendant();

    if (subtreeContainsOutOfFlowPositionedLayer(child)) {
        // Now that the out of flow positioned descendant is in the tree, we
        // need to tell the compositor to reevaluate the compositing
        // requirements since we may be able to mark more layers as having
        // an 'unclipped' descendant.
        compositor()->setNeedsUpdateCompositingRequirementsState();
        setAncestorChainHasOutOfFlowPositionedDescendant();
    }

    // When we first dirty a layer, we will also dirty all the siblings in that
    // layer's stacking context. We need to manually do it here as well, in case
    // we're adding this layer after the stacking context has already been
    // updated.
    child->m_canBePromotedToStackingContainerDirty = true;
    compositor()->layerWasAdded(this, child);
}

RenderLayer* RenderLayer::removeChild(RenderLayer* oldChild)
{
    if (!renderer()->documentBeingDestroyed())
        compositor()->layerWillBeRemoved(this, oldChild);

    // remove the child
    if (oldChild->previousSibling())
        oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
    if (oldChild->nextSibling())
        oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

    if (m_first == oldChild)
        m_first = oldChild->nextSibling();
    if (m_last == oldChild)
        m_last = oldChild->previousSibling();

    if (oldChild->isNormalFlowOnly())
        dirtyNormalFlowList();
    if (!oldChild->isNormalFlowOnly() || oldChild->firstChild()) {
        // Dirty the z-order list in which we are contained.  When called via the
        // reattachment process in removeOnlyThisLayer, the layer may already be disconnected
        // from the main layer tree, so we need to null-check the |stackingContainer| value.
        oldChild->dirtyStackingContainerZOrderLists();
    }

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);

    oldChild->updateDescendantDependentFlags();
    if (subtreeContainsOutOfFlowPositionedLayer(oldChild)) {
        // It may now be the case that a layer no longer has an unclipped
        // descendant. Let the compositor know that it needs to reevaluate
        // its compositing requirements to check this.
        compositor()->setNeedsUpdateCompositingRequirementsState();
        dirtyAncestorChainHasOutOfFlowPositionedDescendantStatus();
    }

    if (oldChild->m_hasVisibleContent || oldChild->m_hasVisibleDescendant)
        dirtyAncestorChainVisibleDescendantStatus();

    if (oldChild->isSelfPaintingLayer() || oldChild->hasSelfPaintingLayerDescendant())
        dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

    return oldChild;
}

void RenderLayer::removeOnlyThisLayer()
{
    if (!m_parent)
        return;

    // Mark that we are about to lose our layer. This makes render tree
    // walks ignore this layer while we're removing it.
    m_renderer->setHasLayer(false);

    compositor()->layerWillBeRemoved(m_parent, this);

    // Dirty the clip rects.
    clearClipRectsIncludingDescendants();

    RenderLayer* nextSib = nextSibling();

    // Remove the child reflection layer before moving other child layers.
    // The reflection layer should not be moved to the parent.
    if (reflection())
        removeChild(reflectionLayer());

    // Now walk our kids and reattach them to our parent.
    RenderLayer* current = m_first;
    while (current) {
        RenderLayer* next = current->nextSibling();
        removeChild(current);
        m_parent->addChild(current, nextSib);
        current->setRepaintStatus(NeedsFullRepaint);
        // updateLayerPositions depends on hasLayer() already being false for proper layout.
        ASSERT(!renderer()->hasLayer());
        current->updateLayerPositions(0); // FIXME: use geometry map.
        current = next;
    }

    // Remove us from the parent.
    m_parent->removeChild(this);
    m_renderer->destroyLayer();
}

void RenderLayer::insertOnlyThisLayer()
{
    if (!m_parent && renderer()->parent()) {
        // We need to connect ourselves when our renderer() has a parent.
        // Find our enclosingLayer and add ourselves.
        RenderLayer* parentLayer = renderer()->parent()->enclosingLayer();
        ASSERT(parentLayer);
        RenderLayer* beforeChild = parentLayer->reflectionLayer() != this ? renderer()->parent()->findNextLayer(parentLayer, renderer()) : 0;
        parentLayer->addChild(this, beforeChild);
    }

    // Remove all descendant layers from the hierarchy and add them to the new position.
    for (RenderObject* curr = renderer()->firstChild(); curr; curr = curr->nextSibling())
        curr->moveLayers(m_parent, this);

    // Clear out all the clip rects.
    clearClipRectsIncludingDescendants();
}

void RenderLayer::convertToPixelSnappedLayerCoords(const RenderLayer* ancestorLayer, IntPoint& roundedLocation) const
{
    LayoutPoint location = roundedLocation;
    convertToLayerCoords(ancestorLayer, location);
    roundedLocation = roundedIntPoint(location);
}

void RenderLayer::convertToPixelSnappedLayerCoords(const RenderLayer* ancestorLayer, IntRect& roundedRect) const
{
    LayoutRect rect = roundedRect;
    convertToLayerCoords(ancestorLayer, rect);
    roundedRect = pixelSnappedIntRect(rect);
}

// Returns the layer reached on the walk up towards the ancestor.
static inline const RenderLayer* accumulateOffsetTowardsAncestor(const RenderLayer* layer, const RenderLayer* ancestorLayer, LayoutPoint& location)
{
    ASSERT(ancestorLayer != layer);

    const RenderLayerModelObject* renderer = layer->renderer();
    EPosition position = renderer->style()->position();

    // FIXME: Special casing RenderFlowThread so much for fixed positioning here is not great.
    RenderFlowThread* fixedFlowThreadContainer = position == FixedPosition ? renderer->flowThreadContainingBlock() : 0;
    if (fixedFlowThreadContainer && !fixedFlowThreadContainer->isOutOfFlowPositioned())
        fixedFlowThreadContainer = 0;

    // FIXME: Positioning of out-of-flow(fixed, absolute) elements collected in a RenderFlowThread
    // may need to be revisited in a future patch.
    // If the fixed renderer is inside a RenderFlowThread, we should not compute location using localToAbsolute,
    // since localToAbsolute maps the coordinates from named flow to regions coordinates and regions can be
    // positioned in a completely different place in the viewport (RenderView).
    if (position == FixedPosition && !fixedFlowThreadContainer && (!ancestorLayer || ancestorLayer == renderer->view()->layer())) {
        // If the fixed layer's container is the root, just add in the offset of the view. We can obtain this by calling
        // localToAbsolute() on the RenderView.
        FloatPoint absPos = renderer->localToAbsolute(FloatPoint(), IsFixed);
        location += LayoutSize(absPos.x(), absPos.y());
        return ancestorLayer;
    }

    // For the fixed positioned elements inside a render flow thread, we should also skip the code path below
    // Otherwise, for the case of ancestorLayer == rootLayer and fixed positioned element child of a transformed
    // element in render flow thread, we will hit the fixed positioned container before hitting the ancestor layer.
    if (position == FixedPosition && !fixedFlowThreadContainer) {
        // For a fixed layers, we need to walk up to the root to see if there's a fixed position container
        // (e.g. a transformed layer). It's an error to call convertToLayerCoords() across a layer with a transform,
        // so we should always find the ancestor at or before we find the fixed position container.
        RenderLayer* fixedPositionContainerLayer = 0;
        bool foundAncestor = false;
        for (RenderLayer* currLayer = layer->parent(); currLayer; currLayer = currLayer->parent()) {
            if (currLayer == ancestorLayer)
                foundAncestor = true;

            if (isFixedPositionedContainer(currLayer)) {
                fixedPositionContainerLayer = currLayer;
                ASSERT_UNUSED(foundAncestor, foundAncestor);
                break;
            }
        }

        ASSERT(fixedPositionContainerLayer); // We should have hit the RenderView's layer at least.

        if (fixedPositionContainerLayer != ancestorLayer) {
            LayoutPoint fixedContainerCoords;
            layer->convertToLayerCoords(fixedPositionContainerLayer, fixedContainerCoords);

            LayoutPoint ancestorCoords;
            ancestorLayer->convertToLayerCoords(fixedPositionContainerLayer, ancestorCoords);

            location += (fixedContainerCoords - ancestorCoords);
        } else {
            location += toSize(layer->location());
        }
        return ancestorLayer;
    }

    RenderLayer* parentLayer;
    if (position == AbsolutePosition || position == FixedPosition) {
        // Do what enclosingPositionedAncestor() does, but check for ancestorLayer along the way.
        parentLayer = layer->parent();
        bool foundAncestorFirst = false;
        while (parentLayer) {
            // RenderFlowThread is a positioned container, child of RenderView, positioned at (0,0).
            // This implies that, for out-of-flow positioned elements inside a RenderFlowThread,
            // we are bailing out before reaching root layer.
            if (isPositionedContainer(parentLayer))
                break;

            if (parentLayer == ancestorLayer) {
                foundAncestorFirst = true;
                break;
            }

            parentLayer = parentLayer->parent();
        }

        // We should not reach RenderView layer past the RenderFlowThread layer for any
        // children of the RenderFlowThread.
        if (renderer->flowThreadContainingBlock() && !layer->isOutOfFlowRenderFlowThread())
            ASSERT(parentLayer != renderer->view()->layer());

        if (foundAncestorFirst) {
            // Found ancestorLayer before the abs. positioned container, so compute offset of both relative
            // to enclosingPositionedAncestor and subtract.
            RenderLayer* positionedAncestor = parentLayer->enclosingPositionedAncestor();

            LayoutPoint thisCoords;
            layer->convertToLayerCoords(positionedAncestor, thisCoords);

            LayoutPoint ancestorCoords;
            ancestorLayer->convertToLayerCoords(positionedAncestor, ancestorCoords);

            location += (thisCoords - ancestorCoords);
            return ancestorLayer;
        }
    } else
        parentLayer = layer->parent();

    if (!parentLayer)
        return 0;

    location += toSize(layer->location());
    return parentLayer;
}

void RenderLayer::convertToLayerCoords(const RenderLayer* ancestorLayer, LayoutPoint& location) const
{
    if (ancestorLayer == this)
        return;

    const RenderLayer* currLayer = this;
    while (currLayer && currLayer != ancestorLayer)
        currLayer = accumulateOffsetTowardsAncestor(currLayer, ancestorLayer, location);
}

void RenderLayer::convertToLayerCoords(const RenderLayer* ancestorLayer, LayoutRect& rect) const
{
    LayoutPoint delta;
    convertToLayerCoords(ancestorLayer, delta);
    rect.move(-delta.x(), -delta.y());
}

bool RenderLayer::usesCompositedScrolling() const
{
    RenderBox* box = renderBox();

    // Scroll form controls on the main thread so they exhibit correct touch scroll event bubbling
    if (box && (box->isIntristicallyScrollable(VerticalScrollbar) || box->isIntristicallyScrollable(HorizontalScrollbar)))
        return false;

    return isComposited() && backing()->scrollingLayer();
}

bool RenderLayer::needsCompositedScrolling() const
{
    if (!compositorDrivenAcceleratedScrollingEnabled())
        return needsToBeStackingContainer();
    if (FrameView* frameView = renderer()->view()->frameView())
        return frameView->containsScrollableArea(scrollableArea());
    return false;
}

bool RenderLayer::needsToBeStackingContainer() const
{
    switch (m_forceNeedsCompositedScrolling) {
    case DoNotForceCompositedScrolling:
        return m_needsCompositedScrolling;
    case CompositedScrollingAlwaysOn:
        return true;
    case CompositedScrollingAlwaysOff:
        return false;
    }

    ASSERT_NOT_REACHED();
    return m_needsCompositedScrolling;
}

void RenderLayer::updateNeedsCompositedScrolling()
{
    TRACE_EVENT0("comp-scroll", "RenderLayer::updateNeedsCompositedScrolling");

    updateCanBeStackingContainer();
    updateDescendantDependentFlags();

    ASSERT(renderer()->view()->frameView() && renderer()->view()->frameView()->containsScrollableArea(scrollableArea()));
    bool needsCompositedScrolling = acceleratedCompositingForOverflowScrollEnabled()
        && canBeStackingContainer()
        && !hasUnclippedDescendant();

    // We gather a boolean value for use with Google UMA histograms to
    // quantify the actual effects of a set of patches attempting to
    // relax composited scrolling requirements, thereby increasing the
    // number of composited overflow divs.
    if (acceleratedCompositingForOverflowScrollEnabled())
        HistogramSupport::histogramEnumeration("Renderer.NeedsCompositedScrolling", needsCompositedScrolling, 2);

    setNeedsCompositedScrolling(needsCompositedScrolling);
}

enum CompositedScrollingHistogramBuckets {
    IsScrollableAreaBucket = 0,
    NeedsToBeStackingContainerBucket = 1,
    WillUseCompositedScrollingBucket = 2,
    CompositedScrollingHistogramMax = 3
};

void RenderLayer::setNeedsCompositedScrolling(bool needsCompositedScrolling)
{
    if (m_needsCompositedScrolling == needsCompositedScrolling)
        return;

    // Count the total number of RenderLayers which need to be stacking
    // containers some point. This should be recorded at most once per
    // RenderLayer, so we check m_needsCompositedScrollingHasBeenRecorded.
    if (acceleratedCompositingForOverflowScrollEnabled() && !m_needsCompositedScrollingHasBeenRecorded) {
        HistogramSupport::histogramEnumeration("Renderer.CompositedScrolling", NeedsToBeStackingContainerBucket, CompositedScrollingHistogramMax);
        m_needsCompositedScrollingHasBeenRecorded = true;
    }

    // Count the total number of RenderLayers which need composited scrolling at
    // some point. This should be recorded at most once per RenderLayer, so we
    // check m_willUseCompositedScrollingHasBeenRecorded.
    //
    // FIXME: Currently, this computes the exact same value as the above.
    // However, it will soon be expanded to cover more than just stacking
    // containers (see crbug.com/249354). When this happens, we should see a
    // spike in "WillUseCompositedScrolling", while "NeedsToBeStackingContainer"
    // will remain relatively static.
    if (acceleratedCompositingForOverflowScrollEnabled() && !m_willUseCompositedScrollingHasBeenRecorded) {
        HistogramSupport::histogramEnumeration("Renderer.CompositedScrolling", WillUseCompositedScrollingBucket, CompositedScrollingHistogramMax);
        m_willUseCompositedScrollingHasBeenRecorded = true;
    }

    m_needsCompositedScrolling = needsCompositedScrolling;

    // Note, the z-order lists may need to be rebuilt, but our code guarantees
    // that we have not affected stacking, so we will not dirty
    // m_canBePromotedToStackingContainer for either us or our stacking context
    // or container.
    didUpdateNeedsCompositedScrolling();
}

void RenderLayer::setForceNeedsCompositedScrolling(RenderLayer::ForceNeedsCompositedScrollingMode mode)
{
    if (m_forceNeedsCompositedScrolling == mode)
        return;

    m_forceNeedsCompositedScrolling = mode;
    didUpdateNeedsCompositedScrolling();
}

void RenderLayer::didUpdateNeedsCompositedScrolling()
{
    updateIsNormalFlowOnly();
    updateSelfPaintingLayer();

    if (isStackingContainer())
        dirtyZOrderLists();
    else
        clearZOrderLists();

    dirtyStackingContainerZOrderLists();

    compositor()->setShouldReevaluateCompositingAfterLayout();
    compositor()->setCompositingLayersNeedRebuild();
}

static inline int adjustedScrollDelta(int beginningDelta) {
    // This implemention matches Firefox's.
    // http://mxr.mozilla.org/firefox/source/toolkit/content/widgets/browser.xml#856.
    const int speedReducer = 12;

    int adjustedDelta = beginningDelta / speedReducer;
    if (adjustedDelta > 1)
        adjustedDelta = static_cast<int>(adjustedDelta * sqrt(static_cast<double>(adjustedDelta))) - 1;
    else if (adjustedDelta < -1)
        adjustedDelta = static_cast<int>(adjustedDelta * sqrt(static_cast<double>(-adjustedDelta))) + 1;

    return adjustedDelta;
}

static inline IntSize adjustedScrollDelta(const IntSize& delta)
{
    return IntSize(adjustedScrollDelta(delta.width()), adjustedScrollDelta(delta.height()));
}

void RenderLayer::panScrollFromPoint(const IntPoint& sourcePoint)
{
    Frame* frame = renderer()->frame();
    if (!frame)
        return;

    IntPoint lastKnownMousePosition = frame->eventHandler()->lastKnownMousePosition();

    // We need to check if the last known mouse position is out of the window. When the mouse is out of the window, the position is incoherent
    static IntPoint previousMousePosition;
    if (lastKnownMousePosition.x() < 0 || lastKnownMousePosition.y() < 0)
        lastKnownMousePosition = previousMousePosition;
    else
        previousMousePosition = lastKnownMousePosition;

    IntSize delta = lastKnownMousePosition - sourcePoint;

    if (abs(delta.width()) <= ScrollView::noPanScrollRadius) // at the center we let the space for the icon
        delta.setWidth(0);
    if (abs(delta.height()) <= ScrollView::noPanScrollRadius)
        delta.setHeight(0);

    scrollByRecursively(adjustedScrollDelta(delta), ScrollOffsetClamped);
}

void RenderLayer::scrollByRecursively(const IntSize& delta, ScrollOffsetClamping clamp)
{
    if (delta.isZero())
        return;

    bool restrictedByLineClamp = false;
    if (renderer()->parent())
        restrictedByLineClamp = !renderer()->parent()->style()->lineClamp().isNone();

    if (renderer()->hasOverflowClip() && !restrictedByLineClamp) {
        IntSize newScrollOffset = adjustedScrollOffset() + delta;
        m_scrollableArea->scrollToOffset(newScrollOffset, clamp);

        // If this layer can't do the scroll we ask the next layer up that can scroll to try
        IntSize remainingScrollOffset = newScrollOffset - adjustedScrollOffset();
        if (!remainingScrollOffset.isZero() && renderer()->parent()) {
            if (RenderLayer* scrollableLayer = enclosingScrollableLayer())
                scrollableLayer->scrollByRecursively(remainingScrollOffset, clamp);

            Frame* frame = renderer()->frame();
            if (frame && frame->page())
                frame->page()->updateAutoscrollRenderer();
        }
    } else if (renderer()->view()->frameView()) {
        // If we are here, we were called on a renderer that can be programmatically scrolled, but doesn't
        // have an overflow clip. Which means that it is a document node that can be scrolled.
        renderer()->view()->frameView()->scrollBy(delta);

        // FIXME: If we didn't scroll the whole way, do we want to try looking at the frames ownerElement?
        // https://bugs.webkit.org/show_bug.cgi?id=28237
    }
}

void RenderLayer::scrollToOffset(const IntSize& scrollOffset, ScrollOffsetClamping clamp)
{
    m_scrollableArea->scrollToOffset(scrollOffset, clamp);
}

static inline bool frameElementAndViewPermitScroll(HTMLFrameElementBase* frameElementBase, FrameView* frameView)
{
    // If scrollbars aren't explicitly forbidden, permit scrolling.
    if (frameElementBase && frameElementBase->scrollingMode() != ScrollbarAlwaysOff)
        return true;

    // If scrollbars are forbidden, user initiated scrolls should obviously be ignored.
    if (frameView->wasScrolledByUser())
        return false;

    // Forbid autoscrolls when scrollbars are off, but permits other programmatic scrolls,
    // like navigation to an anchor.
    Page* page = frameView->frame().page();
    if (!page)
        return false;
    return !page->autoscrollInProgress();
}

void RenderLayer::scrollRectToVisible(const LayoutRect& rect, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
{
    RenderLayer* parentLayer = 0;
    LayoutRect newRect = rect;

    // We may end up propagating a scroll event. It is important that we suspend events until
    // the end of the function since they could delete the layer or the layer's renderer().
    FrameView* frameView = renderer()->document().view();
    if (frameView)
        frameView->pauseScheduledEvents();

    bool restrictedByLineClamp = false;
    if (renderer()->parent()) {
        parentLayer = renderer()->parent()->enclosingLayer();
        restrictedByLineClamp = !renderer()->parent()->style()->lineClamp().isNone();
    }

    if (renderer()->hasOverflowClip() && !restrictedByLineClamp) {
        // Don't scroll to reveal an overflow layer that is restricted by the -webkit-line-clamp property.
        // This will prevent us from revealing text hidden by the slider in Safari RSS.
        RenderBox* box = renderBox();
        ASSERT(box);
        LayoutRect localExposeRect(box->absoluteToLocalQuad(FloatQuad(FloatRect(rect)), UseTransforms).boundingBox());
        LayoutRect layerBounds(0, 0, box->clientWidth(), box->clientHeight());
        LayoutRect r = getRectToExpose(layerBounds, localExposeRect, alignX, alignY);

        IntSize clampedScrollOffset = m_scrollableArea->clampScrollOffset(adjustedScrollOffset() + toIntSize(roundedIntRect(r).location()));
        if (clampedScrollOffset != adjustedScrollOffset()) {
            IntSize oldScrollOffset = adjustedScrollOffset();
            m_scrollableArea->scrollToOffset(clampedScrollOffset);
            IntSize scrollOffsetDifference = adjustedScrollOffset() - oldScrollOffset;
            localExposeRect.move(-scrollOffsetDifference);
            newRect = LayoutRect(box->localToAbsoluteQuad(FloatQuad(FloatRect(localExposeRect)), UseTransforms).boundingBox());
        }
    } else if (!parentLayer && renderer()->isBox() && renderBox()->canBeProgramaticallyScrolled()) {
        if (frameView) {
            Element* ownerElement = renderer()->document().ownerElement();

            if (ownerElement && ownerElement->renderer()) {
                HTMLFrameElementBase* frameElementBase = 0;

                if (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag))
                    frameElementBase = toHTMLFrameElementBase(ownerElement);

                if (frameElementAndViewPermitScroll(frameElementBase, frameView)) {
                    LayoutRect viewRect = frameView->visibleContentRect();
                    LayoutRect exposeRect = getRectToExpose(viewRect, rect, alignX, alignY);

                    int xOffset = roundToInt(exposeRect.x());
                    int yOffset = roundToInt(exposeRect.y());
                    // Adjust offsets if they're outside of the allowable range.
                    xOffset = max(0, min(frameView->contentsWidth(), xOffset));
                    yOffset = max(0, min(frameView->contentsHeight(), yOffset));

                    frameView->setScrollPosition(IntPoint(xOffset, yOffset));
                    if (frameView->safeToPropagateScrollToParent()) {
                        parentLayer = ownerElement->renderer()->enclosingLayer();
                        // FIXME: This doesn't correctly convert the rect to
                        // absolute coordinates in the parent.
                        newRect.setX(rect.x() - frameView->scrollX() + frameView->x());
                        newRect.setY(rect.y() - frameView->scrollY() + frameView->y());
                    } else
                        parentLayer = 0;
                }
            } else {
                LayoutRect viewRect = frameView->visibleContentRect();
                LayoutRect r = getRectToExpose(viewRect, rect, alignX, alignY);
                frameView->setScrollPosition(roundedIntPoint(r.location()));
            }
        }
    }

    if (renderer()->frame()->page()->autoscrollInProgress())
        parentLayer = enclosingScrollableLayer();

    if (parentLayer)
        parentLayer->scrollRectToVisible(newRect, alignX, alignY);

    if (frameView)
        frameView->resumeScheduledEvents();
}

void RenderLayer::updateCompositingLayersAfterScroll()
{
    if (compositor()->inCompositingMode()) {
        // Our stacking container is guaranteed to contain all of our descendants that may need
        // repositioning, so update compositing layers from there.
        if (RenderLayer* compositingAncestor = ancestorStackingContainer()->enclosingCompositingLayer()) {
            if (usesCompositedScrolling())
                compositor()->updateCompositingLayers(CompositingUpdateOnCompositedScroll, compositingAncestor);
            else
                compositor()->updateCompositingLayers(CompositingUpdateOnScroll, compositingAncestor);
        }
    }
}

LayoutRect RenderLayer::getRectToExpose(const LayoutRect &visibleRect, const LayoutRect &exposeRect, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
{
    // Determine the appropriate X behavior.
    ScrollBehavior scrollX;
    LayoutRect exposeRectX(exposeRect.x(), visibleRect.y(), exposeRect.width(), visibleRect.height());
    LayoutUnit intersectWidth = intersection(visibleRect, exposeRectX).width();
    if (intersectWidth == exposeRect.width() || intersectWidth >= MIN_INTERSECT_FOR_REVEAL)
        // If the rectangle is fully visible, use the specified visible behavior.
        // If the rectangle is partially visible, but over a certain threshold,
        // then treat it as fully visible to avoid unnecessary horizontal scrolling
        scrollX = ScrollAlignment::getVisibleBehavior(alignX);
    else if (intersectWidth == visibleRect.width()) {
        // If the rect is bigger than the visible area, don't bother trying to center. Other alignments will work.
        scrollX = ScrollAlignment::getVisibleBehavior(alignX);
        if (scrollX == alignCenter)
            scrollX = noScroll;
    } else if (intersectWidth > 0)
        // If the rectangle is partially visible, but not above the minimum threshold, use the specified partial behavior
        scrollX = ScrollAlignment::getPartialBehavior(alignX);
    else
        scrollX = ScrollAlignment::getHiddenBehavior(alignX);
    // If we're trying to align to the closest edge, and the exposeRect is further right
    // than the visibleRect, and not bigger than the visible area, then align with the right.
    if (scrollX == alignToClosestEdge && exposeRect.maxX() > visibleRect.maxX() && exposeRect.width() < visibleRect.width())
        scrollX = alignRight;

    // Given the X behavior, compute the X coordinate.
    LayoutUnit x;
    if (scrollX == noScroll)
        x = visibleRect.x();
    else if (scrollX == alignRight)
        x = exposeRect.maxX() - visibleRect.width();
    else if (scrollX == alignCenter)
        x = exposeRect.x() + (exposeRect.width() - visibleRect.width()) / 2;
    else
        x = exposeRect.x();

    // Determine the appropriate Y behavior.
    ScrollBehavior scrollY;
    LayoutRect exposeRectY(visibleRect.x(), exposeRect.y(), visibleRect.width(), exposeRect.height());
    LayoutUnit intersectHeight = intersection(visibleRect, exposeRectY).height();
    if (intersectHeight == exposeRect.height())
        // If the rectangle is fully visible, use the specified visible behavior.
        scrollY = ScrollAlignment::getVisibleBehavior(alignY);
    else if (intersectHeight == visibleRect.height()) {
        // If the rect is bigger than the visible area, don't bother trying to center. Other alignments will work.
        scrollY = ScrollAlignment::getVisibleBehavior(alignY);
        if (scrollY == alignCenter)
            scrollY = noScroll;
    } else if (intersectHeight > 0)
        // If the rectangle is partially visible, use the specified partial behavior
        scrollY = ScrollAlignment::getPartialBehavior(alignY);
    else
        scrollY = ScrollAlignment::getHiddenBehavior(alignY);
    // If we're trying to align to the closest edge, and the exposeRect is further down
    // than the visibleRect, and not bigger than the visible area, then align with the bottom.
    if (scrollY == alignToClosestEdge && exposeRect.maxY() > visibleRect.maxY() && exposeRect.height() < visibleRect.height())
        scrollY = alignBottom;

    // Given the Y behavior, compute the Y coordinate.
    LayoutUnit y;
    if (scrollY == noScroll)
        y = visibleRect.y();
    else if (scrollY == alignBottom)
        y = exposeRect.maxY() - visibleRect.height();
    else if (scrollY == alignCenter)
        y = exposeRect.y() + (exposeRect.height() - visibleRect.height()) / 2;
    else
        y = exposeRect.y();

    return LayoutRect(LayoutPoint(x, y), visibleRect.size());
}

void RenderLayer::autoscroll(const IntPoint& position)
{
    Frame* frame = renderer()->frame();
    if (!frame)
        return;

    FrameView* frameView = frame->view();
    if (!frameView)
        return;

    IntPoint currentDocumentPosition = frameView->windowToContents(position);
    scrollRectToVisible(LayoutRect(currentDocumentPosition, LayoutSize(1, 1)), ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded);
}

bool RenderLayer::canResize() const
{
    if (!renderer())
        return false;
    // We need a special case for <iframe> because they never have
    // hasOverflowClip(). However, they do "implicitly" clip their contents, so
    // we want to allow resizing them also.
    return (renderer()->hasOverflowClip() || renderer()->isRenderIFrame()) && renderer()->style()->resize() != RESIZE_NONE;
}

void RenderLayer::resize(const PlatformEvent& evt, const LayoutSize& oldOffset)
{
    // FIXME: This should be possible on generated content but is not right now.
    if (!inResizeMode() || !canResize() || !renderer()->node())
        return;

    ASSERT(renderer()->node()->isElementNode());
    Element* element = toElement(renderer()->node());
    RenderBox* renderer = toRenderBox(element->renderer());

    Document& document = element->document();

    IntPoint pos;
    const PlatformGestureEvent* gevt = 0;

    switch (evt.type()) {
    case PlatformEvent::MouseMoved:
        if (!document.frame()->eventHandler()->mousePressed())
            return;
        pos = static_cast<const PlatformMouseEvent*>(&evt)->position();
        break;
    case PlatformEvent::GestureScrollUpdate:
    case PlatformEvent::GestureScrollUpdateWithoutPropagation:
        pos = static_cast<const PlatformGestureEvent*>(&evt)->position();
        gevt = static_cast<const PlatformGestureEvent*>(&evt);
        pos = gevt->position();
        pos.move(gevt->deltaX(), gevt->deltaY());
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    float zoomFactor = renderer->style()->effectiveZoom();

    LayoutSize newOffset = offsetFromResizeCorner(document.view()->windowToContents(pos));
    newOffset.setWidth(newOffset.width() / zoomFactor);
    newOffset.setHeight(newOffset.height() / zoomFactor);

    LayoutSize currentSize = LayoutSize(renderer->width() / zoomFactor, renderer->height() / zoomFactor);
    LayoutSize minimumSize = element->minimumSizeForResizing().shrunkTo(currentSize);
    element->setMinimumSizeForResizing(minimumSize);

    LayoutSize adjustedOldOffset = LayoutSize(oldOffset.width() / zoomFactor, oldOffset.height() / zoomFactor);
    if (renderer->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        newOffset.setWidth(-newOffset.width());
        adjustedOldOffset.setWidth(-adjustedOldOffset.width());
    }

    LayoutSize difference = (currentSize + newOffset - adjustedOldOffset).expandedTo(minimumSize) - currentSize;

    bool isBoxSizingBorder = renderer->style()->boxSizing() == BORDER_BOX;

    EResize resize = renderer->style()->resize();
    if (resize != RESIZE_VERTICAL && difference.width()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            element->setInlineStyleProperty(CSSPropertyMarginLeft, renderer->marginLeft() / zoomFactor, CSSPrimitiveValue::CSS_PX);
            element->setInlineStyleProperty(CSSPropertyMarginRight, renderer->marginRight() / zoomFactor, CSSPrimitiveValue::CSS_PX);
        }
        LayoutUnit baseWidth = renderer->width() - (isBoxSizingBorder ? LayoutUnit() : renderer->borderAndPaddingWidth());
        baseWidth = baseWidth / zoomFactor;
        element->setInlineStyleProperty(CSSPropertyWidth, roundToInt(baseWidth + difference.width()), CSSPrimitiveValue::CSS_PX);
    }

    if (resize != RESIZE_HORIZONTAL && difference.height()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            element->setInlineStyleProperty(CSSPropertyMarginTop, renderer->marginTop() / zoomFactor, CSSPrimitiveValue::CSS_PX);
            element->setInlineStyleProperty(CSSPropertyMarginBottom, renderer->marginBottom() / zoomFactor, CSSPrimitiveValue::CSS_PX);
        }
        LayoutUnit baseHeight = renderer->height() - (isBoxSizingBorder ? LayoutUnit() : renderer->borderAndPaddingHeight());
        baseHeight = baseHeight / zoomFactor;
        element->setInlineStyleProperty(CSSPropertyHeight, roundToInt(baseHeight + difference.height()), CSSPrimitiveValue::CSS_PX);
    }

    document.updateLayout();

    // FIXME (Radar 4118564): We should also autoscroll the window as necessary to keep the point under the cursor in view.
}

int RenderLayer::scrollSize(ScrollbarOrientation orientation) const
{
    IntSize scrollDimensions = scrollableArea()->maximumScrollPosition() - scrollableArea()->minimumScrollPosition();
    return (orientation == HorizontalScrollbar) ? scrollDimensions.width() : scrollDimensions.height();
}

IntSize RenderLayer::overhangAmount() const
{
    return IntSize();
}

bool RenderLayer::isActive() const
{
    Page* page = renderer()->frame()->page();
    return page && page->focusController().isActive();
}

static int cornerStart(const RenderStyle* style, int minX, int maxX, int thickness)
{
    if (style->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + style->borderLeftWidth();
    return maxX - thickness - style->borderRightWidth();
}

static IntRect cornerRect(const RenderStyle* style, const Scrollbar* horizontalScrollbar, const Scrollbar* verticalScrollbar, const IntRect& bounds)
{
    int horizontalThickness;
    int verticalThickness;
    if (!verticalScrollbar && !horizontalScrollbar) {
        // FIXME: This isn't right.  We need to know the thickness of custom scrollbars
        // even when they don't exist in order to set the resizer square size properly.
        horizontalThickness = ScrollbarTheme::theme()->scrollbarThickness();
        verticalThickness = horizontalThickness;
    } else if (verticalScrollbar && !horizontalScrollbar) {
        horizontalThickness = verticalScrollbar->width();
        verticalThickness = horizontalThickness;
    } else if (horizontalScrollbar && !verticalScrollbar) {
        verticalThickness = horizontalScrollbar->height();
        horizontalThickness = verticalThickness;
    } else {
        horizontalThickness = verticalScrollbar->width();
        verticalThickness = horizontalScrollbar->height();
    }
    return IntRect(cornerStart(style, bounds.x(), bounds.maxX(), horizontalThickness),
                   bounds.maxY() - verticalThickness - style->borderBottomWidth(),
                   horizontalThickness, verticalThickness);
}

IntRect RenderLayer::scrollCornerRect() const
{
    // We have a scrollbar corner when a scrollbar is visible and not filling the entire length of the box.
    // This happens when:
    // (a) A resizer is present and at least one scrollbar is present
    // (b) Both scrollbars are present.
    bool hasHorizontalBar = horizontalScrollbar();
    bool hasVerticalBar = verticalScrollbar();
    bool hasResizer = renderer()->style()->resize() != RESIZE_NONE;
    if ((hasHorizontalBar && hasVerticalBar) || (hasResizer && (hasHorizontalBar || hasVerticalBar)))
        return cornerRect(renderer()->style(), horizontalScrollbar(), verticalScrollbar(), renderBox()->pixelSnappedBorderBoxRect());
    return IntRect();
}

IntRect RenderLayer::resizerCornerRect(const IntRect& bounds, ResizerHitTestType resizerHitTestType) const
{
    ASSERT(renderer()->isBox());
    if (renderer()->style()->resize() == RESIZE_NONE)
        return IntRect();
    IntRect corner = cornerRect(renderer()->style(), horizontalScrollbar(), verticalScrollbar(), bounds);

    if (resizerHitTestType == ResizerForTouch) {
        // We make the resizer virtually larger for touch hit testing. With the
        // expanding ratio k = ResizerControlExpandRatioForTouch, we first move
        // the resizer rect (of width w & height h), by (-w * (k-1), -h * (k-1)),
        // then expand the rect by new_w/h = w/h * k.
        int expand_ratio = ResizerControlExpandRatioForTouch - 1;
        corner.move(-corner.width() * expand_ratio, -corner.height() * expand_ratio);
        corner.expand(corner.width() * expand_ratio, corner.height() * expand_ratio);
    }

    return corner;
}

IntRect RenderLayer::scrollCornerAndResizerRect() const
{
    RenderBox* box = renderBox();
    if (!box)
        return IntRect();
    IntRect scrollCornerAndResizer = scrollCornerRect();
    if (scrollCornerAndResizer.isEmpty())
        scrollCornerAndResizer = resizerCornerRect(box->pixelSnappedBorderBoxRect(), ResizerForPointer);
    return scrollCornerAndResizer;
}

bool RenderLayer::isScrollCornerVisible() const
{
    ASSERT(renderer()->isBox());
    return !scrollCornerRect().isEmpty();
}

IntRect RenderLayer::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntRect& scrollbarRect) const
{
    RenderView* view = renderer()->view();
    if (!view)
        return scrollbarRect;

    IntRect rect = scrollbarRect;
    rect.move(scrollbarOffset(scrollbar));

    return view->frameView()->convertFromRenderer(renderer(), rect);
}

IntRect RenderLayer::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntRect& parentRect) const
{
    RenderView* view = renderer()->view();
    if (!view)
        return parentRect;

    IntRect rect = view->frameView()->convertToRenderer(renderer(), parentRect);
    rect.move(-scrollbarOffset(scrollbar));
    return rect;
}

IntPoint RenderLayer::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntPoint& scrollbarPoint) const
{
    RenderView* view = renderer()->view();
    if (!view)
        return scrollbarPoint;

    IntPoint point = scrollbarPoint;
    point.move(scrollbarOffset(scrollbar));
    return view->frameView()->convertFromRenderer(renderer(), point);
}

IntPoint RenderLayer::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& parentPoint) const
{
    RenderView* view = renderer()->view();
    if (!view)
        return parentPoint;

    IntPoint point = view->frameView()->convertToRenderer(renderer(), parentPoint);

    point.move(-scrollbarOffset(scrollbar));
    return point;
}

int RenderLayer::visibleHeight() const
{
    return m_layerSize.height();
}

int RenderLayer::visibleWidth() const
{
    return m_layerSize.width();
}

bool RenderLayer::shouldSuspendScrollAnimations() const
{
    RenderView* view = renderer()->view();
    if (!view)
        return true;
    return view->frameView()->shouldSuspendScrollAnimations();
}

bool RenderLayer::scrollbarsCanBeActive() const
{
    RenderView* view = renderer()->view();
    if (!view)
        return false;
    return view->frameView()->scrollbarsCanBeActive();
}

IntPoint RenderLayer::lastKnownMousePosition() const
{
    return renderer()->frame() ? renderer()->frame()->eventHandler()->lastKnownMousePosition() : IntPoint();
}

IntRect RenderLayer::rectForHorizontalScrollbar(const IntRect& borderBoxRect) const
{
    if (!m_hBar)
        return IntRect();

    const RenderBox* box = renderBox();
    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(horizontalScrollbarStart(borderBoxRect.x()),
        borderBoxRect.maxY() - box->borderBottom() - m_hBar->height(),
        borderBoxRect.width() - (box->borderLeft() + box->borderRight()) - scrollCorner.width(),
        m_hBar->height());
}

IntRect RenderLayer::rectForVerticalScrollbar(const IntRect& borderBoxRect) const
{
    if (!m_vBar)
        return IntRect();

    const RenderBox* box = renderBox();
    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(verticalScrollbarStart(borderBoxRect.x(), borderBoxRect.maxX()),
        borderBoxRect.y() + box->borderTop(),
        m_vBar->width(),
        borderBoxRect.height() - (box->borderTop() + box->borderBottom()) - scrollCorner.height());
}

LayoutUnit RenderLayer::verticalScrollbarStart(int minX, int maxX) const
{
    const RenderBox* box = renderBox();
    if (renderer()->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + box->borderLeft();
    return maxX - box->borderRight() - m_vBar->width();
}

LayoutUnit RenderLayer::horizontalScrollbarStart(int minX) const
{
    const RenderBox* box = renderBox();
    int x = minX + box->borderLeft();
    if (renderer()->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        x += m_vBar ? m_vBar->width() : resizerCornerRect(box->pixelSnappedBorderBoxRect(), ResizerForPointer).width();
    return x;
}

IntSize RenderLayer::scrollbarOffset(const Scrollbar* scrollbar) const
{
    RenderBox* box = renderBox();

    if (scrollbar == m_vBar.get())
        return IntSize(verticalScrollbarStart(0, box->width()), box->borderTop());

    if (scrollbar == m_hBar.get())
        return IntSize(horizontalScrollbarStart(0), box->height() - box->borderBottom() - scrollbar->height());

    ASSERT_NOT_REACHED();
    return IntSize();
}

void RenderLayer::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    if (scrollbar == m_vBar.get()) {
        if (GraphicsLayer* layer = layerForVerticalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
    } else {
        if (GraphicsLayer* layer = layerForHorizontalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
    }

    IntRect scrollRect = rect;
    RenderBox* box = renderBox();
    ASSERT(box);
    // If we are not yet inserted into the tree, there is no need to repaint.
    if (!box->parent())
        return;

    if (scrollbar == m_vBar.get())
        scrollRect.move(verticalScrollbarStart(0, box->width()), box->borderTop());
    else
        scrollRect.move(horizontalScrollbarStart(0), box->height() - box->borderBottom() - scrollbar->height());
    renderer()->repaintRectangle(scrollRect);
}

void RenderLayer::invalidateScrollCornerRect(const IntRect& rect)
{
    if (GraphicsLayer* layer = layerForScrollCorner()) {
        layer->setNeedsDisplayInRect(rect);
        return;
    }

    if (m_scrollCorner)
        m_scrollCorner->repaintRectangle(rect);
    if (m_resizer)
        m_resizer->repaintRectangle(rect);
}

static inline RenderObject* rendererForScrollbar(RenderObject* renderer)
{
    if (Node* node = renderer->node()) {
        if (ShadowRoot* shadowRoot = node->containingShadowRoot()) {
            if (shadowRoot->type() == ShadowRoot::UserAgentShadowRoot)
                return shadowRoot->host()->renderer();
        }
    }

    return renderer;
}

PassRefPtr<Scrollbar> RenderLayer::createScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar> widget;
    RenderObject* actualRenderer = rendererForScrollbar(renderer());
    bool hasCustomScrollbarStyle = actualRenderer->isBox() && actualRenderer->style()->hasPseudoStyle(SCROLLBAR);
    if (hasCustomScrollbarStyle)
        widget = RenderScrollbar::createCustomScrollbar(scrollableArea(), orientation, actualRenderer->node());
    else {
        widget = Scrollbar::create(scrollableArea(), orientation, RegularScrollbar);
        if (orientation == HorizontalScrollbar)
            scrollableArea()->didAddHorizontalScrollbar(widget.get());
        else
            scrollableArea()->didAddVerticalScrollbar(widget.get());
    }
    renderer()->document().view()->addChild(widget.get());
    return widget.release();
}

void RenderLayer::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar>& scrollbar = orientation == HorizontalScrollbar ? m_hBar : m_vBar;
    if (!scrollbar)
        return;

    if (!scrollbar->isCustomScrollbar()) {
        if (orientation == HorizontalScrollbar)
            scrollableArea()->willRemoveHorizontalScrollbar(scrollbar.get());
        else
            scrollableArea()->willRemoveVerticalScrollbar(scrollbar.get());
    }

    scrollbar->removeFromParent();
    scrollbar->disconnectFromScrollableArea();
    scrollbar = 0;
}

void RenderLayer::setHasHorizontalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasHorizontalScrollbar())
        return;

    if (hasScrollbar)
        m_hBar = createScrollbar(HorizontalScrollbar);
    else
        destroyScrollbar(HorizontalScrollbar);

    // Destroying or creating one bar can cause our scrollbar corner to come and go.  We need to update the opposite scrollbar's style.
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();

    // Force an update since we know the scrollbars have changed things.
    if (renderer()->document().hasAnnotatedRegions())
        renderer()->document().setAnnotatedRegionsDirty(true);
}

void RenderLayer::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasVerticalScrollbar())
        return;

    if (hasScrollbar)
        m_vBar = createScrollbar(VerticalScrollbar);
    else
        destroyScrollbar(VerticalScrollbar);

     // Destroying or creating one bar can cause our scrollbar corner to come and go.  We need to update the opposite scrollbar's style.
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();

    // Force an update since we know the scrollbars have changed things.
    if (renderer()->document().hasAnnotatedRegions())
        renderer()->document().setAnnotatedRegionsDirty(true);
}

ScrollableArea* RenderLayer::enclosingScrollableArea() const
{
    if (RenderLayer* scrollableLayer = enclosingScrollableLayer())
        return scrollableLayer->scrollableArea();

    // FIXME: We should return the frame view here (or possibly an ancestor frame view,
    // if the frame view isn't scrollable.
    return 0;
}

int RenderLayer::verticalScrollbarWidth(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!m_vBar || (m_vBar->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !m_vBar->shouldParticipateInHitTesting())))
        return 0;
    return m_vBar->width();
}

int RenderLayer::horizontalScrollbarHeight(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!m_hBar || (m_hBar->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !m_hBar->shouldParticipateInHitTesting())))
        return 0;
    return m_hBar->height();
}

IntSize RenderLayer::offsetFromResizeCorner(const IntPoint& absolutePoint) const
{
    // Currently the resize corner is either the bottom right corner or the bottom left corner.
    // FIXME: This assumes the location is 0, 0. Is this guaranteed to always be the case?
    IntSize elementSize = size();
    if (renderer()->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        elementSize.setWidth(0);
    IntPoint resizerPoint = IntPoint(elementSize);
    IntPoint localPoint = roundedIntPoint(absoluteToContents(absolutePoint));
    return localPoint - resizerPoint;
}

bool RenderLayer::hasOverflowControls() const
{
    return m_hBar || m_vBar || m_scrollCorner || renderer()->style()->resize() != RESIZE_NONE;
}

void RenderLayer::positionOverflowControls(const IntSize& offsetFromRoot)
{
    if (!m_hBar && !m_vBar && !canResize())
        return;

    RenderBox* box = renderBox();
    if (!box)
        return;

    const IntRect borderBox = box->pixelSnappedBorderBoxRect();
    const IntRect& scrollCorner = scrollCornerRect();
    IntRect absBounds(borderBox.location() + offsetFromRoot, borderBox.size());
    if (m_vBar) {
        IntRect vBarRect = rectForVerticalScrollbar(borderBox);
        vBarRect.move(offsetFromRoot);
        m_vBar->setFrameRect(vBarRect);
    }

    if (m_hBar) {
        IntRect hBarRect = rectForHorizontalScrollbar(borderBox);
        hBarRect.move(offsetFromRoot);
        m_hBar->setFrameRect(hBarRect);
    }

    if (m_scrollCorner)
        m_scrollCorner->setFrameRect(scrollCorner);
    if (m_resizer)
        m_resizer->setFrameRect(resizerCornerRect(borderBox, ResizerForPointer));

    if (isComposited())
        backing()->positionOverflowControlsLayers(offsetFromRoot);
}

int RenderLayer::scrollWidth() const
{
    return m_scrollableArea->scrollWidth();
}

int RenderLayer::scrollHeight() const
{
    return m_scrollableArea->scrollHeight();
}

void RenderLayer::updateScrollbarsAfterLayout()
{
    RenderBox* box = renderBox();
    ASSERT(box);

    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (box->style()->appearance() == ListboxPart)
        return;

    bool hasHorizontalOverflow = m_scrollableArea->hasHorizontalOverflow();
    bool hasVerticalOverflow = m_scrollableArea->hasVerticalOverflow();

    // overflow:scroll should just enable/disable.
    if (renderer()->style()->overflowX() == OSCROLL)
        m_hBar->setEnabled(hasHorizontalOverflow);
    if (renderer()->style()->overflowY() == OSCROLL)
        m_vBar->setEnabled(hasVerticalOverflow);

    // overflow:auto may need to lay out again if scrollbars got added/removed.
    bool autoHorizontalScrollBarChanged = box->hasAutoHorizontalScrollbar() && (hasHorizontalScrollbar() != hasHorizontalOverflow);
    bool autoVerticalScrollBarChanged = box->hasAutoVerticalScrollbar() && (hasVerticalScrollbar() != hasVerticalOverflow);

    if (autoHorizontalScrollBarChanged || autoVerticalScrollBarChanged) {
        if (box->hasAutoHorizontalScrollbar())
            setHasHorizontalScrollbar(hasHorizontalOverflow);
        if (box->hasAutoVerticalScrollbar())
            setHasVerticalScrollbar(hasVerticalOverflow);

        updateSelfPaintingLayer();

        // Force an update since we know the scrollbars have changed things.
        if (renderer()->document().hasAnnotatedRegions())
            renderer()->document().setAnnotatedRegionsDirty(true);

        renderer()->repaint();

        if (renderer()->style()->overflowX() == OAUTO || renderer()->style()->overflowY() == OAUTO) {
            if (!m_inOverflowRelayout) {
                // Our proprietary overflow: overlay value doesn't trigger a layout.
                m_inOverflowRelayout = true;
                SubtreeLayoutScope layoutScope(renderer());
                layoutScope.setNeedsLayout(renderer());
                if (renderer()->isRenderBlock()) {
                    RenderBlock* block = toRenderBlock(renderer());
                    block->scrollbarsChanged(autoHorizontalScrollBarChanged, autoVerticalScrollBarChanged);
                    block->layoutBlock(true);
                } else
                    renderer()->layout();
                m_inOverflowRelayout = false;
            }
        }
    }

    // Set up the range (and page step/line step).
    if (m_hBar) {
        int clientWidth = box->pixelSnappedClientWidth();
        m_hBar->setProportion(clientWidth, m_scrollableArea->overflowRect().width());
    }
    if (m_vBar) {
        int clientHeight = box->pixelSnappedClientHeight();
        m_vBar->setProportion(clientHeight, m_scrollableArea->overflowRect().height());
    }

    updateScrollableAreaSet(m_scrollableArea->hasScrollableHorizontalOverflow() || m_scrollableArea->hasScrollableVerticalOverflow());
}

void RenderLayer::updateScrollInfoAfterLayout()
{
    RenderBox* box = renderBox();
    if (!box)
        return;

    m_scrollableArea->updateAfterLayout();
    updateScrollbarsAfterLayout();

    // Composited scrolling may need to be enabled or disabled if the amount of overflow changed.
    if (renderer()->view() && compositor()->updateLayerCompositingState(this))
        compositor()->setCompositingLayersNeedRebuild();
}

bool RenderLayer::overflowControlsIntersectRect(const IntRect& localRect) const
{
    const IntRect borderBox = renderBox()->pixelSnappedBorderBoxRect();

    if (rectForHorizontalScrollbar(borderBox).intersects(localRect))
        return true;

    if (rectForVerticalScrollbar(borderBox).intersects(localRect))
        return true;

    if (scrollCornerRect().intersects(localRect))
        return true;

    if (resizerCornerRect(borderBox, ResizerForPointer).intersects(localRect))
        return true;

    return false;
}

void RenderLayer::paintOverflowControls(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect, bool paintingOverlayControls)
{
    // Don't do anything if we have no overflow.
    if (!renderer()->hasOverflowClip())
        return;

    // Overlay scrollbars paint in a second pass through the layer tree so that they will paint
    // on top of everything else. If this is the normal painting pass, paintingOverlayControls
    // will be false, and we should just tell the root layer that there are overlay scrollbars
    // that need to be painted. That will cause the second pass through the layer tree to run,
    // and we'll paint the scrollbars then. In the meantime, cache tx and ty so that the
    // second pass doesn't need to re-enter the RenderTree to get it right.
    if (hasOverlayScrollbars() && !paintingOverlayControls) {
        m_cachedOverlayScrollbarOffset = paintOffset;
        // It's not necessary to do the second pass if the scrollbars paint into layers.
        if ((m_hBar && layerForHorizontalScrollbar()) || (m_vBar && layerForVerticalScrollbar()))
            return;
        IntRect localDamgeRect = damageRect;
        localDamgeRect.moveBy(-paintOffset);
        if (!overflowControlsIntersectRect(localDamgeRect))
            return;

        RenderView* renderView = renderer()->view();

        RenderLayer* paintingRoot = 0;
        paintingRoot = enclosingCompositingLayer();
        if (!paintingRoot)
            paintingRoot = renderView->layer();

        paintingRoot->setContainsDirtyOverlayScrollbars(true);
        return;
    }

    // This check is required to avoid painting custom CSS scrollbars twice.
    if (paintingOverlayControls && !hasOverlayScrollbars())
        return;

    IntPoint adjustedPaintOffset = paintOffset;
    if (paintingOverlayControls)
        adjustedPaintOffset = m_cachedOverlayScrollbarOffset;

    // Move the scrollbar widgets if necessary.  We normally move and resize widgets during layout, but sometimes
    // widgets can move without layout occurring (most notably when you scroll a document that
    // contains fixed positioned elements).
    positionOverflowControls(toIntSize(adjustedPaintOffset));

    // Now that we're sure the scrollbars are in the right place, paint them.
    if (m_hBar && !layerForHorizontalScrollbar())
        m_hBar->paint(context, damageRect);
    if (m_vBar && !layerForVerticalScrollbar())
        m_vBar->paint(context, damageRect);

    if (layerForScrollCorner())
        return;

    // We fill our scroll corner with white if we have a scrollbar that doesn't run all the way up to the
    // edge of the box.
    paintScrollCorner(context, adjustedPaintOffset, damageRect);

    // Paint our resizer last, since it sits on top of the scroll corner.
    paintResizer(context, adjustedPaintOffset, damageRect);
}

void RenderLayer::paintScrollCorner(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect)
{
    RenderBox* box = renderBox();
    ASSERT(box);

    IntRect absRect = scrollCornerRect();
    absRect.moveBy(paintOffset);
    if (!absRect.intersects(damageRect))
        return;

    if (context->updatingControlTints()) {
        updateScrollCornerStyle();
        return;
    }

    if (m_scrollCorner) {
        m_scrollCorner->paintIntoRect(context, paintOffset, absRect);
        return;
    }

    // We don't want to paint white if we have overlay scrollbars, since we need
    // to see what is behind it.
    if (!hasOverlayScrollbars())
        context->fillRect(absRect, Color::white);
}

void RenderLayer::drawPlatformResizerImage(GraphicsContext* context, IntRect resizerCornerRect)
{
    float deviceScaleFactor = WebCore::deviceScaleFactor(renderer()->frame());

    RefPtr<Image> resizeCornerImage;
    IntSize cornerResizerSize;
    if (deviceScaleFactor >= 2) {
        DEFINE_STATIC_LOCAL(Image*, resizeCornerImageHiRes, (Image::loadPlatformResource("textAreaResizeCorner@2x").leakRef()));
        resizeCornerImage = resizeCornerImageHiRes;
        cornerResizerSize = resizeCornerImage->size();
        cornerResizerSize.scale(0.5f);
    } else {
        DEFINE_STATIC_LOCAL(Image*, resizeCornerImageLoRes, (Image::loadPlatformResource("textAreaResizeCorner").leakRef()));
        resizeCornerImage = resizeCornerImageLoRes;
        cornerResizerSize = resizeCornerImage->size();
    }

    if (renderer()->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        context->save();
        context->translate(resizerCornerRect.x() + cornerResizerSize.width(), resizerCornerRect.y() + resizerCornerRect.height() - cornerResizerSize.height());
        context->scale(FloatSize(-1.0, 1.0));
        context->drawImage(resizeCornerImage.get(), IntRect(IntPoint(), cornerResizerSize));
        context->restore();
        return;
    }
    IntRect imageRect(resizerCornerRect.maxXMaxYCorner() - cornerResizerSize, cornerResizerSize);
    context->drawImage(resizeCornerImage.get(), imageRect);
}

void RenderLayer::paintResizer(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect)
{
    if (renderer()->style()->resize() == RESIZE_NONE)
        return;

    RenderBox* box = renderBox();
    ASSERT(box);

    IntRect absRect = resizerCornerRect(box->pixelSnappedBorderBoxRect(), ResizerForPointer);
    absRect.moveBy(paintOffset);
    if (!absRect.intersects(damageRect))
        return;

    if (context->updatingControlTints()) {
        updateResizerStyle();
        return;
    }

    if (m_resizer) {
        m_resizer->paintIntoRect(context, paintOffset, absRect);
        return;
    }

    drawPlatformResizerImage(context, absRect);

    // Draw a frame around the resizer (1px grey line) if there are any scrollbars present.
    // Clipping will exclude the right and bottom edges of this frame.
    if (!hasOverlayScrollbars() && (m_vBar || m_hBar)) {
        GraphicsContextStateSaver stateSaver(*context);
        context->clip(absRect);
        IntRect largerCorner = absRect;
        largerCorner.setSize(IntSize(largerCorner.width() + 1, largerCorner.height() + 1));
        context->setStrokeColor(Color(217, 217, 217));
        context->setStrokeThickness(1.0f);
        context->setFillColor(Color::transparent);
        context->drawRect(largerCorner);
    }
}

bool RenderLayer::isPointInResizeControl(const IntPoint& absolutePoint,
                                         ResizerHitTestType resizerHitTestType) const
{
    if (!canResize())
        return false;

    RenderBox* box = renderBox();
    ASSERT(box);

    IntPoint localPoint = roundedIntPoint(absoluteToContents(absolutePoint));
    IntRect localBounds (0, 0, box->pixelSnappedWidth(), box->pixelSnappedHeight());
    return resizerCornerRect(localBounds, resizerHitTestType).contains(localPoint);
}

bool RenderLayer::hitTestOverflowControls(HitTestResult& result, const IntPoint& localPoint)
{
    if (!m_hBar && !m_vBar && !canResize())
        return false;

    RenderBox* box = renderBox();
    ASSERT(box);

    IntRect resizeControlRect;
    if (renderer()->style()->resize() != RESIZE_NONE) {
        resizeControlRect = resizerCornerRect(box->pixelSnappedBorderBoxRect(), ResizerForPointer);
        if (resizeControlRect.contains(localPoint))
            return true;
    }

    int resizeControlSize = max(resizeControlRect.height(), 0);

    // FIXME: We should hit test the m_scrollCorner and pass it back through the result.

    if (m_vBar && m_vBar->shouldParticipateInHitTesting()) {
        LayoutRect vBarRect(verticalScrollbarStart(0, box->width()),
                            box->borderTop(),
                            m_vBar->width(),
                            box->height() - (box->borderTop() + box->borderBottom()) - (m_hBar ? m_hBar->height() : resizeControlSize));
        if (vBarRect.contains(localPoint)) {
            result.setScrollbar(m_vBar.get());
            return true;
        }
    }

    resizeControlSize = max(resizeControlRect.width(), 0);
    if (m_hBar && m_hBar->shouldParticipateInHitTesting()) {
        LayoutRect hBarRect(horizontalScrollbarStart(0),
                            box->height() - box->borderBottom() - m_hBar->height(),
                            box->width() - (box->borderLeft() + box->borderRight()) - (m_vBar ? m_vBar->width() : resizeControlSize),
                            m_hBar->height());
        if (hBarRect.contains(localPoint)) {
            result.setScrollbar(m_hBar.get());
            return true;
        }
    }

    return false;
}

bool RenderLayer::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    if (ScrollableArea* scrollableArea = this->scrollableArea())
        return scrollableArea->scroll(direction, granularity, multiplier);

    return false;
}

void RenderLayer::paint(GraphicsContext* context, const LayoutRect& damageRect, PaintBehavior paintBehavior, RenderObject* paintingRoot, RenderRegion* region, PaintLayerFlags paintFlags)
{
    OverlapTestRequestMap overlapTestRequests;

    LayerPaintingInfo paintingInfo(this, enclosingIntRect(damageRect), paintBehavior, LayoutSize(), paintingRoot, region, &overlapTestRequests);
    paintLayer(context, paintingInfo, paintFlags);

    OverlapTestRequestMap::iterator end = overlapTestRequests.end();
    for (OverlapTestRequestMap::iterator it = overlapTestRequests.begin(); it != end; ++it)
        it->key->setIsOverlapped(false);
}

void RenderLayer::paintOverlayScrollbars(GraphicsContext* context, const LayoutRect& damageRect, PaintBehavior paintBehavior, RenderObject* paintingRoot)
{
    if (!m_containsDirtyOverlayScrollbars)
        return;

    LayerPaintingInfo paintingInfo(this, enclosingIntRect(damageRect), paintBehavior, LayoutSize(), paintingRoot);
    paintLayer(context, paintingInfo, PaintLayerPaintingOverlayScrollbars);

    m_containsDirtyOverlayScrollbars = false;
}

static bool inContainingBlockChain(RenderLayer* startLayer, RenderLayer* endLayer)
{
    if (startLayer == endLayer)
        return true;

    RenderView* view = startLayer->renderer()->view();
    for (RenderBlock* currentBlock = startLayer->renderer()->containingBlock(); currentBlock && currentBlock != view; currentBlock = currentBlock->containingBlock()) {
        if (currentBlock->layer() == endLayer)
            return true;
    }

    return false;
}

void RenderLayer::clipToRect(RenderLayer* rootLayer, GraphicsContext* context, const LayoutRect& paintDirtyRect, const ClipRect& clipRect,
                             BorderRadiusClippingRule rule)
{
    if (clipRect.rect() == paintDirtyRect && !clipRect.hasRadius())
        return;
    context->save();
    context->clip(pixelSnappedIntRect(clipRect.rect()));

    if (!clipRect.hasRadius())
        return;

    // If the clip rect has been tainted by a border radius, then we have to walk up our layer chain applying the clips from
    // any layers with overflow. The condition for being able to apply these clips is that the overflow object be in our
    // containing block chain so we check that also.
    for (RenderLayer* layer = rule == IncludeSelfForBorderRadius ? this : parent(); layer; layer = layer->parent()) {
        if (layer->renderer()->hasOverflowClip() && layer->renderer()->style()->hasBorderRadius() && inContainingBlockChain(this, layer)) {
                LayoutPoint delta;
                layer->convertToLayerCoords(rootLayer, delta);
                context->clipRoundedRect(layer->renderer()->style()->getRoundedInnerBorderFor(LayoutRect(delta, layer->size())));
        }

        if (layer == rootLayer)
            break;
    }
}

void RenderLayer::restoreClip(GraphicsContext* context, const LayoutRect& paintDirtyRect, const ClipRect& clipRect)
{
    if (clipRect.rect() == paintDirtyRect && !clipRect.hasRadius())
        return;
    context->restore();
}

static void performOverlapTests(OverlapTestRequestMap& overlapTestRequests, const RenderLayer* rootLayer, const RenderLayer* layer)
{
    Vector<RenderWidget*> overlappedRequestClients;
    OverlapTestRequestMap::iterator end = overlapTestRequests.end();
    LayoutRect boundingBox = layer->boundingBox(rootLayer);
    for (OverlapTestRequestMap::iterator it = overlapTestRequests.begin(); it != end; ++it) {
        if (!boundingBox.intersects(it->value))
            continue;

        it->key->setIsOverlapped(true);
        overlappedRequestClients.append(it->key);
    }
    for (size_t i = 0; i < overlappedRequestClients.size(); ++i)
        overlapTestRequests.remove(overlappedRequestClients[i]);
}

static bool shouldDoSoftwarePaint(const RenderLayer* layer, bool paintingReflection)
{
    return paintingReflection && !layer->has3DTransform();
}

static inline bool shouldSuppressPaintingLayer(RenderLayer* layer)
{
    // Avoid painting descendants of the root layer when stylesheets haven't loaded. This eliminates FOUC.
    // It's ok not to draw, because later on, when all the stylesheets do load, updateStyleSelector on the Document
    // will do a full repaint().
    if (layer->renderer()->document().didLayoutWithPendingStylesheets() && !layer->isRootLayer() && !layer->renderer()->isRoot())
        return true;

    return false;
}

static bool paintForFixedRootBackground(const RenderLayer* layer, RenderLayer::PaintLayerFlags paintFlags)
{
    return layer->renderer()->isRoot() && (paintFlags & RenderLayer::PaintLayerPaintingRootBackgroundOnly);
}

void RenderLayer::paintLayer(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    if (isComposited()) {
        // The updatingControlTints() painting pass goes through compositing layers,
        // but we need to ensure that we don't cache clip rects computed with the wrong root in this case.
        if (context->updatingControlTints() || (paintingInfo.paintBehavior & PaintBehaviorFlattenCompositingLayers)) {
            paintFlags |= PaintLayerTemporaryClipRects;
        } else if (!backing()->paintsIntoCompositedAncestor()
            && !shouldDoSoftwarePaint(this, paintFlags & PaintLayerPaintingReflection)
            && !paintForFixedRootBackground(this, paintFlags)) {
            // If this RenderLayer should paint into its backing, that will be done via RenderLayerBacking::paintIntoLayer().
            return;
        }
    } else if (viewportConstrainedNotCompositedReason() == NotCompositedForBoundsOutOfView) {
        // Don't paint out-of-view viewport constrained layers (when doing prepainting) because they will never be visible
        // unless their position or viewport size is changed.
        return;
    }

    // Non self-painting leaf layers don't need to be painted as their renderer() should properly paint itself.
    if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
        return;

    if (shouldSuppressPaintingLayer(this))
        return;

    // If this layer is totally invisible then there is nothing to paint.
    if (!renderer()->opacity())
        return;

    if (paintsWithTransparency(paintingInfo.paintBehavior))
        paintFlags |= PaintLayerHaveTransparency;

    // PaintLayerAppliedTransform is used in RenderReplica, to avoid applying the transform twice.
    if (paintsWithTransform(paintingInfo.paintBehavior) && !(paintFlags & PaintLayerAppliedTransform)) {
        TransformationMatrix layerTransform = renderableTransform(paintingInfo.paintBehavior);
        // If the transform can't be inverted, then don't paint anything.
        if (!layerTransform.isInvertible())
            return;

        // If we have a transparency layer enclosing us and we are the root of a transform, then we need to establish the transparency
        // layer from the parent now, assuming there is a parent
        if (paintFlags & PaintLayerHaveTransparency) {
            if (parent())
                parent()->beginTransparencyLayers(context, paintingInfo.rootLayer, paintingInfo.paintDirtyRect, paintingInfo.paintBehavior);
            else
                beginTransparencyLayers(context, paintingInfo.rootLayer, paintingInfo.paintDirtyRect, paintingInfo.paintBehavior);
        }

        if (enclosingPaginationLayer()) {
            paintTransformedLayerIntoFragments(context, paintingInfo, paintFlags);
            return;
        }

        // Make sure the parent's clip rects have been calculated.
        ClipRect clipRect = paintingInfo.paintDirtyRect;
        if (parent()) {
            ClipRectsContext clipRectsContext(paintingInfo.rootLayer, paintingInfo.region, (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects,
                IgnoreOverlayScrollbarSize, (paintFlags & PaintLayerPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip);
            clipRect = backgroundClipRect(clipRectsContext);
            clipRect.intersect(paintingInfo.paintDirtyRect);

            // Push the parent coordinate space's clip.
            parent()->clipToRect(paintingInfo.rootLayer, context, paintingInfo.paintDirtyRect, clipRect);
        }

        paintLayerByApplyingTransform(context, paintingInfo, paintFlags);

        // Restore the clip.
        if (parent())
            parent()->restoreClip(context, paintingInfo.paintDirtyRect, clipRect);

        return;
    }

    paintLayerContentsAndReflection(context, paintingInfo, paintFlags);
}

void RenderLayer::paintLayerContentsAndReflection(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    PaintLayerFlags localPaintFlags = paintFlags & ~(PaintLayerAppliedTransform);

    // Paint the reflection first if we have one.
    if (m_reflection && !m_paintingInsideReflection) {
        // Mark that we are now inside replica painting.
        m_paintingInsideReflection = true;
        reflectionLayer()->paintLayer(context, paintingInfo, localPaintFlags | PaintLayerPaintingReflection);
        m_paintingInsideReflection = false;
    }

    localPaintFlags |= PaintLayerPaintingCompositingAllPhases;
    paintLayerContents(context, paintingInfo, localPaintFlags);
}

void RenderLayer::paintLayerContents(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());
    ASSERT(!(paintFlags & PaintLayerAppliedTransform));

    bool haveTransparency = paintFlags & PaintLayerHaveTransparency;
    bool isSelfPaintingLayer = this->isSelfPaintingLayer();
    bool isPaintingOverlayScrollbars = paintFlags & PaintLayerPaintingOverlayScrollbars;
    bool isPaintingScrollingContent = paintFlags & PaintLayerPaintingCompositingScrollingPhase;
    bool isPaintingCompositedForeground = paintFlags & PaintLayerPaintingCompositingForegroundPhase;
    bool isPaintingCompositedBackground = paintFlags & PaintLayerPaintingCompositingBackgroundPhase;
    bool isPaintingOverflowContents = paintFlags & PaintLayerPaintingOverflowContents;
    // Outline always needs to be painted even if we have no visible content. Also,
    // the outline is painted in the background phase during composited scrolling.
    // If it were painted in the foreground phase, it would move with the scrolled
    // content. When not composited scrolling, the outline is painted in the
    // foreground phase. Since scrolled contents are moved by repainting in this
    // case, the outline won't get 'dragged along'.
    bool shouldPaintOutline = isSelfPaintingLayer && !isPaintingOverlayScrollbars
        && ((isPaintingScrollingContent && isPaintingCompositedBackground)
        || (!isPaintingScrollingContent && isPaintingCompositedForeground));
    bool shouldPaintContent = m_hasVisibleContent && isSelfPaintingLayer && !isPaintingOverlayScrollbars;

    float deviceScaleFactor = WebCore::deviceScaleFactor(renderer()->frame());
    context->setUseHighResMarkers(deviceScaleFactor > 1.5f);

    GraphicsContext* transparencyLayerContext = context;

    if (paintFlags & PaintLayerPaintingRootBackgroundOnly && !renderer()->isRenderView() && !renderer()->isRoot())
        return;

    // Ensure our lists are up-to-date.
    updateLayerListsIfNeeded();

    LayoutPoint offsetFromRoot;
    convertToLayerCoords(paintingInfo.rootLayer, offsetFromRoot);

    IntRect rootRelativeBounds;
    bool rootRelativeBoundsComputed = false;

    // Apply clip-path to context.
    bool hasClipPath = false;
    RenderStyle* style = renderer()->style();
    RenderSVGResourceClipper* resourceClipper = 0;
    if (renderer()->hasClipPath() && !context->paintingDisabled() && style) {
        ASSERT(style->clipPath());
        if (style->clipPath()->getOperationType() == ClipPathOperation::SHAPE) {
            hasClipPath = true;
            context->save();
            ShapeClipPathOperation* clipPath = static_cast<ShapeClipPathOperation*>(style->clipPath());

            if (!rootRelativeBoundsComputed) {
                rootRelativeBounds = calculateLayerBounds(paintingInfo.rootLayer, &offsetFromRoot, 0);
                rootRelativeBoundsComputed = true;
            }

            context->clipPath(clipPath->path(rootRelativeBounds), clipPath->windRule());
        } else if (style->clipPath()->getOperationType() == ClipPathOperation::REFERENCE) {
            ReferenceClipPathOperation* referenceClipPathOperation = static_cast<ReferenceClipPathOperation*>(style->clipPath());
            Document& document = renderer()->document();
            // FIXME: It doesn't work with forward or external SVG references (https://bugs.webkit.org/show_bug.cgi?id=90405)
            Element* element = document.getElementById(referenceClipPathOperation->fragment());
            if (element && element->hasTagName(SVGNames::clipPathTag) && element->renderer()) {
                if (!rootRelativeBoundsComputed) {
                    rootRelativeBounds = calculateLayerBounds(paintingInfo.rootLayer, &offsetFromRoot, 0);
                    rootRelativeBoundsComputed = true;
                }

                // FIXME: This should use a safer cast such as toRenderSVGResourceContainer().
                resourceClipper = static_cast<RenderSVGResourceClipper*>(element->renderer());
                if (!resourceClipper->applyClippingToContext(renderer(), rootRelativeBounds, paintingInfo.paintDirtyRect, context)) {
                    // No need to post-apply the clipper if this failed.
                    resourceClipper = 0;
                }
            }
        }
    }

    LayerPaintingInfo localPaintingInfo(paintingInfo);
    FilterEffectRendererHelper filterPainter(filterRenderer() && paintsWithFilters());
    if (filterPainter.haveFilterEffect() && !context->paintingDisabled()) {
        RenderLayerFilterInfo* filterInfo = this->filterInfo();
        ASSERT(filterInfo);
        LayoutRect filterRepaintRect = filterInfo->dirtySourceRect();
        filterRepaintRect.move(offsetFromRoot.x(), offsetFromRoot.y());

        if (!rootRelativeBoundsComputed) {
            rootRelativeBounds = calculateLayerBounds(paintingInfo.rootLayer, &offsetFromRoot, 0);
            rootRelativeBoundsComputed = true;
        }

        if (filterPainter.prepareFilterEffect(this, rootRelativeBounds, paintingInfo.paintDirtyRect, filterRepaintRect)) {
            // Now we know for sure, that the source image will be updated, so we can revert our tracking repaint rect back to zero.
            filterInfo->resetDirtySourceRect();

            // Rewire the old context to a memory buffer, so that we can capture the contents of the layer.
            // NOTE: We saved the old context in the "transparencyLayerContext" local variable, to be able to start a transparency layer
            // on the original context and avoid duplicating "beginFilterEffect" after each transparency layer call. Also, note that
            // beginTransparencyLayers will only create a single lazy transparency layer, even though it is called twice in this method.
            context = filterPainter.beginFilterEffect(context);

            // Check that we didn't fail to allocate the graphics context for the offscreen buffer.
            if (filterPainter.hasStartedFilterEffect()) {
                localPaintingInfo.paintDirtyRect = filterPainter.repaintRect();
                // If the filter needs the full source image, we need to avoid using the clip rectangles.
                // Otherwise, if for example this layer has overflow:hidden, a drop shadow will not compute correctly.
                // Note that we will still apply the clipping on the final rendering of the filter.
                localPaintingInfo.clipToDirtyRect = !filterRenderer()->hasFilterThatMovesPixels();
            }
        }
    }

    if (filterPainter.hasStartedFilterEffect() && haveTransparency) {
        // If we have a filter and transparency, we have to eagerly start a transparency layer here, rather than risk a child layer lazily starts one with the wrong context.
        beginTransparencyLayers(transparencyLayerContext, localPaintingInfo.rootLayer, paintingInfo.paintDirtyRect, localPaintingInfo.paintBehavior);
    }

    // If this layer's renderer is a child of the paintingRoot, we render unconditionally, which
    // is done by passing a nil paintingRoot down to our renderer (as if no paintingRoot was ever set).
    // Else, our renderer tree may or may not contain the painting root, so we pass that root along
    // so it will be tested against as we descend through the renderers.
    RenderObject* paintingRootForRenderer = 0;
    if (localPaintingInfo.paintingRoot && !renderer()->isDescendantOf(localPaintingInfo.paintingRoot))
        paintingRootForRenderer = localPaintingInfo.paintingRoot;

    if (localPaintingInfo.overlapTestRequests && isSelfPaintingLayer)
        performOverlapTests(*localPaintingInfo.overlapTestRequests, localPaintingInfo.rootLayer, this);

    bool forceBlackText = localPaintingInfo.paintBehavior & PaintBehaviorForceBlackText;
    bool selectionOnly  = localPaintingInfo.paintBehavior & PaintBehaviorSelectionOnly;

    bool shouldPaintBackground = isPaintingCompositedBackground && shouldPaintContent && !selectionOnly;
    bool shouldPaintNegZOrderList = (isPaintingScrollingContent && isPaintingOverflowContents) || (!isPaintingScrollingContent && isPaintingCompositedBackground);
    bool shouldPaintOwnContents = isPaintingCompositedForeground && shouldPaintContent;
    bool shouldPaintNormalFlowAndPosZOrderLists = isPaintingCompositedForeground;
    bool shouldPaintOverlayScrollbars = isPaintingOverlayScrollbars;
    bool shouldPaintMask = (paintFlags & PaintLayerPaintingCompositingMaskPhase) && shouldPaintContent && renderer()->hasMask() && !selectionOnly;

    PaintBehavior paintBehavior = PaintBehaviorNormal;
    if (paintFlags & PaintLayerPaintingSkipRootBackground)
        paintBehavior |= PaintBehaviorSkipRootBackground;
    else if (paintFlags & PaintLayerPaintingRootBackgroundOnly)
        paintBehavior |= PaintBehaviorRootBackgroundOnly;

    LayerFragments layerFragments;
    if (shouldPaintContent || shouldPaintOutline || isPaintingOverlayScrollbars) {
        // Collect the fragments. This will compute the clip rectangles and paint offsets for each layer fragment, as well as whether or not the content of each
        // fragment should paint.
        collectFragments(layerFragments, localPaintingInfo.rootLayer, localPaintingInfo.region, localPaintingInfo.paintDirtyRect,
            (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, IgnoreOverlayScrollbarSize,
            (isPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip, &offsetFromRoot);
        updatePaintingInfoForFragments(layerFragments, localPaintingInfo, paintFlags, shouldPaintContent, &offsetFromRoot);
    }

    if (shouldPaintBackground)
        paintBackgroundForFragments(layerFragments, context, transparencyLayerContext, paintingInfo.paintDirtyRect, haveTransparency,
                localPaintingInfo, paintBehavior, paintingRootForRenderer);

    if (shouldPaintNegZOrderList)
        paintList(negZOrderList(), context, localPaintingInfo, paintFlags);

    if (shouldPaintOwnContents)
        paintForegroundForFragments(layerFragments, context, transparencyLayerContext, paintingInfo.paintDirtyRect, haveTransparency,
                localPaintingInfo, paintBehavior, paintingRootForRenderer, selectionOnly, forceBlackText);

    if (shouldPaintOutline)
        paintOutlineForFragments(layerFragments, context, localPaintingInfo, paintBehavior, paintingRootForRenderer);

    if (shouldPaintNormalFlowAndPosZOrderLists)
        paintList(m_normalFlowList.get(), context, localPaintingInfo, paintFlags);

    if (shouldPaintNormalFlowAndPosZOrderLists)
        paintList(posZOrderList(), context, localPaintingInfo, paintFlags);

    if (shouldPaintOverlayScrollbars)
        paintOverflowControlsForFragments(layerFragments, context, localPaintingInfo);

    if (filterPainter.hasStartedFilterEffect()) {
        // Apply the correct clipping (ie. overflow: hidden).
        // FIXME: It is incorrect to just clip to the damageRect here once multiple fragments are involved.
        ClipRect backgroundRect = layerFragments.isEmpty() ? ClipRect() : layerFragments[0].backgroundRect;
        clipToRect(localPaintingInfo.rootLayer, transparencyLayerContext, localPaintingInfo.paintDirtyRect, backgroundRect);
        context = filterPainter.applyFilterEffect();
        restoreClip(transparencyLayerContext, localPaintingInfo.paintDirtyRect, backgroundRect);
    }

    // Make sure that we now use the original transparency context.
    ASSERT(transparencyLayerContext == context);

    if (shouldPaintMask)
        paintMaskForFragments(layerFragments, context, localPaintingInfo, paintingRootForRenderer);

    // End our transparency layer
    if (haveTransparency && m_usedTransparency && !m_paintingInsideReflection) {
        context->endLayer();
        context->restore();
        m_usedTransparency = false;
    }

    if (resourceClipper)
        resourceClipper->postApplyResource(renderer(), context, ApplyToDefaultMode, 0, 0);

    if (hasClipPath)
        context->restore();
}

void RenderLayer::paintLayerByApplyingTransform(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags, const LayoutPoint& translationOffset)
{
    // This involves subtracting out the position of the layer in our current coordinate space, but preserving
    // the accumulated error for sub-pixel layout.
    LayoutPoint delta;
    convertToLayerCoords(paintingInfo.rootLayer, delta);
    delta.moveBy(translationOffset);
    TransformationMatrix transform(renderableTransform(paintingInfo.paintBehavior));
    IntPoint roundedDelta = roundedIntPoint(delta);
    transform.translateRight(roundedDelta.x(), roundedDelta.y());
    LayoutSize adjustedSubPixelAccumulation = paintingInfo.subPixelAccumulation + (delta - roundedDelta);

    // Apply the transform.
    GraphicsContextStateSaver stateSaver(*context);
    context->concatCTM(transform.toAffineTransform());

    // Now do a paint with the root layer shifted to be us.
    LayerPaintingInfo transformedPaintingInfo(this, enclosingIntRect(transform.inverse().mapRect(paintingInfo.paintDirtyRect)), paintingInfo.paintBehavior,
        adjustedSubPixelAccumulation, paintingInfo.paintingRoot, paintingInfo.region, paintingInfo.overlapTestRequests);
    paintLayerContentsAndReflection(context, transformedPaintingInfo, paintFlags);
}

void RenderLayer::paintList(Vector<RenderLayer*>* list, GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    if (!list)
        return;

    if (!hasSelfPaintingLayerDescendant())
        return;

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(this);
#endif

    for (size_t i = 0; i < list->size(); ++i) {
        RenderLayer* childLayer = list->at(i);
        if (!childLayer->isPaginated())
            childLayer->paintLayer(context, paintingInfo, paintFlags);
        else
            paintPaginatedChildLayer(childLayer, context, paintingInfo, paintFlags);
    }
}

void RenderLayer::collectFragments(LayerFragments& fragments, const RenderLayer* rootLayer, RenderRegion* region, const LayoutRect& dirtyRect,
    ClipRectsType clipRectsType, OverlayScrollbarSizeRelevancy inOverlayScrollbarSizeRelevancy, ShouldRespectOverflowClip respectOverflowClip, const LayoutPoint* offsetFromRoot,
    const LayoutRect* layerBoundingBox)
{
    if (!enclosingPaginationLayer() || hasTransform()) {
        // For unpaginated layers, there is only one fragment.
        LayerFragment fragment;
        ClipRectsContext clipRectsContext(rootLayer, region, clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip);
        calculateRects(clipRectsContext, dirtyRect, fragment.layerBounds, fragment.backgroundRect, fragment.foregroundRect, fragment.outlineRect, offsetFromRoot);
        fragments.append(fragment);
        return;
    }

    // Compute our offset within the enclosing pagination layer.
    LayoutPoint offsetWithinPaginatedLayer;
    convertToLayerCoords(enclosingPaginationLayer(), offsetWithinPaginatedLayer);

    // Calculate clip rects relative to the enclosingPaginationLayer. The purpose of this call is to determine our bounds clipped to intermediate
    // layers between us and the pagination context. It's important to minimize the number of fragments we need to create and this helps with that.
    ClipRectsContext paginationClipRectsContext(enclosingPaginationLayer(), region, clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip);
    LayoutRect layerBoundsInFlowThread;
    ClipRect backgroundRectInFlowThread;
    ClipRect foregroundRectInFlowThread;
    ClipRect outlineRectInFlowThread;
    calculateRects(paginationClipRectsContext, PaintInfo::infiniteRect(), layerBoundsInFlowThread, backgroundRectInFlowThread, foregroundRectInFlowThread,
        outlineRectInFlowThread, &offsetWithinPaginatedLayer);

    // Take our bounding box within the flow thread and clip it.
    LayoutRect layerBoundingBoxInFlowThread = layerBoundingBox ? *layerBoundingBox : boundingBox(enclosingPaginationLayer(), 0, &offsetWithinPaginatedLayer);
    layerBoundingBoxInFlowThread.intersect(backgroundRectInFlowThread.rect());

    // Shift the dirty rect into flow thread coordinates.
    LayoutPoint offsetOfPaginationLayerFromRoot;
    enclosingPaginationLayer()->convertToLayerCoords(rootLayer, offsetOfPaginationLayerFromRoot);
    LayoutRect dirtyRectInFlowThread(dirtyRect);
    dirtyRectInFlowThread.moveBy(-offsetOfPaginationLayerFromRoot);

    // Tell the flow thread to collect the fragments. We pass enough information to create a minimal number of fragments based off the pages/columns
    // that intersect the actual dirtyRect as well as the pages/columns that intersect our layer's bounding box.
    RenderFlowThread* enclosingFlowThread = toRenderFlowThread(enclosingPaginationLayer()->renderer());
    enclosingFlowThread->collectLayerFragments(fragments, layerBoundingBoxInFlowThread, dirtyRectInFlowThread);

    if (fragments.isEmpty())
        return;

    // Get the parent clip rects of the pagination layer, since we need to intersect with that when painting column contents.
    ClipRect ancestorClipRect = dirtyRect;
    if (enclosingPaginationLayer()->parent()) {
        ClipRectsContext clipRectsContext(rootLayer, region, clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip);
        ancestorClipRect = enclosingPaginationLayer()->backgroundClipRect(clipRectsContext);
        ancestorClipRect.intersect(dirtyRect);
    }

    for (size_t i = 0; i < fragments.size(); ++i) {
        LayerFragment& fragment = fragments.at(i);

        // Set our four rects with all clipping applied that was internal to the flow thread.
        fragment.setRects(layerBoundsInFlowThread, backgroundRectInFlowThread, foregroundRectInFlowThread, outlineRectInFlowThread);

        // Shift to the root-relative physical position used when painting the flow thread in this fragment.
        fragment.moveBy(fragment.paginationOffset + offsetOfPaginationLayerFromRoot);

        // Intersect the fragment with our ancestor's background clip so that e.g., columns in an overflow:hidden block are
        // properly clipped by the overflow.
        fragment.intersect(ancestorClipRect.rect());

        // Now intersect with our pagination clip. This will typically mean we're just intersecting the dirty rect with the column
        // clip, so the column clip ends up being all we apply.
        fragment.intersect(fragment.paginationClip);
    }
}

void RenderLayer::updatePaintingInfoForFragments(LayerFragments& fragments, const LayerPaintingInfo& localPaintingInfo, PaintLayerFlags localPaintFlags,
    bool shouldPaintContent, const LayoutPoint* offsetFromRoot)
{
    ASSERT(offsetFromRoot);
    for (size_t i = 0; i < fragments.size(); ++i) {
        LayerFragment& fragment = fragments.at(i);
        fragment.shouldPaintContent = shouldPaintContent;
        if (this != localPaintingInfo.rootLayer || !(localPaintFlags & PaintLayerPaintingOverflowContents)) {
            LayoutPoint newOffsetFromRoot = *offsetFromRoot + fragment.paginationOffset;
            fragment.shouldPaintContent &= intersectsDamageRect(fragment.layerBounds, fragment.backgroundRect.rect(), localPaintingInfo.rootLayer, &newOffsetFromRoot);
        }
    }
}

void RenderLayer::paintTransformedLayerIntoFragments(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    LayerFragments enclosingPaginationFragments;
    LayoutPoint offsetOfPaginationLayerFromRoot;
    LayoutRect transformedExtent = transparencyClipBox(this, enclosingPaginationLayer(), PaintingTransparencyClipBox, RootOfTransparencyClipBox, paintingInfo.paintBehavior);
    enclosingPaginationLayer()->collectFragments(enclosingPaginationFragments, paintingInfo.rootLayer, paintingInfo.region, paintingInfo.paintDirtyRect,
        (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, IgnoreOverlayScrollbarSize,
        (paintFlags & PaintLayerPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip, &offsetOfPaginationLayerFromRoot, &transformedExtent);

    for (size_t i = 0; i < enclosingPaginationFragments.size(); ++i) {
        const LayerFragment& fragment = enclosingPaginationFragments.at(i);

        // Apply the page/column clip for this fragment, as well as any clips established by layers in between us and
        // the enclosing pagination layer.
        LayoutRect clipRect = fragment.backgroundRect.rect();

        // Now compute the clips within a given fragment
        if (parent() != enclosingPaginationLayer()) {
            enclosingPaginationLayer()->convertToLayerCoords(paintingInfo.rootLayer, offsetOfPaginationLayerFromRoot);

            ClipRectsContext clipRectsContext(enclosingPaginationLayer(), paintingInfo.region, (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects,
                IgnoreOverlayScrollbarSize, (paintFlags & PaintLayerPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip);
            LayoutRect parentClipRect = backgroundClipRect(clipRectsContext).rect();
            parentClipRect.moveBy(fragment.paginationOffset + offsetOfPaginationLayerFromRoot);
            clipRect.intersect(parentClipRect);
        }

        parent()->clipToRect(paintingInfo.rootLayer, context, paintingInfo.paintDirtyRect, clipRect);
        paintLayerByApplyingTransform(context, paintingInfo, paintFlags, fragment.paginationOffset);
        parent()->restoreClip(context, paintingInfo.paintDirtyRect, clipRect);
    }
}

void RenderLayer::paintBackgroundForFragments(const LayerFragments& layerFragments, GraphicsContext* context, GraphicsContext* transparencyLayerContext,
    const LayoutRect& transparencyPaintDirtyRect, bool haveTransparency, const LayerPaintingInfo& localPaintingInfo, PaintBehavior paintBehavior,
    RenderObject* paintingRootForRenderer)
{
    for (size_t i = 0; i < layerFragments.size(); ++i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if (!fragment.shouldPaintContent)
            continue;

        // Begin transparency layers lazily now that we know we have to paint something.
        if (haveTransparency)
            beginTransparencyLayers(transparencyLayerContext, localPaintingInfo.rootLayer, transparencyPaintDirtyRect, localPaintingInfo.paintBehavior);

        if (localPaintingInfo.clipToDirtyRect) {
            // Paint our background first, before painting any child layers.
            // Establish the clip used to paint our background.
            clipToRect(localPaintingInfo.rootLayer, context, localPaintingInfo.paintDirtyRect, fragment.backgroundRect, DoNotIncludeSelfForBorderRadius); // Background painting will handle clipping to self.
        }

        // Paint the background.
        // FIXME: Eventually we will collect the region from the fragment itself instead of just from the paint info.
        PaintInfo paintInfo(context, pixelSnappedIntRect(fragment.backgroundRect.rect()), PaintPhaseBlockBackground, paintBehavior, paintingRootForRenderer, localPaintingInfo.region, 0, 0, localPaintingInfo.rootLayer->renderer());
        renderer()->paint(paintInfo, toPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subPixelAccumulation));

        if (localPaintingInfo.clipToDirtyRect)
            restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.backgroundRect);
    }
}

void RenderLayer::paintForegroundForFragments(const LayerFragments& layerFragments, GraphicsContext* context, GraphicsContext* transparencyLayerContext,
    const LayoutRect& transparencyPaintDirtyRect, bool haveTransparency, const LayerPaintingInfo& localPaintingInfo, PaintBehavior paintBehavior,
    RenderObject* paintingRootForRenderer, bool selectionOnly, bool forceBlackText)
{
    // Begin transparency if we have something to paint.
    if (haveTransparency) {
        for (size_t i = 0; i < layerFragments.size(); ++i) {
            const LayerFragment& fragment = layerFragments.at(i);
            if (fragment.shouldPaintContent && !fragment.foregroundRect.isEmpty()) {
                beginTransparencyLayers(transparencyLayerContext, localPaintingInfo.rootLayer, transparencyPaintDirtyRect, localPaintingInfo.paintBehavior);
                break;
            }
        }
    }

    PaintBehavior localPaintBehavior = forceBlackText ? (PaintBehavior)PaintBehaviorForceBlackText : paintBehavior;

    // Optimize clipping for the single fragment case.
    bool shouldClip = localPaintingInfo.clipToDirtyRect && layerFragments.size() == 1 && layerFragments[0].shouldPaintContent && !layerFragments[0].foregroundRect.isEmpty();
    if (shouldClip)
        clipToRect(localPaintingInfo.rootLayer, context, localPaintingInfo.paintDirtyRect, layerFragments[0].foregroundRect);

    // We have to loop through every fragment multiple times, since we have to repaint in each specific phase in order for
    // interleaving of the fragments to work properly.
    paintForegroundForFragmentsWithPhase(selectionOnly ? PaintPhaseSelection : PaintPhaseChildBlockBackgrounds, layerFragments,
        context, localPaintingInfo, localPaintBehavior, paintingRootForRenderer);

    if (!selectionOnly) {
        paintForegroundForFragmentsWithPhase(PaintPhaseFloat, layerFragments, context, localPaintingInfo, localPaintBehavior, paintingRootForRenderer);
        paintForegroundForFragmentsWithPhase(PaintPhaseForeground, layerFragments, context, localPaintingInfo, localPaintBehavior, paintingRootForRenderer);
        paintForegroundForFragmentsWithPhase(PaintPhaseChildOutlines, layerFragments, context, localPaintingInfo, localPaintBehavior, paintingRootForRenderer);
    }

    if (shouldClip)
        restoreClip(context, localPaintingInfo.paintDirtyRect, layerFragments[0].foregroundRect);
}

void RenderLayer::paintForegroundForFragmentsWithPhase(PaintPhase phase, const LayerFragments& layerFragments, GraphicsContext* context,
    const LayerPaintingInfo& localPaintingInfo, PaintBehavior paintBehavior, RenderObject* paintingRootForRenderer)
{
    bool shouldClip = localPaintingInfo.clipToDirtyRect && layerFragments.size() > 1;

    for (size_t i = 0; i < layerFragments.size(); ++i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if (!fragment.shouldPaintContent || fragment.foregroundRect.isEmpty())
            continue;

        if (shouldClip)
            clipToRect(localPaintingInfo.rootLayer, context, localPaintingInfo.paintDirtyRect, fragment.foregroundRect);

        PaintInfo paintInfo(context, pixelSnappedIntRect(fragment.foregroundRect.rect()), phase, paintBehavior, paintingRootForRenderer, localPaintingInfo.region, 0, 0, localPaintingInfo.rootLayer->renderer());
        if (phase == PaintPhaseForeground)
            paintInfo.overlapTestRequests = localPaintingInfo.overlapTestRequests;
        renderer()->paint(paintInfo, toPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subPixelAccumulation));

        if (shouldClip)
            restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.foregroundRect);
    }
}

void RenderLayer::paintOutlineForFragments(const LayerFragments& layerFragments, GraphicsContext* context, const LayerPaintingInfo& localPaintingInfo,
    PaintBehavior paintBehavior, RenderObject* paintingRootForRenderer)
{
    for (size_t i = 0; i < layerFragments.size(); ++i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if (fragment.outlineRect.isEmpty())
            continue;

        // Paint our own outline
        PaintInfo paintInfo(context, pixelSnappedIntRect(fragment.outlineRect.rect()), PaintPhaseSelfOutline, paintBehavior, paintingRootForRenderer, localPaintingInfo.region, 0, 0, localPaintingInfo.rootLayer->renderer());
        clipToRect(localPaintingInfo.rootLayer, context, localPaintingInfo.paintDirtyRect, fragment.outlineRect, DoNotIncludeSelfForBorderRadius);
        renderer()->paint(paintInfo, toPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subPixelAccumulation));
        restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.outlineRect);
    }
}

void RenderLayer::paintMaskForFragments(const LayerFragments& layerFragments, GraphicsContext* context, const LayerPaintingInfo& localPaintingInfo,
    RenderObject* paintingRootForRenderer)
{
    for (size_t i = 0; i < layerFragments.size(); ++i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if (!fragment.shouldPaintContent)
            continue;

        if (localPaintingInfo.clipToDirtyRect)
            clipToRect(localPaintingInfo.rootLayer, context, localPaintingInfo.paintDirtyRect, fragment.backgroundRect, DoNotIncludeSelfForBorderRadius); // Mask painting will handle clipping to self.

        // Paint the mask.
        // FIXME: Eventually we will collect the region from the fragment itself instead of just from the paint info.
        PaintInfo paintInfo(context, pixelSnappedIntRect(fragment.backgroundRect.rect()), PaintPhaseMask, PaintBehaviorNormal, paintingRootForRenderer, localPaintingInfo.region, 0, 0, localPaintingInfo.rootLayer->renderer());
        renderer()->paint(paintInfo, toPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subPixelAccumulation));

        if (localPaintingInfo.clipToDirtyRect)
            restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.backgroundRect);
    }
}

void RenderLayer::paintOverflowControlsForFragments(const LayerFragments& layerFragments, GraphicsContext* context, const LayerPaintingInfo& localPaintingInfo)
{
    for (size_t i = 0; i < layerFragments.size(); ++i) {
        const LayerFragment& fragment = layerFragments.at(i);
        clipToRect(localPaintingInfo.rootLayer, context, localPaintingInfo.paintDirtyRect, fragment.backgroundRect);
        paintOverflowControls(context, roundedIntPoint(toPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subPixelAccumulation)),
            pixelSnappedIntRect(fragment.backgroundRect.rect()), true);
        restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.backgroundRect);
    }
}

void RenderLayer::paintPaginatedChildLayer(RenderLayer* childLayer, GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    // We need to do multiple passes, breaking up our child layer into strips.
    Vector<RenderLayer*> columnLayers;
    RenderLayer* ancestorLayer = isNormalFlowOnly() ? parent() : ancestorStackingContainer();
    for (RenderLayer* curr = childLayer->parent(); curr; curr = curr->parent()) {
        if (curr->renderer()->hasColumns() && checkContainingBlockChainForPagination(childLayer->renderer(), curr->renderBox()))
            columnLayers.append(curr);
        if (curr == ancestorLayer)
            break;
    }

    // It is possible for paintLayer() to be called after the child layer ceases to be paginated but before
    // updateLayerPositions() is called and resets the isPaginated() flag, see <rdar://problem/10098679>.
    // If this is the case, just bail out, since the upcoming call to updateLayerPositions() will repaint the layer.
    if (!columnLayers.size())
        return;

    paintChildLayerIntoColumns(childLayer, context, paintingInfo, paintFlags, columnLayers, columnLayers.size() - 1);
}

void RenderLayer::paintChildLayerIntoColumns(RenderLayer* childLayer, GraphicsContext* context, const LayerPaintingInfo& paintingInfo,
    PaintLayerFlags paintFlags, const Vector<RenderLayer*>& columnLayers, size_t colIndex)
{
    RenderBlock* columnBlock = toRenderBlock(columnLayers[colIndex]->renderer());

    ASSERT(columnBlock && columnBlock->hasColumns());
    if (!columnBlock || !columnBlock->hasColumns())
        return;

    LayoutPoint layerOffset;
    // FIXME: It looks suspicious to call convertToLayerCoords here
    // as canUseConvertToLayerCoords is true for this layer.
    columnBlock->layer()->convertToLayerCoords(paintingInfo.rootLayer, layerOffset);

    bool isHorizontal = columnBlock->style()->isHorizontalWritingMode();

    ColumnInfo* colInfo = columnBlock->columnInfo();
    unsigned colCount = columnBlock->columnCount(colInfo);
    LayoutUnit currLogicalTopOffset = 0;
    for (unsigned i = 0; i < colCount; i++) {
        // For each rect, we clip to the rect, and then we adjust our coords.
        LayoutRect colRect = columnBlock->columnRectAt(colInfo, i);
        columnBlock->flipForWritingMode(colRect);
        LayoutUnit logicalLeftOffset = (isHorizontal ? colRect.x() : colRect.y()) - columnBlock->logicalLeftOffsetForContent();
        LayoutSize offset;
        if (isHorizontal) {
            if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                offset = LayoutSize(logicalLeftOffset, currLogicalTopOffset);
            else
                offset = LayoutSize(0, colRect.y() + currLogicalTopOffset - columnBlock->borderTop() - columnBlock->paddingTop());
        } else {
            if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                offset = LayoutSize(currLogicalTopOffset, logicalLeftOffset);
            else
                offset = LayoutSize(colRect.x() + currLogicalTopOffset - columnBlock->borderLeft() - columnBlock->paddingLeft(), 0);
        }

        colRect.moveBy(layerOffset);

        LayoutRect localDirtyRect(paintingInfo.paintDirtyRect);
        localDirtyRect.intersect(colRect);

        if (!localDirtyRect.isEmpty()) {
            GraphicsContextStateSaver stateSaver(*context);

            // Each strip pushes a clip, since column boxes are specified as being
            // like overflow:hidden.
            context->clip(pixelSnappedIntRect(colRect));

            if (!colIndex) {
                // Apply a translation transform to change where the layer paints.
                TransformationMatrix oldTransform;
                bool oldHasTransform = childLayer->transform();
                if (oldHasTransform)
                    oldTransform = *childLayer->transform();
                TransformationMatrix newTransform(oldTransform);
                newTransform.translateRight(roundToInt(offset.width()), roundToInt(offset.height()));

                childLayer->m_transform = adoptPtr(new TransformationMatrix(newTransform));

                LayerPaintingInfo localPaintingInfo(paintingInfo);
                localPaintingInfo.paintDirtyRect = localDirtyRect;
                childLayer->paintLayer(context, localPaintingInfo, paintFlags);

                if (oldHasTransform)
                    childLayer->m_transform = adoptPtr(new TransformationMatrix(oldTransform));
                else
                    childLayer->m_transform.clear();
            } else {
                // Adjust the transform such that the renderer's upper left corner will paint at (0,0) in user space.
                // This involves subtracting out the position of the layer in our current coordinate space.
                LayoutPoint childOffset;
                columnLayers[colIndex - 1]->convertToLayerCoords(paintingInfo.rootLayer, childOffset);
                TransformationMatrix transform;
                transform.translateRight(roundToInt(childOffset.x() + offset.width()), roundToInt(childOffset.y() + offset.height()));

                // Apply the transform.
                context->concatCTM(transform.toAffineTransform());

                // Now do a paint with the root layer shifted to be the next multicol block.
                LayerPaintingInfo columnPaintingInfo(paintingInfo);
                columnPaintingInfo.rootLayer = columnLayers[colIndex - 1];
                columnPaintingInfo.paintDirtyRect = transform.inverse().mapRect(localDirtyRect);
                paintChildLayerIntoColumns(childLayer, context, columnPaintingInfo, paintFlags, columnLayers, colIndex - 1);
            }
        }

        // Move to the next position.
        LayoutUnit blockDelta = isHorizontal ? colRect.height() : colRect.width();
        if (columnBlock->style()->isFlippedBlocksWritingMode())
            currLogicalTopOffset += blockDelta;
        else
            currLogicalTopOffset -= blockDelta;
    }
}

static inline LayoutRect frameVisibleRect(RenderObject* renderer)
{
    FrameView* frameView = renderer->document().view();
    if (!frameView)
        return LayoutRect();

    return frameView->visibleContentRect();
}

bool RenderLayer::hitTest(const HitTestRequest& request, HitTestResult& result)
{
    return hitTest(request, result.hitTestLocation(), result);
}

bool RenderLayer::hitTest(const HitTestRequest& request, const HitTestLocation& hitTestLocation, HitTestResult& result)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    // RenderView should make sure to update layout before entering hit testing
    ASSERT(!renderer()->frame()->view()->layoutPending());
    ASSERT(!renderer()->document().renderer()->needsLayout());

    LayoutRect hitTestArea = isOutOfFlowRenderFlowThread() ? toRenderFlowThread(renderer())->borderBoxRect() : renderer()->view()->documentRect();
    if (!request.ignoreClipping())
        hitTestArea.intersect(frameVisibleRect(renderer()));

    RenderLayer* insideLayer = hitTestLayer(this, 0, request, result, hitTestArea, hitTestLocation, false);
    if (!insideLayer) {
        // We didn't hit any layer. If we are the root layer and the mouse is -- or just was -- down,
        // return ourselves. We do this so mouse events continue getting delivered after a drag has
        // exited the WebView, and so hit testing over a scrollbar hits the content document.
        if (!request.isChildFrameHitTest() && (request.active() || request.release()) && isRootLayer()) {
            renderer()->updateHitTestResult(result, toRenderView(renderer())->flipForWritingMode(hitTestLocation.point()));
            insideLayer = this;
        }
    }

    // Now determine if the result is inside an anchor - if the urlElement isn't already set.
    Node* node = result.innerNode();
    if (node && !result.URLElement())
        result.setURLElement(toElement(node->enclosingLinkEventParentOrSelf()));

    // Now return whether we were inside this layer (this will always be true for the root
    // layer).
    return insideLayer;
}

Node* RenderLayer::enclosingElement() const
{
    for (RenderObject* r = renderer(); r; r = r->parent()) {
        if (Node* e = r->node())
            return e;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

bool RenderLayer::isInTopLayer() const
{
    Node* node = renderer()->node();
    return node && node->isElementNode() && toElement(node)->isInTopLayer();
}

bool RenderLayer::isInTopLayerSubtree() const
{
    for (const RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (layer->isInTopLayer())
            return true;
    }
    return false;
}

// Compute the z-offset of the point in the transformState.
// This is effectively projecting a ray normal to the plane of ancestor, finding where that
// ray intersects target, and computing the z delta between those two points.
static double computeZOffset(const HitTestingTransformState& transformState)
{
    // We got an affine transform, so no z-offset
    if (transformState.m_accumulatedTransform.isAffine())
        return 0;

    // Flatten the point into the target plane
    FloatPoint targetPoint = transformState.mappedPoint();

    // Now map the point back through the transform, which computes Z.
    FloatPoint3D backmappedPoint = transformState.m_accumulatedTransform.mapPoint(FloatPoint3D(targetPoint));
    return backmappedPoint.z();
}

PassRefPtr<HitTestingTransformState> RenderLayer::createLocalTransformState(RenderLayer* rootLayer, RenderLayer* containerLayer,
                                        const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation,
                                        const HitTestingTransformState* containerTransformState,
                                        const LayoutPoint& translationOffset) const
{
    RefPtr<HitTestingTransformState> transformState;
    LayoutPoint offset;
    if (containerTransformState) {
        // If we're already computing transform state, then it's relative to the container (which we know is non-null).
        transformState = HitTestingTransformState::create(*containerTransformState);
        convertToLayerCoords(containerLayer, offset);
    } else {
        // If this is the first time we need to make transform state, then base it off of hitTestLocation,
        // which is relative to rootLayer.
        transformState = HitTestingTransformState::create(hitTestLocation.transformedPoint(), hitTestLocation.transformedRect(), FloatQuad(hitTestRect));
        convertToLayerCoords(rootLayer, offset);
    }
    offset.moveBy(translationOffset);

    RenderObject* containerRenderer = containerLayer ? containerLayer->renderer() : 0;
    if (renderer()->shouldUseTransformFromContainer(containerRenderer)) {
        TransformationMatrix containerTransform;
        renderer()->getTransformFromContainer(containerRenderer, toLayoutSize(offset), containerTransform);
        transformState->applyTransform(containerTransform, HitTestingTransformState::AccumulateTransform);
    } else {
        transformState->translate(offset.x(), offset.y(), HitTestingTransformState::AccumulateTransform);
    }

    return transformState;
}


static bool isHitCandidate(const RenderLayer* hitLayer, bool canDepthSort, double* zOffset, const HitTestingTransformState* transformState)
{
    if (!hitLayer)
        return false;

    // The hit layer is depth-sorting with other layers, so just say that it was hit.
    if (canDepthSort)
        return true;

    // We need to look at z-depth to decide if this layer was hit.
    if (zOffset) {
        ASSERT(transformState);
        // This is actually computing our z, but that's OK because the hitLayer is coplanar with us.
        double childZOffset = computeZOffset(*transformState);
        if (childZOffset > *zOffset) {
            *zOffset = childZOffset;
            return true;
        }
        return false;
    }

    return true;
}

// hitTestLocation and hitTestRect are relative to rootLayer.
// A 'flattening' layer is one preserves3D() == false.
// transformState.m_accumulatedTransform holds the transform from the containing flattening layer.
// transformState.m_lastPlanarPoint is the hitTestLocation in the plane of the containing flattening layer.
// transformState.m_lastPlanarQuad is the hitTestRect as a quad in the plane of the containing flattening layer.
//
// If zOffset is non-null (which indicates that the caller wants z offset information),
//  *zOffset on return is the z offset of the hit point relative to the containing flattening layer.
RenderLayer* RenderLayer::hitTestLayer(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
                                       const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, bool appliedTransform,
                                       const HitTestingTransformState* transformState, double* zOffset)
{
    if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
        return 0;

    // The natural thing would be to keep HitTestingTransformState on the stack, but it's big, so we heap-allocate.

    // Apply a transform if we have one.
    if (transform() && !appliedTransform) {
        if (enclosingPaginationLayer())
            return hitTestTransformedLayerInFragments(rootLayer, containerLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffset);

        // Make sure the parent's clip rects have been calculated.
        if (parent()) {
            ClipRectsContext clipRectsContext(rootLayer, hitTestLocation.region(), RootRelativeClipRects, IncludeOverlayScrollbarSize);
            ClipRect clipRect = backgroundClipRect(clipRectsContext);
            // Go ahead and test the enclosing clip now.
            if (!clipRect.intersects(hitTestLocation))
                return 0;
        }

        return hitTestLayerByApplyingTransform(rootLayer, containerLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffset);
    }

    // Ensure our lists and 3d status are up-to-date.
    updateLayerListsIfNeeded();
    update3DTransformedDescendantStatus();

    RefPtr<HitTestingTransformState> localTransformState;
    if (appliedTransform) {
        // We computed the correct state in the caller (above code), so just reference it.
        ASSERT(transformState);
        localTransformState = const_cast<HitTestingTransformState*>(transformState);
    } else if (transformState || m_has3DTransformedDescendant || preserves3D()) {
        // We need transform state for the first time, or to offset the container state, so create it here.
        localTransformState = createLocalTransformState(rootLayer, containerLayer, hitTestRect, hitTestLocation, transformState);
    }

    // Check for hit test on backface if backface-visibility is 'hidden'
    if (localTransformState && renderer()->style()->backfaceVisibility() == BackfaceVisibilityHidden) {
        TransformationMatrix invertedMatrix = localTransformState->m_accumulatedTransform.inverse();
        // If the z-vector of the matrix is negative, the back is facing towards the viewer.
        if (invertedMatrix.m33() < 0)
            return 0;
    }

    RefPtr<HitTestingTransformState> unflattenedTransformState = localTransformState;
    if (localTransformState && !preserves3D()) {
        // Keep a copy of the pre-flattening state, for computing z-offsets for the container
        unflattenedTransformState = HitTestingTransformState::create(*localTransformState);
        // This layer is flattening, so flatten the state passed to descendants.
        localTransformState->flatten();
    }

    // The following are used for keeping track of the z-depth of the hit point of 3d-transformed
    // descendants.
    double localZOffset = -numeric_limits<double>::infinity();
    double* zOffsetForDescendantsPtr = 0;
    double* zOffsetForContentsPtr = 0;

    bool depthSortDescendants = false;
    if (preserves3D()) {
        depthSortDescendants = true;
        // Our layers can depth-test with our container, so share the z depth pointer with the container, if it passed one down.
        zOffsetForDescendantsPtr = zOffset ? zOffset : &localZOffset;
        zOffsetForContentsPtr = zOffset ? zOffset : &localZOffset;
    } else if (m_has3DTransformedDescendant) {
        // Flattening layer with 3d children; use a local zOffset pointer to depth-test children and foreground.
        depthSortDescendants = true;
        zOffsetForDescendantsPtr = zOffset ? zOffset : &localZOffset;
        zOffsetForContentsPtr = zOffset ? zOffset : &localZOffset;
    } else if (zOffset) {
        zOffsetForDescendantsPtr = 0;
        // Container needs us to give back a z offset for the hit layer.
        zOffsetForContentsPtr = zOffset;
    }

    // This variable tracks which layer the mouse ends up being inside.
    RenderLayer* candidateLayer = 0;

    // Begin by walking our list of positive layers from highest z-index down to the lowest z-index.
    RenderLayer* hitLayer = hitTestList(posZOrderList(), rootLayer, request, result, hitTestRect, hitTestLocation,
                                        localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Now check our overflow objects.
    hitLayer = hitTestList(m_normalFlowList.get(), rootLayer, request, result, hitTestRect, hitTestLocation,
                           localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Collect the fragments. This will compute the clip rectangles for each layer fragment.
    LayerFragments layerFragments;
    collectFragments(layerFragments, rootLayer, hitTestLocation.region(), hitTestRect, RootRelativeClipRects, IncludeOverlayScrollbarSize);

    if (canResize() && hitTestResizerInFragments(layerFragments, hitTestLocation)) {
        renderer()->updateHitTestResult(result, hitTestLocation.point());
        return this;
    }

    // Next we want to see if the mouse pos is inside the child RenderObjects of the layer. Check
    // every fragment in reverse order.
    if (isSelfPaintingLayer()) {
        // Hit test with a temporary HitTestResult, because we only want to commit to 'result' if we know we're frontmost.
        HitTestResult tempResult(result.hitTestLocation());
        bool insideFragmentForegroundRect = false;
        if (hitTestContentsForFragments(layerFragments, request, tempResult, hitTestLocation, HitTestDescendants, insideFragmentForegroundRect)
            && isHitCandidate(this, false, zOffsetForContentsPtr, unflattenedTransformState.get())) {
            if (result.isRectBasedTest())
                result.append(tempResult);
            else
                result = tempResult;
            if (!depthSortDescendants)
                return this;
            // Foreground can depth-sort with descendant layers, so keep this as a candidate.
            candidateLayer = this;
        } else if (insideFragmentForegroundRect && result.isRectBasedTest())
            result.append(tempResult);
    }

    // Now check our negative z-index children.
    hitLayer = hitTestList(negZOrderList(), rootLayer, request, result, hitTestRect, hitTestLocation,
        localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // If we found a layer, return. Child layers, and foreground always render in front of background.
    if (candidateLayer)
        return candidateLayer;

    if (isSelfPaintingLayer()) {
        HitTestResult tempResult(result.hitTestLocation());
        bool insideFragmentBackgroundRect = false;
        if (hitTestContentsForFragments(layerFragments, request, tempResult, hitTestLocation, HitTestSelf, insideFragmentBackgroundRect)
            && isHitCandidate(this, false, zOffsetForContentsPtr, unflattenedTransformState.get())) {
            if (result.isRectBasedTest())
                result.append(tempResult);
            else
                result = tempResult;
            return this;
        }
        if (insideFragmentBackgroundRect && result.isRectBasedTest())
            result.append(tempResult);
    }

    return 0;
}

bool RenderLayer::hitTestContentsForFragments(const LayerFragments& layerFragments, const HitTestRequest& request, HitTestResult& result,
    const HitTestLocation& hitTestLocation, HitTestFilter hitTestFilter, bool& insideClipRect) const
{
    if (layerFragments.isEmpty())
        return false;

    for (int i = layerFragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if ((hitTestFilter == HitTestSelf && !fragment.backgroundRect.intersects(hitTestLocation))
            || (hitTestFilter == HitTestDescendants && !fragment.foregroundRect.intersects(hitTestLocation)))
            continue;
        insideClipRect = true;
        if (hitTestContents(request, result, fragment.layerBounds, hitTestLocation, hitTestFilter))
            return true;
    }

    return false;
}

bool RenderLayer::hitTestResizerInFragments(const LayerFragments& layerFragments, const HitTestLocation& hitTestLocation) const
{
    if (layerFragments.isEmpty())
        return false;

    for (int i = layerFragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if (fragment.backgroundRect.intersects(hitTestLocation) && resizerCornerRect(pixelSnappedIntRect(fragment.layerBounds), ResizerForPointer).contains(hitTestLocation.roundedPoint()))
            return true;
    }

    return false;
}

RenderLayer* RenderLayer::hitTestTransformedLayerInFragments(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffset)
{
    LayerFragments enclosingPaginationFragments;
    LayoutPoint offsetOfPaginationLayerFromRoot;
    LayoutRect transformedExtent = transparencyClipBox(this, enclosingPaginationLayer(), HitTestingTransparencyClipBox, RootOfTransparencyClipBox);
    enclosingPaginationLayer()->collectFragments(enclosingPaginationFragments, rootLayer, hitTestLocation.region(), hitTestRect,
        RootRelativeClipRects, IncludeOverlayScrollbarSize, RespectOverflowClip, &offsetOfPaginationLayerFromRoot, &transformedExtent);

    for (int i = enclosingPaginationFragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = enclosingPaginationFragments.at(i);

        // Apply the page/column clip for this fragment, as well as any clips established by layers in between us and
        // the enclosing pagination layer.
        LayoutRect clipRect = fragment.backgroundRect.rect();

        // Now compute the clips within a given fragment
        if (parent() != enclosingPaginationLayer()) {
            enclosingPaginationLayer()->convertToLayerCoords(rootLayer, offsetOfPaginationLayerFromRoot);

            ClipRectsContext clipRectsContext(enclosingPaginationLayer(), hitTestLocation.region(), RootRelativeClipRects, IncludeOverlayScrollbarSize);
            LayoutRect parentClipRect = backgroundClipRect(clipRectsContext).rect();
            parentClipRect.moveBy(fragment.paginationOffset + offsetOfPaginationLayerFromRoot);
            clipRect.intersect(parentClipRect);
        }

        if (!hitTestLocation.intersects(clipRect))
            continue;

        RenderLayer* hitLayer = hitTestLayerByApplyingTransform(rootLayer, containerLayer, request, result, hitTestRect, hitTestLocation,
            transformState, zOffset, fragment.paginationOffset);
        if (hitLayer)
            return hitLayer;
    }

    return 0;
}

RenderLayer* RenderLayer::hitTestLayerByApplyingTransform(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffset,
    const LayoutPoint& translationOffset)
{
    // Create a transform state to accumulate this transform.
    RefPtr<HitTestingTransformState> newTransformState = createLocalTransformState(rootLayer, containerLayer, hitTestRect, hitTestLocation, transformState, translationOffset);

    // If the transform can't be inverted, then don't hit test this layer at all.
    if (!newTransformState->m_accumulatedTransform.isInvertible())
        return 0;

    // Compute the point and the hit test rect in the coords of this layer by using the values
    // from the transformState, which store the point and quad in the coords of the last flattened
    // layer, and the accumulated transform which lets up map through preserve-3d layers.
    //
    // We can't just map hitTestLocation and hitTestRect because they may have been flattened (losing z)
    // by our container.
    FloatPoint localPoint = newTransformState->mappedPoint();
    FloatQuad localPointQuad = newTransformState->mappedQuad();
    LayoutRect localHitTestRect = newTransformState->boundsOfMappedArea();
    HitTestLocation newHitTestLocation;
    if (hitTestLocation.isRectBasedTest())
        newHitTestLocation = HitTestLocation(localPoint, localPointQuad);
    else
        newHitTestLocation = HitTestLocation(localPoint);

    // Now do a hit test with the root layer shifted to be us.
    return hitTestLayer(this, containerLayer, request, result, localHitTestRect, newHitTestLocation, true, newTransformState.get(), zOffset);
}

bool RenderLayer::hitTestContents(const HitTestRequest& request, HitTestResult& result, const LayoutRect& layerBounds, const HitTestLocation& hitTestLocation, HitTestFilter hitTestFilter) const
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    if (!renderer()->hitTest(request, result, hitTestLocation, toLayoutPoint(layerBounds.location() - renderBoxLocation()), hitTestFilter)) {
        // It's wrong to set innerNode, but then claim that you didn't hit anything, unless it is
        // a rect-based test.
        ASSERT(!result.innerNode() || (result.isRectBasedTest() && result.rectBasedTestResult().size()));
        return false;
    }

    // For positioned generated content, we might still not have a
    // node by the time we get to the layer level, since none of
    // the content in the layer has an element. So just walk up
    // the tree.
    if (!result.innerNode() || !result.innerNonSharedNode()) {
        Node* e = enclosingElement();
        if (!result.innerNode())
            result.setInnerNode(e);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(e);
    }

    return true;
}

RenderLayer* RenderLayer::hitTestList(Vector<RenderLayer*>* list, RenderLayer* rootLayer,
                                      const HitTestRequest& request, HitTestResult& result,
                                      const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation,
                                      const HitTestingTransformState* transformState,
                                      double* zOffsetForDescendants, double* zOffset,
                                      const HitTestingTransformState* unflattenedTransformState,
                                      bool depthSortDescendants)
{
    if (!list)
        return 0;

    if (!hasSelfPaintingLayerDescendant())
        return 0;

    RenderLayer* resultLayer = 0;
    for (int i = list->size() - 1; i >= 0; --i) {
        RenderLayer* childLayer = list->at(i);
        RenderLayer* hitLayer = 0;
        HitTestResult tempResult(result.hitTestLocation());
        if (childLayer->isPaginated())
            hitLayer = hitTestPaginatedChildLayer(childLayer, rootLayer, request, tempResult, hitTestRect, hitTestLocation, transformState, zOffsetForDescendants);
        else
            hitLayer = childLayer->hitTestLayer(rootLayer, this, request, tempResult, hitTestRect, hitTestLocation, false, transformState, zOffsetForDescendants);

        // If it a rect-based test, we can safely append the temporary result since it might had hit
        // nodes but not necesserily had hitLayer set.
        if (result.isRectBasedTest())
            result.append(tempResult);

        if (isHitCandidate(hitLayer, depthSortDescendants, zOffset, unflattenedTransformState)) {
            resultLayer = hitLayer;
            if (!result.isRectBasedTest())
                result = tempResult;
            if (!depthSortDescendants)
                break;
        }
    }

    return resultLayer;
}

RenderLayer* RenderLayer::hitTestPaginatedChildLayer(RenderLayer* childLayer, RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result,
                                                     const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffset)
{
    Vector<RenderLayer*> columnLayers;
    RenderLayer* ancestorLayer = isNormalFlowOnly() ? parent() : ancestorStackingContainer();
    for (RenderLayer* curr = childLayer->parent(); curr; curr = curr->parent()) {
        if (curr->renderer()->hasColumns() && checkContainingBlockChainForPagination(childLayer->renderer(), curr->renderBox()))
            columnLayers.append(curr);
        if (curr == ancestorLayer)
            break;
    }

    ASSERT(columnLayers.size());
    return hitTestChildLayerColumns(childLayer, rootLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffset,
                                    columnLayers, columnLayers.size() - 1);
}

RenderLayer* RenderLayer::hitTestChildLayerColumns(RenderLayer* childLayer, RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result,
                                                   const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffset,
                                                   const Vector<RenderLayer*>& columnLayers, size_t columnIndex)
{
    RenderBlock* columnBlock = toRenderBlock(columnLayers[columnIndex]->renderer());

    ASSERT(columnBlock && columnBlock->hasColumns());
    if (!columnBlock || !columnBlock->hasColumns())
        return 0;

    LayoutPoint layerOffset;
    columnBlock->layer()->convertToLayerCoords(rootLayer, layerOffset);

    ColumnInfo* colInfo = columnBlock->columnInfo();
    int colCount = columnBlock->columnCount(colInfo);

    // We have to go backwards from the last column to the first.
    bool isHorizontal = columnBlock->style()->isHorizontalWritingMode();
    LayoutUnit logicalLeft = columnBlock->logicalLeftOffsetForContent();
    LayoutUnit currLogicalTopOffset = 0;
    int i;
    for (i = 0; i < colCount; i++) {
        LayoutRect colRect = columnBlock->columnRectAt(colInfo, i);
        LayoutUnit blockDelta =  (isHorizontal ? colRect.height() : colRect.width());
        if (columnBlock->style()->isFlippedBlocksWritingMode())
            currLogicalTopOffset += blockDelta;
        else
            currLogicalTopOffset -= blockDelta;
    }
    for (i = colCount - 1; i >= 0; i--) {
        // For each rect, we clip to the rect, and then we adjust our coords.
        LayoutRect colRect = columnBlock->columnRectAt(colInfo, i);
        columnBlock->flipForWritingMode(colRect);
        LayoutUnit currLogicalLeftOffset = (isHorizontal ? colRect.x() : colRect.y()) - logicalLeft;
        LayoutUnit blockDelta =  (isHorizontal ? colRect.height() : colRect.width());
        if (columnBlock->style()->isFlippedBlocksWritingMode())
            currLogicalTopOffset -= blockDelta;
        else
            currLogicalTopOffset += blockDelta;

        LayoutSize offset;
        if (isHorizontal) {
            if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                offset = LayoutSize(currLogicalLeftOffset, currLogicalTopOffset);
            else
                offset = LayoutSize(0, colRect.y() + currLogicalTopOffset - columnBlock->borderTop() - columnBlock->paddingTop());
        } else {
            if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                offset = LayoutSize(currLogicalTopOffset, currLogicalLeftOffset);
            else
                offset = LayoutSize(colRect.x() + currLogicalTopOffset - columnBlock->borderLeft() - columnBlock->paddingLeft(), 0);
        }

        colRect.moveBy(layerOffset);

        LayoutRect localClipRect(hitTestRect);
        localClipRect.intersect(colRect);

        if (!localClipRect.isEmpty() && hitTestLocation.intersects(localClipRect)) {
            RenderLayer* hitLayer = 0;
            if (!columnIndex) {
                // Apply a translation transform to change where the layer paints.
                TransformationMatrix oldTransform;
                bool oldHasTransform = childLayer->transform();
                if (oldHasTransform)
                    oldTransform = *childLayer->transform();
                TransformationMatrix newTransform(oldTransform);
                newTransform.translateRight(offset.width(), offset.height());

                childLayer->m_transform = adoptPtr(new TransformationMatrix(newTransform));
                hitLayer = childLayer->hitTestLayer(rootLayer, columnLayers[0], request, result, localClipRect, hitTestLocation, false, transformState, zOffset);
                if (oldHasTransform)
                    childLayer->m_transform = adoptPtr(new TransformationMatrix(oldTransform));
                else
                    childLayer->m_transform.clear();
            } else {
                // Adjust the transform such that the renderer's upper left corner will be at (0,0) in user space.
                // This involves subtracting out the position of the layer in our current coordinate space.
                RenderLayer* nextLayer = columnLayers[columnIndex - 1];
                RefPtr<HitTestingTransformState> newTransformState = nextLayer->createLocalTransformState(rootLayer, nextLayer, localClipRect, hitTestLocation, transformState);
                newTransformState->translate(offset.width(), offset.height(), HitTestingTransformState::AccumulateTransform);
                FloatPoint localPoint = newTransformState->mappedPoint();
                FloatQuad localPointQuad = newTransformState->mappedQuad();
                LayoutRect localHitTestRect = newTransformState->mappedArea().enclosingBoundingBox();
                HitTestLocation newHitTestLocation;
                if (hitTestLocation.isRectBasedTest())
                    newHitTestLocation = HitTestLocation(localPoint, localPointQuad);
                else
                    newHitTestLocation = HitTestLocation(localPoint);
                newTransformState->flatten();

                hitLayer = hitTestChildLayerColumns(childLayer, columnLayers[columnIndex - 1], request, result, localHitTestRect, newHitTestLocation,
                                                    newTransformState.get(), zOffset, columnLayers, columnIndex - 1);
            }

            if (hitLayer)
                return hitLayer;
        }
    }

    return 0;
}

void RenderLayer::updateClipRects(const ClipRectsContext& clipRectsContext)
{
    ClipRectsType clipRectsType = clipRectsContext.clipRectsType;
    ASSERT(clipRectsType < NumCachedClipRectsTypes);
    if (m_clipRectsCache && m_clipRectsCache->getClipRects(clipRectsType, clipRectsContext.respectOverflowClip)) {
        ASSERT(clipRectsContext.rootLayer == m_clipRectsCache->m_clipRectsRoot[clipRectsType]);
        ASSERT(m_clipRectsCache->m_scrollbarRelevancy[clipRectsType] == clipRectsContext.overlayScrollbarSizeRelevancy);

#ifdef CHECK_CACHED_CLIP_RECTS
        // This code is useful to check cached clip rects, but is too expensive to leave enabled in debug builds by default.
        ClipRectsContext tempContext(clipRectsContext);
        tempContext.clipRectsType = TemporaryClipRects;
        ClipRects clipRects;
        calculateClipRects(tempContext, clipRects);
        ASSERT(clipRects == *m_clipRectsCache->getClipRects(clipRectsType, clipRectsContext.respectOverflowClip).get());
#endif
        return; // We have the correct cached value.
    }

    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent.  We want to cache clip rects with us as the root.
    RenderLayer* parentLayer = clipRectsContext.rootLayer != this ? parent() : 0;
    if (parentLayer)
        parentLayer->updateClipRects(clipRectsContext);

    ClipRects clipRects;
    calculateClipRects(clipRectsContext, clipRects);

    if (!m_clipRectsCache)
        m_clipRectsCache = adoptPtr(new ClipRectsCache);

    if (parentLayer && parentLayer->clipRects(clipRectsContext) && clipRects == *parentLayer->clipRects(clipRectsContext))
        m_clipRectsCache->setClipRects(clipRectsType, clipRectsContext.respectOverflowClip, parentLayer->clipRects(clipRectsContext));
    else
        m_clipRectsCache->setClipRects(clipRectsType, clipRectsContext.respectOverflowClip, ClipRects::create(clipRects));

#ifndef NDEBUG
    m_clipRectsCache->m_clipRectsRoot[clipRectsType] = clipRectsContext.rootLayer;
    m_clipRectsCache->m_scrollbarRelevancy[clipRectsType] = clipRectsContext.overlayScrollbarSizeRelevancy;
#endif
}

void RenderLayer::calculateClipRects(const ClipRectsContext& clipRectsContext, ClipRects& clipRects) const
{
    if (!parent()) {
        // The root layer's clip rect is always infinite.
        clipRects.reset(PaintInfo::infiniteRect());
        return;
    }

    ClipRectsType clipRectsType = clipRectsContext.clipRectsType;
    bool useCached = clipRectsType != TemporaryClipRects;

    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent.  We want to cache clip rects with us as the root.
    RenderLayer* parentLayer = clipRectsContext.rootLayer != this ? parent() : 0;

    // Ensure that our parent's clip has been calculated so that we can examine the values.
    if (parentLayer) {
        if (useCached && parentLayer->clipRects(clipRectsContext))
            clipRects = *parentLayer->clipRects(clipRectsContext);
        else {
            ClipRectsContext parentContext(clipRectsContext);
            parentContext.overlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize; // FIXME: why?
            parentLayer->calculateClipRects(parentContext, clipRects);
        }
    } else
        clipRects.reset(PaintInfo::infiniteRect());

    // A fixed object is essentially the root of its containing block hierarchy, so when
    // we encounter such an object, we reset our clip rects to the fixedClipRect.
    if (renderer()->style()->position() == FixedPosition) {
        clipRects.setPosClipRect(clipRects.fixedClipRect());
        clipRects.setOverflowClipRect(clipRects.fixedClipRect());
        clipRects.setFixed(true);
    } else if (renderer()->style()->hasInFlowPosition())
        clipRects.setPosClipRect(clipRects.overflowClipRect());
    else if (renderer()->style()->position() == AbsolutePosition)
        clipRects.setOverflowClipRect(clipRects.posClipRect());

    // Update the clip rects that will be passed to child layers.
    if ((renderer()->hasOverflowClip() && (clipRectsContext.respectOverflowClip == RespectOverflowClip || this != clipRectsContext.rootLayer)) || renderer()->hasClip()) {
        // This layer establishes a clip of some kind.

        // This offset cannot use convertToLayerCoords, because sometimes our rootLayer may be across
        // some transformed layer boundary, for example, in the RenderLayerCompositor overlapMap, where
        // clipRects are needed in view space.
        LayoutPoint offset;
        offset = roundedLayoutPoint(renderer()->localToContainerPoint(FloatPoint(), clipRectsContext.rootLayer->renderer()));
        RenderView* view = renderer()->view();
        ASSERT(view);
        if (view && clipRects.fixed() && clipRectsContext.rootLayer->renderer() == view) {
            offset -= view->frameView()->scrollOffsetForFixedPosition();
        }

        if (renderer()->hasOverflowClip()) {
            ClipRect newOverflowClip = toRenderBox(renderer())->overflowClipRect(offset, clipRectsContext.region, clipRectsContext.overlayScrollbarSizeRelevancy);
            if (renderer()->style()->hasBorderRadius())
                newOverflowClip.setHasRadius(true);
            clipRects.setOverflowClipRect(intersection(newOverflowClip, clipRects.overflowClipRect()));
            if (renderer()->isPositioned())
                clipRects.setPosClipRect(intersection(newOverflowClip, clipRects.posClipRect()));
        }
        if (renderer()->hasClip()) {
            LayoutRect newPosClip = toRenderBox(renderer())->clipRect(offset, clipRectsContext.region);
            clipRects.setPosClipRect(intersection(newPosClip, clipRects.posClipRect()));
            clipRects.setOverflowClipRect(intersection(newPosClip, clipRects.overflowClipRect()));
            clipRects.setFixedClipRect(intersection(newPosClip, clipRects.fixedClipRect()));
        }
    }
}

void RenderLayer::parentClipRects(const ClipRectsContext& clipRectsContext, ClipRects& clipRects) const
{
    ASSERT(parent());
    if (clipRectsContext.clipRectsType == TemporaryClipRects) {
        parent()->calculateClipRects(clipRectsContext, clipRects);
        return;
    }

    parent()->updateClipRects(clipRectsContext);
    clipRects = *parent()->clipRects(clipRectsContext);
}

static inline ClipRect backgroundClipRectForPosition(const ClipRects& parentRects, EPosition position)
{
    if (position == FixedPosition)
        return parentRects.fixedClipRect();

    if (position == AbsolutePosition)
        return parentRects.posClipRect();

    return parentRects.overflowClipRect();
}

ClipRect RenderLayer::backgroundClipRect(const ClipRectsContext& clipRectsContext) const
{
    ASSERT(parent());

    ClipRects parentRects;

    // If we cross into a different pagination context, then we can't rely on the cache.
    // Just switch over to using TemporaryClipRects.
    if (clipRectsContext.clipRectsType != TemporaryClipRects && parent()->enclosingPaginationLayer() != enclosingPaginationLayer()) {
        ClipRectsContext tempContext(clipRectsContext);
        tempContext.clipRectsType = TemporaryClipRects;
        parentClipRects(tempContext, parentRects);
    } else
        parentClipRects(clipRectsContext, parentRects);

    ClipRect backgroundClipRect = backgroundClipRectForPosition(parentRects, renderer()->style()->position());
    RenderView* view = renderer()->view();
    ASSERT(view);

    // Note: infinite clipRects should not be scrolled here, otherwise they will accidentally no longer be considered infinite.
    if (parentRects.fixed() && clipRectsContext.rootLayer->renderer() == view && backgroundClipRect != PaintInfo::infiniteRect())
        backgroundClipRect.move(view->frameView()->scrollOffsetForFixedPosition());

    return backgroundClipRect;
}

void RenderLayer::calculateRects(const ClipRectsContext& clipRectsContext, const LayoutRect& paintDirtyRect, LayoutRect& layerBounds,
    ClipRect& backgroundRect, ClipRect& foregroundRect, ClipRect& outlineRect, const LayoutPoint* offsetFromRoot) const
{
    if (clipRectsContext.rootLayer != this && parent()) {
        backgroundRect = backgroundClipRect(clipRectsContext);
        backgroundRect.intersect(paintDirtyRect);
    } else
        backgroundRect = paintDirtyRect;

    foregroundRect = backgroundRect;
    outlineRect = backgroundRect;

    LayoutPoint offset;
    if (offsetFromRoot)
        offset = *offsetFromRoot;
    else
        convertToLayerCoords(clipRectsContext.rootLayer, offset);
    layerBounds = LayoutRect(offset, size());

    // Update the clip rects that will be passed to child layers.
    if (renderer()->hasOverflowClip()) {
        // This layer establishes a clip of some kind.
        if (this != clipRectsContext.rootLayer || clipRectsContext.respectOverflowClip == RespectOverflowClip) {
            foregroundRect.intersect(toRenderBox(renderer())->overflowClipRect(offset, clipRectsContext.region, clipRectsContext.overlayScrollbarSizeRelevancy));
            if (renderer()->style()->hasBorderRadius())
                foregroundRect.setHasRadius(true);
        }

        // If we establish an overflow clip at all, then go ahead and make sure our background
        // rect is intersected with our layer's bounds including our visual overflow,
        // since any visual overflow like box-shadow or border-outset is not clipped by overflow:auto/hidden.
        if (renderBox()->hasVisualOverflow()) {
            // FIXME: Perhaps we should be propagating the borderbox as the clip rect for children, even though
            //        we may need to inflate our clip specifically for shadows or outsets.
            // FIXME: Does not do the right thing with CSS regions yet, since we don't yet factor in the
            // individual region boxes as overflow.
            LayoutRect layerBoundsWithVisualOverflow = renderBox()->visualOverflowRect();
            renderBox()->flipForWritingMode(layerBoundsWithVisualOverflow); // Layers are in physical coordinates, so the overflow has to be flipped.
            layerBoundsWithVisualOverflow.moveBy(offset);
            if (this != clipRectsContext.rootLayer || clipRectsContext.respectOverflowClip == RespectOverflowClip)
                backgroundRect.intersect(layerBoundsWithVisualOverflow);
        } else {
            // Shift the bounds to be for our region only.
            LayoutRect bounds = renderBox()->borderBoxRectInRegion(clipRectsContext.region);
            bounds.moveBy(offset);
            if (this != clipRectsContext.rootLayer || clipRectsContext.respectOverflowClip == RespectOverflowClip)
                backgroundRect.intersect(bounds);
        }
    }

    // CSS clip (different than clipping due to overflow) can clip to any box, even if it falls outside of the border box.
    if (renderer()->hasClip()) {
        // Clip applies to *us* as well, so go ahead and update the damageRect.
        LayoutRect newPosClip = toRenderBox(renderer())->clipRect(offset, clipRectsContext.region);
        backgroundRect.intersect(newPosClip);
        foregroundRect.intersect(newPosClip);
        outlineRect.intersect(newPosClip);
    }
}

LayoutRect RenderLayer::childrenClipRect() const
{
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    RenderView* renderView = renderer()->view();
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    ClipRectsContext clipRectsContext(clippingRootLayer, 0, TemporaryClipRects);
    // Need to use temporary clip rects, because the value of 'dontClipToOverflow' may be different from the painting path (<rdar://problem/11844909>).
    calculateRects(clipRectsContext, renderView->unscaledDocumentRect(), layerBounds, backgroundRect, foregroundRect, outlineRect);
    return clippingRootLayer->renderer()->localToAbsoluteQuad(FloatQuad(foregroundRect.rect())).enclosingBoundingBox();
}

LayoutRect RenderLayer::selfClipRect() const
{
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    RenderView* renderView = renderer()->view();
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    ClipRectsContext clipRectsContext(clippingRootLayer, 0, PaintingClipRects);
    calculateRects(clipRectsContext, renderView->documentRect(), layerBounds, backgroundRect, foregroundRect, outlineRect);
    return clippingRootLayer->renderer()->localToAbsoluteQuad(FloatQuad(backgroundRect.rect())).enclosingBoundingBox();
}

LayoutRect RenderLayer::localClipRect() const
{
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    ClipRectsContext clipRectsContext(clippingRootLayer, 0, PaintingClipRects);
    calculateRects(clipRectsContext, PaintInfo::infiniteRect(), layerBounds, backgroundRect, foregroundRect, outlineRect);

    LayoutRect clipRect = backgroundRect.rect();
    if (clipRect == PaintInfo::infiniteRect())
        return clipRect;

    LayoutPoint clippingRootOffset;
    convertToLayerCoords(clippingRootLayer, clippingRootOffset);
    clipRect.moveBy(-clippingRootOffset);

    return clipRect;
}

void RenderLayer::addBlockSelectionGapsBounds(const LayoutRect& bounds)
{
    m_blockSelectionGapsBounds.unite(enclosingIntRect(bounds));
}

void RenderLayer::clearBlockSelectionGapsBounds()
{
    m_blockSelectionGapsBounds = IntRect();
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->clearBlockSelectionGapsBounds();
}

void RenderLayer::repaintBlockSelectionGaps()
{
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->repaintBlockSelectionGaps();

    if (m_blockSelectionGapsBounds.isEmpty())
        return;

    LayoutRect rect = m_blockSelectionGapsBounds;
    rect.move(-scrolledContentOffset());
    if (renderer()->hasOverflowClip() && !usesCompositedScrolling())
        rect.intersect(toRenderBox(renderer())->overflowClipRect(LayoutPoint(), 0)); // FIXME: Regions not accounted for.
    if (renderer()->hasClip())
        rect.intersect(toRenderBox(renderer())->clipRect(LayoutPoint(), 0)); // FIXME: Regions not accounted for.
    if (!rect.isEmpty())
        renderer()->repaintRectangle(rect);
}

bool RenderLayer::intersectsDamageRect(const LayoutRect& layerBounds, const LayoutRect& damageRect, const RenderLayer* rootLayer, const LayoutPoint* offsetFromRoot) const
{
    // Always examine the canvas and the root.
    // FIXME: Could eliminate the isRoot() check if we fix background painting so that the RenderView
    // paints the root's background.
    if (isRootLayer() || renderer()->isRoot())
        return true;

    // If we aren't an inline flow, and our layer bounds do intersect the damage rect, then we
    // can go ahead and return true.
    RenderView* view = renderer()->view();
    ASSERT(view);
    if (view && !renderer()->isRenderInline()) {
        LayoutRect b = layerBounds;
        b.inflate(view->maximalOutlineSize());
        if (b.intersects(damageRect))
            return true;
    }

    // Otherwise we need to compute the bounding box of this single layer and see if it intersects
    // the damage rect.
    return boundingBox(rootLayer, 0, offsetFromRoot).intersects(damageRect);
}

LayoutRect RenderLayer::localBoundingBox(CalculateLayerBoundsFlags flags) const
{
    // There are three special cases we need to consider.
    // (1) Inline Flows.  For inline flows we will create a bounding box that fully encompasses all of the lines occupied by the
    // inline.  In other words, if some <span> wraps to three lines, we'll create a bounding box that fully encloses the
    // line boxes of all three lines (including overflow on those lines).
    // (2) Left/Top Overflow.  The width/height of layers already includes right/bottom overflow.  However, in the case of left/top
    // overflow, we have to create a bounding box that will extend to include this overflow.
    // (3) Floats.  When a layer has overhanging floats that it paints, we need to make sure to include these overhanging floats
    // as part of our bounding box.  We do this because we are the responsible layer for both hit testing and painting those
    // floats.
    LayoutRect result;
    if (renderer()->isInline() && renderer()->isRenderInline())
        result = toRenderInline(renderer())->linesVisualOverflowBoundingBox();
    else if (renderer()->isTableRow()) {
        // Our bounding box is just the union of all of our cells' border/overflow rects.
        for (RenderObject* child = renderer()->firstChild(); child; child = child->nextSibling()) {
            if (child->isTableCell()) {
                LayoutRect bbox = toRenderBox(child)->borderBoxRect();
                result.unite(bbox);
                LayoutRect overflowRect = renderBox()->visualOverflowRect();
                if (bbox != overflowRect)
                    result.unite(overflowRect);
            }
        }
    } else {
        RenderBox* box = renderBox();
        ASSERT(box);
        if (!(flags & DontConstrainForMask) && box->hasMask()) {
            result = box->maskClipRect();
            box->flipForWritingMode(result); // The mask clip rect is in physical coordinates, so we have to flip, since localBoundingBox is not.
        } else {
            LayoutRect bbox = box->borderBoxRect();
            result = bbox;
            LayoutRect overflowRect = box->visualOverflowRect();
            if (bbox != overflowRect)
                result.unite(overflowRect);
        }
    }

    RenderView* view = renderer()->view();
    ASSERT(view);
    if (view)
        result.inflate(view->maximalOutlineSize()); // Used to apply a fudge factor to dirty-rect checks on blocks/tables.

    return result;
}

LayoutRect RenderLayer::boundingBox(const RenderLayer* ancestorLayer, CalculateLayerBoundsFlags flags, const LayoutPoint* offsetFromRoot) const
{
    LayoutRect result = localBoundingBox(flags);
    if (renderer()->isBox())
        renderBox()->flipForWritingMode(result);
    else
        renderer()->containingBlock()->flipForWritingMode(result);

    if (enclosingPaginationLayer() && (flags & UseFragmentBoxes)) {
        // Split our box up into the actual fragment boxes that render in the columns/pages and unite those together to
        // get our true bounding box.
        LayoutPoint offsetWithinPaginationLayer;
        convertToLayerCoords(enclosingPaginationLayer(), offsetWithinPaginationLayer);
        result.moveBy(offsetWithinPaginationLayer);

        RenderFlowThread* enclosingFlowThread = toRenderFlowThread(enclosingPaginationLayer()->renderer());
        result = enclosingFlowThread->fragmentsBoundingBox(result);

        LayoutPoint delta;
        if (offsetFromRoot)
            delta = *offsetFromRoot;
        else
            enclosingPaginationLayer()->convertToLayerCoords(ancestorLayer, delta);
        result.moveBy(delta);
        return result;
    }

    LayoutPoint delta;
    if (offsetFromRoot)
        delta = *offsetFromRoot;
    else
        convertToLayerCoords(ancestorLayer, delta);

    result.moveBy(delta);
    return result;
}

IntRect RenderLayer::absoluteBoundingBox() const
{
    return pixelSnappedIntRect(boundingBox(root()));
}

IntRect RenderLayer::calculateLayerBounds(const RenderLayer* ancestorLayer, const LayoutPoint* offsetFromRoot, CalculateLayerBoundsFlags flags) const
{
    if (!isSelfPaintingLayer())
        return IntRect();

    // FIXME: This could be improved to do a check like hasVisibleNonCompositingDescendantLayers() (bug 92580).
    if ((flags & ExcludeHiddenDescendants) && this != ancestorLayer && !hasVisibleContent() && !hasVisibleDescendant())
        return IntRect();

    RenderLayerModelObject* renderer = this->renderer();

    if (isRootLayer()) {
        // The root layer is always just the size of the document.
        return renderer->view()->unscaledDocumentRect();
    }

    LayoutRect boundingBoxRect = localBoundingBox(flags);

    if (renderer->isBox())
        toRenderBox(renderer)->flipForWritingMode(boundingBoxRect);
    else
        renderer->containingBlock()->flipForWritingMode(boundingBoxRect);

    if (renderer->isRoot()) {
        // If the root layer becomes composited (e.g. because some descendant with negative z-index is composited),
        // then it has to be big enough to cover the viewport in order to display the background. This is akin
        // to the code in RenderBox::paintRootBoxFillLayers().
        if (FrameView* frameView = renderer->view()->frameView()) {
            LayoutUnit contentsWidth = frameView->contentsWidth();
            LayoutUnit contentsHeight = frameView->contentsHeight();

            boundingBoxRect.setWidth(max(boundingBoxRect.width(), contentsWidth - boundingBoxRect.x()));
            boundingBoxRect.setHeight(max(boundingBoxRect.height(), contentsHeight - boundingBoxRect.y()));
        }
    }

    LayoutRect unionBounds = boundingBoxRect;

    if (flags & UseLocalClipRectIfPossible) {
        LayoutRect localClipRect = this->localClipRect();
        if (localClipRect != PaintInfo::infiniteRect()) {
            if ((flags & IncludeSelfTransform) && paintsWithTransform(PaintBehaviorNormal))
                localClipRect = transform()->mapRect(localClipRect);

            LayoutPoint ancestorRelOffset;
            convertToLayerCoords(ancestorLayer, ancestorRelOffset);
            localClipRect.moveBy(ancestorRelOffset);
            return pixelSnappedIntRect(localClipRect);
        }
    }

    // FIXME: should probably just pass 'flags' down to descendants.
    CalculateLayerBoundsFlags descendantFlags = DefaultCalculateLayerBoundsFlags | (flags & ExcludeHiddenDescendants) | (flags & IncludeCompositedDescendants);

    const_cast<RenderLayer*>(this)->updateLayerListsIfNeeded();

    if (RenderLayer* reflection = reflectionLayer()) {
        if (!reflection->isComposited()) {
            IntRect childUnionBounds = reflection->calculateLayerBounds(this, 0, descendantFlags);
            unionBounds.unite(childUnionBounds);
        }
    }

    ASSERT(isStackingContainer() || (!posZOrderList() || !posZOrderList()->size()));

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(const_cast<RenderLayer*>(this));
#endif

    if (Vector<RenderLayer*>* negZOrderList = this->negZOrderList()) {
        size_t listSize = negZOrderList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = negZOrderList->at(i);
            if (flags & IncludeCompositedDescendants || !curLayer->isComposited()) {
                IntRect childUnionBounds = curLayer->calculateLayerBounds(this, 0, descendantFlags);
                unionBounds.unite(childUnionBounds);
            }
        }
    }

    if (Vector<RenderLayer*>* posZOrderList = this->posZOrderList()) {
        size_t listSize = posZOrderList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = posZOrderList->at(i);
            if (flags & IncludeCompositedDescendants || !curLayer->isComposited()) {
                IntRect childUnionBounds = curLayer->calculateLayerBounds(this, 0, descendantFlags);
                unionBounds.unite(childUnionBounds);
            }
        }
    }

    if (Vector<RenderLayer*>* normalFlowList = this->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            if (flags & IncludeCompositedDescendants || !curLayer->isComposited()) {
                IntRect curAbsBounds = curLayer->calculateLayerBounds(this, 0, descendantFlags);
                unionBounds.unite(curAbsBounds);
            }
        }
    }

    // FIXME: We can optimize the size of the composited layers, by not enlarging
    // filtered areas with the outsets if we know that the filter is going to render in hardware.
    // https://bugs.webkit.org/show_bug.cgi?id=81239
    if (flags & IncludeLayerFilterOutsets)
        renderer->style()->filterOutsets().expandRect(unionBounds);

    if ((flags & IncludeSelfTransform) && paintsWithTransform(PaintBehaviorNormal)) {
        TransformationMatrix* affineTrans = transform();
        boundingBoxRect = affineTrans->mapRect(boundingBoxRect);
        unionBounds = affineTrans->mapRect(unionBounds);
    }

    LayoutPoint ancestorRelOffset;
    if (offsetFromRoot)
        ancestorRelOffset = *offsetFromRoot;
    else
        convertToLayerCoords(ancestorLayer, ancestorRelOffset);
    unionBounds.moveBy(ancestorRelOffset);

    return pixelSnappedIntRect(unionBounds);
}

void RenderLayer::clearClipRectsIncludingDescendants(ClipRectsType typeToClear)
{
    // FIXME: it's not clear how this layer not having clip rects guarantees that no descendants have any.
    if (!m_clipRectsCache)
        return;

    clearClipRects(typeToClear);

    for (RenderLayer* l = firstChild(); l; l = l->nextSibling())
        l->clearClipRectsIncludingDescendants(typeToClear);
}

void RenderLayer::clearClipRects(ClipRectsType typeToClear)
{
    if (typeToClear == AllClipRectTypes)
        m_clipRectsCache = nullptr;
    else {
        ASSERT(typeToClear < NumCachedClipRectsTypes);
        RefPtr<ClipRects> dummy;
        m_clipRectsCache->setClipRects(typeToClear, RespectOverflowClip, dummy);
        m_clipRectsCache->setClipRects(typeToClear, IgnoreOverflowClip, dummy);
    }
}

RenderLayerBacking* RenderLayer::ensureBacking()
{
    if (!m_backing) {
        m_backing = adoptPtr(new RenderLayerBacking(this));
        compositor()->layerBecameComposited(this);

        updateOrRemoveFilterEffectRenderer();

        if (RuntimeEnabledFeatures::cssCompositingEnabled())
            backing()->setBlendMode(m_blendMode);
    }
    return m_backing.get();
}

void RenderLayer::clearBacking(bool layerBeingDestroyed)
{
    if (m_backing && !renderer()->documentBeingDestroyed())
        compositor()->layerBecameNonComposited(this);
    m_backing.clear();

    if (!layerBeingDestroyed)
        updateOrRemoveFilterEffectRenderer();
}

bool RenderLayer::hasCompositedMask() const
{
    return m_backing && m_backing->hasMaskLayer();
}

GraphicsLayer* RenderLayer::layerForScrolling() const
{
    return m_backing ? m_backing->scrollingContentsLayer() : 0;
}

GraphicsLayer* RenderLayer::layerForHorizontalScrollbar() const
{
    return m_backing ? m_backing->layerForHorizontalScrollbar() : 0;
}

GraphicsLayer* RenderLayer::layerForVerticalScrollbar() const
{
    return m_backing ? m_backing->layerForVerticalScrollbar() : 0;
}

GraphicsLayer* RenderLayer::layerForScrollCorner() const
{
    return m_backing ? m_backing->layerForScrollCorner() : 0;
}

bool RenderLayer::paintsWithTransform(PaintBehavior paintBehavior) const
{
    return transform() && ((paintBehavior & PaintBehaviorFlattenCompositingLayers) || !isComposited());
}

bool RenderLayer::backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const
{
    if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
        return false;

    if (paintsWithTransparency(PaintBehaviorNormal))
        return false;

    // We can't use hasVisibleContent(), because that will be true if our renderer is hidden, but some child
    // is visible and that child doesn't cover the entire rect.
    if (renderer()->style()->visibility() != VISIBLE)
        return false;

    if (paintsWithFilters() && renderer()->style()->filter().hasFilterThatAffectsOpacity())
        return false;

    // FIXME: Handle simple transforms.
    if (paintsWithTransform(PaintBehaviorNormal))
        return false;

    // FIXME: Remove this check.
    // This function should not be called when layer-lists are dirty.
    // It is somehow getting triggered during style update.
    if (m_zOrderListsDirty || m_normalFlowListDirty)
        return false;

    // FIXME: We currently only check the immediate renderer,
    // which will miss many cases.
    if (renderer()->backgroundIsKnownToBeOpaqueInRect(localRect))
        return true;

    // We can't consult child layers if we clip, since they might cover
    // parts of the rect that are clipped out.
    if (renderer()->hasOverflowClip())
        return false;

    return listBackgroundIsKnownToBeOpaqueInRect(posZOrderList(), localRect)
        || listBackgroundIsKnownToBeOpaqueInRect(negZOrderList(), localRect)
        || listBackgroundIsKnownToBeOpaqueInRect(normalFlowList(), localRect);
}

bool RenderLayer::listBackgroundIsKnownToBeOpaqueInRect(const Vector<RenderLayer*>* list, const LayoutRect& localRect) const
{
    if (!list || list->isEmpty())
        return false;

    for (Vector<RenderLayer*>::const_reverse_iterator iter = list->rbegin(); iter != list->rend(); ++iter) {
        const RenderLayer* childLayer = *iter;
        if (childLayer->isComposited())
            continue;

        if (!childLayer->canUseConvertToLayerCoords())
            continue;

        LayoutPoint childOffset;
        LayoutRect childLocalRect(localRect);
        childLayer->convertToLayerCoords(this, childOffset);
        childLocalRect.moveBy(-childOffset);

        if (childLayer->backgroundIsKnownToBeOpaqueInRect(childLocalRect))
            return true;
    }
    return false;
}

void RenderLayer::setParent(RenderLayer* parent)
{
    if (parent == m_parent)
        return;

    if (m_parent && !renderer()->documentBeingDestroyed())
        compositor()->layerWillBeRemoved(m_parent, this);

    m_parent = parent;

    if (m_parent && !renderer()->documentBeingDestroyed())
        compositor()->layerWasAdded(m_parent, this);
}

// Helper for the sorting of layers by z-index.
static inline bool compareZIndex(RenderLayer* first, RenderLayer* second)
{
    return first->zIndex() < second->zIndex();
}

void RenderLayer::dirtyNormalFlowListCanBePromotedToStackingContainer()
{
    m_canBePromotedToStackingContainerDirty = true;

    if (m_normalFlowListDirty || !normalFlowList())
        return;

    for (size_t index = 0; index < normalFlowList()->size(); ++index)
        normalFlowList()->at(index)->dirtyNormalFlowListCanBePromotedToStackingContainer();
}

void RenderLayer::dirtySiblingStackingContextCanBePromotedToStackingContainer()
{
    RenderLayer* ancestorStackingContext = this->ancestorStackingContext();
    if (!ancestorStackingContext)
        return;

    if (!ancestorStackingContext->m_zOrderListsDirty && ancestorStackingContext->posZOrderList()) {
        for (size_t index = 0; index < ancestorStackingContext->posZOrderList()->size(); ++index)
            ancestorStackingContext->posZOrderList()->at(index)->m_canBePromotedToStackingContainerDirty = true;
    }

    ancestorStackingContext->dirtyNormalFlowListCanBePromotedToStackingContainer();

    if (!ancestorStackingContext->m_zOrderListsDirty && ancestorStackingContext->negZOrderList()) {
        for (size_t index = 0; index < ancestorStackingContext->negZOrderList()->size(); ++index)
            ancestorStackingContext->negZOrderList()->at(index)->m_canBePromotedToStackingContainerDirty = true;
    }
}

void RenderLayer::dirtyZOrderLists()
{
    ASSERT(m_layerListMutationAllowed);
    ASSERT(isStackingContainer());

    if (m_posZOrderList)
        m_posZOrderList->clear();
    if (m_negZOrderList)
        m_negZOrderList->clear();
    m_zOrderListsDirty = true;

    m_canBePromotedToStackingContainerDirty = true;

    if (!renderer()->documentBeingDestroyed()) {
        compositor()->setNeedsUpdateCompositingRequirementsState();
        compositor()->setCompositingLayersNeedRebuild();
        if (acceleratedCompositingForOverflowScrollEnabled())
            compositor()->setShouldReevaluateCompositingAfterLayout();
    }
}

void RenderLayer::dirtyStackingContainerZOrderLists()
{
    // Any siblings in the ancestor stacking context could also be affected.
    // Changing z-index, for example, could cause us to stack in between a
    // sibling's descendants, meaning that we have to recompute
    // m_canBePromotedToStackingContainer for that sibling.
    dirtySiblingStackingContextCanBePromotedToStackingContainer();

    RenderLayer* stackingContainer = this->ancestorStackingContainer();
    if (stackingContainer)
        stackingContainer->dirtyZOrderLists();

    // Any change that could affect our stacking container's z-order list could
    // cause other RenderLayers in our stacking context to either opt in or out
    // of composited scrolling. It is important that we make our stacking
    // context aware of these z-order changes so the appropriate updating can
    // happen.
    RenderLayer* stackingContext = this->ancestorStackingContext();
    if (stackingContext && stackingContext != stackingContainer)
        stackingContext->dirtyZOrderLists();
}

void RenderLayer::dirtyNormalFlowList()
{
    ASSERT(m_layerListMutationAllowed);

    if (m_normalFlowList)
        m_normalFlowList->clear();
    m_normalFlowListDirty = true;

    if (!renderer()->documentBeingDestroyed()) {
        compositor()->setCompositingLayersNeedRebuild();
        if (acceleratedCompositingForOverflowScrollEnabled())
            compositor()->setShouldReevaluateCompositingAfterLayout();
    }
}

void RenderLayer::rebuildZOrderLists()
{
    ASSERT(m_layerListMutationAllowed);
    ASSERT(isDirtyStackingContainer());
    rebuildZOrderLists(m_posZOrderList, m_negZOrderList);
    m_zOrderListsDirty = false;
}

void RenderLayer::rebuildZOrderLists(OwnPtr<Vector<RenderLayer*> >& posZOrderList, OwnPtr<Vector<RenderLayer*> >& negZOrderList, const RenderLayer* layerToForceAsStackingContainer, CollectLayersBehavior collectLayersBehavior)
{
    bool includeHiddenLayers = compositor()->inCompositingMode();
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        if (!m_reflection || reflectionLayer() != child)
            child->collectLayers(includeHiddenLayers, posZOrderList, negZOrderList, layerToForceAsStackingContainer, collectLayersBehavior);

    // Sort the two lists.
    if (posZOrderList)
        std::stable_sort(posZOrderList->begin(), posZOrderList->end(), compareZIndex);

    if (negZOrderList)
        std::stable_sort(negZOrderList->begin(), negZOrderList->end(), compareZIndex);

    // Append layers for top layer elements after normal layer collection, to ensure they are on top regardless of z-indexes.
    // The renderers of top layer elements are children of the view, sorted in top layer stacking order.
    if (isRootLayer()) {
        RenderObject* view = renderer()->view();
        for (RenderObject* child = view->firstChild(); child; child = child->nextSibling()) {
            Element* childElement = (child->node() && child->node()->isElementNode()) ? toElement(child->node()) : 0;
            if (childElement && childElement->isInTopLayer()) {
                RenderLayer* layer = toRenderLayerModelObject(child)->layer();
                posZOrderList->append(layer);
            }
        }
    }
}

void RenderLayer::updateNormalFlowList()
{
    if (!m_normalFlowListDirty)
        return;

    ASSERT(m_layerListMutationAllowed);

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        // Ignore non-overflow layers and reflections.
        if (child->isNormalFlowOnly() && (!m_reflection || reflectionLayer() != child)) {
            if (!m_normalFlowList)
                m_normalFlowList = adoptPtr(new Vector<RenderLayer*>);
            m_normalFlowList->append(child);
        }
    }

    m_normalFlowListDirty = false;
}

void RenderLayer::collectLayers(bool includeHiddenLayers, OwnPtr<Vector<RenderLayer*> >& posBuffer, OwnPtr<Vector<RenderLayer*> >& negBuffer, const RenderLayer* layerToForceAsStackingContainer, CollectLayersBehavior collectLayersBehavior)
{
    if (isInTopLayer())
        return;

    updateDescendantDependentFlags();

    bool isStacking = false;
    bool isNormalFlow = false;

    switch (collectLayersBehavior) {
    case ForceLayerToStackingContainer:
        ASSERT(layerToForceAsStackingContainer);
        if (this == layerToForceAsStackingContainer) {
            isStacking = true;
            isNormalFlow = false;
        } else {
            isStacking = isStackingContext();
            isNormalFlow = shouldBeNormalFlowOnlyIgnoringCompositedScrolling();
        }
        break;
    case OverflowScrollCanBeStackingContainers:
        ASSERT(!layerToForceAsStackingContainer);
        isStacking = isStackingContainer();
        isNormalFlow = isNormalFlowOnly();
        break;
    case OnlyStackingContextsCanBeStackingContainers:
        isStacking = isStackingContext();
        isNormalFlow = shouldBeNormalFlowOnlyIgnoringCompositedScrolling();
        break;
    }

    // Overflow layers are just painted by their enclosing layers, so they don't get put in zorder lists.
    bool includeHiddenLayer = includeHiddenLayers || (m_hasVisibleContent || (m_hasVisibleDescendant && isStacking));
    if (includeHiddenLayer && !isNormalFlow && !isOutOfFlowRenderFlowThread()) {
        // Determine which buffer the child should be in.
        OwnPtr<Vector<RenderLayer*> >& buffer = (zIndex() >= 0) ? posBuffer : negBuffer;

        // Create the buffer if it doesn't exist yet.
        if (!buffer)
            buffer = adoptPtr(new Vector<RenderLayer*>);

        // Append ourselves at the end of the appropriate buffer.
        buffer->append(this);
    }

    // Recur into our children to collect more layers, but only if we don't establish
    // a stacking context/container.
    if ((includeHiddenLayers || m_hasVisibleDescendant) && !isStacking) {
        for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
            // Ignore reflections.
            if (!m_reflection || reflectionLayer() != child)
                child->collectLayers(includeHiddenLayers, posBuffer, negBuffer, layerToForceAsStackingContainer, collectLayersBehavior);
        }
    }
}

void RenderLayer::updateLayerListsIfNeeded()
{
    updateZOrderLists();
    updateNormalFlowList();

    if (RenderLayer* reflectionLayer = this->reflectionLayer()) {
        reflectionLayer->updateZOrderLists();
        reflectionLayer->updateNormalFlowList();
    }
}

void RenderLayer::repaintIncludingDescendants()
{
    renderer()->repaint();
    for (RenderLayer* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->repaintIncludingDescendants();
}

void RenderLayer::setBackingNeedsRepaint()
{
    ASSERT(isComposited());
    backing()->setContentsNeedDisplay();
}

void RenderLayer::setBackingNeedsRepaintInRect(const LayoutRect& r)
{
    // https://bugs.webkit.org/show_bug.cgi?id=61159 describes an unreproducible crash here,
    // so assert but check that the layer is composited.
    ASSERT(isComposited());
    if (!isComposited()) {
        // If we're trying to repaint the placeholder document layer, propagate the
        // repaint to the native view system.
        LayoutRect absRect(r);
        LayoutPoint delta;
        convertToLayerCoords(root(), delta);
        absRect.moveBy(delta);

        RenderView* view = renderer()->view();
        if (view)
            view->repaintViewRectangle(absRect);
    } else
        backing()->setContentsNeedDisplayInRect(pixelSnappedIntRect(r));
}

// Since we're only painting non-composited layers, we know that they all share the same repaintContainer.
void RenderLayer::repaintIncludingNonCompositingDescendants(RenderLayerModelObject* repaintContainer)
{
    renderer()->repaintUsingContainer(repaintContainer, pixelSnappedIntRect(renderer()->clippedOverflowRectForRepaint(repaintContainer)));

    for (RenderLayer* curr = firstChild(); curr; curr = curr->nextSibling()) {
        if (!curr->isComposited())
            curr->repaintIncludingNonCompositingDescendants(repaintContainer);
    }
}

bool RenderLayer::shouldBeNormalFlowOnly() const
{
    return shouldBeNormalFlowOnlyIgnoringCompositedScrolling() && !needsCompositedScrolling();
}

bool RenderLayer::shouldBeNormalFlowOnlyIgnoringCompositedScrolling() const
{
    const bool couldBeNormalFlow = renderer()->hasOverflowClip()
        || renderer()->hasReflection()
        || renderer()->hasMask()
        || renderer()->isCanvas()
        || renderer()->isVideo()
        || renderer()->isEmbeddedObject()
        || renderer()->isRenderIFrame()
        || (renderer()->style()->specifiesColumns() && !isRootLayer());
    const bool preventsElementFromBeingNormalFlow = renderer()->isPositioned()
        || renderer()->hasTransform()
        || renderer()->hasClipPath()
        || renderer()->hasFilter()
        || renderer()->hasBlendMode()
        || isTransparent();

    return couldBeNormalFlow && !preventsElementFromBeingNormalFlow;
}

void RenderLayer::updateIsNormalFlowOnly()
{
    bool isNormalFlowOnly = shouldBeNormalFlowOnly();
    if (isNormalFlowOnly == m_isNormalFlowOnly)
        return;

    m_isNormalFlowOnly = isNormalFlowOnly;
    if (RenderLayer* p = parent())
        p->dirtyNormalFlowList();
    dirtyStackingContainerZOrderLists();
}

bool RenderLayer::shouldBeSelfPaintingLayer() const
{
    return !isNormalFlowOnly()
        || hasOverlayScrollbars()
        || needsCompositedScrolling()
        || renderer()->hasReflection()
        || renderer()->hasMask()
        || renderer()->isTableRow()
        || renderer()->isCanvas()
        || renderer()->isVideo()
        || renderer()->isEmbeddedObject()
        || renderer()->isRenderIFrame();
}

void RenderLayer::updateSelfPaintingLayer()
{
    bool isSelfPaintingLayer = shouldBeSelfPaintingLayer();
    if (m_isSelfPaintingLayer == isSelfPaintingLayer)
        return;

    m_isSelfPaintingLayer = isSelfPaintingLayer;
    if (!parent())
        return;
    if (isSelfPaintingLayer)
        parent()->setAncestorChainHasSelfPaintingLayerDescendant();
    else
        parent()->dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();
}

bool RenderLayer::hasNonEmptyChildRenderers() const
{
    // Some HTML can cause whitespace text nodes to have renderers, like:
    // <div>
    // <img src=...>
    // </div>
    // so test for 0x0 RenderTexts here
    for (RenderObject* child = renderer()->firstChild(); child; child = child->nextSibling()) {
        if (!child->hasLayer()) {
            if (child->isRenderInline() || !child->isBox())
                return true;

            if (toRenderBox(child)->width() > 0 || toRenderBox(child)->height() > 0)
                return true;
        }
    }
    return false;
}

static bool hasBoxDecorations(const RenderStyle* style)
{
    return style->hasBorder() || style->hasBorderRadius() || style->hasOutline() || style->hasAppearance() || style->boxShadow() || style->hasFilter();
}

bool RenderLayer::hasBoxDecorationsOrBackground() const
{
    return hasBoxDecorations(renderer()->style()) || renderer()->hasBackground();
}

bool RenderLayer::hasVisibleBoxDecorations() const
{
    if (!hasVisibleContent())
        return false;

    return hasBoxDecorationsOrBackground() || hasOverflowControls();
}

bool RenderLayer::isVisuallyNonEmpty() const
{
    ASSERT(!m_visibleDescendantStatusDirty);

    if (hasVisibleContent() && hasNonEmptyChildRenderers())
        return true;

    if (renderer()->isReplaced() || renderer()->hasMask())
        return true;

    if (hasVisibleBoxDecorations())
        return true;

    return false;
}

void RenderLayer::updateVisibilityAfterStyleChange(const RenderStyle* oldStyle)
{
    if (!oldStyle || (oldStyle->visibility() != renderer()->style()->visibility()))
        compositor()->setNeedsUpdateCompositingRequirementsState();
}

void RenderLayer::updateStackingContextsAfterStyleChange(const RenderStyle* oldStyle)
{
    bool wasStackingContext = oldStyle ? isStackingContext(oldStyle) : false;
    EVisibility oldVisibility = oldStyle ? oldStyle->visibility() : VISIBLE;
    int oldZIndex = oldStyle ? oldStyle->zIndex() : 0;

    // FIXME: RenderLayer already handles visibility changes through our visiblity dirty bits. This logic could
    // likely be folded along with the rest.
    bool isStackingContext = this->isStackingContext();
    if (isStackingContext == wasStackingContext && oldVisibility == renderer()->style()->visibility() && oldZIndex == renderer()->style()->zIndex())
        return;

    dirtyStackingContainerZOrderLists();

    if (isStackingContainer())
        dirtyZOrderLists();
    else
        clearZOrderLists();

    compositor()->setNeedsUpdateCompositingRequirementsState();
}

static bool overflowRequiresScrollbar(EOverflow overflow)
{
    return overflow == OSCROLL;
}

static bool overflowDefinesAutomaticScrollbar(EOverflow overflow)
{
    return overflow == OAUTO || overflow == OOVERLAY;
}

void RenderLayer::updateScrollbarsAfterStyleChange(const RenderStyle* oldStyle)
{
    // Overflow are a box concept.
    RenderBox* box = renderBox();
    if (!box)
        return;

    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (box->style()->appearance() == ListboxPart)
        return;

    EOverflow overflowX = box->style()->overflowX();
    EOverflow overflowY = box->style()->overflowY();

    // To avoid doing a relayout in updateScrollbarsAfterLayout, we try to keep any automatic scrollbar that was already present.
    bool needsHorizontalScrollbar = (hasHorizontalScrollbar() && overflowDefinesAutomaticScrollbar(overflowX)) || overflowRequiresScrollbar(overflowX);
    bool needsVerticalScrollbar = (hasVerticalScrollbar() && overflowDefinesAutomaticScrollbar(overflowY)) || overflowRequiresScrollbar(overflowY);
    setHasHorizontalScrollbar(needsHorizontalScrollbar);
    setHasVerticalScrollbar(needsVerticalScrollbar);

    // With overflow: scroll, scrollbars are always visible but may be disabled.
    // When switching to another value, we need to re-enable them (see bug 11985).
    if (needsHorizontalScrollbar && oldStyle && oldStyle->overflowX() == OSCROLL && overflowX != OSCROLL) {
        ASSERT(hasHorizontalScrollbar());
        m_hBar->setEnabled(true);
    }

    if (needsVerticalScrollbar && oldStyle && oldStyle->overflowY() == OSCROLL && overflowY != OSCROLL) {
        ASSERT(hasVerticalScrollbar());
        m_vBar->setEnabled(true);
    }

    m_scrollableArea->updateAfterStyleChange(oldStyle);
}

void RenderLayer::updateOutOfFlowPositioned(const RenderStyle* oldStyle)
{
    if (oldStyle && (renderer()->style()->position() == oldStyle->position()))
        return;

    bool wasOutOfFlowPositioned = oldStyle && (oldStyle->position() == AbsolutePosition || oldStyle->position() == FixedPosition);
    bool isOutOfFlowPositioned = renderer()->isOutOfFlowPositioned();
    if (!wasOutOfFlowPositioned && !isOutOfFlowPositioned)
        return;

    // Even if the layer remains out-of-flow, a change to this property
    // will likely change its containing block. We must clear these bits
    // so that they can be set properly by the RenderLayerCompositor.
    for (RenderLayer* ancestor = parent(); ancestor; ancestor = ancestor->parent())
        ancestor->setHasUnclippedDescendant(false);

    // Ensures that we reset the above bits correctly.
    compositor()->setNeedsUpdateCompositingRequirementsState();

    if (wasOutOfFlowPositioned && isOutOfFlowPositioned)
        return;

    if (isOutOfFlowPositioned) {
        setAncestorChainHasOutOfFlowPositionedDescendant();
        compositor()->addOutOfFlowPositionedLayer(this);
    } else {
        dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();
        compositor()->removeOutOfFlowPositionedLayer(this);

        // We need to reset the isUnclippedDescendant bit here because normally
        // the "unclipped-ness" property is only updated in
        // RenderLayerCompositor::updateCompositingRequirementsState(). However,
        // it is only updated for layers which are known to be out of flow.
        // Since this is no longer out of flow, we have to explicitly ensure
        // that it doesn't think it is unclipped.
        setIsUnclippedDescendant(false);
    }
}

static bool hasOrHadFilters(const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    ASSERT(newStyle);
    return (oldStyle && oldStyle->hasFilter()) || newStyle->hasFilter();
}

inline bool RenderLayer::needsCompositingLayersRebuiltForClip(const RenderStyle* oldStyle, const RenderStyle* newStyle) const
{
    ASSERT(newStyle);
    return oldStyle && (oldStyle->clip() != newStyle->clip() || oldStyle->hasClip() != newStyle->hasClip());
}

inline bool RenderLayer::needsCompositingLayersRebuiltForOverflow(const RenderStyle* oldStyle, const RenderStyle* newStyle) const
{
    ASSERT(newStyle);
    return !isComposited() && oldStyle && (oldStyle->overflowX() != newStyle->overflowX()) && ancestorStackingContainer()->hasCompositingDescendant();
}

inline bool RenderLayer::needsCompositingLayersRebuiltForFilters(const RenderStyle* oldStyle, const RenderStyle* newStyle, bool didPaintWithFilters) const
{
    if (!hasOrHadFilters(oldStyle, newStyle))
        return false;

    if (renderer()->animation()->isRunningAcceleratedAnimationOnRenderer(renderer(), CSSPropertyWebkitFilter)) {
        // When the compositor is performing the filter animation, we shouldn't touch the compositing layers.
        // All of the layers above us should have been promoted to compositing layers already.
        return false;
    }

    FilterOutsets newOutsets = newStyle->filterOutsets();
    if (oldStyle && (oldStyle->filterOutsets() != newOutsets)) {
        // When filter outsets change, we need to:
        // (1) Recompute the overlap map to promote the correct layers to composited layers.
        // (2) Update the composited layer bounds (and child GraphicsLayer positions) on platforms
        //     whose compositors can't compute their own filter outsets.
        return true;
    }

#if HAVE(COMPOSITOR_FILTER_OUTSETS)
    if ((didPaintWithFilters != paintsWithFilters()) && !newOutsets.isZero()) {
        // When the layer used to paint filters in software and now paints filters in the
        // compositor, the compositing layer bounds need to change from including filter outsets to
        // excluding filter outsets, on platforms whose compositors compute their own outsets.
        // Similarly for the reverse change from compositor-painted to software-painted filters.
        return true;
    }
#endif

    return false;
}

void RenderLayer::updateFilters(const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    if (!hasOrHadFilters(oldStyle, newStyle))
        return;

    updateOrRemoveFilterClients();
    // During an accelerated animation, both WebKit and the compositor animate properties.
    // However, WebKit shouldn't ask the compositor to update its filters if the compositor is performing the animation.
    bool shouldUpdateFilters = isComposited() && !renderer()->animation()->isRunningAcceleratedAnimationOnRenderer(renderer(), CSSPropertyWebkitFilter);
    if (shouldUpdateFilters)
        backing()->updateFilters(renderer()->style());
    updateOrRemoveFilterEffectRenderer();
}

void RenderLayer::styleChanged(StyleDifference, const RenderStyle* oldStyle)
{
    updateIsNormalFlowOnly();

    updateResizerAreaSet();
    updateScrollbarsAfterStyleChange(oldStyle);
    updateStackingContextsAfterStyleChange(oldStyle);
    updateVisibilityAfterStyleChange(oldStyle);
    // Overlay scrollbars can make this layer self-painting so we need
    // to recompute the bit once scrollbars have been updated.
    updateSelfPaintingLayer();
    updateOutOfFlowPositioned(oldStyle);

    if (!hasReflection() && m_reflection)
        removeReflection();
    else if (hasReflection()) {
        if (!m_reflection)
            createReflection();
        UseCounter::count(&renderer()->document(), UseCounter::Reflection);
        updateReflectionStyle();
    }

    // FIXME: Need to detect a swap from custom to native scrollbars (and vice versa).
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();

    updateScrollCornerStyle();
    updateResizerStyle();

    updateDescendantDependentFlags();
    updateTransform();

    if (RuntimeEnabledFeatures::cssCompositingEnabled())
        updateBlendMode();

    bool didPaintWithFilters = false;

    if (paintsWithFilters())
        didPaintWithFilters = true;
    updateFilters(oldStyle, renderer()->style());

    const RenderStyle* newStyle = renderer()->style();
    if (compositor()->updateLayerCompositingState(this)
        || needsCompositingLayersRebuiltForClip(oldStyle, newStyle)
        || needsCompositingLayersRebuiltForOverflow(oldStyle, newStyle)
        || needsCompositingLayersRebuiltForFilters(oldStyle, newStyle, didPaintWithFilters))
        compositor()->setCompositingLayersNeedRebuild();
    else if (isComposited())
        backing()->updateGraphicsLayerGeometry();
}

void RenderLayer::updateResizerAreaSet() {
    Frame* frame = renderer()->frame();
    if (!frame)
        return;
    FrameView* frameView = frame->view();
    if (!frameView)
        return;
    if (canResize())
        frameView->addResizerArea(this);
    else
        frameView->removeResizerArea(this);
}

void RenderLayer::updateScrollableAreaSet(bool hasOverflow)
{
    Frame* frame = renderer()->frame();
    if (!frame)
        return;

    FrameView* frameView = frame->view();
    if (!frameView)
        return;

    bool isVisibleToHitTest = renderer()->visibleToHitTesting();
    if (HTMLFrameOwnerElement* owner = frame->ownerElement())
        isVisibleToHitTest &= owner->renderer() && owner->renderer()->visibleToHitTesting();

    if (hasOverflow && isVisibleToHitTest) {
        if (frameView->addScrollableArea(scrollableArea())) {
            compositor()->setNeedsUpdateCompositingRequirementsState();

            // Count the total number of RenderLayers that are scrollable areas for
            // any period. We only want to record this at most once per RenderLayer.
            if (!m_isScrollableAreaHasBeenRecorded) {
                HistogramSupport::histogramEnumeration("Renderer.CompositedScrolling", IsScrollableAreaBucket, CompositedScrollingHistogramMax);
                m_isScrollableAreaHasBeenRecorded = true;
            }
        }
    } else {
        if (frameView->removeScrollableArea(scrollableArea()))
            setNeedsCompositedScrolling(false);
    }
}

void RenderLayer::updateScrollCornerStyle()
{
    RenderObject* actualRenderer = rendererForScrollbar(renderer());
    RefPtr<RenderStyle> corner = renderer()->hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), actualRenderer->style()) : PassRefPtr<RenderStyle>(0);
    if (corner) {
        if (!m_scrollCorner) {
            m_scrollCorner = RenderScrollbarPart::createAnonymous(&renderer()->document());
            m_scrollCorner->setParent(renderer());
        }
        m_scrollCorner->setStyle(corner.release());
    } else if (m_scrollCorner) {
        m_scrollCorner->destroy();
        m_scrollCorner = 0;
    }
}

void RenderLayer::updateResizerStyle()
{
    RenderObject* actualRenderer = rendererForScrollbar(renderer());
    RefPtr<RenderStyle> resizer = renderer()->hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(PseudoStyleRequest(RESIZER), actualRenderer->style()) : PassRefPtr<RenderStyle>(0);
    if (resizer) {
        if (!m_resizer) {
            m_resizer = RenderScrollbarPart::createAnonymous(&renderer()->document());
            m_resizer->setParent(renderer());
        }
        m_resizer->setStyle(resizer.release());
    } else if (m_resizer) {
        m_resizer->destroy();
        m_resizer = 0;
    }
}

RenderLayer* RenderLayer::reflectionLayer() const
{
    return m_reflection ? m_reflection->layer() : 0;
}

void RenderLayer::createReflection()
{
    ASSERT(!m_reflection);
    m_reflection = RenderReplica::createAnonymous(&renderer()->document());
    m_reflection->setParent(renderer()); // We create a 1-way connection.
}

void RenderLayer::removeReflection()
{
    if (!m_reflection->documentBeingDestroyed())
        m_reflection->removeLayers(this);

    m_reflection->setParent(0);
    m_reflection->destroy();
    m_reflection = 0;
}

void RenderLayer::updateReflectionStyle()
{
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    newStyle->inheritFrom(renderer()->style());

    // Map in our transform.
    TransformOperations transform;
    switch (renderer()->style()->boxReflect()->direction()) {
        case ReflectionBelow:
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), Length(100., Percent), TransformOperation::Translate));
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), renderer()->style()->boxReflect()->offset(), TransformOperation::Translate));
            transform.operations().append(ScaleTransformOperation::create(1.0, -1.0, ScaleTransformOperation::Scale));
            break;
        case ReflectionAbove:
            transform.operations().append(ScaleTransformOperation::create(1.0, -1.0, ScaleTransformOperation::Scale));
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), Length(100., Percent), TransformOperation::Translate));
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), renderer()->style()->boxReflect()->offset(), TransformOperation::Translate));
            break;
        case ReflectionRight:
            transform.operations().append(TranslateTransformOperation::create(Length(100., Percent), Length(0, Fixed), TransformOperation::Translate));
            transform.operations().append(TranslateTransformOperation::create(renderer()->style()->boxReflect()->offset(), Length(0, Fixed), TransformOperation::Translate));
            transform.operations().append(ScaleTransformOperation::create(-1.0, 1.0, ScaleTransformOperation::Scale));
            break;
        case ReflectionLeft:
            transform.operations().append(ScaleTransformOperation::create(-1.0, 1.0, ScaleTransformOperation::Scale));
            transform.operations().append(TranslateTransformOperation::create(Length(100., Percent), Length(0, Fixed), TransformOperation::Translate));
            transform.operations().append(TranslateTransformOperation::create(renderer()->style()->boxReflect()->offset(), Length(0, Fixed), TransformOperation::Translate));
            break;
    }
    newStyle->setTransform(transform);

    // Map in our mask.
    newStyle->setMaskBoxImage(renderer()->style()->boxReflect()->mask());

    m_reflection->setStyle(newStyle.release());
}

bool RenderLayer::isCSSCustomFilterEnabled() const
{
    // We only want to enable shaders if WebGL is also enabled on this platform.
    const Settings* settings = renderer()->document().settings();
    return settings && settings->isCSSCustomFilterEnabled() && settings->webGLEnabled();
}

FilterOperations RenderLayer::computeFilterOperations(const RenderStyle* style)
{
    const FilterOperations& filters = style->filter();
    if (filters.hasReferenceFilter()) {
        for (size_t i = 0; i < filters.size(); ++i) {
            FilterOperation* filterOperation = filters.operations().at(i).get();
            if (filterOperation->getOperationType() != FilterOperation::REFERENCE)
                continue;
            ReferenceFilterOperation* referenceOperation = static_cast<ReferenceFilterOperation*>(filterOperation);
            // FIXME: Cache the ReferenceFilter if it didn't change.
            RefPtr<ReferenceFilter> referenceFilter = ReferenceFilter::create();
            float zoom = style->effectiveZoom() * WebCore::deviceScaleFactor(renderer()->frame());
            referenceFilter->setFilterResolution(FloatSize(zoom, zoom));
            referenceFilter->setLastEffect(ReferenceFilterBuilder::build(referenceFilter.get(), renderer(), referenceFilter->sourceGraphic(),
                referenceOperation));
            referenceOperation->setFilter(referenceFilter.release());
        }
    }

    if (!filters.hasCustomFilter())
        return filters;

    if (!isCSSCustomFilterEnabled()) {
        // CSS Custom filters should not parse at all in this case, but there might be
        // remaining styles that were parsed when the flag was enabled. Reproduces in DumpRenderTree
        // because it resets the flag while the previous test is still loaded.
        return FilterOperations();
    }

    FilterOperations outputFilters;
    for (size_t i = 0; i < filters.size(); ++i) {
        RefPtr<FilterOperation> filterOperation = filters.operations().at(i);
        if (filterOperation->getOperationType() == FilterOperation::CUSTOM) {
            // We have to wait until the program of CSS Shaders is loaded before setting it on the layer.
            // Note that we will handle the loading of the shaders and repainting of the layer in updateOrRemoveFilterClients.
            const CustomFilterOperation* customOperation = static_cast<const CustomFilterOperation*>(filterOperation.get());
            RefPtr<CustomFilterProgram> program = customOperation->program();
            if (!program->isLoaded())
                continue;

            CustomFilterGlobalContext* globalContext = renderer()->view()->customFilterGlobalContext();
            RefPtr<CustomFilterValidatedProgram> validatedProgram = globalContext->getValidatedProgram(program->programInfo());
            if (!validatedProgram->isInitialized())
                continue;

            RefPtr<ValidatedCustomFilterOperation> validatedOperation = ValidatedCustomFilterOperation::create(validatedProgram.release(),
                customOperation->parameters(), customOperation->meshRows(), customOperation->meshColumns(), customOperation->meshType());
            outputFilters.operations().append(validatedOperation.release());
            continue;
        }
        outputFilters.operations().append(filterOperation.release());
    }
    return outputFilters;
}

void RenderLayer::updateOrRemoveFilterClients()
{
    if (!hasFilter()) {
        removeFilterInfoIfNeeded();
        return;
    }

    if (renderer()->style()->filter().hasCustomFilter())
        ensureFilterInfo()->updateCustomFilterClients(renderer()->style()->filter());
    else if (hasFilterInfo())
        filterInfo()->removeCustomFilterClients();

    if (renderer()->style()->filter().hasReferenceFilter())
        ensureFilterInfo()->updateReferenceFilterClients(renderer()->style()->filter());
    else if (hasFilterInfo())
        filterInfo()->removeReferenceFilterClients();
}

void RenderLayer::updateOrRemoveFilterEffectRenderer()
{
    // FilterEffectRenderer is only used to render the filters in software mode,
    // so we always need to run updateOrRemoveFilterEffectRenderer after the composited
    // mode might have changed for this layer.
    if (!paintsWithFilters()) {
        // Don't delete the whole filter info here, because we might use it
        // for loading CSS shader files.
        if (RenderLayerFilterInfo* filterInfo = this->filterInfo())
            filterInfo->setRenderer(0);

        return;
    }

    RenderLayerFilterInfo* filterInfo = ensureFilterInfo();
    if (!filterInfo->renderer()) {
        RefPtr<FilterEffectRenderer> filterRenderer = FilterEffectRenderer::create();
        RenderingMode renderingMode = renderer()->frame()->page()->settings().acceleratedFiltersEnabled() ? Accelerated : Unaccelerated;
        filterRenderer->setRenderingMode(renderingMode);
        filterInfo->setRenderer(filterRenderer.release());

        // We can optimize away code paths in other places if we know that there are no software filters.
        renderer()->document().view()->setHasSoftwareFilters(true);
    }

    // If the filter fails to build, remove it from the layer. It will still attempt to
    // go through regular processing (e.g. compositing), but never apply anything.
    if (!filterInfo->renderer()->build(renderer(), computeFilterOperations(renderer()->style())))
        filterInfo->setRenderer(0);
}

void RenderLayer::filterNeedsRepaint()
{
    toElement(renderer()->node())->scheduleLayerUpdate();
    if (renderer()->view())
        renderer()->repaint();
}

void RenderLayer::addLayerHitTestRects(LayerHitTestRects& rects) const
{
    if (!size().isEmpty()) {
        Vector<LayoutRect> rect;

        if (renderBox() && renderBox()->scrollsOverflow()) {
            // For scrolling layers, rects are taken to be in the space of the contents.
            // We need to include both the entire contents, and also the bounding box
            // of the layer in the space of it's parent (eg. for border / scroll bars).
            rect.append(m_scrollableArea->overflowRect());
            rects.set(this, rect);
            if (const RenderLayer* parentLayer = parent()) {
                LayerHitTestRects::iterator iter = rects.find(parentLayer);
                if (iter == rects.end())
                    iter = rects.add(parentLayer, Vector<LayoutRect>()).iterator;
                iter->value.append(boundingBox(parentLayer));
            }
        } else {
            rect.append(localBoundingBox());
            rects.set(this, rect);
        }
    }

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->addLayerHitTestRects(rects);
}

const IntPoint& RenderLayer::scrollOrigin() const
{
    if (!m_scrollableArea) {
        static IntPoint emptyPoint = IntPoint::zero();
        return emptyPoint;
    }

    return m_scrollableArea->scrollOrigin();
}

int RenderLayer::scrollXOffset() const
{
    return m_scrollableArea->scrollXOffset();
}

int RenderLayer::scrollYOffset() const
{
    return m_scrollableArea->scrollYOffset();
}

IntSize RenderLayer::scrolledContentOffset() const
{
    return m_scrollableArea->scrollOffset();
}

bool RenderLayer::hasOverlayScrollbars() const
{
    return m_scrollableArea && m_scrollableArea->hasOverlayScrollbars();
}

} // namespace WebCore

#ifndef NDEBUG
void showLayerTree(const WebCore::RenderLayer* layer)
{
    if (!layer)
        return;

    if (WebCore::Frame* frame = layer->renderer()->frame()) {
        WTF::String output = externalRepresentation(frame, WebCore::RenderAsTextShowAllLayers | WebCore::RenderAsTextShowLayerNesting | WebCore::RenderAsTextShowCompositedLayers | WebCore::RenderAsTextShowAddresses | WebCore::RenderAsTextShowIDAndClass | WebCore::RenderAsTextDontUpdateLayout | WebCore::RenderAsTextShowLayoutState);
        fprintf(stderr, "%s\n", output.utf8().data());
    }
}

void showLayerTree(const WebCore::RenderObject* renderer)
{
    if (!renderer)
        return;
    showLayerTree(renderer->enclosingLayer());
}
#endif
