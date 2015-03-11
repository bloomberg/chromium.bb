/*
 * Copyright (C) 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "core/layout/compositing/CompositedLayerMapping.h"

#include "core/HTMLNames.h"
#include "core/fetch/ImageResource.h"
#include "core/frame/FrameView.h"
#include "core/frame/RemoteFrame.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorNodeIds.h"
#include "core/layout/FilterEffectRenderer.h"
#include "core/layout/LayerStackingNodeIterator.h"
#include "core/layout/LayoutEmbeddedObject.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutVideo.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/LayerCompositor.h"
#include "core/layout/style/KeyframeList.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/paint/LayerPainter.h"
#include "core/paint/ScrollableAreaPainter.h"
#include "core/paint/TransformRecorder.h"
#include "core/plugins/PluginView.h"
#include "platform/LengthFunctions.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/FontCache.h"
#include "platform/geometry/TransformState.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/TransformDisplayItem.h"
#include "wtf/CurrentTime.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

static IntRect clipBox(LayoutBox* renderer);

static IntRect contentsRect(const LayoutObject* renderer)
{
    if (!renderer->isBox())
        return IntRect();

    return renderer->isVideo() ?
        toLayoutVideo(renderer)->videoBox() :
        pixelSnappedIntRect(toLayoutBox(renderer)->contentBoxRect());
}

static IntRect backgroundRect(const LayoutObject* renderer)
{
    if (!renderer->isBox())
        return IntRect();

    LayoutRect rect;
    const LayoutBox* box = toLayoutBox(renderer);
    EFillBox clip = box->style()->backgroundClip();
    switch (clip) {
    case BorderFillBox:
        rect = box->borderBoxRect();
        break;
    case PaddingFillBox:
        rect = box->paddingBoxRect();
        break;
    case ContentFillBox:
        rect = box->contentBoxRect();
        break;
    case TextFillBox:
        break;
    }

    return pixelSnappedIntRect(rect);
}

static inline bool isAcceleratedCanvas(const LayoutObject* renderer)
{
    if (renderer->isCanvas()) {
        HTMLCanvasElement* canvas = toHTMLCanvasElement(renderer->node());
        if (CanvasRenderingContext* context = canvas->renderingContext())
            return context->isAccelerated();
    }
    return false;
}

static bool hasBoxDecorationsOrBackgroundImage(const LayoutStyle& style)
{
    return style.hasBoxDecorations() || style.hasBackgroundImage();
}

static bool contentLayerSupportsDirectBackgroundComposition(const LayoutObject* renderer)
{
    // No support for decorations - border, border-radius or outline.
    // Only simple background - solid color or transparent.
    if (hasBoxDecorationsOrBackgroundImage(renderer->styleRef()))
        return false;

    // If there is no background, there is nothing to support.
    if (!renderer->style()->hasBackground())
        return true;

    // Simple background that is contained within the contents rect.
    return contentsRect(renderer).contains(backgroundRect(renderer));
}

static WebLayer* platformLayerForPlugin(LayoutObject* renderer)
{
    if (!renderer->isEmbeddedObject())
        return 0;
    Widget* widget = toLayoutEmbeddedObject(renderer)->widget();
    if (!widget || !widget->isPluginView())
        return 0;
    return toPluginView(widget)->platformLayer();

}

static inline bool isAcceleratedContents(LayoutObject* renderer)
{
    return isAcceleratedCanvas(renderer)
        || (renderer->isEmbeddedObject() && toLayoutEmbeddedObject(renderer)->requiresAcceleratedCompositing())
        || renderer->isVideo();
}

// Get the scrolling coordinator in a way that works inside CompositedLayerMapping's destructor.
static ScrollingCoordinator* scrollingCoordinatorFromLayer(Layer& layer)
{
    Page* page = layer.layoutObject()->frame()->page();
    if (!page)
        return 0;

    return page->scrollingCoordinator();
}

CompositedLayerMapping::CompositedLayerMapping(Layer& layer)
    : m_owningLayer(layer)
    , m_contentOffsetInCompositingLayerDirty(false)
    , m_pendingUpdateScope(GraphicsLayerUpdateNone)
    , m_isMainFrameLayoutViewLayer(false)
    , m_backgroundLayerPaintsFixedRootBackground(false)
    , m_scrollingContentsAreEmpty(false)
{
    if (layer.isRootLayer() && layoutObject()->frame()->isMainFrame())
        m_isMainFrameLayoutViewLayer = true;

    createPrimaryGraphicsLayer();
}

CompositedLayerMapping::~CompositedLayerMapping()
{
    // Hits in compositing/squashing/squash-onto-nephew.html.
    DisableCompositingQueryAsserts disabler;

    // Do not leave the destroyed pointer dangling on any Layers that painted to this mapping's squashing layer.
    for (size_t i = 0; i < m_squashedLayers.size(); ++i) {
        Layer* oldSquashedLayer = m_squashedLayers[i].renderLayer;
        if (oldSquashedLayer->groupedMapping() == this) {
            oldSquashedLayer->setGroupedMapping(0, true);
            oldSquashedLayer->setLostGroupedMapping(true);
        }
    }

    updateClippingLayers(false, false);
    updateOverflowControlsLayers(false, false, false, false);
    updateChildTransformLayer(false);
    updateForegroundLayer(false);
    updateBackgroundLayer(false);
    updateMaskLayer(false);
    updateClippingMaskLayers(false);
    updateScrollingLayers(false);
    updateSquashingLayers(false);
    destroyGraphicsLayers();
}

PassOwnPtr<GraphicsLayer> CompositedLayerMapping::createGraphicsLayer(CompositingReasons reasons)
{
    GraphicsLayerFactory* graphicsLayerFactory = 0;
    if (Page* page = layoutObject()->frame()->page())
        graphicsLayerFactory = page->chrome().client().graphicsLayerFactory();

    OwnPtr<GraphicsLayer> graphicsLayer = GraphicsLayer::create(graphicsLayerFactory, this);

    graphicsLayer->setCompositingReasons(reasons);
    if (Node* owningNode = m_owningLayer.layoutObject()->generatingNode())
        graphicsLayer->setOwnerNodeId(InspectorNodeIds::idForNode(owningNode));

    return graphicsLayer.release();
}

void CompositedLayerMapping::createPrimaryGraphicsLayer()
{
    m_graphicsLayer = createGraphicsLayer(m_owningLayer.compositingReasons());

#if !OS(ANDROID)
    if (m_isMainFrameLayoutViewLayer)
        m_graphicsLayer->contentLayer()->setDrawCheckerboardForMissingTiles(true);
#endif

    updateOpacity(layoutObject()->styleRef());
    updateTransform(layoutObject()->styleRef());
    updateFilters(layoutObject()->styleRef());

    if (RuntimeEnabledFeatures::cssCompositingEnabled()) {
        updateLayerBlendMode(layoutObject()->styleRef());
        updateIsRootForIsolatedGroup();
    }

    updateScrollBlocksOn(layoutObject()->styleRef());
}

void CompositedLayerMapping::destroyGraphicsLayers()
{
    if (m_graphicsLayer)
        m_graphicsLayer->removeFromParent();

    m_ancestorClippingLayer = nullptr;
    m_graphicsLayer = nullptr;
    m_foregroundLayer = nullptr;
    m_backgroundLayer = nullptr;
    m_childContainmentLayer = nullptr;
    m_childTransformLayer = nullptr;
    m_maskLayer = nullptr;
    m_childClippingMaskLayer = nullptr;

    m_scrollingLayer = nullptr;
    m_scrollingContentsLayer = nullptr;
    m_scrollingBlockSelectionLayer = nullptr;
}

void CompositedLayerMapping::updateOpacity(const LayoutStyle& style)
{
    m_graphicsLayer->setOpacity(compositingOpacity(style.opacity()));
}

void CompositedLayerMapping::updateTransform(const LayoutStyle& style)
{
    // FIXME: This could use m_owningLayer.transform(), but that currently has transform-origin
    // baked into it, and we don't want that.
    TransformationMatrix t;
    if (m_owningLayer.hasTransformRelatedProperty()) {
        style.applyTransform(t, LayoutSize(toLayoutBox(layoutObject())->pixelSnappedSize()), LayoutStyle::ExcludeTransformOrigin);
        makeMatrixRenderable(t, compositor()->hasAcceleratedCompositing());
    }

    m_graphicsLayer->setTransform(t);
}

void CompositedLayerMapping::updateFilters(const LayoutStyle& style)
{
    m_graphicsLayer->setFilters(owningLayer().computeFilterOperations(style));
}

void CompositedLayerMapping::updateLayerBlendMode(const LayoutStyle& style)
{
    setBlendMode(style.blendMode());
}

void CompositedLayerMapping::updateIsRootForIsolatedGroup()
{
    bool isolate = m_owningLayer.shouldIsolateCompositedDescendants();

    // non stacking context layers should never isolate
    ASSERT(m_owningLayer.stackingNode()->isStackingContext() || !isolate);

    m_graphicsLayer->setIsRootForIsolatedGroup(isolate);
}

void CompositedLayerMapping::updateScrollBlocksOn(const LayoutStyle& style)
{
    // Note that blink determines the default scroll blocking policy, even
    // when the scroll-blocks-on CSS feature isn't enabled.
    WebScrollBlocksOn blockingMode = WebScrollBlocksOnStartTouch | WebScrollBlocksOnWheelEvent;

    if (RuntimeEnabledFeatures::cssScrollBlocksOnEnabled())
        blockingMode = style.scrollBlocksOn();

    m_graphicsLayer->setScrollBlocksOn(blockingMode);
}

void CompositedLayerMapping::updateContentsOpaque()
{
    ASSERT(m_isMainFrameLayoutViewLayer || !m_backgroundLayer);
    if (isAcceleratedCanvas(layoutObject())) {
        // Determine whether the rendering context's external texture layer is opaque.
        CanvasRenderingContext* context = toHTMLCanvasElement(layoutObject()->node())->renderingContext();
        if (!context->hasAlpha())
            m_graphicsLayer->setContentsOpaque(true);
        else if (WebLayer* layer = context->platformLayer())
            m_graphicsLayer->setContentsOpaque(!Color(layer->backgroundColor()).hasAlpha());
        else
            m_graphicsLayer->setContentsOpaque(false);
    } else if (m_backgroundLayer) {
        m_graphicsLayer->setContentsOpaque(false);
        m_backgroundLayer->setContentsOpaque(m_owningLayer.backgroundIsKnownToBeOpaqueInRect(compositedBounds()));
    } else {
        // For non-root layers, background is always painted by the primary graphics layer.
        m_graphicsLayer->setContentsOpaque(m_owningLayer.backgroundIsKnownToBeOpaqueInRect(compositedBounds()));
    }
}

void CompositedLayerMapping::updateCompositedBounds()
{
    ASSERT(m_owningLayer.compositor()->lifecycle().state() == DocumentLifecycle::InCompositingUpdate);
    // FIXME: if this is really needed for performance, it would be better to store it on Layer.
    m_compositedBounds = m_owningLayer.boundingBoxForCompositing();
    m_contentOffsetInCompositingLayerDirty = true;
}

void CompositedLayerMapping::updateAfterPartResize()
{
    if (layoutObject()->isLayoutPart()) {
        if (LayerCompositor* innerCompositor = LayerCompositor::frameContentsCompositor(toLayoutPart(layoutObject()))) {
            innerCompositor->frameViewDidChangeSize();
            // We can floor this point because our frameviews are always aligned to pixel boundaries.
            ASSERT(m_compositedBounds.location() == flooredIntPoint(m_compositedBounds.location()));
            innerCompositor->frameViewDidChangeLocation(flooredIntPoint(contentsBox().location()));
        }
    }
}

void CompositedLayerMapping::updateCompositingReasons()
{
    // All other layers owned by this mapping will have the same compositing reason
    // for their lifetime, so they are initialized only when created.
    m_graphicsLayer->setCompositingReasons(m_owningLayer.compositingReasons());
}

bool CompositedLayerMapping::owningLayerClippedByLayerNotAboveCompositedAncestor()
{
    if (!m_owningLayer.parent())
        return false;

    const Layer* compositingAncestor = m_owningLayer.enclosingLayerWithCompositedLayerMapping(ExcludeSelf);
    if (!compositingAncestor)
        return false;

    const LayoutObject* clippingContainer = m_owningLayer.clippingContainer();
    if (!clippingContainer)
        return false;

    if (compositingAncestor->layoutObject()->isDescendantOf(clippingContainer))
        return false;

    // We ignore overflow clip here; we want composited overflow content to
    // behave as if it lives in an unclipped universe so it can prepaint, etc.
    // This means that we need to check if we are actually clipped before
    // setting up m_ancestorClippingLayer otherwise
    // updateAncestorClippingLayerGeometry will fail as the clip rect will be
    // infinite.
    // FIXME: this should use cached clip rects, but this sometimes give
    // inaccurate results (and trips the ASSERTS in LayerClipper).
    ClipRectsContext clipRectsContext(compositingAncestor, UncachedClipRects, IgnoreOverlayScrollbarSize);
    clipRectsContext.setIgnoreOverflowClip();
    IntRect parentClipRect = pixelSnappedIntRect(m_owningLayer.clipper().backgroundClipRect(clipRectsContext).rect());
    return parentClipRect != LayoutRect::infiniteIntRect();
}

bool CompositedLayerMapping::updateGraphicsLayerConfiguration()
{
    ASSERT(m_owningLayer.compositor()->lifecycle().state() == DocumentLifecycle::InCompositingUpdate);

    // Note carefully: here we assume that the compositing state of all descendants have been updated already,
    // so it is legitimate to compute and cache the composited bounds for this layer.
    updateCompositedBounds();

    if (LayerReflectionInfo* reflection = m_owningLayer.reflectionInfo()) {
        if (reflection->reflectionLayer()->hasCompositedLayerMapping())
            reflection->reflectionLayer()->compositedLayerMapping()->updateCompositedBounds();
    }

    LayerCompositor* compositor = this->compositor();
    LayoutObject* renderer = this->layoutObject();

    bool layerConfigChanged = false;
    setBackgroundLayerPaintsFixedRootBackground(compositor->needsFixedRootBackgroundLayer(&m_owningLayer));

    // The background layer is currently only used for fixed root backgrounds.
    if (updateBackgroundLayer(m_backgroundLayerPaintsFixedRootBackground))
        layerConfigChanged = true;

    if (updateForegroundLayer(compositor->needsContentsCompositingLayer(&m_owningLayer)))
        layerConfigChanged = true;

    bool needsDescendantsClippingLayer = compositor->clipsCompositingDescendants(&m_owningLayer);

    // Our scrolling layer will clip.
    if (m_owningLayer.needsCompositedScrolling())
        needsDescendantsClippingLayer = false;

    Layer* scrollParent = compositor->preferCompositingToLCDTextEnabled() ? m_owningLayer.scrollParent() : 0;

    // This is required because compositing layers are parented
    // according to the z-order hierarchy, yet clipping goes down the renderer hierarchy.
    // Thus, a Layer can be clipped by a Layer that is an ancestor in the renderer hierarchy,
    // but a sibling in the z-order hierarchy. Further, that sibling need not be composited at all.
    // In such scenarios, an ancestor clipping layer is necessary to apply the composited clip for this layer.
    bool needsAncestorClip = owningLayerClippedByLayerNotAboveCompositedAncestor();

    if (scrollParent) {
        // If our containing block is our ancestor scrolling layer, then we'll already be clipped
        // to it via our scroll parent and we don't need an ancestor clipping layer.
        if (m_owningLayer.layoutObject()->containingBlock()->enclosingLayer() == m_owningLayer.ancestorScrollingLayer())
            needsAncestorClip = false;
    }

    if (updateClippingLayers(needsAncestorClip, needsDescendantsClippingLayer))
        layerConfigChanged = true;

    bool scrollingConfigChanged = false;
    if (updateScrollingLayers(m_owningLayer.needsCompositedScrolling())) {
        layerConfigChanged = true;
        scrollingConfigChanged = true;
    }

    if (updateOverflowControlsLayers(requiresHorizontalScrollbarLayer(), requiresVerticalScrollbarLayer(), requiresScrollCornerLayer(), needsAncestorClip))
        layerConfigChanged = true;

    bool hasPerspective = false;
    // FIXME: Can |style| be really null that late in the DocumentCycle?
    if (const LayoutStyle* style = renderer->style())
        hasPerspective = style->hasPerspective();
    bool needsChildTransformLayer = hasPerspective && (layerForChildrenTransform() == m_childTransformLayer.get()) && renderer->isBox();
    if (updateChildTransformLayer(needsChildTransformLayer))
        layerConfigChanged = true;

    if (updateSquashingLayers(!m_squashedLayers.isEmpty()))
        layerConfigChanged = true;

    updateScrollParent(scrollParent);
    updateClipParent();

    if (layerConfigChanged)
        updateInternalHierarchy();

    if (scrollingConfigChanged) {
        if (renderer->view())
            compositor->scrollingLayerDidChange(&m_owningLayer);
    }

    // A mask layer is not part of the hierarchy proper, it's an auxiliary layer
    // that's plugged into another GraphicsLayer that is part of the hierarchy.
    // It has no parent or child GraphicsLayer. For that reason, we process it
    // here, after the hierarchy has been updated.
    bool maskLayerChanged = false;
    if (updateMaskLayer(renderer->hasMask())) {
        maskLayerChanged = true;
        m_graphicsLayer->setMaskLayer(m_maskLayer.get());
    }

    bool hasChildClippingLayer = compositor->clipsCompositingDescendants(&m_owningLayer) && (hasClippingLayer() || hasScrollingLayer());
    // If we have a border radius or clip path on a scrolling layer, we need a clipping mask to properly
    // clip the scrolled contents, even if there are no composited descendants.
    bool hasClipPath = renderer->style()->clipPath();
    bool needsChildClippingMask = (hasClipPath || renderer->style()->hasBorderRadius()) && (hasChildClippingLayer || isAcceleratedContents(renderer) || hasScrollingLayer());
    if (updateClippingMaskLayers(needsChildClippingMask)) {
        // Clip path clips the entire subtree, including scrollbars. It must be attached directly onto
        // the main m_graphicsLayer.
        if (hasClipPath)
            m_graphicsLayer->setMaskLayer(m_childClippingMaskLayer.get());
        else if (hasClippingLayer())
            clippingLayer()->setMaskLayer(m_childClippingMaskLayer.get());
        else if (hasScrollingLayer())
            scrollingLayer()->setMaskLayer(m_childClippingMaskLayer.get());
        else if (isAcceleratedContents(renderer))
            m_graphicsLayer->setContentsClippingMaskLayer(m_childClippingMaskLayer.get());
    }

    if (m_owningLayer.reflectionInfo()) {
        if (m_owningLayer.reflectionInfo()->reflectionLayer()->hasCompositedLayerMapping()) {
            GraphicsLayer* reflectionLayer = m_owningLayer.reflectionInfo()->reflectionLayer()->compositedLayerMapping()->mainGraphicsLayer();
            m_graphicsLayer->setReplicatedByLayer(reflectionLayer);
        }
    } else {
        m_graphicsLayer->setReplicatedByLayer(0);
    }

    updateBackgroundColor();

    if (renderer->isImage()) {
        if (isDirectlyCompositedImage()) {
            updateImageContents();
        } else if (m_graphicsLayer->hasContentsLayer()) {
            m_graphicsLayer->setContentsToImage(0);
        }
    }

    if (WebLayer* layer = platformLayerForPlugin(renderer)) {
        m_graphicsLayer->setContentsToPlatformLayer(layer);
    } else if (renderer->node() && renderer->node()->isFrameOwnerElement() && toHTMLFrameOwnerElement(renderer->node())->contentFrame()) {
        Frame* frame = toHTMLFrameOwnerElement(renderer->node())->contentFrame();
        if (frame->isRemoteFrame()) {
            WebLayer* layer = toRemoteFrame(frame)->remotePlatformLayer();
            m_graphicsLayer->setContentsToPlatformLayer(layer);
        }
    } else if (renderer->isVideo()) {
        HTMLMediaElement* mediaElement = toHTMLMediaElement(renderer->node());
        m_graphicsLayer->setContentsToPlatformLayer(mediaElement->platformLayer());
    } else if (isAcceleratedCanvas(renderer)) {
        HTMLCanvasElement* canvas = toHTMLCanvasElement(renderer->node());
        if (CanvasRenderingContext* context = canvas->renderingContext())
            m_graphicsLayer->setContentsToPlatformLayer(context->platformLayer());
        layerConfigChanged = true;
    }
    if (renderer->isLayoutPart())
        layerConfigChanged = LayerCompositor::parentFrameContentLayers(toLayoutPart(renderer));

    // Changes to either the internal hierarchy or the mask layer have an impact
    // on painting phases, so we need to update when either are updated.
    if (layerConfigChanged || maskLayerChanged)
        updatePaintingPhases();

    return layerConfigChanged;
}

static IntRect clipBox(LayoutBox* renderer)
{
    LayoutRect result = LayoutRect(LayoutRect::infiniteIntRect());
    if (renderer->hasOverflowClip())
        result = renderer->overflowClipRect(LayoutPoint());

    if (renderer->hasClip())
        result.intersect(renderer->clipRect(LayoutPoint()));

    return pixelSnappedIntRect(result);
}

static LayoutPoint computeOffsetFromCompositedAncestor(const Layer* layer, const Layer* compositedAncestor)
{
    LayoutPoint offset = layer->visualOffsetFromAncestor(compositedAncestor);
    if (compositedAncestor)
        offset.move(compositedAncestor->compositedLayerMapping()->owningLayer().subpixelAccumulation());
    return offset;
}

void CompositedLayerMapping::computeBoundsOfOwningLayer(const Layer* compositedAncestor, IntRect& localBounds, IntRect& compositingBoundsRelativeToCompositedAncestor, LayoutPoint& offsetFromCompositedAncestor,
    IntPoint& snappedOffsetFromCompositedAncestor)
{
    LayoutRect localRawCompositingBounds = compositedBounds();
    offsetFromCompositedAncestor = computeOffsetFromCompositedAncestor(&m_owningLayer, compositedAncestor);
    snappedOffsetFromCompositedAncestor = IntPoint(offsetFromCompositedAncestor.x().round(), offsetFromCompositedAncestor.y().round());

    LayoutSize subpixelAccumulation = offsetFromCompositedAncestor - snappedOffsetFromCompositedAncestor;
    m_owningLayer.setSubpixelAccumulation(subpixelAccumulation);

    // Move the bounds by the subpixel accumulation so that it pixel-snaps relative to absolute pixels instead of local coordinates.
    localRawCompositingBounds.move(subpixelAccumulation);
    localBounds = pixelSnappedIntRect(localRawCompositingBounds);

    compositingBoundsRelativeToCompositedAncestor = localBounds;
    compositingBoundsRelativeToCompositedAncestor.moveBy(snappedOffsetFromCompositedAncestor);
}

void CompositedLayerMapping::updateSquashingLayerGeometry(const LayoutPoint& offsetFromCompositedAncestor, const IntPoint& graphicsLayerParentLocation, const Layer& referenceLayer,
    Vector<GraphicsLayerPaintInfo>& layers, GraphicsLayer* squashingLayer, LayoutPoint* offsetFromTransformedAncestor, Vector<Layer*>& layersNeedingPaintInvalidation)
{
    if (!squashingLayer)
        return;

    LayoutPoint offsetFromReferenceLayerToParentGraphicsLayer(offsetFromCompositedAncestor);
    offsetFromReferenceLayerToParentGraphicsLayer.moveBy(-graphicsLayerParentLocation);

    // FIXME: Cache these offsets.
    LayoutPoint referenceOffsetFromTransformedAncestor = referenceLayer.computeOffsetFromTransformedAncestor();

    LayoutRect totalSquashBounds;
    for (size_t i = 0; i < layers.size(); ++i) {
        LayoutRect squashedBounds = layers[i].renderLayer->boundingBoxForCompositing();

        // Store the local bounds of the Layer subtree before applying the offset.
        layers[i].compositedBounds = squashedBounds;

        LayoutPoint offsetFromTransformedAncestorForSquashedLayer = layers[i].renderLayer->computeOffsetFromTransformedAncestor();
        LayoutSize offsetFromSquashingLayer = offsetFromTransformedAncestorForSquashedLayer - referenceOffsetFromTransformedAncestor;

        squashedBounds.move(offsetFromSquashingLayer);
        totalSquashBounds.unite(squashedBounds);
    }

    // The totalSquashBounds is positioned with respect to referenceLayer of this CompositedLayerMapping.
    // But the squashingLayer needs to be positioned with respect to the ancestor CompositedLayerMapping.
    // The conversion between referenceLayer and the ancestor CLM is already computed as
    // offsetFromReferenceLayerToParentGraphicsLayer.
    totalSquashBounds.moveBy(offsetFromReferenceLayerToParentGraphicsLayer);
    const IntRect squashLayerBounds = enclosingIntRect(totalSquashBounds);
    const IntPoint squashLayerOrigin = squashLayerBounds.location();
    const LayoutSize squashLayerOriginInOwningLayerSpace = squashLayerOrigin - offsetFromReferenceLayerToParentGraphicsLayer;

    // Now that the squashing bounds are known, we can convert the Layer painting offsets
    // from CLM owning layer space to the squashing layer space.
    //
    // The painting offset we want to compute for each squashed Layer is essentially the position of
    // the squashed Layer described w.r.t. referenceLayer's origin.
    // So we just need to convert that point from referenceLayer space to the squashing layer's
    // space. This is simply done by subtracing squashLayerOriginInOwningLayerSpace, but then the offset
    // overall needs to be negated because that's the direction that the painting code expects the
    // offset to be.
    for (size_t i = 0; i < layers.size(); ++i) {
        const LayoutPoint offsetFromTransformedAncestorForSquashedLayer = layers[i].renderLayer->computeOffsetFromTransformedAncestor();
        const LayoutSize offsetFromSquashLayerOrigin = (offsetFromTransformedAncestorForSquashedLayer - referenceOffsetFromTransformedAncestor) - squashLayerOriginInOwningLayerSpace;

        IntSize newOffsetFromRenderer = -IntSize(offsetFromSquashLayerOrigin.width().round(), offsetFromSquashLayerOrigin.height().round());
        LayoutSize subpixelAccumulation = offsetFromSquashLayerOrigin + newOffsetFromRenderer;
        if (layers[i].offsetFromRendererSet && layers[i].offsetFromRenderer != newOffsetFromRenderer) {
            // It is ok to issue paint invalidation here, because all of the geometry needed to correctly invalidate paint is computed by this point.
            DisablePaintInvalidationStateAsserts disabler;
            layers[i].renderLayer->layoutObject()->invalidatePaintIncludingNonCompositingDescendants();

            TRACE_LAYER_INVALIDATION(layers[i].renderLayer, InspectorLayerInvalidationTrackingEvent::SquashingLayerGeometryWasUpdated);
            layersNeedingPaintInvalidation.append(layers[i].renderLayer);
        }
        layers[i].offsetFromRenderer = newOffsetFromRenderer;
        layers[i].offsetFromRendererSet = true;

        layers[i].renderLayer->setSubpixelAccumulation(subpixelAccumulation);
    }

    squashingLayer->setPosition(squashLayerBounds.location());
    squashingLayer->setSize(squashLayerBounds.size());

    *offsetFromTransformedAncestor = referenceOffsetFromTransformedAncestor;
    offsetFromTransformedAncestor->move(squashLayerOriginInOwningLayerSpace);

    for (size_t i = 0; i < layers.size(); ++i)
        layers[i].localClipRectForSquashedLayer = localClipRectForSquashedLayer(referenceLayer, layers[i], layers);
}

void CompositedLayerMapping::updateGraphicsLayerGeometry(const Layer* compositingContainer, const Layer* compositingStackingContext, Vector<Layer*>& layersNeedingPaintInvalidation)
{
    ASSERT(m_owningLayer.compositor()->lifecycle().state() == DocumentLifecycle::InCompositingUpdate);

    // Set transform property, if it is not animating. We have to do this here because the transform
    // is affected by the layer dimensions.
    if (!layoutObject()->style()->isRunningTransformAnimationOnCompositor())
        updateTransform(layoutObject()->styleRef());

    // Set opacity, if it is not animating.
    if (!layoutObject()->style()->isRunningOpacityAnimationOnCompositor())
        updateOpacity(layoutObject()->styleRef());

    if (!layoutObject()->style()->isRunningFilterAnimationOnCompositor())
        updateFilters(layoutObject()->styleRef());

    // We compute everything relative to the enclosing compositing layer.
    IntRect ancestorCompositingBounds;
    if (compositingContainer) {
        ASSERT(compositingContainer->hasCompositedLayerMapping());
        ancestorCompositingBounds = compositingContainer->compositedLayerMapping()->pixelSnappedCompositedBounds();
    }

    IntRect localCompositingBounds;
    IntRect relativeCompositingBounds;
    LayoutPoint offsetFromCompositedAncestor;
    IntPoint snappedOffsetFromCompositedAncestor;
    computeBoundsOfOwningLayer(compositingContainer, localCompositingBounds, relativeCompositingBounds, offsetFromCompositedAncestor, snappedOffsetFromCompositedAncestor);

    IntPoint graphicsLayerParentLocation;
    computeGraphicsLayerParentLocation(compositingContainer, ancestorCompositingBounds, graphicsLayerParentLocation);

    // Might update graphicsLayerParentLocation.
    updateAncestorClippingLayerGeometry(compositingContainer, snappedOffsetFromCompositedAncestor, graphicsLayerParentLocation);
    updateOverflowControlsHostLayerGeometry(compositingStackingContext, compositingContainer);

    FloatSize contentsSize = relativeCompositingBounds.size();

    updateMainGraphicsLayerGeometry(relativeCompositingBounds, localCompositingBounds, graphicsLayerParentLocation);
    updateContentsOffsetInCompositingLayer(snappedOffsetFromCompositedAncestor, graphicsLayerParentLocation);
    updateSquashingLayerGeometry(offsetFromCompositedAncestor, graphicsLayerParentLocation, m_owningLayer, m_squashedLayers, m_squashingLayer.get(), &m_squashingLayerOffsetFromTransformedAncestor, layersNeedingPaintInvalidation);

    // If we have a layer that clips children, position it.
    IntRect clippingBox;
    if (m_childContainmentLayer)
        clippingBox = clipBox(toLayoutBox(layoutObject()));

    updateChildContainmentLayerGeometry(clippingBox, localCompositingBounds);
    updateChildTransformLayerGeometry();

    updateMaskLayerGeometry();
    updateTransformGeometry(snappedOffsetFromCompositedAncestor, relativeCompositingBounds);
    updateForegroundLayerGeometry(contentsSize, clippingBox);
    updateBackgroundLayerGeometry(contentsSize);
    updateReflectionLayerGeometry(layersNeedingPaintInvalidation);
    updateScrollingLayerGeometry(localCompositingBounds);
    updateChildClippingMaskLayerGeometry();

    if (m_owningLayer.scrollableArea() && m_owningLayer.scrollableArea()->scrollsOverflow())
        m_owningLayer.scrollableArea()->positionOverflowControls();

    if (RuntimeEnabledFeatures::cssCompositingEnabled()) {
        updateLayerBlendMode(layoutObject()->styleRef());
        updateIsRootForIsolatedGroup();
    }

    updateContentsRect();
    updateBackgroundColor();
    updateDrawsContent();
    updateContentsOpaque();
    updateAfterPartResize();
    updateRenderingContext();
    updateShouldFlattenTransform();
    updateChildrenTransform();
    updateScrollParent(compositor()->preferCompositingToLCDTextEnabled() ? m_owningLayer.scrollParent() : 0);
    registerScrollingLayers();

    updateScrollBlocksOn(layoutObject()->styleRef());

    updateCompositingReasons();
}

void CompositedLayerMapping::updateMainGraphicsLayerGeometry(const IntRect& relativeCompositingBounds, const IntRect& localCompositingBounds, const IntPoint& graphicsLayerParentLocation)
{
    m_graphicsLayer->setPosition(FloatPoint(relativeCompositingBounds.location() - graphicsLayerParentLocation));
    m_graphicsLayer->setOffsetFromRenderer(toIntSize(localCompositingBounds.location()));

    FloatSize oldSize = m_graphicsLayer->size();
    const IntSize& contentsSize = relativeCompositingBounds.size();
    if (oldSize != contentsSize)
        m_graphicsLayer->setSize(contentsSize);

    // m_graphicsLayer is the corresponding GraphicsLayer for this Layer and its non-compositing
    // descendants. So, the visibility flag for m_graphicsLayer should be true if there are any
    // non-compositing visible layers.
    bool contentsVisible = m_owningLayer.hasVisibleContent() || hasVisibleNonCompositingDescendant(&m_owningLayer);
    if (RuntimeEnabledFeatures::overlayFullscreenVideoEnabled() && layoutObject()->isVideo()) {
        HTMLVideoElement* videoElement = toHTMLVideoElement(layoutObject()->node());
        if (videoElement->isFullscreen() && !HTMLMediaElement::isMediaStreamURL(videoElement->sourceURL().string()))
            contentsVisible = false;
    }
    m_graphicsLayer->setContentsVisible(contentsVisible);

    m_graphicsLayer->setBackfaceVisibility(layoutObject()->style()->backfaceVisibility() == BackfaceVisibilityVisible);
}

void CompositedLayerMapping::computeGraphicsLayerParentLocation(const Layer* compositingContainer, const IntRect& ancestorCompositingBounds, IntPoint& graphicsLayerParentLocation)
{
    if (compositingContainer && compositingContainer->compositedLayerMapping()->hasClippingLayer()) {
        // If the compositing ancestor has a layer to clip children, we parent in that, and therefore
        // position relative to it.
        IntRect clippingBox = clipBox(toLayoutBox(compositingContainer->layoutObject()));
        graphicsLayerParentLocation = clippingBox.location() + roundedIntSize(compositingContainer->subpixelAccumulation());
    } else if (compositingContainer && compositingContainer->compositedLayerMapping()->childTransformLayer()) {
        // Similarly, if the compositing ancestor has a child transform layer, we parent in that, and therefore
        // position relative to it. It's already taken into account the contents offset, so we do not need to here.
        graphicsLayerParentLocation = roundedIntPoint(compositingContainer->subpixelAccumulation());
    } else if (compositingContainer) {
        graphicsLayerParentLocation = ancestorCompositingBounds.location();
    } else {
        graphicsLayerParentLocation = layoutObject()->view()->documentRect().location();
    }

    if (compositingContainer && compositingContainer->needsCompositedScrolling()) {
        LayoutBox* layoutBox = toLayoutBox(compositingContainer->layoutObject());
        IntSize scrollOffset = layoutBox->scrolledContentOffset();
        IntPoint scrollOrigin(layoutBox->borderLeft(), layoutBox->borderTop());
        graphicsLayerParentLocation = scrollOrigin - scrollOffset;
    }
}

void CompositedLayerMapping::updateAncestorClippingLayerGeometry(const Layer* compositingContainer, const IntPoint& snappedOffsetFromCompositedAncestor, IntPoint& graphicsLayerParentLocation)
{
    if (!compositingContainer || !m_ancestorClippingLayer)
        return;

    ClipRectsContext clipRectsContext(compositingContainer, PaintingClipRectsIgnoringOverflowClip, IgnoreOverlayScrollbarSize);
    IntRect parentClipRect = pixelSnappedIntRect(m_owningLayer.clipper().backgroundClipRect(clipRectsContext).rect());
    ASSERT(parentClipRect != LayoutRect::infiniteIntRect());
    m_ancestorClippingLayer->setPosition(FloatPoint(parentClipRect.location() - graphicsLayerParentLocation));
    m_ancestorClippingLayer->setSize(parentClipRect.size());

    // backgroundRect is relative to compositingContainer, so subtract snappedOffsetFromCompositedAncestor.X/snappedOffsetFromCompositedAncestor.Y to get back to local coords.
    m_ancestorClippingLayer->setOffsetFromRenderer(parentClipRect.location() - snappedOffsetFromCompositedAncestor);

    // The primary layer is then parented in, and positioned relative to this clipping layer.
    graphicsLayerParentLocation = parentClipRect.location();
}

void CompositedLayerMapping::updateOverflowControlsHostLayerGeometry(const Layer* compositingStackingContext, const Layer* compositingContainer)
{
    if (!m_overflowControlsHostLayer)
        return;

    if (needsToReparentOverflowControls()) {
        if (m_overflowControlsClippingLayer) {
            m_overflowControlsClippingLayer->setSize(m_ancestorClippingLayer->size());
            m_overflowControlsClippingLayer->setOffsetFromRenderer(m_ancestorClippingLayer->offsetFromRenderer());
            m_overflowControlsClippingLayer->setMasksToBounds(true);
            m_overflowControlsHostLayer->setPosition(IntPoint(-m_overflowControlsClippingLayer->offsetFromRenderer()));

            FloatPoint position = m_ancestorClippingLayer->position();
            if (compositingStackingContext != compositingContainer) {
                LayoutPoint offset;
                compositingContainer->convertToLayerCoords(compositingStackingContext, offset);
                FloatSize offsetFromStackingContainer = toFloatSize(FloatPoint(offset));
                position += offsetFromStackingContainer;
            }

            m_overflowControlsClippingLayer->setPosition(position);
        } else {
            // The controls are in the same 2D space as the compositing container, so we can map them into the space of the container.
            TransformState transformState(TransformState::ApplyTransformDirection, FloatPoint());
            m_owningLayer.layoutObject()->mapLocalToContainer(compositingStackingContext->layoutObject(), transformState, ApplyContainerFlip);
            transformState.flatten();
            LayoutPoint offsetFromStackingContainer = LayoutPoint(transformState.lastPlanarPoint());
            if (LayerScrollableArea* scrollableArea = compositingStackingContext->scrollableArea())
                offsetFromStackingContainer.move(LayoutSize(scrollableArea->adjustedScrollOffset()));
            m_overflowControlsHostLayer->setPosition(FloatPoint(offsetFromStackingContainer));
        }
    } else {
        m_overflowControlsHostLayer->setPosition(FloatPoint());
    }
}

void CompositedLayerMapping::updateChildContainmentLayerGeometry(const IntRect& clippingBox, const IntRect& localCompositingBounds)
{
    if (!m_childContainmentLayer)
        return;

    m_childContainmentLayer->setPosition(FloatPoint(clippingBox.location() - localCompositingBounds.location() + roundedIntSize(m_owningLayer.subpixelAccumulation())));
    m_childContainmentLayer->setSize(clippingBox.size());
    m_childContainmentLayer->setOffsetFromRenderer(toIntSize(clippingBox.location()));
    if (m_childClippingMaskLayer && !m_scrollingLayer && !layoutObject()->style()->clipPath()) {
        m_childClippingMaskLayer->setPosition(m_childContainmentLayer->position());
        m_childClippingMaskLayer->setSize(m_childContainmentLayer->size());
        m_childClippingMaskLayer->setOffsetFromRenderer(m_childContainmentLayer->offsetFromRenderer());
    }
}

void CompositedLayerMapping::updateChildTransformLayerGeometry()
{
    if (!m_childTransformLayer)
        return;
    const IntRect borderBox = toLayoutBox(m_owningLayer.layoutObject())->pixelSnappedBorderBoxRect();
    m_childTransformLayer->setSize(borderBox.size());
    m_childTransformLayer->setPosition(FloatPoint(contentOffsetInCompositingLayer()));
}

void CompositedLayerMapping::updateMaskLayerGeometry()
{
    if (!m_maskLayer)
        return;

    if (m_maskLayer->size() != m_graphicsLayer->size()) {
        m_maskLayer->setSize(m_graphicsLayer->size());
        m_maskLayer->setNeedsDisplay();
    }
    m_maskLayer->setPosition(FloatPoint());
    m_maskLayer->setOffsetFromRenderer(m_graphicsLayer->offsetFromRenderer());
}

void CompositedLayerMapping::updateTransformGeometry(const IntPoint& snappedOffsetFromCompositedAncestor, const IntRect& relativeCompositingBounds)
{
    if (m_owningLayer.hasTransformRelatedProperty()) {
        const LayoutRect borderBox = toLayoutBox(layoutObject())->borderBoxRect();

        // Get layout bounds in the coords of compositingContainer to match relativeCompositingBounds.
        IntRect layerBounds = pixelSnappedIntRect(toLayoutPoint(m_owningLayer.subpixelAccumulation()), borderBox.size());
        layerBounds.moveBy(snappedOffsetFromCompositedAncestor);

        // Update properties that depend on layer dimensions
        FloatPoint3D transformOrigin = computeTransformOrigin(IntRect(IntPoint(), layerBounds.size()));

        // |transformOrigin| is in the local space of this layer. layerBounds - relativeCompositingBounds converts to the space of the
        // compositing bounds relative to the composited ancestor. This does not apply to the z direction, since the page is 2D.
        FloatPoint3D compositedTransformOrigin(
            layerBounds.x() - relativeCompositingBounds.x() + transformOrigin.x(),
            layerBounds.y() - relativeCompositingBounds.y() + transformOrigin.y(),
            transformOrigin.z());
        m_graphicsLayer->setTransformOrigin(compositedTransformOrigin);
    } else {
        FloatPoint3D compositedTransformOrigin(
            relativeCompositingBounds.width() * 0.5f,
            relativeCompositingBounds.height() * 0.5f,
            0.f);
        m_graphicsLayer->setTransformOrigin(compositedTransformOrigin);
    }
}

void CompositedLayerMapping::updateReflectionLayerGeometry(Vector<Layer*>& layersNeedingPaintInvalidation)
{
    if (!m_owningLayer.reflectionInfo() || !m_owningLayer.reflectionInfo()->reflectionLayer()->hasCompositedLayerMapping())
        return;

    CompositedLayerMapping* reflectionCompositedLayerMapping = m_owningLayer.reflectionInfo()->reflectionLayer()->compositedLayerMapping();
    reflectionCompositedLayerMapping->updateGraphicsLayerGeometry(&m_owningLayer, &m_owningLayer, layersNeedingPaintInvalidation);
}

void CompositedLayerMapping::updateScrollingLayerGeometry(const IntRect& localCompositingBounds)
{
    if (!m_scrollingLayer)
        return;

    ASSERT(m_scrollingContentsLayer);
    LayoutBox* layoutBox = toLayoutBox(layoutObject());
    IntRect clientBox = enclosingIntRect(layoutBox->clientBoxRect());
    DoubleSize adjustedScrollOffset = m_owningLayer.scrollableArea()->adjustedScrollOffset();
    m_scrollingLayer->setPosition(FloatPoint(clientBox.location() - localCompositingBounds.location() + roundedIntSize(m_owningLayer.subpixelAccumulation())));
    m_scrollingLayer->setSize(clientBox.size());

    IntSize oldScrollingLayerOffset = m_scrollingLayer->offsetFromRenderer();
    m_scrollingLayer->setOffsetFromRenderer(-toIntSize(clientBox.location()));

    if (m_childClippingMaskLayer && !layoutObject()->style()->clipPath()) {
        m_childClippingMaskLayer->setPosition(m_scrollingLayer->position());
        m_childClippingMaskLayer->setSize(m_scrollingLayer->size());
        m_childClippingMaskLayer->setOffsetFromRenderer(toIntSize(clientBox.location()));
    }

    bool clientBoxOffsetChanged = oldScrollingLayerOffset != m_scrollingLayer->offsetFromRenderer();

    IntSize scrollSize(layoutBox->scrollWidth(), layoutBox->scrollHeight());
    if (scrollSize != m_scrollingContentsLayer->size() || clientBoxOffsetChanged)
        m_scrollingContentsLayer->setNeedsDisplay();

    DoubleSize scrollingContentsOffset(clientBox.location().x() - adjustedScrollOffset.width(), clientBox.location().y() - adjustedScrollOffset.height());
    // The scroll offset change is compared using floating point so that fractional scroll offset
    // change can be propagated to compositor.
    if (scrollingContentsOffset != m_scrollingContentsLayer->offsetDoubleFromRenderer() || scrollSize != m_scrollingContentsLayer->size()) {
        bool coordinatorHandlesOffset = compositor()->scrollingLayerDidChange(&m_owningLayer);
        m_scrollingContentsLayer->setPosition(coordinatorHandlesOffset ? FloatPoint() : FloatPoint(-toFloatSize(adjustedScrollOffset)));
    }

    m_scrollingContentsLayer->setSize(scrollSize);
    // FIXME: The paint offset and the scroll offset should really be separate concepts.
    m_scrollingContentsLayer->setOffsetDoubleFromRenderer(scrollingContentsOffset, GraphicsLayer::DontSetNeedsDisplay);

    if (m_foregroundLayer) {
        if (m_foregroundLayer->size() != m_scrollingContentsLayer->size())
            m_foregroundLayer->setSize(m_scrollingContentsLayer->size());
        m_foregroundLayer->setNeedsDisplay();
        m_foregroundLayer->setOffsetFromRenderer(m_scrollingContentsLayer->offsetFromRenderer());
    }

    updateScrollingBlockSelection();
}

void CompositedLayerMapping::updateChildClippingMaskLayerGeometry()
{
    if (!m_childClippingMaskLayer || !layoutObject()->style()->clipPath())
        return;
    LayoutBox* layoutBox = toLayoutBox(layoutObject());
    IntRect clientBox = enclosingIntRect(layoutBox->clientBoxRect());

    m_childClippingMaskLayer->setPosition(m_graphicsLayer->position());
    m_childClippingMaskLayer->setSize(m_graphicsLayer->size());
    m_childClippingMaskLayer->setOffsetFromRenderer(toIntSize(clientBox.location()));

    // NOTE: also some stuff happening in updateChildContainmentLayerGeometry().
}

void CompositedLayerMapping::updateForegroundLayerGeometry(const FloatSize& relativeCompositingBoundsSize, const IntRect& clippingBox)
{
    if (!m_foregroundLayer)
        return;

    FloatSize foregroundSize = relativeCompositingBoundsSize;
    IntSize foregroundOffset = m_graphicsLayer->offsetFromRenderer();
    m_foregroundLayer->setPosition(FloatPoint());

    if (hasClippingLayer()) {
        // If we have a clipping layer (which clips descendants), then the foreground layer is a child of it,
        // so that it gets correctly sorted with children. In that case, position relative to the clipping layer.
        foregroundSize = FloatSize(clippingBox.size());
        foregroundOffset = toIntSize(clippingBox.location());
    } else if (m_childTransformLayer) {
        // Things are different if we have a child transform layer rather
        // than a clipping layer. In this case, we want to actually change
        // the position of the layer (to compensate for our ancestor
        // compositing layer's position) rather than leave the position the
        // same and use offset-from-renderer + size to describe a clipped
        // "window" onto the clipped layer.

        m_foregroundLayer->setPosition(-m_childTransformLayer->position());
    }

    if (foregroundSize != m_foregroundLayer->size()) {
        m_foregroundLayer->setSize(foregroundSize);
        m_foregroundLayer->setNeedsDisplay();
    }
    m_foregroundLayer->setOffsetFromRenderer(foregroundOffset);

    // NOTE: there is some more configuring going on in updateScrollingLayerGeometry().
}

void CompositedLayerMapping::updateBackgroundLayerGeometry(const FloatSize& relativeCompositingBoundsSize)
{
    if (!m_backgroundLayer)
        return;

    FloatSize backgroundSize = relativeCompositingBoundsSize;
    if (backgroundLayerPaintsFixedRootBackground()) {
        FrameView* frameView = toLayoutView(layoutObject())->frameView();
        backgroundSize = frameView->visibleContentRect().size();
    }
    m_backgroundLayer->setPosition(FloatPoint());
    if (backgroundSize != m_backgroundLayer->size()) {
        m_backgroundLayer->setSize(backgroundSize);
        m_backgroundLayer->setNeedsDisplay();
    }
    m_backgroundLayer->setOffsetFromRenderer(m_graphicsLayer->offsetFromRenderer());
}

void CompositedLayerMapping::registerScrollingLayers()
{
    // Register fixed position layers and their containers with the scrolling coordinator.
    ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer);
    if (!scrollingCoordinator)
        return;

    scrollingCoordinator->updateLayerPositionConstraint(&m_owningLayer);

    // Page scale is applied as a transform on the root render view layer. Because the scroll
    // layer is further up in the hierarchy, we need to avoid marking the root render view
    // layer as a container.
    bool isContainer = m_owningLayer.hasTransformRelatedProperty() && !m_owningLayer.isRootLayer();
    // FIXME: we should make certain that childForSuperLayers will never be the m_squashingContainmentLayer here
    scrollingCoordinator->setLayerIsContainerForFixedPositionLayers(childForSuperlayers(), isContainer);
}

void CompositedLayerMapping::updateInternalHierarchy()
{
    // m_foregroundLayer has to be inserted in the correct order with child layers,
    // so it's not inserted here.
    if (m_ancestorClippingLayer)
        m_ancestorClippingLayer->removeAllChildren();

    m_graphicsLayer->removeFromParent();

    if (m_ancestorClippingLayer)
        m_ancestorClippingLayer->addChild(m_graphicsLayer.get());

    if (m_childContainmentLayer)
        m_graphicsLayer->addChild(m_childContainmentLayer.get());
    else if (m_childTransformLayer)
        m_graphicsLayer->addChild(m_childTransformLayer.get());

    if (m_scrollingLayer) {
        GraphicsLayer* superLayer = m_graphicsLayer.get();

        if (m_childContainmentLayer)
            superLayer = m_childContainmentLayer.get();

        if (m_childTransformLayer)
            superLayer = m_childTransformLayer.get();

        superLayer->addChild(m_scrollingLayer.get());
    }

    // The clip for child layers does not include space for overflow controls, so they exist as
    // siblings of the clipping layer if we have one. Normal children of this layer are set as
    // children of the clipping layer.
    if (m_overflowControlsClippingLayer) {
        ASSERT(m_overflowControlsHostLayer);
        m_graphicsLayer->addChild(m_overflowControlsClippingLayer.get());
        m_overflowControlsClippingLayer->addChild(m_overflowControlsHostLayer.get());
    } else if (m_overflowControlsHostLayer) {
        m_graphicsLayer->addChild(m_overflowControlsHostLayer.get());
    }

    if (m_layerForHorizontalScrollbar)
        m_overflowControlsHostLayer->addChild(m_layerForHorizontalScrollbar.get());
    if (m_layerForVerticalScrollbar)
        m_overflowControlsHostLayer->addChild(m_layerForVerticalScrollbar.get());
    if (m_layerForScrollCorner)
        m_overflowControlsHostLayer->addChild(m_layerForScrollCorner.get());

    // The squashing containment layer, if it exists, becomes a no-op parent.
    if (m_squashingLayer) {
        ASSERT((m_ancestorClippingLayer && !m_squashingContainmentLayer) || (!m_ancestorClippingLayer && m_squashingContainmentLayer));

        if (m_squashingContainmentLayer) {
            m_squashingContainmentLayer->removeAllChildren();
            m_squashingContainmentLayer->addChild(m_graphicsLayer.get());
            m_squashingContainmentLayer->addChild(m_squashingLayer.get());
        } else {
            // The ancestor clipping layer is already set up and has m_graphicsLayer under it.
            m_ancestorClippingLayer->addChild(m_squashingLayer.get());
        }
    }
}

void CompositedLayerMapping::updatePaintingPhases()
{
    m_graphicsLayer->setPaintingPhase(paintingPhaseForPrimaryLayer());
    if (m_scrollingContentsLayer) {
        GraphicsLayerPaintingPhase paintPhase = GraphicsLayerPaintOverflowContents | GraphicsLayerPaintCompositedScroll;
        if (!m_foregroundLayer)
            paintPhase |= GraphicsLayerPaintForeground;
        m_scrollingContentsLayer->setPaintingPhase(paintPhase);
        m_scrollingBlockSelectionLayer->setPaintingPhase(paintPhase);
    }
}

void CompositedLayerMapping::updateContentsRect()
{
    m_graphicsLayer->setContentsRect(pixelSnappedIntRect(contentsBox()));
}

void CompositedLayerMapping::updateContentsOffsetInCompositingLayer(const IntPoint& snappedOffsetFromCompositedAncestor, const IntPoint& graphicsLayerParentLocation)
{
    // m_graphicsLayer is positioned relative to our compositing ancestor
    // Layer, but it's not positioned at the origin of m_owningLayer, it's
    // offset by m_contentBounds.location(). This is what
    // contentOffsetInCompositingLayer is meant to capture, roughly speaking
    // (ignoring rounding and subpixel accumulation).
    //
    // Our ancestor graphics layers in this CLM (m_graphicsLayer and potentially
    // m_ancestorClippingLayer) have pixel snapped, so if we don't adjust this
    // offset, we'll see accumulated rounding errors due to that snapping.
    //
    // In order to ensure that we account for this rounding, we compute
    // contentsOffsetInCompositingLayer in a somewhat roundabout way.
    //
    // our position = (desired position) - (inherited graphics layer offset).
    //
    // Precisely,
    // Offset = snappedOffsetFromCompositedAncestor - offsetDueToAncestorGraphicsLayers (See code below)
    //      = snappedOffsetFromCompositedAncestor - (m_graphicsLayer->position() + graphicsLayerParentLocation)
    //      = snappedOffsetFromCompositedAncestor - (relativeCompositingBounds.location() - graphicsLayerParentLocation + graphicsLayerParentLocation) (See updateMainGraphicsLayerGeometry)
    //      = snappedOffsetFromCompositedAncestor - relativeCompositingBounds.location()
    //      = snappedOffsetFromCompositedAncestor - (pixelSnappedIntRect(contentBounds.location()) + snappedOffsetFromCompositedAncestor) (See computeBoundsOfOwningLayer)
    //      = -pixelSnappedIntRect(contentBounds.location())
    //
    // As you can see, we've ended up at the same spot (-contentBounds.location()),
    // but by subtracting off our ancestor graphics layers positions, we can be
    // sure we've accounted correctly for any pixel snapping due to ancestor
    // graphics layers.
    //
    // And drawing of composited children takes into account the subpixel
    // accumulation of this CLM already (through its own
    // graphicsLayerParentLocation it appears).
    FloatPoint offsetDueToAncestorGraphicsLayers = m_graphicsLayer->position() + graphicsLayerParentLocation;
    m_contentOffsetInCompositingLayer = LayoutSize(snappedOffsetFromCompositedAncestor - offsetDueToAncestorGraphicsLayers);
    m_contentOffsetInCompositingLayerDirty = false;
}

void CompositedLayerMapping::updateScrollingBlockSelection()
{
    if (!m_scrollingBlockSelectionLayer)
        return;

    if (!m_scrollingContentsAreEmpty) {
        // In this case, the selection will be painted directly into m_scrollingContentsLayer.
        m_scrollingBlockSelectionLayer->setDrawsContent(false);
        return;
    }

    const IntRect blockSelectionGapsBounds = m_owningLayer.blockSelectionGapsBounds();
    const bool shouldDrawContent = !blockSelectionGapsBounds.isEmpty();
    m_scrollingBlockSelectionLayer->setDrawsContent(shouldDrawContent);
    if (!shouldDrawContent)
        return;
    // FIXME: Remove the flooredIntSize conversion. crbug.com/414283.
    const IntPoint position = blockSelectionGapsBounds.location() + flooredIntSize(m_owningLayer.scrollableArea()->adjustedScrollOffset());
    if (m_scrollingBlockSelectionLayer->size() == blockSelectionGapsBounds.size() && m_scrollingBlockSelectionLayer->position() == position)
        return;

    m_scrollingBlockSelectionLayer->setPosition(position);
    m_scrollingBlockSelectionLayer->setSize(blockSelectionGapsBounds.size());
    m_scrollingBlockSelectionLayer->setOffsetFromRenderer(toIntSize(blockSelectionGapsBounds.location()), GraphicsLayer::SetNeedsDisplay);
}

void CompositedLayerMapping::updateDrawsContent()
{
    bool hasPaintedContent = containsPaintedContent();
    m_graphicsLayer->setDrawsContent(hasPaintedContent);

    if (m_scrollingLayer) {
        // m_scrollingLayer never has backing store.
        // m_scrollingContentsLayer only needs backing store if the scrolled contents need to paint.
        m_scrollingContentsAreEmpty = !m_owningLayer.hasVisibleContent() || !(layoutObject()->hasBackground() || paintsChildren());
        m_scrollingContentsLayer->setDrawsContent(!m_scrollingContentsAreEmpty);
        updateScrollingBlockSelection();
    }

    if (hasPaintedContent && isAcceleratedCanvas(layoutObject())) {
        CanvasRenderingContext* context = toHTMLCanvasElement(layoutObject()->node())->renderingContext();
        // Content layer may be null if context is lost.
        if (WebLayer* contentLayer = context->platformLayer()) {
            Color bgColor(Color::transparent);
            if (contentLayerSupportsDirectBackgroundComposition(layoutObject())) {
                bgColor = rendererBackgroundColor();
                hasPaintedContent = false;
            }
            contentLayer->setBackgroundColor(bgColor.rgb());
        }
    }

    // FIXME: we could refine this to only allocate backings for one of these layers if possible.
    if (m_foregroundLayer)
        m_foregroundLayer->setDrawsContent(hasPaintedContent);

    if (m_backgroundLayer)
        m_backgroundLayer->setDrawsContent(hasPaintedContent);

    if (m_maskLayer)
        m_maskLayer->setDrawsContent(true);

    if (m_childClippingMaskLayer)
        m_childClippingMaskLayer->setDrawsContent(true);
}

void CompositedLayerMapping::updateChildrenTransform()
{
    if (GraphicsLayer* childTransformLayer = layerForChildrenTransform()) {
        childTransformLayer->setTransform(owningLayer().perspectiveTransform());
        childTransformLayer->setTransformOrigin(FloatPoint3D(childTransformLayer->size().width() * 0.5f, childTransformLayer->size().height() * 0.5f, 0.f));
    }

    updateShouldFlattenTransform();
}

// Return true if the layers changed.
bool CompositedLayerMapping::updateClippingLayers(bool needsAncestorClip, bool needsDescendantClip)
{
    bool layersChanged = false;

    if (needsAncestorClip) {
        if (!m_ancestorClippingLayer) {
            m_ancestorClippingLayer = createGraphicsLayer(CompositingReasonLayerForAncestorClip);
            m_ancestorClippingLayer->setMasksToBounds(true);
            m_ancestorClippingLayer->setShouldFlattenTransform(false);
            layersChanged = true;
        }
    } else if (m_ancestorClippingLayer) {
        m_ancestorClippingLayer->removeFromParent();
        m_ancestorClippingLayer = nullptr;
        layersChanged = true;
    }

    if (needsDescendantClip) {
        // We don't need a child containment layer if we're the main frame render view
        // layer. It's redundant as the frame clip above us will handle this clipping.
        if (!m_childContainmentLayer && !m_isMainFrameLayoutViewLayer) {
            m_childContainmentLayer = createGraphicsLayer(CompositingReasonLayerForDescendantClip);
            m_childContainmentLayer->setMasksToBounds(true);
            layersChanged = true;
        }
    } else if (hasClippingLayer()) {
        m_childContainmentLayer->removeFromParent();
        m_childContainmentLayer = nullptr;
        layersChanged = true;
    }

    return layersChanged;
}

bool CompositedLayerMapping::updateChildTransformLayer(bool needsChildTransformLayer)
{
    bool layersChanged = false;

    if (needsChildTransformLayer) {
        if (!m_childTransformLayer) {
            m_childTransformLayer = createGraphicsLayer(CompositingReasonLayerForPerspective);
            m_childTransformLayer->setDrawsContent(false);
            layersChanged = true;
        }
    } else if (m_childTransformLayer) {
        m_childTransformLayer->removeFromParent();
        m_childTransformLayer = nullptr;
        layersChanged = true;
    }

    return layersChanged;
}

void CompositedLayerMapping::setBackgroundLayerPaintsFixedRootBackground(bool backgroundLayerPaintsFixedRootBackground)
{
    m_backgroundLayerPaintsFixedRootBackground = backgroundLayerPaintsFixedRootBackground;
}

// Only a member function so it can call createGraphicsLayer.
bool CompositedLayerMapping::toggleScrollbarLayerIfNeeded(OwnPtr<GraphicsLayer>& layer, bool needsLayer, CompositingReasons reason)
{
    if (needsLayer == !!layer)
        return false;
    layer = needsLayer ? createGraphicsLayer(reason) : nullptr;
    return true;
}

bool CompositedLayerMapping::updateOverflowControlsLayers(bool needsHorizontalScrollbarLayer, bool needsVerticalScrollbarLayer, bool needsScrollCornerLayer, bool needsAncestorClip)
{
    bool horizontalScrollbarLayerChanged = toggleScrollbarLayerIfNeeded(m_layerForHorizontalScrollbar, needsHorizontalScrollbarLayer, CompositingReasonLayerForHorizontalScrollbar);
    bool verticalScrollbarLayerChanged = toggleScrollbarLayerIfNeeded(m_layerForVerticalScrollbar, needsVerticalScrollbarLayer, CompositingReasonLayerForVerticalScrollbar);
    bool scrollCornerLayerChanged = toggleScrollbarLayerIfNeeded(m_layerForScrollCorner, needsScrollCornerLayer, CompositingReasonLayerForScrollCorner);

    bool needsOverflowControlsHostLayer = needsHorizontalScrollbarLayer || needsVerticalScrollbarLayer || needsScrollCornerLayer;
    toggleScrollbarLayerIfNeeded(m_overflowControlsHostLayer, needsOverflowControlsHostLayer, CompositingReasonLayerForOverflowControlsHost);
    bool needsOverflowClipLayer = needsOverflowControlsHostLayer && needsAncestorClip;
    toggleScrollbarLayerIfNeeded(m_overflowControlsClippingLayer, needsOverflowClipLayer, CompositingReasonLayerForOverflowControlsHost);

    if (ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer)) {
        if (horizontalScrollbarLayerChanged)
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_owningLayer.scrollableArea(), HorizontalScrollbar);
        if (verticalScrollbarLayerChanged)
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_owningLayer.scrollableArea(), VerticalScrollbar);
    }

    return horizontalScrollbarLayerChanged || verticalScrollbarLayerChanged || scrollCornerLayerChanged;
}

void CompositedLayerMapping::positionOverflowControlsLayers()
{
    IntSize offsetFromRenderer = m_graphicsLayer->offsetFromRenderer() - roundedIntSize(m_owningLayer.subpixelAccumulation());
    if (GraphicsLayer* layer = layerForHorizontalScrollbar()) {
        Scrollbar* hBar = m_owningLayer.scrollableArea()->horizontalScrollbar();
        if (hBar) {
            layer->setPosition(hBar->frameRect().location() - offsetFromRenderer);
            layer->setSize(hBar->frameRect().size());
            if (layer->hasContentsLayer())
                layer->setContentsRect(IntRect(IntPoint(), hBar->frameRect().size()));
        }
        layer->setDrawsContent(hBar && !layer->hasContentsLayer());
    }

    if (GraphicsLayer* layer = layerForVerticalScrollbar()) {
        Scrollbar* vBar = m_owningLayer.scrollableArea()->verticalScrollbar();
        if (vBar) {
            layer->setPosition(vBar->frameRect().location() - offsetFromRenderer);
            layer->setSize(vBar->frameRect().size());
            if (layer->hasContentsLayer())
                layer->setContentsRect(IntRect(IntPoint(), vBar->frameRect().size()));
        }
        layer->setDrawsContent(vBar && !layer->hasContentsLayer());
    }

    if (GraphicsLayer* layer = layerForScrollCorner()) {
        const IntRect& scrollCornerAndResizer = m_owningLayer.scrollableArea()->scrollCornerAndResizerRect();
        layer->setPosition(FloatPoint(scrollCornerAndResizer.location() - offsetFromRenderer));
        layer->setSize(FloatSize(scrollCornerAndResizer.size()));
        layer->setDrawsContent(!scrollCornerAndResizer.isEmpty());
    }
}

bool CompositedLayerMapping::hasUnpositionedOverflowControlsLayers() const
{
    if (GraphicsLayer* layer = layerForHorizontalScrollbar()) {
        if (!layer->drawsContent())
            return true;
    }

    if (GraphicsLayer* layer = layerForVerticalScrollbar()) {
        if (!layer->drawsContent())
            return true;
    }

    if (GraphicsLayer* layer = layerForScrollCorner()) {
        if (!layer->drawsContent())
            return true;
    }

    return false;
}

enum ApplyToGraphicsLayersModeFlags {
    ApplyToLayersAffectedByPreserve3D = (1 << 0),
    ApplyToSquashingLayer = (1 << 1),
    ApplyToScrollbarLayers = (1 << 2),
    ApplyToBackgroundLayer = (1 << 3),
    ApplyToMaskLayers = (1 << 4),
    ApplyToContentLayers = (1 << 5),
    ApplyToAllGraphicsLayers = (ApplyToSquashingLayer | ApplyToScrollbarLayers | ApplyToBackgroundLayer | ApplyToMaskLayers | ApplyToLayersAffectedByPreserve3D | ApplyToContentLayers)
};
typedef unsigned ApplyToGraphicsLayersMode;

template <typename Func>
static void ApplyToGraphicsLayers(const CompositedLayerMapping* mapping, const Func& f, ApplyToGraphicsLayersMode mode)
{
    ASSERT(mode);

    if ((mode & ApplyToLayersAffectedByPreserve3D) && mapping->childTransformLayer())
        f(mapping->childTransformLayer());
    if (((mode & ApplyToLayersAffectedByPreserve3D) || (mode & ApplyToContentLayers)) && mapping->mainGraphicsLayer())
        f(mapping->mainGraphicsLayer());
    if ((mode & ApplyToLayersAffectedByPreserve3D) && mapping->clippingLayer())
        f(mapping->clippingLayer());
    if ((mode & ApplyToLayersAffectedByPreserve3D) && mapping->scrollingLayer())
        f(mapping->scrollingLayer());
    if ((mode & ApplyToLayersAffectedByPreserve3D) && mapping->scrollingBlockSelectionLayer())
        f(mapping->scrollingBlockSelectionLayer());
    if (((mode & ApplyToLayersAffectedByPreserve3D) || (mode & ApplyToContentLayers)) && mapping->scrollingContentsLayer())
        f(mapping->scrollingContentsLayer());
    if (((mode & ApplyToLayersAffectedByPreserve3D) || (mode & ApplyToContentLayers)) && mapping->foregroundLayer())
        f(mapping->foregroundLayer());

    if ((mode & ApplyToSquashingLayer) && mapping->squashingLayer())
        f(mapping->squashingLayer());

    if (((mode & ApplyToMaskLayers) || (mode & ApplyToContentLayers)) && mapping->maskLayer())
        f(mapping->maskLayer());
    if (((mode & ApplyToMaskLayers) || (mode & ApplyToContentLayers)) && mapping->childClippingMaskLayer())
        f(mapping->childClippingMaskLayer());

    if (((mode & ApplyToBackgroundLayer) || (mode & ApplyToContentLayers)) && mapping->backgroundLayer())
        f(mapping->backgroundLayer());

    if ((mode & ApplyToScrollbarLayers) && mapping->layerForHorizontalScrollbar())
        f(mapping->layerForHorizontalScrollbar());
    if ((mode & ApplyToScrollbarLayers) && mapping->layerForVerticalScrollbar())
        f(mapping->layerForVerticalScrollbar());
    if ((mode & ApplyToScrollbarLayers) && mapping->layerForScrollCorner())
        f(mapping->layerForScrollCorner());
}

struct UpdateRenderingContextFunctor {
    void operator() (GraphicsLayer* layer) const { layer->setRenderingContext(renderingContext); }
    int renderingContext;
};

void CompositedLayerMapping::updateRenderingContext()
{
    // All layers but the squashing layer (which contains 'alien' content) should be included in this
    // rendering context.
    int id = 0;

    // NB, it is illegal at this point to query an ancestor's compositing state. Some compositing
    // reasons depend on the compositing state of ancestors. So if we want a rendering context id
    // for the context root, we cannot ask for the id of its associated WebLayer now; it may not have
    // one yet. We could do a second past after doing the compositing updates to get these ids,
    // but this would actually be harmful. We do not want to attach any semantic meaning to
    // the context id other than the fact that they group a number of layers together for the
    // sake of 3d sorting. So instead we will ask the compositor to vend us an arbitrary, but
    // consistent id.
    if (Layer* root = m_owningLayer.renderingContextRoot()) {
        if (Node* node = root->layoutObject()->node())
            id = static_cast<int>(WTF::PtrHash<Node*>::hash(node));
    }

    UpdateRenderingContextFunctor functor = { id };
    ApplyToGraphicsLayersMode mode = ApplyToAllGraphicsLayers & ~ApplyToSquashingLayer;
    ApplyToGraphicsLayers<UpdateRenderingContextFunctor>(this, functor, mode);
}

struct UpdateShouldFlattenTransformFunctor {
    void operator() (GraphicsLayer* layer) const { layer->setShouldFlattenTransform(shouldFlatten); }
    bool shouldFlatten;
};

void CompositedLayerMapping::updateShouldFlattenTransform()
{
    // All CLM-managed layers that could affect a descendant layer should update their
    // should-flatten-transform value (the other layers' transforms don't matter here).
    UpdateShouldFlattenTransformFunctor functor = { !m_owningLayer.shouldPreserve3D() };
    ApplyToGraphicsLayersMode mode = ApplyToLayersAffectedByPreserve3D;
    ApplyToGraphicsLayers(this, functor, mode);

    // Note, if we apply perspective, we have to set should flatten differently
    // so that the transform propagates to child layers correctly.
    if (GraphicsLayer* childTransformLayer = layerForChildrenTransform()) {
        bool hasPerspective = false;
        // FIXME: Can |style| be really null here?
        if (const LayoutStyle* style = m_owningLayer.layoutObject()->style())
            hasPerspective = style->hasPerspective();
        if (hasPerspective)
            childTransformLayer->setShouldFlattenTransform(false);

        // Note, if the target is the scrolling layer, we need to ensure that the
        // scrolling content layer doesn't flatten the transform. (It would be nice
        // if we could apply transform to the scrolling content layer, but that's
        // too late, we need the children transform to be applied _before_ the
        // scrolling offset.)
        if (childTransformLayer == m_scrollingLayer.get()) {
            m_scrollingContentsLayer->setShouldFlattenTransform(false);
            m_scrollingBlockSelectionLayer->setShouldFlattenTransform(false);
        }
    }
}

bool CompositedLayerMapping::updateForegroundLayer(bool needsForegroundLayer)
{
    bool layerChanged = false;
    if (needsForegroundLayer) {
        if (!m_foregroundLayer) {
            m_foregroundLayer = createGraphicsLayer(CompositingReasonLayerForForeground);
            m_foregroundLayer->setPaintingPhase(GraphicsLayerPaintForeground);
            layerChanged = true;
        }
    } else if (m_foregroundLayer) {
        m_foregroundLayer->removeFromParent();
        m_foregroundLayer = nullptr;
        layerChanged = true;
    }

    return layerChanged;
}

bool CompositedLayerMapping::updateBackgroundLayer(bool needsBackgroundLayer)
{
    bool layerChanged = false;
    if (needsBackgroundLayer) {
        if (!m_backgroundLayer) {
            m_backgroundLayer = createGraphicsLayer(CompositingReasonLayerForBackground);
            m_backgroundLayer->setTransformOrigin(FloatPoint3D());
            m_backgroundLayer->setPaintingPhase(GraphicsLayerPaintBackground);
#if !OS(ANDROID)
            m_backgroundLayer->contentLayer()->setDrawCheckerboardForMissingTiles(true);
            m_graphicsLayer->contentLayer()->setDrawCheckerboardForMissingTiles(false);
#endif
            layerChanged = true;
        }
    } else {
        if (m_backgroundLayer) {
            m_backgroundLayer->removeFromParent();
            m_backgroundLayer = nullptr;
#if !OS(ANDROID)
            m_graphicsLayer->contentLayer()->setDrawCheckerboardForMissingTiles(true);
#endif
            layerChanged = true;
        }
    }

    if (layerChanged && !m_owningLayer.layoutObject()->documentBeingDestroyed())
        compositor()->rootFixedBackgroundsChanged();

    return layerChanged;
}

bool CompositedLayerMapping::updateMaskLayer(bool needsMaskLayer)
{
    bool layerChanged = false;
    if (needsMaskLayer) {
        if (!m_maskLayer) {
            m_maskLayer = createGraphicsLayer(CompositingReasonLayerForMask);
            m_maskLayer->setPaintingPhase(GraphicsLayerPaintMask);
            layerChanged = true;
        }
    } else if (m_maskLayer) {
        m_maskLayer = nullptr;
        layerChanged = true;
    }

    return layerChanged;
}

bool CompositedLayerMapping::updateClippingMaskLayers(bool needsChildClippingMaskLayer)
{
    bool layerChanged = false;
    if (needsChildClippingMaskLayer) {
        if (!m_childClippingMaskLayer) {
            m_childClippingMaskLayer = createGraphicsLayer(CompositingReasonLayerForClippingMask);
            m_childClippingMaskLayer->setPaintingPhase(GraphicsLayerPaintChildClippingMask);
            layerChanged = true;
        }
    } else if (m_childClippingMaskLayer) {
        m_childClippingMaskLayer = nullptr;
        layerChanged = true;
    }
    return layerChanged;
}

bool CompositedLayerMapping::updateScrollingLayers(bool needsScrollingLayers)
{
    ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer);

    bool layerChanged = false;
    if (needsScrollingLayers) {
        if (!m_scrollingLayer) {
            // Outer layer which corresponds with the scroll view.
            m_scrollingLayer = createGraphicsLayer(CompositingReasonLayerForScrollingContainer);
            m_scrollingLayer->setDrawsContent(false);
            m_scrollingLayer->setMasksToBounds(true);

            // Inner layer which renders the content that scrolls.
            m_scrollingContentsLayer = createGraphicsLayer(CompositingReasonLayerForScrollingContents);
            m_scrollingLayer->addChild(m_scrollingContentsLayer.get());

            m_scrollingBlockSelectionLayer = createGraphicsLayer(CompositingReasonLayerForScrollingBlockSelection);
            m_scrollingBlockSelectionLayer->setDrawsContent(true);
            m_scrollingContentsLayer->addChild(m_scrollingBlockSelectionLayer.get());

            layerChanged = true;
            if (scrollingCoordinator)
                scrollingCoordinator->scrollableAreaScrollLayerDidChange(m_owningLayer.scrollableArea());
        }
    } else if (m_scrollingLayer) {
        m_scrollingLayer = nullptr;
        m_scrollingContentsLayer = nullptr;
        m_scrollingBlockSelectionLayer = nullptr;
        layerChanged = true;
        if (scrollingCoordinator)
            scrollingCoordinator->scrollableAreaScrollLayerDidChange(m_owningLayer.scrollableArea());
    }

    return layerChanged;
}

static void updateScrollParentForGraphicsLayer(GraphicsLayer* layer, GraphicsLayer* topmostLayer, Layer* scrollParent, ScrollingCoordinator* scrollingCoordinator)
{
    if (!layer)
        return;

    // Only the topmost layer has a scroll parent. All other layers have a null scroll parent.
    if (layer != topmostLayer)
        scrollParent = 0;

    scrollingCoordinator->updateScrollParentForGraphicsLayer(layer, scrollParent);
}

void CompositedLayerMapping::updateScrollParent(Layer* scrollParent)
{
    if (ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer)) {
        GraphicsLayer* topmostLayer = childForSuperlayers();
        updateScrollParentForGraphicsLayer(m_squashingContainmentLayer.get(), topmostLayer, scrollParent, scrollingCoordinator);
        updateScrollParentForGraphicsLayer(m_ancestorClippingLayer.get(), topmostLayer, scrollParent, scrollingCoordinator);
        updateScrollParentForGraphicsLayer(m_graphicsLayer.get(), topmostLayer, scrollParent, scrollingCoordinator);
    }
}

static void updateClipParentForGraphicsLayer(GraphicsLayer* layer, GraphicsLayer* topmostLayer, Layer* clipParent, ScrollingCoordinator* scrollingCoordinator)
{
    if (!layer)
        return;

    // Only the topmost layer has a scroll parent. All other layers have a null scroll parent.
    if (layer != topmostLayer)
        clipParent = 0;

    scrollingCoordinator->updateClipParentForGraphicsLayer(layer, clipParent);
}

void CompositedLayerMapping::updateClipParent()
{
    if (owningLayerClippedByLayerNotAboveCompositedAncestor())
        return;

    Layer* clipParent = m_owningLayer.clipParent();
    if (clipParent)
        clipParent = clipParent->enclosingLayerWithCompositedLayerMapping(IncludeSelf);

    if (ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer)) {
        GraphicsLayer* topmostLayer = childForSuperlayers();
        updateClipParentForGraphicsLayer(m_squashingContainmentLayer.get(), topmostLayer, clipParent, scrollingCoordinator);
        updateClipParentForGraphicsLayer(m_ancestorClippingLayer.get(), topmostLayer, clipParent, scrollingCoordinator);
        updateClipParentForGraphicsLayer(m_graphicsLayer.get(), topmostLayer, clipParent, scrollingCoordinator);
    }
}

bool CompositedLayerMapping::updateSquashingLayers(bool needsSquashingLayers)
{
    bool layersChanged = false;

    if (needsSquashingLayers) {
        if (!m_squashingLayer) {
            m_squashingLayer = createGraphicsLayer(CompositingReasonLayerForSquashingContents);
            m_squashingLayer->setDrawsContent(true);
            layersChanged = true;
        }

        if (m_ancestorClippingLayer) {
            if (m_squashingContainmentLayer) {
                m_squashingContainmentLayer->removeFromParent();
                m_squashingContainmentLayer = nullptr;
                layersChanged = true;
            }
        } else {
            if (!m_squashingContainmentLayer) {
                m_squashingContainmentLayer = createGraphicsLayer(CompositingReasonLayerForSquashingContainer);
                m_squashingContainmentLayer->setShouldFlattenTransform(false);
                layersChanged = true;
            }
        }

        ASSERT((m_ancestorClippingLayer && !m_squashingContainmentLayer) || (!m_ancestorClippingLayer && m_squashingContainmentLayer));
        ASSERT(m_squashingLayer);
    } else {
        if (m_squashingLayer) {
            m_squashingLayer->removeFromParent();
            m_squashingLayer = nullptr;
            layersChanged = true;
        }
        if (m_squashingContainmentLayer) {
            m_squashingContainmentLayer->removeFromParent();
            m_squashingContainmentLayer = nullptr;
            layersChanged = true;
        }
        ASSERT(!m_squashingLayer && !m_squashingContainmentLayer);
    }

    return layersChanged;
}

GraphicsLayerPaintingPhase CompositedLayerMapping::paintingPhaseForPrimaryLayer() const
{
    unsigned phase = 0;
    if (!m_backgroundLayer)
        phase |= GraphicsLayerPaintBackground;
    if (!m_foregroundLayer)
        phase |= GraphicsLayerPaintForeground;
    if (!m_maskLayer)
        phase |= GraphicsLayerPaintMask;

    if (m_scrollingContentsLayer) {
        phase &= ~GraphicsLayerPaintForeground;
        phase |= GraphicsLayerPaintCompositedScroll;
    }

    return static_cast<GraphicsLayerPaintingPhase>(phase);
}

float CompositedLayerMapping::compositingOpacity(float rendererOpacity) const
{
    float finalOpacity = rendererOpacity;

    for (Layer* curr = m_owningLayer.parent(); curr; curr = curr->parent()) {
        // We only care about parents that are stacking contexts.
        // Recall that opacity creates stacking context.
        if (!curr->stackingNode()->isStackingContext())
            continue;

        // If we found a composited layer, regardless of whether it actually
        // paints into it, we want to compute opacity relative to it. So we can
        // break here.
        //
        // FIXME: with grouped backings, a composited descendant will have to
        // continue past the grouped (squashed) layers that its parents may
        // contribute to. This whole confusion can be avoided by specifying
        // explicitly the composited ancestor where we would stop accumulating
        // opacity.
        if (curr->compositingState() == PaintsIntoOwnBacking)
            break;

        finalOpacity *= curr->layoutObject()->opacity();
    }

    return finalOpacity;
}

Color CompositedLayerMapping::rendererBackgroundColor() const
{
    LayoutObject* backgroundRenderer = layoutObject();
    if (backgroundRenderer->isDocumentElement())
        backgroundRenderer = backgroundRenderer->rendererForRootBackground();

    return backgroundRenderer->resolveColor(CSSPropertyBackgroundColor);
}

void CompositedLayerMapping::updateBackgroundColor()
{
    m_graphicsLayer->setBackgroundColor(rendererBackgroundColor());
}

bool CompositedLayerMapping::paintsChildren() const
{
    if (m_owningLayer.hasVisibleContent() && m_owningLayer.hasNonEmptyChildRenderers())
        return true;

    if (hasVisibleNonCompositingDescendant(&m_owningLayer))
        return true;

    return false;
}

static bool isCompositedPlugin(LayoutObject* renderer)
{
    return renderer->isEmbeddedObject() && toLayoutEmbeddedObject(renderer)->requiresAcceleratedCompositing();
}

bool CompositedLayerMapping::hasVisibleNonCompositingDescendant(Layer* parent)
{
    if (!parent->hasVisibleDescendant())
        return false;

    // FIXME: We shouldn't be called with a stale z-order lists. See bug 85512.
    parent->stackingNode()->updateLayerListsIfNeeded();

#if ENABLE(ASSERT)
    LayerListMutationDetector mutationChecker(parent->stackingNode());
#endif

    LayerStackingNodeIterator normalFlowIterator(*parent->stackingNode(), AllChildren);
    while (LayerStackingNode* curNode = normalFlowIterator.next()) {
        Layer* curLayer = curNode->layer();
        if (curLayer->hasCompositedLayerMapping())
            continue;
        if (curLayer->hasVisibleContent() || hasVisibleNonCompositingDescendant(curLayer))
            return true;
    }

    return false;
}

bool CompositedLayerMapping::containsPaintedContent() const
{
    if (m_owningLayer.isReflection())
        return false;

    if (layoutObject()->isImage() && isDirectlyCompositedImage())
        return false;

    LayoutObject* layoutObject = this->layoutObject();
    // FIXME: we could optimize cases where the image, video or canvas is known to fill the border box entirely,
    // and set background color on the layer in that case, instead of allocating backing store and painting.
    if (layoutObject->isVideo() && toLayoutVideo(layoutObject)->shouldDisplayVideo())
        return m_owningLayer.hasBoxDecorationsOrBackground();

    if (m_owningLayer.hasVisibleBoxDecorations())
        return true;

    if (layoutObject->hasMask()) // masks require special treatment
        return true;

    if (layoutObject->isReplaced() && !isCompositedPlugin(layoutObject))
        return true;

    if (layoutObject->isLayoutRegion())
        return true;

    if (layoutObject->node() && layoutObject->node()->isDocumentNode()) {
        // Look to see if the root object has a non-simple background
        LayoutObject* rootObject = layoutObject->document().documentElement() ? layoutObject->document().documentElement()->layoutObject() : 0;
        // Reject anything that has a border, a border-radius or outline,
        // or is not a simple background (no background, or solid color).
        if (rootObject && hasBoxDecorationsOrBackgroundImage(rootObject->styleRef()))
            return true;

        // Now look at the body's renderer.
        HTMLElement* body = layoutObject->document().body();
        LayoutObject* bodyObject = isHTMLBodyElement(body) ? body->layoutObject() : 0;
        if (bodyObject && hasBoxDecorationsOrBackgroundImage(bodyObject->styleRef()))
            return true;
    }

    // FIXME: it's O(n^2). A better solution is needed.
    return paintsChildren();
}

// An image can be directly compositing if it's the sole content of the layer, and has no box decorations
// that require painting. Direct compositing saves backing store.
bool CompositedLayerMapping::isDirectlyCompositedImage() const
{
    ASSERT(layoutObject()->isImage());

    LayoutObject* layoutObject = this->layoutObject();
    if (m_owningLayer.hasBoxDecorationsOrBackground() || layoutObject->hasClip() || layoutObject->hasClipPath())
        return false;

    LayoutImage* imageRenderer = toLayoutImage(layoutObject);
    if (ImageResource* cachedImage = imageRenderer->cachedImage()) {
        if (!cachedImage->hasImage())
            return false;

        Image* image = cachedImage->imageForRenderer(imageRenderer);
        return image->isBitmapImage();
    }

    return false;
}

void CompositedLayerMapping::contentChanged(ContentChangeType changeType)
{
    if ((changeType == ImageChanged) && layoutObject()->isImage() && isDirectlyCompositedImage()) {
        updateImageContents();
        return;
    }

    if (changeType == CanvasChanged && isAcceleratedCanvas(layoutObject())) {
        m_graphicsLayer->setContentsNeedsDisplay();
        return;
    }
}

void CompositedLayerMapping::updateImageContents()
{
    ASSERT(layoutObject()->isImage());
    LayoutImage* imageRenderer = toLayoutImage(layoutObject());

    ImageResource* cachedImage = imageRenderer->cachedImage();
    if (!cachedImage)
        return;

    Image* image = cachedImage->imageForRenderer(imageRenderer);
    if (!image)
        return;

    // We have to wait until the image is fully loaded before setting it on the layer.
    if (!cachedImage->isLoaded())
        return;

    // This is a no-op if the layer doesn't have an inner layer for the image.
    m_graphicsLayer->setContentsToImage(image);

    m_graphicsLayer->setFilterLevel(layoutObject()->style()->imageRendering() == ImageRenderingPixelated ? SkPaint::kNone_FilterLevel : SkPaint::kLow_FilterLevel);

    // Prevent double-drawing: https://bugs.webkit.org/show_bug.cgi?id=58632
    updateDrawsContent();

    // Image animation is "lazy", in that it automatically stops unless someone is drawing
    // the image. So we have to kick the animation each time; this has the downside that the
    // image will keep animating, even if its layer is not visible.
    image->startAnimation();
}

FloatPoint3D CompositedLayerMapping::computeTransformOrigin(const IntRect& borderBox) const
{
    const LayoutStyle& style = layoutObject()->styleRef();

    FloatPoint3D origin;
    origin.setX(floatValueForLength(style.transformOriginX(), borderBox.width()));
    origin.setY(floatValueForLength(style.transformOriginY(), borderBox.height()));
    origin.setZ(style.transformOriginZ());

    return origin;
}

// Return the offset from the top-left of this compositing layer at which the
// renderer's contents are painted.
LayoutSize CompositedLayerMapping::contentOffsetInCompositingLayer() const
{
    ASSERT(!m_contentOffsetInCompositingLayerDirty);
    return m_contentOffsetInCompositingLayer;
}

LayoutRect CompositedLayerMapping::contentsBox() const
{
    LayoutRect contentsBox = LayoutRect(contentsRect(layoutObject()));
    contentsBox.move(contentOffsetInCompositingLayer());
    return contentsBox;
}

bool CompositedLayerMapping::needsToReparentOverflowControls() const
{
    return m_owningLayer.scrollableArea()
        && m_owningLayer.scrollableArea()->hasOverlayScrollbars()
        && m_owningLayer.scrollableArea()->topmostScrollChild();
}

GraphicsLayer* CompositedLayerMapping::detachLayerForOverflowControls(const Layer& enclosingLayer)
{
    GraphicsLayer* host = m_overflowControlsClippingLayer.get();
    if (!host)
        host = m_overflowControlsHostLayer.get();
    host->removeFromParent();
    return host;
}

GraphicsLayer* CompositedLayerMapping::parentForSublayers() const
{
    if (m_scrollingBlockSelectionLayer)
        return m_scrollingBlockSelectionLayer.get();

    if (m_scrollingContentsLayer)
        return m_scrollingContentsLayer.get();

    if (m_childContainmentLayer)
        return m_childContainmentLayer.get();

    if (m_childTransformLayer)
        return m_childTransformLayer.get();

    return m_graphicsLayer.get();
}

GraphicsLayer* CompositedLayerMapping::childForSuperlayers() const
{
    if (m_squashingContainmentLayer)
        return m_squashingContainmentLayer.get();

    if (m_ancestorClippingLayer)
        return m_ancestorClippingLayer.get();

    return m_graphicsLayer.get();
}

GraphicsLayer* CompositedLayerMapping::layerForChildrenTransform() const
{
    if (GraphicsLayer* clipLayer = clippingLayer())
        return clipLayer;
    if (m_scrollingLayer)
        return m_scrollingLayer.get();
    return m_childTransformLayer.get();
}

void CompositedLayerMapping::setBlendMode(WebBlendMode blendMode)
{
    if (m_ancestorClippingLayer) {
        m_ancestorClippingLayer->setBlendMode(blendMode);
        m_graphicsLayer->setBlendMode(WebBlendModeNormal);
    } else {
        m_graphicsLayer->setBlendMode(blendMode);
    }
}

GraphicsLayerUpdater::UpdateType CompositedLayerMapping::updateTypeForChildren(GraphicsLayerUpdater::UpdateType updateType) const
{
    if (m_pendingUpdateScope >= GraphicsLayerUpdateSubtree)
        return GraphicsLayerUpdater::ForceUpdate;
    return updateType;
}

struct SetContentsNeedsDisplayFunctor {
    void operator() (GraphicsLayer* layer) const
    {
        if (layer->drawsContent())
            layer->setNeedsDisplay();
    }
};

void CompositedLayerMapping::setSquashingContentsNeedDisplay()
{
    ApplyToGraphicsLayers(this, SetContentsNeedsDisplayFunctor(), ApplyToSquashingLayer);
}

void CompositedLayerMapping::setContentsNeedDisplay()
{
    // FIXME: need to split out paint invalidations for the background.
    ApplyToGraphicsLayers(this, SetContentsNeedsDisplayFunctor(), ApplyToContentLayers);
}

struct SetContentsNeedsDisplayInRectFunctor {
    void operator() (GraphicsLayer* layer) const
    {
        if (layer->drawsContent()) {
            IntRect layerDirtyRect = r;
            layerDirtyRect.move(-layer->offsetFromRenderer());
            layer->setNeedsDisplayInRect(layerDirtyRect, invalidationReason);
        }
    }

    IntRect r;
    PaintInvalidationReason invalidationReason;
};

// r is in the coordinate space of the layer's render object
void CompositedLayerMapping::setContentsNeedDisplayInRect(const LayoutRect& r, PaintInvalidationReason invalidationReason)
{
    // FIXME: need to split out paint invalidations for the background.
    // FIXME: need to distinguish invalidations for different layers (e.g. the main layer and scrolling layer). crbug.com/416535.
    SetContentsNeedsDisplayInRectFunctor functor = {
        pixelSnappedIntRect(r.location() + m_owningLayer.subpixelAccumulation(), r.size()),
        invalidationReason
    };
    ApplyToGraphicsLayers(this, functor, ApplyToContentLayers);
}

void CompositedLayerMapping::invalidateDisplayItemClient(const DisplayItemClientData& displayItemClientData)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());

    // FIXME: need to split out paint invalidations for the background.
    // FIXME: need to distinguish invalidations for different layers (e.g. the main layer and scrolling layer). crbug.com/416535.
    ApplyToGraphicsLayers(this, [&displayItemClientData](GraphicsLayer* layer) {
        if (layer->drawsContent())
            layer->invalidateDisplayItemClient(displayItemClientData);
    }, ApplyToContentLayers);
}

const GraphicsLayerPaintInfo* CompositedLayerMapping::containingSquashedLayer(const LayoutObject* layoutObject, const Vector<GraphicsLayerPaintInfo>& layers, unsigned maxSquashedLayerIndex)
{
    for (size_t i = 0; i < layers.size() && i < maxSquashedLayerIndex; ++i) {
        if (layoutObject->isDescendantOf(layers[i].renderLayer->layoutObject()))
            return &layers[i];
    }
    return 0;
}

const GraphicsLayerPaintInfo* CompositedLayerMapping::containingSquashedLayer(const LayoutObject* layoutObject, unsigned maxSquashedLayerIndex)
{
    return CompositedLayerMapping::containingSquashedLayer(layoutObject, m_squashedLayers, maxSquashedLayerIndex);
}

IntRect CompositedLayerMapping::localClipRectForSquashedLayer(const Layer& referenceLayer, const GraphicsLayerPaintInfo& paintInfo, const Vector<GraphicsLayerPaintInfo>& layers)
{
    const LayoutObject* clippingContainer = paintInfo.renderLayer->clippingContainer();
    if (clippingContainer == referenceLayer.clippingContainer())
        return LayoutRect::infiniteIntRect();

    ASSERT(clippingContainer);

    const GraphicsLayerPaintInfo* ancestorPaintInfo = containingSquashedLayer(clippingContainer, layers, layers.size());
    // Must be there, otherwise CompositingLayerAssigner::canSquashIntoCurrentSquashingOwner would have disallowed squashing.
    ASSERT(ancestorPaintInfo);

    // FIXME: this is a potential performance issue. We should consider caching these clip rects or otherwise optimizing.
    ClipRectsContext clipRectsContext(ancestorPaintInfo->renderLayer, UncachedClipRects);
    IntRect parentClipRect = pixelSnappedIntRect(paintInfo.renderLayer->clipper().backgroundClipRect(clipRectsContext).rect());
    ASSERT(parentClipRect != LayoutRect::infiniteIntRect());

    // Convert from ancestor to local coordinates.
    IntSize ancestorToLocalOffset = paintInfo.offsetFromRenderer - ancestorPaintInfo->offsetFromRenderer;
    parentClipRect.move(ancestorToLocalOffset);
    return parentClipRect;
}

void CompositedLayerMapping::doPaintTask(const GraphicsLayerPaintInfo& paintInfo, const PaintLayerFlags& paintLayerFlags, GraphicsContext* context,
    const IntRect& clip) // In the coords of rootLayer.
{
    FontCachePurgePreventer fontCachePurgePreventer;

    IntSize offset = paintInfo.offsetFromRenderer;
    AffineTransform translation;
    translation.translate(-offset.width(), -offset.height());
    TransformRecorder transformRecorder(*context, displayItemClient(), translation);

    // The dirtyRect is in the coords of the painting root.
    IntRect dirtyRect(clip);
    dirtyRect.move(offset);

    if (!(paintLayerFlags & PaintLayerPaintingOverflowContents)) {
        LayoutRect bounds = paintInfo.compositedBounds;
        bounds.move(paintInfo.renderLayer->subpixelAccumulation());
        dirtyRect.intersect(pixelSnappedIntRect(bounds));
    } else {
        dirtyRect.move(roundedIntSize(paintInfo.renderLayer->subpixelAccumulation()));
    }

#if ENABLE(ASSERT)
    paintInfo.renderLayer->layoutObject()->assertSubtreeIsLaidOut();
#endif

    float deviceScaleFactor = blink::deviceScaleFactor(paintInfo.renderLayer->layoutObject()->frame());
    context->setDeviceScaleFactor(deviceScaleFactor);

    if (paintInfo.renderLayer->compositingState() != PaintsIntoGroupedBacking) {
        // FIXME: GraphicsLayers need a way to split for LayoutRegions.
        LayerPaintingInfo paintingInfo(paintInfo.renderLayer, LayoutRect(dirtyRect), PaintBehaviorNormal, paintInfo.renderLayer->subpixelAccumulation());
        LayerPainter(*paintInfo.renderLayer).paintLayerContents(context, paintingInfo, paintLayerFlags);

        if (paintInfo.renderLayer->containsDirtyOverlayScrollbars())
            LayerPainter(*paintInfo.renderLayer).paintLayerContents(context, paintingInfo, paintLayerFlags | PaintLayerPaintingOverlayScrollbars);
    } else {
        LayerPaintingInfo paintingInfo(paintInfo.renderLayer, LayoutRect(dirtyRect), PaintBehaviorNormal, paintInfo.renderLayer->subpixelAccumulation());

        // Layer::paintLayer assumes that the caller clips to the passed rect. Squashed layers need to do this clipping in software,
        // since there is no graphics layer to clip them precisely. Furthermore, in some cases we squash layers that need clipping in software
        // from clipping ancestors (see CompositedLayerMapping::localClipRectForSquashedLayer()).
        // FIXME: Is it correct to clip to dirtyRect in slimming paint mode?
        // FIXME: Combine similar code here and LayerClipRecorder.
        dirtyRect.intersect(paintInfo.localClipRectForSquashedLayer);
        {
            OwnPtr<DisplayItem> clipDisplayItem = ClipDisplayItem::create(displayItemClient(), DisplayItem::ClipLayerOverflowControls, dirtyRect);
            if (context->displayItemList()) {
                ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
                context->displayItemList()->add(clipDisplayItem.release());
            } else {
                clipDisplayItem->replay(context);
            }
        }
        LayerPainter(*paintInfo.renderLayer).paintLayer(context, paintingInfo, paintLayerFlags);
        {
            OwnPtr<DisplayItem> endClipDisplayItem = EndClipDisplayItem::create(displayItemClient(), DisplayItem::clipTypeToEndClipType(DisplayItem::ClipLayerOverflowControls));
            if (context->displayItemList()) {
                ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
                context->displayItemList()->add(endClipDisplayItem.release());
            } else {
                endClipDisplayItem->replay(context);
            }
        }
    }
}

static void paintScrollbar(Scrollbar* scrollbar, GraphicsContext& context, const IntRect& clip)
{
    if (!scrollbar)
        return;

    const IntRect& scrollbarRect = scrollbar->frameRect();
    TransformRecorder transformRecorder(context, scrollbar->displayItemClient(), AffineTransform::translation(-scrollbarRect.x(), -scrollbarRect.y()));
    IntRect transformedClip = clip;
    transformedClip.moveBy(scrollbarRect.location());
    scrollbar->paint(&context, transformedClip);
}

// Up-call from compositing layer drawing callback.
void CompositedLayerMapping::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& context, GraphicsLayerPaintingPhase graphicsLayerPaintingPhase, const IntRect& clip)
{
    // https://code.google.com/p/chromium/issues/detail?id=343772
    DisableCompositingQueryAsserts disabler;
#if ENABLE(ASSERT)
    // FIXME: once the state machine is ready, this can be removed and we can refer to that instead.
    if (Page* page = layoutObject()->frame()->page())
        page->setIsPainting(true);
#endif
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "Paint", "data", InspectorPaintEvent::data(m_owningLayer.layoutObject(), LayoutRect(clip), graphicsLayer));

    PaintLayerFlags paintLayerFlags = 0;
    if (graphicsLayerPaintingPhase & GraphicsLayerPaintBackground)
        paintLayerFlags |= PaintLayerPaintingCompositingBackgroundPhase;
    if (graphicsLayerPaintingPhase & GraphicsLayerPaintForeground)
        paintLayerFlags |= PaintLayerPaintingCompositingForegroundPhase;
    if (graphicsLayerPaintingPhase & GraphicsLayerPaintMask)
        paintLayerFlags |= PaintLayerPaintingCompositingMaskPhase;
    if (graphicsLayerPaintingPhase & GraphicsLayerPaintChildClippingMask)
        paintLayerFlags |= PaintLayerPaintingChildClippingMaskPhase;
    if (graphicsLayerPaintingPhase & GraphicsLayerPaintOverflowContents)
        paintLayerFlags |= PaintLayerPaintingOverflowContents;
    if (graphicsLayerPaintingPhase & GraphicsLayerPaintCompositedScroll)
        paintLayerFlags |= PaintLayerPaintingCompositingScrollingPhase;

    if (graphicsLayer == m_backgroundLayer)
        paintLayerFlags |= (PaintLayerPaintingRootBackgroundOnly | PaintLayerPaintingCompositingForegroundPhase); // Need PaintLayerPaintingCompositingForegroundPhase to walk child layers.
    else if (compositor()->fixedRootBackgroundLayer())
        paintLayerFlags |= PaintLayerPaintingSkipRootBackground;

    if (graphicsLayer == m_graphicsLayer.get()
        || graphicsLayer == m_foregroundLayer.get()
        || graphicsLayer == m_backgroundLayer.get()
        || graphicsLayer == m_maskLayer.get()
        || graphicsLayer == m_childClippingMaskLayer.get()
        || graphicsLayer == m_scrollingContentsLayer.get()
        || graphicsLayer == m_scrollingBlockSelectionLayer.get()) {

        GraphicsLayerPaintInfo paintInfo;
        paintInfo.renderLayer = &m_owningLayer;
        paintInfo.compositedBounds = compositedBounds();
        paintInfo.offsetFromRenderer = graphicsLayer->offsetFromRenderer();

        // We have to use the same root as for hit testing, because both methods can compute and cache clipRects.
        doPaintTask(paintInfo, paintLayerFlags, &context, clip);
    } else if (graphicsLayer == m_squashingLayer.get()) {
        for (size_t i = 0; i < m_squashedLayers.size(); ++i)
            doPaintTask(m_squashedLayers[i], paintLayerFlags, &context, clip);
    } else if (graphicsLayer == layerForHorizontalScrollbar()) {
        paintScrollbar(m_owningLayer.scrollableArea()->horizontalScrollbar(), context, clip);
    } else if (graphicsLayer == layerForVerticalScrollbar()) {
        paintScrollbar(m_owningLayer.scrollableArea()->verticalScrollbar(), context, clip);
    } else if (graphicsLayer == layerForScrollCorner()) {
        const IntRect& scrollCornerAndResizer = m_owningLayer.scrollableArea()->scrollCornerAndResizerRect();
        TransformRecorder transformRecorder(context, m_owningLayer.scrollableArea()->displayItemClient(), AffineTransform::translation(-scrollCornerAndResizer.x(), -scrollCornerAndResizer.y()));
        IntRect transformedClip = clip;
        transformedClip.moveBy(scrollCornerAndResizer.location());
        ScrollableAreaPainter(*m_owningLayer.scrollableArea()).paintScrollCorner(&context, IntPoint(), transformedClip);
        ScrollableAreaPainter(*m_owningLayer.scrollableArea()).paintResizer(&context, IntPoint(), transformedClip);
    }
    InspectorInstrumentation::didPaint(m_owningLayer.layoutObject(), graphicsLayer, &context, LayoutRect(clip));
#if ENABLE(ASSERT)
    if (Page* page = layoutObject()->frame()->page())
        page->setIsPainting(false);
#endif
}

bool CompositedLayerMapping::isTrackingPaintInvalidations() const
{
    GraphicsLayerClient* client = compositor();
    return client ? client->isTrackingPaintInvalidations() : false;
}

#if ENABLE(ASSERT)
void CompositedLayerMapping::verifyNotPainting()
{
    ASSERT(!layoutObject()->frame()->page() || !layoutObject()->frame()->page()->isPainting());
}
#endif

void CompositedLayerMapping::notifyAnimationStarted(const GraphicsLayer*, double monotonicTime, int group)
{
    layoutObject()->node()->document().compositorPendingAnimations().notifyCompositorAnimationStarted(monotonicTime, group);
}

IntRect CompositedLayerMapping::pixelSnappedCompositedBounds() const
{
    LayoutRect bounds = m_compositedBounds;
    bounds.move(m_owningLayer.subpixelAccumulation());
    return pixelSnappedIntRect(bounds);
}

bool CompositedLayerMapping::updateSquashingLayerAssignment(Layer* squashedLayer, const Layer& owningLayer, size_t nextSquashedLayerIndex)
{
    GraphicsLayerPaintInfo paintInfo;
    paintInfo.renderLayer = squashedLayer;
    // NOTE: composited bounds are updated elsewhere
    // NOTE: offsetFromRenderer is updated elsewhere

    // Change tracking on squashing layers: at the first sign of something changed, just invalidate the layer.
    // FIXME: Perhaps we can find a tighter more clever mechanism later.
    bool updatedAssignment = false;
    if (nextSquashedLayerIndex < m_squashedLayers.size()) {
        if (paintInfo.renderLayer != m_squashedLayers[nextSquashedLayerIndex].renderLayer) {
            compositor()->paintInvalidationOnCompositingChange(squashedLayer);
            updatedAssignment = true;
            m_squashedLayers[nextSquashedLayerIndex] = paintInfo;
        }
    } else {
        compositor()->paintInvalidationOnCompositingChange(squashedLayer);
        m_squashedLayers.append(paintInfo);
        updatedAssignment = true;
    }
    squashedLayer->setGroupedMapping(this);
    return updatedAssignment;
}

void CompositedLayerMapping::removeLayerFromSquashingGraphicsLayer(const Layer* layer)
{
    size_t layerIndex = kNotFound;

    for (size_t i = 0; i < m_squashedLayers.size(); ++i) {
        if (m_squashedLayers[i].renderLayer == layer) {
            layerIndex = i;
            break;
        }
    }

    if (layerIndex == kNotFound)
        return;

    m_squashedLayers.remove(layerIndex);
}

void CompositedLayerMapping::finishAccumulatingSquashingLayers(size_t nextSquashedLayerIndex)
{
    // Any additional squashed Layers in the array no longer exist, and removing invalidates the squashingLayer contents.
    if (nextSquashedLayerIndex < m_squashedLayers.size())
        m_squashedLayers.remove(nextSquashedLayerIndex, m_squashedLayers.size() - nextSquashedLayerIndex);
}

String CompositedLayerMapping::debugName(const GraphicsLayer* graphicsLayer)
{
    String name;
    if (graphicsLayer == m_graphicsLayer.get()) {
        name = m_owningLayer.debugName();
    } else if (graphicsLayer == m_squashingContainmentLayer.get()) {
        name = "Squashing Containment Layer";
    } else if (graphicsLayer == m_squashingLayer.get()) {
        name = "Squashing Layer";
    } else if (graphicsLayer == m_ancestorClippingLayer.get()) {
        name = "Ancestor Clipping Layer";
    } else if (graphicsLayer == m_foregroundLayer.get()) {
        name = m_owningLayer.debugName() + " (foreground) Layer";
    } else if (graphicsLayer == m_backgroundLayer.get()) {
        name = m_owningLayer.debugName() + " (background) Layer";
    } else if (graphicsLayer == m_childContainmentLayer.get()) {
        name = "Child Containment Layer";
    } else if (graphicsLayer == m_childTransformLayer.get()) {
        name = "Child Transform Layer";
    } else if (graphicsLayer == m_maskLayer.get()) {
        name = "Mask Layer";
    } else if (graphicsLayer == m_childClippingMaskLayer.get()) {
        name = "Child Clipping Mask Layer";
    } else if (graphicsLayer == m_layerForHorizontalScrollbar.get()) {
        name = "Horizontal Scrollbar Layer";
    } else if (graphicsLayer == m_layerForVerticalScrollbar.get()) {
        name = "Vertical Scrollbar Layer";
    } else if (graphicsLayer == m_layerForScrollCorner.get()) {
        name = "Scroll Corner Layer";
    } else if (graphicsLayer == m_overflowControlsHostLayer.get()) {
        name = "Overflow Controls Host Layer";
    } else if (graphicsLayer == m_overflowControlsClippingLayer.get()) {
        name = "Overflow Controls ClipLayer Layer";
    } else if (graphicsLayer == m_scrollingLayer.get()) {
        name = "Scrolling Layer";
    } else if (graphicsLayer == m_scrollingContentsLayer.get()) {
        name = "Scrolling Contents Layer";
    } else if (graphicsLayer == m_scrollingBlockSelectionLayer.get()) {
        name = "Scrolling Block Selection Layer";
    } else {
        ASSERT_NOT_REACHED();
    }

    return name;
}

} // namespace blink
