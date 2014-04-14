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

#include "core/rendering/compositing/CompositedLayerMapping.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/animation/ActiveAnimations.h"
#include "core/fetch/ImageResource.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/Chrome.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/plugins/PluginView.h"
#include "core/rendering/FilterEffectRenderer.h"
#include "core/rendering/RenderApplet.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/RenderIFrame.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderLayerStackingNodeIterator.h"
#include "core/rendering/RenderVideo.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/GraphicsLayerUpdater.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "core/rendering/style/KeyframeList.h"
#include "platform/LengthFunctions.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/GraphicsContext.h"
#include "wtf/CurrentTime.h"
#include "wtf/text/StringBuilder.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

static IntRect clipBox(RenderBox* renderer);

static IntRect contentsRect(const RenderObject* renderer)
{
    if (!renderer->isBox())
        return IntRect();

    return renderer->isVideo() ?
        toRenderVideo(renderer)->videoBox() :
        pixelSnappedIntRect(toRenderBox(renderer)->contentBoxRect());
}

static IntRect backgroundRect(const RenderObject* renderer)
{
    if (!renderer->isBox())
        return IntRect();

    LayoutRect rect;
    const RenderBox* box = toRenderBox(renderer);
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

static inline bool isAcceleratedCanvas(const RenderObject* renderer)
{
    if (renderer->isCanvas()) {
        HTMLCanvasElement* canvas = toHTMLCanvasElement(renderer->node());
        if (CanvasRenderingContext* context = canvas->renderingContext())
            return context->isAccelerated();
    }
    return false;
}

static bool hasBoxDecorations(const RenderStyle* style)
{
    return style->hasBorder() || style->hasBorderRadius() || style->hasOutline() || style->hasAppearance() || style->boxShadow() || style->hasFilter();
}

static bool hasBoxDecorationsOrBackgroundImage(const RenderStyle* style)
{
    return hasBoxDecorations(style) || style->hasBackgroundImage();
}

static bool contentLayerSupportsDirectBackgroundComposition(const RenderObject* renderer)
{
    // No support for decorations - border, border-radius or outline.
    // Only simple background - solid color or transparent.
    if (hasBoxDecorationsOrBackgroundImage(renderer->style()))
        return false;

    // If there is no background, there is nothing to support.
    if (!renderer->style()->hasBackground())
        return true;

    // Simple background that is contained within the contents rect.
    return contentsRect(renderer).contains(backgroundRect(renderer));
}

static blink::WebLayer* platformLayerForPlugin(RenderObject* renderer)
{
    if (!renderer->isEmbeddedObject())
        return 0;
    Widget* widget = toRenderEmbeddedObject(renderer)->widget();
    if (!widget || !widget->isPluginView())
        return 0;
    return toPluginView(widget)->platformLayer();

}

static inline bool isAcceleratedContents(RenderObject* renderer)
{
    return isAcceleratedCanvas(renderer)
        || (renderer->isEmbeddedObject() && toRenderEmbeddedObject(renderer)->requiresAcceleratedCompositing())
        || renderer->isVideo();
}

// Get the scrolling coordinator in a way that works inside CompositedLayerMapping's destructor.
static ScrollingCoordinator* scrollingCoordinatorFromLayer(RenderLayer& layer)
{
    Page* page = layer.renderer()->frame()->page();
    if (!page)
        return 0;

    return page->scrollingCoordinator();
}

CompositedLayerMapping::CompositedLayerMapping(RenderLayer& layer)
    : m_owningLayer(layer)
    , m_artificiallyInflatedBounds(false)
    , m_isMainFrameRenderViewLayer(false)
    , m_requiresOwnBackingStoreForIntrinsicReasons(true)
    , m_requiresOwnBackingStoreForAncestorReasons(true)
    , m_canCompositeFilters(false)
    , m_backgroundLayerPaintsFixedRootBackground(false)
    , m_needToUpdateGraphicsLayer(false)
    , m_needToUpdateGraphicsLayerOfAllDecendants(false)
{
    if (layer.isRootLayer() && renderer()->frame()->isMainFrame())
        m_isMainFrameRenderViewLayer = true;

    createPrimaryGraphicsLayer();
}

CompositedLayerMapping::~CompositedLayerMapping()
{
    // Do not leave the destroyed pointer dangling on any RenderLayers that painted to this mapping's squashing layer.
    for (size_t i = 0; i < m_squashedLayers.size(); ++i) {
        RenderLayer* oldSquashedLayer = m_squashedLayers[i].renderLayer;
        if (oldSquashedLayer->groupedMapping() == this) {
            oldSquashedLayer->setGroupedMapping(0, true);
            oldSquashedLayer->setLostGroupedMapping(true);
        }
    }

    updateClippingLayers(false, false);
    updateOverflowControlsLayers(false, false, false);
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
    if (Page* page = renderer()->frame()->page())
        graphicsLayerFactory = page->chrome().client().graphicsLayerFactory();

    OwnPtr<GraphicsLayer> graphicsLayer = GraphicsLayer::create(graphicsLayerFactory, this);

    graphicsLayer->setCompositingReasons(reasons);

    return graphicsLayer.release();
}

void CompositedLayerMapping::createPrimaryGraphicsLayer()
{
    m_graphicsLayer = createGraphicsLayer(m_owningLayer.compositingReasons());

#if !OS(ANDROID)
    if (m_isMainFrameRenderViewLayer)
        m_graphicsLayer->contentLayer()->setDrawCheckerboardForMissingTiles(true);
#endif

    updateOpacity(renderer()->style());
    updateTransform(renderer()->style());
    updateFilters(renderer()->style());
    updateHasGpuRasterizationHint(renderer()->style());

    if (RuntimeEnabledFeatures::cssCompositingEnabled()) {
        updateLayerBlendMode(renderer()->style());
        updateIsRootForIsolatedGroup();
    }
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
}

void CompositedLayerMapping::updateOpacity(const RenderStyle* style)
{
    m_graphicsLayer->setOpacity(compositingOpacity(style->opacity()));
}

void CompositedLayerMapping::updateTransform(const RenderStyle* style)
{
    // FIXME: This could use m_owningLayer.transform(), but that currently has transform-origin
    // baked into it, and we don't want that.
    TransformationMatrix t;
    if (m_owningLayer.hasTransform()) {
        style->applyTransform(t, toRenderBox(renderer())->pixelSnappedBorderBoxRect().size(), RenderStyle::ExcludeTransformOrigin);
        makeMatrixRenderable(t, compositor()->canRender3DTransforms());
    }

    m_graphicsLayer->setTransform(t);
}

void CompositedLayerMapping::updateFilters(const RenderStyle* style)
{
    bool didCompositeFilters = m_canCompositeFilters;
    m_canCompositeFilters = m_graphicsLayer->setFilters(owningLayer().computeFilterOperations(style));
    if (didCompositeFilters != m_canCompositeFilters) {
        //
        // If filters used to be painted in software and are now painted in the compositor, we need to:
        // (1) Remove the FilterEffectRenderer, which was used for painting filters in software.
        // (2) Repaint the layer contents to remove the software-applied filter because the compositor will apply it.
        //
        // Similarly, if filters used to be painted in the compositor and are now painted in software, we need to:
        // (1) Create a FilterEffectRenderer.
        // (2) Repaint the layer contents to apply a software filter because the compositor won't apply it.
        //
        m_owningLayer.updateOrRemoveFilterEffectRenderer();
        setContentsNeedDisplay();
    }
}

void CompositedLayerMapping::updateLayerBlendMode(const RenderStyle* style)
{
    setBlendMode(style->blendMode());
}

void CompositedLayerMapping::updateIsRootForIsolatedGroup()
{
    bool isolate = m_owningLayer.shouldIsolateCompositedDescendants();

    // non stacking context layers should never isolate
    ASSERT(m_owningLayer.stackingNode()->isStackingContext() || !isolate);

    m_graphicsLayer->setIsRootForIsolatedGroup(isolate);
}

void CompositedLayerMapping::updateHasGpuRasterizationHint(const RenderStyle* style)
{
    m_graphicsLayer->setHasGpuRasterizationHint(style->hasWillChangeGpuRasterizationHint());
}

void CompositedLayerMapping::updateContentsOpaque()
{
    // For non-root layers, background is always painted by the primary graphics layer.
    ASSERT(m_isMainFrameRenderViewLayer || !m_backgroundLayer);
    if (m_backgroundLayer) {
        m_graphicsLayer->setContentsOpaque(false);
        m_backgroundLayer->setContentsOpaque(m_owningLayer.backgroundIsKnownToBeOpaqueInRect(compositedBounds()));
    } else {
        m_graphicsLayer->setContentsOpaque(m_owningLayer.backgroundIsKnownToBeOpaqueInRect(compositedBounds()));
    }
}

static bool hasNonZeroTransformOrigin(const RenderObject* renderer)
{
    RenderStyle* style = renderer->style();
    return (style->transformOriginX().type() == Fixed && style->transformOriginX().value())
        || (style->transformOriginY().type() == Fixed && style->transformOriginY().value());
}

static bool layerOrAncestorIsTransformedOrUsingCompositedScrolling(RenderLayer* layer)
{
    for (RenderLayer* curr = layer; curr; curr = curr->parent()) {
        if (curr->hasTransform() || curr->needsCompositedScrolling())
            return true;
    }

    return false;
}

bool CompositedLayerMapping::shouldClipCompositedBounds() const
{
    // Scrollbar layers use this layer for relative positioning, so don't clip.
    if (layerForHorizontalScrollbar() || layerForVerticalScrollbar())
        return false;

    if (layerOrAncestorIsTransformedOrUsingCompositedScrolling(&m_owningLayer))
        return false;

    // Scrolled composited layers are clipped by their ancestor clipping layer,
    // so don't clip these, either.
    if (!compositor()->clippedByAncestor(&m_owningLayer))
        return true;

    if (m_owningLayer.renderer()->containingBlock()->enclosingLayer() != m_owningLayer.ancestorScrollingLayer())
        return true;

    return false;
}

void CompositedLayerMapping::updateCompositedBounds(GraphicsLayerUpdater::UpdateType updateType)
{
    if (!shouldUpdateGraphicsLayer(updateType))
        return;

    // We need to know if we draw content in order to update our bounds (this has an effect
    // on whether or not descendands will paint into our backing). Update this value now.
    updateDrawsContent();

    LayoutRect layerBounds = m_owningLayer.boundingBoxForCompositing();

    // Clip to the size of the document or enclosing overflow-scroll layer.
    // If this or an ancestor is transformed, we can't currently compute the correct rect to intersect with.
    // We'd need RenderObject::convertContainerToLocalQuad(), which doesn't yet exist.
    if (shouldClipCompositedBounds()) {
        RenderView* view = m_owningLayer.renderer()->view();
        RenderLayer* rootLayer = view->layer();

        LayoutRect clippingBounds;
        if (renderer()->style()->position() == FixedPosition && renderer()->container() == view)
            clippingBounds = view->frameView()->viewportConstrainedVisibleContentRect();
        else
            clippingBounds = view->unscaledDocumentRect();

        if (&m_owningLayer != rootLayer)
            clippingBounds.intersect(m_owningLayer.clipper().backgroundClipRect(ClipRectsContext(rootLayer, AbsoluteClipRects)).rect());

        LayoutPoint delta;
        m_owningLayer.convertToLayerCoords(rootLayer, delta);
        clippingBounds.move(-delta.x(), -delta.y());

        layerBounds.intersect(clippingBounds);
    }

    // If the element has a transform-origin that has fixed lengths, and the renderer has zero size,
    // then we need to ensure that the compositing layer has non-zero size so that we can apply
    // the transform-origin via the GraphicsLayer anchorPoint (which is expressed as a fractional value).
    if (layerBounds.isEmpty() && hasNonZeroTransformOrigin(renderer())) {
        layerBounds.setWidth(1);
        layerBounds.setHeight(1);
        m_artificiallyInflatedBounds = true;
    } else {
        m_artificiallyInflatedBounds = false;
    }

    m_compositedBounds = layerBounds;
}

void CompositedLayerMapping::updateAfterWidgetResize()
{
    if (renderer()->isRenderPart()) {
        if (RenderLayerCompositor* innerCompositor = RenderLayerCompositor::frameContentsCompositor(toRenderPart(renderer()))) {
            innerCompositor->frameViewDidChangeSize();
            // We can floor this point because our frameviews are always aligned to pixel boundaries.
            ASSERT(contentsBox().location() == flooredIntPoint(contentsBox().location()));
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

bool CompositedLayerMapping::updateGraphicsLayerConfiguration(GraphicsLayerUpdater::UpdateType updateType)
{
    if (!shouldUpdateGraphicsLayer(updateType))
        return false;

    RenderLayerCompositor* compositor = this->compositor();
    RenderObject* renderer = this->renderer();

    m_owningLayer.updateDescendantDependentFlags();
    m_owningLayer.stackingNode()->updateZOrderLists();

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

    RenderLayer* scrollParent = compositor->acceleratedCompositingForOverflowScrollEnabled() ? m_owningLayer.scrollParent() : 0;
    bool needsAncestorClip = compositor->clippedByAncestor(&m_owningLayer);
    if (scrollParent) {
        // If our containing block is our ancestor scrolling layer, then we'll already be clipped
        // to it via our scroll parent and we don't need an ancestor clipping layer.
        if (m_owningLayer.renderer()->containingBlock()->enclosingLayer() == m_owningLayer.ancestorCompositedScrollingLayer())
            needsAncestorClip = false;
    }

    if (updateClippingLayers(needsAncestorClip, needsDescendantsClippingLayer))
        layerConfigChanged = true;

    if (updateOverflowControlsLayers(requiresHorizontalScrollbarLayer(), requiresVerticalScrollbarLayer(), requiresScrollCornerLayer()))
        layerConfigChanged = true;

    if (updateScrollingLayers(m_owningLayer.needsCompositedScrolling()))
        layerConfigChanged = true;

    bool hasPerspective = false;
    if (RenderStyle* style = renderer->style())
        hasPerspective = style->hasPerspective();
    bool needsChildTransformLayer = hasPerspective && (layerForChildrenTransform() == m_childTransformLayer.get());
    if (updateChildTransformLayer(needsChildTransformLayer))
        layerConfigChanged = true;

    updateScrollParent(scrollParent);
    updateClipParent(m_owningLayer.clipParent());

    if (updateSquashingLayers(!m_squashedLayers.isEmpty()))
        layerConfigChanged = true;

    if (layerConfigChanged)
        updateInternalHierarchy();

    if (updateMaskLayer(renderer->hasMask()))
        m_graphicsLayer->setMaskLayer(m_maskLayer.get());

    bool hasChildClippingLayer = compositor->clipsCompositingDescendants(&m_owningLayer) && (hasClippingLayer() || hasScrollingLayer());
    bool needsChildClippingMask = (renderer->style()->clipPath() || renderer->style()->hasBorderRadius()) && (hasChildClippingLayer || isAcceleratedContents(renderer));
    if (updateClippingMaskLayers(needsChildClippingMask)) {
        if (hasClippingLayer())
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

    if (isDirectlyCompositedImage())
        updateImageContents();

    if (blink::WebLayer* layer = platformLayerForPlugin(renderer)) {
        m_graphicsLayer->setContentsToPlatformLayer(layer);
    } else if (renderer->node() && renderer->node()->isFrameOwnerElement() && toHTMLFrameOwnerElement(renderer->node())->contentFrame()) {
        blink::WebLayer* layer = toHTMLFrameOwnerElement(renderer->node())->contentFrame()->remotePlatformLayer();
        if (layer)
            m_graphicsLayer->setContentsToPlatformLayer(layer);
    } else if (renderer->isVideo()) {
        HTMLMediaElement* mediaElement = toHTMLMediaElement(renderer->node());
        m_graphicsLayer->setContentsToPlatformLayer(mediaElement->platformLayer());
    } else if (isAcceleratedCanvas(renderer)) {
        HTMLCanvasElement* canvas = toHTMLCanvasElement(renderer->node());
        if (CanvasRenderingContext* context = canvas->renderingContext())
            m_graphicsLayer->setContentsToPlatformLayer(context->platformLayer());
        layerConfigChanged = true;
    }
    if (renderer->isRenderPart())
        layerConfigChanged = RenderLayerCompositor::parentFrameContentLayers(toRenderPart(renderer));

    return layerConfigChanged;
}

static IntRect clipBox(RenderBox* renderer)
{
    LayoutRect result = PaintInfo::infiniteRect();
    if (renderer->hasOverflowClip())
        result = renderer->overflowClipRect(LayoutPoint());

    if (renderer->hasClip())
        result.intersect(renderer->clipRect(LayoutPoint()));

    return pixelSnappedIntRect(result);
}

static LayoutPoint computeOffsetFromCompositedAncestor(const RenderLayer* layer, const RenderLayer* compositedAncestor)
{
    LayoutPoint offset;
    layer->convertToLayerCoords(compositedAncestor, offset);
    if (compositedAncestor)
        offset.move(compositedAncestor->compositedLayerMapping()->owningLayer().subpixelAccumulation());
    return offset;
}

void CompositedLayerMapping::adjustBoundsForSubPixelAccumulation(const RenderLayer* compositedAncestor, IntRect& localBounds, IntRect& relativeBounds, IntPoint& delta)
{
    LayoutRect localRawCompositingBounds = compositedBounds();
    LayoutPoint rawDelta = computeOffsetFromCompositedAncestor(&m_owningLayer, compositedAncestor);
    delta = flooredIntPoint(rawDelta);
    LayoutSize subpixelAccumulation = toLayoutSize(rawDelta).fraction();
    m_owningLayer.setSubpixelAccumulation(subpixelAccumulation);

    // Move the bounds by the subpixel accumulation so that it pixel-snaps relative to absolute pixels instead of local coordinates.
    localRawCompositingBounds.move(subpixelAccumulation);
    localBounds = pixelSnappedIntRect(localRawCompositingBounds);

    relativeBounds = localBounds;
    relativeBounds.moveBy(delta);
}

void CompositedLayerMapping::updateSquashingLayerGeometry(const IntPoint& delta)
{
    if (!m_squashingLayer)
        return;

    ASSERT(compositor()->layerSquashingEnabled());

    LayoutRect totalSquashBounds;
    for (size_t i = 0; i < m_squashedLayers.size(); ++i) {
        LayoutRect squashedBounds = m_squashedLayers[i].renderLayer->boundingBoxForCompositing();

        // Store the local bounds of the RenderLayer subtree before applying the offset.
        m_squashedLayers[i].compositedBounds = squashedBounds;

        squashedBounds.move(m_squashedLayers[i].offsetFromSquashingCLM);
        totalSquashBounds.unite(squashedBounds);
    }

    // The totalSquashBounds is positioned with respect to m_owningLayer of this CompositedLayerMapping.
    // But the squashingLayer needs to be positioned with respect to the ancestor CompositedLayerMapping.
    // The conversion between m_owningLayer and the ancestor CLM is already computed in the caller as
    // |delta| + |subpixelAccumulation|.
    LayoutPoint rawDelta = delta + m_owningLayer.subpixelAccumulation();
    totalSquashBounds.moveBy(rawDelta);
    IntRect squashLayerBounds = enclosingIntRect(totalSquashBounds);
    IntPoint squashLayerOrigin = squashLayerBounds.location();
    LayoutPoint squashLayerOriginInOwningLayerSpace = LayoutPoint(squashLayerOrigin - rawDelta);

    m_squashingLayer->setPosition(squashLayerBounds.location());
    m_squashingLayer->setSize(squashLayerBounds.size());

    // Now that the squashing bounds are known, we can convert the RenderLayer painting offsets
    // from CLM owning layer space to the squashing layer space.
    //
    // The painting offset we want to compute for each squashed RenderLayer is essentially the position of
    // the squashed RenderLayer described w.r.t. m_squashingLayer's origin. For this purpose we already cached
    // offsetFromSquashingCLM before, which describes where the squashed RenderLayer is located w.r.t.
    // m_owningLayer. So we just need to convert that point from m_owningLayer space to m_squashingLayer
    // space. This is simply done by subtracing squashLayerOriginInOwningLayerSpace, but then the offset
    // overall needs to be negated because that's the direction that the painting code expects the
    // offset to be.
    for (size_t i = 0; i < m_squashedLayers.size(); ++i) {
        LayoutSize offsetFromSquashLayerOrigin = LayoutPoint(m_squashedLayers[i].offsetFromSquashingCLM) - squashLayerOriginInOwningLayerSpace;
        m_squashedLayers[i].offsetFromRenderer = -flooredIntSize(offsetFromSquashLayerOrigin);

        m_squashedLayers[i].renderLayer->setSubpixelAccumulation(offsetFromSquashLayerOrigin.fraction());
        ASSERT(m_squashedLayers[i].renderLayer->subpixelAccumulation() ==
            toLayoutSize(computeOffsetFromCompositedAncestor(m_squashedLayers[i].renderLayer, m_squashedLayers[i].renderLayer->ancestorCompositingLayer())).fraction());

        // FIXME: find a better design to avoid this redundant value - most likely it will make
        // sense to move the paint task info into RenderLayer's m_compositingProperties.
        m_squashedLayers[i].renderLayer->setOffsetFromSquashingLayerOrigin(m_squashedLayers[i].offsetFromRenderer);
    }
}

void CompositedLayerMapping::updateGraphicsLayerGeometry(GraphicsLayerUpdater::UpdateType updateType)
{
    // If we haven't built z-order lists yet, wait until later.
    if (m_owningLayer.stackingNode()->isStackingContainer() && m_owningLayer.stackingNode()->zOrderListsDirty())
        return;

    if (!shouldUpdateGraphicsLayer(updateType))
        return;

    // Set transform property, if it is not animating. We have to do this here because the transform
    // is affected by the layer dimensions.
    if (!renderer()->style()->isRunningTransformAnimationOnCompositor())
        updateTransform(renderer()->style());

    // Set opacity, if it is not animating.
    if (!renderer()->style()->isRunningOpacityAnimationOnCompositor())
        updateOpacity(renderer()->style());

    m_owningLayer.updateDescendantDependentFlags();

    // m_graphicsLayer is the corresponding GraphicsLayer for this RenderLayer and its non-compositing
    // descendants. So, the visibility flag for m_graphicsLayer should be true if there are any
    // non-compositing visible layers.
    bool contentsVisible = m_owningLayer.hasVisibleContent() || hasVisibleNonCompositingDescendantLayers();
    if (RuntimeEnabledFeatures::overlayFullscreenVideoEnabled() && renderer()->isVideo()) {
        HTMLMediaElement* mediaElement = toHTMLMediaElement(renderer()->node());
        if (mediaElement->isFullscreen())
            contentsVisible = false;
    }
    m_graphicsLayer->setContentsVisible(contentsVisible);

    RenderStyle* style = renderer()->style();
    m_graphicsLayer->setBackfaceVisibility(style->backfaceVisibility() == BackfaceVisibilityVisible);

    RenderLayer* compAncestor = m_owningLayer.ancestorCompositingLayer();

    // We compute everything relative to the enclosing compositing layer.
    IntRect ancestorCompositingBounds;
    if (compAncestor) {
        ASSERT(compAncestor->hasCompositedLayerMapping());
        ancestorCompositingBounds = compAncestor->compositedLayerMapping()->pixelSnappedCompositedBounds();
    }

    IntRect localCompositingBounds;
    IntRect relativeCompositingBounds;
    IntPoint delta;
    adjustBoundsForSubPixelAccumulation(compAncestor, localCompositingBounds, relativeCompositingBounds, delta);

    IntPoint graphicsLayerParentLocation;
    if (compAncestor && compAncestor->compositedLayerMapping()->hasClippingLayer()) {
        // If the compositing ancestor has a layer to clip children, we parent in that, and therefore
        // position relative to it.
        IntRect clippingBox = clipBox(toRenderBox(compAncestor->renderer()));
        graphicsLayerParentLocation = clippingBox.location() + roundedIntSize(compAncestor->subpixelAccumulation());
    } else if (compAncestor && compAncestor->compositedLayerMapping()->childTransformLayer()) {
        // Similarly, if the compositing ancestor has a child transform layer, we parent in that, and therefore
        // position relative to it. It's already taken into account the contents offset, so we do not need to here.
        graphicsLayerParentLocation = roundedIntPoint(compAncestor->subpixelAccumulation());
    } else if (compAncestor) {
        graphicsLayerParentLocation = ancestorCompositingBounds.location();
    } else {
        graphicsLayerParentLocation = renderer()->view()->documentRect().location();
    }

    if (compAncestor && compAncestor->needsCompositedScrolling()) {
        RenderBox* renderBox = toRenderBox(compAncestor->renderer());
        IntSize scrollOffset = renderBox->scrolledContentOffset();
        IntPoint scrollOrigin(renderBox->borderLeft(), renderBox->borderTop());
        graphicsLayerParentLocation = scrollOrigin - scrollOffset;
    }

    if (compAncestor && m_ancestorClippingLayer) {
        ClipRectsContext clipRectsContext(compAncestor, CompositingClipRects, IgnoreOverlayScrollbarSize, IgnoreOverflowClip);
        IntRect parentClipRect = pixelSnappedIntRect(m_owningLayer.clipper().backgroundClipRect(clipRectsContext).rect());
        ASSERT(parentClipRect != PaintInfo::infiniteRect());
        m_ancestorClippingLayer->setPosition(FloatPoint(parentClipRect.location() - graphicsLayerParentLocation));
        m_ancestorClippingLayer->setSize(parentClipRect.size());

        // backgroundRect is relative to compAncestor, so subtract deltaX/deltaY to get back to local coords.
        m_ancestorClippingLayer->setOffsetFromRenderer(parentClipRect.location() - delta);

        // The primary layer is then parented in, and positioned relative to this clipping layer.
        graphicsLayerParentLocation = parentClipRect.location();
    }

    FloatSize contentsSize = relativeCompositingBounds.size();

    m_graphicsLayer->setPosition(FloatPoint(relativeCompositingBounds.location() - graphicsLayerParentLocation));
    m_graphicsLayer->setOffsetFromRenderer(toIntSize(localCompositingBounds.location()));

    FloatSize oldSize = m_graphicsLayer->size();
    if (oldSize != contentsSize)
        m_graphicsLayer->setSize(contentsSize);

    // If we have a layer that clips children, position it.
    IntRect clippingBox;
    if (GraphicsLayer* clipLayer = clippingLayer()) {
        clippingBox = clipBox(toRenderBox(renderer()));
        clipLayer->setPosition(FloatPoint(clippingBox.location() - localCompositingBounds.location() + roundedIntSize(m_owningLayer.subpixelAccumulation())));
        clipLayer->setSize(clippingBox.size());
        clipLayer->setOffsetFromRenderer(toIntSize(clippingBox.location()));
        if (m_childClippingMaskLayer && !m_scrollingLayer) {
            m_childClippingMaskLayer->setPosition(clipLayer->position());
            m_childClippingMaskLayer->setSize(clipLayer->size());
            m_childClippingMaskLayer->setOffsetFromRenderer(clipLayer->offsetFromRenderer());
        }
    } else if (m_childTransformLayer) {
        const IntRect borderBox = toRenderBox(m_owningLayer.renderer())->pixelSnappedBorderBoxRect();
        m_childTransformLayer->setSize(borderBox.size());
        m_childTransformLayer->setPosition(FloatPoint(contentOffsetInCompostingLayer()));
    }

    if (m_maskLayer) {
        if (m_maskLayer->size() != m_graphicsLayer->size()) {
            m_maskLayer->setSize(m_graphicsLayer->size());
            m_maskLayer->setNeedsDisplay();
        }
        m_maskLayer->setPosition(FloatPoint());
        m_maskLayer->setOffsetFromRenderer(m_graphicsLayer->offsetFromRenderer());
    }

    if (m_owningLayer.hasTransform()) {
        const LayoutRect borderBox = toRenderBox(renderer())->borderBoxRect();

        // Get layout bounds in the coords of compAncestor to match relativeCompositingBounds.
        IntRect layerBounds = pixelSnappedIntRect(toLayoutPoint(m_owningLayer.subpixelAccumulation()), borderBox.size());
        layerBounds.moveBy(delta);

        // Update properties that depend on layer dimensions
        FloatPoint3D transformOrigin = computeTransformOrigin(IntRect(IntPoint(), layerBounds.size()));
        // Compute the anchor point, which is in the center of the renderer box unless transform-origin is set.
        FloatPoint3D anchor(
            relativeCompositingBounds.width() ? (layerBounds.x() - relativeCompositingBounds.x() + transformOrigin.x()) / relativeCompositingBounds.width()  : 0.5f,
            relativeCompositingBounds.height() ? (layerBounds.y() - relativeCompositingBounds.y() + transformOrigin.y()) / relativeCompositingBounds.height() : 0.5f,
            transformOrigin.z());
        m_graphicsLayer->setAnchorPoint(anchor);
    } else {
        m_graphicsLayer->setAnchorPoint(FloatPoint3D(0.5f, 0.5f, 0));
    }

    if (m_foregroundLayer) {
        FloatSize foregroundSize = contentsSize;
        IntSize foregroundOffset = m_graphicsLayer->offsetFromRenderer();
        if (hasClippingLayer()) {
            // If we have a clipping layer (which clips descendants), then the foreground layer is a child of it,
            // so that it gets correctly sorted with children. In that case, position relative to the clipping layer.
            foregroundSize = FloatSize(clippingBox.size());
            foregroundOffset = toIntSize(clippingBox.location());
        }

        m_foregroundLayer->setPosition(FloatPoint());
        if (foregroundSize != m_foregroundLayer->size()) {
            m_foregroundLayer->setSize(foregroundSize);
            m_foregroundLayer->setNeedsDisplay();
        }
        m_foregroundLayer->setOffsetFromRenderer(foregroundOffset);
    }

    if (m_backgroundLayer) {
        FloatSize backgroundSize = contentsSize;
        if (backgroundLayerPaintsFixedRootBackground()) {
            FrameView* frameView = toRenderView(renderer())->frameView();
            backgroundSize = frameView->visibleContentRect().size();
        }
        m_backgroundLayer->setPosition(FloatPoint());
        if (backgroundSize != m_backgroundLayer->size()) {
            m_backgroundLayer->setSize(backgroundSize);
            m_backgroundLayer->setNeedsDisplay();
        }
        m_backgroundLayer->setOffsetFromRenderer(m_graphicsLayer->offsetFromRenderer());
    }

    if (m_owningLayer.reflectionInfo() && m_owningLayer.reflectionInfo()->reflectionLayer()->hasCompositedLayerMapping()) {
        CompositedLayerMappingPtr reflectionCompositedLayerMapping = m_owningLayer.reflectionInfo()->reflectionLayer()->compositedLayerMapping();
        reflectionCompositedLayerMapping->updateGraphicsLayerGeometry(GraphicsLayerUpdater::ForceUpdate);

        // The reflection layer has the bounds of m_owningLayer.reflectionLayer(),
        // but the reflected layer is the bounds of this layer, so we need to position it appropriately.
        FloatRect layerBounds = compositedBounds();
        FloatRect reflectionLayerBounds = reflectionCompositedLayerMapping->compositedBounds();
        reflectionCompositedLayerMapping->mainGraphicsLayer()->setReplicatedLayerPosition(FloatPoint(layerBounds.location() - reflectionLayerBounds.location()));
    }

    if (m_scrollingLayer) {
        ASSERT(m_scrollingContentsLayer);
        RenderBox* renderBox = toRenderBox(renderer());
        IntRect clientBox = enclosingIntRect(renderBox->clientBoxRect());

        IntSize adjustedScrollOffset = m_owningLayer.scrollableArea()->adjustedScrollOffset();
        m_scrollingLayer->setPosition(FloatPoint(clientBox.location() - localCompositingBounds.location() + roundedIntSize(m_owningLayer.subpixelAccumulation())));
        m_scrollingLayer->setSize(clientBox.size());

        IntSize oldScrollingLayerOffset = m_scrollingLayer->offsetFromRenderer();
        m_scrollingLayer->setOffsetFromRenderer(-toIntSize(clientBox.location()));

        if (m_childClippingMaskLayer) {
            m_childClippingMaskLayer->setPosition(m_scrollingLayer->position());
            m_childClippingMaskLayer->setSize(m_scrollingLayer->size());
            m_childClippingMaskLayer->setOffsetFromRenderer(toIntSize(clientBox.location()));
        }

        bool clientBoxOffsetChanged = oldScrollingLayerOffset != m_scrollingLayer->offsetFromRenderer();

        IntSize scrollSize(renderBox->scrollWidth(), renderBox->scrollHeight());
        if (scrollSize != m_scrollingContentsLayer->size() || clientBoxOffsetChanged)
            m_scrollingContentsLayer->setNeedsDisplay();

        IntSize scrollingContentsOffset = toIntSize(clientBox.location() - adjustedScrollOffset);
        if (scrollingContentsOffset != m_scrollingContentsLayer->offsetFromRenderer() || scrollSize != m_scrollingContentsLayer->size()) {
            bool coordinatorHandlesOffset = compositor()->scrollingLayerDidChange(&m_owningLayer);
            m_scrollingContentsLayer->setPosition(coordinatorHandlesOffset ? FloatPoint() : FloatPoint(-adjustedScrollOffset));
        }

        m_scrollingContentsLayer->setSize(scrollSize);
        // FIXME: The paint offset and the scroll offset should really be separate concepts.
        m_scrollingContentsLayer->setOffsetFromRenderer(scrollingContentsOffset, GraphicsLayer::DontSetNeedsDisplay);

        if (m_foregroundLayer) {
            if (m_foregroundLayer->size() != m_scrollingContentsLayer->size())
                m_foregroundLayer->setSize(m_scrollingContentsLayer->size());
            m_foregroundLayer->setNeedsDisplay();
            m_foregroundLayer->setOffsetFromRenderer(m_scrollingContentsLayer->offsetFromRenderer());
        }
    }

    {
        IntPoint squashingDelta(delta);
        squashingDelta.moveBy(-graphicsLayerParentLocation);
        updateSquashingLayerGeometry(squashingDelta);
    }

    if (m_owningLayer.scrollableArea() && m_owningLayer.scrollableArea()->scrollsOverflow())
        m_owningLayer.scrollableArea()->positionOverflowControls();

    // We can't make this call in RenderLayerCompositor::allocateOrClearCompositedLayerMapping
    // since it depends on whether compAncestor draws content, which gets updated later.
    updateRequiresOwnBackingStoreForAncestorReasons(compAncestor);

    if (RuntimeEnabledFeatures::cssCompositingEnabled()) {
        updateLayerBlendMode(style);
        updateIsRootForIsolatedGroup();
    }

    updateHasGpuRasterizationHint(renderer()->style());
    updateContentsRect();
    updateBackgroundColor();
    updateDrawsContent();
    updateContentsOpaque();
    updateAfterWidgetResize();
    updateRenderingContext();
    updateShouldFlattenTransform();
    updateChildrenTransform();
    updateScrollParent(compositor()->acceleratedCompositingForOverflowScrollEnabled() ? m_owningLayer.scrollParent() : 0);
    registerScrollingLayers();

    updateCompositingReasons();
}

void CompositedLayerMapping::registerScrollingLayers()
{
    // Register fixed position layers and their containers with the scrolling coordinator.
    ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer);
    if (!scrollingCoordinator)
        return;

    compositor()->updateViewportConstraintStatus(&m_owningLayer);

    scrollingCoordinator->updateLayerPositionConstraint(&m_owningLayer);

    // Page scale is applied as a transform on the root render view layer. Because the scroll
    // layer is further up in the hierarchy, we need to avoid marking the root render view
    // layer as a container.
    bool isContainer = m_owningLayer.hasTransform() && !m_owningLayer.isRootLayer();
    // FIXME: we should make certain that childForSuperLayers will never be the m_squashingContainmentLayer here
    scrollingCoordinator->setLayerIsContainerForFixedPositionLayers(localRootForOwningLayer(), isContainer);
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
    if (m_layerForHorizontalScrollbar)
        m_graphicsLayer->addChild(m_layerForHorizontalScrollbar.get());
    if (m_layerForVerticalScrollbar)
        m_graphicsLayer->addChild(m_layerForVerticalScrollbar.get());
    if (m_layerForScrollCorner)
        m_graphicsLayer->addChild(m_layerForScrollCorner.get());

    // The squashing containment layer, if it exists, becomes a no-op parent.
    if (m_squashingLayer) {
        ASSERT(compositor()->layerSquashingEnabled());
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

void CompositedLayerMapping::updateContentsRect()
{
    m_graphicsLayer->setContentsRect(pixelSnappedIntRect(contentsBox()));
}

void CompositedLayerMapping::updateDrawsContent()
{
    if (m_scrollingLayer) {
        // We don't have to consider overflow controls, because we know that the scrollbars are drawn elsewhere.
        // m_graphicsLayer only needs backing store if the non-scrolling parts (background, outlines, borders, shadows etc) need to paint.
        // m_scrollingLayer never has backing store.
        // m_scrollingContentsLayer only needs backing store if the scrolled contents need to paint.
        bool hasNonScrollingPaintedContent = m_owningLayer.hasVisibleContent() && m_owningLayer.hasBoxDecorationsOrBackground();
        m_graphicsLayer->setDrawsContent(hasNonScrollingPaintedContent);

        bool hasScrollingPaintedContent = m_owningLayer.hasVisibleContent() && (renderer()->hasBackground() || paintsChildren());
        m_scrollingContentsLayer->setDrawsContent(hasScrollingPaintedContent);
        return;
    }

    bool hasPaintedContent = containsPaintedContent();
    if (hasPaintedContent && isAcceleratedCanvas(renderer())) {
        CanvasRenderingContext* context = toHTMLCanvasElement(renderer()->node())->renderingContext();
        // Content layer may be null if context is lost.
        if (blink::WebLayer* contentLayer = context->platformLayer()) {
            Color bgColor(Color::transparent);
            if (contentLayerSupportsDirectBackgroundComposition(renderer())) {
                bgColor = rendererBackgroundColor();
                hasPaintedContent = false;
            }
            contentLayer->setBackgroundColor(bgColor.rgb());
        }
    }

    // FIXME: we could refine this to only allocate backings for one of these layers if possible.
    m_graphicsLayer->setDrawsContent(hasPaintedContent);
    if (m_foregroundLayer)
        m_foregroundLayer->setDrawsContent(hasPaintedContent);

    if (m_backgroundLayer)
        m_backgroundLayer->setDrawsContent(hasPaintedContent);
}

void CompositedLayerMapping::updateChildrenTransform()
{
    if (GraphicsLayer* childTransformLayer = layerForChildrenTransform()) {
        childTransformLayer->setTransform(owningLayer().perspectiveTransform());
        bool hasPerspective = false;
        if (RenderStyle* style = m_owningLayer.renderer()->style())
            hasPerspective = style->hasPerspective();
        if (hasPerspective)
            childTransformLayer->setShouldFlattenTransform(false);

        // Note, if the target is the scrolling layer, we need to ensure that the
        // scrolling content layer doesn't flatten the transform. (It would be nice
        // if we could apply transform to the scrolling content layer, but that's
        // too late, we need the children transform to be applied _before_ the
        // scrolling offset.)
        if (childTransformLayer == m_scrollingLayer.get())
            m_scrollingContentsLayer->setShouldFlattenTransform(false);
    }
}

// Return true if the layers changed.
bool CompositedLayerMapping::updateClippingLayers(bool needsAncestorClip, bool needsDescendantClip)
{
    bool layersChanged = false;

    if (needsAncestorClip) {
        if (!m_ancestorClippingLayer) {
            m_ancestorClippingLayer = createGraphicsLayer(CompositingReasonLayerForAncestorClip);
            m_ancestorClippingLayer->setMasksToBounds(true);
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
        if (!m_childContainmentLayer && !m_isMainFrameRenderViewLayer) {
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
            m_childTransformLayer->setShouldFlattenTransform(false);
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

bool CompositedLayerMapping::updateOverflowControlsLayers(bool needsHorizontalScrollbarLayer, bool needsVerticalScrollbarLayer, bool needsScrollCornerLayer)
{
    bool horizontalScrollbarLayerChanged = toggleScrollbarLayerIfNeeded(m_layerForHorizontalScrollbar, needsHorizontalScrollbarLayer, CompositingReasonLayerForHorizontalScrollbar);
    bool verticalScrollbarLayerChanged = toggleScrollbarLayerIfNeeded(m_layerForVerticalScrollbar, needsVerticalScrollbarLayer, CompositingReasonLayerForVerticalScrollbar);
    bool scrollCornerLayerChanged = toggleScrollbarLayerIfNeeded(m_layerForScrollCorner, needsScrollCornerLayer, CompositingReasonLayerForScrollCorner);

    if (ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer)) {
        if (horizontalScrollbarLayerChanged)
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_owningLayer.scrollableArea(), HorizontalScrollbar);
        if (verticalScrollbarLayerChanged)
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_owningLayer.scrollableArea(), VerticalScrollbar);
    }

    return horizontalScrollbarLayerChanged || verticalScrollbarLayerChanged || scrollCornerLayerChanged;
}

void CompositedLayerMapping::positionOverflowControlsLayers(const IntSize& offsetFromRoot)
{
    IntSize offsetFromRenderer = m_graphicsLayer->offsetFromRenderer() - roundedIntSize(m_owningLayer.subpixelAccumulation());
    if (GraphicsLayer* layer = layerForHorizontalScrollbar()) {
        Scrollbar* hBar = m_owningLayer.scrollableArea()->horizontalScrollbar();
        if (hBar) {
            layer->setPosition(hBar->frameRect().location() - offsetFromRoot - offsetFromRenderer);
            layer->setSize(hBar->frameRect().size());
            if (layer->hasContentsLayer())
                layer->setContentsRect(IntRect(IntPoint(), hBar->frameRect().size()));
        }
        layer->setDrawsContent(hBar && !layer->hasContentsLayer());
    }

    if (GraphicsLayer* layer = layerForVerticalScrollbar()) {
        Scrollbar* vBar = m_owningLayer.scrollableArea()->verticalScrollbar();
        if (vBar) {
            layer->setPosition(vBar->frameRect().location() - offsetFromRoot - offsetFromRenderer);
            layer->setSize(vBar->frameRect().size());
            if (layer->hasContentsLayer())
                layer->setContentsRect(IntRect(IntPoint(), vBar->frameRect().size()));
        }
        layer->setDrawsContent(vBar && !layer->hasContentsLayer());
    }

    if (GraphicsLayer* layer = layerForScrollCorner()) {
        const LayoutRect& scrollCornerAndResizer = m_owningLayer.scrollableArea()->scrollCornerAndResizerRect();
        layer->setPosition(scrollCornerAndResizer.location() - offsetFromRenderer);
        layer->setSize(scrollCornerAndResizer.size());
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
    ApplyToCoreLayers = (1 << 0),
    ApplyToSquashingLayer = (1 << 1),
    ApplyToScrollbarLayers = (1 << 2),
    ApplyToBackgroundLayer = (1 << 3),
    ApplyToMaskLayers = (1 << 4),
    ApplyToContentLayers = (1 << 5),
    ApplyToAllGraphicsLayers = (ApplyToSquashingLayer | ApplyToScrollbarLayers | ApplyToBackgroundLayer | ApplyToMaskLayers | ApplyToCoreLayers | ApplyToContentLayers)
};
typedef unsigned ApplyToGraphicsLayersMode;

template <typename Func>
static void ApplyToGraphicsLayers(const CompositedLayerMapping* mapping, const Func& f, ApplyToGraphicsLayersMode mode)
{
    ASSERT(mode);

    if ((mode & ApplyToCoreLayers) && mapping->squashingContainmentLayer())
        f(mapping->squashingContainmentLayer());
    if ((mode & ApplyToCoreLayers) && mapping->childTransformLayer())
        f(mapping->childTransformLayer());
    if ((mode & ApplyToCoreLayers) && mapping->ancestorClippingLayer())
        f(mapping->ancestorClippingLayer());
    if (((mode & ApplyToCoreLayers) || (mode & ApplyToContentLayers)) && mapping->mainGraphicsLayer())
        f(mapping->mainGraphicsLayer());
    if ((mode & ApplyToCoreLayers) && mapping->clippingLayer())
        f(mapping->clippingLayer());
    if ((mode & ApplyToCoreLayers) && mapping->scrollingLayer())
        f(mapping->scrollingLayer());
    if (((mode & ApplyToCoreLayers) || (mode & ApplyToContentLayers)) && mapping->scrollingContentsLayer())
        f(mapping->scrollingContentsLayer());
    if (((mode & ApplyToCoreLayers) || (mode & ApplyToContentLayers)) && mapping->foregroundLayer())
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
    if (RenderLayer* root = m_owningLayer.renderingContextRoot()) {
        if (Node* node = root->renderer()->node())
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
    ApplyToGraphicsLayersMode mode = ApplyToCoreLayers;
    ApplyToGraphicsLayers(this, functor, mode);
}

bool CompositedLayerMapping::updateForegroundLayer(bool needsForegroundLayer)
{
    bool layerChanged = false;
    if (needsForegroundLayer) {
        if (!m_foregroundLayer) {
            m_foregroundLayer = createGraphicsLayer(CompositingReasonLayerForForeground);
            m_foregroundLayer->setDrawsContent(true);
            m_foregroundLayer->setPaintingPhase(GraphicsLayerPaintForeground);
            // If the foreground layer pops content out of the graphics
            // layer, then the graphics layer needs to be repainted.
            // FIXME: This is conservative, as m_foregroundLayer could
            // be smaller than m_graphicsLayer due to clipping.
            m_graphicsLayer->setNeedsDisplay();
            layerChanged = true;
        }
    } else if (m_foregroundLayer) {
        FloatRect repaintRect(FloatPoint(), m_foregroundLayer->size());
        m_graphicsLayer->setNeedsDisplayInRect(repaintRect);
        m_foregroundLayer->removeFromParent();
        m_foregroundLayer = nullptr;
        layerChanged = true;
    }

    if (layerChanged)
        m_graphicsLayer->setPaintingPhase(paintingPhaseForPrimaryLayer());

    return layerChanged;
}

bool CompositedLayerMapping::updateBackgroundLayer(bool needsBackgroundLayer)
{
    bool layerChanged = false;
    if (needsBackgroundLayer) {
        if (!m_backgroundLayer) {
            m_backgroundLayer = createGraphicsLayer(CompositingReasonLayerForBackground);
            m_backgroundLayer->setDrawsContent(true);
            m_backgroundLayer->setAnchorPoint(FloatPoint3D());
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

    if (layerChanged && !m_owningLayer.renderer()->documentBeingDestroyed())
        compositor()->rootFixedBackgroundsChanged();

    return layerChanged;
}

bool CompositedLayerMapping::updateMaskLayer(bool needsMaskLayer)
{
    bool layerChanged = false;
    if (needsMaskLayer) {
        if (!m_maskLayer) {
            m_maskLayer = createGraphicsLayer(CompositingReasonLayerForMask);
            m_maskLayer->setDrawsContent(true);
            m_maskLayer->setPaintingPhase(GraphicsLayerPaintMask);
            layerChanged = true;
        }
    } else if (m_maskLayer) {
        m_maskLayer = nullptr;
        layerChanged = true;
    }

    if (layerChanged)
        m_graphicsLayer->setPaintingPhase(paintingPhaseForPrimaryLayer());

    return layerChanged;
}

bool CompositedLayerMapping::updateClippingMaskLayers(bool needsChildClippingMaskLayer)
{
    bool layerChanged = false;
    if (needsChildClippingMaskLayer) {
        if (!m_childClippingMaskLayer) {
            m_childClippingMaskLayer = createGraphicsLayer(CompositingReasonLayerForClippingMask);
            m_childClippingMaskLayer->setDrawsContent(true);
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
            m_scrollingContentsLayer->setDrawsContent(true);
            GraphicsLayerPaintingPhase paintPhase = GraphicsLayerPaintOverflowContents | GraphicsLayerPaintCompositedScroll;
            if (!m_foregroundLayer)
                paintPhase |= GraphicsLayerPaintForeground;
            m_scrollingContentsLayer->setPaintingPhase(paintPhase);
            m_scrollingLayer->addChild(m_scrollingContentsLayer.get());

            layerChanged = true;
            if (scrollingCoordinator)
                scrollingCoordinator->scrollableAreaScrollLayerDidChange(m_owningLayer.scrollableArea());
        }
    } else if (m_scrollingLayer) {
        m_scrollingLayer = nullptr;
        m_scrollingContentsLayer = nullptr;
        layerChanged = true;
        if (scrollingCoordinator)
            scrollingCoordinator->scrollableAreaScrollLayerDidChange(m_owningLayer.scrollableArea());
    }

    if (layerChanged) {
        updateInternalHierarchy();
        m_graphicsLayer->setPaintingPhase(paintingPhaseForPrimaryLayer());
        m_graphicsLayer->setNeedsDisplay();
        if (renderer()->view())
            compositor()->scrollingLayerDidChange(&m_owningLayer);
    }

    return layerChanged;
}

static void updateScrollParentForGraphicsLayer(GraphicsLayer* layer, GraphicsLayer* topmostLayer, RenderLayer* scrollParent, ScrollingCoordinator* scrollingCoordinator)
{
    if (!layer)
        return;

    // Only the topmost layer has a scroll parent. All other layers have a null scroll parent.
    if (layer != topmostLayer)
        scrollParent = 0;

    scrollingCoordinator->updateScrollParentForGraphicsLayer(layer, scrollParent);
}

void CompositedLayerMapping::updateScrollParent(RenderLayer* scrollParent)
{
    if (!scrollParent && m_squashedLayers.size() && compositor()->acceleratedCompositingForOverflowScrollEnabled())
        scrollParent = m_squashedLayers[0].renderLayer->scrollParent();

    if (ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer)) {
        GraphicsLayer* topmostLayer = childForSuperlayers();
        updateScrollParentForGraphicsLayer(m_squashingContainmentLayer.get(), topmostLayer, scrollParent, scrollingCoordinator);
        updateScrollParentForGraphicsLayer(m_ancestorClippingLayer.get(), topmostLayer, scrollParent, scrollingCoordinator);
        updateScrollParentForGraphicsLayer(m_graphicsLayer.get(), topmostLayer, scrollParent, scrollingCoordinator);
    }
}

void CompositedLayerMapping::updateClipParent(RenderLayer* clipParent)
{
    if (ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer))
        scrollingCoordinator->updateClipParentForGraphicsLayer(m_graphicsLayer.get(), clipParent);
}

bool CompositedLayerMapping::updateSquashingLayers(bool needsSquashingLayers)
{
    bool layersChanged = false;

    if (needsSquashingLayers) {
        ASSERT(compositor()->layerSquashingEnabled());

        if (!m_squashingLayer) {
            ASSERT(!m_squashingContainmentLayer);

            m_squashingLayer = createGraphicsLayer(CompositingReasonLayerForSquashingContents);
            m_squashingLayer->setDrawsContent(true);

            // FIXME: containment layer needs a new CompositingReason, CompositingReasonOverlap is not appropriate.
            if (!m_ancestorClippingLayer)
                m_squashingContainmentLayer = createGraphicsLayer(CompositingReasonLayerForSquashingContainer);
            layersChanged = true;
        }

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

    if (m_owningLayer.compositingReasons() & CompositingReasonOverflowScrollingParent)
        phase |= GraphicsLayerPaintCompositedScroll;

    return static_cast<GraphicsLayerPaintingPhase>(phase);
}

float CompositedLayerMapping::compositingOpacity(float rendererOpacity) const
{
    float finalOpacity = rendererOpacity;

    for (RenderLayer* curr = m_owningLayer.parent(); curr; curr = curr->parent()) {
        // We only care about parents that are stacking contexts.
        // Recall that opacity creates stacking context.
        if (!curr->stackingNode()->isStackingContainer())
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
        if (curr->compositingState() == PaintsIntoOwnBacking || curr->compositingState() == HasOwnBackingButPaintsIntoAncestor)
            break;

        finalOpacity *= curr->renderer()->opacity();
    }

    return finalOpacity;
}

Color CompositedLayerMapping::rendererBackgroundColor() const
{
    RenderObject* backgroundRenderer = renderer();
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

    if (hasVisibleNonCompositingDescendantLayers())
        return true;

    return false;
}

static bool isCompositedPlugin(RenderObject* renderer)
{
    return renderer->isEmbeddedObject() && toRenderEmbeddedObject(renderer)->requiresAcceleratedCompositing();
}

static bool hasVisibleNonCompositingDescendant(RenderLayer* parent)
{
    if (!parent->hasVisibleDescendant())
        return false;

    // FIXME: We shouldn't be called with a stale z-order lists. See bug 85512.
    parent->stackingNode()->updateLayerListsIfNeeded();

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(parent->stackingNode());
#endif

    RenderLayerStackingNodeIterator normalFlowIterator(*parent->stackingNode(), AllChildren);
    while (RenderLayerStackingNode* curNode = normalFlowIterator.next()) {
        RenderLayer* curLayer = curNode->layer();
        if (curLayer->hasCompositedLayerMapping())
            continue;
        if (curLayer->hasVisibleContent() || hasVisibleNonCompositingDescendant(curLayer))
            return true;
    }

    return false;
}

// FIXME: By name the implementation is correct. But the code that uses this function means something
// very slightly different - the implementation needs to also include composited descendants that
// don't paint into their own backing, and instead paint into this backing.
bool CompositedLayerMapping::hasVisibleNonCompositingDescendantLayers() const
{
    return hasVisibleNonCompositingDescendant(&m_owningLayer);
}

bool CompositedLayerMapping::containsPaintedContent() const
{
    if (paintsIntoCompositedAncestor() || m_artificiallyInflatedBounds || m_owningLayer.isReflection())
        return false;

    if (isDirectlyCompositedImage())
        return false;

    RenderObject* renderObject = renderer();
    // FIXME: we could optimize cases where the image, video or canvas is known to fill the border box entirely,
    // and set background color on the layer in that case, instead of allocating backing store and painting.
    if (renderObject->isVideo() && toRenderVideo(renderer())->shouldDisplayVideo())
        return m_owningLayer.hasBoxDecorationsOrBackground();

    if (m_owningLayer.hasVisibleBoxDecorations())
        return true;

    if (renderObject->hasMask()) // masks require special treatment
        return true;

    if (renderObject->isReplaced() && !isCompositedPlugin(renderObject))
        return true;

    if (renderObject->isRenderRegion())
        return true;

    if (renderObject->node() && renderObject->node()->isDocumentNode()) {
        // Look to see if the root object has a non-simple background
        RenderObject* rootObject = renderObject->document().documentElement() ? renderObject->document().documentElement()->renderer() : 0;
        // Reject anything that has a border, a border-radius or outline,
        // or is not a simple background (no background, or solid color).
        if (rootObject && hasBoxDecorationsOrBackgroundImage(rootObject->style()))
            return true;

        // Now look at the body's renderer.
        HTMLElement* body = renderObject->document().body();
        RenderObject* bodyObject = (body && body->hasLocalName(bodyTag)) ? body->renderer() : 0;
        if (bodyObject && hasBoxDecorationsOrBackgroundImage(bodyObject->style()))
            return true;
    }

    // FIXME: it's O(n^2). A better solution is needed.
    return paintsChildren();
}

// An image can be directly compositing if it's the sole content of the layer, and has no box decorations
// that require painting. Direct compositing saves backing store.
bool CompositedLayerMapping::isDirectlyCompositedImage() const
{
    RenderObject* renderObject = renderer();

    if (!renderObject->isImage() || m_owningLayer.hasBoxDecorationsOrBackground() || renderObject->hasClip())
        return false;

    RenderImage* imageRenderer = toRenderImage(renderObject);
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
    if ((changeType == ImageChanged) && isDirectlyCompositedImage()) {
        updateImageContents();
        return;
    }

    if ((changeType == CanvasChanged || changeType == CanvasPixelsChanged) && isAcceleratedCanvas(renderer())) {
        m_graphicsLayer->setContentsNeedsDisplay();
        return;
    }
}

void CompositedLayerMapping::updateImageContents()
{
    ASSERT(renderer()->isImage());
    RenderImage* imageRenderer = toRenderImage(renderer());

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
    updateDrawsContent();

    // Image animation is "lazy", in that it automatically stops unless someone is drawing
    // the image. So we have to kick the animation each time; this has the downside that the
    // image will keep animating, even if its layer is not visible.
    image->startAnimation();
}

FloatPoint3D CompositedLayerMapping::computeTransformOrigin(const IntRect& borderBox) const
{
    RenderStyle* style = renderer()->style();

    FloatPoint3D origin;
    origin.setX(floatValueForLength(style->transformOriginX(), borderBox.width()));
    origin.setY(floatValueForLength(style->transformOriginY(), borderBox.height()));
    origin.setZ(style->transformOriginZ());

    return origin;
}

FloatPoint CompositedLayerMapping::computePerspectiveOrigin(const IntRect& borderBox) const
{
    RenderStyle* style = renderer()->style();

    float boxWidth = borderBox.width();
    float boxHeight = borderBox.height();

    FloatPoint origin;
    origin.setX(floatValueForLength(style->perspectiveOriginX(), boxWidth));
    origin.setY(floatValueForLength(style->perspectiveOriginY(), boxHeight));

    return origin;
}

// Return the offset from the top-left of this compositing layer at which the renderer's contents are painted.
LayoutSize CompositedLayerMapping::contentOffsetInCompostingLayer() const
{
    return LayoutSize(-m_compositedBounds.x(), -m_compositedBounds.y());
}

LayoutRect CompositedLayerMapping::contentsBox() const
{
    LayoutRect contentsBox = contentsRect(renderer());
    contentsBox.move(contentOffsetInCompostingLayer());
    return contentsBox;
}

GraphicsLayer* CompositedLayerMapping::parentForSublayers() const
{
    if (m_scrollingContentsLayer)
        return m_scrollingContentsLayer.get();

    if (m_childContainmentLayer)
        return m_childContainmentLayer.get();

    if (m_childTransformLayer)
        return m_childTransformLayer.get();

    return m_graphicsLayer.get();
}

GraphicsLayer* CompositedLayerMapping::localRootForOwningLayer() const
{
    if (m_ancestorClippingLayer)
        return m_ancestorClippingLayer.get();

    return m_graphicsLayer.get();
}

GraphicsLayer* CompositedLayerMapping::childForSuperlayers() const
{
    if (m_squashingContainmentLayer)
        return m_squashingContainmentLayer.get();

    return localRootForOwningLayer();
}

GraphicsLayer* CompositedLayerMapping::layerForChildrenTransform() const
{
    if (GraphicsLayer* clipLayer = clippingLayer())
        return clipLayer;
    if (m_scrollingLayer)
        return m_scrollingLayer.get();
    return m_childTransformLayer.get();
}

bool CompositedLayerMapping::updateRequiresOwnBackingStoreForAncestorReasons(const RenderLayer* compositingAncestorLayer)
{
    bool previousRequiresOwnBackingStoreForAncestorReasons = m_requiresOwnBackingStoreForAncestorReasons;
    bool previousPaintsIntoCompositedAncestor = paintsIntoCompositedAncestor();
    bool canPaintIntoAncestor = compositingAncestorLayer
        && (compositingAncestorLayer->compositedLayerMapping()->mainGraphicsLayer()->drawsContent()
            || compositingAncestorLayer->compositedLayerMapping()->paintsIntoCompositedAncestor());
    m_requiresOwnBackingStoreForAncestorReasons = !canPaintIntoAncestor;

    if (paintsIntoCompositedAncestor() != previousPaintsIntoCompositedAncestor)
        paintsIntoCompositedAncestorChanged();
    return m_requiresOwnBackingStoreForAncestorReasons != previousRequiresOwnBackingStoreForAncestorReasons;
}

bool CompositedLayerMapping::updateRequiresOwnBackingStoreForIntrinsicReasons()
{
    bool previousRequiresOwnBackingStoreForIntrinsicReasons = m_requiresOwnBackingStoreForIntrinsicReasons;
    bool previousPaintsIntoCompositedAncestor = paintsIntoCompositedAncestor();
    RenderObject* renderer = m_owningLayer.renderer();
    m_requiresOwnBackingStoreForIntrinsicReasons = m_owningLayer.isRootLayer()
        || (m_owningLayer.compositingReasons() & CompositingReasonComboReasonsThatRequireOwnBacking)
        || m_owningLayer.transform()
        || m_owningLayer.clipsCompositingDescendantsWithBorderRadius() // FIXME: Revisit this if the paintsIntoCompositedAncestor state is removed.
        || renderer->isTransparent()
        || renderer->hasMask()
        || renderer->hasReflection()
        || renderer->hasFilter();

    if (paintsIntoCompositedAncestor() != previousPaintsIntoCompositedAncestor)
        paintsIntoCompositedAncestorChanged();
    return m_requiresOwnBackingStoreForIntrinsicReasons != previousRequiresOwnBackingStoreForIntrinsicReasons;
}

void CompositedLayerMapping::paintsIntoCompositedAncestorChanged()
{
    // The answer to paintsIntoCompositedAncestor() affects cached clip rects, so when
    // it changes we have to clear clip rects on descendants.
    m_owningLayer.clipper().clearClipRectsIncludingDescendants(PaintingClipRects);
    m_owningLayer.repainter().computeRepaintRectsIncludingDescendants();

    compositor()->repaintInCompositedAncestor(&m_owningLayer, compositedBounds());
}

void CompositedLayerMapping::setBlendMode(blink::WebBlendMode blendMode)
{
    if (m_ancestorClippingLayer) {
        m_ancestorClippingLayer->setBlendMode(blendMode);
        m_graphicsLayer->setBlendMode(blink::WebBlendModeNormal);
    } else {
        m_graphicsLayer->setBlendMode(blendMode);
    }
}

void CompositedLayerMapping::setNeedsGraphicsLayerUpdate()
{
    m_needToUpdateGraphicsLayerOfAllDecendants = true;

    for (RenderLayer* current = &m_owningLayer; current; current = current->ancestorCompositingLayer()) {
        ASSERT(current->hasCompositedLayerMapping());
        CompositedLayerMappingPtr mapping = current->compositedLayerMapping();
        if (mapping->m_needToUpdateGraphicsLayer)
            return;
        mapping->m_needToUpdateGraphicsLayer = true;
    }
}

GraphicsLayerUpdater::UpdateType CompositedLayerMapping::updateTypeForChildren(GraphicsLayerUpdater::UpdateType updateType) const
{
    if (m_needToUpdateGraphicsLayerOfAllDecendants)
        return GraphicsLayerUpdater::ForceUpdate;
    return updateType;
}

void CompositedLayerMapping::clearNeedsGraphicsLayerUpdate()
{
    m_needToUpdateGraphicsLayer = false;
    m_needToUpdateGraphicsLayerOfAllDecendants = false;
}

#if !ASSERT_DISABLED

void CompositedLayerMapping::assertNeedsToUpdateGraphicsLayerBitsCleared()
{
    ASSERT(!m_needToUpdateGraphicsLayer);
    ASSERT(!m_needToUpdateGraphicsLayerOfAllDecendants);
}

#endif

struct SetContentsNeedsDisplayFunctor {
    void operator() (GraphicsLayer* layer) const
    {
        if (layer->drawsContent())
            layer->setNeedsDisplay();
    }
};

void CompositedLayerMapping::setContentsNeedDisplay()
{
    // FIXME: need to split out repaints for the background.
    ASSERT(!paintsIntoCompositedAncestor());
    ApplyToGraphicsLayers(this, SetContentsNeedsDisplayFunctor(), ApplyToContentLayers);
}

struct SetContentsNeedsDisplayInRectFunctor {
    void operator() (GraphicsLayer* layer) const
    {
        if (layer->drawsContent()) {
            IntRect layerDirtyRect = r;
            layerDirtyRect.move(-layer->offsetFromRenderer());
            layer->setNeedsDisplayInRect(layerDirtyRect);
        }
    }

    IntRect r;
};

// r is in the coordinate space of the layer's render object
void CompositedLayerMapping::setContentsNeedDisplayInRect(const IntRect& r)
{
    // FIXME: need to split out repaints for the background.
    ASSERT(!paintsIntoCompositedAncestor());
    SetContentsNeedsDisplayInRectFunctor functor = { r };
    ApplyToGraphicsLayers(this, functor, ApplyToContentLayers);
}

void CompositedLayerMapping::doPaintTask(GraphicsLayerPaintInfo& paintInfo, GraphicsContext* context,
    const IntRect& clip) // In the coords of rootLayer.
{
    if (paintsIntoCompositedAncestor()) {
        ASSERT_NOT_REACHED();
        return;
    }

    FontCachePurgePreventer fontCachePurgePreventer;

    PaintLayerFlags paintFlags = 0;
    if (paintInfo.paintingPhase & GraphicsLayerPaintBackground)
        paintFlags |= PaintLayerPaintingCompositingBackgroundPhase;
    if (paintInfo.paintingPhase & GraphicsLayerPaintForeground)
        paintFlags |= PaintLayerPaintingCompositingForegroundPhase;
    if (paintInfo.paintingPhase & GraphicsLayerPaintMask)
        paintFlags |= PaintLayerPaintingCompositingMaskPhase;
    if (paintInfo.paintingPhase & GraphicsLayerPaintChildClippingMask)
        paintFlags |= PaintLayerPaintingChildClippingMaskPhase;
    if (paintInfo.paintingPhase & GraphicsLayerPaintOverflowContents)
        paintFlags |= PaintLayerPaintingOverflowContents;
    if (paintInfo.paintingPhase & GraphicsLayerPaintCompositedScroll)
        paintFlags |= PaintLayerPaintingCompositingScrollingPhase;

    if (paintInfo.isBackgroundLayer)
        paintFlags |= (PaintLayerPaintingRootBackgroundOnly | PaintLayerPaintingCompositingForegroundPhase); // Need PaintLayerPaintingCompositingForegroundPhase to walk child layers.
    else if (compositor()->fixedRootBackgroundLayer())
        paintFlags |= PaintLayerPaintingSkipRootBackground;

    // Note carefully: in theory it is appropriate to invoke context->save() here
    // and restore the context after painting. For efficiency, we are assuming that
    // it is equivalent to manually undo this offset translation, which means we are
    // assuming that the context's space was not affected by the RenderLayer
    // painting code.

    IntSize offset = paintInfo.offsetFromRenderer;
    context->translate(-offset);

    // The dirtyRect is in the coords of the painting root.
    IntRect dirtyRect(clip);
    dirtyRect.move(offset);

    if (!(paintInfo.paintingPhase & GraphicsLayerPaintOverflowContents)) {
        LayoutRect bounds = paintInfo.compositedBounds;
        bounds.move(paintInfo.renderLayer->subpixelAccumulation());
        dirtyRect.intersect(pixelSnappedIntRect(bounds));
    } else {
        dirtyRect.move(roundedIntSize(paintInfo.renderLayer->subpixelAccumulation()));
    }

#ifndef NDEBUG
    paintInfo.renderLayer->renderer()->assertSubtreeIsLaidOut();
#endif

    if (paintInfo.renderLayer->compositingState() != PaintsIntoGroupedBacking) {
        // FIXME: GraphicsLayers need a way to split for RenderRegions.
        LayerPaintingInfo paintingInfo(paintInfo.renderLayer, dirtyRect, PaintBehaviorNormal, paintInfo.renderLayer->subpixelAccumulation());
        paintInfo.renderLayer->paintLayerContents(context, paintingInfo, paintFlags);

        ASSERT(!paintInfo.isBackgroundLayer || paintFlags & PaintLayerPaintingRootBackgroundOnly);

        if (paintInfo.renderLayer->containsDirtyOverlayScrollbars())
            paintInfo.renderLayer->paintLayerContents(context, paintingInfo, paintFlags | PaintLayerPaintingOverlayScrollbars);
    } else {
        ASSERT(compositor()->layerSquashingEnabled());
        LayerPaintingInfo paintingInfo(paintInfo.renderLayer, dirtyRect, PaintBehaviorNormal, paintInfo.renderLayer->subpixelAccumulation());
        paintInfo.renderLayer->paintLayer(context, paintingInfo, paintFlags);
    }

    ASSERT(!paintInfo.renderLayer->usedTransparency());

    // Manually restore the context to its original state by applying the opposite translation.
    context->translate(offset);
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

// Up-call from compositing layer drawing callback.
void CompositedLayerMapping::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& context, GraphicsLayerPaintingPhase paintingPhase, const IntRect& clip)
{
    // https://code.google.com/p/chromium/issues/detail?id=343772
    DisableCompositingQueryAsserts disabler;
#ifndef NDEBUG
    // FIXME: once the state machine is ready, this can be removed and we can refer to that instead.
    if (Page* page = renderer()->frame()->page())
        page->setIsPainting(true);
#endif
    InspectorInstrumentation::willPaint(m_owningLayer.renderer(), graphicsLayer);

    if (graphicsLayer == m_graphicsLayer.get()
        || graphicsLayer == m_foregroundLayer.get()
        || graphicsLayer == m_backgroundLayer.get()
        || graphicsLayer == m_maskLayer.get()
        || graphicsLayer == m_childClippingMaskLayer.get()
        || graphicsLayer == m_scrollingContentsLayer.get()) {

        GraphicsLayerPaintInfo paintInfo;
        paintInfo.renderLayer = &m_owningLayer;
        paintInfo.compositedBounds = compositedBounds();
        paintInfo.offsetFromRenderer = graphicsLayer->offsetFromRenderer();
        paintInfo.paintingPhase = paintingPhase;
        paintInfo.isBackgroundLayer = (graphicsLayer == m_backgroundLayer);

        // We have to use the same root as for hit testing, because both methods can compute and cache clipRects.
        doPaintTask(paintInfo, &context, clip);
    } else if (graphicsLayer == m_squashingLayer.get()) {
        ASSERT(compositor()->layerSquashingEnabled());
        for (size_t i = 0; i < m_squashedLayers.size(); ++i)
            doPaintTask(m_squashedLayers[i], &context, clip);
    } else if (graphicsLayer == layerForHorizontalScrollbar()) {
        paintScrollbar(m_owningLayer.scrollableArea()->horizontalScrollbar(), context, clip);
    } else if (graphicsLayer == layerForVerticalScrollbar()) {
        paintScrollbar(m_owningLayer.scrollableArea()->verticalScrollbar(), context, clip);
    } else if (graphicsLayer == layerForScrollCorner()) {
        const IntRect& scrollCornerAndResizer = m_owningLayer.scrollableArea()->scrollCornerAndResizerRect();
        context.save();
        context.translate(-scrollCornerAndResizer.x(), -scrollCornerAndResizer.y());
        IntRect transformedClip = clip;
        transformedClip.moveBy(scrollCornerAndResizer.location());
        m_owningLayer.scrollableArea()->paintScrollCorner(&context, IntPoint(), transformedClip);
        m_owningLayer.scrollableArea()->paintResizer(&context, IntPoint(), transformedClip);
        context.restore();
    }
    InspectorInstrumentation::didPaint(m_owningLayer.renderer(), graphicsLayer, &context, clip);
#ifndef NDEBUG
    if (Page* page = renderer()->frame()->page())
        page->setIsPainting(false);
#endif
}

bool CompositedLayerMapping::isTrackingRepaints() const
{
    GraphicsLayerClient* client = compositor();
    return client ? client->isTrackingRepaints() : false;
}

struct CollectTrackedRepaintRectsFunctor {
    void operator() (GraphicsLayer* layer) const { layer->collectTrackedRepaintRects(*rects); }
    Vector<FloatRect>* rects;
};

PassOwnPtr<Vector<FloatRect> > CompositedLayerMapping::collectTrackedRepaintRects() const
{
    OwnPtr<Vector<FloatRect> > rects = adoptPtr(new Vector<FloatRect>);
    CollectTrackedRepaintRectsFunctor functor = { rects.get() };
    ApplyToGraphicsLayers(this, functor, ApplyToAllGraphicsLayers);
    return rects.release();
}

#ifndef NDEBUG
void CompositedLayerMapping::verifyNotPainting()
{
    ASSERT(!renderer()->frame()->page() || !renderer()->frame()->page()->isPainting());
}
#endif

void CompositedLayerMapping::notifyAnimationStarted(const GraphicsLayer*, double monotonicTime)
{
    renderer()->node()->document().compositorPendingAnimations().notifyCompositorAnimationStarted(monotonicTime);
}

IntRect CompositedLayerMapping::pixelSnappedCompositedBounds() const
{
    LayoutRect bounds = m_compositedBounds;
    bounds.move(m_owningLayer.subpixelAccumulation());
    return pixelSnappedIntRect(bounds);
}

bool CompositedLayerMapping::updateSquashingLayerAssignment(RenderLayer* layer, LayoutSize offsetFromSquashingCLM, size_t nextSquashedLayerIndex)
{
    ASSERT(compositor()->layerSquashingEnabled());

    GraphicsLayerPaintInfo paintInfo;
    paintInfo.renderLayer = layer;
    // NOTE: composited bounds are updated elsewhere
    // NOTE: offsetFromRenderer is updated elsewhere
    paintInfo.offsetFromSquashingCLM = offsetFromSquashingCLM;
    paintInfo.paintingPhase = GraphicsLayerPaintAllWithOverflowClip;
    paintInfo.isBackgroundLayer = false;

    // Change tracking on squashing layers: at the first sign of something changed, just invalidate the layer.
    // FIXME: Perhaps we can find a tighter more clever mechanism later.
    bool updatedAssignment = false;
    if (nextSquashedLayerIndex < m_squashedLayers.size()) {
        if (!paintInfo.isEquivalentForSquashing(m_squashedLayers[nextSquashedLayerIndex])) {
            updatedAssignment = true;
        }
        m_squashedLayers[nextSquashedLayerIndex] = paintInfo;
    } else {
        m_squashedLayers.append(paintInfo);
        updatedAssignment = true;
    }
    layer->setGroupedMapping(this);
    return updatedAssignment;
}

void CompositedLayerMapping::removeRenderLayerFromSquashingGraphicsLayer(const RenderLayer* layer)
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
    ASSERT(compositor()->layerSquashingEnabled());

    // Any additional squashed RenderLayers in the array no longer exist, and removing invalidates the squashingLayer contents.
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
    } else if (graphicsLayer == m_scrollingLayer.get()) {
        name = "Scrolling Layer";
    } else if (graphicsLayer == m_scrollingContentsLayer.get()) {
        name = "Scrolling Contents Layer";
    } else {
        ASSERT_NOT_REACHED();
    }

    return name;
}

} // namespace WebCore
