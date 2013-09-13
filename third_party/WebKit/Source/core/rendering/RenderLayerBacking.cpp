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

#include "core/rendering/RenderLayerBacking.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/fetch/ImageResource.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/Chrome.h"
#include "core/page/FrameView.h"
#include "core/page/Settings.h"
#include "core/page/animation/AnimationController.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/platform/animation/KeyframeValueList.h"
#include "core/platform/graphics/FontCache.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/GraphicsLayer.h"
#include "core/plugins/PluginView.h"
#include "core/rendering/RenderApplet.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/RenderIFrame.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderVideo.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/KeyframeList.h"
#include "wtf/CurrentTime.h"
#include "wtf/text/StringBuilder.h"

#include "core/platform/graphics/filters/custom/CustomFilterOperation.h"
#include "core/rendering/FilterEffectRenderer.h"

#include "core/platform/graphics/GraphicsContext3D.h"

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

// Get the scrolling coordinator in a way that works inside RenderLayerBacking's destructor.
static ScrollingCoordinator* scrollingCoordinatorFromLayer(RenderLayer* layer)
{
    Page* page = layer->renderer()->frame()->page();
    if (!page)
        return 0;

    return page->scrollingCoordinator();
}

RenderLayerBacking::RenderLayerBacking(RenderLayer* layer)
    : m_owningLayer(layer)
    , m_artificiallyInflatedBounds(false)
    , m_boundsConstrainedByClipping(false)
    , m_isMainFrameRenderViewLayer(false)
    , m_requiresOwnBackingStore(true)
    , m_canCompositeFilters(false)
    , m_backgroundLayerPaintsFixedRootBackground(false)
{
    if (layer->isRootLayer()) {
        Frame& frame = toRenderView(renderer())->frameView()->frame();
        Page* page = frame.page();
        if (page && page->mainFrame() == &frame) {
            m_isMainFrameRenderViewLayer = true;
        }
    }

    createPrimaryGraphicsLayer();
}

RenderLayerBacking::~RenderLayerBacking()
{
    updateClippingLayers(false, false, false);
    updateOverflowControlsLayers(false, false, false);
    updateForegroundLayer(false);
    updateBackgroundLayer(false);
    updateMaskLayer(false);
    updateScrollingLayers(false);
    destroyGraphicsLayers();
}

PassOwnPtr<GraphicsLayer> RenderLayerBacking::createGraphicsLayer(CompositingReasons reasons)
{
    GraphicsLayerFactory* graphicsLayerFactory = 0;
    if (Page* page = renderer()->frame()->page())
        graphicsLayerFactory = page->chrome().client().graphicsLayerFactory();

    OwnPtr<GraphicsLayer> graphicsLayer = GraphicsLayer::create(graphicsLayerFactory, this);

    graphicsLayer->setCompositingReasons(reasons);

    return graphicsLayer.release();
}

void RenderLayerBacking::createPrimaryGraphicsLayer()
{
    m_graphicsLayer = createGraphicsLayer(m_owningLayer->compositingReasons());

#if !OS(ANDROID)
    if (m_isMainFrameRenderViewLayer)
        m_graphicsLayer->contentLayer()->setDrawCheckerboardForMissingTiles(true);
#endif

    updateOpacity(renderer()->style());
    updateTransform(renderer()->style());
    updateFilters(renderer()->style());

    if (RuntimeEnabledFeatures::cssCompositingEnabled())
        updateLayerBlendMode(renderer()->style());
}

void RenderLayerBacking::destroyGraphicsLayers()
{
    if (m_graphicsLayer)
        m_graphicsLayer->removeFromParent();

    m_ancestorClippingLayer = nullptr;
    m_graphicsLayer = nullptr;
    m_foregroundLayer = nullptr;
    m_backgroundLayer = nullptr;
    m_childContainmentLayer = nullptr;
    m_maskLayer = nullptr;

    m_scrollingLayer = nullptr;
    m_scrollingContentsLayer = nullptr;
}

void RenderLayerBacking::updateOpacity(const RenderStyle* style)
{
    m_graphicsLayer->setOpacity(compositingOpacity(style->opacity()));
}

void RenderLayerBacking::updateTransform(const RenderStyle* style)
{
    // FIXME: This could use m_owningLayer->transform(), but that currently has transform-origin
    // baked into it, and we don't want that.
    TransformationMatrix t;
    if (m_owningLayer->hasTransform()) {
        style->applyTransform(t, toRenderBox(renderer())->pixelSnappedBorderBoxRect().size(), RenderStyle::ExcludeTransformOrigin);
        makeMatrixRenderable(t, compositor()->canRender3DTransforms());
    }

    m_graphicsLayer->setTransform(t);
}

void RenderLayerBacking::updateFilters(const RenderStyle* style)
{
    bool didCompositeFilters = m_canCompositeFilters;
    m_canCompositeFilters = m_graphicsLayer->setFilters(owningLayer()->computeFilterOperations(style));
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
        m_owningLayer->updateOrRemoveFilterEffectRenderer();
        setContentsNeedDisplay();
    }
}

void RenderLayerBacking::updateLayerBlendMode(const RenderStyle*)
{
}

void RenderLayerBacking::updateContentsOpaque()
{
    // For non-root layers, background is always painted by the primary graphics layer.
    ASSERT(m_isMainFrameRenderViewLayer || !m_backgroundLayer);
    if (m_backgroundLayer) {
        m_graphicsLayer->setContentsOpaque(false);
        m_backgroundLayer->setContentsOpaque(m_owningLayer->backgroundIsKnownToBeOpaqueInRect(compositedBounds()));
    } else {
        m_graphicsLayer->setContentsOpaque(m_owningLayer->backgroundIsKnownToBeOpaqueInRect(compositedBounds()));
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

bool RenderLayerBacking::shouldClipCompositedBounds() const
{
    // Scrollbar layers use this layer for relative positioning, so don't clip.
    if (layerForHorizontalScrollbar() || layerForVerticalScrollbar())
        return false;

    if (layerOrAncestorIsTransformedOrUsingCompositedScrolling(m_owningLayer))
        return false;

    return true;
}

void RenderLayerBacking::updateCompositedBounds()
{
    IntRect layerBounds = compositor()->calculateCompositedBounds(m_owningLayer, m_owningLayer);

    // Clip to the size of the document or enclosing overflow-scroll layer.
    // If this or an ancestor is transformed, we can't currently compute the correct rect to intersect with.
    // We'd need RenderObject::convertContainerToLocalQuad(), which doesn't yet exist.
    if (shouldClipCompositedBounds()) {
        RenderView* view = m_owningLayer->renderer()->view();
        RenderLayer* rootLayer = view->layer();

        LayoutRect clippingBounds;
        if (renderer()->style()->position() == FixedPosition && renderer()->container() == view)
            clippingBounds = view->frameView()->viewportConstrainedVisibleContentRect();
        else
            clippingBounds = view->unscaledDocumentRect();

        if (m_owningLayer != rootLayer)
            clippingBounds.intersect(m_owningLayer->backgroundClipRect(RenderLayer::ClipRectsContext(rootLayer, 0, AbsoluteClipRects)).rect()); // FIXME: Incorrect for CSS regions.

        LayoutPoint delta;
        m_owningLayer->convertToLayerCoords(rootLayer, delta);
        clippingBounds.move(-delta.x(), -delta.y());

        layerBounds.intersect(pixelSnappedIntRect(clippingBounds));
        m_boundsConstrainedByClipping = true;
    } else
        m_boundsConstrainedByClipping = false;

    // If the element has a transform-origin that has fixed lengths, and the renderer has zero size,
    // then we need to ensure that the compositing layer has non-zero size so that we can apply
    // the transform-origin via the GraphicsLayer anchorPoint (which is expressed as a fractional value).
    if (layerBounds.isEmpty() && hasNonZeroTransformOrigin(renderer())) {
        layerBounds.setWidth(1);
        layerBounds.setHeight(1);
        m_artificiallyInflatedBounds = true;
    } else
        m_artificiallyInflatedBounds = false;

    setCompositedBounds(layerBounds);
}

void RenderLayerBacking::updateAfterWidgetResize()
{
    if (renderer()->isRenderPart()) {
        if (RenderLayerCompositor* innerCompositor = RenderLayerCompositor::frameContentsCompositor(toRenderPart(renderer()))) {
            innerCompositor->frameViewDidChangeSize();
            innerCompositor->frameViewDidChangeLocation(contentsBox().location());
        }
    }
}

void RenderLayerBacking::updateCompositingReasons()
{
    // All other layers owned by this backing will have the same compositing reason
    // for their lifetime, so they are initialized only when created.
    m_graphicsLayer->setCompositingReasons(m_owningLayer->compositingReasons());
}

void RenderLayerBacking::updateAfterLayout(UpdateAfterLayoutFlags flags)
{
    RenderLayerCompositor* layerCompositor = compositor();
    if (!layerCompositor->compositingLayersNeedRebuild()) {
        // Calling updateGraphicsLayerGeometry() here gives incorrect results, because the
        // position of this layer's GraphicsLayer depends on the position of our compositing
        // ancestor's GraphicsLayer. That cannot be determined until all the descendant
        // RenderLayers of that ancestor have been processed via updateLayerPositions().
        //
        // The solution is to update compositing children of this layer here,
        // via updateCompositingChildrenGeometry().
        updateCompositedBounds();
        HashSet<RenderLayer*> visited;
        layerCompositor->updateCompositingDescendantGeometry(m_owningLayer, m_owningLayer, visited, flags & CompositingChildrenOnly);
        visited.clear();

        if (flags & IsUpdateRoot) {
            updateGraphicsLayerGeometry();
            layerCompositor->updateRootLayerPosition();
            RenderLayer* stackingContainer = m_owningLayer->enclosingStackingContainer();
            if (!layerCompositor->compositingLayersNeedRebuild() && stackingContainer && (stackingContainer != m_owningLayer))
                layerCompositor->updateCompositingDescendantGeometry(stackingContainer, stackingContainer, visited, flags & CompositingChildrenOnly);
        }
    }

    if (flags & NeedsFullRepaint && !paintsIntoCompositedAncestor())
        setContentsNeedDisplay();
}

bool RenderLayerBacking::updateGraphicsLayerConfiguration()
{
    RenderLayerCompositor* compositor = this->compositor();
    RenderObject* renderer = this->renderer();

    m_owningLayer->updateDescendantDependentFlags();
    m_owningLayer->updateZOrderLists();

    bool layerConfigChanged = false;
    setBackgroundLayerPaintsFixedRootBackground(compositor->needsFixedRootBackgroundLayer(m_owningLayer));

    // The background layer is currently only used for fixed root backgrounds.
    if (updateBackgroundLayer(m_backgroundLayerPaintsFixedRootBackground))
        layerConfigChanged = true;

    if (updateForegroundLayer(compositor->needsContentsCompositingLayer(m_owningLayer)))
        layerConfigChanged = true;

    bool needsDescendentsClippingLayer = compositor->clipsCompositingDescendants(m_owningLayer);

    // Our scrolling layer will clip.
    if (m_owningLayer->needsCompositedScrolling())
        needsDescendentsClippingLayer = false;

    RenderLayer* scrollParent = m_owningLayer->scrollParent();
    bool needsAncestorClip = compositor->clippedByAncestor(m_owningLayer);
    bool needsScrollClip = !!scrollParent;
    if (updateClippingLayers(needsAncestorClip, needsDescendentsClippingLayer, needsScrollClip))
        layerConfigChanged = true;

    if (updateOverflowControlsLayers(requiresHorizontalScrollbarLayer(), requiresVerticalScrollbarLayer(), requiresScrollCornerLayer()))
        layerConfigChanged = true;

    if (updateScrollingLayers(m_owningLayer->needsCompositedScrolling()))
        layerConfigChanged = true;

    updateScrollParent(scrollParent);
    updateClipParent(m_owningLayer->clipParent());

    if (layerConfigChanged)
        updateInternalHierarchy();

    if (updateMaskLayer(renderer->hasMask()))
        m_graphicsLayer->setMaskLayer(m_maskLayer.get());

    if (m_owningLayer->hasReflection()) {
        if (m_owningLayer->reflectionLayer()->backing()) {
            GraphicsLayer* reflectionLayer = m_owningLayer->reflectionLayer()->backing()->graphicsLayer();
            m_graphicsLayer->setReplicatedByLayer(reflectionLayer);
        }
    } else
        m_graphicsLayer->setReplicatedByLayer(0);

    updateBackgroundColor(isSimpleContainerCompositingLayer());

    if (isDirectlyCompositedImage())
        updateImageContents();

    if (renderer->isEmbeddedObject() && toRenderEmbeddedObject(renderer)->allowsAcceleratedCompositing()) {
        PluginView* pluginView = toPluginView(toRenderWidget(renderer)->widget());
        m_graphicsLayer->setContentsToMedia(pluginView->platformLayer());
    } else if (renderer->isVideo()) {
        HTMLMediaElement* mediaElement = toHTMLMediaElement(renderer->node());
        m_graphicsLayer->setContentsToMedia(mediaElement->platformLayer());
    } else if (isAcceleratedCanvas(renderer)) {
        HTMLCanvasElement* canvas = toHTMLCanvasElement(renderer->node());
        if (CanvasRenderingContext* context = canvas->renderingContext())
            m_graphicsLayer->setContentsToCanvas(context->platformLayer());
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
        result = renderer->overflowClipRect(LayoutPoint(), 0); // FIXME: Incorrect for CSS regions.

    if (renderer->hasClip())
        result.intersect(renderer->clipRect(LayoutPoint(), 0)); // FIXME: Incorrect for CSS regions.

    return pixelSnappedIntRect(result);
}

void RenderLayerBacking::updateGraphicsLayerGeometry()
{
    // If we haven't built z-order lists yet, wait until later.
    if (m_owningLayer->isStackingContainer() && m_owningLayer->m_zOrderListsDirty)
        return;

    // Set transform property, if it is not animating. We have to do this here because the transform
    // is affected by the layer dimensions.
    if (!renderer()->animation()->isRunningAcceleratedAnimationOnRenderer(renderer(), CSSPropertyWebkitTransform))
        updateTransform(renderer()->style());

    // Set opacity, if it is not animating.
    if (!renderer()->animation()->isRunningAcceleratedAnimationOnRenderer(renderer(), CSSPropertyOpacity))
        updateOpacity(renderer()->style());

    if (RuntimeEnabledFeatures::cssCompositingEnabled())
        updateLayerBlendMode(renderer()->style());

    bool isSimpleContainer = isSimpleContainerCompositingLayer();

    m_owningLayer->updateDescendantDependentFlags();

    // m_graphicsLayer is the corresponding GraphicsLayer for this RenderLayer and its non-compositing
    // descendants. So, the visibility flag for m_graphicsLayer should be true if there are any
    // non-compositing visible layers.
    m_graphicsLayer->setContentsVisible(m_owningLayer->hasVisibleContent() || hasVisibleNonCompositingDescendantLayers());

    RenderStyle* style = renderer()->style();
    // FIXME: reflections should force transform-style to be flat in the style: https://bugs.webkit.org/show_bug.cgi?id=106959
    bool preserves3D = style->transformStyle3D() == TransformStyle3DPreserve3D && !renderer()->hasReflection();
    m_graphicsLayer->setPreserves3D(preserves3D);
    m_graphicsLayer->setBackfaceVisibility(style->backfaceVisibility() == BackfaceVisibilityVisible);

    RenderLayer* compAncestor = m_owningLayer->ancestorCompositingLayer();

    // We compute everything relative to the enclosing compositing layer.
    IntRect ancestorCompositingBounds;
    if (compAncestor) {
        ASSERT(compAncestor->backing());
        ancestorCompositingBounds = pixelSnappedIntRect(compAncestor->backing()->compositedBounds());
    }

    IntRect localCompositingBounds = pixelSnappedIntRect(compositedBounds());

    IntRect relativeCompositingBounds(localCompositingBounds);
    IntPoint delta;
    m_owningLayer->convertToPixelSnappedLayerCoords(compAncestor, delta);
    relativeCompositingBounds.moveBy(delta);

    IntPoint graphicsLayerParentLocation;
    if (compAncestor && compAncestor->backing()->hasClippingLayer()) {
        // If the compositing ancestor has a layer to clip children, we parent in that, and therefore
        // position relative to it.
        IntRect clippingBox = clipBox(toRenderBox(compAncestor->renderer()));
        graphicsLayerParentLocation = clippingBox.location();
    } else if (compAncestor)
        graphicsLayerParentLocation = ancestorCompositingBounds.location();
    else
        graphicsLayerParentLocation = renderer()->view()->documentRect().location();

    if (compAncestor && compAncestor->needsCompositedScrolling()) {
        RenderBox* renderBox = toRenderBox(compAncestor->renderer());
        IntSize scrollOffset = compAncestor->scrolledContentOffset();
        IntPoint scrollOrigin(renderBox->borderLeft(), renderBox->borderTop());
        graphicsLayerParentLocation = scrollOrigin - scrollOffset;
    }

    if (compAncestor && m_ancestorScrollClippingLayer && m_owningLayer->ancestorScrollingLayer()) {
        // Our scroll parent must have been processed before us. The code in RenderLayerCompositor
        // that coordinates updating graphics layer geometry has been set up to guarantee that this is the case.
        RenderLayer* scrollParent = m_owningLayer->ancestorScrollingLayer();
        GraphicsLayer* scrollParentClippingLayer = scrollParent->backing()->scrollingLayer();

        // Not relative to our parent graphics layer.
        FloatPoint position;
        GraphicsLayer* scrollParentChildForSuperlayers = scrollParent->backing()->childForSuperlayers();

        for (GraphicsLayer* scrollAncestor = scrollParentClippingLayer; scrollAncestor; scrollAncestor = scrollAncestor->parent()) {
            ASSERT(scrollAncestor->transform().isIdentity());
            position = position + toFloatSize(scrollAncestor->position());
            if (scrollAncestor == scrollParentChildForSuperlayers)
                break;
        }

        m_ancestorScrollClippingLayer->setPosition(position);
        m_ancestorScrollClippingLayer->setSize(scrollParentClippingLayer->size());
        m_ancestorScrollClippingLayer->setOffsetFromRenderer(toIntSize(roundedIntPoint(-position)));

        graphicsLayerParentLocation = roundedIntPoint(position);
    }

    if (compAncestor && m_ancestorClippingLayer) {
        // Call calculateRects to get the backgroundRect which is what is used to clip the contents of this
        // layer. Note that we call it with temporaryClipRects = true because normally when computing clip rects
        // for a compositing layer, rootLayer is the layer itself.
        RenderLayer::ClipRectsContext clipRectsContext(compAncestor, 0, TemporaryClipRects, IgnoreOverlayScrollbarSize, IgnoreOverflowClip);
        IntRect parentClipRect = pixelSnappedIntRect(m_owningLayer->backgroundClipRect(clipRectsContext).rect()); // FIXME: Incorrect for CSS regions.
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
    if (oldSize != contentsSize) {
        m_graphicsLayer->setSize(contentsSize);
        // Usually invalidation will happen via layout etc, but if we've affected the layer
        // size by constraining relative to a clipping ancestor or the viewport, we
        // have to invalidate to avoid showing stretched content.
        if (m_boundsConstrainedByClipping)
            m_graphicsLayer->setNeedsDisplay();
    }

    // If we have a layer that clips children, position it.
    IntRect clippingBox;
    if (GraphicsLayer* clipLayer = clippingLayer()) {
        clippingBox = clipBox(toRenderBox(renderer()));
        clipLayer->setPosition(FloatPoint(clippingBox.location() - localCompositingBounds.location()));
        clipLayer->setSize(clippingBox.size());
        clipLayer->setOffsetFromRenderer(toIntSize(clippingBox.location()));
    }

    if (m_maskLayer) {
        if (m_maskLayer->size() != m_graphicsLayer->size()) {
            m_maskLayer->setSize(m_graphicsLayer->size());
            m_maskLayer->setNeedsDisplay();
        }
        m_maskLayer->setPosition(FloatPoint());
        m_maskLayer->setOffsetFromRenderer(m_graphicsLayer->offsetFromRenderer());
    }

    if (m_owningLayer->hasTransform()) {
        const IntRect borderBox = toRenderBox(renderer())->pixelSnappedBorderBoxRect();

        // Get layout bounds in the coords of compAncestor to match relativeCompositingBounds.
        IntRect layerBounds = IntRect(delta, borderBox.size());

        // Update properties that depend on layer dimensions
        FloatPoint3D transformOrigin = computeTransformOrigin(borderBox);
        // Compute the anchor point, which is in the center of the renderer box unless transform-origin is set.
        FloatPoint3D anchor(relativeCompositingBounds.width()  != 0.0f ? ((layerBounds.x() - relativeCompositingBounds.x()) + transformOrigin.x()) / relativeCompositingBounds.width()  : 0.5f,
                            relativeCompositingBounds.height() != 0.0f ? ((layerBounds.y() - relativeCompositingBounds.y()) + transformOrigin.y()) / relativeCompositingBounds.height() : 0.5f,
                            transformOrigin.z());
        m_graphicsLayer->setAnchorPoint(anchor);

        RenderStyle* style = renderer()->style();
        GraphicsLayer* clipLayer = clippingLayer();
        if (style->hasPerspective()) {
            TransformationMatrix t = owningLayer()->perspectiveTransform();

            if (clipLayer) {
                clipLayer->setChildrenTransform(t);
                m_graphicsLayer->setChildrenTransform(TransformationMatrix());
            }
            else
                m_graphicsLayer->setChildrenTransform(t);
        } else {
            if (clipLayer)
                clipLayer->setChildrenTransform(TransformationMatrix());
            else
                m_graphicsLayer->setChildrenTransform(TransformationMatrix());
        }
    } else {
        m_graphicsLayer->setAnchorPoint(FloatPoint3D(0.5f, 0.5f, 0));
    }

    if (m_foregroundLayer) {
        FloatPoint foregroundPosition;
        FloatSize foregroundSize = contentsSize;
        IntSize foregroundOffset = m_graphicsLayer->offsetFromRenderer();
        if (hasClippingLayer()) {
            // If we have a clipping layer (which clips descendants), then the foreground layer is a child of it,
            // so that it gets correctly sorted with children. In that case, position relative to the clipping layer.
            foregroundSize = FloatSize(clippingBox.size());
            foregroundOffset = toIntSize(clippingBox.location());
        }

        m_foregroundLayer->setPosition(foregroundPosition);
        if (foregroundSize != m_foregroundLayer->size()) {
            m_foregroundLayer->setSize(foregroundSize);
            m_foregroundLayer->setNeedsDisplay();
        }
        m_foregroundLayer->setOffsetFromRenderer(foregroundOffset);
    }

    if (m_backgroundLayer) {
        FloatPoint backgroundPosition;
        FloatSize backgroundSize = contentsSize;
        if (backgroundLayerPaintsFixedRootBackground()) {
            FrameView* frameView = toRenderView(renderer())->frameView();
            backgroundSize = frameView->visibleContentRect().size();
        }
        m_backgroundLayer->setPosition(backgroundPosition);
        if (backgroundSize != m_backgroundLayer->size()) {
            m_backgroundLayer->setSize(backgroundSize);
            m_backgroundLayer->setNeedsDisplay();
        }
        m_backgroundLayer->setOffsetFromRenderer(m_graphicsLayer->offsetFromRenderer());
    }

    if (m_owningLayer->reflectionLayer() && m_owningLayer->reflectionLayer()->isComposited()) {
        RenderLayerBacking* reflectionBacking = m_owningLayer->reflectionLayer()->backing();
        reflectionBacking->updateGraphicsLayerGeometry();

        // The reflection layer has the bounds of m_owningLayer->reflectionLayer(),
        // but the reflected layer is the bounds of this layer, so we need to position it appropriately.
        FloatRect layerBounds = compositedBounds();
        FloatRect reflectionLayerBounds = reflectionBacking->compositedBounds();
        reflectionBacking->graphicsLayer()->setReplicatedLayerPosition(FloatPoint(layerBounds.location() - reflectionLayerBounds.location()));
    }

    if (m_scrollingLayer) {
        ASSERT(m_scrollingContentsLayer);
        RenderBox* renderBox = toRenderBox(renderer());
        IntRect clientBox = enclosingIntRect(renderBox->clientBoxRect());
        // FIXME: We should make RenderBox::clientBoxRect consider scrollbar placement.
        if (style->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
            clientBox.move(renderBox->verticalScrollbarWidth(), 0);

        IntSize adjustedScrollOffset = m_owningLayer->adjustedScrollOffset();
        m_scrollingLayer->setPosition(FloatPoint(clientBox.location() - localCompositingBounds.location()));
        m_scrollingLayer->setSize(clientBox.size());

        IntSize oldScrollingLayerOffset = m_scrollingLayer->offsetFromRenderer();
        m_scrollingLayer->setOffsetFromRenderer(-toIntSize(clientBox.location()));

        bool clientBoxOffsetChanged = oldScrollingLayerOffset != m_scrollingLayer->offsetFromRenderer();

        IntSize scrollSize(m_owningLayer->scrollWidth(), m_owningLayer->scrollHeight());
        if (scrollSize != m_scrollingContentsLayer->size() || clientBoxOffsetChanged)
            m_scrollingContentsLayer->setNeedsDisplay();

        IntSize scrollingContentsOffset = toIntSize(clientBox.location() - adjustedScrollOffset);
        if (scrollingContentsOffset != m_scrollingContentsLayer->offsetFromRenderer() || scrollSize != m_scrollingContentsLayer->size()) {
            bool scrollingCoordinatorHandlesOffset = compositor()->scrollingLayerDidChange(m_owningLayer);

            if (scrollingCoordinatorHandlesOffset)
                m_scrollingContentsLayer->setPosition(-m_owningLayer->scrollOrigin());
            else
                m_scrollingContentsLayer->setPosition(FloatPoint(-adjustedScrollOffset));
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

    // If this layer was created just for clipping or to apply perspective, it doesn't need its own backing store.
    setRequiresOwnBackingStore(compositor()->requiresOwnBackingStore(m_owningLayer, compAncestor));

    updateContentsRect(isSimpleContainer);
    updateBackgroundColor(isSimpleContainer);
    updateDrawsContent(isSimpleContainer);
    updateContentsOpaque();
    updateAfterWidgetResize();
    registerScrollingLayers();

    updateCompositingReasons();
}

void RenderLayerBacking::registerScrollingLayers()
{
    // Register fixed position layers and their containers with the scrolling coordinator.
    ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer);
    if (!scrollingCoordinator)
        return;

    compositor()->updateViewportConstraintStatus(m_owningLayer);

    scrollingCoordinator->updateLayerPositionConstraint(m_owningLayer);

    // Page scale is applied as a transform on the root render view layer. Because the scroll
    // layer is further up in the hierarchy, we need to avoid marking the root render view
    // layer as a container.
    bool isContainer = m_owningLayer->hasTransform() && !m_owningLayer->isRootLayer();
    scrollingCoordinator->setLayerIsContainerForFixedPositionLayers(childForSuperlayers(), isContainer);
}

void RenderLayerBacking::updateInternalHierarchy()
{
    if (m_ancestorScrollClippingLayer)
        m_ancestorScrollClippingLayer->removeAllChildren();

    // m_foregroundLayer has to be inserted in the correct order with child layers,
    // so it's not inserted here.
    if (m_ancestorClippingLayer)
        m_ancestorClippingLayer->removeAllChildren();

    m_graphicsLayer->removeFromParent();

    if (m_ancestorClippingLayer)
        m_ancestorClippingLayer->addChild(m_graphicsLayer.get());

    if (m_ancestorScrollClippingLayer) {
        if (m_ancestorClippingLayer)
            m_ancestorScrollClippingLayer->addChild(m_ancestorClippingLayer.get());
        else
            m_ancestorScrollClippingLayer->addChild(m_graphicsLayer.get());
    }

    if (m_childContainmentLayer) {
        m_childContainmentLayer->removeFromParent();
        m_graphicsLayer->addChild(m_childContainmentLayer.get());
    }

    if (m_scrollingLayer) {
        GraphicsLayer* superlayer = m_childContainmentLayer ? m_childContainmentLayer.get() : m_graphicsLayer.get();
        m_scrollingLayer->removeFromParent();
        superlayer->addChild(m_scrollingLayer.get());
    }

    // The clip for child layers does not include space for overflow controls, so they exist as
    // siblings of the clipping layer if we have one. Normal children of this layer are set as
    // children of the clipping layer.
    if (m_layerForHorizontalScrollbar) {
        m_layerForHorizontalScrollbar->removeFromParent();
        m_graphicsLayer->addChild(m_layerForHorizontalScrollbar.get());
    }
    if (m_layerForVerticalScrollbar) {
        m_layerForVerticalScrollbar->removeFromParent();
        m_graphicsLayer->addChild(m_layerForVerticalScrollbar.get());
    }
    if (m_layerForScrollCorner) {
        m_layerForScrollCorner->removeFromParent();
        m_graphicsLayer->addChild(m_layerForScrollCorner.get());
    }
}

void RenderLayerBacking::updateContentsRect(bool isSimpleContainer)
{
    IntRect contentsRect;
    if (isSimpleContainer && renderer()->hasBackground())
        contentsRect = backgroundBox();
    else
        contentsRect = contentsBox();

    m_graphicsLayer->setContentsRect(contentsRect);
}

void RenderLayerBacking::updateDrawsContent(bool isSimpleContainer)
{
    if (m_scrollingLayer) {
        // We don't have to consider overflow controls, because we know that the scrollbars are drawn elsewhere.
        // m_graphicsLayer only needs backing store if the non-scrolling parts (background, outlines, borders, shadows etc) need to paint.
        // m_scrollingLayer never has backing store.
        // m_scrollingContentsLayer only needs backing store if the scrolled contents need to paint.
        bool hasNonScrollingPaintedContent = m_owningLayer->hasVisibleContent() && m_owningLayer->hasBoxDecorationsOrBackground();
        m_graphicsLayer->setDrawsContent(hasNonScrollingPaintedContent);

        bool hasScrollingPaintedContent = m_owningLayer->hasVisibleContent() && (renderer()->hasBackground() || paintsChildren());
        m_scrollingContentsLayer->setDrawsContent(hasScrollingPaintedContent);
        return;
    }

    bool hasPaintedContent = containsPaintedContent(isSimpleContainer);
    if (hasPaintedContent && isAcceleratedCanvas(renderer())) {
        CanvasRenderingContext* context = toHTMLCanvasElement(renderer()->node())->renderingContext();
        // Content layer may be null if context is lost.
        if (WebKit::WebLayer* contentLayer = context->platformLayer()) {
            Color bgColor;
            if (contentLayerSupportsDirectBackgroundComposition(renderer())) {
                bgColor = rendererBackgroundColor();
                hasPaintedContent = false;
            }
            contentLayer->setBackgroundColor(bgColor.rgb());
        }
    }

    // FIXME: we could refine this to only allocate backing for one of these layers if possible.
    m_graphicsLayer->setDrawsContent(hasPaintedContent);
    if (m_foregroundLayer)
        m_foregroundLayer->setDrawsContent(hasPaintedContent);

    if (m_backgroundLayer)
        m_backgroundLayer->setDrawsContent(hasPaintedContent);
}

// Return true if the layers changed.
bool RenderLayerBacking::updateClippingLayers(bool needsAncestorClip, bool needsDescendantClip, bool needsScrollClip)
{
    bool layersChanged = false;

    if (needsAncestorClip) {
        if (!m_ancestorClippingLayer) {
            m_ancestorClippingLayer = createGraphicsLayer(CompositingReasonLayerForClip);
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
            m_childContainmentLayer = createGraphicsLayer(CompositingReasonLayerForClip);
            m_childContainmentLayer->setMasksToBounds(true);
            layersChanged = true;
        }
    } else if (hasClippingLayer()) {
        m_childContainmentLayer->removeFromParent();
        m_childContainmentLayer = nullptr;
        layersChanged = true;
    }

    if (needsScrollClip) {
        if (!m_ancestorScrollClippingLayer) {
            m_ancestorScrollClippingLayer = createGraphicsLayer(CompositingReasonLayerForClip);
            m_ancestorScrollClippingLayer->setMasksToBounds(true);
            layersChanged = true;
        }
    } else if (m_ancestorScrollClippingLayer) {
        m_ancestorScrollClippingLayer->removeFromParent();
        m_ancestorScrollClippingLayer = nullptr;
        layersChanged = true;
    }

    return layersChanged;
}

void RenderLayerBacking::setBackgroundLayerPaintsFixedRootBackground(bool backgroundLayerPaintsFixedRootBackground)
{
    m_backgroundLayerPaintsFixedRootBackground = backgroundLayerPaintsFixedRootBackground;
}

bool RenderLayerBacking::updateOverflowControlsLayers(bool needsHorizontalScrollbarLayer, bool needsVerticalScrollbarLayer, bool needsScrollCornerLayer)
{
    bool horizontalScrollbarLayerChanged = false;
    if (needsHorizontalScrollbarLayer) {
        if (!m_layerForHorizontalScrollbar) {
            m_layerForHorizontalScrollbar = createGraphicsLayer(CompositingReasonLayerForScrollbar);
            horizontalScrollbarLayerChanged = true;
        }
    } else if (m_layerForHorizontalScrollbar) {
        m_layerForHorizontalScrollbar = nullptr;
        horizontalScrollbarLayerChanged = true;
    }

    bool verticalScrollbarLayerChanged = false;
    if (needsVerticalScrollbarLayer) {
        if (!m_layerForVerticalScrollbar) {
            m_layerForVerticalScrollbar = createGraphicsLayer(CompositingReasonLayerForScrollbar);
            verticalScrollbarLayerChanged = true;
        }
    } else if (m_layerForVerticalScrollbar) {
        m_layerForVerticalScrollbar = nullptr;
        verticalScrollbarLayerChanged = true;
    }

    bool scrollCornerLayerChanged = false;
    if (needsScrollCornerLayer) {
        if (!m_layerForScrollCorner) {
            m_layerForScrollCorner = createGraphicsLayer(CompositingReasonLayerForScrollbar);
            scrollCornerLayerChanged = true;
        }
    } else if (m_layerForScrollCorner) {
        m_layerForScrollCorner = nullptr;
        scrollCornerLayerChanged = true;
    }

    if (ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer)) {
        if (horizontalScrollbarLayerChanged)
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_owningLayer->scrollableArea(), HorizontalScrollbar);
        if (verticalScrollbarLayerChanged)
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_owningLayer->scrollableArea(), VerticalScrollbar);
    }

    return horizontalScrollbarLayerChanged || verticalScrollbarLayerChanged || scrollCornerLayerChanged;
}

void RenderLayerBacking::positionOverflowControlsLayers(const IntSize& offsetFromRoot)
{
    IntSize offsetFromRenderer = m_graphicsLayer->offsetFromRenderer();
    if (GraphicsLayer* layer = layerForHorizontalScrollbar()) {
        Scrollbar* hBar = m_owningLayer->horizontalScrollbar();
        if (hBar) {
            layer->setPosition(hBar->frameRect().location() - offsetFromRoot - offsetFromRenderer);
            layer->setSize(hBar->frameRect().size());
            if (layer->hasContentsLayer())
                layer->setContentsRect(IntRect(IntPoint(), hBar->frameRect().size()));
        }
        layer->setDrawsContent(hBar && !layer->hasContentsLayer());
    }

    if (GraphicsLayer* layer = layerForVerticalScrollbar()) {
        Scrollbar* vBar = m_owningLayer->verticalScrollbar();
        if (vBar) {
            layer->setPosition(vBar->frameRect().location() - offsetFromRoot - offsetFromRenderer);
            layer->setSize(vBar->frameRect().size());
            if (layer->hasContentsLayer())
                layer->setContentsRect(IntRect(IntPoint(), vBar->frameRect().size()));
        }
        layer->setDrawsContent(vBar && !layer->hasContentsLayer());
    }

    if (GraphicsLayer* layer = layerForScrollCorner()) {
        const LayoutRect& scrollCornerAndResizer = m_owningLayer->scrollCornerAndResizerRect();
        layer->setPosition(scrollCornerAndResizer.location() - offsetFromRenderer);
        layer->setSize(scrollCornerAndResizer.size());
        layer->setDrawsContent(!scrollCornerAndResizer.isEmpty());
    }
}

bool RenderLayerBacking::hasUnpositionedOverflowControlsLayers() const
{
    if (GraphicsLayer* layer = layerForHorizontalScrollbar())
        if (!layer->drawsContent())
            return true;

    if (GraphicsLayer* layer = layerForVerticalScrollbar())
        if (!layer->drawsContent())
            return true;

    if (GraphicsLayer* layer = layerForScrollCorner())
        if (!layer->drawsContent())
            return true;

    return false;
}

bool RenderLayerBacking::updateForegroundLayer(bool needsForegroundLayer)
{
    bool layerChanged = false;
    if (needsForegroundLayer) {
        if (!m_foregroundLayer) {
            m_foregroundLayer = createGraphicsLayer(CompositingReasonLayerForForeground);
            m_foregroundLayer->setDrawsContent(true);
            m_foregroundLayer->setPaintingPhase(GraphicsLayerPaintForeground);
            layerChanged = true;
        }
    } else if (m_foregroundLayer) {
        m_foregroundLayer->removeFromParent();
        m_foregroundLayer = nullptr;
        layerChanged = true;
    }

    if (layerChanged)
        m_graphicsLayer->setPaintingPhase(paintingPhaseForPrimaryLayer());

    return layerChanged;
}

bool RenderLayerBacking::updateBackgroundLayer(bool needsBackgroundLayer)
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

    if (layerChanged && !m_owningLayer->renderer()->documentBeingDestroyed())
        compositor()->rootFixedBackgroundsChanged();

    return layerChanged;
}

bool RenderLayerBacking::updateMaskLayer(bool needsMaskLayer)
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

bool RenderLayerBacking::updateScrollingLayers(bool needsScrollingLayers)
{
    ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer);

    bool layerChanged = false;
    if (needsScrollingLayers) {
        if (!m_scrollingLayer) {
            // Outer layer which corresponds with the scroll view.
            m_scrollingLayer = createGraphicsLayer(CompositingReasonLayerForClip);
            m_scrollingLayer->setDrawsContent(false);
            m_scrollingLayer->setMasksToBounds(true);

            // Inner layer which renders the content that scrolls.
            m_scrollingContentsLayer = createGraphicsLayer(CompositingReasonLayerForScrollingContainer);
            m_scrollingContentsLayer->setDrawsContent(true);
            GraphicsLayerPaintingPhase paintPhase = GraphicsLayerPaintOverflowContents | GraphicsLayerPaintCompositedScroll;
            if (!m_foregroundLayer)
                paintPhase |= GraphicsLayerPaintForeground;
            m_scrollingContentsLayer->setPaintingPhase(paintPhase);
            m_scrollingLayer->addChild(m_scrollingContentsLayer.get());

            layerChanged = true;
            if (scrollingCoordinator)
                scrollingCoordinator->scrollableAreaScrollLayerDidChange(m_owningLayer->scrollableArea());
        }
    } else if (m_scrollingLayer) {
        m_scrollingLayer = nullptr;
        m_scrollingContentsLayer = nullptr;
        layerChanged = true;
        if (scrollingCoordinator)
            scrollingCoordinator->scrollableAreaScrollLayerDidChange(m_owningLayer->scrollableArea());
    }

    if (layerChanged) {
        updateInternalHierarchy();
        m_graphicsLayer->setPaintingPhase(paintingPhaseForPrimaryLayer());
        m_graphicsLayer->setNeedsDisplay();
        if (renderer()->view())
            compositor()->scrollingLayerDidChange(m_owningLayer);
    }

    return layerChanged;
}

void RenderLayerBacking::updateScrollParent(RenderLayer* scrollParent)
{
    if (ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer))
        scrollingCoordinator->updateScrollParentForLayer(m_owningLayer, scrollParent);
}

void RenderLayerBacking::updateClipParent(RenderLayer* clipParent)
{
    if (ScrollingCoordinator* scrollingCoordinator = scrollingCoordinatorFromLayer(m_owningLayer))
        scrollingCoordinator->updateClipParentForLayer(m_owningLayer, clipParent);
}

GraphicsLayerPaintingPhase RenderLayerBacking::paintingPhaseForPrimaryLayer() const
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

    if (m_owningLayer->compositingReasons() & CompositingReasonOverflowScrollingParent)
        phase |= GraphicsLayerPaintCompositedScroll;

    return static_cast<GraphicsLayerPaintingPhase>(phase);
}

float RenderLayerBacking::compositingOpacity(float rendererOpacity) const
{
    float finalOpacity = rendererOpacity;

    for (RenderLayer* curr = m_owningLayer->parent(); curr; curr = curr->parent()) {
        // We only care about parents that are stacking contexts.
        // Recall that opacity creates stacking context.
        if (!curr->isStackingContainer())
            continue;

        // If we found a compositing layer, we want to compute opacity
        // relative to it. So we can break here.
        if (curr->isComposited())
            break;

        finalOpacity *= curr->renderer()->opacity();
    }

    return finalOpacity;
}

Color RenderLayerBacking::rendererBackgroundColor() const
{
    RenderObject* backgroundRenderer = renderer();
    if (backgroundRenderer->isRoot())
        backgroundRenderer = backgroundRenderer->rendererForRootBackground();

    return backgroundRenderer->resolveColor(CSSPropertyBackgroundColor);
}

void RenderLayerBacking::updateBackgroundColor(bool isSimpleContainer)
{
    Color backgroundColor = rendererBackgroundColor();
    if (isSimpleContainer) {
        m_graphicsLayer->setContentsToSolidColor(backgroundColor);
        m_graphicsLayer->setBackgroundColor(Color());
    } else {
        // An unset (invalid) color will remove the solid color.
        m_graphicsLayer->setContentsToSolidColor(Color());
        m_graphicsLayer->setBackgroundColor(backgroundColor);
    }
}

static bool supportsDirectBoxDecorationsComposition(const RenderObject* renderer)
{
    if (!GraphicsLayer::supportsBackgroundColorContent())
        return false;

    if (renderer->hasClip())
        return false;

    if (hasBoxDecorationsOrBackgroundImage(renderer->style()))
        return false;

    // FIXME: we should be able to allow backgroundComposite; However since this is not a common use case it has been deferred for now.
    if (renderer->style()->backgroundComposite() != CompositeSourceOver)
        return false;

    if (renderer->style()->backgroundClip() == TextFillBox)
        return false;

    return true;
}

bool RenderLayerBacking::paintsBoxDecorations() const
{
    if (!m_owningLayer->hasVisibleBoxDecorations())
        return false;

    if (!supportsDirectBoxDecorationsComposition(renderer()))
        return true;

    return false;
}

bool RenderLayerBacking::paintsChildren() const
{
    if (m_owningLayer->hasVisibleContent() && m_owningLayer->hasNonEmptyChildRenderers())
        return true;

    if (hasVisibleNonCompositingDescendantLayers())
        return true;

    return false;
}

static bool isCompositedPlugin(RenderObject* renderer)
{
    return renderer->isEmbeddedObject() && toRenderEmbeddedObject(renderer)->allowsAcceleratedCompositing();
}

// A "simple container layer" is a RenderLayer which has no visible content to render.
// It may have no children, or all its children may be themselves composited.
// This is a useful optimization, because it allows us to avoid allocating backing store.
bool RenderLayerBacking::isSimpleContainerCompositingLayer() const
{
    RenderObject* renderObject = renderer();
    if (renderObject->hasMask()) // masks require special treatment
        return false;

    if (renderObject->isReplaced() && !isCompositedPlugin(renderObject))
        return false;

    if (paintsBoxDecorations() || paintsChildren())
        return false;

    if (renderObject->isRenderRegion())
        return false;

    if (renderObject->node() && renderObject->node()->isDocumentNode()) {
        // Look to see if the root object has a non-simple background
        RenderObject* rootObject = renderObject->document().documentElement() ? renderObject->document().documentElement()->renderer() : 0;
        if (!rootObject)
            return false;

        RenderStyle* style = rootObject->style();

        // Reject anything that has a border, a border-radius or outline,
        // or is not a simple background (no background, or solid color).
        if (hasBoxDecorationsOrBackgroundImage(style))
            return false;

        // Now look at the body's renderer.
        HTMLElement* body = renderObject->document().body();
        RenderObject* bodyObject = (body && body->hasLocalName(bodyTag)) ? body->renderer() : 0;
        if (!bodyObject)
            return false;

        style = bodyObject->style();

        if (hasBoxDecorationsOrBackgroundImage(style))
            return false;
    }

    return true;
}

static bool hasVisibleNonCompositingDescendant(RenderLayer* parent)
{
    // FIXME: We shouldn't be called with a stale z-order lists. See bug 85512.
    parent->updateLayerListsIfNeeded();

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(parent);
#endif

    if (Vector<RenderLayer*>* normalFlowList = parent->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            if (!curLayer->isComposited()
                && (curLayer->hasVisibleContent() || hasVisibleNonCompositingDescendant(curLayer)))
                return true;
        }
    }

    if (parent->isStackingContainer()) {
        if (!parent->hasVisibleDescendant())
            return false;

        // Use the m_hasCompositingDescendant bit to optimize?
        if (Vector<RenderLayer*>* negZOrderList = parent->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = negZOrderList->at(i);
                if (!curLayer->isComposited()
                    && (curLayer->hasVisibleContent() || hasVisibleNonCompositingDescendant(curLayer)))
                    return true;
            }
        }

        if (Vector<RenderLayer*>* posZOrderList = parent->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = posZOrderList->at(i);
                if (!curLayer->isComposited()
                    && (curLayer->hasVisibleContent() || hasVisibleNonCompositingDescendant(curLayer)))
                    return true;
            }
        }
    }

    return false;
}

// Conservative test for having no rendered children.
bool RenderLayerBacking::hasVisibleNonCompositingDescendantLayers() const
{
    return hasVisibleNonCompositingDescendant(m_owningLayer);
}

bool RenderLayerBacking::containsPaintedContent(bool isSimpleContainer) const
{
    if (isSimpleContainer || paintsIntoCompositedAncestor() || m_artificiallyInflatedBounds || m_owningLayer->isReflection())
        return false;

    if (isDirectlyCompositedImage())
        return false;

    // FIXME: we could optimize cases where the image, video or canvas is known to fill the border box entirely,
    // and set background color on the layer in that case, instead of allocating backing store and painting.
    if (renderer()->isVideo() && toRenderVideo(renderer())->shouldDisplayVideo())
        return m_owningLayer->hasBoxDecorationsOrBackground();

    return true;
}

// An image can be directly compositing if it's the sole content of the layer, and has no box decorations
// that require painting. Direct compositing saves backing store.
bool RenderLayerBacking::isDirectlyCompositedImage() const
{
    RenderObject* renderObject = renderer();

    if (!renderObject->isImage() || m_owningLayer->hasBoxDecorationsOrBackground() || renderObject->hasClip())
        return false;

    RenderImage* imageRenderer = toRenderImage(renderObject);
    if (ImageResource* cachedImage = imageRenderer->cachedImage()) {
        if (!cachedImage->hasImage())
            return false;

        Image* image = cachedImage->imageForRenderer(imageRenderer);
        if (!image->isBitmapImage())
            return false;

        return m_graphicsLayer->shouldDirectlyCompositeImage(image);
    }

    return false;
}

void RenderLayerBacking::contentChanged(ContentChangeType changeType)
{
    if ((changeType == ImageChanged) && isDirectlyCompositedImage()) {
        updateImageContents();
        return;
    }

    if ((changeType == MaskImageChanged) && m_maskLayer) {
        // The composited layer bounds relies on box->maskClipRect(), which changes
        // when the mask image becomes available.
        updateAfterLayout(CompositingChildrenOnly | IsUpdateRoot);
    }

    if ((changeType == CanvasChanged || changeType == CanvasPixelsChanged) && isAcceleratedCanvas(renderer())) {
        m_graphicsLayer->setContentsNeedsDisplay();
        return;
    }
}

void RenderLayerBacking::updateImageContents()
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
    bool isSimpleContainer = false;
    updateDrawsContent(isSimpleContainer);

    // Image animation is "lazy", in that it automatically stops unless someone is drawing
    // the image. So we have to kick the animation each time; this has the downside that the
    // image will keep animating, even if its layer is not visible.
    image->startAnimation();
}

FloatPoint3D RenderLayerBacking::computeTransformOrigin(const IntRect& borderBox) const
{
    RenderStyle* style = renderer()->style();

    FloatPoint3D origin;
    origin.setX(floatValueForLength(style->transformOriginX(), borderBox.width()));
    origin.setY(floatValueForLength(style->transformOriginY(), borderBox.height()));
    origin.setZ(style->transformOriginZ());

    return origin;
}

FloatPoint RenderLayerBacking::computePerspectiveOrigin(const IntRect& borderBox) const
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
IntSize RenderLayerBacking::contentOffsetInCompostingLayer() const
{
    return IntSize(-m_compositedBounds.x(), -m_compositedBounds.y());
}

IntRect RenderLayerBacking::contentsBox() const
{
    IntRect contentsBox = contentsRect(renderer());
    contentsBox.move(contentOffsetInCompostingLayer());
    return contentsBox;
}

IntRect RenderLayerBacking::backgroundBox() const
{
    IntRect backgroundBox = backgroundRect(renderer());
    backgroundBox.move(contentOffsetInCompostingLayer());
    return backgroundBox;
}

GraphicsLayer* RenderLayerBacking::parentForSublayers() const
{
    if (m_scrollingContentsLayer)
        return m_scrollingContentsLayer.get();

    return m_childContainmentLayer ? m_childContainmentLayer.get() : m_graphicsLayer.get();
}

GraphicsLayer* RenderLayerBacking::childForSuperlayers() const
{
    if (m_ancestorScrollClippingLayer)
        return m_ancestorScrollClippingLayer.get();

    if (m_ancestorClippingLayer)
        return m_ancestorClippingLayer.get();

    return m_graphicsLayer.get();
}

void RenderLayerBacking::setRequiresOwnBackingStore(bool requiresOwnBacking)
{
    if (requiresOwnBacking == m_requiresOwnBackingStore)
        return;

    m_requiresOwnBackingStore = requiresOwnBacking;

    // This affects the answer to paintsIntoCompositedAncestor(), which in turn affects
    // cached clip rects, so when it changes we have to clear clip rects on descendants.
    m_owningLayer->clearClipRectsIncludingDescendants(PaintingClipRects);
    m_owningLayer->computeRepaintRectsIncludingDescendants();

    compositor()->repaintInCompositedAncestor(m_owningLayer, compositedBounds());
}

void RenderLayerBacking::setBlendMode(BlendMode)
{
}

void RenderLayerBacking::setContentsNeedDisplay()
{
    ASSERT(!paintsIntoCompositedAncestor());

    if (m_graphicsLayer && m_graphicsLayer->drawsContent())
        m_graphicsLayer->setNeedsDisplay();

    if (m_foregroundLayer && m_foregroundLayer->drawsContent())
        m_foregroundLayer->setNeedsDisplay();

    if (m_backgroundLayer && m_backgroundLayer->drawsContent())
        m_backgroundLayer->setNeedsDisplay();

    if (m_maskLayer && m_maskLayer->drawsContent())
        m_maskLayer->setNeedsDisplay();

    if (m_scrollingContentsLayer && m_scrollingContentsLayer->drawsContent())
        m_scrollingContentsLayer->setNeedsDisplay();
}

// r is in the coordinate space of the layer's render object
void RenderLayerBacking::setContentsNeedDisplayInRect(const IntRect& r)
{
    ASSERT(!paintsIntoCompositedAncestor());

    if (m_graphicsLayer && m_graphicsLayer->drawsContent()) {
        IntRect layerDirtyRect = r;
        layerDirtyRect.move(-m_graphicsLayer->offsetFromRenderer());
        m_graphicsLayer->setNeedsDisplayInRect(layerDirtyRect);
    }

    if (m_foregroundLayer && m_foregroundLayer->drawsContent()) {
        IntRect layerDirtyRect = r;
        layerDirtyRect.move(-m_foregroundLayer->offsetFromRenderer());
        m_foregroundLayer->setNeedsDisplayInRect(layerDirtyRect);
    }

    // FIXME: need to split out repaints for the background.
    if (m_backgroundLayer && m_backgroundLayer->drawsContent()) {
        IntRect layerDirtyRect = r;
        layerDirtyRect.move(-m_backgroundLayer->offsetFromRenderer());
        m_backgroundLayer->setNeedsDisplayInRect(layerDirtyRect);
    }

    if (m_maskLayer && m_maskLayer->drawsContent()) {
        IntRect layerDirtyRect = r;
        layerDirtyRect.move(-m_maskLayer->offsetFromRenderer());
        m_maskLayer->setNeedsDisplayInRect(layerDirtyRect);
    }

    if (m_scrollingContentsLayer && m_scrollingContentsLayer->drawsContent()) {
        IntRect layerDirtyRect = r;
        layerDirtyRect.move(-m_scrollingContentsLayer->offsetFromRenderer());
        m_scrollingContentsLayer->setNeedsDisplayInRect(layerDirtyRect);
    }
}

void RenderLayerBacking::doPaintTask(GraphicsLayerPaintInfo& paintInfo, GraphicsContext* context,
    const IntRect& clip) // In the coords of rootLayer.
{
    if (paintsIntoCompositedAncestor()) {
        ASSERT_NOT_REACHED();
        return;
    }

    FontCachePurgePreventer fontCachePurgePreventer;

    RenderLayer::PaintLayerFlags paintFlags = 0;
    if (paintInfo.paintingPhase & GraphicsLayerPaintBackground)
        paintFlags |= RenderLayer::PaintLayerPaintingCompositingBackgroundPhase;
    if (paintInfo.paintingPhase & GraphicsLayerPaintForeground)
        paintFlags |= RenderLayer::PaintLayerPaintingCompositingForegroundPhase;
    if (paintInfo.paintingPhase & GraphicsLayerPaintMask)
        paintFlags |= RenderLayer::PaintLayerPaintingCompositingMaskPhase;
    if (paintInfo.paintingPhase & GraphicsLayerPaintOverflowContents)
        paintFlags |= RenderLayer::PaintLayerPaintingOverflowContents;
    if (paintInfo.paintingPhase & GraphicsLayerPaintCompositedScroll)
        paintFlags |= RenderLayer::PaintLayerPaintingCompositingScrollingPhase;

    if (paintInfo.isBackgroundLayer)
        paintFlags |= (RenderLayer::PaintLayerPaintingRootBackgroundOnly | RenderLayer::PaintLayerPaintingCompositingForegroundPhase); // Need PaintLayerPaintingCompositingForegroundPhase to walk child layers.
    else if (compositor()->fixedRootBackgroundLayer())
        paintFlags |= RenderLayer::PaintLayerPaintingSkipRootBackground;

    InspectorInstrumentation::willPaint(paintInfo.renderLayer->renderer());

    // Note carefully: in theory it is appropriate to invoke context->save() here
    // and restore the context after painting. For efficiency, we are assuming that
    // it is equivalent to manually undo this offset translation, which means we are
    // assuming that the context's space was not affected by the RenderLayer
    // painting code.

    LayoutSize offset = paintInfo.offsetFromRenderer;
    context->translate(-offset);
    LayoutRect relativeClip(clip);
    relativeClip.move(offset);

    // The dirtyRect is in the coords of the painting root.
    IntRect dirtyRect = pixelSnappedIntRect(relativeClip);
    if (!(paintInfo.paintingPhase & GraphicsLayerPaintOverflowContents))
        dirtyRect.intersect(paintInfo.compositedBounds);

#ifndef NDEBUG
    paintInfo.renderLayer->renderer()->assertSubtreeIsLaidOut();
#endif

    // FIXME: GraphicsLayers need a way to split for RenderRegions.
    RenderLayer::LayerPaintingInfo paintingInfo(paintInfo.renderLayer, dirtyRect, PaintBehaviorNormal, LayoutSize());
    paintInfo.renderLayer->paintLayerContents(context, paintingInfo, paintFlags);

    ASSERT(!paintInfo.isBackgroundLayer || paintFlags & RenderLayer::PaintLayerPaintingRootBackgroundOnly);

    if (paintInfo.renderLayer->containsDirtyOverlayScrollbars())
        paintInfo.renderLayer->paintLayerContents(context, paintingInfo, paintFlags | RenderLayer::PaintLayerPaintingOverlayScrollbars);

    ASSERT(!paintInfo.renderLayer->m_usedTransparency);

    // Manually restore the context to its original state by applying the opposite translation.
    context->translate(offset);

    InspectorInstrumentation::didPaint(paintInfo.renderLayer->renderer(), context, clip);
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
void RenderLayerBacking::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& context, GraphicsLayerPaintingPhase paintingPhase, const IntRect& clip)
{
#ifndef NDEBUG
    if (Page* page = renderer()->frame()->page())
        page->setIsPainting(true);
#endif

    if (graphicsLayer == m_graphicsLayer.get()
        || graphicsLayer == m_foregroundLayer.get()
        || graphicsLayer == m_backgroundLayer.get()
        || graphicsLayer == m_maskLayer.get()
        || graphicsLayer == m_scrollingContentsLayer.get()) {

        GraphicsLayerPaintInfo paintInfo;
        paintInfo.renderLayer = m_owningLayer;
        paintInfo.compositedBounds = compositedBounds();
        paintInfo.offsetFromRenderer = graphicsLayer->offsetFromRenderer();
        paintInfo.paintingPhase = paintingPhase;
        paintInfo.isBackgroundLayer = (graphicsLayer == m_backgroundLayer);

        // We have to use the same root as for hit testing, because both methods can compute and cache clipRects.
        doPaintTask(paintInfo, &context, clip);
    } else if (graphicsLayer == layerForHorizontalScrollbar()) {
        paintScrollbar(m_owningLayer->horizontalScrollbar(), context, clip);
    } else if (graphicsLayer == layerForVerticalScrollbar()) {
        paintScrollbar(m_owningLayer->verticalScrollbar(), context, clip);
    } else if (graphicsLayer == layerForScrollCorner()) {
        const IntRect& scrollCornerAndResizer = m_owningLayer->scrollCornerAndResizerRect();
        context.save();
        context.translate(-scrollCornerAndResizer.x(), -scrollCornerAndResizer.y());
        IntRect transformedClip = clip;
        transformedClip.moveBy(scrollCornerAndResizer.location());
        m_owningLayer->paintScrollCorner(&context, IntPoint(), transformedClip);
        m_owningLayer->paintResizer(&context, IntPoint(), transformedClip);
        context.restore();
    }
#ifndef NDEBUG
    if (Page* page = renderer()->frame()->page())
        page->setIsPainting(false);
#endif
}

void RenderLayerBacking::didCommitChangesForLayer(const GraphicsLayer* layer) const
{
}

bool RenderLayerBacking::getCurrentTransform(const GraphicsLayer* graphicsLayer, TransformationMatrix& transform) const
{
    if (graphicsLayer != m_graphicsLayer.get())
        return false;

    if (m_owningLayer->hasTransform()) {
        transform = m_owningLayer->currentTransform(RenderStyle::ExcludeTransformOrigin);
        return true;
    }
    return false;
}

bool RenderLayerBacking::isTrackingRepaints() const
{
    GraphicsLayerClient* client = compositor();
    return client ? client->isTrackingRepaints() : false;
}

#ifndef NDEBUG
void RenderLayerBacking::verifyNotPainting()
{
    ASSERT(!renderer()->frame()->page() || !renderer()->frame()->page()->isPainting());
}
#endif

bool RenderLayerBacking::startAnimation(double timeOffset, const CSSAnimationData* anim, const KeyframeList& keyframes)
{
    bool hasOpacity = keyframes.containsProperty(CSSPropertyOpacity);
    bool hasTransform = renderer()->isBox() && keyframes.containsProperty(CSSPropertyWebkitTransform);
    bool hasFilter = keyframes.containsProperty(CSSPropertyWebkitFilter);

    if (!hasOpacity && !hasTransform && !hasFilter)
        return false;

    KeyframeValueList transformVector(AnimatedPropertyWebkitTransform);
    KeyframeValueList opacityVector(AnimatedPropertyOpacity);
    KeyframeValueList filterVector(AnimatedPropertyWebkitFilter);

    size_t numKeyframes = keyframes.size();
    for (size_t i = 0; i < numKeyframes; ++i) {
        const KeyframeValue& currentKeyframe = keyframes[i];
        const RenderStyle* keyframeStyle = currentKeyframe.style();
        double key = currentKeyframe.key();

        if (!keyframeStyle)
            continue;

        // Get timing function.
        RefPtr<TimingFunction> tf = currentKeyframe.timingFunction(keyframes.animationName());

        bool isFirstOrLastKeyframe = key == 0 || key == 1;
        if ((hasTransform && isFirstOrLastKeyframe) || currentKeyframe.containsProperty(CSSPropertyWebkitTransform))
            transformVector.insert(adoptPtr(new TransformAnimationValue(key, &(keyframeStyle->transform()), tf)));

        if ((hasOpacity && isFirstOrLastKeyframe) || currentKeyframe.containsProperty(CSSPropertyOpacity))
            opacityVector.insert(adoptPtr(new FloatAnimationValue(key, keyframeStyle->opacity(), tf)));

        if ((hasFilter && isFirstOrLastKeyframe) || currentKeyframe.containsProperty(CSSPropertyWebkitFilter))
            filterVector.insert(adoptPtr(new FilterAnimationValue(key, &(keyframeStyle->filter()), tf)));
    }

    bool didAnimate = false;

    if (hasTransform && m_graphicsLayer->addAnimation(transformVector, toRenderBox(renderer())->pixelSnappedBorderBoxRect().size(), anim, keyframes.animationName(), timeOffset))
        didAnimate = true;

    if (hasOpacity && m_graphicsLayer->addAnimation(opacityVector, IntSize(), anim, keyframes.animationName(), timeOffset))
        didAnimate = true;

    if (hasFilter && m_graphicsLayer->addAnimation(filterVector, IntSize(), anim, keyframes.animationName(), timeOffset))
        didAnimate = true;

    return didAnimate;
}

void RenderLayerBacking::animationPaused(double timeOffset, const String& animationName)
{
    m_graphicsLayer->pauseAnimation(animationName, timeOffset);
}

void RenderLayerBacking::animationFinished(const String& animationName)
{
    m_graphicsLayer->removeAnimation(animationName);
}

bool RenderLayerBacking::startTransition(double timeOffset, CSSPropertyID property, const RenderStyle* fromStyle, const RenderStyle* toStyle)
{
    bool didAnimate = false;

    ASSERT(property != CSSPropertyInvalid);

    if (property == CSSPropertyOpacity) {
        const CSSAnimationData* opacityAnim = toStyle->transitionForProperty(CSSPropertyOpacity);
        if (opacityAnim && !opacityAnim->isEmptyOrZeroDuration()) {
            KeyframeValueList opacityVector(AnimatedPropertyOpacity);
            opacityVector.insert(adoptPtr(new FloatAnimationValue(0, compositingOpacity(fromStyle->opacity()))));
            opacityVector.insert(adoptPtr(new FloatAnimationValue(1, compositingOpacity(toStyle->opacity()))));
            // The boxSize param is only used for transform animations (which can only run on RenderBoxes), so we pass an empty size here.
            if (m_graphicsLayer->addAnimation(opacityVector, IntSize(), opacityAnim, GraphicsLayer::animationNameForTransition(AnimatedPropertyOpacity), timeOffset)) {
                // To ensure that the correct opacity is visible when the animation ends, also set the final opacity.
                updateOpacity(toStyle);
                didAnimate = true;
            }
        }
    }

    if (property == CSSPropertyWebkitTransform && m_owningLayer->hasTransform()) {
        const CSSAnimationData* transformAnim = toStyle->transitionForProperty(CSSPropertyWebkitTransform);
        if (transformAnim && !transformAnim->isEmptyOrZeroDuration()) {
            KeyframeValueList transformVector(AnimatedPropertyWebkitTransform);
            transformVector.insert(adoptPtr(new TransformAnimationValue(0, &fromStyle->transform())));
            transformVector.insert(adoptPtr(new TransformAnimationValue(1, &toStyle->transform())));
            if (m_graphicsLayer->addAnimation(transformVector, toRenderBox(renderer())->pixelSnappedBorderBoxRect().size(), transformAnim, GraphicsLayer::animationNameForTransition(AnimatedPropertyWebkitTransform), timeOffset)) {
                // To ensure that the correct transform is visible when the animation ends, also set the final transform.
                updateTransform(toStyle);
                didAnimate = true;
            }
        }
    }

    if (property == CSSPropertyWebkitFilter && m_owningLayer->hasFilter()) {
        const CSSAnimationData* filterAnim = toStyle->transitionForProperty(CSSPropertyWebkitFilter);
        if (filterAnim && !filterAnim->isEmptyOrZeroDuration()) {
            KeyframeValueList filterVector(AnimatedPropertyWebkitFilter);
            filterVector.insert(adoptPtr(new FilterAnimationValue(0, &fromStyle->filter())));
            filterVector.insert(adoptPtr(new FilterAnimationValue(1, &toStyle->filter())));
            if (m_graphicsLayer->addAnimation(filterVector, IntSize(), filterAnim, GraphicsLayer::animationNameForTransition(AnimatedPropertyWebkitFilter), timeOffset)) {
                // To ensure that the correct filter is visible when the animation ends, also set the final filter.
                updateFilters(toStyle);
                didAnimate = true;
            }
        }
    }

    return didAnimate;
}

void RenderLayerBacking::transitionPaused(double timeOffset, CSSPropertyID property)
{
    AnimatedPropertyID animatedProperty = cssToGraphicsLayerProperty(property);
    if (animatedProperty != AnimatedPropertyInvalid)
        m_graphicsLayer->pauseAnimation(GraphicsLayer::animationNameForTransition(animatedProperty), timeOffset);
}

void RenderLayerBacking::transitionFinished(CSSPropertyID property)
{
    AnimatedPropertyID animatedProperty = cssToGraphicsLayerProperty(property);
    if (animatedProperty != AnimatedPropertyInvalid)
        m_graphicsLayer->removeAnimation(GraphicsLayer::animationNameForTransition(animatedProperty));
}

void RenderLayerBacking::notifyAnimationStarted(const GraphicsLayer*, double time)
{
    renderer()->animation()->notifyAnimationStarted(renderer(), time);
}

// This is used for the 'freeze' API, for testing only.
void RenderLayerBacking::suspendAnimations(double time)
{
    m_graphicsLayer->suspendAnimations(time);
}

void RenderLayerBacking::resumeAnimations()
{
    m_graphicsLayer->resumeAnimations();
}

IntRect RenderLayerBacking::compositedBounds() const
{
    return m_compositedBounds;
}

void RenderLayerBacking::setCompositedBounds(const IntRect& bounds)
{
    m_compositedBounds = bounds;
}

CSSPropertyID RenderLayerBacking::graphicsLayerToCSSProperty(AnimatedPropertyID property)
{
    CSSPropertyID cssProperty = CSSPropertyInvalid;
    switch (property) {
        case AnimatedPropertyWebkitTransform:
            cssProperty = CSSPropertyWebkitTransform;
            break;
        case AnimatedPropertyOpacity:
            cssProperty = CSSPropertyOpacity;
            break;
        case AnimatedPropertyBackgroundColor:
            cssProperty = CSSPropertyBackgroundColor;
            break;
        case AnimatedPropertyWebkitFilter:
            cssProperty = CSSPropertyWebkitFilter;
            break;
        case AnimatedPropertyInvalid:
            ASSERT_NOT_REACHED();
    }
    return cssProperty;
}

AnimatedPropertyID RenderLayerBacking::cssToGraphicsLayerProperty(CSSPropertyID cssProperty)
{
    switch (cssProperty) {
        case CSSPropertyWebkitTransform:
            return AnimatedPropertyWebkitTransform;
        case CSSPropertyOpacity:
            return AnimatedPropertyOpacity;
        case CSSPropertyBackgroundColor:
            return AnimatedPropertyBackgroundColor;
        case CSSPropertyWebkitFilter:
            return AnimatedPropertyWebkitFilter;
        default:
            // It's fine if we see other css properties here; they are just not accelerated.
            break;
    }
    return AnimatedPropertyInvalid;
}

CompositingLayerType RenderLayerBacking::compositingLayerType() const
{
    if (m_graphicsLayer->hasContentsLayer())
        return MediaCompositingLayer;

    if (m_graphicsLayer->drawsContent())
        return NormalCompositingLayer;

    return ContainerCompositingLayer;
}

double RenderLayerBacking::backingStoreMemoryEstimate() const
{
    double backingMemory;

    // m_ancestorClippingLayer and m_childContainmentLayer are just used for masking or containment, so have no backing.
    backingMemory = m_graphicsLayer->backingStoreMemoryEstimate();
    if (m_foregroundLayer)
        backingMemory += m_foregroundLayer->backingStoreMemoryEstimate();
    if (m_backgroundLayer)
        backingMemory += m_backgroundLayer->backingStoreMemoryEstimate();
    if (m_maskLayer)
        backingMemory += m_maskLayer->backingStoreMemoryEstimate();

    if (m_scrollingContentsLayer)
        backingMemory += m_scrollingContentsLayer->backingStoreMemoryEstimate();

    if (m_layerForHorizontalScrollbar)
        backingMemory += m_layerForHorizontalScrollbar->backingStoreMemoryEstimate();

    if (m_layerForVerticalScrollbar)
        backingMemory += m_layerForVerticalScrollbar->backingStoreMemoryEstimate();

    if (m_layerForScrollCorner)
        backingMemory += m_layerForScrollCorner->backingStoreMemoryEstimate();

    return backingMemory;
}

String RenderLayerBacking::debugName(const GraphicsLayer* graphicsLayer)
{
    String name;
    if (graphicsLayer == m_graphicsLayer.get()) {
        name = m_owningLayer->debugName();
    } else if (graphicsLayer == m_ancestorScrollClippingLayer.get()) {
        name = "Ancestor Scroll Clipping Layer";
    } else if (graphicsLayer == m_ancestorClippingLayer.get()) {
        name = "Ancestor Clipping Layer";
    } else if (graphicsLayer == m_foregroundLayer.get()) {
        name = m_owningLayer->debugName() + " (foreground) Layer";
    } else if (graphicsLayer == m_backgroundLayer.get()) {
        name = m_owningLayer->debugName() + " (background) Layer";
    } else if (graphicsLayer == m_childContainmentLayer.get()) {
        name = "Child Containment Layer";
    } else if (graphicsLayer == m_maskLayer.get()) {
        name = "Mask Layer";
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
