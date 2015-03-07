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
#include "core/layout/Layer.h"

#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/css/PseudoStyleRequest.h"
#include "core/dom/Document.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/DeprecatedScheduleStyleRecalcDuringLayout.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameElement.h"
#include "core/layout/ColumnInfo.h"
#include "core/layout/FilterEffectRenderer.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/HitTestingTransformState.h"
#include "core/layout/LayoutFlowThread.h"
#include "core/layout/LayoutGeometryMap.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutReplica.h"
#include "core/layout/LayoutScrollbar.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/layout/LayoutTreeAsText.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/LayerCompositor.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/ReferenceFilterBuilder.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "platform/LengthFunctions.h"
#include "platform/Partitions.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/TransformState.h"
#include "platform/graphics/filters/ReferenceFilter.h"
#include "platform/graphics/filters/SourceGraphic.h"
#include "platform/transforms/ScaleTransformOperation.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "public/platform/Platform.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CString.h"

namespace blink {

namespace {

static CompositingQueryMode gCompositingQueryMode =
    CompositingQueriesAreOnlyAllowedInCertainDocumentLifecyclePhases;

} // namespace

using namespace HTMLNames;

Layer::Layer(LayoutBoxModelObject* renderer, LayerType type)
    : m_layerType(type)
    , m_hasSelfPaintingLayerDescendant(false)
    , m_hasSelfPaintingLayerDescendantDirty(false)
    , m_isRootLayer(renderer->isLayoutView())
    , m_visibleContentStatusDirty(true)
    , m_hasVisibleContent(false)
    , m_visibleDescendantStatusDirty(false)
    , m_hasVisibleDescendant(false)
    , m_hasVisibleNonLayerContent(false)
    , m_isPaginated(false)
#if ENABLE(ASSERT)
    , m_needsPositionUpdate(true)
#endif
    , m_3DTransformedDescendantStatusDirty(true)
    , m_has3DTransformedDescendant(false)
    , m_containsDirtyOverlayScrollbars(false)
    , m_hasFilterInfo(false)
    , m_needsAncestorDependentCompositingInputsUpdate(true)
    , m_needsDescendantDependentCompositingInputsUpdate(true)
    , m_childNeedsCompositingInputsUpdate(true)
    , m_hasCompositingDescendant(false)
    , m_hasNonCompositedChild(false)
    , m_shouldIsolateCompositedDescendants(false)
    , m_lostGroupedMapping(false)
    , m_renderer(renderer)
    , m_parent(0)
    , m_previous(0)
    , m_next(0)
    , m_first(0)
    , m_last(0)
    , m_staticInlinePosition(0)
    , m_staticBlockPosition(0)
    , m_enclosingPaginationLayer(0)
    , m_potentialCompositingReasonsFromStyle(CompositingReasonNone)
    , m_compositingReasons(CompositingReasonNone)
    , m_groupedMapping(0)
    , m_clipper(*renderer)
{
    updateStackingNode();

    m_isSelfPaintingLayer = shouldBeSelfPaintingLayer();

    if (!renderer->slowFirstChild() && renderer->style()) {
        m_visibleContentStatusDirty = false;
        m_hasVisibleContent = renderer->style()->visibility() == VISIBLE;
    }

    updateScrollableArea();
}

Layer::~Layer()
{
    if (layoutObject()->frame() && layoutObject()->frame()->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = layoutObject()->frame()->page()->scrollingCoordinator())
            scrollingCoordinator->willDestroyLayer(this);
    }

    removeFilterInfoIfNeeded();

    if (groupedMapping()) {
        DisableCompositingQueryAsserts disabler;
        groupedMapping()->removeLayerFromSquashingGraphicsLayer(this);
        setGroupedMapping(0);
    }

    // Child layers will be deleted by their corresponding render objects, so
    // we don't need to delete them ourselves.

    clearCompositedLayerMapping(true);

    if (m_reflectionInfo)
        m_reflectionInfo->destroy();
}

String Layer::debugName() const
{
    if (isReflection())
        return layoutObject()->parent()->debugName() + " (reflection)";
    return layoutObject()->debugName();
}

LayerCompositor* Layer::compositor() const
{
    if (!layoutObject()->view())
        return 0;
    return layoutObject()->view()->compositor();
}

void Layer::contentChanged(ContentChangeType changeType)
{
    // updateLayerCompositingState will query compositingReasons for accelerated overflow scrolling.
    // This is tripped by LayoutTests/compositing/content-changed-chicken-egg.html
    DisableCompositingQueryAsserts disabler;

    if (changeType == CanvasChanged)
        compositor()->setNeedsCompositingUpdate(CompositingUpdateAfterCompositingInputChange);

    if (changeType == CanvasContextChanged) {
        compositor()->setNeedsCompositingUpdate(CompositingUpdateAfterCompositingInputChange);

        // Although we're missing test coverage, we need to call
        // GraphicsLayer::setContentsToPlatformLayer with the new platform
        // layer for this canvas.
        // See http://crbug.com/349195
        if (hasCompositedLayerMapping())
            compositedLayerMapping()->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateSubtree);
    }

    if (m_compositedLayerMapping)
        m_compositedLayerMapping->contentChanged(changeType);
}

bool Layer::paintsWithFilters() const
{
    if (!layoutObject()->hasFilter())
        return false;

    // https://code.google.com/p/chromium/issues/detail?id=343759
    DisableCompositingQueryAsserts disabler;
    return !m_compositedLayerMapping || compositingState() != PaintsIntoOwnBacking;
}

LayoutSize Layer::subpixelAccumulation() const
{
    return m_subpixelAccumulation;
}

void Layer::setSubpixelAccumulation(const LayoutSize& size)
{
    m_subpixelAccumulation = size;
}

void Layer::updateLayerPositionsAfterLayout()
{
    TRACE_EVENT0("blink,benchmark", "Layer::updateLayerPositionsAfterLayout");

    m_clipper.clearClipRectsIncludingDescendants();
    updateLayerPositionRecursive();

    {
        // FIXME: Remove incremental compositing updates after fixing the chicken/egg issues
        // https://code.google.com/p/chromium/issues/detail?id=343756
        DisableCompositingQueryAsserts disabler;
        bool needsPaginationUpdate = isPaginated() || enclosingPaginationLayer();
        updatePaginationRecursive(needsPaginationUpdate);
    }
}

void Layer::updateLayerPositionRecursive()
{
    updateLayerPosition();

    if (m_reflectionInfo)
        m_reflectionInfo->reflection()->layout();

    // FIXME(400589): We would like to do this in RenderLayerScrollableArea::updateAfterLayout,
    // but it depends on the size computed by updateLayerPosition.
    if (m_scrollableArea) {
        if (ScrollAnimator* scrollAnimator = m_scrollableArea->existingScrollAnimator())
            scrollAnimator->updateAfterLayout();
    }

    // FIXME: We should be able to remove this call because we don't care about
    // any descendant-dependent flags, but code somewhere else is reading these
    // flags and depending on us to update them.
    updateDescendantDependentFlags();

    for (Layer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositionRecursive();
}

void Layer::updateHasSelfPaintingLayerDescendant() const
{
    ASSERT(m_hasSelfPaintingLayerDescendantDirty);

    m_hasSelfPaintingLayerDescendant = false;

    for (Layer* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isSelfPaintingLayer() || child->hasSelfPaintingLayerDescendant()) {
            m_hasSelfPaintingLayerDescendant = true;
            break;
        }
    }

    m_hasSelfPaintingLayerDescendantDirty = false;
}

void Layer::dirtyAncestorChainHasSelfPaintingLayerDescendantStatus()
{
    for (Layer* layer = this; layer; layer = layer->parent()) {
        layer->m_hasSelfPaintingLayerDescendantDirty = true;
        // If we have reached a self-painting layer, we know our parent should have a self-painting descendant
        // in this case, there is no need to dirty our ancestors further.
        if (layer->isSelfPaintingLayer()) {
            ASSERT(!parent() || parent()->m_hasSelfPaintingLayerDescendantDirty || parent()->m_hasSelfPaintingLayerDescendant);
            break;
        }
    }
}

bool Layer::scrollsWithViewport() const
{
    return layoutObject()->style()->position() == FixedPosition && layoutObject()->containerForFixedPosition() == layoutObject()->view();
}

bool Layer::scrollsWithRespectTo(const Layer* other) const
{
    if (scrollsWithViewport() != other->scrollsWithViewport())
        return true;
    return ancestorScrollingLayer() != other->ancestorScrollingLayer();
}

void Layer::updateLayerPositionsAfterOverflowScroll()
{
    m_clipper.clearClipRectsIncludingDescendants();
    updateLayerPositionsAfterScrollRecursive();
}

void Layer::updateLayerPositionsAfterScrollRecursive()
{
    if (updateLayerPosition())
        m_renderer->setPreviousPaintInvalidationRect(m_renderer->boundsRectForPaintInvalidation(m_renderer->containerForPaintInvalidation()));

    for (Layer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositionsAfterScrollRecursive();
}

void Layer::updateTransformationMatrix()
{
    if (m_transform) {
        LayoutBox* box = layoutBox();
        ASSERT(box);
        m_transform->makeIdentity();
        box->style()->applyTransform(*m_transform, LayoutSize(box->pixelSnappedSize()), LayoutStyle::IncludeTransformOrigin);
        makeMatrixRenderable(*m_transform, compositor()->hasAcceleratedCompositing());
    }
}

void Layer::updateTransform(const LayoutStyle* oldStyle, const LayoutStyle& newStyle)
{
    if (oldStyle && newStyle.transformDataEquivalent(*oldStyle))
        return;

    // hasTransform() on the renderer is also true when there is transform-style: preserve-3d or perspective set,
    // so check style too.
    bool hasTransform = layoutObject()->hasTransformRelatedProperty() && newStyle.hasTransform();
    bool had3DTransform = has3DTransform();

    bool hadTransform = m_transform;
    if (hasTransform != hadTransform) {
        if (hasTransform)
            m_transform = adoptPtr(new TransformationMatrix);
        else
            m_transform.clear();

        // Layers with transforms act as clip rects roots, so clear the cached clip rects here.
        m_clipper.clearClipRectsIncludingDescendants();
    } else if (hasTransform) {
        m_clipper.clearClipRectsIncludingDescendants(AbsoluteClipRects);
    }

    updateTransformationMatrix();

    if (had3DTransform != has3DTransform())
        dirty3DTransformedDescendantStatus();
}

static Layer* enclosingLayerForContainingBlock(Layer* layer)
{
    if (LayoutObject* containingBlock = layer->layoutObject()->containingBlock())
        return containingBlock->enclosingLayer();
    return 0;
}

Layer* Layer::renderingContextRoot()
{
    Layer* renderingContext = 0;

    if (shouldPreserve3D())
        renderingContext = this;

    for (Layer* current = enclosingLayerForContainingBlock(this); current && current->shouldPreserve3D(); current = enclosingLayerForContainingBlock(current))
        renderingContext = current;

    return renderingContext;
}

TransformationMatrix Layer::currentTransform(LayoutStyle::ApplyTransformOrigin applyOrigin) const
{
    if (!m_transform)
        return TransformationMatrix();

    // m_transform includes transform-origin, so we need to recompute the transform here.
    if (applyOrigin == LayoutStyle::ExcludeTransformOrigin) {
        LayoutBox* box = layoutBox();
        TransformationMatrix currTransform;
        box->style()->applyTransform(currTransform, LayoutSize(box->pixelSnappedSize()), LayoutStyle::ExcludeTransformOrigin);
        makeMatrixRenderable(currTransform, compositor()->hasAcceleratedCompositing());
        return currTransform;
    }

    return *m_transform;
}

TransformationMatrix Layer::renderableTransform(PaintBehavior paintBehavior) const
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

static bool checkContainingBlockChainForPagination(LayoutBoxModelObject* renderer, LayoutBox* ancestorColumnsRenderer)
{
    LayoutView* view = renderer->view();
    LayoutBoxModelObject* prevBlock = renderer;
    LayoutBlock* containingBlock;
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

// Convert a bounding box from flow thread coordinates, relative to |layer|, to visual coordinates, relative to |ancestorLayer|.
// See http://www.chromium.org/developers/design-documents/multi-column-layout for more info on these coordinate types.
static void convertFromFlowThreadToVisualBoundingBoxInAncestor(const Layer* layer, const Layer* ancestorLayer, LayoutRect& rect)
{
    Layer* paginationLayer = layer->enclosingPaginationLayer();
    ASSERT(paginationLayer);
    LayoutFlowThread* flowThread = toLayoutFlowThread(paginationLayer->layoutObject());

    // First make the flow thread rectangle relative to the flow thread, not to |layer|.
    LayoutPoint offsetWithinPaginationLayer;
    layer->convertToLayerCoords(paginationLayer, offsetWithinPaginationLayer);
    rect.moveBy(offsetWithinPaginationLayer);

    // Then make the rectangle visual, relative to the fragmentation context. Split our box up into
    // the actual fragment boxes that render in the columns/pages and unite those together to get
    // our true bounding box.
    rect = flowThread->fragmentsBoundingBox(rect);

    // Finally, make the visual rectangle relative to |ancestorLayer|.
    if (ancestorLayer->enclosingPaginationLayer() != paginationLayer) {
        rect.moveBy(paginationLayer->visualOffsetFromAncestor(ancestorLayer));
        return;
    }
    // The ancestor layer is inside the same pagination layer as |layer|, so we need to subtract
    // the visual distance from the ancestor layer to the pagination layer.
    rect.moveBy(-ancestorLayer->visualOffsetFromAncestor(paginationLayer));
}

bool Layer::useRegionBasedColumns() const
{
    return layoutObject()->document().regionBasedColumnsEnabled();
}

void Layer::updatePaginationRecursive(bool needsPaginationUpdate)
{
    m_isPaginated = false;
    m_enclosingPaginationLayer = 0;

    if (useRegionBasedColumns() && layoutObject()->isLayoutFlowThread())
        needsPaginationUpdate = true;

    if (needsPaginationUpdate)
        updatePagination();

    if (layoutObject()->hasColumns())
        needsPaginationUpdate = true;

    for (Layer* child = firstChild(); child; child = child->nextSibling())
        child->updatePaginationRecursive(needsPaginationUpdate);
}

void Layer::updatePagination()
{
    bool usesRegionBasedColumns = useRegionBasedColumns();
    if ((!usesRegionBasedColumns && compositingState() != NotComposited) || !parent())
        return; // FIXME: For now the LayoutView can't be paginated.  Eventually printing will move to a model where it is though.

    // The main difference between the paginated booleans for the old column code and the new column code
    // is that each paginated layer has to paint on its own with the new code. There is no
    // recurring into child layers. This means that the m_isPaginated bits for the new column code can't just be set on
    // "roots" that get split and paint all their descendants. Instead each layer has to be checked individually and
    // genuinely know if it is going to have to split itself up when painting only its contents (and not any other descendant
    // layers). We track an enclosingPaginationLayer instead of using a simple bit, since we want to be able to get back
    // to that layer easily.
    if (usesRegionBasedColumns && layoutObject()->isLayoutFlowThread()) {
        m_enclosingPaginationLayer = this;
        return;
    }

    if (m_stackingNode->isNormalFlowOnly()) {
        if (usesRegionBasedColumns) {
            // Content inside a transform is not considered to be paginated, since we simply
            // paint the transform multiple times in each column, so we don't have to use
            // fragments for the transformed content.
            m_enclosingPaginationLayer = parent()->enclosingPaginationLayer();
            if (m_enclosingPaginationLayer && m_enclosingPaginationLayer->hasTransformRelatedProperty())
                m_enclosingPaginationLayer = 0;
        } else {
            m_isPaginated = parent()->layoutObject()->hasColumns();
        }
        return;
    }

    // For the new columns code, we want to walk up our containing block chain looking for an enclosing layer. Once
    // we find one, then we just check its pagination status.
    if (usesRegionBasedColumns) {
        LayoutView* view = layoutObject()->view();
        LayoutBlock* containingBlock;
        for (containingBlock = layoutObject()->containingBlock();
            containingBlock && containingBlock != view;
            containingBlock = containingBlock->containingBlock()) {
            if (containingBlock->hasLayer()) {
                // Content inside a transform is not considered to be paginated, since we simply
                // paint the transform multiple times in each column, so we don't have to use
                // fragments for the transformed content.
                m_enclosingPaginationLayer = containingBlock->layer()->enclosingPaginationLayer();
                if (m_enclosingPaginationLayer && m_enclosingPaginationLayer->hasTransformRelatedProperty())
                    m_enclosingPaginationLayer = 0;
                return;
            }
        }
        return;
    }

    // If we're not normal flow, then we need to look for a multi-column object between us and our stacking container.
    LayerStackingNode* ancestorStackingContextNode = m_stackingNode->ancestorStackingContextNode();
    for (Layer* curr = parent(); curr; curr = curr->parent()) {
        if (curr->layoutObject()->hasColumns()) {
            m_isPaginated = checkContainingBlockChainForPagination(layoutObject(), curr->layoutBox());
            return;
        }
        if (curr->stackingNode() == ancestorStackingContextNode)
            return;
    }
}

void Layer::clearPaginationRecursive()
{
    m_enclosingPaginationLayer = 0;
    for (Layer* child = firstChild(); child; child = child->nextSibling())
        child->clearPaginationRecursive();
}

LayoutPoint Layer::positionFromPaintInvalidationBacking(const LayoutObject* layoutObject, const LayoutBoxModelObject* paintInvalidationContainer, const PaintInvalidationState* paintInvalidationState)
{
    FloatPoint point = layoutObject->localToContainerPoint(FloatPoint(), paintInvalidationContainer, 0, 0, paintInvalidationState);

    // FIXME: Eventually we are going to unify coordinates in GraphicsLayer space.
    if (paintInvalidationContainer && paintInvalidationContainer->layer()->groupedMapping())
        mapPointToPaintBackingCoordinates(paintInvalidationContainer, point);

    return LayoutPoint(point);
}

void Layer::mapPointToPaintBackingCoordinates(const LayoutBoxModelObject* paintInvalidationContainer, FloatPoint& point)
{
    Layer* paintInvalidationLayer = paintInvalidationContainer->layer();
    if (!paintInvalidationLayer->groupedMapping()) {
        point.move(paintInvalidationLayer->compositedLayerMapping()->contentOffsetInCompositingLayer());
        return;
    }

    LayoutBoxModelObject* transformedAncestor = paintInvalidationLayer->enclosingTransformedAncestor()->layoutObject();
    if (!transformedAncestor)
        return;

    // |paintInvalidationContainer| may have a local 2D transform on it, so take that into account when mapping into the space of the
    // transformed ancestor.
    point = paintInvalidationContainer->localToContainerPoint(point, transformedAncestor);

    point.moveBy(-paintInvalidationLayer->groupedMapping()->squashingOffsetFromTransformedAncestor());
}

void Layer::mapRectToPaintBackingCoordinates(const LayoutBoxModelObject* paintInvalidationContainer, LayoutRect& rect)
{
    Layer* paintInvalidationLayer = paintInvalidationContainer->layer();
    if (!paintInvalidationLayer->groupedMapping()) {
        rect.move(paintInvalidationLayer->compositedLayerMapping()->contentOffsetInCompositingLayer());
        return;
    }

    LayoutBoxModelObject* transformedAncestor = paintInvalidationLayer->enclosingTransformedAncestor()->layoutObject();
    if (!transformedAncestor)
        return;

    // |paintInvalidationContainer| may have a local 2D transform on it, so take that into account when mapping into the space of the
    // transformed ancestor.
    rect = LayoutRect(paintInvalidationContainer->localToContainerQuad(FloatRect(rect), transformedAncestor).boundingBox());

    rect.moveBy(-paintInvalidationLayer->groupedMapping()->squashingOffsetFromTransformedAncestor());
}

void Layer::mapRectToPaintInvalidationBacking(const LayoutObject* layoutObject, const LayoutBoxModelObject* paintInvalidationContainer, LayoutRect& rect, const PaintInvalidationState* paintInvalidationState)
{
    if (!paintInvalidationContainer->layer()->groupedMapping()) {
        layoutObject->mapRectToPaintInvalidationBacking(paintInvalidationContainer, rect, paintInvalidationState);
        return;
    }

    // This code adjusts the paint invalidation rectangle to be in the space of the transformed ancestor of the grouped (i.e. squashed)
    // layer. This is because all layers that squash together need to issue paint invalidations w.r.t. a single container that is
    // an ancestor of all of them, in order to properly take into account any local transforms etc.
    // FIXME: remove this special-case code that works around the paint invalidation code structure.
    layoutObject->mapRectToPaintInvalidationBacking(paintInvalidationContainer, rect, paintInvalidationState);

    mapRectToPaintBackingCoordinates(paintInvalidationContainer, rect);
}

LayoutRect Layer::computePaintInvalidationRect(const LayoutObject* layoutObject, const Layer* paintInvalidationContainer, const PaintInvalidationState* paintInvalidationState)
{
    if (!paintInvalidationContainer->groupedMapping())
        return layoutObject->computePaintInvalidationRect(paintInvalidationContainer->layoutObject(), paintInvalidationState);

    LayoutRect rect = layoutObject->clippedOverflowRectForPaintInvalidation(paintInvalidationContainer->layoutObject(), paintInvalidationState);
    mapRectToPaintBackingCoordinates(paintInvalidationContainer->layoutObject(), rect);
    return rect;
}

void Layer::dirtyVisibleContentStatus()
{
    m_visibleContentStatusDirty = true;
    if (parent())
        parent()->dirtyAncestorChainVisibleDescendantStatus();
}

void Layer::potentiallyDirtyVisibleContentStatus(EVisibility visibility)
{
    if (m_visibleContentStatusDirty)
        return;
    if (hasVisibleContent() == (visibility == VISIBLE))
        return;
    dirtyVisibleContentStatus();
}

void Layer::dirtyAncestorChainVisibleDescendantStatus()
{
    for (Layer* layer = this; layer; layer = layer->parent()) {
        if (layer->m_visibleDescendantStatusDirty)
            break;

        layer->m_visibleDescendantStatusDirty = true;
    }
}

// FIXME: this is quite brute-force. We could be more efficient if we were to
// track state and update it as appropriate as changes are made in the Render tree.
void Layer::updateScrollingStateAfterCompositingChange()
{
    TRACE_EVENT0("blink", "Layer::updateScrollingStateAfterCompositingChange");
    m_hasVisibleNonLayerContent = false;
    for (LayoutObject* r = layoutObject()->slowFirstChild(); r; r = r->nextSibling()) {
        if (!r->hasLayer()) {
            m_hasVisibleNonLayerContent = true;
            break;
        }
    }

    m_hasNonCompositedChild = false;
    for (Layer* child = firstChild(); child; child = child->nextSibling()) {
        if (child->compositingState() == NotComposited) {
            m_hasNonCompositedChild = true;
            return;
        }
    }
}

// The descendant-dependent flags system is badly broken because we clean dirty
// bits in upward tree walks, which means we need to call updateDescendantDependentFlags
// at every node in the tree to fully clean all the dirty bits. While we'll in
// the process of fixing this issue, updateDescendantDependentFlagsForEntireSubtree
// provides a big hammer for actually cleaning all the dirty bits in a subtree.
//
// FIXME: Remove this function once the descendant-dependent flags system keeps
// its dirty bits scoped to subtrees.
void Layer::updateDescendantDependentFlagsForEntireSubtree()
{
    updateDescendantDependentFlags();

    for (Layer* child = firstChild(); child; child = child->nextSibling())
        child->updateDescendantDependentFlagsForEntireSubtree();
}

void Layer::updateDescendantDependentFlags()
{
    if (m_visibleDescendantStatusDirty) {
        m_hasVisibleDescendant = false;

        for (Layer* child = firstChild(); child; child = child->nextSibling()) {
            child->updateDescendantDependentFlags();

            if (child->m_hasVisibleContent || child->m_hasVisibleDescendant) {
                m_hasVisibleDescendant = true;
                break;
            }
        }

        m_visibleDescendantStatusDirty = false;
    }

    if (m_visibleContentStatusDirty) {
        bool previouslyHasVisibleContent = m_hasVisibleContent;
        if (layoutObject()->style()->visibility() == VISIBLE) {
            m_hasVisibleContent = true;
        } else {
            // layer may be hidden but still have some visible content, check for this
            m_hasVisibleContent = false;
            LayoutObject* r = layoutObject()->slowFirstChild();
            while (r) {
                if (r->style()->visibility() == VISIBLE && !r->hasLayer()) {
                    m_hasVisibleContent = true;
                    break;
                }
                LayoutObject* rendererFirstChild = r->slowFirstChild();
                if (rendererFirstChild && !r->hasLayer()) {
                    r = rendererFirstChild;
                } else if (r->nextSibling()) {
                    r = r->nextSibling();
                } else {
                    do {
                        r = r->parent();
                        if (r == layoutObject())
                            r = 0;
                    } while (r && !r->nextSibling());
                    if (r)
                        r = r->nextSibling();
                }
            }
        }
        m_visibleContentStatusDirty = false;

        if (hasVisibleContent() != previouslyHasVisibleContent) {
            setNeedsCompositingInputsUpdate();
            // We need to tell m_renderer to recheck its rect because we
            // pretend that invisible LayoutObjects have 0x0 rects. Changing
            // visibility therefore changes our rect and we need to visit
            // this LayoutObject during the invalidateTreeIfNeeded walk.
            m_renderer->setMayNeedPaintInvalidation();
        }
    }
}

void Layer::dirty3DTransformedDescendantStatus()
{
    LayerStackingNode* stackingNode = m_stackingNode->ancestorStackingContextNode();
    if (!stackingNode)
        return;

    stackingNode->layer()->m_3DTransformedDescendantStatusDirty = true;

    // This propagates up through preserve-3d hierarchies to the enclosing flattening layer.
    // Note that preserves3D() creates stacking context, so we can just run up the stacking containers.
    while (stackingNode && stackingNode->layer()->preserves3D()) {
        stackingNode->layer()->m_3DTransformedDescendantStatusDirty = true;
        stackingNode = stackingNode->ancestorStackingContextNode();
    }
}

// Return true if this layer or any preserve-3d descendants have 3d.
bool Layer::update3DTransformedDescendantStatus()
{
    if (m_3DTransformedDescendantStatusDirty) {
        m_has3DTransformedDescendant = false;

        m_stackingNode->updateZOrderLists();

        // Transformed or preserve-3d descendants can only be in the z-order lists, not
        // in the normal flow list, so we only need to check those.
        LayerStackingNodeIterator iterator(*m_stackingNode.get(), PositiveZOrderChildren | NegativeZOrderChildren);
        while (LayerStackingNode* node = iterator.next())
            m_has3DTransformedDescendant |= node->layer()->update3DTransformedDescendantStatus();

        m_3DTransformedDescendantStatusDirty = false;
    }

    // If we live in a 3d hierarchy, then the layer at the root of that hierarchy needs
    // the m_has3DTransformedDescendant set.
    if (preserves3D())
        return has3DTransform() || m_has3DTransformedDescendant;

    return has3DTransform();
}

bool Layer::updateLayerPosition()
{
    LayoutPoint localPoint;
    LayoutPoint inlineBoundingBoxOffset; // We don't put this into the Layer x/y for inlines, so we need to subtract it out when done.

    if (layoutObject()->isInline() && layoutObject()->isLayoutInline()) {
        LayoutInline* inlineFlow = toLayoutInline(layoutObject());
        IntRect lineBox = inlineFlow->linesBoundingBox();
        m_size = lineBox.size();
        inlineBoundingBoxOffset = lineBox.location();
        localPoint.moveBy(inlineBoundingBoxOffset);
    } else if (LayoutBox* box = layoutBox()) {
        m_size = pixelSnappedIntSize(box->size(), box->location());
        localPoint.moveBy(box->topLeftLocation());
    }

    if (!layoutObject()->isOutOfFlowPositioned() && !layoutObject()->isColumnSpanAll() && layoutObject()->parent()) {
        // We must adjust our position by walking up the render tree looking for the
        // nearest enclosing object with a layer.
        LayoutObject* curr = layoutObject()->parent();
        while (curr && !curr->hasLayer()) {
            if (curr->isBox() && !curr->isTableRow()) {
                // Rows and cells share the same coordinate space (that of the section).
                // Omit them when computing our xpos/ypos.
                localPoint.moveBy(toLayoutBox(curr)->topLeftLocation());
            }
            curr = curr->parent();
        }
        if (curr->isBox() && curr->isTableRow()) {
            // Put ourselves into the row coordinate space.
            localPoint.moveBy(-toLayoutBox(curr)->topLeftLocation());
        }
    }

    // Subtract our parent's scroll offset.
    if (layoutObject()->isOutOfFlowPositioned() && enclosingPositionedAncestor()) {
        Layer* positionedParent = enclosingPositionedAncestor();

        // For positioned layers, we subtract out the enclosing positioned layer's scroll offset.
        if (positionedParent->layoutObject()->hasOverflowClip()) {
            IntSize offset = positionedParent->layoutBox()->scrolledContentOffset();
            localPoint -= offset;
        }

        if (positionedParent->layoutObject()->isRelPositioned() && positionedParent->layoutObject()->isLayoutInline()) {
            LayoutSize offset = toLayoutInline(positionedParent->layoutObject())->offsetForInFlowPositionedInline(*toLayoutBox(layoutObject()));
            localPoint += offset;
        }
    } else if (parent()) {
        // FIXME: This code is very wrong, but luckily only needed in the old/current multicol
        // implementation. The compositing system doesn't understand columns and we're hacking
        // around that fact by faking the position of the Layers when we think we'll end up
        // being composited.
        if (hasStyleDeterminedDirectCompositingReasons() && !useRegionBasedColumns()) {
            // FIXME: Composited layers ignore pagination, so about the best we can do is make sure they're offset into the appropriate column.
            // They won't split across columns properly.
            if (!parent()->layoutObject()->hasColumns() && parent()->layoutObject()->isDocumentElement() && layoutObject()->view()->hasColumns())
                localPoint += layoutObject()->view()->columnOffset(localPoint);
            else
                localPoint += parent()->layoutObject()->columnOffset(localPoint);
        }

        if (parent()->layoutObject()->hasOverflowClip()) {
            IntSize scrollOffset = parent()->layoutBox()->scrolledContentOffset();
            localPoint -= scrollOffset;
        }
    }

    bool positionOrOffsetChanged = false;
    if (layoutObject()->isRelPositioned()) {
        LayoutSize newOffset = layoutObject()->offsetForInFlowPosition();
        positionOrOffsetChanged = newOffset != m_offsetForInFlowPosition;
        m_offsetForInFlowPosition = newOffset;
        localPoint.move(m_offsetForInFlowPosition);
    } else {
        m_offsetForInFlowPosition = LayoutSize();
    }

    // FIXME: We'd really like to just get rid of the concept of a layer rectangle and rely on the renderers.
    localPoint.moveBy(-inlineBoundingBoxOffset);

    if (m_location != localPoint)
        positionOrOffsetChanged = true;
    m_location = localPoint;

#if ENABLE(ASSERT)
    m_needsPositionUpdate = false;
#endif
    return positionOrOffsetChanged;
}

TransformationMatrix Layer::perspectiveTransform() const
{
    if (!layoutObject()->hasTransformRelatedProperty())
        return TransformationMatrix();

    const LayoutStyle& style = layoutObject()->styleRef();
    if (!style.hasPerspective())
        return TransformationMatrix();

    // Maybe fetch the perspective from the backing?
    const IntRect borderBox = toLayoutBox(layoutObject())->pixelSnappedBorderBoxRect();
    const float boxWidth = borderBox.width();
    const float boxHeight = borderBox.height();

    float perspectiveOriginX = floatValueForLength(style.perspectiveOriginX(), boxWidth);
    float perspectiveOriginY = floatValueForLength(style.perspectiveOriginY(), boxHeight);

    // A perspective origin of 0,0 makes the vanishing point in the center of the element.
    // We want it to be in the top-left, so subtract half the height and width.
    perspectiveOriginX -= boxWidth / 2.0f;
    perspectiveOriginY -= boxHeight / 2.0f;

    TransformationMatrix t;
    t.translate(perspectiveOriginX, perspectiveOriginY);
    t.applyPerspective(style.perspective());
    t.translate(-perspectiveOriginX, -perspectiveOriginY);

    return t;
}

FloatPoint Layer::perspectiveOrigin() const
{
    if (!layoutObject()->hasTransformRelatedProperty())
        return FloatPoint();

    const LayoutRect borderBox = toLayoutBox(layoutObject())->borderBoxRect();
    const LayoutStyle& style = layoutObject()->styleRef();

    return FloatPoint(floatValueForLength(style.perspectiveOriginX(), borderBox.width().toFloat()), floatValueForLength(style.perspectiveOriginY(), borderBox.height().toFloat()));
}

static inline bool isFixedPositionedContainer(Layer* layer)
{
    return layer->isRootLayer() || layer->hasTransformRelatedProperty();
}

Layer* Layer::enclosingPositionedAncestor() const
{
    if (layoutObject()->style()->position() == FixedPosition) {
        Layer* curr = parent();
        while (curr && !isFixedPositionedContainer(curr))
            curr = curr->parent();

        return curr;
    }

    Layer* curr = parent();
    while (curr && !curr->isPositionedContainer())
        curr = curr->parent();

    return curr;
}

Layer* Layer::enclosingTransformedAncestor() const
{
    Layer* curr = parent();
    while (curr && !curr->isRootLayer() && !curr->layoutObject()->hasTransformRelatedProperty())
        curr = curr->parent();

    return curr;
}

LayoutPoint Layer::computeOffsetFromTransformedAncestor() const
{
    const AncestorDependentCompositingInputs& properties = ancestorDependentCompositingInputs();

    TransformState transformState(TransformState::ApplyTransformDirection, FloatPoint());
    // FIXME: add a test that checks flipped writing mode and ApplyContainerFlip are correct.
    layoutObject()->mapLocalToContainer(properties.transformAncestor ? properties.transformAncestor->layoutObject() : 0, transformState, ApplyContainerFlip);
    transformState.flatten();
    return LayoutPoint(transformState.lastPlanarPoint());
}

const Layer* Layer::compositingContainer() const
{
    if (stackingNode()->isNormalFlowOnly())
        return parent();
    if (LayerStackingNode* ancestorStackingNode = stackingNode()->ancestorStackingContextNode())
        return ancestorStackingNode->layer();
    return 0;
}

bool Layer::isPaintInvalidationContainer() const
{
    return compositingState() == PaintsIntoOwnBacking || compositingState() == PaintsIntoGroupedBacking;
}

// Note: enclosingCompositingLayer does not include squashed layers. Compositing stacking children of squashed layers
// receive graphics layers that are parented to the compositing ancestor of the squashed layer.
Layer* Layer::enclosingLayerWithCompositedLayerMapping(IncludeSelfOrNot includeSelf) const
{
    ASSERT(isAllowedToQueryCompositingState());

    if ((includeSelf == IncludeSelf) && compositingState() != NotComposited && compositingState() != PaintsIntoGroupedBacking)
        return const_cast<Layer*>(this);

    for (const Layer* curr = compositingContainer(); curr; curr = curr->compositingContainer()) {
        if (curr->compositingState() != NotComposited && curr->compositingState() != PaintsIntoGroupedBacking)
            return const_cast<Layer*>(curr);
    }

    return 0;
}

// Return the enclosingCompositedLayerForPaintInvalidation for the given Layer
// including crossing frame boundaries.
Layer* Layer::enclosingLayerForPaintInvalidationCrossingFrameBoundaries() const
{
    const Layer* layer = this;
    Layer* compositedLayer = 0;
    while (!compositedLayer) {
        compositedLayer = layer->enclosingLayerForPaintInvalidation();
        if (!compositedLayer) {
            LayoutObject* owner = layer->layoutObject()->frame()->ownerLayoutObject();
            if (!owner)
                break;
            layer = owner->enclosingLayer();
        }
    }
    return compositedLayer;
}

Layer* Layer::enclosingLayerForPaintInvalidation() const
{
    ASSERT(isAllowedToQueryCompositingState());

    if (isPaintInvalidationContainer())
        return const_cast<Layer*>(this);

    for (const Layer* curr = compositingContainer(); curr; curr = curr->compositingContainer()) {
        if (curr->isPaintInvalidationContainer())
            return const_cast<Layer*>(curr);
    }

    return 0;
}

void Layer::setNeedsCompositingInputsUpdate()
{
    m_needsAncestorDependentCompositingInputsUpdate = true;
    m_needsDescendantDependentCompositingInputsUpdate = true;

    for (Layer* current = this; current && !current->m_childNeedsCompositingInputsUpdate; current = current->parent())
        current->m_childNeedsCompositingInputsUpdate = true;

    compositor()->setNeedsCompositingUpdate(CompositingUpdateAfterCompositingInputChange);
}

void Layer::updateAncestorDependentCompositingInputs(const AncestorDependentCompositingInputs& compositingInputs)
{
    m_ancestorDependentCompositingInputs = compositingInputs;
    m_needsAncestorDependentCompositingInputsUpdate = false;
}

void Layer::updateDescendantDependentCompositingInputs(const DescendantDependentCompositingInputs& compositingInputs)
{
    m_descendantDependentCompositingInputs = compositingInputs;
    m_needsDescendantDependentCompositingInputsUpdate = false;
}

void Layer::didUpdateCompositingInputs()
{
    ASSERT(!needsCompositingInputsUpdate());
    m_childNeedsCompositingInputsUpdate = false;
    if (m_scrollableArea)
        m_scrollableArea->updateNeedsCompositedScrolling();
}

bool Layer::hasNonIsolatedDescendantWithBlendMode() const
{
    if (descendantDependentCompositingInputs().hasNonIsolatedDescendantWithBlendMode)
        return true;
    if (layoutObject()->isSVGRoot())
        return toLayoutSVGRoot(layoutObject())->hasNonIsolatedBlendingDescendants();
    return false;
}

void Layer::setCompositingReasons(CompositingReasons reasons, CompositingReasons mask)
{
    if ((compositingReasons() & mask) == (reasons & mask))
        return;
    m_compositingReasons = (reasons & mask) | (compositingReasons() & ~mask);
}

void Layer::setHasCompositingDescendant(bool hasCompositingDescendant)
{
    if (m_hasCompositingDescendant == static_cast<unsigned>(hasCompositingDescendant))
        return;

    m_hasCompositingDescendant = hasCompositingDescendant;

    if (hasCompositedLayerMapping())
        compositedLayerMapping()->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateLocal);
}

void Layer::setShouldIsolateCompositedDescendants(bool shouldIsolateCompositedDescendants)
{
    if (m_shouldIsolateCompositedDescendants == static_cast<unsigned>(shouldIsolateCompositedDescendants))
        return;

    m_shouldIsolateCompositedDescendants = shouldIsolateCompositedDescendants;

    if (hasCompositedLayerMapping())
        compositedLayerMapping()->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateLocal);
}

bool Layer::hasAncestorWithFilterOutsets() const
{
    for (const Layer* curr = this; curr; curr = curr->parent()) {
        LayoutBoxModelObject* renderer = curr->layoutObject();
        if (renderer->style()->hasFilterOutsets())
            return true;
    }
    return false;
}

static void expandClipRectForDescendantsAndReflection(LayoutRect& clipRect, const Layer* layer, const Layer* rootLayer,
    Layer::TransparencyClipBoxBehavior transparencyBehavior, const LayoutSize& subPixelAccumulation, PaintBehavior paintBehavior)
{
    // If we have a mask, then the clip is limited to the border box area (and there is
    // no need to examine child layers).
    if (!layer->layoutObject()->hasMask()) {
        // Note: we don't have to walk z-order lists since transparent elements always establish
        // a stacking container. This means we can just walk the layer tree directly.
        for (Layer* curr = layer->firstChild(); curr; curr = curr->nextSibling()) {
            if (!layer->reflectionInfo() || layer->reflectionInfo()->reflectionLayer() != curr)
                clipRect.unite(Layer::transparencyClipBox(curr, rootLayer, transparencyBehavior, Layer::DescendantsOfTransparencyClipBox, subPixelAccumulation, paintBehavior));
        }
    }

    // If we have a reflection, then we need to account for that when we push the clip.  Reflect our entire
    // current transparencyClipBox to catch all child layers.
    // FIXME: Accelerated compositing will eventually want to do something smart here to avoid incorporating this
    // size into the parent layer.
    if (layer->layoutObject()->hasReflection()) {
        LayoutPoint delta;
        layer->convertToLayerCoords(rootLayer, delta);
        clipRect.move(-delta.x(), -delta.y());
        clipRect.unite(layer->layoutBox()->reflectedRect(clipRect));
        clipRect.moveBy(delta);
    }
}

LayoutRect Layer::transparencyClipBox(const Layer* layer, const Layer* rootLayer, TransparencyClipBoxBehavior transparencyBehavior,
    TransparencyClipBoxMode transparencyMode, const LayoutSize& subPixelAccumulation, PaintBehavior paintBehavior)
{
    // FIXME: Although this function completely ignores CSS-imposed clipping, we did already intersect with the
    // paintDirtyRect, and that should cut down on the amount we have to paint.  Still it
    // would be better to respect clips.

    if (rootLayer != layer && ((transparencyBehavior == PaintingTransparencyClipBox && layer->paintsWithTransform(paintBehavior))
        || (transparencyBehavior == HitTestingTransparencyClipBox && layer->hasTransformRelatedProperty()))) {
        // The best we can do here is to use enclosed bounding boxes to establish a "fuzzy" enough clip to encompass
        // the transformed layer and all of its children.
        const Layer* paginationLayer = transparencyMode == DescendantsOfTransparencyClipBox ? layer->enclosingPaginationLayer() : 0;
        const Layer* rootLayerForTransform = paginationLayer ? paginationLayer : rootLayer;
        LayoutPoint delta;
        layer->convertToLayerCoords(rootLayerForTransform, delta);

        delta.move(subPixelAccumulation);
        IntPoint pixelSnappedDelta = roundedIntPoint(delta);
        TransformationMatrix transform;
        transform.translate(pixelSnappedDelta.x(), pixelSnappedDelta.y());
        transform = transform * *layer->transform();

        // We don't use fragment boxes when collecting a transformed layer's bounding box, since it always
        // paints unfragmented.
        LayoutRect clipRect = layer->physicalBoundingBox(layer);
        expandClipRectForDescendantsAndReflection(clipRect, layer, layer, transparencyBehavior, subPixelAccumulation, paintBehavior);
        clipRect.expand(layer->layoutObject()->style()->filterOutsets());
        LayoutRect result = transform.mapRect(clipRect);
        if (!paginationLayer)
            return result;

        // We have to break up the transformed extent across our columns.
        // Split our box up into the actual fragment boxes that render in the columns/pages and unite those together to
        // get our true bounding box.
        LayoutFlowThread* enclosingFlowThread = toLayoutFlowThread(paginationLayer->layoutObject());
        result = enclosingFlowThread->fragmentsBoundingBox(result);

        LayoutPoint rootLayerDelta;
        paginationLayer->convertToLayerCoords(rootLayer, rootLayerDelta);
        result.moveBy(rootLayerDelta);
        return result;
    }

    LayoutRect clipRect = layer->fragmentsBoundingBox(rootLayer);
    expandClipRectForDescendantsAndReflection(clipRect, layer, rootLayer, transparencyBehavior, subPixelAccumulation, paintBehavior);
    clipRect.expand(layer->layoutObject()->style()->filterOutsets());
    clipRect.move(subPixelAccumulation);
    return clipRect;
}

LayoutRect Layer::paintingExtent(const Layer* rootLayer, const LayoutRect& paintDirtyRect, const LayoutSize& subPixelAccumulation, PaintBehavior paintBehavior)
{
    return intersection(transparencyClipBox(this, rootLayer, PaintingTransparencyClipBox, RootOfTransparencyClipBox, subPixelAccumulation, paintBehavior), paintDirtyRect);
}

void* Layer::operator new(size_t sz)
{
    return partitionAlloc(Partitions::getRenderingPartition(), sz);
}

void Layer::operator delete(void* ptr)
{
    partitionFree(ptr);
}

void Layer::addChild(Layer* child, Layer* beforeChild)
{
    Layer* prevSibling = beforeChild ? beforeChild->previousSibling() : lastChild();
    if (prevSibling) {
        child->setPreviousSibling(prevSibling);
        prevSibling->setNextSibling(child);
        ASSERT(prevSibling != child);
    } else {
        setFirstChild(child);
    }

    if (beforeChild) {
        beforeChild->setPreviousSibling(child);
        child->setNextSibling(beforeChild);
        ASSERT(beforeChild != child);
    } else {
        setLastChild(child);
    }

    child->m_parent = this;

    setNeedsCompositingInputsUpdate();

    if (child->stackingNode()->isNormalFlowOnly())
        m_stackingNode->dirtyNormalFlowList();

    if (!child->stackingNode()->isNormalFlowOnly() || child->firstChild()) {
        // Dirty the z-order list in which we are contained. The ancestorStackingContextNode() can be null in the
        // case where we're building up generated content layers. This is ok, since the lists will start
        // off dirty in that case anyway.
        child->stackingNode()->dirtyStackingContextZOrderLists();
    }

    dirtyAncestorChainVisibleDescendantStatus();
    dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

    child->updateDescendantDependentFlags();
}

Layer* Layer::removeChild(Layer* oldChild)
{
    if (oldChild->previousSibling())
        oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
    if (oldChild->nextSibling())
        oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

    if (m_first == oldChild)
        m_first = oldChild->nextSibling();
    if (m_last == oldChild)
        m_last = oldChild->previousSibling();

    if (oldChild->stackingNode()->isNormalFlowOnly())
        m_stackingNode->dirtyNormalFlowList();
    if (!oldChild->stackingNode()->isNormalFlowOnly() || oldChild->firstChild()) {
        // Dirty the z-order list in which we are contained.  When called via the
        // reattachment process in removeOnlyThisLayer, the layer may already be disconnected
        // from the main layer tree, so we need to null-check the
        // |stackingContext| value.
        oldChild->stackingNode()->dirtyStackingContextZOrderLists();
    }

    if (layoutObject()->style()->visibility() != VISIBLE)
        dirtyVisibleContentStatus();

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->m_parent = 0;

    dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

    oldChild->updateDescendantDependentFlags();

    if (oldChild->m_hasVisibleContent || oldChild->m_hasVisibleDescendant)
        dirtyAncestorChainVisibleDescendantStatus();

    if (oldChild->enclosingPaginationLayer())
        oldChild->clearPaginationRecursive();

    return oldChild;
}

void Layer::removeOnlyThisLayer()
{
    if (!m_parent)
        return;

    {
        DisableCompositingQueryAsserts disabler; // We need the current compositing status.
        if (isPaintInvalidationContainer()) {
            // Our children will be reparented and contained by a new paint invalidation container,
            // so need paint invalidation. CompositingUpdate can't see this layer (which has been
            // removed) so won't do this for us.
            setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
        }
    }

    m_clipper.clearClipRectsIncludingDescendants();

    Layer* nextSib = nextSibling();

    // Remove the child reflection layer before moving other child layers.
    // The reflection layer should not be moved to the parent.
    if (m_reflectionInfo)
        removeChild(m_reflectionInfo->reflectionLayer());

    // Now walk our kids and reattach them to our parent.
    Layer* current = m_first;
    while (current) {
        Layer* next = current->nextSibling();
        removeChild(current);
        m_parent->addChild(current, nextSib);

        // FIXME: We should call a specialized version of this function.
        current->updateLayerPositionsAfterLayout();
        current = next;
    }

    // Remove us from the parent.
    m_parent->removeChild(this);
    m_renderer->destroyLayer();
}

void Layer::insertOnlyThisLayer()
{
    if (!m_parent && layoutObject()->parent()) {
        // We need to connect ourselves when our layoutObject() has a parent.
        // Find our enclosingLayer and add ourselves.
        Layer* parentLayer = layoutObject()->parent()->enclosingLayer();
        ASSERT(parentLayer);
        Layer* beforeChild = !parentLayer->reflectionInfo() || parentLayer->reflectionInfo()->reflectionLayer() != this ? layoutObject()->parent()->findNextLayer(parentLayer, layoutObject()) : 0;
        parentLayer->addChild(this, beforeChild);
    }

    // Remove all descendant layers from the hierarchy and add them to the new position.
    for (LayoutObject* curr = layoutObject()->slowFirstChild(); curr; curr = curr->nextSibling())
        curr->moveLayers(m_parent, this);

    // Clear out all the clip rects.
    m_clipper.clearClipRectsIncludingDescendants();
}

// Returns the layer reached on the walk up towards the ancestor.
static inline const Layer* accumulateOffsetTowardsAncestor(const Layer* layer, const Layer* ancestorLayer, LayoutPoint& location)
{
    ASSERT(ancestorLayer != layer);

    const LayoutBoxModelObject* renderer = layer->layoutObject();
    EPosition position = renderer->style()->position();

    // FIXME: Positioning of out-of-flow(fixed, absolute) elements collected in a LayoutFlowThread
    // may need to be revisited in a future patch.
    // If the fixed renderer is inside a LayoutFlowThread, we should not compute location using localToAbsolute,
    // since localToAbsolute maps the coordinates from flow thread to regions coordinates and regions can be
    // positioned in a completely different place in the viewport (LayoutView).
    if (position == FixedPosition && (!ancestorLayer || ancestorLayer == renderer->view()->layer())) {
        // If the fixed layer's container is the root, just add in the offset of the view. We can obtain this by calling
        // localToAbsolute() on the LayoutView.
        FloatPoint absPos = renderer->localToAbsolute(FloatPoint(), IsFixed);
        location += LayoutSize(absPos.x(), absPos.y());
        return ancestorLayer;
    }

    // For the fixed positioned elements inside a render flow thread, we should also skip the code path below
    // Otherwise, for the case of ancestorLayer == rootLayer and fixed positioned element child of a transformed
    // element in render flow thread, we will hit the fixed positioned container before hitting the ancestor layer.
    if (position == FixedPosition) {
        // For a fixed layers, we need to walk up to the root to see if there's a fixed position container
        // (e.g. a transformed layer). It's an error to call convertToLayerCoords() across a layer with a transform,
        // so we should always find the ancestor at or before we find the fixed position container.
        Layer* fixedPositionContainerLayer = 0;
        bool foundAncestor = false;
        for (Layer* currLayer = layer->parent(); currLayer; currLayer = currLayer->parent()) {
            if (currLayer == ancestorLayer)
                foundAncestor = true;

            if (isFixedPositionedContainer(currLayer)) {
                fixedPositionContainerLayer = currLayer;
                ASSERT_UNUSED(foundAncestor, foundAncestor);
                break;
            }
        }

        ASSERT(fixedPositionContainerLayer); // We should have hit the LayoutView's layer at least.

        if (fixedPositionContainerLayer != ancestorLayer) {
            LayoutPoint fixedContainerCoords;
            layer->convertToLayerCoords(fixedPositionContainerLayer, fixedContainerCoords);

            LayoutPoint ancestorCoords;
            ancestorLayer->convertToLayerCoords(fixedPositionContainerLayer, ancestorCoords);

            location += (fixedContainerCoords - ancestorCoords);
        } else {
            // LayoutView has been handled in the first top-level 'if' block above.
            ASSERT(ancestorLayer != renderer->view()->layer());
            ASSERT(ancestorLayer->hasTransformRelatedProperty());

            location += layer->location();
        }
        return ancestorLayer;
    }

    Layer* parentLayer;
    if (position == AbsolutePosition) {
        // Do what enclosingPositionedAncestor() does, but check for ancestorLayer along the way.
        parentLayer = layer->parent();
        bool foundAncestorFirst = false;
        while (parentLayer) {
            // LayoutFlowThread is a positioned container, child of LayoutView, positioned at (0,0).
            // This implies that, for out-of-flow positioned elements inside a LayoutFlowThread,
            // we are bailing out before reaching root layer.
            if (parentLayer->isPositionedContainer())
                break;

            if (parentLayer == ancestorLayer) {
                foundAncestorFirst = true;
                break;
            }

            parentLayer = parentLayer->parent();
        }

        // We should not reach LayoutView layer past the LayoutFlowThread layer for any
        // children of the LayoutFlowThread.
        ASSERT(!renderer->flowThreadContainingBlock() || parentLayer != renderer->view()->layer());

        if (foundAncestorFirst) {
            // Found ancestorLayer before the abs. positioned container, so compute offset of both relative
            // to enclosingPositionedAncestor and subtract.
            Layer* positionedAncestor = parentLayer->enclosingPositionedAncestor();

            LayoutPoint thisCoords;
            layer->convertToLayerCoords(positionedAncestor, thisCoords);

            LayoutPoint ancestorCoords;
            ancestorLayer->convertToLayerCoords(positionedAncestor, ancestorCoords);

            location += (thisCoords - ancestorCoords);
            return ancestorLayer;
        }
    } else if (renderer->isColumnSpanAll()) {
        LayoutBlock* multicolContainer = renderer->containingBlock();
        ASSERT(toLayoutBlockFlow(multicolContainer)->multiColumnFlowThread());
        parentLayer = multicolContainer->layer();
        ASSERT(parentLayer);
    } else {
        parentLayer = layer->parent();
    }

    if (!parentLayer)
        return 0;

    location += layer->location();
    return parentLayer;
}

void Layer::convertToLayerCoords(const Layer* ancestorLayer, LayoutPoint& location) const
{
    if (ancestorLayer == this)
        return;

    const Layer* currLayer = this;
    while (currLayer && currLayer != ancestorLayer)
        currLayer = accumulateOffsetTowardsAncestor(currLayer, ancestorLayer, location);
}

void Layer::convertToLayerCoords(const Layer* ancestorLayer, LayoutRect& rect) const
{
    LayoutPoint delta;
    convertToLayerCoords(ancestorLayer, delta);
    rect.moveBy(delta);
}

LayoutPoint Layer::visualOffsetFromAncestor(const Layer* ancestorLayer) const
{
    LayoutPoint offset;
    if (ancestorLayer == this)
        return offset;
    Layer* paginationLayer = enclosingPaginationLayer();
    if (paginationLayer == this)
        paginationLayer = parent()->enclosingPaginationLayer();
    if (!paginationLayer) {
        convertToLayerCoords(ancestorLayer, offset);
        return offset;
    }

    LayoutFlowThread* flowThread = toLayoutFlowThread(paginationLayer->layoutObject());
    convertToLayerCoords(paginationLayer, offset);
    offset = flowThread->flowThreadPointToVisualPoint(offset);
    if (ancestorLayer == paginationLayer)
        return offset;

    if (ancestorLayer->enclosingPaginationLayer() != paginationLayer) {
        offset.moveBy(paginationLayer->visualOffsetFromAncestor(ancestorLayer));
    } else {
        // The ancestor layer is also inside the pagination layer, so we need to subtract the visual
        // distance from the ancestor layer to the pagination layer.
        offset.moveBy(-ancestorLayer->visualOffsetFromAncestor(paginationLayer));
    }
    return offset;
}

void Layer::didUpdateNeedsCompositedScrolling()
{
    updateSelfPaintingLayer();
}

void Layer::updateReflectionInfo(const LayoutStyle* oldStyle)
{
    ASSERT(!oldStyle || !layoutObject()->style()->reflectionDataEquivalent(oldStyle));
    if (layoutObject()->hasReflection()) {
        if (!m_reflectionInfo)
            m_reflectionInfo = adoptPtr(new LayerReflectionInfo(*layoutBox()));
        m_reflectionInfo->updateAfterStyleChange(oldStyle);
    } else if (m_reflectionInfo) {
        m_reflectionInfo->destroy();
        m_reflectionInfo = nullptr;
    }
}

void Layer::updateStackingNode()
{
    if (requiresStackingNode())
        m_stackingNode = adoptPtr(new LayerStackingNode(this));
    else
        m_stackingNode = nullptr;
}

void Layer::updateScrollableArea()
{
    if (requiresScrollableArea())
        m_scrollableArea = adoptPtr(new LayerScrollableArea(*this));
    else
        m_scrollableArea = nullptr;
}

bool Layer::hasOverflowControls() const
{
    return m_scrollableArea && (m_scrollableArea->hasScrollbar() || m_scrollableArea->scrollCorner() || layoutObject()->style()->resize() != RESIZE_NONE);
}

void Layer::appendSingleFragmentIgnoringPagination(LayerFragments& fragments, const Layer* rootLayer, const LayoutRect& dirtyRect, ClipRectsCacheSlot clipRectsCacheSlot, OverlayScrollbarSizeRelevancy inOverlayScrollbarSizeRelevancy, ShouldRespectOverflowClip respectOverflowClip, const LayoutPoint* offsetFromRoot, const LayoutSize& subPixelAccumulation)
{
    LayerFragment fragment;
    ClipRectsContext clipRectsContext(rootLayer, clipRectsCacheSlot, inOverlayScrollbarSizeRelevancy, subPixelAccumulation);
    if (respectOverflowClip == IgnoreOverflowClip)
        clipRectsContext.setIgnoreOverflowClip();
    clipper().calculateRects(clipRectsContext, dirtyRect, fragment.layerBounds, fragment.backgroundRect, fragment.foregroundRect, fragment.outlineRect, offsetFromRoot);
    fragments.append(fragment);
}

void Layer::collectFragments(LayerFragments& fragments, const Layer* rootLayer, const LayoutRect& dirtyRect,
    ClipRectsCacheSlot clipRectsCacheSlot, OverlayScrollbarSizeRelevancy inOverlayScrollbarSizeRelevancy, ShouldRespectOverflowClip respectOverflowClip, const LayoutPoint* offsetFromRoot,
    const LayoutSize& subPixelAccumulation, const LayoutRect* layerBoundingBox)
{
    if (!enclosingPaginationLayer()) {
        // For unpaginated layers, there is only one fragment.
        appendSingleFragmentIgnoringPagination(fragments, rootLayer, dirtyRect, clipRectsCacheSlot, inOverlayScrollbarSizeRelevancy, respectOverflowClip, offsetFromRoot, subPixelAccumulation);
        return;
    }

    // Compute our offset within the enclosing pagination layer.
    LayoutPoint offsetWithinPaginatedLayer;
    convertToLayerCoords(enclosingPaginationLayer(), offsetWithinPaginatedLayer);

    // Calculate clip rects relative to the enclosingPaginationLayer. The purpose of this call is to determine our bounds clipped to intermediate
    // layers between us and the pagination context. It's important to minimize the number of fragments we need to create and this helps with that.
    ClipRectsContext paginationClipRectsContext(enclosingPaginationLayer(), clipRectsCacheSlot, inOverlayScrollbarSizeRelevancy);
    if (respectOverflowClip == IgnoreOverflowClip)
        paginationClipRectsContext.setIgnoreOverflowClip();
    LayoutRect layerBoundsInFlowThread;
    ClipRect backgroundRectInFlowThread;
    ClipRect foregroundRectInFlowThread;
    ClipRect outlineRectInFlowThread;
    clipper().calculateRects(paginationClipRectsContext, LayoutRect(LayoutRect::infiniteIntRect()), layerBoundsInFlowThread, backgroundRectInFlowThread, foregroundRectInFlowThread,
        outlineRectInFlowThread, &offsetWithinPaginatedLayer);

    // Take our bounding box within the flow thread and clip it.
    LayoutRect layerBoundingBoxInFlowThread = layerBoundingBox ? *layerBoundingBox : physicalBoundingBox(enclosingPaginationLayer(), &offsetWithinPaginatedLayer);
    layerBoundingBoxInFlowThread.intersect(backgroundRectInFlowThread.rect());

    // Make the dirty rect relative to the fragmentation context (multicol container, etc.).
    LayoutFlowThread* enclosingFlowThread = toLayoutFlowThread(enclosingPaginationLayer()->layoutObject());
    LayoutPoint offsetOfPaginationLayerFromRoot; // Visual offset from the root layer to the nearest fragmentation context.
    bool rootLayerIsInsidePaginationLayer = rootLayer->enclosingPaginationLayer() == enclosingPaginationLayer();
    if (rootLayerIsInsidePaginationLayer) {
        // The root layer is in the same fragmentation context as this layer, so we need to look
        // inside it and subtract the offset between the fragmentation context and the root layer.
        offsetOfPaginationLayerFromRoot = -rootLayer->visualOffsetFromAncestor(enclosingPaginationLayer());
    } else {
        offsetOfPaginationLayerFromRoot = enclosingPaginationLayer()->visualOffsetFromAncestor(rootLayer);
    }
    LayoutRect dirtyRectInFlowThread(dirtyRect);
    dirtyRectInFlowThread.moveBy(-offsetOfPaginationLayerFromRoot);

    // Tell the flow thread to collect the fragments. We pass enough information to create a minimal number of fragments based off the pages/columns
    // that intersect the actual dirtyRect as well as the pages/columns that intersect our layer's bounding box.
    enclosingFlowThread->collectLayerFragments(fragments, layerBoundingBoxInFlowThread, dirtyRectInFlowThread);

    if (fragments.isEmpty())
        return;

    // Get the parent clip rects of the pagination layer, since we need to intersect with that when painting column contents.
    ClipRect ancestorClipRect = dirtyRect;
    if (const Layer* paginationParentLayer = enclosingPaginationLayer()->parent()) {
        const Layer* ancestorLayer = rootLayerIsInsidePaginationLayer ? paginationParentLayer : rootLayer;
        ClipRectsContext clipRectsContext(ancestorLayer, clipRectsCacheSlot, inOverlayScrollbarSizeRelevancy);
        if (respectOverflowClip == IgnoreOverflowClip)
            clipRectsContext.setIgnoreOverflowClip();
        ancestorClipRect = enclosingPaginationLayer()->clipper().backgroundClipRect(clipRectsContext);
        if (rootLayerIsInsidePaginationLayer)
            ancestorClipRect.moveBy(-rootLayer->visualOffsetFromAncestor(ancestorLayer));
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

static inline LayoutRect frameVisibleRect(LayoutObject* renderer)
{
    FrameView* frameView = renderer->document().view();
    if (!frameView)
        return LayoutRect();

    return LayoutRect(frameView->visibleContentRect());
}

bool Layer::hitTest(const HitTestRequest& request, HitTestResult& result)
{
    return hitTest(request, result.hitTestLocation(), result);
}

bool Layer::hitTest(const HitTestRequest& request, const HitTestLocation& hitTestLocation, HitTestResult& result)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    // LayoutView should make sure to update layout before entering hit testing
    ASSERT(!layoutObject()->frame()->view()->layoutPending());
    ASSERT(!layoutObject()->document().layoutView()->needsLayout());

    // Start with frameVisibleRect to ensure we include the scrollbars.
    LayoutRect hitTestArea = frameVisibleRect(layoutObject());
    if (request.ignoreClipping())
        hitTestArea.unite(LayoutRect(layoutObject()->view()->documentRect()));

    Layer* insideLayer = hitTestLayer(this, 0, request, result, hitTestArea, hitTestLocation, false);
    if (!insideLayer) {
        // We didn't hit any layer. If we are the root layer and the mouse is -- or just was -- down,
        // return ourselves. We do this so mouse events continue getting delivered after a drag has
        // exited the WebView, and so hit testing over a scrollbar hits the content document.
        // In addtion, it is possible for the mouse to stay in the document but there is no element.
        // At that time, the events of the mouse should be fired.
        LayoutPoint hitPoint = hitTestLocation.point();
        if (!request.isChildFrameHitTest() && ((request.active() || request.release()) || (request.move() && hitTestArea.contains(hitPoint.x(), hitPoint.y()))) && isRootLayer()) {
            layoutObject()->updateHitTestResult(result, toLayoutView(layoutObject())->flipForWritingMode(hitTestLocation.point()));
            insideLayer = this;
        }
    }

    // Now determine if the result is inside an anchor - if the urlElement isn't already set.
    Node* node = result.innerNode();
    if (node && !result.URLElement())
        result.setURLElement(node->enclosingLinkEventParentOrSelf());

    // Now return whether we were inside this layer (this will always be true for the root
    // layer).
    return insideLayer;
}

Node* Layer::enclosingElement() const
{
    for (LayoutObject* r = layoutObject(); r; r = r->parent()) {
        if (Node* e = r->node())
            return e;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

bool Layer::isInTopLayer() const
{
    Node* node = layoutObject()->node();
    return node && node->isElementNode() && toElement(node)->isInTopLayer();
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

PassRefPtr<HitTestingTransformState> Layer::createLocalTransformState(Layer* rootLayer, Layer* containerLayer,
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

    LayoutObject* containerRenderer = containerLayer ? containerLayer->layoutObject() : 0;
    if (layoutObject()->shouldUseTransformFromContainer(containerRenderer)) {
        TransformationMatrix containerTransform;
        layoutObject()->getTransformFromContainer(containerRenderer, toLayoutSize(offset), containerTransform);
        transformState->applyTransform(containerTransform, HitTestingTransformState::AccumulateTransform);
    } else {
        transformState->translate(offset.x(), offset.y(), HitTestingTransformState::AccumulateTransform);
    }

    return transformState;
}


static bool isHitCandidate(const Layer* hitLayer, bool canDepthSort, double* zOffset, const HitTestingTransformState* transformState)
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
Layer* Layer::hitTestLayer(Layer* rootLayer, Layer* containerLayer, const HitTestRequest& request, HitTestResult& result,
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
            ClipRect clipRect = clipper().backgroundClipRect(ClipRectsContext(rootLayer, RootRelativeClipRects, IncludeOverlayScrollbarSize));
            // Go ahead and test the enclosing clip now.
            if (!clipRect.intersects(hitTestLocation))
                return 0;
        }

        return hitTestLayerByApplyingTransform(rootLayer, containerLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffset);
    }

    // Ensure our lists and 3d status are up-to-date.
    m_stackingNode->updateLayerListsIfNeeded();
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
    if (localTransformState && layoutObject()->style()->backfaceVisibility() == BackfaceVisibilityHidden) {
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
    double localZOffset = -std::numeric_limits<double>::infinity();
    double* zOffsetForDescendantsPtr = 0;
    double* zOffsetForContentsPtr = 0;

    bool depthSortDescendants = false;
    if (preserves3D()) {
        depthSortDescendants = true;
        // Our layers can depth-test with our container, so share the z depth pointer with the container, if it passed one down.
        zOffsetForDescendantsPtr = zOffset ? zOffset : &localZOffset;
        zOffsetForContentsPtr = zOffset ? zOffset : &localZOffset;
    } else if (zOffset) {
        zOffsetForDescendantsPtr = 0;
        // Container needs us to give back a z offset for the hit layer.
        zOffsetForContentsPtr = zOffset;
    }

    // This variable tracks which layer the mouse ends up being inside.
    Layer* candidateLayer = 0;

    // Begin by walking our list of positive layers from highest z-index down to the lowest z-index.
    Layer* hitLayer = hitTestChildren(PositiveZOrderChildren, rootLayer, request, result, hitTestRect, hitTestLocation,
        localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Now check our overflow objects.
    hitLayer = hitTestChildren(NormalFlowChildren, rootLayer, request, result, hitTestRect, hitTestLocation,
        localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Collect the fragments. This will compute the clip rectangles for each layer fragment.
    LayerFragments layerFragments;
    if (appliedTransform)
        appendSingleFragmentIgnoringPagination(layerFragments, rootLayer, hitTestRect, RootRelativeClipRects, IncludeOverlayScrollbarSize);
    else
        collectFragments(layerFragments, rootLayer, hitTestRect, RootRelativeClipRects, IncludeOverlayScrollbarSize);

    if (m_scrollableArea && m_scrollableArea->hitTestResizerInFragments(layerFragments, hitTestLocation)) {
        layoutObject()->updateHitTestResult(result, hitTestLocation.point());
        return this;
    }

    // Next we want to see if the mouse pos is inside the child LayoutObjects of the layer. Check
    // every fragment in reverse order.
    if (isSelfPaintingLayer()) {
        // Hit test with a temporary HitTestResult, because we only want to commit to 'result' if we know we're frontmost.
        HitTestResult tempResult(result.hitTestLocation());
        bool insideFragmentForegroundRect = false;
        if (hitTestContentsForFragments(layerFragments, request, tempResult, hitTestLocation, HitTestDescendants, insideFragmentForegroundRect)
            && isHitCandidate(this, false, zOffsetForContentsPtr, unflattenedTransformState.get())) {
            if (request.listBased())
                result.append(tempResult, request);
            else
                result = tempResult;
            if (!depthSortDescendants)
                return this;
            // Foreground can depth-sort with descendant layers, so keep this as a candidate.
            candidateLayer = this;
        } else if (insideFragmentForegroundRect && request.listBased()) {
            result.append(tempResult, request);
        }
    }

    // Now check our negative z-index children.
    hitLayer = hitTestChildren(NegativeZOrderChildren, rootLayer, request, result, hitTestRect, hitTestLocation,
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
                result.append(tempResult, request);
            else
                result = tempResult;
            return this;
        }
        if (insideFragmentBackgroundRect && request.listBased())
            result.append(tempResult, request);
    }

    return 0;
}

bool Layer::hitTestContentsForFragments(const LayerFragments& layerFragments, const HitTestRequest& request, HitTestResult& result,
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

Layer* Layer::hitTestTransformedLayerInFragments(Layer* rootLayer, Layer* containerLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffset)
{
    LayerFragments enclosingPaginationFragments;
    LayoutPoint offsetOfPaginationLayerFromRoot;
    // FIXME: We're missing a sub-pixel offset here crbug.com/348728
    LayoutRect transformedExtent = transparencyClipBox(this, enclosingPaginationLayer(), HitTestingTransparencyClipBox, Layer::RootOfTransparencyClipBox, LayoutSize());
    enclosingPaginationLayer()->collectFragments(enclosingPaginationFragments, rootLayer, hitTestRect,
        RootRelativeClipRects, IncludeOverlayScrollbarSize, RespectOverflowClip, &offsetOfPaginationLayerFromRoot, LayoutSize(), &transformedExtent);

    for (int i = enclosingPaginationFragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = enclosingPaginationFragments.at(i);

        // Apply the page/column clip for this fragment, as well as any clips established by layers in between us and
        // the enclosing pagination layer.
        LayoutRect clipRect = fragment.backgroundRect.rect();

        // Now compute the clips within a given fragment
        if (parent() != enclosingPaginationLayer()) {
            enclosingPaginationLayer()->convertToLayerCoords(rootLayer, offsetOfPaginationLayerFromRoot);
            LayoutRect parentClipRect = clipper().backgroundClipRect(ClipRectsContext(enclosingPaginationLayer(), RootRelativeClipRects, IncludeOverlayScrollbarSize)).rect();
            parentClipRect.moveBy(fragment.paginationOffset + offsetOfPaginationLayerFromRoot);
            clipRect.intersect(parentClipRect);
        }

        if (!hitTestLocation.intersects(clipRect))
            continue;

        Layer* hitLayer = hitTestLayerByApplyingTransform(rootLayer, containerLayer, request, result, hitTestRect, hitTestLocation,
            transformState, zOffset, fragment.paginationOffset);
        if (hitLayer)
            return hitLayer;
    }

    return 0;
}

Layer* Layer::hitTestLayerByApplyingTransform(Layer* rootLayer, Layer* containerLayer, const HitTestRequest& request, HitTestResult& result,
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

bool Layer::hitTestContents(const HitTestRequest& request, HitTestResult& result, const LayoutRect& layerBounds, const HitTestLocation& hitTestLocation, HitTestFilter hitTestFilter) const
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    if (!layoutObject()->hitTest(request, result, hitTestLocation, toLayoutPoint(layerBounds.location() - layoutBoxLocation()), hitTestFilter)) {
        // It's wrong to set innerNode, but then claim that you didn't hit anything, unless it is
        // a rect-based test.
        ASSERT(!result.innerNode() || (request.listBased() && result.listBasedTestResult().size()));
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

Layer* Layer::hitTestChildren(ChildrenIteration childrentoVisit, Layer* rootLayer,
    const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation,
    const HitTestingTransformState* transformState,
    double* zOffsetForDescendants, double* zOffset,
    const HitTestingTransformState* unflattenedTransformState,
    bool depthSortDescendants)
{
    if (!hasSelfPaintingLayerDescendant())
        return 0;

    Layer* resultLayer = 0;
    LayerStackingNodeReverseIterator iterator(*m_stackingNode, childrentoVisit);
    while (LayerStackingNode* child = iterator.next()) {
        Layer* childLayer = child->layer();
        Layer* hitLayer = 0;
        HitTestResult tempResult(result.hitTestLocation());
        if (childLayer->isPaginated())
            hitLayer = hitTestPaginatedChildLayer(childLayer, rootLayer, request, tempResult, hitTestRect, hitTestLocation, transformState, zOffsetForDescendants);
        else
            hitLayer = childLayer->hitTestLayer(rootLayer, this, request, tempResult, hitTestRect, hitTestLocation, false, transformState, zOffsetForDescendants);

        // If it is a list-based test, we can safely append the temporary result since it might had hit
        // nodes but not necesserily had hitLayer set.
        ASSERT(!result.isRectBasedTest() || request.listBased());
        if (request.listBased())
            result.append(tempResult, request);

        if (isHitCandidate(hitLayer, depthSortDescendants, zOffset, unflattenedTransformState)) {
            resultLayer = hitLayer;
            if (!request.listBased())
                result = tempResult;
            if (!depthSortDescendants)
                break;
        }
    }

    return resultLayer;
}

Layer* Layer::hitTestPaginatedChildLayer(Layer* childLayer, Layer* rootLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffset)
{
    Vector<Layer*> columnLayers;
    LayerStackingNode* ancestorNode = m_stackingNode->isNormalFlowOnly() ? parent()->stackingNode() : m_stackingNode->ancestorStackingContextNode();
    for (Layer* curr = childLayer->parent(); curr; curr = curr->parent()) {
        if (curr->layoutObject()->hasColumns() && checkContainingBlockChainForPagination(childLayer->layoutObject(), curr->layoutBox()))
            columnLayers.append(curr);
        if (curr->stackingNode() == ancestorNode)
            break;
    }

    ASSERT(columnLayers.size());
    return hitTestChildLayerColumns(childLayer, rootLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffset,
        columnLayers, columnLayers.size() - 1);
}

Layer* Layer::hitTestChildLayerColumns(Layer* childLayer, Layer* rootLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffset,
    const Vector<Layer*>& columnLayers, size_t columnIndex)
{
    LayoutBlock* columnBlock = toLayoutBlock(columnLayers[columnIndex]->layoutObject());

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
            Layer* hitLayer = 0;
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
                Layer* nextLayer = columnLayers[columnIndex - 1];
                RefPtr<HitTestingTransformState> newTransformState = nextLayer->createLocalTransformState(rootLayer, nextLayer, localClipRect, hitTestLocation, transformState);
                newTransformState->translate(offset.width(), offset.height(), HitTestingTransformState::AccumulateTransform);
                FloatPoint localPoint = newTransformState->mappedPoint();
                FloatQuad localPointQuad = newTransformState->mappedQuad();
                LayoutRect localHitTestRect(newTransformState->mappedArea().enclosingBoundingBox());
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

void Layer::blockSelectionGapsBoundsChanged()
{
    setNeedsCompositingInputsUpdate();
}

void Layer::addBlockSelectionGapsBounds(const LayoutRect& bounds)
{
    m_blockSelectionGapsBounds.unite(enclosingIntRect(bounds));
    blockSelectionGapsBoundsChanged();
}

void Layer::clearBlockSelectionGapsBounds()
{
    m_blockSelectionGapsBounds = IntRect();
    for (Layer* child = firstChild(); child; child = child->nextSibling())
        child->clearBlockSelectionGapsBounds();
    blockSelectionGapsBoundsChanged();
}

void Layer::invalidatePaintForBlockSelectionGaps()
{
    for (Layer* child = firstChild(); child; child = child->nextSibling()) {
        // FIXME: We should not allow paint invalidation out of paint invalidation state. crbug.com/457415
        DisablePaintInvalidationStateAsserts disabler;
        child->invalidatePaintForBlockSelectionGaps();
    }

    if (m_blockSelectionGapsBounds.isEmpty())
        return;

    LayoutRect rect(m_blockSelectionGapsBounds);
    if (layoutObject()->hasOverflowClip()) {
        LayoutBox* box = layoutBox();
        rect.move(-box->scrolledContentOffset());
        if (!scrollableArea()->usesCompositedScrolling())
            rect.intersect(box->overflowClipRect(LayoutPoint()));
    }
    if (layoutObject()->hasClip())
        rect.intersect(toLayoutBox(layoutObject())->clipRect(LayoutPoint()));
    if (!rect.isEmpty()) {
        // FIXME: We should not allow paint invalidation out of paint invalidation state. crbug.com/457415
        DisablePaintInvalidationStateAsserts disabler;
        layoutObject()->invalidatePaintRectangle(rect);
    }
}

IntRect Layer::blockSelectionGapsBounds() const
{
    if (!layoutObject()->isLayoutBlockFlow())
        return IntRect();

    LayoutBlockFlow* layoutBlockFlow = toLayoutBlockFlow(layoutObject());
    LayoutRect gapRects = layoutBlockFlow->selectionGapRectsForPaintInvalidation(layoutBlockFlow);

    return pixelSnappedIntRect(gapRects);
}

bool Layer::hasBlockSelectionGapBounds() const
{
    // FIXME: it would be more accurate to return !blockSelectionGapsBounds().isEmpty(), but this is impossible
    // at the moment because it causes invalid queries to layout-dependent code (crbug.com/372802).
    // ASSERT(layoutObject()->document().lifecycle().state() >= DocumentLifecycle::LayoutClean);

    if (!layoutObject()->isLayoutBlock())
        return false;

    return toLayoutBlock(layoutObject())->shouldPaintSelectionGaps();
}

bool Layer::intersectsDamageRect(const LayoutRect& layerBounds, const LayoutRect& damageRect, const Layer* rootLayer, const LayoutPoint* offsetFromRoot) const
{
    // Always examine the canvas and the root.
    // FIXME: Could eliminate the isDocumentElement() check if we fix background painting so that the LayoutView
    // paints the root's background.
    if (isRootLayer() || layoutObject()->isDocumentElement())
        return true;

    // If we aren't an inline flow, and our layer bounds do intersect the damage rect, then we
    // can go ahead and return true.
    LayoutView* view = layoutObject()->view();
    ASSERT(view);
    if (view && !layoutObject()->isLayoutInline()) {
        if (layerBounds.intersects(damageRect))
            return true;
    }

    // Otherwise we need to compute the bounding box of this single layer and see if it intersects
    // the damage rect.
    return physicalBoundingBox(rootLayer, offsetFromRoot).intersects(damageRect);
}

LayoutRect Layer::logicalBoundingBox() const
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
    if (layoutObject()->isInline() && layoutObject()->isLayoutInline()) {
        result = toLayoutInline(layoutObject())->linesVisualOverflowBoundingBox();
    } else if (layoutObject()->isTableRow()) {
        // Our bounding box is just the union of all of our cells' border/overflow rects.
        for (LayoutObject* child = layoutObject()->slowFirstChild(); child; child = child->nextSibling()) {
            if (child->isTableCell()) {
                LayoutRect bbox = toLayoutBox(child)->borderBoxRect();
                result.unite(bbox);
                LayoutRect overflowRect = layoutBox()->visualOverflowRect();
                if (bbox != overflowRect)
                    result.unite(overflowRect);
            }
        }
    } else {
        LayoutBox* box = layoutBox();
        ASSERT(box);
        result = box->borderBoxRect();
        result.unite(box->visualOverflowRect());
    }

    ASSERT(layoutObject()->view());
    return result;
}

static inline LayoutRect flippedLogicalBoundingBox(LayoutRect boundingBox, LayoutObject* renderer)
{
    LayoutRect result = boundingBox;
    if (renderer->isBox())
        toLayoutBox(renderer)->flipForWritingMode(result);
    else
        renderer->containingBlock()->flipForWritingMode(result);
    return result;
}

LayoutRect Layer::physicalBoundingBox(const Layer* ancestorLayer, const LayoutPoint* offsetFromRoot) const
{
    LayoutRect result = flippedLogicalBoundingBox(logicalBoundingBox(), layoutObject());
    if (offsetFromRoot)
        result.moveBy(*offsetFromRoot);
    else
        convertToLayerCoords(ancestorLayer, result);
    return result;
}

LayoutRect Layer::fragmentsBoundingBox(const Layer* ancestorLayer) const
{
    if (!enclosingPaginationLayer())
        return physicalBoundingBox(ancestorLayer);

    LayoutRect result = flippedLogicalBoundingBox(logicalBoundingBox(), layoutObject());
    convertFromFlowThreadToVisualBoundingBoxInAncestor(this, ancestorLayer, result);
    return result;
}

LayoutRect Layer::boundingBoxForCompositingOverlapTest() const
{
    return overlapBoundsIncludeChildren() ? boundingBoxForCompositing() : fragmentsBoundingBox(this);
}

static void expandRectForReflectionAndStackingChildren(const Layer* ancestorLayer, Layer::CalculateBoundsOptions options, LayoutRect& result)
{
    if (ancestorLayer->reflectionInfo() && !ancestorLayer->reflectionInfo()->reflectionLayer()->hasCompositedLayerMapping())
        result.unite(ancestorLayer->reflectionInfo()->reflectionLayer()->boundingBoxForCompositing(ancestorLayer));

    ASSERT(ancestorLayer->stackingNode()->isStackingContext() || !ancestorLayer->stackingNode()->hasPositiveZOrderList());

#if ENABLE(ASSERT)
    LayerListMutationDetector mutationChecker(const_cast<Layer*>(ancestorLayer)->stackingNode());
#endif

    LayerStackingNodeIterator iterator(*ancestorLayer->stackingNode(), AllChildren);
    while (LayerStackingNode* node = iterator.next()) {
        // Here we exclude both directly composited layers and squashing layers
        // because those Layers don't paint into the graphics layer
        // for this Layer. For example, the bounds of squashed Layers
        // will be included in the computation of the appropriate squashing
        // GraphicsLayer.
        if (options != Layer::ApplyBoundsChickenEggHacks && node->layer()->compositingState() != NotComposited)
            continue;
        result.unite(node->layer()->boundingBoxForCompositing(ancestorLayer, options));
    }
}

LayoutRect Layer::physicalBoundingBoxIncludingReflectionAndStackingChildren(const Layer* ancestorLayer, const LayoutPoint& offsetFromRoot) const
{
    LayoutPoint origin;
    LayoutRect result = physicalBoundingBox(ancestorLayer, &origin);

    const_cast<Layer*>(this)->stackingNode()->updateLayerListsIfNeeded();

    expandRectForReflectionAndStackingChildren(this, DoNotApplyBoundsChickenEggHacks, result);

    result.moveBy(offsetFromRoot);
    return result;
}

LayoutRect Layer::boundingBoxForCompositing(const Layer* ancestorLayer, CalculateBoundsOptions options) const
{
    if (!isSelfPaintingLayer())
        return LayoutRect();

    if (!ancestorLayer)
        ancestorLayer = this;

    // FIXME: This could be improved to do a check like hasVisibleNonCompositingDescendantLayers() (bug 92580).
    if (this != ancestorLayer && !hasVisibleContent() && !hasVisibleDescendant())
        return LayoutRect();

    // The root layer is always just the size of the document.
    if (isRootLayer())
        return LayoutRect(m_renderer->view()->unscaledDocumentRect());

    // The layer created for the LayoutFlowThread is just a helper for painting and hit-testing,
    // and should not contribute to the bounding box. The LayoutMultiColumnSets will contribute
    // the correct size for the rendered content of the multicol container.
    if (useRegionBasedColumns() && layoutObject()->isLayoutFlowThread())
        return LayoutRect();

    LayoutRect result = clipper().localClipRect();
    if (result == LayoutRect::infiniteIntRect()) {
        LayoutPoint origin;
        result = physicalBoundingBox(ancestorLayer, &origin);

        const_cast<Layer*>(this)->stackingNode()->updateLayerListsIfNeeded();

        // Reflections are implemented with Layers that hang off of the reflected layer. However,
        // the reflection layer subtree does not include the subtree of the parent Layer, so
        // a recursive computation of stacking children yields no results. This breaks cases when there are stacking
        // children of the parent, that need to be included in reflected composited bounds.
        // Fix this by including composited bounds of stacking children of the reflected Layer.
        if (hasCompositedLayerMapping() && parent() && parent()->reflectionInfo() && parent()->reflectionInfo()->reflectionLayer() == this)
            expandRectForReflectionAndStackingChildren(parent(), options, result);
        else
            expandRectForReflectionAndStackingChildren(this, options, result);

        // FIXME: We can optimize the size of the composited layers, by not enlarging
        // filtered areas with the outsets if we know that the filter is going to render in hardware.
        // https://bugs.webkit.org/show_bug.cgi?id=81239
        result.expand(m_renderer->style()->filterOutsets());
    }

    if (paintsWithTransform(PaintBehaviorNormal) || (options == ApplyBoundsChickenEggHacks && transform()))
        result = transform()->mapRect(result);

    if (enclosingPaginationLayer()) {
        convertFromFlowThreadToVisualBoundingBoxInAncestor(this, ancestorLayer, result);
        return result;
    }
    LayoutPoint delta;
    convertToLayerCoords(ancestorLayer, delta);
    result.moveBy(delta);
    return result;
}

CompositingState Layer::compositingState() const
{
    ASSERT(isAllowedToQueryCompositingState());

    // This is computed procedurally so there is no redundant state variable that
    // can get out of sync from the real actual compositing state.

    if (m_groupedMapping) {
        ASSERT(!m_compositedLayerMapping);
        return PaintsIntoGroupedBacking;
    }

    if (!m_compositedLayerMapping)
        return NotComposited;

    return PaintsIntoOwnBacking;
}

bool Layer::isAllowedToQueryCompositingState() const
{
    if (gCompositingQueryMode == CompositingQueriesAreAllowed)
        return true;
    return layoutObject()->document().lifecycle().state() >= DocumentLifecycle::InCompositingUpdate;
}

CompositedLayerMapping* Layer::compositedLayerMapping() const
{
    ASSERT(isAllowedToQueryCompositingState());
    return m_compositedLayerMapping.get();
}

GraphicsLayer* Layer::graphicsLayerBacking() const
{
    switch (compositingState()) {
    case NotComposited:
        return 0;
    case PaintsIntoGroupedBacking:
        return groupedMapping()->squashingLayer();
    default:
        return compositedLayerMapping()->mainGraphicsLayer();
    }
}

GraphicsLayer* Layer::graphicsLayerBackingForScrolling() const
{
    switch (compositingState()) {
    case NotComposited:
        return 0;
    case PaintsIntoGroupedBacking:
        return groupedMapping()->squashingLayer();
    default:
        return compositedLayerMapping()->scrollingContentsLayer() ? compositedLayerMapping()->scrollingContentsLayer() : compositedLayerMapping()->mainGraphicsLayer();
    }
}

void Layer::ensureCompositedLayerMapping()
{
    if (m_compositedLayerMapping)
        return;

    m_compositedLayerMapping = adoptPtr(new CompositedLayerMapping(*this));
    m_compositedLayerMapping->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateSubtree);

    updateOrRemoveFilterEffectRenderer();
}

void Layer::clearCompositedLayerMapping(bool layerBeingDestroyed)
{
    if (!layerBeingDestroyed) {
        // We need to make sure our decendants get a geometry update. In principle,
        // we could call setNeedsGraphicsLayerUpdate on our children, but that would
        // require walking the z-order lists to find them. Instead, we over-invalidate
        // by marking our parent as needing a geometry update.
        if (Layer* compositingParent = enclosingLayerWithCompositedLayerMapping(ExcludeSelf))
            compositingParent->compositedLayerMapping()->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateSubtree);
    }

    m_compositedLayerMapping.clear();

    if (!layerBeingDestroyed)
        updateOrRemoveFilterEffectRenderer();
}

void Layer::setGroupedMapping(CompositedLayerMapping* groupedMapping, bool layerBeingDestroyed)
{
    if (groupedMapping == m_groupedMapping)
        return;

    if (!layerBeingDestroyed && m_groupedMapping) {
        m_groupedMapping->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateSubtree);
        m_groupedMapping->removeLayerFromSquashingGraphicsLayer(this);
    }
    m_groupedMapping = groupedMapping;
    if (!layerBeingDestroyed && m_groupedMapping)
        m_groupedMapping->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateSubtree);
}

bool Layer::hasCompositedMask() const
{
    return m_compositedLayerMapping && m_compositedLayerMapping->hasMaskLayer();
}

bool Layer::hasCompositedClippingMask() const
{
    return m_compositedLayerMapping && m_compositedLayerMapping->hasChildClippingMaskLayer();
}

bool Layer::paintsWithTransform(PaintBehavior paintBehavior) const
{
    return transform() && ((paintBehavior & PaintBehaviorFlattenCompositingLayers) || compositingState() != PaintsIntoOwnBacking);
}

bool Layer::backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const
{
    if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
        return false;

    if (paintsWithTransparency(PaintBehaviorNormal))
        return false;

    // We can't use hasVisibleContent(), because that will be true if our renderer is hidden, but some child
    // is visible and that child doesn't cover the entire rect.
    if (layoutObject()->style()->visibility() != VISIBLE)
        return false;

    if (paintsWithFilters() && layoutObject()->style()->filter().hasFilterThatAffectsOpacity())
        return false;

    // FIXME: Handle simple transforms.
    if (paintsWithTransform(PaintBehaviorNormal))
        return false;

    // FIXME: Remove this check.
    // This function should not be called when layer-lists are dirty.
    // It is somehow getting triggered during style update.
    if (m_stackingNode->zOrderListsDirty() || m_stackingNode->normalFlowListDirty())
        return false;

    // FIXME: We currently only check the immediate renderer,
    // which will miss many cases.
    if (layoutObject()->backgroundIsKnownToBeOpaqueInRect(localRect))
        return true;

    // We can't consult child layers if we clip, since they might cover
    // parts of the rect that are clipped out.
    if (layoutObject()->hasOverflowClip())
        return false;

    return childBackgroundIsKnownToBeOpaqueInRect(localRect);
}

bool Layer::childBackgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const
{
    LayerStackingNodeReverseIterator revertseIterator(*m_stackingNode, PositiveZOrderChildren | NormalFlowChildren | NegativeZOrderChildren);
    while (LayerStackingNode* child = revertseIterator.next()) {
        const Layer* childLayer = child->layer();
        // Stop at composited paint boundaries.
        if (childLayer->isPaintInvalidationContainer())
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

bool Layer::shouldBeSelfPaintingLayer() const
{
    if (layoutObject()->isLayoutPart() && toLayoutPart(layoutObject())->requiresAcceleratedCompositing())
        return true;
    return m_layerType == NormalLayer
        || (m_scrollableArea && m_scrollableArea->hasOverlayScrollbars())
        || needsCompositedScrolling();
}

void Layer::updateSelfPaintingLayer()
{
    bool isSelfPaintingLayer = shouldBeSelfPaintingLayer();
    if (this->isSelfPaintingLayer() == isSelfPaintingLayer)
        return;

    m_isSelfPaintingLayer = isSelfPaintingLayer;

    if (parent())
        parent()->dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();
}

bool Layer::hasNonEmptyChildRenderers() const
{
    // Some HTML can cause whitespace text nodes to have renderers, like:
    // <div>
    // <img src=...>
    // </div>
    // so test for 0x0 RenderTexts here
    for (LayoutObject* child = layoutObject()->slowFirstChild(); child; child = child->nextSibling()) {
        if (!child->hasLayer()) {
            if (child->isLayoutInline() || !child->isBox())
                return true;

            if (toLayoutBox(child)->size().width() > 0 || toLayoutBox(child)->size().height() > 0)
                return true;
        }
    }
    return false;
}

bool Layer::hasBoxDecorationsOrBackground() const
{
    return layoutObject()->style()->hasBoxDecorations() || layoutObject()->style()->hasBackground();
}

bool Layer::hasVisibleBoxDecorations() const
{
    if (!hasVisibleContent())
        return false;

    return hasBoxDecorationsOrBackground() || hasOverflowControls();
}

void Layer::updateFilters(const LayoutStyle* oldStyle, const LayoutStyle& newStyle)
{
    if (!newStyle.hasFilter() && (!oldStyle || !oldStyle->hasFilter()))
        return;

    updateOrRemoveFilterClients();
    updateOrRemoveFilterEffectRenderer();
}

bool Layer::attemptDirectCompositingUpdate(StyleDifference diff, const LayoutStyle* oldStyle)
{
    CompositingReasons oldPotentialCompositingReasonsFromStyle = m_potentialCompositingReasonsFromStyle;
    compositor()->updatePotentialCompositingReasonsFromStyle(this);

    // This function implements an optimization for transforms and opacity.
    // A common pattern is for a touchmove handler to update the transform
    // and/or an opacity of an element every frame while the user moves their
    // finger across the screen. The conditions below recognize when the
    // compositing state is set up to receive a direct transform or opacity
    // update.

    if (!diff.hasAtMostPropertySpecificDifferences(StyleDifference::TransformChanged | StyleDifference::OpacityChanged))
        return false;
    // The potentialCompositingReasonsFromStyle could have changed without
    // a corresponding StyleDifference if an animation started or ended.
    if (m_potentialCompositingReasonsFromStyle != oldPotentialCompositingReasonsFromStyle)
        return false;
    // We could add support for reflections if we updated the transform on
    // the reflection layers.
    if (layoutObject()->hasReflection())
        return false;
    // If we're unwinding a scheduleSVGFilterLayerUpdateHack(), then we can't
    // perform a direct compositing update because the filters code is going
    // to produce different output this time around. We can remove this code
    // once we fix the chicken/egg bugs in the filters code and delete the
    // scheduleSVGFilterLayerUpdateHack().
    if (layoutObject()->node() && layoutObject()->node()->svgFilterNeedsLayerUpdate())
        return false;
    if (!m_compositedLayerMapping)
        return false;

    // To cut off almost all the work in the compositing update for
    // this case, we treat inline transforms has having assumed overlap
    // (similar to how we treat animated transforms). Notice that we read
    // CompositingReasonInlineTransform from the m_compositingReasons, which
    // means that the inline transform actually triggered assumed overlap in
    // the overlap map.
    if (diff.transformChanged() && !(m_compositingReasons & CompositingReasonInlineTransform))
        return false;

    // We composite transparent Layers differently from non-transparent
    // Layers even when the non-transparent Layers are already a
    // stacking context.
    if (diff.opacityChanged() && m_renderer->style()->hasOpacity() != oldStyle->hasOpacity())
        return false;

    updateTransform(oldStyle, layoutObject()->styleRef());

    // FIXME: Consider introducing a smaller graphics layer update scope
    // that just handles transforms and opacity. GraphicsLayerUpdateLocal
    // will also program bounds, clips, and many other properties that could
    // not possibly have changed.
    m_compositedLayerMapping->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateLocal);
    compositor()->setNeedsCompositingUpdate(CompositingUpdateAfterGeometryChange);
    return true;
}

void Layer::styleChanged(StyleDifference diff, const LayoutStyle* oldStyle)
{
    if (attemptDirectCompositingUpdate(diff, oldStyle))
        return;

    m_stackingNode->updateIsNormalFlowOnly();
    m_stackingNode->updateStackingNodesAfterStyleChange(oldStyle);

    if (m_scrollableArea)
        m_scrollableArea->updateAfterStyleChange(oldStyle);

    // Overlay scrollbars can make this layer self-painting so we need
    // to recompute the bit once scrollbars have been updated.
    updateSelfPaintingLayer();

    if (!oldStyle || !layoutObject()->style()->reflectionDataEquivalent(oldStyle)) {
        ASSERT(!oldStyle || diff.needsFullLayout());
        updateReflectionInfo(oldStyle);
    }

    updateDescendantDependentFlags();

    updateTransform(oldStyle, layoutObject()->styleRef());
    updateFilters(oldStyle, layoutObject()->styleRef());

    setNeedsCompositingInputsUpdate();
}

bool Layer::scrollsOverflow() const
{
    if (LayerScrollableArea* scrollableArea = this->scrollableArea())
        return scrollableArea->scrollsOverflow();

    return false;
}

FilterOperations Layer::computeFilterOperations(const LayoutStyle& style)
{
    const FilterOperations& filters = style.filter();
    if (filters.hasReferenceFilter()) {
        for (size_t i = 0; i < filters.size(); ++i) {
            FilterOperation* filterOperation = filters.operations().at(i).get();
            if (filterOperation->type() != FilterOperation::REFERENCE)
                continue;
            ReferenceFilterOperation* referenceOperation = toReferenceFilterOperation(filterOperation);
            // FIXME: Cache the ReferenceFilter if it didn't change.
            RefPtrWillBeRawPtr<ReferenceFilter> referenceFilter = ReferenceFilter::create(style.effectiveZoom());
            referenceFilter->setLastEffect(ReferenceFilterBuilder::build(referenceFilter.get(), layoutObject(), referenceFilter->sourceGraphic(),
                referenceOperation));
            referenceOperation->setFilter(referenceFilter.release());
        }
    }

    return filters;
}

void Layer::updateOrRemoveFilterClients()
{
    if (!hasFilter()) {
        removeFilterInfoIfNeeded();
        return;
    }

    if (layoutObject()->style()->filter().hasReferenceFilter())
        ensureFilterInfo()->updateReferenceFilterClients(layoutObject()->style()->filter());
    else if (hasFilterInfo())
        filterInfo()->removeReferenceFilterClients();
}

void Layer::updateOrRemoveFilterEffectRenderer()
{
    // FilterEffectRenderer is only used to render the filters in software mode,
    // so we always need to run updateOrRemoveFilterEffectRenderer after the composited
    // mode might have changed for this layer.
    if (!paintsWithFilters()) {
        // Don't delete the whole filter info here, because we might use it
        // for loading CSS shader files.
        if (LayerFilterInfo* filterInfo = this->filterInfo())
            filterInfo->setRenderer(nullptr);

        return;
    }

    LayerFilterInfo* filterInfo = ensureFilterInfo();
    if (!filterInfo->layoutObject()) {
        RefPtrWillBeRawPtr<FilterEffectRenderer> filterRenderer = FilterEffectRenderer::create();
        filterInfo->setRenderer(filterRenderer.release());
    }

    // If the filter fails to build, remove it from the layer. It will still attempt to
    // go through regular processing (e.g. compositing), but never apply anything.
    if (!filterInfo->layoutObject()->build(layoutObject(), computeFilterOperations(layoutObject()->styleRef())))
        filterInfo->setRenderer(nullptr);
}

void Layer::filterNeedsPaintInvalidation()
{
    {
        DeprecatedScheduleStyleRecalcDuringLayout marker(layoutObject()->document().lifecycle());
        // It's possible for scheduleSVGFilterLayerUpdateHack to schedule a style recalc, which
        // is a problem because this function can be called while performing layout.
        // Presumably this represents an illegal data flow of layout or compositing
        // information into the style system.
        toElement(layoutObject()->node())->scheduleSVGFilterLayerUpdateHack();
    }

    layoutObject()->setShouldDoFullPaintInvalidation();
}

void Layer::addLayerHitTestRects(LayerHitTestRects& rects) const
{
    computeSelfHitTestRects(rects);
    for (Layer* child = firstChild(); child; child = child->nextSibling())
        child->addLayerHitTestRects(rects);
}

void Layer::computeSelfHitTestRects(LayerHitTestRects& rects) const
{
    if (!size().isEmpty()) {
        Vector<LayoutRect> rect;

        if (layoutBox() && layoutBox()->scrollsOverflow()) {
            // For scrolling layers, rects are taken to be in the space of the contents.
            // We need to include the bounding box of the layer in the space of its parent
            // (eg. for border / scroll bars) and if it's composited then the entire contents
            // as well as they may be on another composited layer. Skip reporting contents
            // for non-composited layers as they'll get projected to the same layer as the
            // bounding box.
            if (compositingState() != NotComposited)
                rect.append(m_scrollableArea->overflowRect());

            rects.set(this, rect);
            if (const Layer* parentLayer = parent()) {
                LayerHitTestRects::iterator iter = rects.find(parentLayer);
                if (iter == rects.end()) {
                    rects.add(parentLayer, Vector<LayoutRect>()).storedValue->value.append(physicalBoundingBox(parentLayer));
                } else {
                    iter->value.append(physicalBoundingBox(parentLayer));
                }
            }
        } else {
            rect.append(logicalBoundingBox());
            rects.set(this, rect);
        }
    }
}

void Layer::setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants()
{
    layoutObject()->setShouldDoFullPaintInvalidation();

    // Disable for reading compositingState() in isPaintInvalidationContainer() below.
    DisableCompositingQueryAsserts disabler;

    for (Layer* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isPaintInvalidationContainer())
            child->setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
    }
}

DisableCompositingQueryAsserts::DisableCompositingQueryAsserts()
    : m_disabler(gCompositingQueryMode, CompositingQueriesAreAllowed) { }

} // namespace blink

#ifndef NDEBUG
void showLayerTree(const blink::Layer* layer)
{
    if (!layer)
        return;

    if (blink::LocalFrame* frame = layer->layoutObject()->frame()) {
        WTF::String output = externalRepresentation(frame, blink::LayoutAsTextShowAllLayers | blink::LayoutAsTextShowLayerNesting | blink::LayoutAsTextShowCompositedLayers | blink::LayoutAsTextShowAddresses | blink::LayoutAsTextShowIDAndClass | blink::LayoutAsTextDontUpdateLayout | blink::LayoutAsTextShowLayoutState);
        fprintf(stderr, "%s\n", output.utf8().data());
    }
}

void showLayerTree(const blink::LayoutObject* renderer)
{
    if (!renderer)
        return;
    showLayerTree(renderer->enclosingLayer());
}
#endif
