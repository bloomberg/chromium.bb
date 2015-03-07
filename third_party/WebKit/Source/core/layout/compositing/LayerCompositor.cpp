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

#include "core/layout/compositing/LayerCompositor.h"

#include "core/animation/DocumentAnimations.h"
#include "core/dom/Fullscreen.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorNodeIds.h"
#include "core/layout/LayerStackingNode.h"
#include "core/layout/LayerStackingNodeIterator.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutVideo.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/CompositingInputsUpdater.h"
#include "core/layout/compositing/CompositingLayerAssigner.h"
#include "core/layout/compositing/CompositingRequirementsUpdater.h"
#include "core/layout/compositing/GraphicsLayerTreeBuilder.h"
#include "core/layout/compositing/GraphicsLayerUpdater.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/paint/FramePainter.h"
#include "core/paint/TransformRecorder.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/TransformDisplayItem.h"
#include "public/platform/Platform.h"

namespace blink {

LayerCompositor::LayerCompositor(LayoutView& layoutView)
    : m_layoutView(layoutView)
    , m_compositingReasonFinder(layoutView)
    , m_pendingUpdateType(CompositingUpdateNone)
    , m_hasAcceleratedCompositing(true)
    , m_compositing(false)
    , m_rootShouldAlwaysCompositeDirty(true)
    , m_needsUpdateFixedBackground(false)
    , m_isTrackingPaintInvalidations(false)
    , m_rootLayerAttachment(RootLayerUnattached)
    , m_inOverlayFullscreenVideo(false)
{
    updateAcceleratedCompositingSettings();
}

LayerCompositor::~LayerCompositor()
{
    ASSERT(m_rootLayerAttachment == RootLayerUnattached);
}

bool LayerCompositor::inCompositingMode() const
{
    // FIXME: This should assert that lifecycle is >= CompositingClean since
    // the last step of updateIfNeeded can set this bit to false.
    ASSERT(m_layoutView.layer()->isAllowedToQueryCompositingState());
    return m_compositing;
}

bool LayerCompositor::staleInCompositingMode() const
{
    return m_compositing;
}

void LayerCompositor::setCompositingModeEnabled(bool enable)
{
    if (enable == m_compositing)
        return;

    m_compositing = enable;

    // LayoutPart::requiresAcceleratedCompositing is used to determine self-paintingness
    // and bases it's return value for frames on the m_compositing bit here.
    if (HTMLFrameOwnerElement* ownerElement = m_layoutView.document().ownerElement()) {
        if (LayoutPart* renderer = ownerElement->layoutPart())
            renderer->layer()->updateSelfPaintingLayer();
    }

    if (m_compositing)
        ensureRootLayer();
    else
        destroyRootLayer();

    // Compositing also affects the answer to LayoutIFrame::requiresAcceleratedCompositing(), so
    // we need to schedule a style recalc in our parent document.
    if (HTMLFrameOwnerElement* ownerElement = m_layoutView.document().ownerElement())
        ownerElement->setNeedsCompositingUpdate();
}

void LayerCompositor::enableCompositingModeIfNeeded()
{
    if (!m_rootShouldAlwaysCompositeDirty)
        return;

    m_rootShouldAlwaysCompositeDirty = false;
    if (m_compositing)
        return;

    if (rootShouldAlwaysComposite()) {
        // FIXME: Is this needed? It was added in https://bugs.webkit.org/show_bug.cgi?id=26651.
        // No tests fail if it's deleted.
        setNeedsCompositingUpdate(CompositingUpdateRebuildTree);
        setCompositingModeEnabled(true);
    }
}

bool LayerCompositor::rootShouldAlwaysComposite() const
{
    if (!m_hasAcceleratedCompositing)
        return false;
    return m_layoutView.frame()->isLocalRoot() || m_compositingReasonFinder.requiresCompositingForScrollableFrame();
}

void LayerCompositor::updateAcceleratedCompositingSettings()
{
    m_compositingReasonFinder.updateTriggers();
    m_hasAcceleratedCompositing = m_layoutView.document().settings()->acceleratedCompositingEnabled();
    m_rootShouldAlwaysCompositeDirty = true;
    if (m_rootLayerAttachment != RootLayerUnattached)
        rootLayer()->setNeedsCompositingInputsUpdate();
}

bool LayerCompositor::preferCompositingToLCDTextEnabled() const
{
    return m_compositingReasonFinder.hasOverflowScrollTrigger();
}

static LayoutVideo* findFullscreenVideoRenderer(Document& document)
{
    // Recursively find the document that is in fullscreen.
    Element* fullscreenElement = Fullscreen::fullscreenElementFrom(document);
    Document* contentDocument = &document;
    while (fullscreenElement && fullscreenElement->isFrameOwnerElement()) {
        contentDocument = toHTMLFrameOwnerElement(fullscreenElement)->contentDocument();
        if (!contentDocument)
            return 0;
        fullscreenElement = Fullscreen::fullscreenElementFrom(*contentDocument);
    }
    // Get the current fullscreen element from the document.
    fullscreenElement = Fullscreen::currentFullScreenElementFrom(*contentDocument);
    if (!isHTMLVideoElement(fullscreenElement))
        return 0;
    LayoutObject* renderer = fullscreenElement->layoutObject();
    if (!renderer)
        return 0;
    return toLayoutVideo(renderer);
}

void LayerCompositor::updateIfNeededRecursive()
{
    for (Frame* child = m_layoutView.frameView()->frame().tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            toLocalFrame(child)->contentRenderer()->compositor()->updateIfNeededRecursive();
    }

    TRACE_EVENT0("blink", "LayerCompositor::updateIfNeededRecursive");

    ASSERT(!m_layoutView.needsLayout());

    ScriptForbiddenScope forbidScript;

    // FIXME: enableCompositingModeIfNeeded can trigger a CompositingUpdateRebuildTree,
    // which asserts that it's not InCompositingUpdate.
    enableCompositingModeIfNeeded();

    rootLayer()->updateDescendantDependentFlagsForEntireSubtree();
    m_layoutView.commitPendingSelection();

    lifecycle().advanceTo(DocumentLifecycle::InCompositingUpdate);
    updateIfNeeded();
    lifecycle().advanceTo(DocumentLifecycle::CompositingClean);

    DocumentAnimations::updateCompositorAnimations(m_layoutView.document());

    m_layoutView.frameView()->updateCompositorScrollAnimations();
    if (const FrameView::ScrollableAreaSet* animatingScrollableAreas = m_layoutView.frameView()->animatingScrollableAreas()) {
        for (ScrollableArea* scrollableArea : *animatingScrollableAreas)
            scrollableArea->updateCompositorScrollAnimations();
    }

#if ENABLE(ASSERT)
    ASSERT(lifecycle().state() == DocumentLifecycle::CompositingClean);
    assertNoUnresolvedDirtyBits();
    for (Frame* child = m_layoutView.frameView()->frame().tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            toLocalFrame(child)->contentRenderer()->compositor()->assertNoUnresolvedDirtyBits();
    }
#endif
}

void LayerCompositor::setNeedsCompositingUpdate(CompositingUpdateType updateType)
{
    ASSERT(updateType != CompositingUpdateNone);
    m_pendingUpdateType = std::max(m_pendingUpdateType, updateType);
    page()->animator().scheduleVisualUpdate(m_layoutView.frame());
    lifecycle().ensureStateAtMost(DocumentLifecycle::LayoutClean);
}

void LayerCompositor::didLayout()
{
    // FIXME: Technically we only need to do this when the FrameView's
    // isScrollable method would return a different value.
    m_rootShouldAlwaysCompositeDirty = true;
    enableCompositingModeIfNeeded();

    // FIXME: Rather than marking the entire LayoutView as dirty, we should
    // track which Layers moved during layout and only dirty those
    // specific Layers.
    rootLayer()->setNeedsCompositingInputsUpdate();
}

#if ENABLE(ASSERT)

void LayerCompositor::assertNoUnresolvedDirtyBits()
{
    ASSERT(m_pendingUpdateType == CompositingUpdateNone);
    ASSERT(!m_rootShouldAlwaysCompositeDirty);
}

#endif

void LayerCompositor::applyOverlayFullscreenVideoAdjustment()
{
    m_inOverlayFullscreenVideo = false;
    if (!m_rootContentLayer)
        return;

    bool isLocalRoot = m_layoutView.frame()->isLocalRoot();
    LayoutVideo* video = findFullscreenVideoRenderer(m_layoutView.document());
    if (!video || !video->layer()->hasCompositedLayerMapping()) {
        if (isLocalRoot) {
            GraphicsLayer* backgroundLayer = fixedRootBackgroundLayer();
            if (backgroundLayer && !backgroundLayer->parent())
                rootFixedBackgroundsChanged();
        }
        return;
    }

    GraphicsLayer* videoLayer = video->layer()->compositedLayerMapping()->mainGraphicsLayer();

    // The fullscreen video has layer position equal to its enclosing frame's scroll position because fullscreen container is fixed-positioned.
    // We should reset layer position here since we are going to reattach the layer at the very top level.
    videoLayer->setPosition(IntPoint());

    // Only steal fullscreen video layer and clear all other layers if we are the main frame.
    if (!isLocalRoot)
        return;

    m_rootContentLayer->removeAllChildren();
    m_overflowControlsHostLayer->addChild(videoLayer);
    if (GraphicsLayer* backgroundLayer = fixedRootBackgroundLayer())
        backgroundLayer->removeFromParent();
    m_inOverlayFullscreenVideo = true;
}

void LayerCompositor::updateWithoutAcceleratedCompositing(CompositingUpdateType updateType)
{
    ASSERT(!hasAcceleratedCompositing());

    if (updateType >= CompositingUpdateAfterCompositingInputChange)
        CompositingInputsUpdater(rootLayer()).update();

#if ENABLE(ASSERT)
    CompositingInputsUpdater::assertNeedsCompositingInputsUpdateBitsCleared(rootLayer());
#endif
}

static void forceRecomputePaintInvalidationRectsIncludingNonCompositingDescendants(LayoutObject* renderer)
{
    // We clear the previous paint invalidation rect as it's wrong (paint invaliation container
    // changed, ...). Forcing a full invalidation will make us recompute it. Also we are not
    // changing the previous position from our paint invalidation container, which is fine as
    // we want a full paint invalidation anyway.
    renderer->setPreviousPaintInvalidationRect(LayoutRect());
    renderer->setShouldDoFullPaintInvalidation();

    for (LayoutObject* child = renderer->slowFirstChild(); child; child = child->nextSibling()) {
        if (!child->isPaintInvalidationContainer())
            forceRecomputePaintInvalidationRectsIncludingNonCompositingDescendants(child);
    }
}


void LayerCompositor::updateIfNeeded()
{
    CompositingUpdateType updateType = m_pendingUpdateType;
    m_pendingUpdateType = CompositingUpdateNone;

    if (!hasAcceleratedCompositing()) {
        updateWithoutAcceleratedCompositing(updateType);
        return;
    }

    if (updateType == CompositingUpdateNone)
        return;

    Layer* updateRoot = rootLayer();

    Vector<Layer*> layersNeedingPaintInvalidation;

    if (updateType >= CompositingUpdateAfterCompositingInputChange) {
        CompositingInputsUpdater(updateRoot).update();

#if ENABLE(ASSERT)
        // FIXME: Move this check to the end of the compositing update.
        CompositingInputsUpdater::assertNeedsCompositingInputsUpdateBitsCleared(updateRoot);
#endif

        CompositingRequirementsUpdater(m_layoutView, m_compositingReasonFinder).update(updateRoot);

        CompositingLayerAssigner layerAssigner(this);
        layerAssigner.assign(updateRoot, layersNeedingPaintInvalidation);

        bool layersChanged = layerAssigner.layersChanged();

        {
            TRACE_EVENT0("blink", "LayerCompositor::updateAfterCompositingChange");
            if (const FrameView::ScrollableAreaSet* scrollableAreas = m_layoutView.frameView()->scrollableAreas()) {
                for (FrameView::ScrollableAreaSet::iterator it = scrollableAreas->begin(); it != scrollableAreas->end(); ++it)
                    layersChanged |= (*it)->updateAfterCompositingChange();
            }
        }

        if (layersChanged)
            updateType = std::max(updateType, CompositingUpdateRebuildTree);
    }

    if (updateType != CompositingUpdateNone) {
        GraphicsLayerUpdater updater;
        updater.update(*updateRoot, layersNeedingPaintInvalidation);

        if (updater.needsRebuildTree())
            updateType = std::max(updateType, CompositingUpdateRebuildTree);

#if ENABLE(ASSERT)
        // FIXME: Move this check to the end of the compositing update.
        GraphicsLayerUpdater::assertNeedsToUpdateGraphicsLayerBitsCleared(*updateRoot);
#endif
    }

    if (updateType >= CompositingUpdateRebuildTree) {
        GraphicsLayerTreeBuilder::AncestorInfo ancestorInfo;
        GraphicsLayerVector childList;
        ancestorInfo.childLayersOfEnclosingCompositedLayer = &childList;
        {
            TRACE_EVENT0("blink", "GraphicsLayerTreeBuilder::rebuild");
            GraphicsLayerTreeBuilder().rebuild(*updateRoot, ancestorInfo);
        }

        if (childList.isEmpty())
            destroyRootLayer();
        else
            m_rootContentLayer->setChildren(childList);

        if (RuntimeEnabledFeatures::overlayFullscreenVideoEnabled())
            applyOverlayFullscreenVideoAdjustment();
    }

    if (m_needsUpdateFixedBackground) {
        rootFixedBackgroundsChanged();
        m_needsUpdateFixedBackground = false;
    }

    for (unsigned i = 0; i < layersNeedingPaintInvalidation.size(); i++)
        forceRecomputePaintInvalidationRectsIncludingNonCompositingDescendants(layersNeedingPaintInvalidation[i]->layoutObject());

    // Inform the inspector that the layer tree has changed.
    if (m_layoutView.frame()->isMainFrame())
        InspectorInstrumentation::layerTreeDidChange(m_layoutView.frame());
}

bool LayerCompositor::allocateOrClearCompositedLayerMapping(Layer* layer, const CompositingStateTransitionType compositedLayerUpdate)
{
    bool compositedLayerMappingChanged = false;

    // FIXME: It would be nice to directly use the layer's compositing reason,
    // but allocateOrClearCompositedLayerMapping also gets called without having updated compositing
    // requirements fully.
    switch (compositedLayerUpdate) {
    case AllocateOwnCompositedLayerMapping:
        ASSERT(!layer->hasCompositedLayerMapping());
        setCompositingModeEnabled(true);

        // If we need to issue paint invalidations, do so before allocating the compositedLayerMapping and clearing out the groupedMapping.
        paintInvalidationOnCompositingChange(layer);

        // If this layer was previously squashed, we need to remove its reference to a groupedMapping right away, so
        // that computing paint invalidation rects will know the layer's correct compositingState.
        // FIXME: do we need to also remove the layer from it's location in the squashing list of its groupedMapping?
        // Need to create a test where a squashed layer pops into compositing. And also to cover all other
        // sorts of compositingState transitions.
        layer->setLostGroupedMapping(false);
        layer->setGroupedMapping(0);

        layer->ensureCompositedLayerMapping();
        compositedLayerMappingChanged = true;

        // At this time, the ScrollingCooridnator only supports the top-level frame.
        if (layer->isRootLayer() && m_layoutView.frame()->isLocalRoot()) {
            if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->frameViewRootLayerDidChange(m_layoutView.frameView());
        }
        break;
    case RemoveOwnCompositedLayerMapping:
    // PutInSquashingLayer means you might have to remove the composited layer mapping first.
    case PutInSquashingLayer:
        if (layer->hasCompositedLayerMapping()) {
            // If we're removing the compositedLayerMapping from a reflection, clear the source GraphicsLayer's pointer to
            // its replica GraphicsLayer. In practice this should never happen because reflectee and reflection
            // are both either composited, or not composited.
            if (layer->isReflection()) {
                Layer* sourceLayer = toLayoutBoxModelObject(layer->layoutObject()->parent())->layer();
                if (sourceLayer->hasCompositedLayerMapping()) {
                    ASSERT(sourceLayer->compositedLayerMapping()->mainGraphicsLayer()->replicaLayer() == layer->compositedLayerMapping()->mainGraphicsLayer());
                    sourceLayer->compositedLayerMapping()->mainGraphicsLayer()->setReplicatedByLayer(0);
                }
            }

            layer->clearCompositedLayerMapping();
            compositedLayerMappingChanged = true;
        }

        break;
    case RemoveFromSquashingLayer:
    case NoCompositingStateChange:
        // Do nothing.
        break;
    }

    if (compositedLayerMappingChanged && layer->layoutObject()->isLayoutPart()) {
        LayerCompositor* innerCompositor = frameContentsCompositor(toLayoutPart(layer->layoutObject()));
        if (innerCompositor && innerCompositor->staleInCompositingMode())
            innerCompositor->updateRootLayerAttachment();
    }

    if (compositedLayerMappingChanged)
        layer->clipper().clearClipRectsIncludingDescendants(PaintingClipRects);

    // If a fixed position layer gained/lost a compositedLayerMapping or the reason not compositing it changed,
    // the scrolling coordinator needs to recalculate whether it can do fast scrolling.
    if (compositedLayerMappingChanged) {
        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->frameViewFixedObjectsDidChange(m_layoutView.frameView());
    }

    return compositedLayerMappingChanged;
}

void LayerCompositor::paintInvalidationOnCompositingChange(Layer* layer)
{
    // If the renderer is not attached yet, no need to issue paint invalidations.
    if (layer->layoutObject() != &m_layoutView && !layer->layoutObject()->parent())
        return;

    // For querying Layer::compositingState()
    // Eager invalidation here is correct, since we are invalidating with respect to the previous frame's
    // compositing state when changing the compositing backing of the layer.
    DisableCompositingQueryAsserts disabler;
    // FIXME: We should not allow paint invalidation out of paint invalidation state. crbug.com/457415
    DisablePaintInvalidationStateAsserts paintInvalidationAssertisabler;

    layer->layoutObject()->invalidatePaintIncludingNonCompositingDescendants();
}

void LayerCompositor::frameViewDidChangeLocation(const IntPoint& contentsOffset)
{
    if (m_overflowControlsHostLayer)
        m_overflowControlsHostLayer->setPosition(contentsOffset);
}

void LayerCompositor::frameViewDidChangeSize()
{
    if (m_containerLayer) {
        FrameView* frameView = m_layoutView.frameView();
        m_containerLayer->setSize(frameView->unscaledVisibleContentSize());
        m_overflowControlsHostLayer->setSize(frameView->unscaledVisibleContentSize(IncludeScrollbars));

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

void LayerCompositor::frameViewDidScroll()
{
    FrameView* frameView = m_layoutView.frameView();
    IntPoint scrollPosition = frameView->scrollPosition();

    if (!m_scrollLayer)
        return;

    bool scrollingCoordinatorHandlesOffset = false;
    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator()) {
        if (Settings* settings = m_layoutView.document().settings()) {
            if (m_layoutView.frame()->isLocalRoot() || settings->preferCompositingToLCDTextEnabled())
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


    Platform::current()->histogramEnumeration("Renderer.AcceleratedFixedRootBackground",
        ScrolledMainFrameBucket,
        AcceleratedFixedRootBackgroundHistogramMax);
}

void LayerCompositor::frameViewScrollbarsExistenceDidChange()
{
    if (m_containerLayer)
        updateOverflowControlsLayers();
}

void LayerCompositor::rootFixedBackgroundsChanged()
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

bool LayerCompositor::scrollingLayerDidChange(Layer* layer)
{
    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->scrollableAreaScrollLayerDidChange(layer->scrollableArea());
    return false;
}

String LayerCompositor::layerTreeAsText(LayerTreeFlags flags)
{
    ASSERT(lifecycle().state() >= DocumentLifecycle::PaintInvalidationClean);

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
    // its paint invalidation rects, they must be included here.
    if (flags & LayerTreeIncludesPaintInvalidationRects)
        return m_layoutView.frameView()->trackedPaintInvalidationRectsAsText() + layerTreeText;

    return layerTreeText;
}

LayerCompositor* LayerCompositor::frameContentsCompositor(LayoutPart* renderer)
{
    if (!renderer->node()->isFrameOwnerElement())
        return 0;

    HTMLFrameOwnerElement* element = toHTMLFrameOwnerElement(renderer->node());
    if (Document* contentDocument = element->contentDocument()) {
        if (LayoutView* view = contentDocument->layoutView())
            return view->compositor();
    }
    return 0;
}

// FIXME: What does this function do? It needs a clearer name.
bool LayerCompositor::parentFrameContentLayers(LayoutPart* renderer)
{
    LayerCompositor* innerCompositor = frameContentsCompositor(renderer);
    if (!innerCompositor || !innerCompositor->staleInCompositingMode() || innerCompositor->rootLayerAttachment() != RootLayerAttachedViaEnclosingFrame)
        return false;

    Layer* layer = renderer->layer();
    if (!layer->hasCompositedLayerMapping())
        return false;

    CompositedLayerMapping* compositedLayerMapping = layer->compositedLayerMapping();
    GraphicsLayer* hostingLayer = compositedLayerMapping->parentForSublayers();
    GraphicsLayer* rootLayer = innerCompositor->rootGraphicsLayer();
    if (hostingLayer->children().size() != 1 || hostingLayer->children()[0] != rootLayer) {
        hostingLayer->removeAllChildren();
        hostingLayer->addChild(rootLayer);
    }
    return true;
}

static void fullyInvalidatePaintRecursive(Layer* layer)
{
    if (layer->compositingState() == PaintsIntoOwnBacking) {
        layer->compositedLayerMapping()->setContentsNeedDisplay();
        layer->compositedLayerMapping()->setSquashingContentsNeedDisplay();
    }

    for (Layer* child = layer->firstChild(); child; child = child->nextSibling())
        fullyInvalidatePaintRecursive(child);
}

void LayerCompositor::fullyInvalidatePaint()
{
    // We're walking all compositing layers and invalidating them, so there's
    // no need to have up-to-date compositing state.
    DisableCompositingQueryAsserts disabler;
    fullyInvalidatePaintRecursive(rootLayer());
}

Layer* LayerCompositor::rootLayer() const
{
    return m_layoutView.layer();
}

GraphicsLayer* LayerCompositor::rootGraphicsLayer() const
{
    if (m_overflowControlsHostLayer)
        return m_overflowControlsHostLayer.get();
    return m_rootContentLayer.get();
}

GraphicsLayer* LayerCompositor::frameScrollLayer() const
{
    return m_scrollLayer.get();
}

GraphicsLayer* LayerCompositor::scrollLayer() const
{
    if (ScrollableArea* scrollableArea = m_layoutView.frameView()->scrollableArea())
        return scrollableArea->layerForScrolling();
    return nullptr;
}

GraphicsLayer* LayerCompositor::containerLayer() const
{
    return m_containerLayer.get();
}

GraphicsLayer* LayerCompositor::ensureRootTransformLayer()
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

void LayerCompositor::setIsInWindow(bool isInWindow)
{
    if (!staleInCompositingMode())
        return;

    if (isInWindow) {
        if (m_rootLayerAttachment != RootLayerUnattached)
            return;

        RootLayerAttachment attachment = m_layoutView.frame()->isLocalRoot() ? RootLayerAttachedViaChromeClient : RootLayerAttachedViaEnclosingFrame;
        attachRootLayer(attachment);
    } else {
        if (m_rootLayerAttachment == RootLayerUnattached)
            return;

        detachRootLayer();
    }
}

void LayerCompositor::updateRootLayerPosition()
{
    if (m_rootContentLayer) {
        const IntRect& documentRect = m_layoutView.documentRect();
        m_rootContentLayer->setSize(documentRect.size());
        m_rootContentLayer->setPosition(documentRect.location());
    }
    if (m_containerLayer) {
        FrameView* frameView = m_layoutView.frameView();
        m_containerLayer->setSize(frameView->unscaledVisibleContentSize());
        m_overflowControlsHostLayer->setSize(frameView->unscaledVisibleContentSize(IncludeScrollbars));
    }
}

void LayerCompositor::updatePotentialCompositingReasonsFromStyle(Layer* layer)
{
    layer->setPotentialCompositingReasonsFromStyle(m_compositingReasonFinder.potentialCompositingReasonsFromStyle(layer->layoutObject()));
}

void LayerCompositor::updateDirectCompositingReasons(Layer* layer)
{
    layer->setCompositingReasons(m_compositingReasonFinder.directReasons(layer), CompositingReasonComboAllDirectReasons);
}

void LayerCompositor::setOverlayLayer(GraphicsLayer* layer)
{
    ASSERT(rootGraphicsLayer());

    if (layer->parent() != m_overflowControlsHostLayer.get())
        m_overflowControlsHostLayer->addChild(layer);
}

bool LayerCompositor::canBeComposited(const Layer* layer) const
{
    return m_hasAcceleratedCompositing && layer->isSelfPaintingLayer() && !layer->subtreeIsInvisible();
}

// Return true if the given layer is a stacking context and has compositing child
// layers that it needs to clip. In this case we insert a clipping GraphicsLayer
// into the hierarchy between this layer and its children in the z-order hierarchy.
bool LayerCompositor::clipsCompositingDescendants(const Layer* layer) const
{
    return layer->hasCompositingDescendant() && layer->layoutObject()->hasClipOrOverflowClip();
}

// If an element has composited negative z-index children, those children render in front of the
// layer background, so we need an extra 'contents' layer for the foreground of the layer
// object.
bool LayerCompositor::needsContentsCompositingLayer(const Layer* layer) const
{
    if (!layer->hasCompositingDescendant())
        return false;
    return layer->stackingNode()->hasNegativeZOrderList();
}

static void paintScrollbar(Scrollbar* scrollbar, GraphicsContext& context, const IntRect& clip)
{
    if (!scrollbar)
        return;

    // Frame scrollbars are painted in the space of the containing frame, not the local space of the scrollbar.
    const IntPoint& paintOffset = scrollbar->frameRect().location();
    IntRect transformedClip = clip;
    transformedClip.moveBy(paintOffset);

    AffineTransform translation;
    translation.translate(-paintOffset.x(), -paintOffset.y());
    TransformRecorder transformRecorder(context, scrollbar->displayItemClient(), translation);

    scrollbar->paint(&context, transformedClip);
}

void LayerCompositor::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& context, GraphicsLayerPaintingPhase, const IntRect& clip)
{
    if (graphicsLayer == layerForHorizontalScrollbar())
        paintScrollbar(m_layoutView.frameView()->horizontalScrollbar(), context, clip);
    else if (graphicsLayer == layerForVerticalScrollbar())
        paintScrollbar(m_layoutView.frameView()->verticalScrollbar(), context, clip);
    else if (graphicsLayer == layerForScrollCorner())
        FramePainter(*m_layoutView.frameView()).paintScrollCorner(&context, clip);
}

bool LayerCompositor::supportsFixedRootBackgroundCompositing() const
{
    if (Settings* settings = m_layoutView.document().settings())
        return settings->preferCompositingToLCDTextEnabled();
    return false;
}

bool LayerCompositor::needsFixedRootBackgroundLayer(const Layer* layer) const
{
    if (layer != m_layoutView.layer())
        return false;

    return supportsFixedRootBackgroundCompositing() && m_layoutView.rootBackgroundIsEntirelyFixed();
}

GraphicsLayer* LayerCompositor::fixedRootBackgroundLayer() const
{
    // Get the fixed root background from the LayoutView layer's compositedLayerMapping.
    Layer* viewLayer = m_layoutView.layer();
    if (!viewLayer)
        return 0;

    if (viewLayer->compositingState() == PaintsIntoOwnBacking && viewLayer->compositedLayerMapping()->backgroundLayerPaintsFixedRootBackground())
        return viewLayer->compositedLayerMapping()->backgroundLayer();

    return 0;
}

static void resetTrackedPaintInvalidationRectsRecursive(GraphicsLayer* graphicsLayer)
{
    if (!graphicsLayer)
        return;

    graphicsLayer->resetTrackedPaintInvalidations();

    for (size_t i = 0; i < graphicsLayer->children().size(); ++i)
        resetTrackedPaintInvalidationRectsRecursive(graphicsLayer->children()[i]);

    if (GraphicsLayer* replicaLayer = graphicsLayer->replicaLayer())
        resetTrackedPaintInvalidationRectsRecursive(replicaLayer);

    if (GraphicsLayer* maskLayer = graphicsLayer->maskLayer())
        resetTrackedPaintInvalidationRectsRecursive(maskLayer);

    if (GraphicsLayer* clippingMaskLayer = graphicsLayer->contentsClippingMaskLayer())
        resetTrackedPaintInvalidationRectsRecursive(clippingMaskLayer);
}

void LayerCompositor::resetTrackedPaintInvalidationRects()
{
    if (GraphicsLayer* rootLayer = rootGraphicsLayer())
        resetTrackedPaintInvalidationRectsRecursive(rootLayer);
}

void LayerCompositor::setTracksPaintInvalidations(bool tracksPaintInvalidations)
{
    ASSERT(lifecycle().state() == DocumentLifecycle::PaintInvalidationClean);
    m_isTrackingPaintInvalidations = tracksPaintInvalidations;
}

bool LayerCompositor::isTrackingPaintInvalidations() const
{
    return m_isTrackingPaintInvalidations;
}

bool LayerCompositor::requiresHorizontalScrollbarLayer() const
{
    return m_layoutView.frameView()->horizontalScrollbar();
}

bool LayerCompositor::requiresVerticalScrollbarLayer() const
{
    return m_layoutView.frameView()->verticalScrollbar();
}

bool LayerCompositor::requiresScrollCornerLayer() const
{
    return m_layoutView.frameView()->isScrollCornerVisible();
}

void LayerCompositor::updateOverflowControlsLayers()
{
    GraphicsLayer* controlsParent = m_rootTransformLayer.get() ? m_rootTransformLayer.get() : m_overflowControlsHostLayer.get();
    // On Mac, main frame scrollbars should always be stuck to the sides of the screen (in overscroll and in pinch-zoom), so
    // make the parent for the scrollbars be the viewport container layer.
#if OS(MACOSX)
    if (m_layoutView.frame()->isMainFrame() && m_layoutView.frame()->settings()->pinchVirtualViewportEnabled()) {
        PinchViewport& pinchViewport = m_layoutView.frameView()->page()->frameHost().pinchViewport();
        controlsParent = pinchViewport.containerLayer();
    }
#endif

    if (requiresHorizontalScrollbarLayer()) {
        if (!m_layerForHorizontalScrollbar) {
            m_layerForHorizontalScrollbar = GraphicsLayer::create(graphicsLayerFactory(), this);
        }

        if (m_layerForHorizontalScrollbar->parent() != controlsParent) {
            controlsParent->addChild(m_layerForHorizontalScrollbar.get());

            if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_layoutView.frameView(), HorizontalScrollbar);
        }
    } else if (m_layerForHorizontalScrollbar) {
        m_layerForHorizontalScrollbar->removeFromParent();
        m_layerForHorizontalScrollbar = nullptr;

        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_layoutView.frameView(), HorizontalScrollbar);
    }

    if (requiresVerticalScrollbarLayer()) {
        if (!m_layerForVerticalScrollbar) {
            m_layerForVerticalScrollbar = GraphicsLayer::create(graphicsLayerFactory(), this);
        }

        if (m_layerForVerticalScrollbar->parent() != controlsParent) {
            controlsParent->addChild(m_layerForVerticalScrollbar.get());

            if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_layoutView.frameView(), VerticalScrollbar);
        }
    } else if (m_layerForVerticalScrollbar) {
        m_layerForVerticalScrollbar->removeFromParent();
        m_layerForVerticalScrollbar = nullptr;

        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_layoutView.frameView(), VerticalScrollbar);
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

    m_layoutView.frameView()->positionScrollbarLayers();
}

void LayerCompositor::ensureRootLayer()
{
    RootLayerAttachment expectedAttachment = m_layoutView.frame()->isLocalRoot() ? RootLayerAttachedViaChromeClient : RootLayerAttachedViaEnclosingFrame;
    if (expectedAttachment == m_rootLayerAttachment)
        return;

    Settings* settings = m_layoutView.document().settings();
    if (!m_rootContentLayer) {
        m_rootContentLayer = GraphicsLayer::create(graphicsLayerFactory(), this);
        IntRect overflowRect = m_layoutView.pixelSnappedLayoutOverflowRect();
        m_rootContentLayer->setSize(FloatSize(overflowRect.maxX(), overflowRect.maxY()));
        m_rootContentLayer->setPosition(FloatPoint());
        m_rootContentLayer->setOwnerNodeId(InspectorNodeIds::idForNode(m_layoutView.generatingNode()));

        // FIXME: with rootLayerScrolls, we probably don't even need m_rootContentLayer?
        if (!(settings && settings->rootLayerScrolls())) {
            // Need to clip to prevent transformed content showing outside this frame
            m_rootContentLayer->setMasksToBounds(true);
        }
    }

    if (!m_overflowControlsHostLayer) {
        ASSERT(!m_scrollLayer);
        ASSERT(!m_containerLayer);

        // Create a layer to host the clipping layer and the overflow controls layers.
        m_overflowControlsHostLayer = GraphicsLayer::create(graphicsLayerFactory(), this);

        // Clip iframe's overflow controls layer.
        bool containerMasksToBounds = !m_layoutView.frame()->isLocalRoot();
        m_overflowControlsHostLayer->setMasksToBounds(containerMasksToBounds);

        // Create a clipping layer if this is an iframe or settings require to clip.
        m_containerLayer = GraphicsLayer::create(graphicsLayerFactory(), this);
        m_containerLayer->setMasksToBounds(containerMasksToBounds);

        m_scrollLayer = GraphicsLayer::create(graphicsLayerFactory(), this);
        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->setLayerIsContainerForFixedPositionLayers(m_scrollLayer.get(), true);

        // Hook them up
        m_overflowControlsHostLayer->addChild(m_containerLayer.get());
        m_containerLayer->addChild(m_scrollLayer.get());
        m_scrollLayer->addChild(m_rootContentLayer.get());

        frameViewDidChangeSize();
    }

    // Check to see if we have to change the attachment
    if (m_rootLayerAttachment != RootLayerUnattached)
        detachRootLayer();

    attachRootLayer(expectedAttachment);
}

void LayerCompositor::destroyRootLayer()
{
    if (!m_rootContentLayer)
        return;

    detachRootLayer();

    if (m_layerForHorizontalScrollbar) {
        m_layerForHorizontalScrollbar->removeFromParent();
        m_layerForHorizontalScrollbar = nullptr;
        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_layoutView.frameView(), HorizontalScrollbar);
        if (Scrollbar* horizontalScrollbar = m_layoutView.frameView()->verticalScrollbar())
            m_layoutView.frameView()->invalidateScrollbar(horizontalScrollbar, IntRect(IntPoint(0, 0), horizontalScrollbar->frameRect().size()));
    }

    if (m_layerForVerticalScrollbar) {
        m_layerForVerticalScrollbar->removeFromParent();
        m_layerForVerticalScrollbar = nullptr;
        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_layoutView.frameView(), VerticalScrollbar);
        if (Scrollbar* verticalScrollbar = m_layoutView.frameView()->verticalScrollbar())
            m_layoutView.frameView()->invalidateScrollbar(verticalScrollbar, IntRect(IntPoint(0, 0), verticalScrollbar->frameRect().size()));
    }

    if (m_layerForScrollCorner) {
        m_layerForScrollCorner = nullptr;
        m_layoutView.frameView()->invalidateScrollCorner(m_layoutView.frameView()->scrollCornerRect());
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

void LayerCompositor::attachRootLayer(RootLayerAttachment attachment)
{
    if (!m_rootContentLayer)
        return;

    switch (attachment) {
    case RootLayerUnattached:
        ASSERT_NOT_REACHED();
        break;
    case RootLayerAttachedViaChromeClient: {
        LocalFrame& frame = m_layoutView.frameView()->frame();
        Page* page = frame.page();
        if (!page)
            return;
        page->chrome().client().attachRootGraphicsLayer(rootGraphicsLayer(), &frame);
        break;
    }
    case RootLayerAttachedViaEnclosingFrame: {
        HTMLFrameOwnerElement* ownerElement = m_layoutView.document().ownerElement();
        ASSERT(ownerElement);
        // The layer will get hooked up via CompositedLayerMapping::updateGraphicsLayerConfiguration()
        // for the frame's renderer in the parent document.
        ownerElement->setNeedsCompositingUpdate();
        break;
    }
    }

    m_rootLayerAttachment = attachment;
}

void LayerCompositor::detachRootLayer()
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

        if (HTMLFrameOwnerElement* ownerElement = m_layoutView.document().ownerElement())
            ownerElement->setNeedsCompositingUpdate();
        break;
    }
    case RootLayerAttachedViaChromeClient: {
        LocalFrame& frame = m_layoutView.frameView()->frame();
        Page* page = frame.page();
        if (!page)
            return;
        page->chrome().client().attachRootGraphicsLayer(0, &frame);
        break;
    }
    case RootLayerUnattached:
        break;
    }

    m_rootLayerAttachment = RootLayerUnattached;
}

void LayerCompositor::updateRootLayerAttachment()
{
    ensureRootLayer();
}

ScrollingCoordinator* LayerCompositor::scrollingCoordinator() const
{
    if (Page* page = this->page())
        return page->scrollingCoordinator();

    return 0;
}

GraphicsLayerFactory* LayerCompositor::graphicsLayerFactory() const
{
    if (Page* page = this->page())
        return page->chrome().client().graphicsLayerFactory();
    return 0;
}

Page* LayerCompositor::page() const
{
    return m_layoutView.frameView()->frame().page();
}

DocumentLifecycle& LayerCompositor::lifecycle() const
{
    return m_layoutView.document().lifecycle();
}

String LayerCompositor::debugName(const GraphicsLayer* graphicsLayer)
{
    String name;
    if (graphicsLayer == m_rootContentLayer.get()) {
        name = "Content Root Layer";
    } else if (graphicsLayer == m_rootTransformLayer.get()) {
        name = "Root Transform Layer";
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

} // namespace blink
