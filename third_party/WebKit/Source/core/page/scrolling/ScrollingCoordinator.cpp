/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/page/scrolling/ScrollingCoordinator.h"

#include "RuntimeEnabledFeatures.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/dom/WheelController.h"
#include "core/html/HTMLElement.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/PlatformWheelEvent.h"
#include "core/platform/ScrollAnimator.h"
#include "core/platform/ScrollbarTheme.h"
#include "core/platform/chromium/TraceEvent.h"
#include "core/platform/chromium/support/WebScrollbarImpl.h"
#include "core/platform/chromium/support/WebScrollbarThemeGeometryNative.h"
#include "core/platform/graphics/GraphicsLayer.h"
#include "core/platform/graphics/IntRect.h"
#include "core/platform/graphics/Region.h"
#include "core/platform/graphics/transforms/TransformState.h"
#if OS(MACOSX)
#include "core/platform/mac/ScrollAnimatorMac.h"
#endif
#include "core/plugins/PluginView.h"
#include "core/rendering/RenderGeometryMap.h"
#include "core/rendering/RenderLayerBacking.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderView.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayerPositionConstraint.h"
#include "public/platform/WebScrollbarLayer.h"
#include "public/platform/WebScrollbarThemeGeometry.h"
#include "public/platform/WebScrollbarThemePainter.h"
#include "wtf/text/StringBuilder.h"

using WebKit::WebLayer;
using WebKit::WebLayerPositionConstraint;
using WebKit::WebRect;
using WebKit::WebScrollbarLayer;
using WebKit::WebVector;


namespace WebCore {

static WebLayer* scrollingWebLayerForGraphicsLayer(GraphicsLayer* layer)
{
    return layer->platformLayer();
}

WebLayer* ScrollingCoordinator::scrollingWebLayerForScrollableArea(ScrollableArea* scrollableArea)
{
    GraphicsLayer* graphicsLayer = scrollLayerForScrollableArea(scrollableArea);
    return graphicsLayer ? scrollingWebLayerForGraphicsLayer(graphicsLayer) : 0;
}

PassRefPtr<ScrollingCoordinator> ScrollingCoordinator::create(Page* page)
{
    return adoptRef(new ScrollingCoordinator(page));
}

ScrollingCoordinator::ScrollingCoordinator(Page* page)
    : m_page(page)
{
}

ScrollingCoordinator::~ScrollingCoordinator()
{
    ASSERT(!m_page);
    for (ScrollbarMap::iterator it = m_horizontalScrollbars.begin(); it != m_horizontalScrollbars.end(); ++it)
        GraphicsLayer::unregisterContentsLayer(it->value->layer());
    for (ScrollbarMap::iterator it = m_verticalScrollbars.begin(); it != m_verticalScrollbars.end(); ++it)
        GraphicsLayer::unregisterContentsLayer(it->value->layer());

}

bool ScrollingCoordinator::touchHitTestingEnabled() const
{
    RenderView* contentRenderer = m_page->mainFrame()->contentRenderer();
    return RuntimeEnabledFeatures::touchEnabled() && contentRenderer && contentRenderer->usesCompositing();
}

void ScrollingCoordinator::setShouldHandleScrollGestureOnMainThreadRegion(const Region& region)
{
    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(m_page->mainFrame()->view())) {
        Vector<IntRect> rects = region.rects();
        WebVector<WebRect> webRects(rects.size());
        for (size_t i = 0; i < rects.size(); ++i)
            webRects[i] = rects[i];
        scrollLayer->setNonFastScrollableRegion(webRects);
    }
}

void ScrollingCoordinator::frameViewLayoutUpdated(FrameView* frameView)
{
    TRACE_EVENT0("input", "ScrollingCoordinator::frameViewLayoutUpdated");

    // Compute the region of the page where we can't handle scroll gestures and mousewheel events
    // on the impl thread. This currently includes:
    // 1. All scrollable areas, such as subframes, overflow divs and list boxes, whose composited
    // scrolling are not enabled. We need to do this even if the frame view whose layout was updated
    // is not the main frame.
    // 2. Resize control areas, e.g. the small rect at the right bottom of div/textarea/iframe when
    // CSS property "resize" is enabled.
    // 3. Plugin areas.
    Region shouldHandleScrollGestureOnMainThreadRegion = computeShouldHandleScrollGestureOnMainThreadRegion(m_page->mainFrame(), IntPoint());
    setShouldHandleScrollGestureOnMainThreadRegion(shouldHandleScrollGestureOnMainThreadRegion);

    if (touchHitTestingEnabled()) {
        LayerHitTestRects touchEventTargetRects;
        computeTouchEventTargetRects(touchEventTargetRects);
        setTouchEventTargetRects(touchEventTargetRects);
    }

    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(frameView))
        scrollLayer->setBounds(frameView->contentsSize());
}

void ScrollingCoordinator::setLayerIsContainerForFixedPositionLayers(GraphicsLayer* layer, bool enable)
{
    if (WebLayer* scrollableLayer = scrollingWebLayerForGraphicsLayer(layer))
        scrollableLayer->setIsContainerForFixedPositionLayers(enable);
}

static void clearPositionConstraintExceptForLayer(GraphicsLayer* layer, GraphicsLayer* except)
{
    if (layer && layer != except && scrollingWebLayerForGraphicsLayer(layer))
        scrollingWebLayerForGraphicsLayer(layer)->setPositionConstraint(WebLayerPositionConstraint());
}

static WebLayerPositionConstraint computePositionConstraint(const RenderLayer* layer)
{
    ASSERT(layer->isComposited());
    do {
        if (layer->renderer()->style()->position() == FixedPosition) {
            const RenderObject* fixedPositionObject = layer->renderer();
            bool fixedToRight = !fixedPositionObject->style()->right().isAuto();
            bool fixedToBottom = !fixedPositionObject->style()->bottom().isAuto();
            return WebLayerPositionConstraint::fixedPosition(fixedToRight, fixedToBottom);
        }

        layer = layer->parent();
    } while (layer && !layer->isComposited());
    return WebLayerPositionConstraint();
}

void ScrollingCoordinator::updateLayerPositionConstraint(RenderLayer* layer)
{
    ASSERT(layer->backing());
    RenderLayerBacking* backing = layer->backing();
    GraphicsLayer* mainLayer = backing->childForSuperlayers();

    // Avoid unnecessary commits
    clearPositionConstraintExceptForLayer(backing->ancestorClippingLayer(), mainLayer);
    clearPositionConstraintExceptForLayer(backing->graphicsLayer(), mainLayer);

    if (WebLayer* scrollableLayer = scrollingWebLayerForGraphicsLayer(mainLayer))
        scrollableLayer->setPositionConstraint(computePositionConstraint(layer));
}

void ScrollingCoordinator::willDestroyScrollableArea(ScrollableArea* scrollableArea)
{
    removeWebScrollbarLayer(scrollableArea, HorizontalScrollbar);
    removeWebScrollbarLayer(scrollableArea, VerticalScrollbar);
}

void ScrollingCoordinator::removeWebScrollbarLayer(ScrollableArea* scrollableArea, ScrollbarOrientation orientation)
{
    ScrollbarMap& scrollbars = orientation == HorizontalScrollbar ? m_horizontalScrollbars : m_verticalScrollbars;
    if (OwnPtr<WebScrollbarLayer> scrollbarLayer = scrollbars.take(scrollableArea))
        GraphicsLayer::unregisterContentsLayer(scrollbarLayer->layer());
}

static PassOwnPtr<WebScrollbarLayer> createScrollbarLayer(Scrollbar* scrollbar)
{
    ScrollbarTheme* theme = scrollbar->theme();
    WebKit::WebScrollbarThemePainter painter(theme, scrollbar);
    OwnPtr<WebKit::WebScrollbarThemeGeometry> geometry(WebKit::WebScrollbarThemeGeometryNative::create(theme));

    OwnPtr<WebScrollbarLayer> scrollbarLayer = adoptPtr(WebKit::Platform::current()->compositorSupport()->createScrollbarLayer(new WebKit::WebScrollbarImpl(scrollbar), painter, geometry.leakPtr()));
    GraphicsLayer::registerContentsLayer(scrollbarLayer->layer());
    return scrollbarLayer.release();
}

PassOwnPtr<WebScrollbarLayer> ScrollingCoordinator::createSolidColorScrollbarLayer(ScrollbarOrientation orientation, int thumbThickness, bool isLeftSideVerticalScrollbar)
{
    WebKit::WebScrollbar::Orientation webOrientation = (orientation == HorizontalScrollbar) ? WebKit::WebScrollbar::Horizontal : WebKit::WebScrollbar::Vertical;
    OwnPtr<WebScrollbarLayer> scrollbarLayer = adoptPtr(WebKit::Platform::current()->compositorSupport()->createSolidColorScrollbarLayer(webOrientation, thumbThickness, isLeftSideVerticalScrollbar));
    GraphicsLayer::registerContentsLayer(scrollbarLayer->layer());
    return scrollbarLayer.release();
}

static void detachScrollbarLayer(GraphicsLayer* scrollbarGraphicsLayer)
{
    ASSERT(scrollbarGraphicsLayer);

    scrollbarGraphicsLayer->setContentsToPlatformLayer(0);
    scrollbarGraphicsLayer->setDrawsContent(true);
}

static void setupScrollbarLayer(GraphicsLayer* scrollbarGraphicsLayer, WebScrollbarLayer* scrollbarLayer, WebLayer* scrollLayer)
{
    ASSERT(scrollbarGraphicsLayer);
    ASSERT(scrollbarLayer);

    if (!scrollLayer) {
        detachScrollbarLayer(scrollbarGraphicsLayer);
        return;
    }
    scrollbarLayer->setScrollLayer(scrollLayer);
    scrollbarGraphicsLayer->setContentsToPlatformLayer(scrollbarLayer->layer());
    scrollbarGraphicsLayer->setDrawsContent(false);
}

WebScrollbarLayer* ScrollingCoordinator::addWebScrollbarLayer(ScrollableArea* scrollableArea, ScrollbarOrientation orientation, PassOwnPtr<WebKit::WebScrollbarLayer> scrollbarLayer)
{
    ScrollbarMap& scrollbars = orientation == HorizontalScrollbar ? m_horizontalScrollbars : m_verticalScrollbars;
    return scrollbars.add(scrollableArea, scrollbarLayer).iterator->value.get();
}

WebScrollbarLayer* ScrollingCoordinator::getWebScrollbarLayer(ScrollableArea* scrollableArea, ScrollbarOrientation orientation)
{
    ScrollbarMap& scrollbars = orientation == HorizontalScrollbar ? m_horizontalScrollbars : m_verticalScrollbars;
    return scrollbars.get(scrollableArea);
}

void ScrollingCoordinator::scrollableAreaScrollbarLayerDidChange(ScrollableArea* scrollableArea, ScrollbarOrientation orientation)
{
// FIXME: Instead of hardcode here, we should make a setting flag.
#if OS(MACOSX)
    static const bool platformSupportsCoordinatedScrollbar = ScrollAnimatorMac::canUseCoordinatedScrollbar();
    static const bool platformSupportsMainFrameOnly = false; // Don't care.
#elif OS(ANDROID)
    static const bool platformSupportsCoordinatedScrollbar = true;
    static const bool platformSupportsMainFrameOnly = false;
#else
    static const bool platformSupportsCoordinatedScrollbar = true;
    static const bool platformSupportsMainFrameOnly = true;
#endif
    if (!platformSupportsCoordinatedScrollbar)
        return;

    bool isMainFrame = isForMainFrame(scrollableArea);
    if (!isMainFrame && platformSupportsMainFrameOnly)
        return;

    GraphicsLayer* scrollbarGraphicsLayer = orientation == HorizontalScrollbar ? horizontalScrollbarLayerForScrollableArea(scrollableArea) : verticalScrollbarLayerForScrollableArea(scrollableArea);
    if (scrollbarGraphicsLayer) {
        Scrollbar* scrollbar = orientation == HorizontalScrollbar ? scrollableArea->horizontalScrollbar() : scrollableArea->verticalScrollbar();
        if (scrollbar->isCustomScrollbar()) {
            detachScrollbarLayer(scrollbarGraphicsLayer);
            return;
        }

        WebScrollbarLayer* scrollbarLayer = getWebScrollbarLayer(scrollableArea, orientation);
        if (!scrollbarLayer) {
            Settings* settings = m_page->mainFrame()->document()->settings();

            OwnPtr<WebScrollbarLayer> webScrollbarLayer;
            if (settings->useSolidColorScrollbars()) {
                ASSERT(RuntimeEnabledFeatures::overlayScrollbarsEnabled());
                webScrollbarLayer = createSolidColorScrollbarLayer(orientation, -1, scrollableArea->shouldPlaceVerticalScrollbarOnLeft());
            } else {
                webScrollbarLayer = createScrollbarLayer(scrollbar);
            }
            scrollbarLayer = addWebScrollbarLayer(scrollableArea, orientation, webScrollbarLayer.release());
        }

        // Root layer non-overlay scrollbars should be marked opaque to disable
        // blending.
        bool isOpaqueScrollbar = !scrollbar->isOverlayScrollbar();
        if (!scrollbarGraphicsLayer->contentsOpaque())
            scrollbarGraphicsLayer->setContentsOpaque(isMainFrame && isOpaqueScrollbar);
        scrollbarLayer->layer()->setOpaque(scrollbarGraphicsLayer->contentsOpaque());

        setupScrollbarLayer(scrollbarGraphicsLayer, scrollbarLayer, scrollingWebLayerForScrollableArea(scrollableArea));
    } else
        removeWebScrollbarLayer(scrollableArea, orientation);
}

bool ScrollingCoordinator::scrollableAreaScrollLayerDidChange(ScrollableArea* scrollableArea)
{
    GraphicsLayer* scrollLayer = scrollLayerForScrollableArea(scrollableArea);
    if (scrollLayer) {
        bool isMainFrame = isForMainFrame(scrollableArea);
        scrollLayer->setScrollableArea(scrollableArea, isMainFrame);
    }

    WebLayer* webLayer = scrollingWebLayerForScrollableArea(scrollableArea);
    if (webLayer) {
        webLayer->setScrollable(true);
        webLayer->setScrollPosition(IntPoint(scrollableArea->scrollPosition() - scrollableArea->minimumScrollPosition()));
        webLayer->setMaxScrollPosition(IntSize(scrollableArea->scrollSize(HorizontalScrollbar), scrollableArea->scrollSize(VerticalScrollbar)));
    }
    if (WebScrollbarLayer* scrollbarLayer = getWebScrollbarLayer(scrollableArea, HorizontalScrollbar)) {
        GraphicsLayer* horizontalScrollbarLayer = horizontalScrollbarLayerForScrollableArea(scrollableArea);
        if (horizontalScrollbarLayer)
            setupScrollbarLayer(horizontalScrollbarLayer, scrollbarLayer, webLayer);
    }
    if (WebScrollbarLayer* scrollbarLayer = getWebScrollbarLayer(scrollableArea, VerticalScrollbar)) {
        GraphicsLayer* verticalScrollbarLayer = verticalScrollbarLayerForScrollableArea(scrollableArea);
        if (verticalScrollbarLayer)
            setupScrollbarLayer(verticalScrollbarLayer, scrollbarLayer, webLayer);
    }

    return !!webLayer;
}

// In order to do a DFS cross-frame walk of the RenderLayer tree, we need to know which
// RenderLayers have child frames inside of them. This computes a mapping for the
// current frame which we can consult while walking the layers of that frame.
// Whenever we descend into a new frame, a new map will be created.
typedef HashMap<const RenderLayer*, Vector<const Frame*> > LayerFrameMap;
static void makeLayerChildFrameMap(const Frame* currentFrame, LayerFrameMap* map)
{
    map->clear();
    const FrameTree* tree = currentFrame->tree();
    for (const Frame* child = tree->firstChild(); child; child = child->tree()->nextSibling()) {
        const RenderLayer* containingLayer = child->ownerRenderer()->enclosingLayer();
        LayerFrameMap::iterator iter = map->find(containingLayer);
        if (iter == map->end())
            iter = map->add(containingLayer, Vector<const Frame*>()).iterator;
        iter->value.append(child);
    }
}

static void convertLayerRectsToEnclosingCompositedLayerRecursive(
    const RenderLayer* curLayer,
    const LayerHitTestRects& layerRects,
    LayerHitTestRects& compositorRects,
    RenderGeometryMap& geometryMap,
    HashSet<const RenderLayer*>& layersWithRects,
    LayerFrameMap& layerChildFrameMap)
{
    // Project any rects for the current layer
    LayerHitTestRects::const_iterator layerIter = layerRects.find(curLayer);
    if (layerIter != layerRects.end()) {
        // Find the enclosing composited layer when it's in another document (for non-composited iframes).
        RenderLayer* compositedLayer = 0;
        for (const RenderLayer* layer = layerIter->key; !compositedLayer;) {
            compositedLayer = layer->enclosingCompositingLayerForRepaint();
            if (!compositedLayer) {
                RenderObject* owner = layer->renderer()->frame()->ownerRenderer();
                if (!owner)
                    break;
                layer = owner->enclosingLayer();
            }
        }
        if (!compositedLayer) {
            // Since this machinery is used only when accelerated compositing is enabled, we expect
            // that every layer should have an enclosing composited layer.
            ASSERT_NOT_REACHED();
            return;
        }

        LayerHitTestRects::iterator compIter = compositorRects.find(compositedLayer);
        if (compIter == compositorRects.end())
            compIter = compositorRects.add(compositedLayer, Vector<LayoutRect>()).iterator;
        // Transform each rect to the co-ordinate space of it's enclosing composited layer.
        for (size_t i = 0; i < layerIter->value.size(); ++i) {
            LayoutRect rect = layerIter->value[i];
            if (compositedLayer != curLayer) {
                FloatQuad compositorQuad = geometryMap.mapToContainer(rect, compositedLayer->renderer());
                rect = LayoutRect(compositorQuad.boundingBox());
            }
            compIter->value.append(rect);
        }
    }

    // Walk child layers of interest
    for (const RenderLayer* childLayer = curLayer->firstChild(); childLayer; childLayer = childLayer->nextSibling()) {
        if (layersWithRects.contains(childLayer)) {
            geometryMap.pushMappingsToAncestor(childLayer, curLayer);
            convertLayerRectsToEnclosingCompositedLayerRecursive(childLayer, layerRects, compositorRects, geometryMap, layersWithRects, layerChildFrameMap);
            geometryMap.popMappingsToAncestor(curLayer);
        }
    }

    // If this layer has any frames of interest as a child of it, walk those (with an updated frame map).
    LayerFrameMap::iterator mapIter = layerChildFrameMap.find(curLayer);
    if (mapIter != layerChildFrameMap.end()) {
        for (size_t i = 0; i < mapIter->value.size(); i++) {
            const Frame* childFrame = mapIter->value[i];
            const RenderLayer* childLayer = childFrame->view()->renderView()->layer();
            if (layersWithRects.contains(childLayer)) {
                LayerFrameMap newLayerChildFrameMap;
                makeLayerChildFrameMap(childFrame, &newLayerChildFrameMap);
                geometryMap.pushMappingsToAncestor(childLayer, curLayer);
                convertLayerRectsToEnclosingCompositedLayerRecursive(childLayer, layerRects, compositorRects, geometryMap, layersWithRects, newLayerChildFrameMap);
                geometryMap.popMappingsToAncestor(curLayer);
            }
        }
    }
}

static void convertLayerRectsToEnclosingCompositedLayer(Frame* mainFrame, const LayerHitTestRects& layerRects, LayerHitTestRects& compositorRects)
{
    TRACE_EVENT0("input", "ScrollingCoordinator::convertLayerRectsToEnclosingCompositedLayer");
    bool touchHandlerInChildFrame = false;

    // We have a set of rects per RenderLayer, we need to map them to their bounding boxes in their
    // enclosing composited layer. To do this most efficiently we'll walk the RenderLayer tree using
    // RenderGeometryMap. First record all the branches we should traverse in the tree (including
    // all documents on the page).
    HashSet<const RenderLayer*> layersWithRects;
    for (LayerHitTestRects::const_iterator layerIter = layerRects.begin(); layerIter != layerRects.end(); ++layerIter) {
        const RenderLayer* layer = layerIter->key;
        do {
            if (!layersWithRects.add(layer).isNewEntry)
                break;

            if (layer->parent()) {
                layer = layer->parent();
            } else if (RenderObject* parentDocRenderer = layer->renderer()->frame()->ownerRenderer()) {
                layer = parentDocRenderer->enclosingLayer();
                touchHandlerInChildFrame = true;
            }
        } while (layer);
    }

    // Now walk the layer projecting rects while maintaining a RenderGeometryMap
    MapCoordinatesFlags flags = UseTransforms;
    if (touchHandlerInChildFrame)
        flags |= TraverseDocumentBoundaries;
    RenderGeometryMap geometryMap(flags);
    LayerFrameMap layerChildFrameMap;
    makeLayerChildFrameMap(mainFrame, &layerChildFrameMap);
    convertLayerRectsToEnclosingCompositedLayerRecursive(mainFrame->contentRenderer()->layer(), layerRects, compositorRects, geometryMap, layersWithRects, layerChildFrameMap);
}

// Note that in principle this could be called more often than computeTouchEventTargetRects, for
// example during a non-composited scroll (although that's not yet implemented - crbug.com/261307).
void ScrollingCoordinator::setTouchEventTargetRects(const LayerHitTestRects& layerRects)
{
    TRACE_EVENT0("input", "ScrollingCoordinator::setTouchEventTargetRects");

    LayerHitTestRects compositorRects;
    convertLayerRectsToEnclosingCompositedLayer(m_page->mainFrame(), layerRects, compositorRects);

    HashSet<const RenderLayer*> oldLayersWithTouchRects;
    m_layersWithTouchRects.swap(oldLayersWithTouchRects);

    for (LayerHitTestRects::const_iterator iter = compositorRects.begin(); iter != compositorRects.end(); ++iter) {
        const RenderLayer* layer = iter->key;
        WebVector<WebRect> webRects(iter->value.size());
        for (size_t i = 0; i < iter->value.size(); ++i)
            webRects[i] = enclosingIntRect(iter->value[i]);
        RenderLayerBacking* backing = layer->backing();
        // If the layer is using composited scrolling, then it's the contents that these
        // rects apply to.
        GraphicsLayer* graphicsLayer = backing->scrollingContentsLayer();
        if (!graphicsLayer)
            graphicsLayer = backing->graphicsLayer();
        graphicsLayer->platformLayer()->setTouchEventHandlerRegion(webRects);
        oldLayersWithTouchRects.remove(layer);
        m_layersWithTouchRects.add(layer);
    }

    // If there are any layers left that we haven't updated, clear them out.
    for (HashSet<const RenderLayer*>::iterator it = oldLayersWithTouchRects.begin(); it != oldLayersWithTouchRects.end(); ++it) {
        if (RenderLayerBacking* backing = (*it)->backing()) {
            GraphicsLayer* graphicsLayer = backing->scrollingContentsLayer();
            if (!graphicsLayer)
                graphicsLayer = backing->graphicsLayer();
            graphicsLayer->platformLayer()->setTouchEventHandlerRegion(WebVector<WebRect>());
        }
    }
}

void ScrollingCoordinator::touchEventTargetRectsDidChange(const Document*)
{
    if (!touchHitTestingEnabled())
        return;

    // Wait until after layout to update.
    if (m_page->mainFrame()->view()->needsLayout())
        return;

    TRACE_EVENT0("input", "ScrollingCoordinator::touchEventTargetRectsDidChange");

    LayerHitTestRects touchEventTargetRects;
    computeTouchEventTargetRects(touchEventTargetRects);
    setTouchEventTargetRects(touchEventTargetRects);
}

void ScrollingCoordinator::willDestroyRenderLayer(RenderLayer* layer)
{
    m_layersWithTouchRects.remove(layer);
}

void ScrollingCoordinator::setWheelEventHandlerCount(unsigned count)
{
    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(m_page->mainFrame()->view()))
        scrollLayer->setHaveWheelEventHandlers(count > 0);
}

void ScrollingCoordinator::recomputeWheelEventHandlerCountForFrameView(FrameView* frameView)
{
    UNUSED_PARAM(frameView);
    setWheelEventHandlerCount(computeCurrentWheelEventHandlerCount());
}

void ScrollingCoordinator::setShouldUpdateScrollLayerPositionOnMainThread(MainThreadScrollingReasons reasons)
{
    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(m_page->mainFrame()->view()))
        scrollLayer->setShouldScrollOnMainThread(reasons);
}

void ScrollingCoordinator::pageDestroyed()
{
    ASSERT(m_page);
    m_page = 0;
}

bool ScrollingCoordinator::coordinatesScrollingForFrameView(FrameView* frameView) const
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    // We currently only handle the main frame.
    if (&frameView->frame() != m_page->mainFrame())
        return false;

    // We currently only support composited mode.
    RenderView* renderView = m_page->mainFrame()->contentRenderer();
    if (!renderView)
        return false;
    return renderView->usesCompositing();
}

Region ScrollingCoordinator::computeShouldHandleScrollGestureOnMainThreadRegion(const Frame* frame, const IntPoint& frameLocation) const
{
    Region shouldHandleScrollGestureOnMainThreadRegion;
    FrameView* frameView = frame->view();
    if (!frameView)
        return shouldHandleScrollGestureOnMainThreadRegion;

    IntPoint offset = frameLocation;
    offset.moveBy(frameView->frameRect().location());

    if (const FrameView::ScrollableAreaSet* scrollableAreas = frameView->scrollableAreas()) {
        for (FrameView::ScrollableAreaSet::const_iterator it = scrollableAreas->begin(), end = scrollableAreas->end(); it != end; ++it) {
            ScrollableArea* scrollableArea = *it;
            // Composited scrollable areas can be scrolled off the main thread.
            if (scrollableArea->usesCompositedScrolling())
                continue;
            IntRect box = scrollableArea->scrollableAreaBoundingBox();
            box.moveBy(offset);
            shouldHandleScrollGestureOnMainThreadRegion.unite(box);
        }
    }

    // We use GestureScrollBegin/Update/End for moving the resizer handle. So we mark these
    // small resizer areas as non-fast-scrollable to allow the scroll gestures to be passed to
    // main thread if they are targeting the resizer area. (Resizing is done in EventHandler.cpp
    // on main thread).
    if (const FrameView::ResizerAreaSet* resizerAreas = frameView->resizerAreas()) {
        for (FrameView::ResizerAreaSet::const_iterator it = resizerAreas->begin(), end = resizerAreas->end(); it != end; ++it) {
            RenderLayer* layer = *it;
            IntRect bounds = layer->renderer()->absoluteBoundingBoxRect();
            IntRect corner = layer->resizerCornerRect(bounds, ResizerForTouch);
            corner.moveBy(offset);
            shouldHandleScrollGestureOnMainThreadRegion.unite(corner);
        }
    }

    if (const HashSet<RefPtr<Widget> >* children = frameView->children()) {
        for (HashSet<RefPtr<Widget> >::const_iterator it = children->begin(), end = children->end(); it != end; ++it) {
            if (!(*it)->isPluginView())
                continue;

            PluginView* pluginView = toPluginView((*it).get());
            if (pluginView->wantsWheelEvents())
                shouldHandleScrollGestureOnMainThreadRegion.unite(pluginView->frameRect());
        }
    }

    FrameTree* tree = frame->tree();
    for (Frame* subFrame = tree->firstChild(); subFrame; subFrame = subFrame->tree()->nextSibling())
        shouldHandleScrollGestureOnMainThreadRegion.unite(computeShouldHandleScrollGestureOnMainThreadRegion(subFrame, offset));

    return shouldHandleScrollGestureOnMainThreadRegion;
}

static void accumulateDocumentTouchEventTargetRects(LayerHitTestRects& rects, const Document* document)
{
    ASSERT(document);
    if (!document->touchEventTargets())
        return;

    const TouchEventTargetSet* targets = document->touchEventTargets();

    // If there's a handler on the document, html or body element (fairly common in practice),
    // then we can quickly mark the entire document and skip looking at any other handlers.
    // Note that technically a handler on the body doesn't cover the whole document, but it's
    // reasonable to be conservative and report the whole document anyway.
    for (TouchEventTargetSet::const_iterator iter = targets->begin(); iter != targets->end(); ++iter) {
        Node* target = iter->key;
        if (target == document || target == document->documentElement() || target == document->body()) {
            if (RenderObject* renderer = document->renderer()) {
                renderer->computeLayerHitTestRects(rects);
            }
            return;
        }
    }

    for (TouchEventTargetSet::const_iterator iter = targets->begin(); iter != targets->end(); ++iter) {
        const Node* target = iter->key;
        if (!target->inDocument())
            continue;

        if (target->isDocumentNode()) {
            ASSERT(target != document);
            accumulateDocumentTouchEventTargetRects(rects, toDocument(target));
        } else if (RenderObject* renderer = target->renderer()) {
            // If the set also contains one of our ancestor nodes then processing
            // this node would be redundant.
            bool hasTouchEventTargetAncestor = false;
            for (Node* ancestor = target->parentNode(); ancestor && !hasTouchEventTargetAncestor; ancestor = ancestor->parentNode()) {
                if (targets->contains(ancestor))
                    hasTouchEventTargetAncestor = true;
            }
            if (!hasTouchEventTargetAncestor)
                renderer->computeLayerHitTestRects(rects);
        }
    }

}

void ScrollingCoordinator::computeTouchEventTargetRects(LayerHitTestRects& rects)
{
    TRACE_EVENT0("input", "ScrollingCoordinator::computeTouchEventTargetRects");
    ASSERT(touchHitTestingEnabled());

    Document* document = m_page->mainFrame()->document();
    if (!document || !document->view())
        return;

    accumulateDocumentTouchEventTargetRects(rects, document);
}

unsigned ScrollingCoordinator::computeCurrentWheelEventHandlerCount()
{
    unsigned wheelEventHandlerCount = 0;

    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document())
            wheelEventHandlerCount += WheelController::from(frame->document())->wheelEventHandlerCount();
    }

    return wheelEventHandlerCount;
}

void ScrollingCoordinator::frameViewWheelEventHandlerCountChanged(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    recomputeWheelEventHandlerCountForFrameView(frameView);
}

void ScrollingCoordinator::frameViewHasSlowRepaintObjectsDidChange(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    updateShouldUpdateScrollLayerPositionOnMainThread();
}

void ScrollingCoordinator::frameViewFixedObjectsDidChange(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    updateShouldUpdateScrollLayerPositionOnMainThread();
}

GraphicsLayer* ScrollingCoordinator::scrollLayerForScrollableArea(ScrollableArea* scrollableArea)
{
    return scrollableArea->layerForScrolling();
}

GraphicsLayer* ScrollingCoordinator::horizontalScrollbarLayerForScrollableArea(ScrollableArea* scrollableArea)
{
    return scrollableArea->layerForHorizontalScrollbar();
}

GraphicsLayer* ScrollingCoordinator::verticalScrollbarLayerForScrollableArea(ScrollableArea* scrollableArea)
{
    return scrollableArea->layerForVerticalScrollbar();
}

bool ScrollingCoordinator::isForMainFrame(ScrollableArea* scrollableArea) const
{
    return scrollableArea == m_page->mainFrame()->view();
}

GraphicsLayer* ScrollingCoordinator::scrollLayerForFrameView(FrameView* frameView)
{
    RenderView* renderView = frameView->frame().contentRenderer();
    if (!renderView)
        return 0;
    return renderView->compositor()->scrollLayer();
}

GraphicsLayer* ScrollingCoordinator::counterScrollingLayerForFrameView(FrameView*)
{
    return 0;
}

void ScrollingCoordinator::frameViewRootLayerDidChange(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    frameViewLayoutUpdated(frameView);
    recomputeWheelEventHandlerCountForFrameView(frameView);
    updateShouldUpdateScrollLayerPositionOnMainThread();
}

#if OS(MACOSX)
void ScrollingCoordinator::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    ASSERT(isMainThread());

    if (!m_page)
        return;

    FrameView* frameView = m_page->mainFrame()->view();
    if (!frameView)
        return;

    frameView->scrollAnimator()->handleWheelEventPhase(phase);
}
#endif

bool ScrollingCoordinator::hasVisibleSlowRepaintViewportConstrainedObjects(FrameView* frameView) const
{
    const FrameView::ViewportConstrainedObjectSet* viewportConstrainedObjects = frameView->viewportConstrainedObjects();
    if (!viewportConstrainedObjects)
        return false;

    for (FrameView::ViewportConstrainedObjectSet::const_iterator it = viewportConstrainedObjects->begin(), end = viewportConstrainedObjects->end(); it != end; ++it) {
        RenderObject* viewportConstrainedObject = *it;
        if (!viewportConstrainedObject->isBoxModelObject() || !viewportConstrainedObject->hasLayer())
            return true;
        RenderLayer* layer = toRenderBoxModelObject(viewportConstrainedObject)->layer();
        // Any explicit reason that a fixed position element is not composited shouldn't cause slow scrolling.
        if (!layer->isComposited() && layer->viewportConstrainedNotCompositedReason() == RenderLayer::NoNotCompositedReason)
            return true;
    }
    return false;
}

MainThreadScrollingReasons ScrollingCoordinator::mainThreadScrollingReasons() const
{
    // The main thread scrolling reasons are applicable to scrolls of the main
    // frame. If it does not exist or if it is not scrollable, there is no
    // reason to force main thread scrolling.
    FrameView* frameView = m_page->mainFrame()->view();
    if (!frameView || !frameView->isScrollable())
        return static_cast<MainThreadScrollingReasons>(0);

    MainThreadScrollingReasons mainThreadScrollingReasons = (MainThreadScrollingReasons)0;

    if (frameView->hasSlowRepaintObjects())
        mainThreadScrollingReasons |= HasSlowRepaintObjects;
    if (hasVisibleSlowRepaintViewportConstrainedObjects(frameView))
        mainThreadScrollingReasons |= HasNonLayerViewportConstrainedObjects;

    return mainThreadScrollingReasons;
}

void ScrollingCoordinator::updateShouldUpdateScrollLayerPositionOnMainThread()
{
    setShouldUpdateScrollLayerPositionOnMainThread(mainThreadScrollingReasons());
}

String ScrollingCoordinator::mainThreadScrollingReasonsAsText(MainThreadScrollingReasons reasons)
{
    StringBuilder stringBuilder;

    if (reasons & ScrollingCoordinator::HasSlowRepaintObjects)
        stringBuilder.append("Has slow repaint objects, ");
    if (reasons & ScrollingCoordinator::HasViewportConstrainedObjectsWithoutSupportingFixedLayers)
        stringBuilder.append("Has viewport constrained objects without supporting fixed layers, ");
    if (reasons & ScrollingCoordinator::HasNonLayerViewportConstrainedObjects)
        stringBuilder.append("Has non-layer viewport-constrained objects, ");

    if (stringBuilder.length())
        stringBuilder.resize(stringBuilder.length() - 2);
    return stringBuilder.toString();
}

String ScrollingCoordinator::mainThreadScrollingReasonsAsText() const
{
    return mainThreadScrollingReasonsAsText(mainThreadScrollingReasons());
}

} // namespace WebCore
