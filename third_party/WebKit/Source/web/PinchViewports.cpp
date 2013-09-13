/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PinchViewports.h"

#include "WebSettingsImpl.h"
#include "WebViewImpl.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/platform/Scrollbar.h"
#include "core/platform/graphics/FloatSize.h"
#include "core/platform/graphics/GraphicsLayer.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebScrollbarLayer.h"

using WebCore::GraphicsLayer;

namespace WebKit {

PassOwnPtr<PinchViewports> PinchViewports::create(WebViewImpl* owner)
{
    return adoptPtr(new PinchViewports(owner));
}

PinchViewports::PinchViewports(WebViewImpl* owner)
    : m_owner(owner)
    , m_innerViewportContainerLayer(GraphicsLayer::create(m_owner->graphicsLayerFactory(), this))
    , m_pageScaleLayer(GraphicsLayer::create(m_owner->graphicsLayerFactory(), this))
    , m_innerViewportScrollLayer(GraphicsLayer::create(m_owner->graphicsLayerFactory(), this))
    , m_overlayScrollbarHorizontal(GraphicsLayer::create(m_owner->graphicsLayerFactory(), this))
    , m_overlayScrollbarVertical(GraphicsLayer::create(m_owner->graphicsLayerFactory(), this))
{
    m_innerViewportContainerLayer->platformLayer()->setIsContainerForFixedPositionLayers(true);
    // No need for the inner viewport to clip, since the compositing
    // surface takes care of it -- and clipping here would interfere with
    // dynamically-sized viewports on Android.
    m_innerViewportContainerLayer->setMasksToBounds(false);

    m_innerViewportScrollLayer->platformLayer()->setScrollable(true);

    m_innerViewportContainerLayer->addChild(m_pageScaleLayer.get());
    m_pageScaleLayer->addChild(m_innerViewportScrollLayer.get());
    m_innerViewportContainerLayer->addChild(m_overlayScrollbarHorizontal.get());
    m_innerViewportContainerLayer->addChild(m_overlayScrollbarVertical.get());

    // Setup the inner viewport overlay scrollbars.
    setupScrollbar(WebScrollbar::Horizontal);
    setupScrollbar(WebScrollbar::Vertical);
}

PinchViewports::~PinchViewports() { }

void PinchViewports::setViewportSize(const WebCore::IntSize& newSize)
{
    m_innerViewportContainerLayer->setSize(newSize);

    // Need to re-compute sizes for the overlay scrollbars.
    setupScrollbar(WebScrollbar::Horizontal);
    setupScrollbar(WebScrollbar::Vertical);
}

// Modifies the top of the graphics layer tree to add layers needed to support
// the inner/outer viewport fixed-position model for pinch zoom. When finished,
// the tree will look like this (with * denoting added layers):
//
// *innerViewportContainerLayer (fixed pos container)
//  +- *pageScaleLayer
//  |   +- *innerViewportScrollLayer
//  |       +-- overflowControlsHostLayer (root layer)
//  |           +-- outerViewportContainerLayer (fixed pos container) [frame container layer in RenderLayerCompositor]
//  |           |   +-- outerViewportScrollLayer [frame scroll layer in RenderLayerCompositor]
//  |           |       +-- content layers ...
//  |           +-- horizontal ScrollbarLayer (non-overlay)
//  |           +-- verticalScrollbarLayer (non-overlay)
//  |           +-- scroll corner (non-overlay)
//  +- *horizontalScrollbarLayer (overlay)
//  +- *verticalScrollbarLayer (overlay)
//
void PinchViewports::setOverflowControlsHostLayer(GraphicsLayer* layer)
{
    if (layer) {
        ASSERT(!m_innerViewportScrollLayer->children().size());
        m_innerViewportScrollLayer->addChild(layer);
    } else {
        m_innerViewportScrollLayer->removeAllChildren();
        return;
    }

    WebCore::Page* page = m_owner->page();
    if (!page)
        return;

    // We only need to disable the existing (outer viewport) scrollbars
    // if the existing ones are already overlay.
    // FIXME: If we knew in advance before the overflowControlsHostLayer goes
    // away, we would re-enable the drawing of these scrollbars.
    if (GraphicsLayer* scrollbar = m_owner->compositor()->layerForHorizontalScrollbar())
        scrollbar->setDrawsContent(!page->mainFrame()->view()->hasOverlayScrollbars());
    if (GraphicsLayer* scrollbar = m_owner->compositor()->layerForVerticalScrollbar())
        scrollbar->setDrawsContent(!page->mainFrame()->view()->hasOverlayScrollbars());
}

void PinchViewports::setupScrollbar(WebScrollbar::Orientation orientation)
{
    bool isHorizontal = orientation == WebScrollbar::Horizontal;
    GraphicsLayer* scrollbarGraphicsLayer = isHorizontal ?
        m_overlayScrollbarHorizontal.get() : m_overlayScrollbarVertical.get();
    OwnPtr<WebScrollbarLayer>& webScrollbarLayer = isHorizontal ?
        m_webOverlayScrollbarHorizontal : m_webOverlayScrollbarVertical;

    const int overlayScrollbarThickness = m_owner->settingsImpl()->pinchOverlayScrollbarThickness();

    if (!webScrollbarLayer) {
        WebCore::ScrollingCoordinator* coordinator = m_owner->page()->scrollingCoordinator();
        ASSERT(coordinator);
        WebCore::ScrollbarOrientation webcoreOrientation = isHorizontal ? WebCore::HorizontalScrollbar : WebCore::VerticalScrollbar;
        webScrollbarLayer = coordinator->createSolidColorScrollbarLayer(webcoreOrientation, overlayScrollbarThickness, false);

        webScrollbarLayer->setScrollLayer(m_innerViewportScrollLayer->platformLayer());
        scrollbarGraphicsLayer->setContentsToPlatformLayer(webScrollbarLayer->layer());
        scrollbarGraphicsLayer->setDrawsContent(false);
    }

    int xPosition = isHorizontal ? 0 : m_innerViewportContainerLayer->size().width() - overlayScrollbarThickness;
    int yPosition = isHorizontal ? m_innerViewportContainerLayer->size().height() - overlayScrollbarThickness : 0;
    int width = isHorizontal ? m_innerViewportContainerLayer->size().width() - overlayScrollbarThickness : overlayScrollbarThickness;
    int height = isHorizontal ? overlayScrollbarThickness : m_innerViewportContainerLayer->size().height() - overlayScrollbarThickness;

    scrollbarGraphicsLayer->setPosition(WebCore::IntPoint(xPosition, yPosition));
    scrollbarGraphicsLayer->setSize(WebCore::IntSize(width, height));
}

void PinchViewports::registerViewportLayersWithTreeView(WebLayerTreeView* layerTreeView) const
{
    ASSERT(layerTreeView);

    WebCore::RenderLayerCompositor* compositor = m_owner->compositor();
    GraphicsLayer* scrollLayer = compositor->scrollLayer();

    ASSERT(compositor);
    layerTreeView->registerViewportLayers(
        m_pageScaleLayer->platformLayer(),
        m_innerViewportScrollLayer->platformLayer(),
        scrollLayer->platformLayer());
}

void PinchViewports::clearViewportLayersForTreeView(WebLayerTreeView* layerTreeView) const
{
    ASSERT(layerTreeView);

    layerTreeView->clearViewportLayers();
}

void PinchViewports::notifyAnimationStarted(const GraphicsLayer*, double time)
{
}

void PinchViewports::paintContents(const GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& inClip)
{
}

String PinchViewports::debugName(const GraphicsLayer* graphicsLayer)
{
    String name;
    if (graphicsLayer == m_innerViewportContainerLayer.get()) {
        name = "Inner Viewport Container Layer";
    } else if (graphicsLayer == m_pageScaleLayer.get()) {
        name =  "Page Scale Layer";
    } else if (graphicsLayer == m_innerViewportScrollLayer.get()) {
        name =  "Inner Viewport Scroll Layer";
    } else if (graphicsLayer == m_overlayScrollbarHorizontal.get()) {
        name =  "Overlay Scrollbar Horizontal Layer";
    } else if (graphicsLayer == m_overlayScrollbarVertical.get()) {
        name =  "Overlay Scrollbar Vertical Layer";
    } else {
        ASSERT_NOT_REACHED();
    }

    return name;
}

} // namespace WebKit
