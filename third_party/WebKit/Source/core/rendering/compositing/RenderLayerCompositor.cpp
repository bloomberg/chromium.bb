/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/rendering/compositing/RenderLayerCompositor.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/animation/ActiveAnimations.h"
#include "core/animation/DocumentAnimations.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/NodeList.h"
#include "core/frame/DeprecatedScheduleStyleRecalcDuringCompositingUpdate.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/Chrome.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingConstraints.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderApplet.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/RenderFullScreen.h"
#include "core/rendering/RenderGeometryMap.h"
#include "core/rendering/RenderIFrame.h"
#include "core/rendering/RenderLayerStackingNode.h"
#include "core/rendering/RenderLayerStackingNodeIterator.h"
#include "core/rendering/RenderReplica.h"
#include "core/rendering/RenderVideo.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "platform/OverscrollTheme.h"
#include "platform/TraceEvent.h"
#include "platform/geometry/TransformState.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/Platform.h"
#include "wtf/TemporaryChange.h"

#ifndef NDEBUG
#include "core/rendering/RenderTreeAsText.h"
#endif

namespace WebCore {

using namespace HTMLNames;

class OverlapMapContainer {
public:
    void add(const IntRect& bounds)
    {
        m_layerRects.append(bounds);
        m_boundingBox.unite(bounds);
    }

    bool overlapsLayers(const IntRect& bounds) const
    {
        // Checking with the bounding box will quickly reject cases when
        // layers are created for lists of items going in one direction and
        // never overlap with each other.
        if (!bounds.intersects(m_boundingBox))
            return false;
        for (unsigned i = 0; i < m_layerRects.size(); i++) {
            if (m_layerRects[i].intersects(bounds))
                return true;
        }
        return false;
    }

    void unite(const OverlapMapContainer& otherContainer)
    {
        m_layerRects.appendVector(otherContainer.m_layerRects);
        m_boundingBox.unite(otherContainer.m_boundingBox);
    }
private:
    Vector<IntRect, 64> m_layerRects;
    IntRect m_boundingBox;
};

class RenderLayerCompositor::OverlapMap {
    WTF_MAKE_NONCOPYABLE(OverlapMap);
public:
    OverlapMap()
        : m_geometryMap(UseTransforms)
    {
        // Begin by assuming the root layer will be composited so that there
        // is something on the stack. The root layer should also never get a
        // finishCurrentOverlapTestingContext() call.
        beginNewOverlapTestingContext();
    }

    void add(const RenderLayer* layer, const IntRect& bounds)
    {
        // Layers do not contribute to overlap immediately--instead, they will
        // contribute to overlap as soon as they have been recursively processed
        // and popped off the stack.
        ASSERT(m_overlapStack.size() >= 2);
        m_overlapStack[m_overlapStack.size() - 2].add(bounds);
        m_layers.add(layer);
    }

    bool contains(const RenderLayer* layer)
    {
        return m_layers.contains(layer);
    }

    bool overlapsLayers(const IntRect& bounds) const
    {
        return m_overlapStack.last().overlapsLayers(bounds);
    }

    bool isEmpty()
    {
        return m_layers.isEmpty();
    }

    void beginNewOverlapTestingContext()
    {
        // This effectively creates a new "clean slate" for overlap state.
        // This is used when we know that a subtree or remaining set of
        // siblings does not need to check overlap with things behind it.
        m_overlapStack.append(OverlapMapContainer());
    }

    void finishCurrentOverlapTestingContext()
    {
        // The overlap information on the top of the stack is still necessary
        // for checking overlap of any layers outside this context that may
        // overlap things from inside this context. Therefore, we must merge
        // the information from the top of the stack before popping the stack.
        //
        // FIXME: we may be able to avoid this deep copy by rearranging how
        //        overlapMap state is managed.
        m_overlapStack[m_overlapStack.size() - 2].unite(m_overlapStack.last());
        m_overlapStack.removeLast();
    }

    RenderGeometryMap& geometryMap() { return m_geometryMap; }

private:
    Vector<OverlapMapContainer> m_overlapStack;
    HashSet<const RenderLayer*> m_layers;
    RenderGeometryMap m_geometryMap;
};

struct CompositingRecursionData {
    CompositingRecursionData(RenderLayer* compAncestor, RenderLayer* mostRecentCompositedLayer, bool testOverlap)
        : m_compositingAncestor(compAncestor)
        , m_mostRecentCompositedLayer(mostRecentCompositedLayer)
        , m_subtreeIsCompositing(false)
        , m_hasUnisolatedCompositedBlendingDescendant(false)
        , m_testingOverlap(testOverlap)
#ifndef NDEBUG
        , m_depth(0)
#endif
    {
    }

    CompositingRecursionData(const CompositingRecursionData& other)
        : m_compositingAncestor(other.m_compositingAncestor)
        , m_mostRecentCompositedLayer(other.m_mostRecentCompositedLayer)
        , m_subtreeIsCompositing(other.m_subtreeIsCompositing)
        , m_hasUnisolatedCompositedBlendingDescendant(other.m_hasUnisolatedCompositedBlendingDescendant)
        , m_testingOverlap(other.m_testingOverlap)
#ifndef NDEBUG
        , m_depth(other.m_depth + 1)
#endif
    {
    }

    RenderLayer* m_compositingAncestor;
    RenderLayer* m_mostRecentCompositedLayer; // in paint order regardless of hierarchy.
    bool m_subtreeIsCompositing;
    bool m_hasUnisolatedCompositedBlendingDescendant;
    bool m_testingOverlap;
#ifndef NDEBUG
    int m_depth;
#endif
};

RenderLayerCompositor::RenderLayerCompositor(RenderView* renderView)
    : m_renderView(renderView)
    , m_compositingReasonFinder(*renderView)
    , m_hasAcceleratedCompositing(true)
    , m_showRepaintCounter(false)
    , m_needsToRecomputeCompositingRequirements(false)
    , m_needsToUpdateLayerTreeGeometry(false)
    , m_compositing(false)
    , m_compositingLayersNeedRebuild(false)
    , m_forceCompositingMode(false)
    , m_needsUpdateCompositingRequirementsState(false)
    , m_isTrackingRepaints(false)
    , m_rootLayerAttachment(RootLayerUnattached)
{
    ASSERT(renderView);
}

RenderLayerCompositor::~RenderLayerCompositor()
{
    ASSERT(m_rootLayerAttachment == RootLayerUnattached);
}

void RenderLayerCompositor::enableCompositingMode(bool enable /* = true */)
{
    if (enable != m_compositing) {
        m_compositing = enable;

        if (m_compositing) {
            ensureRootLayer();
            notifyIFramesOfCompositingChange();
        } else
            destroyRootLayer();
    }
}

void RenderLayerCompositor::cacheAcceleratedCompositingFlags()
{
    bool hasAcceleratedCompositing = false;
    bool showRepaintCounter = false;
    bool forceCompositingMode = false;

    if (Settings* settings = m_renderView->document().settings()) {
        hasAcceleratedCompositing = settings->acceleratedCompositingEnabled();

        // We allow the chrome to override the settings, in case the page is rendered
        // on a chrome that doesn't allow accelerated compositing.
        if (hasAcceleratedCompositing) {
            if (page()) {
                m_compositingReasonFinder.updateTriggers();
                hasAcceleratedCompositing = m_compositingReasonFinder.hasTriggers();
            }
        }

        showRepaintCounter = settings->showRepaintCounter();
        forceCompositingMode = settings->forceCompositingMode() && hasAcceleratedCompositing;

        if (forceCompositingMode && !isMainFrame())
            forceCompositingMode = m_compositingReasonFinder.requiresCompositingForScrollableFrame();
    }

    if (hasAcceleratedCompositing != m_hasAcceleratedCompositing || showRepaintCounter != m_showRepaintCounter || forceCompositingMode != m_forceCompositingMode)
        setCompositingLayersNeedRebuild();

    m_hasAcceleratedCompositing = hasAcceleratedCompositing;
    m_showRepaintCounter = showRepaintCounter;
    m_forceCompositingMode = forceCompositingMode;
}

bool RenderLayerCompositor::layerSquashingEnabled() const
{
    if (RuntimeEnabledFeatures::bleedingEdgeFastPathsEnabled())
        return true;
    if (Settings* settings = m_renderView->document().settings())
        return settings->layerSquashingEnabled();

    return false;
}

bool RenderLayerCompositor::canRender3DTransforms() const
{
    return hasAcceleratedCompositing() && m_compositingReasonFinder.has3DTransformTrigger();
}

void RenderLayerCompositor::setCompositingLayersNeedRebuild(bool needRebuild)
{
    // FIXME: crbug,com/332248 ideally this could be merged with setNeedsCompositingUpdate().
    if (inCompositingMode())
        m_compositingLayersNeedRebuild = needRebuild;

    m_renderView->frameView()->scheduleAnimation();
}

void RenderLayerCompositor::updateCompositingRequirementsState()
{
    if (!m_needsUpdateCompositingRequirementsState)
        return;

    TRACE_EVENT0("blink_rendering,comp-scroll", "RenderLayerCompositor::updateCompositingRequirementsState");

    m_needsUpdateCompositingRequirementsState = false;

    if (!rootRenderLayer() || !m_renderView->acceleratedCompositingForOverflowScrollEnabled())
        return;

    for (HashSet<RenderLayer*>::iterator it = m_outOfFlowPositionedLayers.begin(); it != m_outOfFlowPositionedLayers.end(); ++it)
        (*it)->updateHasUnclippedDescendant();

    const FrameView::ScrollableAreaSet* scrollableAreas = m_renderView->frameView()->scrollableAreas();
    if (!scrollableAreas)
        return;

    for (FrameView::ScrollableAreaSet::iterator it = scrollableAreas->begin(); it != scrollableAreas->end(); ++it)
        (*it)->updateNeedsCompositedScrolling();
}

static RenderVideo* findFullscreenVideoRenderer(Document& document)
{
    Element* fullscreenElement = FullscreenElementStack::currentFullScreenElementFrom(document);
    while (fullscreenElement && fullscreenElement->isFrameOwnerElement()) {
        Document* contentDocument = toHTMLFrameOwnerElement(fullscreenElement)->contentDocument();
        if (!contentDocument)
            return 0;
        fullscreenElement = FullscreenElementStack::currentFullScreenElementFrom(*contentDocument);
    }
    if (!fullscreenElement || !fullscreenElement->hasTagName(videoTag))
        return 0;
    RenderObject* renderer = fullscreenElement->renderer();
    if (!renderer)
        return 0;
    return toRenderVideo(renderer);
}

void RenderLayerCompositor::finishCompositingUpdateForFrameTree(LocalFrame* frame)
{
    for (LocalFrame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling())
        finishCompositingUpdateForFrameTree(child);

    // Update compositing for current frame after all descendant frames are updated.
    if (frame && frame->contentRenderer()) {
        RenderLayerCompositor* frameCompositor = frame->contentRenderer()->compositor();
        if (frameCompositor && !frameCompositor->isMainFrame())
            frame->contentRenderer()->compositor()->updateCompositingLayers();
    }
}

void RenderLayerCompositor::setNeedsCompositingUpdate(CompositingUpdateType updateType)
{
    // FIXME: this code was historically part of updateCompositingLayers, and
    // for now is kept totally equivalent to the previous implementation. We
    // should carefully clean up the awkward early-exit semantics, balancing between
    // skipping unnecessary compositing updates and not incorrectly skipping
    // necessary updates.

    // Avoid updating the layers with old values. Compositing layers will be updated after the layout is finished.
    if (m_renderView->needsLayout())
        return;

    if (m_forceCompositingMode && !m_compositing)
        enableCompositingMode(true);

    if (!m_needsToRecomputeCompositingRequirements && !m_compositing)
        return;

    switch (updateType) {
    case CompositingUpdateAfterStyleChange:
    case CompositingUpdateAfterLayout:
        m_needsToRecomputeCompositingRequirements = true;
        break;
    case CompositingUpdateOnScroll:
        m_needsToRecomputeCompositingRequirements = true; // Overlap can change with scrolling, so need to check for hierarchy updates.
        m_needsToUpdateLayerTreeGeometry = true;
        break;
    case CompositingUpdateOnCompositedScroll:
        m_needsToUpdateLayerTreeGeometry = true;
        break;
    }

    m_renderView->frameView()->scheduleAnimation();
}

void RenderLayerCompositor::updateCompositingLayers()
{
    TRACE_EVENT0("blink_rendering", "RenderLayerCompositor::updateCompositingLayers");

    // FIXME: We should carefully clean up the awkward early-exit semantics, balancing
    // between skipping unnecessary compositing updates and not incorrectly skipping
    // necessary updates.

    // Avoid updating the layers with old values. Compositing layers will be updated after the layout is finished.
    // FIXME: Can we assert that we never return here?
    if (m_renderView->needsLayout())
        return;

    lifecycle().advanceTo(DocumentLifecycle::InCompositingUpdate);

    updateCompositingLayersInternal();

    lifecycle().advanceTo(DocumentLifecycle::CompositingClean);

    DocumentAnimations::startPendingAnimations(m_renderView->document());
    DocumentAnimations::dispatchAnimationEventsAsync(m_renderView->document());
    ASSERT(m_renderView->document().lifecycle().state() == DocumentLifecycle::CompositingClean);
}

void RenderLayerCompositor::updateCompositingLayersInternal()
{
    if (isMainFrame() && m_renderView->frameView())
        finishCompositingUpdateForFrameTree(&m_renderView->frameView()->frame());

    if (m_forceCompositingMode && !m_compositing)
        enableCompositingMode(true);

    if (!m_needsToRecomputeCompositingRequirements && !m_compositing)
        return;

    bool needCompositingRequirementsUpdate = m_needsToRecomputeCompositingRequirements;
    bool needHierarchyAndGeometryUpdate = m_compositingLayersNeedRebuild;
    bool needGeometryUpdate = m_needsToUpdateLayerTreeGeometry;
    bool needsToUpdateScrollingCoordinator = scrollingCoordinator() ? scrollingCoordinator()->needsToUpdateAfterCompositingChange() : false;

    if (!needCompositingRequirementsUpdate && !needHierarchyAndGeometryUpdate && !needGeometryUpdate && !needsToUpdateScrollingCoordinator)
        return;

    // Only clear the flags if we're updating the entire hierarchy.
    m_compositingLayersNeedRebuild = false;
    m_needsToUpdateLayerTreeGeometry = false;
    m_needsToRecomputeCompositingRequirements = false;
    RenderLayer* updateRoot = rootRenderLayer();

    if (needCompositingRequirementsUpdate) {
        // Go through the layers in presentation order, so that we can compute which RenderLayers need compositing layers.
        // FIXME: we could maybe do this and the hierarchy udpate in one pass, but the parenting logic would be more complex.
        CompositingRecursionData recursionData(updateRoot, 0, true);
        bool layersChanged = false;
        bool saw3DTransform = false;
        {
            TRACE_EVENT0("blink_rendering", "RenderLayerCompositor::computeCompositingRequirements");
            OverlapMap overlapTestRequestMap;
            // Turn off the overlap map test if bleeding edge features are on.
            OverlapMap* overlapMap = 0;
            if (!RuntimeEnabledFeatures::bleedingEdgeFastPathsEnabled())
                overlapMap = &overlapTestRequestMap;

            // FIXME: Passing these unclippedDescendants down and keeping track
            // of them dynamically, we are requiring a full tree walk. This
            // should be removed as soon as proper overlap testing based on
            // scrolling and animation bounds is implemented (crbug.com/252472).
            Vector<RenderLayer*> unclippedDescendants;
            computeCompositingRequirements(0, updateRoot, overlapMap, recursionData, saw3DTransform, unclippedDescendants);
        }

        {
            TRACE_EVENT0("blink_rendering", "RenderLayerCompositor::assignLayersToBackings");
            assignLayersToBackings(updateRoot, layersChanged);
        }

        {
            TRACE_EVENT0("blink_rendering", "RenderLayerCompositor::updateHasVisibleNonLayerContentLoop");
            const FrameView::ScrollableAreaSet* scrollableAreas = m_renderView->frameView()->scrollableAreas();
            if (scrollableAreas) {
                for (FrameView::ScrollableAreaSet::iterator it = scrollableAreas->begin(); it != scrollableAreas->end(); ++it)
                    (*it)->updateHasVisibleNonLayerContent();
            }
        }

        needHierarchyAndGeometryUpdate |= layersChanged;
    }

    if (needHierarchyAndGeometryUpdate) {
        // Update the hierarchy of the compositing layers.
        Vector<GraphicsLayer*> childList;
        {
            TRACE_EVENT0("blink_rendering", "RenderLayerCompositor::rebuildCompositingLayerTree");
            rebuildCompositingLayerTree(updateRoot, childList, 0);
        }

        // Host the document layer in the RenderView's root layer.
        if (RuntimeEnabledFeatures::overlayFullscreenVideoEnabled() && isMainFrame()) {
            RenderVideo* video = findFullscreenVideoRenderer(m_renderView->document());
            if (video && video->hasCompositedLayerMapping()) {
                childList.clear();
                childList.append(video->compositedLayerMapping()->mainGraphicsLayer());
            }
        }

        if (childList.isEmpty())
            destroyRootLayer();
        else
            m_rootContentLayer->setChildren(childList);
    } else if (needGeometryUpdate) {
        // We just need to do a geometry update. This is only used for position:fixed scrolling;
        // most of the time, geometry is updated via RenderLayer::styleChanged().
        TRACE_EVENT0("blink_rendering", "RenderLayerCompositor::updateLayerTreeGeometry");
        updateLayerTreeGeometry(updateRoot);
    }

    ASSERT(updateRoot || !m_compositingLayersNeedRebuild);

    if (!hasAcceleratedCompositing())
        enableCompositingMode(false);

    // The scrolling coordinator may realize that it needs updating while compositing was being updated in this function.
    needsToUpdateScrollingCoordinator |= scrollingCoordinator() ? scrollingCoordinator()->needsToUpdateAfterCompositingChange() : false;
    if (needsToUpdateScrollingCoordinator && isMainFrame() && scrollingCoordinator() && inCompositingMode())
        scrollingCoordinator()->updateAfterCompositingChange();

    // Inform the inspector that the layer tree has changed.
    if (isMainFrame())
        InspectorInstrumentation::layerTreeDidChange(page());
}

static bool requiresCompositing(CompositingReasons reasons)
{
    // Any reasons other than overlap or assumed overlap will require the layer to be separately compositing.
    return reasons & ~CompositingReasonComboSquashableReasons;
}

static bool requiresSquashing(CompositingReasons reasons)
{
    // If the layer has overlap or assumed overlap, but no other reasons, then it should be squashed.
    return !requiresCompositing(reasons) && (reasons & CompositingReasonComboSquashableReasons);
}

static bool requiresCompositingOrSquashing(CompositingReasons reasons)
{
#ifndef NDEBUG
    bool fastAnswer = reasons != CompositingReasonNone;
    bool slowAnswer = requiresCompositing(reasons) || requiresSquashing(reasons);
    ASSERT(fastAnswer == slowAnswer);
#endif
    return reasons != CompositingReasonNone;
}

void RenderLayerCompositor::addOutOfFlowPositionedLayer(RenderLayer* layer)
{
    m_outOfFlowPositionedLayers.add(layer);
}

void RenderLayerCompositor::removeOutOfFlowPositionedLayer(RenderLayer* layer)
{
    m_outOfFlowPositionedLayers.remove(layer);
}

bool RenderLayerCompositor::allocateOrClearCompositedLayerMapping(RenderLayer* layer)
{
    bool compositedLayerMappingChanged = false;
    bool nonCompositedReasonChanged = updateLayerIfViewportConstrained(layer);
    CompositingStateTransitionType compositedLayerUpdate = computeCompositedLayerUpdate(layer);

    // FIXME: It would be nice to directly use the layer's compositing reason,
    // but allocateOrClearCompositedLayerMapping also gets called without having updated compositing
    // requirements fully.
    switch (compositedLayerUpdate) {
    case AllocateOwnCompositedLayerMapping:
        ASSERT(!layer->hasCompositedLayerMapping());
        enableCompositingMode();

        // If we need to repaint, do so before allocating the compositedLayerMapping
        repaintOnCompositingChange(layer);
        layer->ensureCompositedLayerMapping();
        compositedLayerMappingChanged = true;

        // At this time, the ScrollingCooridnator only supports the top-level frame.
        if (layer->isRootLayer() && isMainFrame()) {
            if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->frameViewRootLayerDidChange(m_renderView->frameView());
        }

        // If this layer was previously squashed, we need to remove its reference to a groupedMapping right away, so
        // that computing repaint rects will know the layer's correct compositingState.
        // FIXME: do we need to also remove the layer from it's location in the squashing list of its groupedMapping?
        // Need to create a test where a squashed layer pops into compositing. And also to cover all other
        // sorts of compositingState transitions.
        layer->setLostGroupedMapping(false);
        layer->setGroupedMapping(0);

        // FIXME: it seems premature to compute this before all compositing state has been updated?
        // This layer and all of its descendants have cached repaints rects that are relative to
        // the repaint container, so change when compositing changes; we need to update them here.
        if (layer->parent())
            layer->repainter().computeRepaintRectsIncludingDescendants();

        break;
    case RemoveOwnCompositedLayerMapping:
    // PutInSquashingLayer means you might have to remove the composited layer mapping first.
    case PutInSquashingLayer:
        if (layer->hasCompositedLayerMapping()) {
            // If we're removing the compositedLayerMapping from a reflection, clear the source GraphicsLayer's pointer to
            // its replica GraphicsLayer. In practice this should never happen because reflectee and reflection
            // are both either composited, or not composited.
            if (layer->isReflection()) {
                RenderLayer* sourceLayer = toRenderLayerModelObject(layer->renderer()->parent())->layer();
                if (sourceLayer->hasCompositedLayerMapping()) {
                    ASSERT(sourceLayer->compositedLayerMapping()->mainGraphicsLayer()->replicaLayer() == layer->compositedLayerMapping()->mainGraphicsLayer());
                    sourceLayer->compositedLayerMapping()->mainGraphicsLayer()->setReplicatedByLayer(0);
                }
            }

            removeViewportConstrainedLayer(layer);

            layer->clearCompositedLayerMapping();
            compositedLayerMappingChanged = true;

            // This layer and all of its descendants have cached repaints rects that are relative to
            // the repaint container, so change when compositing changes; we need to update them here.
            layer->repainter().computeRepaintRectsIncludingDescendants();

            // If we need to repaint, do so now that we've removed the compositedLayerMapping
            repaintOnCompositingChange(layer);
        }

        break;
    case RemoveFromSquashingLayer:
    case NoCompositingStateChange:
        // Do nothing.
        break;
    }

    if (layer->hasCompositedLayerMapping() && layer->compositedLayerMapping()->updateRequiresOwnBackingStoreForIntrinsicReasons())
        compositedLayerMappingChanged = true;

    if (compositedLayerMappingChanged && layer->renderer()->isRenderPart()) {
        RenderLayerCompositor* innerCompositor = frameContentsCompositor(toRenderPart(layer->renderer()));
        if (innerCompositor && innerCompositor->inCompositingMode())
            innerCompositor->updateRootLayerAttachment();
    }

    if (compositedLayerMappingChanged)
        layer->clipper().clearClipRectsIncludingDescendants(PaintingClipRects);

    // If a fixed position layer gained/lost a compositedLayerMapping or the reason not compositing it changed,
    // the scrolling coordinator needs to recalculate whether it can do fast scrolling.
    if (compositedLayerMappingChanged || nonCompositedReasonChanged) {
        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->frameViewFixedObjectsDidChange(m_renderView->frameView());
    }

    return compositedLayerMappingChanged || nonCompositedReasonChanged;
}

static IntPoint computeOffsetFromAbsolute(RenderLayer* layer)
{
    TransformState transformState(TransformState::ApplyTransformDirection, FloatPoint());
    layer->renderer()->mapLocalToContainer(0, transformState, ApplyContainerFlip);
    transformState.flatten();
    return roundedIntPoint(transformState.lastPlanarPoint());
}

bool RenderLayerCompositor::updateSquashingAssignment(RenderLayer* layer, SquashingState& squashingState)
{
    CompositingStateTransitionType compositedLayerUpdate = computeCompositedLayerUpdate(layer);

    // NOTE: In the future as we generalize this, the background of this layer may need to be assigned to a different backing than
    // the squashed RenderLayer's own primary contents. This would happen when we have a composited negative z-index element that needs
    // to paint on top of the background, but below the layer's main contents. For now, because we always composite layers
    // when they have a composited negative z-index child, such layers will never need squashing so it is not yet an issue.
    if (compositedLayerUpdate == PutInSquashingLayer) {
        // A layer that is squashed with other layers cannot have its own CompositedLayerMapping.
        ASSERT(!layer->hasCompositedLayerMapping());
        ASSERT(squashingState.hasMostRecentMapping);

        IntPoint offsetFromAbsoluteForSquashedLayer = computeOffsetFromAbsolute(layer);

        IntSize offsetFromSquashingCLM(offsetFromAbsoluteForSquashedLayer.x() - squashingState.offsetFromAbsoluteForSquashingCLM.x(),
            offsetFromAbsoluteForSquashedLayer.y() - squashingState.offsetFromAbsoluteForSquashingCLM.y());

        bool changedSquashingLayer =
            squashingState.mostRecentMapping->updateSquashingLayerAssignment(layer, offsetFromSquashingCLM, squashingState.nextSquashedLayerIndex);
        squashingState.nextSquashedLayerIndex++;

        if (!changedSquashingLayer)
            return true;

        layer->clipper().clearClipRectsIncludingDescendants();

        // If we need to repaint, do so before allocating the layer to the squashing layer.
        repaintOnCompositingChange(layer);

        // FIXME: it seems premature to compute this before all compositing state has been updated?
        // This layer and all of its descendants have cached repaints rects that are relative to
        // the repaint container, so change when compositing changes; we need to update them here.

        // FIXME: what's up with parent()?
        if (layer->parent())
            layer->repainter().computeRepaintRectsIncludingDescendants();

        return true;
    } else if (compositedLayerUpdate == RemoveFromSquashingLayer) {
        layer->setGroupedMapping(0);

        // This layer and all of its descendants have cached repaints rects that are relative to
        // the repaint container, so change when compositing changes; we need to update them here.
        layer->repainter().computeRepaintRectsIncludingDescendants();

        // If we need to repaint, do so now that we've removed it from a squashed layer
        repaintOnCompositingChange(layer);

        layer->setLostGroupedMapping(false);
        return true;
    }
    return false;
}

bool RenderLayerCompositor::updateLayerIfViewportConstrained(RenderLayer* layer)
{
    RenderLayer::ViewportConstrainedNotCompositedReason viewportConstrainedNotCompositedReason = RenderLayer::NoNotCompositedReason;
    m_compositingReasonFinder.requiresCompositingForPosition(layer->renderer(), layer, &viewportConstrainedNotCompositedReason, &m_needsToRecomputeCompositingRequirements);

    if (layer->viewportConstrainedNotCompositedReason() != viewportConstrainedNotCompositedReason) {
        ASSERT(layer->renderer()->style()->position() == FixedPosition);

        layer->setViewportConstrainedNotCompositedReason(viewportConstrainedNotCompositedReason);
        return true;
    }
    return false;
}

RenderLayerCompositor::CompositingStateTransitionType RenderLayerCompositor::computeCompositedLayerUpdate(const RenderLayer* layer)
{
    CompositingStateTransitionType update = NoCompositingStateChange;
    if (needsOwnBacking(layer)) {
        if (!layer->hasCompositedLayerMapping()) {
            update = AllocateOwnCompositedLayerMapping;
        }
    } else {
        if (layer->hasCompositedLayerMapping())
            update = RemoveOwnCompositedLayerMapping;

        if (layerSquashingEnabled()) {
            if (requiresSquashing(layer->compositingReasons())) {
                // We can't compute at this time whether the squashing layer update is a no-op,
                // since that requires walking the render layer tree.
                update = PutInSquashingLayer;
            } else if (layer->groupedMapping() || layer->lostGroupedMapping()) {
                update = RemoveFromSquashingLayer;
            }
        }
    }
    return update;
}

// These are temporary hacks to work around chicken-egg issues while we continue to refactor the compositing code.
// See crbug.com/339892 for a list of tests that fail if this method is removed.
void RenderLayerCompositor::applyUpdateLayerCompositingStateChickenEggHacks(RenderLayer* layer, CompositingStateTransitionType compositedLayerUpdate)
{
    // See if we need content or clipping layers. Methods called here should assume
    // that the compositing state of descendant layers has not been updated yet.
    if (layer->hasCompositedLayerMapping() && layer->compositedLayerMapping()->updateGraphicsLayerConfiguration()) {
        setCompositingLayersNeedRebuild();
    } else if (compositedLayerUpdate == NoCompositingStateChange) {
        if (layer->compositingState() == PaintsIntoOwnBacking || layer->compositingState() == HasOwnBackingButPaintsIntoAncestor)
            setCompositingLayersNeedRebuild();
    }

    if (compositedLayerUpdate != NoCompositingStateChange)
        allocateOrClearCompositedLayerMapping(layer);
}

void RenderLayerCompositor::updateLayerCompositingState(RenderLayer* layer, UpdateLayerCompositingStateOptions options)
{
    updateDirectCompositingReasons(layer);
    CompositingStateTransitionType compositedLayerUpdate = computeCompositedLayerUpdate(layer);

    if (compositedLayerUpdate != NoCompositingStateChange) {
        setCompositingLayersNeedRebuild();
        setNeedsToRecomputeCompositingRequirements();
    }

    if (options == UseChickenEggHacks)
        applyUpdateLayerCompositingStateChickenEggHacks(layer, compositedLayerUpdate);
}

void RenderLayerCompositor::repaintOnCompositingChange(RenderLayer* layer)
{
    // If the renderer is not attached yet, no need to repaint.
    if (layer->renderer() != m_renderView && !layer->renderer()->parent())
        return;

    RenderLayerModelObject* repaintContainer = layer->renderer()->containerForRepaint();
    if (!repaintContainer)
        repaintContainer = m_renderView;

    layer->repainter().repaintIncludingNonCompositingDescendants(repaintContainer);
}

// This method assumes that layout is up-to-date, unlike repaintOnCompositingChange().
void RenderLayerCompositor::repaintInCompositedAncestor(RenderLayer* layer, const LayoutRect& rect)
{
    RenderLayer* compositedAncestor = layer->enclosingCompositingLayerForRepaint(ExcludeSelf);
    if (compositedAncestor) {
        // FIXME: make sure repaintRect is computed correctly for squashed scenario
        LayoutPoint offset;
        layer->convertToLayerCoords(compositedAncestor, offset);

        LayoutRect repaintRect = rect;
        repaintRect.moveBy(offset);

        if (compositedAncestor->compositingState() == PaintsIntoOwnBacking) {
            compositedAncestor->repainter().setBackingNeedsRepaintInRect(repaintRect);
        } else if (compositedAncestor->compositingState() == PaintsIntoGroupedBacking) {
            // FIXME: Need to perform the correct coordinate conversion for repaintRect here, including transforms
            compositedAncestor->groupedMapping()->squashingLayer()->setNeedsDisplayInRect(repaintRect);
        } else {
            ASSERT_NOT_REACHED();
        }
    }
}

// The bounds of the GraphicsLayer created for a compositing layer is the union of the bounds of all the descendant
// RenderLayers that are rendered by the composited RenderLayer.
LayoutRect RenderLayerCompositor::calculateCompositedBounds(const RenderLayer* layer, const RenderLayer* ancestorLayer) const
{
    if (!canBeComposited(layer))
        return LayoutRect();

    RenderLayer::CalculateLayerBoundsFlags flags = RenderLayer::DefaultCalculateLayerBoundsFlags | RenderLayer::ExcludeHiddenDescendants | RenderLayer::DontConstrainForMask;
#if HAVE(COMPOSITOR_FILTER_OUTSETS)
    // If the compositor computes its own filter outsets, don't include them in the composited bounds.
    if (!layer->paintsWithFilters())
        flags &= ~RenderLayer::IncludeLayerFilterOutsets;
#endif
    return layer->calculateLayerBounds(ancestorLayer, 0, flags);
}

void RenderLayerCompositor::layerWasAdded(RenderLayer* /*parent*/, RenderLayer* /*child*/)
{
    setCompositingLayersNeedRebuild();
}

void RenderLayerCompositor::layerWillBeRemoved(RenderLayer* parent, RenderLayer* child)
{
    if (!child->hasCompositedLayerMapping() || parent->renderer()->documentBeingDestroyed())
        return;

    removeViewportConstrainedLayer(child);

    {
        // FIXME: This is called from within RenderLayer::removeChild, which is called from RenderObject::RemoveChild.
        // There's no guarantee that compositor state is up to date.
        DisableCompositingQueryAsserts disabler;
        repaintInCompositedAncestor(child, child->compositedLayerMapping()->compositedBounds());
    }

    setCompositingParent(child, 0);
    setCompositingLayersNeedRebuild();
}

RenderLayer* RenderLayerCompositor::enclosingNonStackingClippingLayer(const RenderLayer* layer) const
{
    for (RenderLayer* curr = layer->parent(); curr != 0; curr = curr->parent()) {
        if (curr->stackingNode()->isStackingContainer())
            return 0;

        if (curr->renderer()->hasClipOrOverflowClip())
            return curr;
    }
    return 0;
}

void RenderLayerCompositor::addToOverlapMap(OverlapMap& overlapMap, RenderLayer* layer, IntRect& layerBounds, bool& boundsComputed)
{
    if (layer->isRootLayer())
        return;

    if (!boundsComputed) {
        // FIXME: If this layer's overlap bounds include its children, we don't need to add its
        // children's bounds to the overlap map.
        layerBounds = enclosingIntRect(overlapMap.geometryMap().absoluteRect(layer->overlapBounds()));
        // Empty rects never intersect, but we need them to for the purposes of overlap testing.
        if (layerBounds.isEmpty())
            layerBounds.setSize(IntSize(1, 1));
        boundsComputed = true;
    }

    IntRect clipRect = pixelSnappedIntRect(layer->clipper().backgroundClipRect(ClipRectsContext(rootRenderLayer(), AbsoluteClipRects)).rect());
    clipRect.intersect(layerBounds);
    overlapMap.add(layer, clipRect);
}

void RenderLayerCompositor::addToOverlapMapRecursive(OverlapMap& overlapMap, RenderLayer* layer, RenderLayer* ancestorLayer)
{
    if (!canBeComposited(layer) || overlapMap.contains(layer))
        return;

    // A null ancestorLayer is an indication that 'layer' has already been pushed.
    if (ancestorLayer)
        overlapMap.geometryMap().pushMappingsToAncestor(layer, ancestorLayer);

    IntRect bounds;
    bool haveComputedBounds = false;
    addToOverlapMap(overlapMap, layer, bounds, haveComputedBounds);

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer->stackingNode());
#endif

    RenderLayerStackingNodeIterator iterator(*layer->stackingNode(), AllChildren);
    while (RenderLayerStackingNode* curNode = iterator.next())
        addToOverlapMapRecursive(overlapMap, curNode->layer(), layer);

    if (ancestorLayer)
        overlapMap.geometryMap().popMappingsToAncestor(ancestorLayer);
}

//  Recurse through the layers in z-index and overflow order (which is equivalent to painting order)
//  For the z-order children of a compositing layer:
//      If a child layers has a compositing layer, then all subsequent layers must
//      be compositing in order to render above that layer.
//
//      If a child in the negative z-order list is compositing, then the layer itself
//      must be compositing so that its contents render over that child.
//      This implies that its positive z-index children must also be compositing.
//
void RenderLayerCompositor::computeCompositingRequirements(RenderLayer* ancestorLayer, RenderLayer* layer, OverlapMap* overlapMap, CompositingRecursionData& currentRecursionData, bool& descendantHas3DTransform, Vector<RenderLayer*>& unclippedDescendants)
{
    layer->stackingNode()->updateLayerListsIfNeeded();

    if (overlapMap)
        overlapMap->geometryMap().pushMappingsToAncestor(layer, ancestorLayer);

    // Clear the flag
    layer->setHasCompositingDescendant(false);
    layer->setHasNonCompositedChild(false);

    // Start by assuming this layer will not need to composite.
    CompositingReasons reasonsToComposite = CompositingReasonNone;

    // First accumulate the straightforward compositing reasons.
    CompositingReasons directReasons = m_compositingReasonFinder.directReasons(layer, &m_needsToRecomputeCompositingRequirements);

    // Video is special. It's the only RenderLayer type that can both have
    // RenderLayer children and whose children can't use its backing to render
    // into. These children (the controls) always need to be promoted into their
    // own layers to draw on top of the accelerated video.
    if (currentRecursionData.m_compositingAncestor && currentRecursionData.m_compositingAncestor->renderer()->isVideo())
        directReasons |= CompositingReasonVideoOverlay;

    if (canBeComposited(layer))
        reasonsToComposite |= directReasons;

    // Next, accumulate reasons related to overlap.
    // If overlap testing is used, this reason will be overridden. If overlap testing is not
    // used, we must assume we overlap if there is anything composited behind us in paint-order.
    CompositingReasons overlapCompositingReason = currentRecursionData.m_subtreeIsCompositing ? CompositingReasonAssumedOverlap : CompositingReasonNone;

    if (m_renderView->compositorDrivenAcceleratedScrollingEnabled()) {
        Vector<size_t> unclippedDescendantsToRemove;
        for (size_t i = 0; i < unclippedDescendants.size(); i++) {
            RenderLayer* unclippedDescendant = unclippedDescendants.at(i);
            // If we've reached the containing block of one of the unclipped
            // descendants, that element is no longer relevant to whether or not we
            // should opt in. Unfortunately we can't easily remove from the list
            // while we're iterating, so we have to store it for later removal.
            if (unclippedDescendant->renderer()->containingBlock() == layer->renderer()) {
                unclippedDescendantsToRemove.append(i);
                continue;
            }
            if (layer->scrollsWithRespectTo(unclippedDescendant))
                reasonsToComposite |= CompositingReasonAssumedOverlap;
        }

        // Remove irrelevant unclipped descendants in reverse order so our stored
        // indices remain valid.
        for (size_t i = 0; i < unclippedDescendantsToRemove.size(); i++)
            unclippedDescendants.remove(unclippedDescendantsToRemove.at(unclippedDescendantsToRemove.size() - i - 1));

        if (reasonsToComposite & CompositingReasonOutOfFlowClipping)
            unclippedDescendants.append(layer);
    }

    bool haveComputedBounds = false;
    IntRect absBounds;
    // If we know for sure the layer is going to be composited, don't bother looking it up in the overlap map.
    if (overlapMap && !overlapMap->isEmpty() && currentRecursionData.m_testingOverlap && !requiresCompositingOrSquashing(directReasons)) {
        // If we're testing for overlap, we only need to composite if we overlap something that is already composited.
        absBounds = enclosingIntRect(overlapMap->geometryMap().absoluteRect(layer->overlapBounds()));

        // Empty rects never intersect, but we need them to for the purposes of overlap testing.
        if (absBounds.isEmpty())
            absBounds.setSize(IntSize(1, 1));
        haveComputedBounds = true;
        overlapCompositingReason = overlapMap->overlapsLayers(absBounds) ? CompositingReasonOverlap : CompositingReasonNone;
    }

    reasonsToComposite |= overlapCompositingReason;

    // If the layer is squashable, but would scroll differently than the
    // most recent backing that it would squash onto, then don't squash it.
    // Note that this happens before we know all possible compositing reasons
    // for this layer, but it's OK because we're just forcing the layer conservatively
    // to be separately composited rather than squashed, anyway.
    if (currentRecursionData.m_mostRecentCompositedLayer && requiresSquashing(reasonsToComposite)
        && layer->scrollsWithRespectTo(currentRecursionData.m_mostRecentCompositedLayer))
        reasonsToComposite |= CompositingReasonOverlapsWithoutSquashingTarget;

    // The children of this layer don't need to composite, unless there is
    // a compositing layer among them, so start by inheriting the compositing
    // ancestor with m_subtreeIsCompositing set to false.
    CompositingRecursionData childRecursionData(currentRecursionData);
    childRecursionData.m_subtreeIsCompositing = false;

    bool willBeCompositedOrSquashed = canBeComposited(layer) && requiresCompositingOrSquashing(reasonsToComposite);
    if (willBeCompositedOrSquashed) {
        // Tell the parent it has compositing descendants.
        currentRecursionData.m_subtreeIsCompositing = true;
        // This layer now acts as the ancestor for kids.
        childRecursionData.m_compositingAncestor = layer;

        // Here we know that all children and the layer's own contents can blindly paint into
        // this layer's backing, until a descendant is composited. So, we don't need to check
        // for overlap with anything behind this layer.
        if (overlapMap)
            overlapMap->beginNewOverlapTestingContext();
        // This layer is going to be composited, so children can safely ignore the fact that there's an
        // animation running behind this layer, meaning they can rely on the overlap map testing again.
        childRecursionData.m_testingOverlap = true;
    }

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer->stackingNode());
#endif

    bool anyDescendantHas3DTransform = false;
    bool willHaveForegroundLayer = false;

    if (layer->stackingNode()->isStackingContainer()) {
        RenderLayerStackingNodeIterator iterator(*layer->stackingNode(), NegativeZOrderChildren);
        while (RenderLayerStackingNode* curNode = iterator.next()) {
            computeCompositingRequirements(layer, curNode->layer(), overlapMap, childRecursionData, anyDescendantHas3DTransform, unclippedDescendants);

            // If we have to make a layer for this child, make one now so we can have a contents layer
            // (since we need to ensure that the -ve z-order child renders underneath our contents).
            if (childRecursionData.m_subtreeIsCompositing) {
                reasonsToComposite |= CompositingReasonNegativeZIndexChildren;

                if (!willBeCompositedOrSquashed) {
                    // make layer compositing
                    childRecursionData.m_compositingAncestor = layer;
                    if (overlapMap)
                        overlapMap->beginNewOverlapTestingContext();
                    willBeCompositedOrSquashed = true;
                    willHaveForegroundLayer = true;

                    // FIXME: temporary solution for the first negative z-index composited child:
                    //        re-compute the absBounds for the child so that we can add the
                    //        negative z-index child's bounds to the new overlap context.
                    if (overlapMap) {
                        overlapMap->geometryMap().pushMappingsToAncestor(curNode->layer(), layer);
                        IntRect childAbsBounds = enclosingIntRect(overlapMap->geometryMap().absoluteRect(curNode->layer()->overlapBounds()));
                        bool boundsComputed = true;
                        overlapMap->beginNewOverlapTestingContext();
                        addToOverlapMap(*overlapMap, curNode->layer(), childAbsBounds, boundsComputed);
                        overlapMap->finishCurrentOverlapTestingContext();
                        overlapMap->geometryMap().popMappingsToAncestor(layer);
                    }
                }
            }
        }
    }

    if (overlapMap && willHaveForegroundLayer) {
        ASSERT(willBeCompositedOrSquashed);
        // A foreground layer effectively is a new backing for all subsequent children, so
        // we don't need to test for overlap with anything behind this. So, we can finish
        // the previous context that was accumulating rects for the negative z-index
        // children, and start with a fresh new empty context.
        overlapMap->finishCurrentOverlapTestingContext();
        overlapMap->beginNewOverlapTestingContext();
        // This layer is going to be composited, so children can safely ignore the fact that there's an
        // animation running behind this layer, meaning they can rely on the overlap map testing again
        childRecursionData.m_testingOverlap = true;
    }

    if (requiresCompositing(reasonsToComposite)) {
        currentRecursionData.m_mostRecentCompositedLayer = layer;
        childRecursionData.m_mostRecentCompositedLayer = layer;
    }

    RenderLayerStackingNodeIterator iterator(*layer->stackingNode(), NormalFlowChildren | PositiveZOrderChildren);
    while (RenderLayerStackingNode* curNode = iterator.next())
        computeCompositingRequirements(layer, curNode->layer(), overlapMap, childRecursionData, anyDescendantHas3DTransform, unclippedDescendants);

    currentRecursionData.m_mostRecentCompositedLayer = childRecursionData.m_mostRecentCompositedLayer;

    // Now that the subtree has been traversed, we can check for compositing reasons that depended on the state of the subtree.

    // If we entered compositing mode during the recursion, the root will also need to be composited (as long as accelerated compositing is enabled).
    if (layer->isRootLayer()) {
        if (inCompositingMode() && m_hasAcceleratedCompositing)
            willBeCompositedOrSquashed = true;
    }

    // All layers (even ones that aren't being composited) need to get added to
    // the overlap map. Layers that are not separately composited will paint into their
    // compositing ancestor's backing, and so are still considered for overlap.
    if (overlapMap && childRecursionData.m_compositingAncestor && !childRecursionData.m_compositingAncestor->isRootLayer())
        addToOverlapMap(*overlapMap, layer, absBounds, haveComputedBounds);

    if (layer->stackingNode()->isStackingContext()) {
        layer->setShouldIsolateCompositedDescendants(childRecursionData.m_hasUnisolatedCompositedBlendingDescendant);
    } else {
        layer->setShouldIsolateCompositedDescendants(false);
        currentRecursionData.m_hasUnisolatedCompositedBlendingDescendant = childRecursionData.m_hasUnisolatedCompositedBlendingDescendant;
    }

    // Now check for reasons to become composited that depend on the state of descendant layers.
    CompositingReasons subtreeCompositingReasons = subtreeReasonsForCompositing(layer->renderer(), childRecursionData.m_subtreeIsCompositing, anyDescendantHas3DTransform);
    reasonsToComposite |= subtreeCompositingReasons;
    if (!willBeCompositedOrSquashed && canBeComposited(layer) && requiresCompositingOrSquashing(subtreeCompositingReasons)) {
        childRecursionData.m_compositingAncestor = layer;
        if (overlapMap) {
            // FIXME: this context push is effectively a no-op but needs to exist for
            // now, because the code is designed to push overlap information to the
            // second-from-top context of the stack.
            overlapMap->beginNewOverlapTestingContext();
            addToOverlapMapRecursive(*overlapMap, layer);
        }
        willBeCompositedOrSquashed = true;
    }

    // If the original layer is composited, the reflection needs to be, too.
    if (layer->reflectionInfo()) {
        // FIXME: Shouldn't we call computeCompositingRequirements to handle a reflection overlapping with another renderer?
        CompositingReasons reflectionCompositingReason = willBeCompositedOrSquashed ? CompositingReasonReflectionOfCompositedParent : CompositingReasonNone;
        layer->reflectionInfo()->reflectionLayer()->setCompositingReasons(layer->reflectionInfo()->reflectionLayer()->compositingReasons() | reflectionCompositingReason);
    }

    // Subsequent layers in the parent's stacking context may also need to composite.
    if (childRecursionData.m_subtreeIsCompositing)
        currentRecursionData.m_subtreeIsCompositing = true;

    if (willBeCompositedOrSquashed && layer->blendInfo().hasBlendMode())
        currentRecursionData.m_hasUnisolatedCompositedBlendingDescendant = true;

    // Set the flag to say that this SC has compositing children.
    layer->setHasCompositingDescendant(childRecursionData.m_subtreeIsCompositing);

    // Turn overlap testing off for later layers if it's already off, or if we have an animating transform.
    // Note that if the layer clips its descendants, there's no reason to propagate the child animation to the parent layers. That's because
    // we know for sure the animation is contained inside the clipping rectangle, which is already added to the overlap map.
    bool isCompositedClippingLayer = canBeComposited(layer) && (reasonsToComposite & CompositingReasonClipsCompositingDescendants);
    if ((!childRecursionData.m_testingOverlap && !isCompositedClippingLayer) || isRunningAcceleratedTransformAnimation(layer->renderer()))
        currentRecursionData.m_testingOverlap = false;

    if (overlapMap && childRecursionData.m_compositingAncestor == layer && !layer->isRootLayer())
        overlapMap->finishCurrentOverlapTestingContext();

    if (layer->isRootLayer()) {
        // The root layer needs to be composited if anything else in the tree is composited.
        // Otherwise, we can disable compositing entirely.
        if (childRecursionData.m_subtreeIsCompositing || requiresCompositingOrSquashing(reasonsToComposite) || m_forceCompositingMode) {
            willBeCompositedOrSquashed = true;
            reasonsToComposite |= CompositingReasonRoot;
        } else {
            enableCompositingMode(false);
            willBeCompositedOrSquashed = false;
            reasonsToComposite = CompositingReasonNone;
        }
    }

    if (requiresCompositing(reasonsToComposite))
        currentRecursionData.m_mostRecentCompositedLayer = layer;

    // At this point we have finished collecting all reasons to composite this layer.
    layer->setCompositingReasons(reasonsToComposite);

    if (!willBeCompositedOrSquashed && layer->parent())
        layer->parent()->setHasNonCompositedChild(true);

    descendantHas3DTransform |= anyDescendantHas3DTransform || layer->has3DTransform();

    if (overlapMap)
        overlapMap->geometryMap().popMappingsToAncestor(ancestorLayer);
}

void RenderLayerCompositor::SquashingState::updateSquashingStateForNewMapping(CompositedLayerMappingPtr newCompositedLayerMapping, bool hasNewCompositedLayerMapping, IntPoint newOffsetFromAbsoluteForSquashingCLM)
{
    // The most recent backing is done accumulating any more squashing layers.
    if (hasMostRecentMapping)
        mostRecentMapping->finishAccumulatingSquashingLayers(nextSquashedLayerIndex);

    nextSquashedLayerIndex = 0;
    mostRecentMapping = newCompositedLayerMapping;
    hasMostRecentMapping = hasNewCompositedLayerMapping;
    offsetFromAbsoluteForSquashingCLM = newOffsetFromAbsoluteForSquashingCLM;
}

void RenderLayerCompositor::assignLayersToBackings(RenderLayer* updateRoot, bool& layersChanged)
{
    SquashingState squashingState;
    assignLayersToBackingsInternal(updateRoot, squashingState, layersChanged);
    if (squashingState.hasMostRecentMapping)
        squashingState.mostRecentMapping->finishAccumulatingSquashingLayers(squashingState.nextSquashedLayerIndex);
}

void RenderLayerCompositor::assignLayersToBackingsForReflectionLayer(RenderLayer* reflectionLayer, bool& layersChanged)
{
    if (computeCompositedLayerUpdate(reflectionLayer) != NoCompositingStateChange) {
        layersChanged = true;
        allocateOrClearCompositedLayerMapping(reflectionLayer);
    }
    updateDirectCompositingReasons(reflectionLayer);
    if (reflectionLayer->hasCompositedLayerMapping())
        reflectionLayer->compositedLayerMapping()->updateGraphicsLayerConfiguration();
}

void RenderLayerCompositor::assignLayersToBackingsInternal(RenderLayer* layer, SquashingState& squashingState, bool& layersChanged)
{
    if (allocateOrClearCompositedLayerMapping(layer))
        layersChanged = true;

    // FIXME: special-casing reflection layers here is not right.
    if (layer->reflectionInfo())
        assignLayersToBackingsForReflectionLayer(layer->reflectionInfo()->reflectionLayer(), layersChanged);

    // Add this layer to a squashing backing if needed.
    if (layerSquashingEnabled()) {
        if (updateSquashingAssignment(layer, squashingState))
            layersChanged = true;
    }

    if (layer->stackingNode()->isStackingContainer()) {
        RenderLayerStackingNodeIterator iterator(*layer->stackingNode(), NegativeZOrderChildren);
        while (RenderLayerStackingNode* curNode = iterator.next())
            assignLayersToBackingsInternal(curNode->layer(), squashingState, layersChanged);
    }

    if (layerSquashingEnabled()) {
        // At this point, if the layer is to be "separately" composited, then its backing becomes the most recent in paint-order.
        if (layer->compositingState() == PaintsIntoOwnBacking || layer->compositingState() == HasOwnBackingButPaintsIntoAncestor) {
            ASSERT(!requiresSquashing(layer->compositingReasons()));
            IntPoint offsetFromAbsoluteForSquashingCLM = computeOffsetFromAbsolute(layer);
            squashingState.updateSquashingStateForNewMapping(layer->compositedLayerMapping(), layer->hasCompositedLayerMapping(), offsetFromAbsoluteForSquashingCLM);
        }
    }

    RenderLayerStackingNodeIterator iterator(*layer->stackingNode(), NormalFlowChildren | PositiveZOrderChildren);
    while (RenderLayerStackingNode* curNode = iterator.next())
        assignLayersToBackingsInternal(curNode->layer(), squashingState, layersChanged);
}

void RenderLayerCompositor::setCompositingParent(RenderLayer* childLayer, RenderLayer* parentLayer)
{
    ASSERT(!parentLayer || childLayer->ancestorCompositingLayer() == parentLayer);
    ASSERT(childLayer->hasCompositedLayerMapping());

    // It's possible to be called with a parent that isn't yet composited when we're doing
    // partial updates as required by painting or hit testing. Just bail in that case;
    // we'll do a full layer update soon.
    if (!parentLayer || !parentLayer->hasCompositedLayerMapping())
        return;

    if (parentLayer) {
        GraphicsLayer* hostingLayer = parentLayer->compositedLayerMapping()->parentForSublayers();
        GraphicsLayer* hostedLayer = childLayer->compositedLayerMapping()->childForSuperlayers();

        hostingLayer->addChild(hostedLayer);
    } else {
        childLayer->compositedLayerMapping()->childForSuperlayers()->removeFromParent();
    }
}

void RenderLayerCompositor::removeCompositedChildren(RenderLayer* layer)
{
    ASSERT(layer->hasCompositedLayerMapping());

    GraphicsLayer* hostingLayer = layer->compositedLayerMapping()->parentForSublayers();
    hostingLayer->removeAllChildren();
}

void RenderLayerCompositor::updateGraphicsLayersMappedToRenderLayer(RenderLayer* layer)
{
    if (!layer->hasCompositedLayerMapping())
        return;

    CompositedLayerMappingPtr compositedLayerMapping = layer->compositedLayerMapping();

    // Note carefully: here we assume that the compositing state of all descendants have been updated already,
    // so it is legitimate to compute and cache the composited bounds for this layer.
    compositedLayerMapping->updateCompositedBounds();

    if (layer->reflectionInfo()) {
        RenderLayer* reflectionLayer = layer->reflectionInfo()->reflectionLayer();
        ASSERT(reflectionLayer);
        if (reflectionLayer->hasCompositedLayerMapping())
            reflectionLayer->compositedLayerMapping()->updateCompositedBounds();
    }

    compositedLayerMapping->updateGraphicsLayerConfiguration();
    compositedLayerMapping->updateGraphicsLayerGeometry();

    if (!layer->parent())
        updateRootLayerPosition();

    if (compositedLayerMapping->hasUnpositionedOverflowControlsLayers())
        layer->scrollableArea()->positionOverflowControls();
}

void RenderLayerCompositor::rebuildCompositingLayerTree(RenderLayer* layer, Vector<GraphicsLayer*>& childLayersOfEnclosingLayer, int depth)
{
    // Make the layer compositing if necessary, and set up clipping and content layers.
    // Note that we can only do work here that is independent of whether the descendant layers
    // have been processed. computeCompositingRequirements() will already have done the repaint if necessary.

    layer->stackingNode()->updateLayerListsIfNeeded();
    layer->update3dRenderingContext();

    // Used for gathering UMA data about the effect on memory usage of promoting all layers
    // that have a webkit-transition on opacity or transform and intersect the viewport.
    static double pixelsWithoutPromotingAllTransitions = 0.0;
    static double pixelsAddedByPromotingAllTransitions = 0.0;

    if (!depth) {
        pixelsWithoutPromotingAllTransitions = 0.0;
        pixelsAddedByPromotingAllTransitions = 0.0;
    }

    const bool hasCompositedLayerMapping = layer->hasCompositedLayerMapping();
    CompositedLayerMappingPtr currentCompositedLayerMapping = layer->compositedLayerMapping();

    updateGraphicsLayersMappedToRenderLayer(layer);

    // Grab some stats for histograms.
    if (hasCompositedLayerMapping) {
        pixelsWithoutPromotingAllTransitions += layer->size().height() * layer->size().width();
    } else {
        if ((layer->renderer()->style()->transitionForProperty(CSSPropertyOpacity) ||
             layer->renderer()->style()->transitionForProperty(CSSPropertyWebkitTransform)) &&
            m_renderView->viewRect().intersects(layer->absoluteBoundingBox()))
            pixelsAddedByPromotingAllTransitions += layer->size().height() * layer->size().width();
    }

    // If this layer has a compositedLayerMapping, then that is where we place subsequent children GraphicsLayers.
    // Otherwise children continue to append to the child list of the enclosing layer.
    Vector<GraphicsLayer*> layerChildren;
    Vector<GraphicsLayer*>& childList = hasCompositedLayerMapping ? layerChildren : childLayersOfEnclosingLayer;

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer->stackingNode());
#endif

    if (layer->stackingNode()->isStackingContainer()) {
        RenderLayerStackingNodeIterator iterator(*layer->stackingNode(), NegativeZOrderChildren);
        while (RenderLayerStackingNode* curNode = iterator.next())
            rebuildCompositingLayerTree(curNode->layer(), childList, depth + 1);

        // If a negative z-order child is compositing, we get a foreground layer which needs to get parented.
        if (hasCompositedLayerMapping && currentCompositedLayerMapping->foregroundLayer())
            childList.append(currentCompositedLayerMapping->foregroundLayer());
    }

    RenderLayerStackingNodeIterator iterator(*layer->stackingNode(), NormalFlowChildren | PositiveZOrderChildren);
    while (RenderLayerStackingNode* curNode = iterator.next())
        rebuildCompositingLayerTree(curNode->layer(), childList, depth + 1);

    if (hasCompositedLayerMapping) {
        bool parented = false;
        if (layer->renderer()->isRenderPart())
            parented = parentFrameContentLayers(toRenderPart(layer->renderer()));

        if (!parented)
            currentCompositedLayerMapping->parentForSublayers()->setChildren(layerChildren);

        // If the layer has a clipping layer the overflow controls layers will be siblings of the clipping layer.
        // Otherwise, the overflow control layers are normal children.
        if (!currentCompositedLayerMapping->hasClippingLayer() && !currentCompositedLayerMapping->hasScrollingLayer()) {
            if (GraphicsLayer* overflowControlLayer = currentCompositedLayerMapping->layerForHorizontalScrollbar()) {
                overflowControlLayer->removeFromParent();
                currentCompositedLayerMapping->parentForSublayers()->addChild(overflowControlLayer);
            }

            if (GraphicsLayer* overflowControlLayer = currentCompositedLayerMapping->layerForVerticalScrollbar()) {
                overflowControlLayer->removeFromParent();
                currentCompositedLayerMapping->parentForSublayers()->addChild(overflowControlLayer);
            }

            if (GraphicsLayer* overflowControlLayer = currentCompositedLayerMapping->layerForScrollCorner()) {
                overflowControlLayer->removeFromParent();
                currentCompositedLayerMapping->parentForSublayers()->addChild(overflowControlLayer);
            }
        }

        childLayersOfEnclosingLayer.append(currentCompositedLayerMapping->childForSuperlayers());
    }

    if (!depth) {
        int percentageIncreaseInPixels = static_cast<int>(pixelsAddedByPromotingAllTransitions / pixelsWithoutPromotingAllTransitions * 100);
        blink::Platform::current()->histogramCustomCounts("Renderer.PixelIncreaseFromTransitions", percentageIncreaseInPixels, 0, 1000, 50);
    }
}

void RenderLayerCompositor::frameViewDidChangeLocation(const IntPoint& contentsOffset)
{
    if (m_overflowControlsHostLayer)
        m_overflowControlsHostLayer->setPosition(contentsOffset);
}

void RenderLayerCompositor::frameViewDidChangeSize()
{
    if (m_containerLayer) {
        FrameView* frameView = m_renderView->frameView();
        m_containerLayer->setSize(frameView->unscaledVisibleContentSize());

        frameViewDidScroll();
        updateOverflowControlsLayers();
    }
}

enum AcceleratedFixedRootBackgroundHistogramBuckets {
    ScrolledMainFrameBucket = 0,
    ScrolledMainFrameWithAcceleratedFixedRootBackground = 1,
    ScrolledMainFrameWithUnacceleratedFixedRootBackground = 2,
    AcceleratedFixedRootBackgroundHistogramMax = 3
};

void RenderLayerCompositor::frameViewDidScroll()
{
    FrameView* frameView = m_renderView->frameView();
    IntPoint scrollPosition = frameView->scrollPosition();

    if (!m_scrollLayer)
        return;

    bool scrollingCoordinatorHandlesOffset = false;
    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator()) {
        if (Settings* settings = m_renderView->document().settings()) {
            if (isMainFrame() || settings->compositedScrollingForFramesEnabled())
                scrollingCoordinatorHandlesOffset = scrollingCoordinator->scrollableAreaScrollLayerDidChange(frameView);
        }
    }

    // Scroll position = scroll minimum + scroll offset. Adjust the layer's
    // position to handle whatever the scroll coordinator isn't handling.
    // The minimum scroll position is non-zero for RTL pages with overflow.
    if (scrollingCoordinatorHandlesOffset)
        m_scrollLayer->setPosition(-frameView->minimumScrollPosition());
    else
        m_scrollLayer->setPosition(-scrollPosition);


    blink::Platform::current()->histogramEnumeration("Renderer.AcceleratedFixedRootBackground",
        ScrolledMainFrameBucket,
        AcceleratedFixedRootBackgroundHistogramMax);
}

void RenderLayerCompositor::frameViewScrollbarsExistenceDidChange()
{
    if (m_containerLayer)
        updateOverflowControlsLayers();
}

void RenderLayerCompositor::rootFixedBackgroundsChanged()
{
    if (!supportsFixedRootBackgroundCompositing())
        return;

    // To avoid having to make the fixed root background layer fixed positioned to
    // stay put, we position it in the layer tree as follows:
    //
    // + Overflow controls host
    //   + LocalFrame clip
    //     + (Fixed root background) <-- Here.
    //     + LocalFrame scroll
    //       + Root content layer
    //   + Scrollbars
    //
    // That is, it needs to be the first child of the frame clip, the sibling of
    // the frame scroll layer. The compositor does not own the background layer, it
    // just positions it (like the foreground layer).
    if (GraphicsLayer* backgroundLayer = fixedRootBackgroundLayer())
        m_containerLayer->addChildBelow(backgroundLayer, m_scrollLayer.get());
}

bool RenderLayerCompositor::scrollingLayerDidChange(RenderLayer* layer)
{
    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->scrollableAreaScrollLayerDidChange(layer->scrollableArea());
    return false;
}

String RenderLayerCompositor::layerTreeAsText(LayerTreeFlags flags)
{
    // Before dumping the layer tree, finish any pending compositing update.
    updateCompositingLayers();

    if (!m_rootContentLayer)
        return String();

    // We skip dumping the scroll and clip layers to keep layerTreeAsText output
    // similar between platforms (unless we explicitly request dumping from the
    // root.
    GraphicsLayer* rootLayer = m_rootContentLayer.get();
    if (flags & LayerTreeIncludesRootLayer)
        rootLayer = rootGraphicsLayer();

    String layerTreeText = rootLayer->layerTreeAsText(flags);

    // The true root layer is not included in the dump, so if we want to report
    // its repaint rects, they must be included here.
    if (flags & LayerTreeIncludesRepaintRects)
        return m_renderView->frameView()->trackedRepaintRectsAsText() + layerTreeText;

    return layerTreeText;
}

RenderLayerCompositor* RenderLayerCompositor::frameContentsCompositor(RenderPart* renderer)
{
    if (!renderer->node()->isFrameOwnerElement())
        return 0;

    HTMLFrameOwnerElement* element = toHTMLFrameOwnerElement(renderer->node());
    if (Document* contentDocument = element->contentDocument()) {
        if (RenderView* view = contentDocument->renderView())
            return view->compositor();
    }
    return 0;
}

bool RenderLayerCompositor::parentFrameContentLayers(RenderPart* renderer)
{
    RenderLayerCompositor* innerCompositor = frameContentsCompositor(renderer);
    if (!innerCompositor || !innerCompositor->inCompositingMode() || innerCompositor->rootLayerAttachment() != RootLayerAttachedViaEnclosingFrame)
        return false;

    RenderLayer* layer = renderer->layer();
    if (!layer->hasCompositedLayerMapping())
        return false;

    CompositedLayerMappingPtr compositedLayerMapping = layer->compositedLayerMapping();
    GraphicsLayer* hostingLayer = compositedLayerMapping->parentForSublayers();
    GraphicsLayer* rootLayer = innerCompositor->rootGraphicsLayer();
    if (hostingLayer->children().size() != 1 || hostingLayer->children()[0] != rootLayer) {
        hostingLayer->removeAllChildren();
        hostingLayer->addChild(rootLayer);
    }
    return true;
}

// This just updates layer geometry without changing the hierarchy.
void RenderLayerCompositor::updateLayerTreeGeometry(RenderLayer* layer)
{
    updateGraphicsLayersMappedToRenderLayer(layer);

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer->stackingNode());
#endif

    RenderLayerStackingNodeIterator iterator(*layer->stackingNode(), AllChildren);
    while (RenderLayerStackingNode* curNode = iterator.next())
        updateLayerTreeGeometry(curNode->layer());
}

// Recurs down the RenderLayer tree until its finds the compositing descendants of compositingAncestor and updates their geometry.
void RenderLayerCompositor::updateCompositingDescendantGeometry(RenderLayerStackingNode* compositingAncestor, RenderLayer* layer, bool compositedChildrenOnly)
{
    if (layer->stackingNode() != compositingAncestor) {
        if (layer->hasCompositedLayerMapping()) {
            CompositedLayerMappingPtr compositedLayerMapping = layer->compositedLayerMapping();
            compositedLayerMapping->updateCompositedBounds();

            if (layer->reflectionInfo()) {
                RenderLayer* reflectionLayer = layer->reflectionInfo()->reflectionLayer();
                if (reflectionLayer->hasCompositedLayerMapping())
                    reflectionLayer->compositedLayerMapping()->updateCompositedBounds();
            }

            compositedLayerMapping->updateGraphicsLayerGeometry();
            if (compositedChildrenOnly)
                return;
        }
    }

    if (layer->reflectionInfo())
        updateCompositingDescendantGeometry(compositingAncestor, layer->reflectionInfo()->reflectionLayer(), compositedChildrenOnly);

    if (!layer->hasCompositingDescendant())
        return;

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer->stackingNode());
#endif

    RenderLayerStackingNodeIterator iterator(*layer->stackingNode(), AllChildren);
    while (RenderLayerStackingNode* curNode = iterator.next())
        updateCompositingDescendantGeometry(compositingAncestor, curNode->layer(), compositedChildrenOnly);
}


void RenderLayerCompositor::repaintCompositedLayers(const IntRect* absRect)
{
    recursiveRepaintLayer(rootRenderLayer(), absRect);
}

void RenderLayerCompositor::recursiveRepaintLayer(RenderLayer* layer, const IntRect* rect)
{
    // FIXME: This method does not work correctly with transforms.
    if (layer->compositingState() == PaintsIntoOwnBacking) {
        if (rect)
            layer->repainter().setBackingNeedsRepaintInRect(*rect);
        else
            layer->repainter().setBackingNeedsRepaint();
    }

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer->stackingNode());
#endif

    unsigned childrenToVisit = NormalFlowChildren;
    if (layer->hasCompositingDescendant())
        childrenToVisit |= PositiveZOrderChildren | NegativeZOrderChildren;
    RenderLayerStackingNodeIterator iterator(*layer->stackingNode(), childrenToVisit);
    while (RenderLayerStackingNode* curNode = iterator.next()) {
        if (rect) {
            IntRect childRect(*rect);
            curNode->layer()->convertToPixelSnappedLayerCoords(layer, childRect);
            recursiveRepaintLayer(curNode->layer(), &childRect);
        } else {
            recursiveRepaintLayer(curNode->layer());
        }
    }
}

RenderLayer* RenderLayerCompositor::rootRenderLayer() const
{
    return m_renderView->layer();
}

GraphicsLayer* RenderLayerCompositor::rootGraphicsLayer() const
{
    if (m_overflowControlsHostLayer)
        return m_overflowControlsHostLayer.get();
    return m_rootContentLayer.get();
}

GraphicsLayer* RenderLayerCompositor::scrollLayer() const
{
    return m_scrollLayer.get();
}

GraphicsLayer* RenderLayerCompositor::containerLayer() const
{
    return m_containerLayer.get();
}

GraphicsLayer* RenderLayerCompositor::ensureRootTransformLayer()
{
    ASSERT(rootGraphicsLayer());

    if (!m_rootTransformLayer.get()) {
        m_rootTransformLayer = GraphicsLayer::create(graphicsLayerFactory(), this);
        m_overflowControlsHostLayer->addChild(m_rootTransformLayer.get());
        m_rootTransformLayer->addChild(m_containerLayer.get());
        updateOverflowControlsLayers();
    }

    return m_rootTransformLayer.get();
}

void RenderLayerCompositor::setIsInWindow(bool isInWindow)
{
    if (!inCompositingMode())
        return;

    if (isInWindow) {
        if (m_rootLayerAttachment != RootLayerUnattached)
            return;

        RootLayerAttachment attachment = isMainFrame() ? RootLayerAttachedViaChromeClient : RootLayerAttachedViaEnclosingFrame;
        attachRootLayer(attachment);
    } else {
        if (m_rootLayerAttachment == RootLayerUnattached)
            return;

        detachRootLayer();
    }
}

void RenderLayerCompositor::clearMappingForRenderLayerIncludingDescendants(RenderLayer* layer)
{
    if (!layer)
        return;

    if (layer->hasCompositedLayerMapping()) {
        removeViewportConstrainedLayer(layer);
        layer->clearCompositedLayerMapping();
    }

    for (RenderLayer* currLayer = layer->firstChild(); currLayer; currLayer = currLayer->nextSibling())
        clearMappingForRenderLayerIncludingDescendants(currLayer);
}

void RenderLayerCompositor::clearMappingForAllRenderLayers()
{
    clearMappingForRenderLayerIncludingDescendants(m_renderView->layer());
}

void RenderLayerCompositor::updateRootLayerPosition()
{
    if (m_rootContentLayer) {
        const IntRect& documentRect = m_renderView->documentRect();
        m_rootContentLayer->setSize(documentRect.size());
        m_rootContentLayer->setPosition(documentRect.location());
#if USE(RUBBER_BANDING)
        if (m_layerForOverhangShadow)
            OverscrollTheme::theme()->updateOverhangShadowLayer(m_layerForOverhangShadow.get(), m_rootContentLayer.get());
#endif
    }
    if (m_containerLayer) {
        FrameView* frameView = m_renderView->frameView();
        m_containerLayer->setSize(frameView->unscaledVisibleContentSize());
    }
}

bool RenderLayerCompositor::has3DContent() const
{
    return layerHas3DContent(rootRenderLayer());
}

void RenderLayerCompositor::updateDirectCompositingReasons(RenderLayer* layer)
{
    CompositingReasons layerReasons = layer->compositingReasons();

    layerReasons &= ~CompositingReasonComboAllDirectReasons;
    layerReasons |= m_compositingReasonFinder.directReasons(layer, &m_needsToRecomputeCompositingRequirements);
    layer->setCompositingReasons(layerReasons);
}

bool RenderLayerCompositor::needsOwnBacking(const RenderLayer* layer) const
{
    if (!canBeComposited(layer))
        return false;

    // If squashing is disabled, then layers that would have been squashed should just be separately composited.
    bool needsOwnBackingForDisabledSquashing = !layerSquashingEnabled() && requiresSquashing(layer->compositingReasons());

    return requiresCompositing(layer->compositingReasons()) || needsOwnBackingForDisabledSquashing || (inCompositingMode() && layer->isRootLayer());
}

bool RenderLayerCompositor::canBeComposited(const RenderLayer* layer) const
{
    // FIXME: We disable accelerated compositing for elements in a RenderFlowThread as it doesn't work properly.
    // See http://webkit.org/b/84900 to re-enable it.
    return m_hasAcceleratedCompositing && layer->isSelfPaintingLayer() && layer->renderer()->flowThreadState() == RenderObject::NotInsideFlowThread;
}

// Return true if the given layer has some ancestor in the RenderLayer hierarchy that clips,
// up to the enclosing compositing ancestor. This is required because compositing layers are parented
// according to the z-order hierarchy, yet clipping goes down the renderer hierarchy.
// Thus, a RenderLayer can be clipped by a RenderLayer that is an ancestor in the renderer hierarchy,
// but a sibling in the z-order hierarchy.
bool RenderLayerCompositor::clippedByAncestor(const RenderLayer* layer) const
{
    if (!layer->hasCompositedLayerMapping() || !layer->parent())
        return false;

    const RenderLayer* compositingAncestor = layer->ancestorCompositingLayer();
    if (!compositingAncestor)
        return false;

    RenderObject* clippingContainer = layer->renderer()->clippingContainer();
    if (!clippingContainer)
        return false;

    if (compositingAncestor->renderer()->isDescendantOf(clippingContainer))
        return false;

    return true;
}

// Return true if the given layer is a stacking context and has compositing child
// layers that it needs to clip. In this case we insert a clipping GraphicsLayer
// into the hierarchy between this layer and its children in the z-order hierarchy.
bool RenderLayerCompositor::clipsCompositingDescendants(const RenderLayer* layer) const
{
    return layer->hasCompositingDescendant() && layer->renderer()->hasClipOrOverflowClip();
}

CompositingReasons RenderLayerCompositor::subtreeReasonsForCompositing(RenderObject* renderer, bool hasCompositedDescendants, bool has3DTransformedDescendants) const
{
    CompositingReasons subtreeReasons = CompositingReasonNone;

    // FIXME: this seems to be a potentially different layer than the layer for which this was called. May not be an error, but is very confusing.
    RenderLayer* layer = toRenderBoxModelObject(renderer)->layer();

    // When a layer has composited descendants, some effects, like 2d transforms, filters, masks etc must be implemented
    // via compositing so that they also apply to those composited descdendants.
    if (hasCompositedDescendants) {
        if (layer->transform())
            subtreeReasons |= CompositingReasonTransformWithCompositedDescendants;

        if (layer->shouldIsolateCompositedDescendants()) {
            ASSERT(layer->stackingNode()->isStackingContext());
            subtreeReasons |= CompositingReasonIsolateCompositedDescendants;
        }

        // If the implementation of createsGroup changes, we need to be aware of that in this part of code.
        ASSERT((renderer->isTransparent() || renderer->hasMask() || renderer->hasFilter() || renderer->hasBlendMode()) == renderer->createsGroup());
        if (renderer->isTransparent())
            subtreeReasons |= CompositingReasonOpacityWithCompositedDescendants;
        if (renderer->hasMask())
            subtreeReasons |= CompositingReasonMaskWithCompositedDescendants;
        if (renderer->hasFilter())
            subtreeReasons |= CompositingReasonFilterWithCompositedDescendants;
        if (renderer->hasBlendMode())
            subtreeReasons |= CompositingReasonBlendingWithCompositedDescendants;

        if (renderer->hasReflection())
            subtreeReasons |= CompositingReasonReflectionWithCompositedDescendants;

        if (renderer->hasClipOrOverflowClip())
            subtreeReasons |= CompositingReasonClipsCompositingDescendants;
    }


    // A layer with preserve-3d or perspective only needs to be composited if there are descendant layers that
    // will be affected by the preserve-3d or perspective.
    if (has3DTransformedDescendants) {
        if (renderer->style()->transformStyle3D() == TransformStyle3DPreserve3D)
            subtreeReasons |= CompositingReasonPreserve3DWith3DDescendants;

        if (renderer->style()->hasPerspective())
            subtreeReasons |= CompositingReasonPerspectiveWith3DDescendants;
    }

    return subtreeReasons;
}

bool RenderLayerCompositor::isRunningAcceleratedTransformAnimation(RenderObject* renderer) const
{
    if (!m_compositingReasonFinder.hasAnimationTrigger())
        return false;
    return hasActiveAnimations(*renderer, CSSPropertyWebkitTransform);
}

// If an element has negative z-index children, those children render in front of the
// layer background, so we need an extra 'contents' layer for the foreground of the layer
// object.
bool RenderLayerCompositor::needsContentsCompositingLayer(const RenderLayer* layer) const
{
    return layer->stackingNode()->hasNegativeZOrderList();
}

static void paintScrollbar(Scrollbar* scrollbar, GraphicsContext& context, const IntRect& clip)
{
    if (!scrollbar)
        return;

    context.save();
    const IntRect& scrollbarRect = scrollbar->frameRect();
    context.translate(-scrollbarRect.x(), -scrollbarRect.y());
    IntRect transformedClip = clip;
    transformedClip.moveBy(scrollbarRect.location());
    scrollbar->paint(&context, transformedClip);
    context.restore();
}

void RenderLayerCompositor::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& context, GraphicsLayerPaintingPhase, const IntRect& clip)
{
    if (graphicsLayer == layerForHorizontalScrollbar())
        paintScrollbar(m_renderView->frameView()->horizontalScrollbar(), context, clip);
    else if (graphicsLayer == layerForVerticalScrollbar())
        paintScrollbar(m_renderView->frameView()->verticalScrollbar(), context, clip);
    else if (graphicsLayer == layerForScrollCorner()) {
        const IntRect& scrollCorner = m_renderView->frameView()->scrollCornerRect();
        context.save();
        context.translate(-scrollCorner.x(), -scrollCorner.y());
        IntRect transformedClip = clip;
        transformedClip.moveBy(scrollCorner.location());
        m_renderView->frameView()->paintScrollCorner(&context, transformedClip);
        context.restore();
    }
}

bool RenderLayerCompositor::supportsFixedRootBackgroundCompositing() const
{
    if (Settings* settings = m_renderView->document().settings()) {
        if (settings->acceleratedCompositingForFixedRootBackgroundEnabled())
            return true;
    }
    return false;
}

bool RenderLayerCompositor::needsFixedRootBackgroundLayer(const RenderLayer* layer) const
{
    if (layer != m_renderView->layer())
        return false;

    return supportsFixedRootBackgroundCompositing() && m_renderView->rootBackgroundIsEntirelyFixed();
}

GraphicsLayer* RenderLayerCompositor::fixedRootBackgroundLayer() const
{
    // Get the fixed root background from the RenderView layer's compositedLayerMapping.
    RenderLayer* viewLayer = m_renderView->layer();
    if (!viewLayer)
        return 0;

    if (viewLayer->compositingState() == PaintsIntoOwnBacking && viewLayer->compositedLayerMapping()->backgroundLayerPaintsFixedRootBackground())
        return viewLayer->compositedLayerMapping()->backgroundLayer();

    return 0;
}

static void resetTrackedRepaintRectsRecursive(GraphicsLayer* graphicsLayer)
{
    if (!graphicsLayer)
        return;

    graphicsLayer->resetTrackedRepaints();

    for (size_t i = 0; i < graphicsLayer->children().size(); ++i)
        resetTrackedRepaintRectsRecursive(graphicsLayer->children()[i]);

    if (GraphicsLayer* replicaLayer = graphicsLayer->replicaLayer())
        resetTrackedRepaintRectsRecursive(replicaLayer);

    if (GraphicsLayer* maskLayer = graphicsLayer->maskLayer())
        resetTrackedRepaintRectsRecursive(maskLayer);

    if (GraphicsLayer* clippingMaskLayer = graphicsLayer->contentsClippingMaskLayer())
        resetTrackedRepaintRectsRecursive(clippingMaskLayer);
}

void RenderLayerCompositor::resetTrackedRepaintRects()
{
    if (GraphicsLayer* rootLayer = rootGraphicsLayer())
        resetTrackedRepaintRectsRecursive(rootLayer);
}

void RenderLayerCompositor::setTracksRepaints(bool tracksRepaints)
{
    updateCompositingLayers();
    m_isTrackingRepaints = tracksRepaints;
}

bool RenderLayerCompositor::isTrackingRepaints() const
{
    return m_isTrackingRepaints;
}

static bool shouldCompositeOverflowControls(FrameView* view)
{
    if (Page* page = view->frame().page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            if (scrollingCoordinator->coordinatesScrollingForFrameView(view))
                return true;
    }

    return true;
}

bool RenderLayerCompositor::requiresHorizontalScrollbarLayer() const
{
    FrameView* view = m_renderView->frameView();
    return shouldCompositeOverflowControls(view) && view->horizontalScrollbar();
}

bool RenderLayerCompositor::requiresVerticalScrollbarLayer() const
{
    FrameView* view = m_renderView->frameView();
    return shouldCompositeOverflowControls(view) && view->verticalScrollbar();
}

bool RenderLayerCompositor::requiresScrollCornerLayer() const
{
    FrameView* view = m_renderView->frameView();
    return shouldCompositeOverflowControls(view) && view->isScrollCornerVisible();
}

#if USE(RUBBER_BANDING)
bool RenderLayerCompositor::requiresOverhangLayers() const
{
    // We don't want a layer if this is a subframe.
    if (!isMainFrame())
        return false;

    // We do want a layer if we have a scrolling coordinator and can scroll.
    if (scrollingCoordinator() && m_renderView->frameView()->hasOpaqueBackground())
        return true;

    // Chromium always wants a layer.
    return true;
}
#endif

void RenderLayerCompositor::updateOverflowControlsLayers()
{
#if USE(RUBBER_BANDING)
    if (requiresOverhangLayers()) {
        if (!m_layerForOverhangShadow) {
            m_layerForOverhangShadow = GraphicsLayer::create(graphicsLayerFactory(), this);
            OverscrollTheme::theme()->setUpOverhangShadowLayer(m_layerForOverhangShadow.get());
            OverscrollTheme::theme()->updateOverhangShadowLayer(m_layerForOverhangShadow.get(), m_rootContentLayer.get());
            m_scrollLayer->addChild(m_layerForOverhangShadow.get());
        }
    } else {
        if (m_layerForOverhangShadow) {
            m_layerForOverhangShadow->removeFromParent();
            m_layerForOverhangShadow = nullptr;
        }
    }
#endif
    GraphicsLayer* controlsParent = m_rootTransformLayer.get() ? m_rootTransformLayer.get() : m_overflowControlsHostLayer.get();

    if (requiresHorizontalScrollbarLayer()) {
        if (!m_layerForHorizontalScrollbar) {
            m_layerForHorizontalScrollbar = GraphicsLayer::create(graphicsLayerFactory(), this);
            controlsParent->addChild(m_layerForHorizontalScrollbar.get());

            if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView->frameView(), HorizontalScrollbar);
        }
    } else if (m_layerForHorizontalScrollbar) {
        m_layerForHorizontalScrollbar->removeFromParent();
        m_layerForHorizontalScrollbar = nullptr;

        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView->frameView(), HorizontalScrollbar);
    }

    if (requiresVerticalScrollbarLayer()) {
        if (!m_layerForVerticalScrollbar) {
            m_layerForVerticalScrollbar = GraphicsLayer::create(graphicsLayerFactory(), this);
            controlsParent->addChild(m_layerForVerticalScrollbar.get());

            if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView->frameView(), VerticalScrollbar);
        }
    } else if (m_layerForVerticalScrollbar) {
        m_layerForVerticalScrollbar->removeFromParent();
        m_layerForVerticalScrollbar = nullptr;

        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView->frameView(), VerticalScrollbar);
    }

    if (requiresScrollCornerLayer()) {
        if (!m_layerForScrollCorner) {
            m_layerForScrollCorner = GraphicsLayer::create(graphicsLayerFactory(), this);
            controlsParent->addChild(m_layerForScrollCorner.get());
        }
    } else if (m_layerForScrollCorner) {
        m_layerForScrollCorner->removeFromParent();
        m_layerForScrollCorner = nullptr;
    }

    m_renderView->frameView()->positionScrollbarLayers();
}

void RenderLayerCompositor::ensureRootLayer()
{
    RootLayerAttachment expectedAttachment = isMainFrame() ? RootLayerAttachedViaChromeClient : RootLayerAttachedViaEnclosingFrame;
    if (expectedAttachment == m_rootLayerAttachment)
         return;

    if (!m_rootContentLayer) {
        m_rootContentLayer = GraphicsLayer::create(graphicsLayerFactory(), this);
        IntRect overflowRect = m_renderView->pixelSnappedLayoutOverflowRect();
        m_rootContentLayer->setSize(FloatSize(overflowRect.maxX(), overflowRect.maxY()));
        m_rootContentLayer->setPosition(FloatPoint());

        // Need to clip to prevent transformed content showing outside this frame
        m_rootContentLayer->setMasksToBounds(true);
    }

    if (!m_overflowControlsHostLayer) {
        ASSERT(!m_scrollLayer);
        ASSERT(!m_containerLayer);

        // Create a layer to host the clipping layer and the overflow controls layers.
        m_overflowControlsHostLayer = GraphicsLayer::create(graphicsLayerFactory(), this);

        // Create a clipping layer if this is an iframe or settings require to clip.
        m_containerLayer = GraphicsLayer::create(graphicsLayerFactory(), this);
        bool containerMasksToBounds = !isMainFrame();
        if (Settings* settings = m_renderView->document().settings()) {
            if (settings->mainFrameClipsContent())
                containerMasksToBounds = true;
        }
        m_containerLayer->setMasksToBounds(containerMasksToBounds);

        m_scrollLayer = GraphicsLayer::create(graphicsLayerFactory(), this);
        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->setLayerIsContainerForFixedPositionLayers(m_scrollLayer.get(), true);

        // Hook them up
        m_overflowControlsHostLayer->addChild(m_containerLayer.get());
        m_containerLayer->addChild(m_scrollLayer.get());
        m_scrollLayer->addChild(m_rootContentLayer.get());

        frameViewDidChangeSize();
        frameViewDidScroll();
    }

    // Check to see if we have to change the attachment
    if (m_rootLayerAttachment != RootLayerUnattached)
        detachRootLayer();

    attachRootLayer(expectedAttachment);
}

void RenderLayerCompositor::destroyRootLayer()
{
    if (!m_rootContentLayer)
        return;

    detachRootLayer();

#if USE(RUBBER_BANDING)
    if (m_layerForOverhangShadow) {
        m_layerForOverhangShadow->removeFromParent();
        m_layerForOverhangShadow = nullptr;
    }
#endif

    if (m_layerForHorizontalScrollbar) {
        m_layerForHorizontalScrollbar->removeFromParent();
        m_layerForHorizontalScrollbar = nullptr;
        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView->frameView(), HorizontalScrollbar);
        if (Scrollbar* horizontalScrollbar = m_renderView->frameView()->verticalScrollbar())
            m_renderView->frameView()->invalidateScrollbar(horizontalScrollbar, IntRect(IntPoint(0, 0), horizontalScrollbar->frameRect().size()));
    }

    if (m_layerForVerticalScrollbar) {
        m_layerForVerticalScrollbar->removeFromParent();
        m_layerForVerticalScrollbar = nullptr;
        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView->frameView(), VerticalScrollbar);
        if (Scrollbar* verticalScrollbar = m_renderView->frameView()->verticalScrollbar())
            m_renderView->frameView()->invalidateScrollbar(verticalScrollbar, IntRect(IntPoint(0, 0), verticalScrollbar->frameRect().size()));
    }

    if (m_layerForScrollCorner) {
        m_layerForScrollCorner = nullptr;
        m_renderView->frameView()->invalidateScrollCorner(m_renderView->frameView()->scrollCornerRect());
    }

    if (m_overflowControlsHostLayer) {
        m_overflowControlsHostLayer = nullptr;
        m_containerLayer = nullptr;
        m_scrollLayer = nullptr;
    }
    ASSERT(!m_scrollLayer);
    m_rootContentLayer = nullptr;
    m_rootTransformLayer = nullptr;
}

void RenderLayerCompositor::attachRootLayer(RootLayerAttachment attachment)
{
    if (!m_rootContentLayer)
        return;

    switch (attachment) {
        case RootLayerUnattached:
            ASSERT_NOT_REACHED();
            break;
        case RootLayerAttachedViaChromeClient: {
            LocalFrame& frame = m_renderView->frameView()->frame();
            Page* page = frame.page();
            if (!page)
                return;
            page->chrome().client().attachRootGraphicsLayer(rootGraphicsLayer());
            break;
        }
        case RootLayerAttachedViaEnclosingFrame: {
            HTMLFrameOwnerElement* ownerElement = m_renderView->document().ownerElement();
            ASSERT(ownerElement);
            DeprecatedScheduleStyleRecalcDuringCompositingUpdate marker(ownerElement->document().lifecycle());
            // The layer will get hooked up via CompositedLayerMapping::updateGraphicsLayerConfiguration()
            // for the frame's renderer in the parent document.
            ownerElement->scheduleLayerUpdate();
            break;
        }
    }

    m_rootLayerAttachment = attachment;
}

void RenderLayerCompositor::detachRootLayer()
{
    if (!m_rootContentLayer || m_rootLayerAttachment == RootLayerUnattached)
        return;

    switch (m_rootLayerAttachment) {
    case RootLayerAttachedViaEnclosingFrame: {
        // The layer will get unhooked up via CompositedLayerMapping::updateGraphicsLayerConfiguration()
        // for the frame's renderer in the parent document.
        if (m_overflowControlsHostLayer)
            m_overflowControlsHostLayer->removeFromParent();
        else
            m_rootContentLayer->removeFromParent();

        if (HTMLFrameOwnerElement* ownerElement = m_renderView->document().ownerElement()) {
            DeprecatedScheduleStyleRecalcDuringCompositingUpdate marker(ownerElement->document().lifecycle());
            ownerElement->scheduleLayerUpdate();
        }
        break;
    }
    case RootLayerAttachedViaChromeClient: {
        LocalFrame& frame = m_renderView->frameView()->frame();
        Page* page = frame.page();
        if (!page)
            return;
        page->chrome().client().attachRootGraphicsLayer(0);
    }
    break;
    case RootLayerUnattached:
        break;
    }

    m_rootLayerAttachment = RootLayerUnattached;
}

void RenderLayerCompositor::updateRootLayerAttachment()
{
    ensureRootLayer();
}

bool RenderLayerCompositor::isMainFrame() const
{
    // FIXME: LocalFrame::isMainFrame() is probably better.
    return !m_renderView->document().ownerElement();
}

// IFrames are special, because we hook compositing layers together across iframe boundaries
// when both parent and iframe content are composited. So when this frame becomes composited, we have
// to use a synthetic style change to get the iframes into RenderLayers in order to allow them to composite.
void RenderLayerCompositor::notifyIFramesOfCompositingChange()
{
    if (!m_renderView->frameView())
        return;
    LocalFrame& frame = m_renderView->frameView()->frame();

    for (LocalFrame* child = frame.tree().firstChild(); child; child = child->tree().traverseNext(&frame)) {
        if (!child->document())
            continue; // FIXME: Can this happen?
        if (HTMLFrameOwnerElement* ownerElement = child->document()->ownerElement()) {
            DeprecatedScheduleStyleRecalcDuringCompositingUpdate marker(ownerElement->document().lifecycle());
            ownerElement->scheduleLayerUpdate();
        }
    }

    // Compositing also affects the answer to RenderIFrame::requiresAcceleratedCompositing(), so
    // we need to schedule a style recalc in our parent document.
    if (HTMLFrameOwnerElement* ownerElement = m_renderView->document().ownerElement()) {
        ownerElement->document().renderView()->compositor()->setNeedsToRecomputeCompositingRequirements();
        DeprecatedScheduleStyleRecalcDuringCompositingUpdate marker(ownerElement->document().lifecycle());
        ownerElement->scheduleLayerUpdate();
    }
}

bool RenderLayerCompositor::layerHas3DContent(const RenderLayer* layer) const
{
    const RenderStyle* style = layer->renderer()->style();
    RenderLayerStackingNode* stackingNode = const_cast<RenderLayer*>(layer)->stackingNode();

    if (style &&
        (style->transformStyle3D() == TransformStyle3DPreserve3D ||
         style->hasPerspective() ||
         style->transform().has3DOperation()))
        return true;

    stackingNode->updateLayerListsIfNeeded();

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(stackingNode);
#endif

    RenderLayerStackingNodeIterator iterator(*layer->stackingNode(), AllChildren);
    while (RenderLayerStackingNode* curNode = iterator.next()) {
        if (layerHas3DContent(curNode->layer()))
            return true;
    }

    return false;
}

void RenderLayerCompositor::updateViewportConstraintStatus(RenderLayer* layer)
{
    if (CompositingReasonFinder::isViewportConstrainedFixedOrStickyLayer(layer))
        addViewportConstrainedLayer(layer);
    else
        removeViewportConstrainedLayer(layer);
}

void RenderLayerCompositor::addViewportConstrainedLayer(RenderLayer* layer)
{
    m_viewportConstrainedLayers.add(layer);
}

void RenderLayerCompositor::removeViewportConstrainedLayer(RenderLayer* layer)
{
    if (!m_viewportConstrainedLayers.contains(layer))
        return;

    m_viewportConstrainedLayers.remove(layer);
}

FixedPositionViewportConstraints RenderLayerCompositor::computeFixedViewportConstraints(RenderLayer* layer) const
{
    ASSERT(layer->hasCompositedLayerMapping());

    FrameView* frameView = m_renderView->frameView();
    LayoutRect viewportRect = frameView->viewportConstrainedVisibleContentRect();

    FixedPositionViewportConstraints constraints;

    GraphicsLayer* graphicsLayer = layer->compositedLayerMapping()->mainGraphicsLayer();

    constraints.setLayerPositionAtLastLayout(graphicsLayer->position());
    constraints.setViewportRectAtLastLayout(viewportRect);

    RenderStyle* style = layer->renderer()->style();
    if (!style->left().isAuto())
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeLeft);

    if (!style->right().isAuto())
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeRight);

    if (!style->top().isAuto())
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeTop);

    if (!style->bottom().isAuto())
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeBottom);

    // If left and right are auto, use left.
    if (style->left().isAuto() && style->right().isAuto())
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeLeft);

    // If top and bottom are auto, use top.
    if (style->top().isAuto() && style->bottom().isAuto())
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeTop);

    return constraints;
}

StickyPositionViewportConstraints RenderLayerCompositor::computeStickyViewportConstraints(RenderLayer* layer) const
{
    ASSERT(layer->hasCompositedLayerMapping());
    // We should never get here for stickies constrained by an enclosing clipping layer.
    ASSERT(!layer->enclosingOverflowClipLayer(ExcludeSelf));

    FrameView* frameView = m_renderView->frameView();
    LayoutRect viewportRect = frameView->viewportConstrainedVisibleContentRect();

    StickyPositionViewportConstraints constraints;

    RenderBoxModelObject* renderer = toRenderBoxModelObject(layer->renderer());

    renderer->computeStickyPositionConstraints(constraints, viewportRect);

    GraphicsLayer* graphicsLayer = layer->compositedLayerMapping()->mainGraphicsLayer();

    constraints.setLayerPositionAtLastLayout(graphicsLayer->position());
    constraints.setStickyOffsetAtLastLayout(renderer->stickyPositionOffset());

    return constraints;
}

ScrollingCoordinator* RenderLayerCompositor::scrollingCoordinator() const
{
    if (Page* page = this->page())
        return page->scrollingCoordinator();

    return 0;
}

GraphicsLayerFactory* RenderLayerCompositor::graphicsLayerFactory() const
{
    if (Page* page = this->page())
        return page->chrome().client().graphicsLayerFactory();
    return 0;
}

Page* RenderLayerCompositor::page() const
{
    return m_renderView->frameView()->frame().page();
}

DocumentLifecycle& RenderLayerCompositor::lifecycle() const
{
    return m_renderView->document().lifecycle();
}

String RenderLayerCompositor::debugName(const GraphicsLayer* graphicsLayer)
{
    String name;
    if (graphicsLayer == m_rootContentLayer.get()) {
        name = "Content Root Layer";
    } else if (graphicsLayer == m_rootTransformLayer.get()) {
        name = "Root Transform Layer";
#if USE(RUBBER_BANDING)
    } else if (graphicsLayer == m_layerForOverhangShadow.get()) {
        name = "Overhang Areas Shadow";
#endif
    } else if (graphicsLayer == m_overflowControlsHostLayer.get()) {
        name = "Overflow Controls Host Layer";
    } else if (graphicsLayer == m_layerForHorizontalScrollbar.get()) {
        name = "Horizontal Scrollbar Layer";
    } else if (graphicsLayer == m_layerForVerticalScrollbar.get()) {
        name = "Vertical Scrollbar Layer";
    } else if (graphicsLayer == m_layerForScrollCorner.get()) {
        name = "Scroll Corner Layer";
    } else if (graphicsLayer == m_containerLayer.get()) {
        name = "LocalFrame Clipping Layer";
    } else if (graphicsLayer == m_scrollLayer.get()) {
        name = "LocalFrame Scrolling Layer";
    } else {
        ASSERT_NOT_REACHED();
    }

    return name;
}

} // namespace WebCore
