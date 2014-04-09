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
#include "core/animation/ActiveAnimations.h"
#include "core/css/PseudoStyleRequest.h"
#include "core/dom/Document.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/DeprecatedScheduleStyleRecalcDuringLayout.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameElement.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/rendering/ColumnInfo.h"
#include "core/rendering/FilterEffectRenderer.h"
#include "core/rendering/HitTestRequest.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/HitTestingTransformState.h"
#include "core/rendering/RenderFlowThread.h"
#include "core/rendering/RenderGeometryMap.h"
#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderReplica.h"
#include "core/rendering/RenderScrollbar.h"
#include "core/rendering/RenderScrollbarPart.h"
#include "core/rendering/RenderTreeAsText.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "core/rendering/svg/ReferenceFilterBuilder.h"
#include "core/rendering/svg/RenderSVGResourceClipper.h"
#include "platform/LengthFunctions.h"
#include "platform/Partitions.h"
#include "platform/TraceEvent.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/filters/ReferenceFilter.h"
#include "platform/graphics/filters/SourceGraphic.h"
#include "platform/transforms/ScaleTransformOperation.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "public/platform/Platform.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CString.h"

using namespace std;

namespace WebCore {

namespace {

static CompositingQueryMode gCompositingQueryMode =
    CompositingQueriesAreOnlyAllowedInCertainDocumentLifecyclePhases;

} // namespace

using namespace HTMLNames;

RenderLayer::RenderLayer(RenderLayerModelObject* renderer, LayerType type)
    : m_layerType(type)
    , m_hasSelfPaintingLayerDescendant(false)
    , m_hasSelfPaintingLayerDescendantDirty(false)
    , m_hasOutOfFlowPositionedDescendant(false)
    , m_hasOutOfFlowPositionedDescendantDirty(true)
    , m_hasUnclippedDescendant(false)
    , m_isUnclippedDescendant(false)
    , m_isRootLayer(renderer->isRenderView())
    , m_usedTransparency(false)
    , m_visibleContentStatusDirty(true)
    , m_hasVisibleContent(false)
    , m_visibleDescendantStatusDirty(false)
    , m_hasVisibleDescendant(false)
    , m_hasVisibleNonLayerContent(false)
    , m_isPaginated(false)
    , m_3DTransformedDescendantStatusDirty(true)
    , m_has3DTransformedDescendant(false)
    , m_containsDirtyOverlayScrollbars(false)
    , m_canSkipRepaintRectsUpdateOnScroll(renderer->isTableCell())
    , m_hasFilterInfo(false)
    , m_needsToUpdateAncestorDependentProperties(true)
    , m_childNeedsToUpdateAncestorDependantProperties(true)
    , m_renderer(renderer)
    , m_parent(0)
    , m_previous(0)
    , m_next(0)
    , m_first(0)
    , m_last(0)
    , m_staticInlinePosition(0)
    , m_staticBlockPosition(0)
    , m_enclosingPaginationLayer(0)
    , m_groupedMapping(0)
    , m_repainter(renderer)
    , m_clipper(renderer)
    , m_blendInfo(renderer)
{
    updateStackingNode();

    m_isSelfPaintingLayer = shouldBeSelfPaintingLayer();

    if (!renderer->firstChild() && renderer->style()) {
        m_visibleContentStatusDirty = false;
        m_hasVisibleContent = renderer->style()->visibility() == VISIBLE;
    }

    updateScrollableArea();
}

RenderLayer::~RenderLayer()
{
    if (!m_renderer->documentBeingDestroyed())
        compositor()->removeOutOfFlowPositionedLayer(this);

    if (renderer()->frame() && renderer()->frame()->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = renderer()->frame()->page()->scrollingCoordinator())
            scrollingCoordinator->willDestroyRenderLayer(this);
    }

    removeFilterInfoIfNeeded();

    if (groupedMapping()) {
        DisableCompositingQueryAsserts disabler;
        groupedMapping()->removeRenderLayerFromSquashingGraphicsLayer(this);
        setGroupedMapping(0);
    }

    // Child layers will be deleted by their corresponding render objects, so
    // we don't need to delete them ourselves.

    clearCompositedLayerMapping(true);
}

String RenderLayer::debugName() const
{
    if (isReflection()) {
        ASSERT(m_reflectionInfo);
        return m_reflectionInfo->debugName();
    }
    return renderer()->debugName();
}

RenderLayerCompositor* RenderLayer::compositor() const
{
    if (!renderer()->view())
        return 0;
    return renderer()->view()->compositor();
}

void RenderLayer::contentChanged(ContentChangeType changeType)
{
    // updateLayerCompositingState will query compositingReasons for accelerated overflow scrolling.
    // This is tripped by LayoutTests/compositing/content-changed-chicken-egg.html
    DisableCompositingQueryAsserts disabler;

    // This can get called when video becomes accelerated, so the layers may change.
    if (changeType == CanvasChanged || changeType == VideoChanged || changeType == FullScreenChanged)
        compositor()->updateLayerCompositingState(this);

    if (changeType == CanvasContextChanged)
        compositor()->setNeedsCompositingUpdate(CompositingUpdateAfterCanvasContextChange);

    if (m_compositedLayerMapping)
        m_compositedLayerMapping->contentChanged(changeType);
}

bool RenderLayer::canRender3DTransforms() const
{
    return compositor()->canRender3DTransforms();
}

bool RenderLayer::paintsWithFilters() const
{
    if (!renderer()->hasFilter())
        return false;

    // https://code.google.com/p/chromium/issues/detail?id=343759
    DisableCompositingQueryAsserts disabler;
    if (!m_compositedLayerMapping
        || compositingState() != PaintsIntoOwnBacking
        || !m_compositedLayerMapping->canCompositeFilters())
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

LayoutSize RenderLayer::subpixelAccumulation() const
{
    return m_subpixelAccumulation;
}

void RenderLayer::setSubpixelAccumulation(const LayoutSize& size)
{
    m_subpixelAccumulation = size;
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
    TRACE_EVENT0("blink_rendering", "RenderLayer::updateLayerPositionsAfterLayout");

    // FIXME: Remove incremental compositing updates after fixing the chicken/egg issues
    // https://code.google.com/p/chromium/issues/detail?id=343756
    DisableCompositingQueryAsserts disabler;

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
    m_clipper.clearClipRects();

    if (hasOverflowControls()) {
        LayoutPoint offsetFromRoot;
        if (geometryMap)
            offsetFromRoot = LayoutPoint(geometryMap->absolutePoint(FloatPoint()));
        else {
            // FIXME: It looks suspicious to call convertToLayerCoords here
            // as canUseConvertToLayerCoords may be true for an ancestor layer.
            convertToLayerCoords(root(), offsetFromRoot);
        }
        scrollableArea()->positionOverflowControls(toIntSize(roundedIntPoint(offsetFromRoot)));
    }

    updateDescendantDependentFlags();

    if (flags & UpdatePagination)
        updatePagination();
    else {
        m_isPaginated = false;
        m_enclosingPaginationLayer = 0;
    }

    repainter().repaintAfterLayout(geometryMap, flags & CheckForRepaint);

    // Go ahead and update the reflection's position and size.
    if (m_reflectionInfo)
        m_reflectionInfo->reflection()->layout();

    if (useRegionBasedColumns() && renderer()->isInFlowRenderFlowThread()) {
        updatePagination();
        flags |= UpdatePagination;
    }

    if (renderer()->hasColumns())
        flags |= UpdatePagination;

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositions(geometryMap, flags);

    if ((flags & NeedsFullRepaintInBacking) && hasCompositedLayerMapping() && !compositedLayerMapping()->paintsIntoCompositedAncestor())
        compositedLayerMapping()->setContentsNeedDisplay();

    if (geometryMap)
        geometryMap->popMappingsToAncestor(parent());
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

    if (containingBlock == otherContainingBlock)
        return false;

    // Maintain a set of containing blocks between the first layer and its
    // closest scrollable ancestor.
    HashSet<const RenderObject*> containingBlocks;
    while (containingBlock) {
        if (containingBlock->enclosingLayer()->scrollsOverflow())
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
        if (otherContainingBlock->enclosingLayer()->scrollsOverflow())
            break;
        otherContainingBlock = otherContainingBlock->containingBlock();
    }

    return true;
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
    if (subtreeIsInvisible())
        return;

    bool positionChanged = updateLayerPosition();
    if (positionChanged)
        flags |= HasChangedAncestor;

    if (geometryMap)
        geometryMap->pushMappingsToAncestor(this, parent());

    if ((flags & HasChangedAncestor) || (flags & HasSeenViewportConstrainedAncestor) || (flags & IsOverflowScroll))
        m_clipper.clearClipRects();

    if (renderer()->style()->hasViewportConstrainedPosition())
        flags |= HasSeenViewportConstrainedAncestor;

    if (renderer()->hasOverflowClip())
        flags |= HasSeenAncestorWithOverflowClip;

    if ((flags & IsOverflowScroll) && (flags & HasSeenAncestorWithOverflowClip) && !m_canSkipRepaintRectsUpdateOnScroll) {
        // FIXME: We could track the repaint container as we walk down the tree.
        repainter().computeRepaintRects(renderer()->containerForRepaint(), geometryMap);
    } else {
        // Check that RenderLayerRepainter's cached rects are correct.
        // FIXME: re-enable these assertions when the issue with table cells is resolved: https://bugs.webkit.org/show_bug.cgi?id=103432
        // ASSERT(repainter().m_repaintRect == renderer()->clippedOverflowRectForRepaint(renderer()->containerForRepaint()));
    }

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositionsAfterScroll(geometryMap, flags);

    // We don't update our reflection as scrolling is a translation which does not change the size()
    // of an object, thus RenderReplica will still repaint itself properly as the layer position was
    // updated above.

    if (geometryMap)
        geometryMap->popMappingsToAncestor(parent());
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
        m_clipper.clearClipRectsIncludingDescendants();
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

static RenderLayer* enclosingLayerForContainingBlock(RenderLayer* layer)
{
    if (RenderObject* containingBlock = layer->renderer()->containingBlock())
        return containingBlock->enclosingLayer();
    return 0;
}

RenderLayer* RenderLayer::renderingContextRoot()
{
    RenderLayer* renderingContext = 0;

    if (shouldPreserve3D())
        renderingContext = this;

    for (RenderLayer* current = enclosingLayerForContainingBlock(this); current && current->shouldPreserve3D(); current = enclosingLayerForContainingBlock(current))
        renderingContext = current;

    return renderingContext;
}

TransformationMatrix RenderLayer::currentTransform(RenderStyle::ApplyTransformOrigin applyOrigin) const
{
    if (!m_transform)
        return TransformationMatrix();

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

RenderLayer* RenderLayer::enclosingOverflowClipLayer(IncludeSelfOrNot includeSelf) const
{
    const RenderLayer* layer = (includeSelf == IncludeSelf) ? this : parent();
    while (layer) {
        if (layer->renderer()->hasOverflowClip())
            return const_cast<RenderLayer*>(layer);

        layer = layer->parent();
    }
    return 0;
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
    return renderer()->document().regionBasedColumnsEnabled();
}

void RenderLayer::updatePagination()
{
    m_isPaginated = false;
    m_enclosingPaginationLayer = 0;

    if (hasCompositedLayerMapping() || !parent())
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

    if (m_stackingNode->isNormalFlowOnly()) {
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
    RenderLayerStackingNode* ancestorStackingContainerNode = m_stackingNode->ancestorStackingContainerNode();
    for (RenderLayer* curr = parent(); curr; curr = curr->parent()) {
        if (curr->renderer()->hasColumns()) {
            m_isPaginated = checkContainingBlockChainForPagination(renderer(), curr->renderBox());
            return;
        }
        if (curr->stackingNode() == ancestorStackingContainerNode)
            return;
    }
}

void RenderLayer::setHasVisibleContent()
{
    if (m_hasVisibleContent && !m_visibleContentStatusDirty) {
        ASSERT(!parent() || parent()->hasVisibleDescendant());
        return;
    }

    m_hasVisibleContent = true;
    m_visibleContentStatusDirty = false;

    setNeedsToUpdateAncestorDependentProperties();

    repainter().computeRepaintRects(renderer()->containerForRepaint());
    if (!m_stackingNode->isNormalFlowOnly()) {
        // We don't collect invisible layers in z-order lists if we are not in compositing mode.
        // As we became visible, we need to dirty our stacking containers ancestors to be properly
        // collected. FIXME: When compositing, we could skip this dirtying phase.
        for (RenderLayerStackingNode* sc = m_stackingNode->ancestorStackingContainerNode(); sc; sc = sc->ancestorStackingContainerNode()) {
            sc->dirtyZOrderLists();
            if (sc->layer()->hasVisibleContent())
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

    FrameView* frameView = renderer()->view()->frameView();
    if (!frameView)
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
        if (ancestor->scrollsOverflow())
            setIsUnclippedDescendant(true);
        ancestor->setHasUnclippedDescendant(true);
    }
}

// FIXME: this is quite brute-force. We could be more efficient if we were to
// track state and update it as appropriate as changes are made in the Render tree.
void RenderLayer::updateScrollingStateAfterCompositingChange()
{
    TRACE_EVENT0("blink_rendering", "RenderLayer::updateScrollingStateAfterCompositingChange");
    m_hasVisibleNonLayerContent = false;
    for (RenderObject* r = renderer()->firstChild(); r; r = r->nextSibling()) {
        if (!r->hasLayer()) {
            m_hasVisibleNonLayerContent = true;
            break;
        }
    }

    m_compositingProperties.hasNonCompositedChild = false;
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        if (child->compositingState() == NotComposited) {
            m_compositingProperties.hasNonCompositedChild = true;
            return;
        }
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

    if (m_blendInfo.childLayerHasBlendModeStatusDirty()) {
        m_blendInfo.setChildLayerHasBlendMode(false);
        for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
            if (!child->stackingNode()->isStackingContext())
                child->updateDescendantDependentFlags();

            bool childLayerHadBlendMode = child->blendInfo().childLayerHasBlendModeWhileDirty();
            bool childLayerHasBlendMode = childLayerHadBlendMode || child->blendInfo().hasBlendMode();

            m_blendInfo.setChildLayerHasBlendMode(childLayerHasBlendMode);

            if (childLayerHasBlendMode)
                break;
        }
        m_blendInfo.setChildLayerHasBlendModeStatusDirty(false);
    }

    if (m_visibleContentStatusDirty) {
        bool previouslyHasVisibleContent = m_hasVisibleContent;
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

        // FIXME: We can remove this code once we remove the recursive tree
        // walk inside updateGraphicsLayerGeometry.
        if (hasVisibleContent() != previouslyHasVisibleContent)
            setNeedsToUpdateAncestorDependentProperties();
    }
}

void RenderLayer::dirty3DTransformedDescendantStatus()
{
    RenderLayerStackingNode* stackingNode = m_stackingNode->ancestorStackingContainerNode();
    if (!stackingNode)
        return;

    stackingNode->layer()->m_3DTransformedDescendantStatusDirty = true;

    // This propagates up through preserve-3d hierarchies to the enclosing flattening layer.
    // Note that preserves3D() creates stacking context, so we can just run up the stacking containers.
    while (stackingNode && stackingNode->layer()->preserves3D()) {
        stackingNode->layer()->m_3DTransformedDescendantStatusDirty = true;
        stackingNode = stackingNode->ancestorStackingContainerNode();
    }
}

// Return true if this layer or any preserve-3d descendants have 3d.
bool RenderLayer::update3DTransformedDescendantStatus()
{
    if (m_3DTransformedDescendantStatusDirty) {
        m_has3DTransformedDescendant = false;

        m_stackingNode->updateZOrderLists();

        // Transformed or preserve-3d descendants can only be in the z-order lists, not
        // in the normal flow list, so we only need to check those.
        RenderLayerStackingNodeIterator iterator(*m_stackingNode.get(), PositiveZOrderChildren | NegativeZOrderChildren);
        while (RenderLayerStackingNode* node = iterator.next())
            m_has3DTransformedDescendant |= node->layer()->update3DTransformedDescendantStatus();

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
            LayoutSize offset = positionedParent->renderBox()->scrolledContentOffset();
            localPoint -= offset;
        }

        if (renderer()->isOutOfFlowPositioned() && positionedParent->renderer()->isInFlowPositioned() && positionedParent->renderer()->isRenderInline()) {
            LayoutSize offset = toRenderInline(positionedParent->renderer())->offsetForInFlowPositionedInline(*toRenderBox(renderer()));
            localPoint += offset;
        }
    } else if (parent()) {
        if (hasCompositedLayerMapping()) {
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
            IntSize scrollOffset = parent()->renderBox()->scrolledContentOffset();
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

    return FloatPoint(floatValueForLength(style->perspectiveOriginX(), borderBox.width().toFloat()), floatValueForLength(style->perspectiveOriginY(), borderBox.height().toFloat()));
}

static inline bool isFixedPositionedContainer(RenderLayer* layer)
{
    return layer->isRootLayer() || layer->hasTransform();
}

RenderLayer* RenderLayer::enclosingPositionedAncestor() const
{
    RenderLayer* curr = parent();
    while (curr && !curr->isPositionedContainer())
        curr = curr->parent();

    return curr;
}

RenderLayer* RenderLayer::enclosingTransformedAncestor() const
{
    RenderLayer* curr = parent();
    while (curr && !curr->isRootLayer() && !curr->transform())
        curr = curr->parent();

    return curr;
}

const RenderLayer* RenderLayer::compositingContainer() const
{
    if (stackingNode()->isNormalFlowOnly())
        return parent();
    if (RenderLayerStackingNode* ancestorStackingNode = stackingNode()->ancestorStackingContainerNode())
        return ancestorStackingNode->layer();
    return 0;
}

// FIXME: having two different functions named enclosingCompositingLayer and enclosingCompositingLayerForRepaint
// is error-prone and misleading for reading code that uses these functions - especially compounded with
// the includeSelf option. It is very likely that we don't even want either of these functions; A layer
// should be told explicitly which GraphicsLayer is the repaintContainer for a RenderLayer, and
// any other use cases should probably have an API between the non-compositing and compositing sides of code.
RenderLayer* RenderLayer::enclosingCompositingLayer(IncludeSelfOrNot includeSelf) const
{
    ASSERT(isAllowedToQueryCompositingState());

    if ((includeSelf == IncludeSelf) && compositingState() != NotComposited && compositingState() != PaintsIntoGroupedBacking)
        return const_cast<RenderLayer*>(this);

    for (const RenderLayer* curr = compositingContainer(); curr; curr = curr->compositingContainer()) {
        if (curr->compositingState() != NotComposited && curr->compositingState() != PaintsIntoGroupedBacking)
            return const_cast<RenderLayer*>(curr);
    }

    return 0;
}

RenderLayer* RenderLayer::enclosingCompositingLayerForRepaint(IncludeSelfOrNot includeSelf) const
{
    ASSERT(isAllowedToQueryCompositingState());

    if ((includeSelf == IncludeSelf) && (compositingState() == PaintsIntoOwnBacking || compositingState() == PaintsIntoGroupedBacking))
        return const_cast<RenderLayer*>(this);

    for (const RenderLayer* curr = compositingContainer(); curr; curr = curr->compositingContainer()) {
        if (curr->compositingState() == PaintsIntoOwnBacking || curr->compositingState() == PaintsIntoGroupedBacking)
            return const_cast<RenderLayer*>(curr);
    }

    return 0;
}

RenderLayer* RenderLayer::ancestorCompositedScrollingLayer() const
{
    if (!compositor()->legacyAcceleratedCompositingForOverflowScrollEnabled())
        return 0;

    RenderObject* containingBlock = renderer()->containingBlock();
    if (!containingBlock)
        return 0;

    RenderLayer* ancestorCompositedScrollingLayer = 0;
    for (RenderLayer* ancestorLayer = containingBlock->enclosingLayer(); ancestorLayer; ancestorLayer = ancestorLayer->parent()) {
        if (ancestorLayer->needsCompositedScrolling()) {
            ancestorCompositedScrollingLayer = ancestorLayer;
            break;
        }
    }

    return ancestorCompositedScrollingLayer;
}

RenderLayer* RenderLayer::ancestorScrollingLayer() const
{
    RenderObject* containingBlock = renderer()->containingBlock();
    if (!containingBlock)
        return 0;

    for (RenderLayer* ancestorLayer = containingBlock->enclosingLayer(); ancestorLayer; ancestorLayer = ancestorLayer->parent()) {
        if (ancestorLayer->scrollsOverflow())
            return ancestorLayer;
    }

    return 0;
}

RenderLayer* RenderLayer::enclosingFilterLayer(IncludeSelfOrNot includeSelf) const
{
    const RenderLayer* curr = (includeSelf == IncludeSelf) ? this : parent();
    for (; curr; curr = curr->parent()) {
        if (curr->requiresFullLayerImageForFilters())
            return const_cast<RenderLayer*>(curr);
    }

    return 0;
}

void RenderLayer::setNeedsToUpdateAncestorDependentProperties()
{
    m_needsToUpdateAncestorDependentProperties = true;

    for (RenderLayer* current = this; current && !current->m_childNeedsToUpdateAncestorDependantProperties; current = current->parent())
        current->m_childNeedsToUpdateAncestorDependantProperties = true;
}

void RenderLayer::updateAncestorDependentProperties(const AncestorDependentProperties& ancestorDependentProperties)
{
    m_ancestorDependentProperties = ancestorDependentProperties;
    m_needsToUpdateAncestorDependentProperties = false;
}

void RenderLayer::clearChildNeedsToUpdateAncestorDependantProperties()
{
    ASSERT(!m_needsToUpdateAncestorDependentProperties);
    m_childNeedsToUpdateAncestorDependantProperties = false;
}

void RenderLayer::setCompositingReasons(CompositingReasons reasons, CompositingReasons mask)
{
    ASSERT(reasons == (reasons & mask));
    if ((m_compositingProperties.compositingReasons & mask) == (reasons & mask))
        return;
    m_compositingProperties.compositingReasons = (reasons & mask) | (m_compositingProperties.compositingReasons & ~mask);
    m_clipper.setCompositingClipRectsDirty();
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

bool RenderLayer::cannotBlitToWindow() const
{
    if (isTransparent() || m_reflectionInfo || hasTransform())
        return true;
    if (!parent())
        return false;
    return parent()->cannotBlitToWindow();
}

bool RenderLayer::isTransparent() const
{
    if (renderer()->node() && renderer()->node()->isSVGElement())
        return false;

    return renderer()->isTransparent() || renderer()->hasMask();
}

RenderLayer* RenderLayer::transparentPaintingAncestor()
{
    if (hasCompositedLayerMapping())
        return 0;

    for (RenderLayer* curr = parent(); curr; curr = curr->parent()) {
        if (curr->hasCompositedLayerMapping())
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

static LayoutRect transparencyClipBox(const RenderLayer*, const RenderLayer* rootLayer, TransparencyClipBoxBehavior, TransparencyClipBoxMode, const LayoutSize& subPixelAccumulation, PaintBehavior = 0);

static void expandClipRectForDescendantsAndReflection(LayoutRect& clipRect, const RenderLayer* layer, const RenderLayer* rootLayer,
    TransparencyClipBoxBehavior transparencyBehavior, const LayoutSize& subPixelAccumulation, PaintBehavior paintBehavior)
{
    // If we have a mask, then the clip is limited to the border box area (and there is
    // no need to examine child layers).
    if (!layer->renderer()->hasMask()) {
        // Note: we don't have to walk z-order lists since transparent elements always establish
        // a stacking container. This means we can just walk the layer tree directly.
        for (RenderLayer* curr = layer->firstChild(); curr; curr = curr->nextSibling()) {
            if (!layer->reflectionInfo() || layer->reflectionInfo()->reflectionLayer() != curr)
                clipRect.unite(transparencyClipBox(curr, rootLayer, transparencyBehavior, DescendantsOfTransparencyClipBox, subPixelAccumulation, paintBehavior));
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
    TransparencyClipBoxMode transparencyMode, const LayoutSize& subPixelAccumulation, PaintBehavior paintBehavior)
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

        delta.move(subPixelAccumulation);
        IntPoint pixelSnappedDelta = roundedIntPoint(delta);
        TransformationMatrix transform;
        transform.translate(pixelSnappedDelta.x(), pixelSnappedDelta.y());
        transform = transform * *layer->transform();

        // We don't use fragment boxes when collecting a transformed layer's bounding box, since it always
        // paints unfragmented.
        LayoutRect clipRect = layer->physicalBoundingBox(layer);
        expandClipRectForDescendantsAndReflection(clipRect, layer, layer, transparencyBehavior, subPixelAccumulation, paintBehavior);
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

    LayoutRect clipRect = layer->physicalBoundingBox(rootLayer);
    expandClipRectForDescendantsAndReflection(clipRect, layer, rootLayer, transparencyBehavior, subPixelAccumulation, paintBehavior);
    layer->renderer()->style()->filterOutsets().expandRect(clipRect);
    clipRect.move(subPixelAccumulation);
    return clipRect;
}

LayoutRect RenderLayer::paintingExtent(const RenderLayer* rootLayer, const LayoutRect& paintDirtyRect, const LayoutSize& subPixelAccumulation, PaintBehavior paintBehavior)
{
    return intersection(transparencyClipBox(this, rootLayer, PaintingTransparencyClipBox, RootOfTransparencyClipBox, subPixelAccumulation, paintBehavior), paintDirtyRect);
}

void RenderLayer::beginTransparencyLayers(GraphicsContext* context, const RenderLayer* rootLayer, const LayoutRect& paintDirtyRect, const LayoutSize& subPixelAccumulation, PaintBehavior paintBehavior)
{
    bool createTransparencyLayerForBlendMode = m_stackingNode->isStackingContext() && m_blendInfo.childLayerHasBlendMode();
    if (context->paintingDisabled() || ((paintsWithTransparency(paintBehavior) || paintsWithBlendMode() || createTransparencyLayerForBlendMode) && m_usedTransparency))
        return;

    RenderLayer* ancestor = transparentPaintingAncestor();
    if (ancestor)
        ancestor->beginTransparencyLayers(context, rootLayer, paintDirtyRect, subPixelAccumulation, paintBehavior);

    if (paintsWithTransparency(paintBehavior) || paintsWithBlendMode() || createTransparencyLayerForBlendMode) {
        m_usedTransparency = true;
        context->save();
        LayoutRect clipRect = paintingExtent(rootLayer, paintDirtyRect, subPixelAccumulation, paintBehavior);
        context->clip(clipRect);

        if (paintsWithBlendMode())
            context->setCompositeOperation(context->compositeOperation(), m_blendInfo.blendMode());

        context->beginTransparencyLayer(renderer()->opacity());

        if (paintsWithBlendMode())
            context->setCompositeOperation(context->compositeOperation(), blink::WebBlendModeNormal);
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

    setNeedsToUpdateAncestorDependentProperties();

    if (child->stackingNode()->isNormalFlowOnly())
        m_stackingNode->dirtyNormalFlowList();

    if (!child->stackingNode()->isNormalFlowOnly() || child->firstChild()) {
        // Dirty the z-order list in which we are contained. The ancestorStackingContainerNode() can be null in the
        // case where we're building up generated content layers. This is ok, since the lists will start
        // off dirty in that case anyway.
        child->stackingNode()->dirtyStackingContainerZOrderLists();
    }

    child->updateDescendantDependentFlags();
    if (child->m_hasVisibleContent || child->m_hasVisibleDescendant)
        setAncestorChainHasVisibleDescendant();

    if (child->isSelfPaintingLayer() || child->hasSelfPaintingLayerDescendant())
        setAncestorChainHasSelfPaintingLayerDescendant();

    if (child->blendInfo().hasBlendMode() || child->blendInfo().childLayerHasBlendMode())
        m_blendInfo.setAncestorChainBlendedDescendant();

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
    child->stackingNode()->setDescendantsAreContiguousInStackingOrderDirty(true);
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

    if (oldChild->stackingNode()->isNormalFlowOnly())
        m_stackingNode->dirtyNormalFlowList();
    if (!oldChild->stackingNode()->isNormalFlowOnly() || oldChild->firstChild()) {
        // Dirty the z-order list in which we are contained.  When called via the
        // reattachment process in removeOnlyThisLayer, the layer may already be disconnected
        // from the main layer tree, so we need to null-check the |stackingContainer| value.
        oldChild->stackingNode()->dirtyStackingContainerZOrderLists();
    }

    if (renderer()->style()->visibility() != VISIBLE)
        dirtyVisibleContentStatus();

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

    if (oldChild->m_blendInfo.hasBlendMode() || oldChild->blendInfo().childLayerHasBlendMode())
        m_blendInfo.dirtyAncestorChainBlendedDescendantStatus();

    if (oldChild->isSelfPaintingLayer() || oldChild->hasSelfPaintingLayerDescendant())
        dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

    return oldChild;
}

void RenderLayer::removeOnlyThisLayer()
{
    if (!m_parent)
        return;

    compositor()->layerWillBeRemoved(m_parent, this);

    // Dirty the clip rects.
    m_clipper.clearClipRectsIncludingDescendants();

    RenderLayer* nextSib = nextSibling();

    // Remove the child reflection layer before moving other child layers.
    // The reflection layer should not be moved to the parent.
    if (m_reflectionInfo)
        removeChild(m_reflectionInfo->reflectionLayer());

    // Now walk our kids and reattach them to our parent.
    RenderLayer* current = m_first;
    while (current) {
        RenderLayer* next = current->nextSibling();
        removeChild(current);
        m_parent->addChild(current, nextSib);

        if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
            current->renderer()->setShouldDoFullRepaintAfterLayout(true);
        else
            current->repainter().setRepaintStatus(NeedsFullRepaint);

        // Hits in compositing/overflow/automatically-opt-into-composited-scrolling-part-1.html
        DisableCompositingQueryAsserts disabler;

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
        RenderLayer* beforeChild = !parentLayer->reflectionInfo() || parentLayer->reflectionInfo()->reflectionLayer() != this ? renderer()->parent()->findNextLayer(parentLayer, renderer()) : 0;
        parentLayer->addChild(this, beforeChild);
    }

    // Remove all descendant layers from the hierarchy and add them to the new position.
    for (RenderObject* curr = renderer()->firstChild(); curr; curr = curr->nextSibling())
        curr->moveLayers(m_parent, this);

    // Clear out all the clip rects.
    m_clipper.clearClipRectsIncludingDescendants();
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
    // since localToAbsolute maps the coordinates from flow thread to regions coordinates and regions can be
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
            if (parentLayer->isPositionedContainer())
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

RenderLayer* RenderLayer::scrollParent() const
{
    ASSERT(compositor()->acceleratedCompositingForOverflowScrollEnabled());

    // Normal flow elements will be parented under the main scrolling layer, so
    // we don't need a scroll parent/child relationship to get them to scroll.
    if (stackingNode()->isNormalFlowOnly())
        return 0;

    // A layer scrolls with its containing block. So to find the overflow scrolling layer
    // that we scroll with respect to, we must ascend the layer tree until we reach the
    // first overflow scrolling div at or above our containing block. I will refer to this
    // layer as our 'scrolling ancestor'.
    //
    // Now, if we reside in a normal flow list, then we will naturally scroll with our scrolling
    // ancestor, and we need not be composited. If, on the other hand, we reside in a z-order
    // list, and on our walk upwards to our scrolling ancestor we find no layer that is a stacking
    // context, then we know that in the stacking tree, we will not be in the subtree rooted at
    // our scrolling ancestor, and we will therefore not scroll with it. In this case, we must
    // be a composited layer since the compositor will need to take special measures to ensure
    // that we scroll with our scrolling ancestor and it cannot do this if we do not promote.

    RenderLayer* scrollParent = ancestorCompositedScrollingLayer();
    if (!scrollParent || scrollParent->stackingNode()->isStackingContainer())
        return 0;

    // If we hit a stacking context on our way up to the ancestor scrolling layer, it will already
    // be composited due to an overflow scrolling parent, so we don't need to.
    for (RenderLayer* ancestor = parent(); ancestor && ancestor != scrollParent; ancestor = ancestor->parent()) {
        if (ancestor->stackingNode()->isStackingContainer()) {
            scrollParent = 0;
            break;
        }
    }

    return scrollParent;
}

RenderLayer* RenderLayer::clipParent() const
{
    if (compositingReasons() & CompositingReasonOutOfFlowClipping && !compositor()->clippedByAncestor(this)) {
        if (RenderObject* containingBlock = renderer()->containingBlock())
            return containingBlock->enclosingLayer()->enclosingCompositingLayer();
    }
    return 0;
}

void RenderLayer::didUpdateNeedsCompositedScrolling()
{
    m_stackingNode->updateIsNormalFlowOnly();
    updateSelfPaintingLayer();

    if (m_stackingNode->isStackingContainer())
        m_stackingNode->dirtyZOrderLists();
    else
        m_stackingNode->clearZOrderLists();

    m_stackingNode->dirtyStackingContainerZOrderLists();

    compositor()->setNeedsToRecomputeCompositingRequirements();
    compositor()->setCompositingLayersNeedRebuild();
}

void RenderLayer::updateReflectionInfo(const RenderStyle* oldStyle)
{
    ASSERT(!oldStyle || !renderer()->style()->reflectionDataEquivalent(oldStyle));
    if (renderer()->hasReflection()) {
        if (!m_reflectionInfo)
            m_reflectionInfo = adoptPtr(new RenderLayerReflectionInfo(toRenderBox(renderer())));
        m_reflectionInfo->updateAfterStyleChange(oldStyle);
    } else if (m_reflectionInfo) {
        m_reflectionInfo = nullptr;
    }
}

void RenderLayer::updateStackingNode()
{
    if (requiresStackingNode())
        m_stackingNode = adoptPtr(new RenderLayerStackingNode(this));
    else
        m_stackingNode = nullptr;
}

void RenderLayer::updateScrollableArea()
{
    if (requiresScrollableArea())
        m_scrollableArea = adoptPtr(new RenderLayerScrollableArea(renderBox()));
    else
        m_scrollableArea = nullptr;
}

PassOwnPtr<Vector<FloatRect> > RenderLayer::collectTrackedRepaintRects() const
{
    if (hasCompositedLayerMapping())
        return compositedLayerMapping()->collectTrackedRepaintRects();
    return nullptr;
}

bool RenderLayer::hasOverflowControls() const
{
    return m_scrollableArea && (m_scrollableArea->hasScrollbar() || m_scrollableArea->hasScrollCorner() || renderer()->style()->resize() != RESIZE_NONE);
}

void RenderLayer::paint(GraphicsContext* context, const LayoutRect& damageRect, PaintBehavior paintBehavior, RenderObject* paintingRoot, PaintLayerFlags paintFlags)
{
    OverlapTestRequestMap overlapTestRequests;

    LayerPaintingInfo paintingInfo(this, enclosingIntRect(damageRect), paintBehavior, LayoutSize(), paintingRoot, &overlapTestRequests);
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

void RenderLayer::clipToRect(const LayerPaintingInfo& localPaintingInfo, GraphicsContext* context, const ClipRect& clipRect,
                             BorderRadiusClippingRule rule)
{
    if (clipRect.rect() == localPaintingInfo.paintDirtyRect && !clipRect.hasRadius())
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
                layer->convertToLayerCoords(localPaintingInfo.rootLayer, delta);
                context->clipRoundedRect(layer->renderer()->style()->getRoundedInnerBorderFor(LayoutRect(delta, layer->size())));
        }

        if (layer == localPaintingInfo.rootLayer)
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
    LayoutRect boundingBox = layer->physicalBoundingBox(rootLayer);
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

static bool paintForFixedRootBackground(const RenderLayer* layer, PaintLayerFlags paintFlags)
{
    return layer->renderer()->isRoot() && (paintFlags & PaintLayerPaintingRootBackgroundOnly);
}

void RenderLayer::paintLayer(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    // https://code.google.com/p/chromium/issues/detail?id=343772
    DisableCompositingQueryAsserts disabler;

    if (compositingState() != NotComposited && compositingState() != PaintsIntoGroupedBacking) {
        // The updatingControlTints() painting pass goes through compositing layers,
        // but we need to ensure that we don't cache clip rects computed with the wrong root in this case.
        if (context->updatingControlTints() || (paintingInfo.paintBehavior & PaintBehaviorFlattenCompositingLayers)) {
            paintFlags |= PaintLayerTemporaryClipRects;
        } else if (!compositedLayerMapping()->paintsIntoCompositedAncestor()
            && !shouldDoSoftwarePaint(this, paintFlags & PaintLayerPaintingReflection)
            && !paintForFixedRootBackground(this, paintFlags)) {
            // If this RenderLayer should paint into its own backing, that will be done via CompositedLayerMapping::paintIntoLayer().
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
                parent()->beginTransparencyLayers(context, paintingInfo.rootLayer, paintingInfo.paintDirtyRect, paintingInfo.subPixelAccumulation, paintingInfo.paintBehavior);
            else
                beginTransparencyLayers(context, paintingInfo.rootLayer, paintingInfo.paintDirtyRect, paintingInfo.subPixelAccumulation, paintingInfo.paintBehavior);
        }

        if (enclosingPaginationLayer()) {
            paintTransformedLayerIntoFragments(context, paintingInfo, paintFlags);
            return;
        }

        // Make sure the parent's clip rects have been calculated.
        ClipRect clipRect = paintingInfo.paintDirtyRect;
        if (parent()) {
            ClipRectsContext clipRectsContext(paintingInfo.rootLayer, (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects,
                IgnoreOverlayScrollbarSize, (paintFlags & PaintLayerPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip);
            clipRect = clipper().backgroundClipRect(clipRectsContext);
            clipRect.intersect(paintingInfo.paintDirtyRect);

            // Push the parent coordinate space's clip.
            parent()->clipToRect(paintingInfo, context, clipRect);
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
    if (m_reflectionInfo)
        m_reflectionInfo->paint(context, paintingInfo, localPaintFlags | PaintLayerPaintingReflection);

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
    m_stackingNode->updateLayerListsIfNeeded();

    LayoutPoint offsetFromRoot;
    convertToLayerCoords(paintingInfo.rootLayer, offsetFromRoot);

    if (compositingState() == PaintsIntoOwnBacking)
        offsetFromRoot.move(subpixelAccumulation());

    LayoutRect rootRelativeBounds;
    bool rootRelativeBoundsComputed = false;

    // Apply clip-path to context.
    bool hasClipPath = false;
    RenderStyle* style = renderer()->style();
    RenderSVGResourceClipper* resourceClipper = 0;
    ClipperContext clipperContext;
    if (renderer()->hasClipPath() && !context->paintingDisabled() && style) {
        ASSERT(style->clipPath());
        if (style->clipPath()->type() == ClipPathOperation::SHAPE) {
            hasClipPath = true;
            context->save();
            ShapeClipPathOperation* clipPath = toShapeClipPathOperation(style->clipPath());

            if (!rootRelativeBoundsComputed) {
                rootRelativeBounds = physicalBoundingBoxIncludingReflectionAndStackingChildren(paintingInfo.rootLayer, offsetFromRoot);
                rootRelativeBoundsComputed = true;
            }

            context->clipPath(clipPath->path(rootRelativeBounds), clipPath->windRule());
        } else if (style->clipPath()->type() == ClipPathOperation::REFERENCE) {
            ReferenceClipPathOperation* referenceClipPathOperation = toReferenceClipPathOperation(style->clipPath());
            Document& document = renderer()->document();
            // FIXME: It doesn't work with forward or external SVG references (https://bugs.webkit.org/show_bug.cgi?id=90405)
            Element* element = document.getElementById(referenceClipPathOperation->fragment());
            if (isSVGClipPathElement(element) && element->renderer()) {
                if (!rootRelativeBoundsComputed) {
                    rootRelativeBounds = physicalBoundingBoxIncludingReflectionAndStackingChildren(paintingInfo.rootLayer, offsetFromRoot);
                    rootRelativeBoundsComputed = true;
                }

                resourceClipper = toRenderSVGResourceClipper(toRenderSVGResourceContainer(element->renderer()));
                if (!resourceClipper->applyClippingToContext(renderer(), rootRelativeBounds,
                    paintingInfo.paintDirtyRect, context, clipperContext)) {
                    // No need to post-apply the clipper if this failed.
                    resourceClipper = 0;
                }
            }
        }
    }

    // Blending operations must be performed only with the nearest ancestor stacking context.
    // Note that there is no need to create a transparency layer if we're painting the root.
    bool createTransparencyLayerForBlendMode = !renderer()->isRoot() && m_stackingNode->isStackingContext() && m_blendInfo.childLayerHasBlendMode();

    if (createTransparencyLayerForBlendMode)
        beginTransparencyLayers(context, paintingInfo.rootLayer, paintingInfo.paintDirtyRect, paintingInfo.subPixelAccumulation, paintingInfo.paintBehavior);

    LayerPaintingInfo localPaintingInfo(paintingInfo);
    FilterEffectRendererHelper filterPainter(filterRenderer() && paintsWithFilters());
    if (filterPainter.haveFilterEffect() && !context->paintingDisabled()) {
        RenderLayerFilterInfo* filterInfo = this->filterInfo();
        ASSERT(filterInfo);
        LayoutRect filterRepaintRect = filterInfo->dirtySourceRect();
        filterRepaintRect.move(offsetFromRoot.x(), offsetFromRoot.y());

        if (!rootRelativeBoundsComputed)
            rootRelativeBounds = physicalBoundingBoxIncludingReflectionAndStackingChildren(paintingInfo.rootLayer, offsetFromRoot);

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
        beginTransparencyLayers(transparencyLayerContext, localPaintingInfo.rootLayer, paintingInfo.paintDirtyRect, paintingInfo.subPixelAccumulation, localPaintingInfo.paintBehavior);
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
    bool shouldPaintClippingMask = (paintFlags & PaintLayerPaintingChildClippingMaskPhase) && shouldPaintContent && !selectionOnly;

    PaintBehavior paintBehavior = PaintBehaviorNormal;
    if (paintFlags & PaintLayerPaintingSkipRootBackground)
        paintBehavior |= PaintBehaviorSkipRootBackground;
    else if (paintFlags & PaintLayerPaintingRootBackgroundOnly)
        paintBehavior |= PaintBehaviorRootBackgroundOnly;

    LayerFragments layerFragments;
    if (shouldPaintContent || shouldPaintOutline || isPaintingOverlayScrollbars) {
        // Collect the fragments. This will compute the clip rectangles and paint offsets for each layer fragment, as well as whether or not the content of each
        // fragment should paint.
        collectFragments(layerFragments, localPaintingInfo.rootLayer, localPaintingInfo.paintDirtyRect,
            (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, IgnoreOverlayScrollbarSize,
            (isPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip, &offsetFromRoot, localPaintingInfo.subPixelAccumulation);
        updatePaintingInfoForFragments(layerFragments, localPaintingInfo, paintFlags, shouldPaintContent, &offsetFromRoot);
    }

    if (shouldPaintBackground)
        paintBackgroundForFragments(layerFragments, context, transparencyLayerContext, paintingInfo.paintDirtyRect, haveTransparency,
                localPaintingInfo, paintBehavior, paintingRootForRenderer);

    if (shouldPaintNegZOrderList)
        paintChildren(NegativeZOrderChildren, context, localPaintingInfo, paintFlags);

    if (shouldPaintOwnContents)
        paintForegroundForFragments(layerFragments, context, transparencyLayerContext, paintingInfo.paintDirtyRect, haveTransparency,
                localPaintingInfo, paintBehavior, paintingRootForRenderer, selectionOnly, forceBlackText);

    if (shouldPaintOutline)
        paintOutlineForFragments(layerFragments, context, localPaintingInfo, paintBehavior, paintingRootForRenderer);

    if (shouldPaintNormalFlowAndPosZOrderLists)
        paintChildren(NormalFlowChildren | PositiveZOrderChildren, context, localPaintingInfo, paintFlags);

    if (shouldPaintOverlayScrollbars)
        paintOverflowControlsForFragments(layerFragments, context, localPaintingInfo);

    if (filterPainter.hasStartedFilterEffect()) {
        // Apply the correct clipping (ie. overflow: hidden).
        // FIXME: It is incorrect to just clip to the damageRect here once multiple fragments are involved.
        ClipRect backgroundRect = layerFragments.isEmpty() ? ClipRect() : layerFragments[0].backgroundRect;
        clipToRect(localPaintingInfo, transparencyLayerContext, backgroundRect);
        context = filterPainter.applyFilterEffect();
        restoreClip(transparencyLayerContext, localPaintingInfo.paintDirtyRect, backgroundRect);
    }

    // Make sure that we now use the original transparency context.
    ASSERT(transparencyLayerContext == context);

    if (shouldPaintMask)
        paintMaskForFragments(layerFragments, context, localPaintingInfo, paintingRootForRenderer);

    if (shouldPaintClippingMask) {
        // Paint the border radius mask for the fragments.
        paintChildClippingMaskForFragments(layerFragments, context, localPaintingInfo, paintingRootForRenderer);
    }

    // End our transparency layer
    if ((haveTransparency || paintsWithBlendMode() || createTransparencyLayerForBlendMode) && m_usedTransparency && !(m_reflectionInfo && m_reflectionInfo->isPaintingInsideReflection())) {
        context->endLayer();
        context->restore();
        m_usedTransparency = false;
    }

    if (resourceClipper)
        resourceClipper->postApplyStatefulResource(renderer(), context, clipperContext);

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
    GraphicsContextStateSaver stateSaver(*context, false);
    if (!transform.isIdentity()) {
        stateSaver.save();
        context->concatCTM(transform.toAffineTransform());
    }

    // Now do a paint with the root layer shifted to be us.
    LayerPaintingInfo transformedPaintingInfo(this, enclosingIntRect(transform.inverse().mapRect(paintingInfo.paintDirtyRect)), paintingInfo.paintBehavior,
        adjustedSubPixelAccumulation, paintingInfo.paintingRoot, paintingInfo.overlapTestRequests);
    paintLayerContentsAndReflection(context, transformedPaintingInfo, paintFlags);
}

void RenderLayer::paintChildren(unsigned childrenToVisit, GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    if (!hasSelfPaintingLayerDescendant())
        return;

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(m_stackingNode.get());
#endif

    RenderLayerStackingNodeIterator iterator(*m_stackingNode, childrenToVisit);
    while (RenderLayerStackingNode* child = iterator.next()) {
        RenderLayer* childLayer = child->layer();

        // Squashed RenderLayers should not paint into their ancestor.
        if (childLayer->compositingState() == PaintsIntoGroupedBacking)
            continue;

        if (!childLayer->isPaginated())
            childLayer->paintLayer(context, paintingInfo, paintFlags);
        else
            paintPaginatedChildLayer(childLayer, context, paintingInfo, paintFlags);
    }
}

void RenderLayer::collectFragments(LayerFragments& fragments, const RenderLayer* rootLayer, const LayoutRect& dirtyRect,
    ClipRectsType clipRectsType, OverlayScrollbarSizeRelevancy inOverlayScrollbarSizeRelevancy, ShouldRespectOverflowClip respectOverflowClip, const LayoutPoint* offsetFromRoot,
    const LayoutSize& subPixelAccumulation, const LayoutRect* layerBoundingBox)
{
    if (!enclosingPaginationLayer() || hasTransform()) {
        // For unpaginated layers, there is only one fragment.
        LayerFragment fragment;
        ClipRectsContext clipRectsContext(rootLayer, clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip, subPixelAccumulation);
        clipper().calculateRects(clipRectsContext, dirtyRect, fragment.layerBounds, fragment.backgroundRect, fragment.foregroundRect, fragment.outlineRect, offsetFromRoot);
        fragments.append(fragment);
        return;
    }

    // Compute our offset within the enclosing pagination layer.
    LayoutPoint offsetWithinPaginatedLayer;
    convertToLayerCoords(enclosingPaginationLayer(), offsetWithinPaginatedLayer);

    // Calculate clip rects relative to the enclosingPaginationLayer. The purpose of this call is to determine our bounds clipped to intermediate
    // layers between us and the pagination context. It's important to minimize the number of fragments we need to create and this helps with that.
    ClipRectsContext paginationClipRectsContext(enclosingPaginationLayer(), clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip);
    LayoutRect layerBoundsInFlowThread;
    ClipRect backgroundRectInFlowThread;
    ClipRect foregroundRectInFlowThread;
    ClipRect outlineRectInFlowThread;
    clipper().calculateRects(paginationClipRectsContext, PaintInfo::infiniteRect(), layerBoundsInFlowThread, backgroundRectInFlowThread, foregroundRectInFlowThread,
        outlineRectInFlowThread, &offsetWithinPaginatedLayer);

    // Take our bounding box within the flow thread and clip it.
    LayoutRect layerBoundingBoxInFlowThread = layerBoundingBox ? *layerBoundingBox : physicalBoundingBox(enclosingPaginationLayer(), &offsetWithinPaginatedLayer);
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
        ClipRectsContext clipRectsContext(rootLayer, clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip);
        ancestorClipRect = enclosingPaginationLayer()->clipper().backgroundClipRect(clipRectsContext);
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
    LayoutRect transformedExtent = transparencyClipBox(this, enclosingPaginationLayer(), PaintingTransparencyClipBox, RootOfTransparencyClipBox, paintingInfo.subPixelAccumulation, paintingInfo.paintBehavior);
    enclosingPaginationLayer()->collectFragments(enclosingPaginationFragments, paintingInfo.rootLayer, paintingInfo.paintDirtyRect,
        (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, IgnoreOverlayScrollbarSize,
        (paintFlags & PaintLayerPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip, &offsetOfPaginationLayerFromRoot, paintingInfo.subPixelAccumulation, &transformedExtent);

    for (size_t i = 0; i < enclosingPaginationFragments.size(); ++i) {
        const LayerFragment& fragment = enclosingPaginationFragments.at(i);

        // Apply the page/column clip for this fragment, as well as any clips established by layers in between us and
        // the enclosing pagination layer.
        LayoutRect clipRect = fragment.backgroundRect.rect();

        // Now compute the clips within a given fragment
        if (parent() != enclosingPaginationLayer()) {
            enclosingPaginationLayer()->convertToLayerCoords(paintingInfo.rootLayer, offsetOfPaginationLayerFromRoot);

            ClipRectsContext clipRectsContext(enclosingPaginationLayer(), (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects,
                IgnoreOverlayScrollbarSize, (paintFlags & PaintLayerPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip);
            LayoutRect parentClipRect = clipper().backgroundClipRect(clipRectsContext).rect();
            parentClipRect.moveBy(fragment.paginationOffset + offsetOfPaginationLayerFromRoot);
            clipRect.intersect(parentClipRect);
        }

        parent()->clipToRect(paintingInfo, context, clipRect);
        paintLayerByApplyingTransform(context, paintingInfo, paintFlags, fragment.paginationOffset);
        parent()->restoreClip(context, paintingInfo.paintDirtyRect, clipRect);
    }
}

static inline LayoutSize subPixelAccumulationIfNeeded(const LayoutSize& subPixelAccumulation, CompositingState compositingState)
{
    // Only apply the sub-pixel accumulation if we don't paint into our own backing layer, otherwise the position
    // of the renderer already includes any sub-pixel offset.
    if (compositingState == PaintsIntoOwnBacking)
        return LayoutSize();
    return subPixelAccumulation;
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
        if (haveTransparency || paintsWithBlendMode())
            beginTransparencyLayers(transparencyLayerContext, localPaintingInfo.rootLayer, transparencyPaintDirtyRect, localPaintingInfo.subPixelAccumulation, localPaintingInfo.paintBehavior);

        if (localPaintingInfo.clipToDirtyRect) {
            // Paint our background first, before painting any child layers.
            // Establish the clip used to paint our background.
            clipToRect(localPaintingInfo, context, fragment.backgroundRect, DoNotIncludeSelfForBorderRadius); // Background painting will handle clipping to self.
        }

        // Paint the background.
        // FIXME: Eventually we will collect the region from the fragment itself instead of just from the paint info.
        PaintInfo paintInfo(context, pixelSnappedIntRect(fragment.backgroundRect.rect()), PaintPhaseBlockBackground, paintBehavior, paintingRootForRenderer, 0, 0, localPaintingInfo.rootLayer->renderer());
        renderer()->paint(paintInfo, toPoint(fragment.layerBounds.location() - renderBoxLocation() + subPixelAccumulationIfNeeded(localPaintingInfo.subPixelAccumulation, compositingState())));

        if (localPaintingInfo.clipToDirtyRect)
            restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.backgroundRect);
    }
}

void RenderLayer::paintForegroundForFragments(const LayerFragments& layerFragments, GraphicsContext* context, GraphicsContext* transparencyLayerContext,
    const LayoutRect& transparencyPaintDirtyRect, bool haveTransparency, const LayerPaintingInfo& localPaintingInfo, PaintBehavior paintBehavior,
    RenderObject* paintingRootForRenderer, bool selectionOnly, bool forceBlackText)
{
    // Begin transparency if we have something to paint.
    if (haveTransparency || paintsWithBlendMode()) {
        for (size_t i = 0; i < layerFragments.size(); ++i) {
            const LayerFragment& fragment = layerFragments.at(i);
            if (fragment.shouldPaintContent && !fragment.foregroundRect.isEmpty()) {
                beginTransparencyLayers(transparencyLayerContext, localPaintingInfo.rootLayer, transparencyPaintDirtyRect, localPaintingInfo.subPixelAccumulation, localPaintingInfo.paintBehavior);
                break;
            }
        }
    }

    PaintBehavior localPaintBehavior = forceBlackText ? (PaintBehavior)PaintBehaviorForceBlackText : paintBehavior;

    // Optimize clipping for the single fragment case.
    bool shouldClip = localPaintingInfo.clipToDirtyRect && layerFragments.size() == 1 && layerFragments[0].shouldPaintContent && !layerFragments[0].foregroundRect.isEmpty();
    if (shouldClip)
        clipToRect(localPaintingInfo, context, layerFragments[0].foregroundRect);

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
            clipToRect(localPaintingInfo, context, fragment.foregroundRect);

        PaintInfo paintInfo(context, pixelSnappedIntRect(fragment.foregroundRect.rect()), phase, paintBehavior, paintingRootForRenderer, 0, 0, localPaintingInfo.rootLayer->renderer());
        if (phase == PaintPhaseForeground)
            paintInfo.overlapTestRequests = localPaintingInfo.overlapTestRequests;
        renderer()->paint(paintInfo, toPoint(fragment.layerBounds.location() - renderBoxLocation() + subPixelAccumulationIfNeeded(localPaintingInfo.subPixelAccumulation, compositingState())));

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
        PaintInfo paintInfo(context, pixelSnappedIntRect(fragment.outlineRect.rect()), PaintPhaseSelfOutline, paintBehavior, paintingRootForRenderer, 0, 0, localPaintingInfo.rootLayer->renderer());
        clipToRect(localPaintingInfo, context, fragment.outlineRect, DoNotIncludeSelfForBorderRadius);
        renderer()->paint(paintInfo, toPoint(fragment.layerBounds.location() - renderBoxLocation() + subPixelAccumulationIfNeeded(localPaintingInfo.subPixelAccumulation, compositingState())));
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
            clipToRect(localPaintingInfo, context, fragment.backgroundRect, DoNotIncludeSelfForBorderRadius); // Mask painting will handle clipping to self.

        // Paint the mask.
        // FIXME: Eventually we will collect the region from the fragment itself instead of just from the paint info.
        PaintInfo paintInfo(context, pixelSnappedIntRect(fragment.backgroundRect.rect()), PaintPhaseMask, PaintBehaviorNormal, paintingRootForRenderer, 0, 0, localPaintingInfo.rootLayer->renderer());
        renderer()->paint(paintInfo, toPoint(fragment.layerBounds.location() - renderBoxLocation() + subPixelAccumulationIfNeeded(localPaintingInfo.subPixelAccumulation, compositingState())));

        if (localPaintingInfo.clipToDirtyRect)
            restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.backgroundRect);
    }
}

void RenderLayer::paintChildClippingMaskForFragments(const LayerFragments& layerFragments, GraphicsContext* context, const LayerPaintingInfo& localPaintingInfo,
    RenderObject* paintingRootForRenderer)
{
    for (size_t i = 0; i < layerFragments.size(); ++i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if (!fragment.shouldPaintContent)
            continue;

        if (localPaintingInfo.clipToDirtyRect)
            clipToRect(localPaintingInfo, context, fragment.foregroundRect, IncludeSelfForBorderRadius); // Child clipping mask painting will handle clipping to self.

        // Paint the the clipped mask.
        PaintInfo paintInfo(context, pixelSnappedIntRect(fragment.backgroundRect.rect()), PaintPhaseClippingMask, PaintBehaviorNormal, paintingRootForRenderer, 0, 0, localPaintingInfo.rootLayer->renderer());
        renderer()->paint(paintInfo, toPoint(fragment.layerBounds.location() - renderBoxLocation() + subPixelAccumulationIfNeeded(localPaintingInfo.subPixelAccumulation, compositingState())));

        if (localPaintingInfo.clipToDirtyRect)
            restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.foregroundRect);
    }
}

void RenderLayer::paintOverflowControlsForFragments(const LayerFragments& layerFragments, GraphicsContext* context, const LayerPaintingInfo& localPaintingInfo)
{
    for (size_t i = 0; i < layerFragments.size(); ++i) {
        const LayerFragment& fragment = layerFragments.at(i);
        clipToRect(localPaintingInfo, context, fragment.backgroundRect);
        if (RenderLayerScrollableArea* scrollableArea = this->scrollableArea())
            scrollableArea->paintOverflowControls(context, roundedIntPoint(toPoint(fragment.layerBounds.location() - renderBoxLocation() + subPixelAccumulationIfNeeded(localPaintingInfo.subPixelAccumulation, compositingState()))), pixelSnappedIntRect(fragment.backgroundRect.rect()), true);
        restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.backgroundRect);
    }
}

void RenderLayer::paintPaginatedChildLayer(RenderLayer* childLayer, GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    // We need to do multiple passes, breaking up our child layer into strips.
    Vector<RenderLayer*> columnLayers;
    RenderLayerStackingNode* ancestorNode = m_stackingNode->isNormalFlowOnly() ? parent()->stackingNode() : m_stackingNode->ancestorStackingContainerNode();
    for (RenderLayer* curr = childLayer->parent(); curr; curr = curr->parent()) {
        if (curr->renderer()->hasColumns() && checkContainingBlockChainForPagination(childLayer->renderer(), curr->renderBox()))
            columnLayers.append(curr);
        if (curr->stackingNode() == ancestorNode)
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
            ClipRectsContext clipRectsContext(rootLayer, RootRelativeClipRects, IncludeOverlayScrollbarSize);
            ClipRect clipRect = clipper().backgroundClipRect(clipRectsContext);
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
    RenderLayer* hitLayer = hitTestChildren(PositiveZOrderChildren, rootLayer, request, result, hitTestRect, hitTestLocation,
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
    collectFragments(layerFragments, rootLayer, hitTestRect, RootRelativeClipRects, IncludeOverlayScrollbarSize);

    if (m_scrollableArea && m_scrollableArea->hitTestResizerInFragments(layerFragments, hitTestLocation)) {
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

RenderLayer* RenderLayer::hitTestTransformedLayerInFragments(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffset)
{
    LayerFragments enclosingPaginationFragments;
    LayoutPoint offsetOfPaginationLayerFromRoot;
    // FIXME: We're missing a sub-pixel offset here crbug.com/348728
    LayoutRect transformedExtent = transparencyClipBox(this, enclosingPaginationLayer(), HitTestingTransparencyClipBox, RootOfTransparencyClipBox, LayoutSize());
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

            ClipRectsContext clipRectsContext(enclosingPaginationLayer(), RootRelativeClipRects, IncludeOverlayScrollbarSize);
            LayoutRect parentClipRect = clipper().backgroundClipRect(clipRectsContext).rect();
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

RenderLayer* RenderLayer::hitTestChildren(ChildrenIteration childrentoVisit, RenderLayer* rootLayer,
    const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation,
    const HitTestingTransformState* transformState,
    double* zOffsetForDescendants, double* zOffset,
    const HitTestingTransformState* unflattenedTransformState,
    bool depthSortDescendants)
{
    if (!hasSelfPaintingLayerDescendant())
        return 0;

    RenderLayer* resultLayer = 0;
    RenderLayerStackingNodeReverseIterator iterator(*m_stackingNode, childrentoVisit);
    while (RenderLayerStackingNode* child = iterator.next()) {
        RenderLayer* childLayer = child->layer();
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
    RenderLayerStackingNode* ancestorNode = m_stackingNode->isNormalFlowOnly() ? parent()->stackingNode() : m_stackingNode->ancestorStackingContainerNode();
    for (RenderLayer* curr = childLayer->parent(); curr; curr = curr->parent()) {
        if (curr->renderer()->hasColumns() && checkContainingBlockChainForPagination(childLayer->renderer(), curr->renderBox()))
            columnLayers.append(curr);
        if (curr->stackingNode() == ancestorNode)
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
    if (renderer()->hasOverflowClip()) {
        RenderBox* box = renderBox();
        rect.move(-box->scrolledContentOffset());
        if (!scrollableArea()->usesCompositedScrolling())
            rect.intersect(box->overflowClipRect(LayoutPoint()));
    }
    if (renderer()->hasClip())
        rect.intersect(toRenderBox(renderer())->clipRect(LayoutPoint()));
    if (!rect.isEmpty())
        renderer()->repaintRectangle(rect);
}

bool RenderLayer::hasBlockSelectionGapBounds() const
{
    return !m_blockSelectionGapsBounds.isEmpty();
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
        if (layerBounds.intersects(damageRect))
            return true;
    }

    // Otherwise we need to compute the bounding box of this single layer and see if it intersects
    // the damage rect.
    return physicalBoundingBox(rootLayer, offsetFromRoot).intersects(damageRect);
}

LayoutRect RenderLayer::logicalBoundingBox() const
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
    if (renderer()->isInline() && renderer()->isRenderInline()) {
        result = toRenderInline(renderer())->linesVisualOverflowBoundingBox();
    } else if (renderer()->isTableRow()) {
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
        result = box->borderBoxRect();
        result.unite(box->visualOverflowRect());
    }

    ASSERT(renderer()->view());
    return result;
}

LayoutRect RenderLayer::physicalBoundingBox(const RenderLayer* ancestorLayer, const LayoutPoint* offsetFromRoot) const
{
    LayoutRect result = logicalBoundingBox();
    if (m_renderer->isBox())
        renderBox()->flipForWritingMode(result);
    else
        m_renderer->containingBlock()->flipForWritingMode(result);

    LayoutPoint delta;
    if (offsetFromRoot)
        delta = *offsetFromRoot;
    else
        convertToLayerCoords(ancestorLayer, delta);

    result.moveBy(delta);
    return result;
}

LayoutRect RenderLayer::physicalBoundingBoxIncludingReflectionAndStackingChildren(const RenderLayer* ancestorLayer, const LayoutPoint& offsetFromRoot) const
{
    LayoutPoint origin;
    LayoutRect result = physicalBoundingBox(ancestorLayer, &origin);

    if (m_reflectionInfo && !m_reflectionInfo->reflectionLayer()->hasCompositedLayerMapping())
        result.unite(m_reflectionInfo->reflectionLayer()->physicalBoundingBox(this));

    ASSERT(m_stackingNode->isStackingContainer() || !m_stackingNode->hasPositiveZOrderList());

    const_cast<RenderLayer*>(this)->stackingNode()->updateLayerListsIfNeeded();

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(const_cast<RenderLayer*>(this)->stackingNode());
#endif

    RenderLayerStackingNodeIterator iterator(*m_stackingNode.get(), AllChildren);
    while (RenderLayerStackingNode* node = iterator.next()) {
        if (node->layer()->hasCompositedLayerMapping())
            continue;
        // FIXME: Can we call physicalBoundingBoxIncludingReflectionAndStackingChildren instead of boundingBoxForCompositing?
        result.unite(node->layer()->boundingBoxForCompositing(this));
    }

    result.moveBy(offsetFromRoot);
    return result;
}

LayoutRect RenderLayer::boundingBoxForCompositing(const RenderLayer* ancestorLayer, CalculateBoundsOptions options) const
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
        return m_renderer->view()->unscaledDocumentRect();

    const bool shouldIncludeTransform = paintsWithTransform(PaintBehaviorNormal) || (options == ApplyBoundsChickenEggHacks && transform() && this == ancestorLayer);

    LayoutRect localClipRect = clipper().localClipRect();
    if (localClipRect != PaintInfo::infiniteRect()) {
        if (shouldIncludeTransform)
            localClipRect = transform()->mapRect(localClipRect);

        LayoutPoint delta;
        convertToLayerCoords(ancestorLayer, delta);
        localClipRect.moveBy(delta);
        return localClipRect;
    }

    LayoutPoint origin;
    LayoutRect result = physicalBoundingBox(ancestorLayer, &origin);

    const_cast<RenderLayer*>(this)->stackingNode()->updateLayerListsIfNeeded();

    if (m_reflectionInfo && !m_reflectionInfo->reflectionLayer()->hasCompositedLayerMapping())
        result.unite(m_reflectionInfo->reflectionLayer()->boundingBoxForCompositing(this));

    ASSERT(m_stackingNode->isStackingContainer() || !m_stackingNode->hasPositiveZOrderList());

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(const_cast<RenderLayer*>(this)->stackingNode());
#endif

    RenderLayerStackingNodeIterator iterator(*m_stackingNode.get(), AllChildren);
    while (RenderLayerStackingNode* node = iterator.next()) {
        if (node->layer()->hasCompositedLayerMapping() && options != ApplyBoundsChickenEggHacks)
            continue;
        result.unite(node->layer()->boundingBoxForCompositing(this, options));
    }

    // FIXME: We can optimize the size of the composited layers, by not enlarging
    // filtered areas with the outsets if we know that the filter is going to render in hardware.
    // https://bugs.webkit.org/show_bug.cgi?id=81239
    m_renderer->style()->filterOutsets().expandRect(result);

    if (shouldIncludeTransform)
        result = transform()->mapRect(result);

    LayoutPoint delta;
    convertToLayerCoords(ancestorLayer, delta);
    result.moveBy(delta);
    return result;
}

CompositingState RenderLayer::compositingState() const
{
    ASSERT(isAllowedToQueryCompositingState());

    // This is computed procedurally so there is no redundant state variable that
    // can get out of sync from the real actual compositing state.

    if (m_groupedMapping) {
        ASSERT(compositor()->layerSquashingEnabled());
        ASSERT(!m_compositedLayerMapping);
        return PaintsIntoGroupedBacking;
    }

    if (!m_compositedLayerMapping)
        return NotComposited;

    if (compositedLayerMapping()->paintsIntoCompositedAncestor())
        return HasOwnBackingButPaintsIntoAncestor;

    return PaintsIntoOwnBacking;
}

bool RenderLayer::isAllowedToQueryCompositingState() const
{
    if (gCompositingQueryMode == CompositingQueriesAreAllowed)
        return true;
    return renderer()->document().lifecycle().state() >= DocumentLifecycle::InCompositingUpdate;
}

bool RenderLayer::isInCompositingUpdate() const
{
    return renderer()->document().lifecycle().state() == DocumentLifecycle::InCompositingUpdate;
}

CompositedLayerMappingPtr RenderLayer::compositedLayerMapping() const
{
    ASSERT(isAllowedToQueryCompositingState());
    return m_compositedLayerMapping.get();
}

CompositedLayerMappingPtr RenderLayer::ensureCompositedLayerMapping()
{
    if (!m_compositedLayerMapping) {
        m_compositedLayerMapping = adoptPtr(new CompositedLayerMapping(*this));
        m_compositedLayerMapping->setNeedsGraphicsLayerUpdate();

        updateOrRemoveFilterEffectRenderer();

        if (RuntimeEnabledFeatures::cssCompositingEnabled())
            compositedLayerMapping()->setBlendMode(m_blendInfo.blendMode());
    }
    return m_compositedLayerMapping.get();
}

void RenderLayer::clearCompositedLayerMapping(bool layerBeingDestroyed)
{
    if (!layerBeingDestroyed) {
        // We need to make sure our decendants get a geometry update. In principle,
        // we could call setNeedsGraphicsLayerUpdate on our children, but that would
        // require walking the z-order lists to find them. Instead, we over-invalidate
        // by marking our parent as needing a geometry update.
        if (RenderLayer* compositingParent = enclosingCompositingLayer(ExcludeSelf))
            compositingParent->compositedLayerMapping()->setNeedsGraphicsLayerUpdate();
    }

    m_compositedLayerMapping.clear();

    if (!layerBeingDestroyed)
        updateOrRemoveFilterEffectRenderer();
}

void RenderLayer::setGroupedMapping(CompositedLayerMapping* groupedMapping, bool layerBeingDestroyed)
{
    if (!layerBeingDestroyed && m_groupedMapping)
        m_groupedMapping->setNeedsGraphicsLayerUpdate();
    m_groupedMapping = groupedMapping;
    if (!layerBeingDestroyed && m_groupedMapping)
        m_groupedMapping->setNeedsGraphicsLayerUpdate();
}

bool RenderLayer::hasCompositedMask() const
{
    return m_compositedLayerMapping && m_compositedLayerMapping->hasMaskLayer();
}

bool RenderLayer::hasCompositedClippingMask() const
{
    return m_compositedLayerMapping && m_compositedLayerMapping->hasChildClippingMaskLayer();
}

bool RenderLayer::clipsCompositingDescendantsWithBorderRadius() const
{
    RenderStyle* style = renderer()->style();
    if (!style)
        return false;

    return compositor()->clipsCompositingDescendants(this) && style->hasBorderRadius();
}

bool RenderLayer::paintsWithTransform(PaintBehavior paintBehavior) const
{
    return transform() && ((paintBehavior & PaintBehaviorFlattenCompositingLayers) || compositingState() != PaintsIntoOwnBacking);
}

bool RenderLayer::paintsWithBlendMode() const
{
    return m_blendInfo.hasBlendMode() && compositingState() != PaintsIntoOwnBacking;
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
    if (m_stackingNode->zOrderListsDirty() || m_stackingNode->normalFlowListDirty())
        return false;

    // FIXME: We currently only check the immediate renderer,
    // which will miss many cases.
    if (renderer()->backgroundIsKnownToBeOpaqueInRect(localRect))
        return true;

    // We can't consult child layers if we clip, since they might cover
    // parts of the rect that are clipped out.
    if (renderer()->hasOverflowClip())
        return false;

    return childBackgroundIsKnownToBeOpaqueInRect(localRect);
}

bool RenderLayer::childBackgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const
{
    RenderLayerStackingNodeReverseIterator revertseIterator(*m_stackingNode, PositiveZOrderChildren | NormalFlowChildren | NegativeZOrderChildren);
    while (RenderLayerStackingNode* child = revertseIterator.next()) {
        const RenderLayer* childLayer = child->layer();
        if (childLayer->hasCompositedLayerMapping())
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

bool RenderLayer::shouldBeSelfPaintingLayer() const
{
    return m_layerType == NormalLayer
        || (m_scrollableArea && m_scrollableArea->hasOverlayScrollbars())
        || needsCompositedScrolling();
}

void RenderLayer::updateSelfPaintingLayer()
{
    bool isSelfPaintingLayer = this->shouldBeSelfPaintingLayer();
    if (this->isSelfPaintingLayer() == isSelfPaintingLayer)
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

void RenderLayer::updateOutOfFlowPositioned(const RenderStyle* oldStyle)
{
    ASSERT(!oldStyle || renderer()->style()->position() != oldStyle->position());

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
    if (hasCompositedLayerMapping())
        return false;
    if (!oldStyle)
        return false;
    if (oldStyle->overflowX() == newStyle->overflowX())
        return false;
    RenderLayerStackingNode* stackingNode = m_stackingNode->ancestorStackingContainerNode();
    return stackingNode && stackingNode->layer()->hasCompositingDescendant();
}

inline bool RenderLayer::needsCompositingLayersRebuiltForFilters(const RenderStyle* oldStyle, const RenderStyle* newStyle, bool didPaintWithFilters) const
{
    if (!hasOrHadFilters(oldStyle, newStyle))
        return false;

    if (newStyle->isRunningFilterAnimationOnCompositor()) {
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

    return false;
}

inline bool RenderLayer::needsCompositingLayersRebuiltForBlending(const RenderStyle* oldStyle, const RenderStyle* newStyle) const
{
    ASSERT(newStyle);
    if (!hasCompositedLayerMapping())
        return false;
    return (shouldIsolateCompositedDescendants() && !stackingNode()->isStackingContext())
        || (oldStyle && (oldStyle->hasBlendMode() != newStyle->hasBlendMode()));
}

void RenderLayer::updateFilters(const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    if (!hasOrHadFilters(oldStyle, newStyle))
        return;

    updateOrRemoveFilterClients();
    // During an accelerated animation, both WebKit and the compositor animate properties.
    // However, WebKit shouldn't ask the compositor to update its filters if the compositor is performing the animation.
    if (hasCompositedLayerMapping() && !newStyle->isRunningFilterAnimationOnCompositor())
        compositedLayerMapping()->updateFilters(renderer()->style());
    updateOrRemoveFilterEffectRenderer();
}

void RenderLayer::styleChanged(StyleDifference diff, const RenderStyle* oldStyle)
{
    m_stackingNode->updateIsNormalFlowOnly();
    m_stackingNode->updateStackingNodesAfterStyleChange(oldStyle);

    if (m_scrollableArea)
        m_scrollableArea->updateAfterStyleChange(oldStyle);

    if (!oldStyle || oldStyle->visibility() != renderer()->style()->visibility()) {
        ASSERT(!oldStyle || diff >= StyleDifferenceRepaint);
        compositor()->setNeedsUpdateCompositingRequirementsState();
    }

    // Overlay scrollbars can make this layer self-painting so we need
    // to recompute the bit once scrollbars have been updated.
    updateSelfPaintingLayer();

    if (!oldStyle || renderer()->style()->position() != oldStyle->position()) {
        ASSERT(!oldStyle || diff >= StyleDifferenceLayout);
        updateOutOfFlowPositioned(oldStyle);
    }

    if (!oldStyle || !renderer()->style()->reflectionDataEquivalent(oldStyle)) {
        ASSERT(!oldStyle || diff >= StyleDifferenceLayout);
        updateReflectionInfo(oldStyle);
    }

    if (RuntimeEnabledFeatures::cssCompositingEnabled())
        m_blendInfo.updateBlendMode();

    updateDescendantDependentFlags();

    if (!oldStyle || !renderer()->style()->transformDataEquivalent(*oldStyle))
        updateTransform();

    bool didPaintWithFilters = false;

    {
        // https://code.google.com/p/chromium/issues/detail?id=343759
        DisableCompositingQueryAsserts disabler;
        if (paintsWithFilters())
            didPaintWithFilters = true;
        updateFilters(oldStyle, renderer()->style());
    }

    compositor()->updateStyleDeterminedCompositingReasons(this);

    setNeedsToUpdateAncestorDependentProperties();

    // FIXME: Remove incremental compositing updates after fixing the chicken/egg issues
    // https://code.google.com/p/chromium/issues/detail?id=343756
    DisableCompositingQueryAsserts disabler;

    const RenderStyle* newStyle = renderer()->style();

    compositor()->updateLayerCompositingState(this, RenderLayerCompositor::UseChickenEggHacks);
    // FIXME: this compositing logic should be pushed into the compositing code, not here.
    if (needsCompositingLayersRebuiltForClip(oldStyle, newStyle)
        || needsCompositingLayersRebuiltForOverflow(oldStyle, newStyle)
        || needsCompositingLayersRebuiltForFilters(oldStyle, newStyle, didPaintWithFilters)
        || needsCompositingLayersRebuiltForBlending(oldStyle, newStyle)) {
        compositor()->setCompositingLayersNeedRebuild();
    }
}

bool RenderLayer::scrollsOverflow() const
{
    if (RenderLayerScrollableArea* scrollableArea = this->scrollableArea())
        return scrollableArea->scrollsOverflow();

    return false;
}

FilterOperations RenderLayer::computeFilterOperations(const RenderStyle* style)
{
    const FilterOperations& filters = style->filter();
    if (filters.hasReferenceFilter()) {
        for (size_t i = 0; i < filters.size(); ++i) {
            FilterOperation* filterOperation = filters.operations().at(i).get();
            if (filterOperation->type() != FilterOperation::REFERENCE)
                continue;
            ReferenceFilterOperation* referenceOperation = toReferenceFilterOperation(filterOperation);
            // FIXME: Cache the ReferenceFilter if it didn't change.
            RefPtr<ReferenceFilter> referenceFilter = ReferenceFilter::create();
#ifdef BLINK_SCALE_FILTERS_AT_RECORD_TIME
            float zoom = style->effectiveZoom() * WebCore::deviceScaleFactor(renderer()->frame());
#else
            float zoom = style->effectiveZoom();
#endif
            referenceFilter->setAbsoluteTransform(AffineTransform().scale(zoom, zoom));
            referenceFilter->setLastEffect(ReferenceFilterBuilder::build(referenceFilter.get(), renderer(), referenceFilter->sourceGraphic(),
                referenceOperation));
            referenceOperation->setFilter(referenceFilter.release());
        }
    }

    return filters;
}

void RenderLayer::updateOrRemoveFilterClients()
{
    if (!hasFilter()) {
        removeFilterInfoIfNeeded();
        return;
    }

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
            filterInfo->setRenderer(nullptr);

        return;
    }

    RenderLayerFilterInfo* filterInfo = ensureFilterInfo();
    if (!filterInfo->renderer()) {
        RefPtr<FilterEffectRenderer> filterRenderer = FilterEffectRenderer::create();
        filterInfo->setRenderer(filterRenderer.release());

        // We can optimize away code paths in other places if we know that there are no software filters.
        renderer()->document().view()->setHasSoftwareFilters(true);
    }

    // If the filter fails to build, remove it from the layer. It will still attempt to
    // go through regular processing (e.g. compositing), but never apply anything.
    if (!filterInfo->renderer()->build(renderer(), computeFilterOperations(renderer()->style())))
        filterInfo->setRenderer(nullptr);
}

void RenderLayer::filterNeedsRepaint()
{
    {
        DeprecatedScheduleStyleRecalcDuringLayout marker(renderer()->document().lifecycle());
        // It's possible for scheduleLayerUpdate to schedule a style recalc, which
        // is a problem because this function can be called while performing layout.
        // Presumably this represents an illegal data flow of layout or compositing
        // information into the style system.
        toElement(renderer()->node())->scheduleLayerUpdate();
    }

    if (renderer()->view()) {
        if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled() && renderer()->frameView()->isInPerformLayout())
            renderer()->setShouldDoFullRepaintAfterLayout(true);
        else
            renderer()->repaint();
    }
}

void RenderLayer::addLayerHitTestRects(LayerHitTestRects& rects) const
{
    computeSelfHitTestRects(rects);
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->addLayerHitTestRects(rects);
}

void RenderLayer::computeSelfHitTestRects(LayerHitTestRects& rects) const
{
    if (!size().isEmpty()) {
        Vector<LayoutRect> rect;

        if (renderBox() && renderBox()->scrollsOverflow()) {
            // For scrolling layers, rects are taken to be in the space of the contents.
            // We need to include the bounding box of the layer in the space of its parent
            // (eg. for border / scroll bars) and if it's composited then the entire contents
            // as well as they may be on another composited layer. Skip reporting contents
            // for non-composited layers as they'll get projected to the same layer as the
            // bounding box.
            if (compositingState() != NotComposited)
                rect.append(m_scrollableArea->overflowRect());

            rects.set(this, rect);
            if (const RenderLayer* parentLayer = parent()) {
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

DisableCompositingQueryAsserts::DisableCompositingQueryAsserts()
    : m_disabler(gCompositingQueryMode, CompositingQueriesAreAllowed) { }

COMPILE_ASSERT(1 << RenderLayer::ViewportConstrainedNotCompositedReasonBits >= RenderLayer::NumNotCompositedReasons, too_many_viewport_constrained_not_compositing_reasons);

} // namespace WebCore

#ifndef NDEBUG
void showLayerTree(const WebCore::RenderLayer* layer)
{
    if (!layer)
        return;

    if (WebCore::LocalFrame* frame = layer->renderer()->frame()) {
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
