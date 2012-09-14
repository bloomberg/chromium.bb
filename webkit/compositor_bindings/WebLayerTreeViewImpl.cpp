// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebLayerTreeViewImpl.h"

#include "CCFontAtlas.h"
#include "CCInputHandler.h"
#include "CCLayerTreeHost.h"
#include "LayerChromium.h"
#include "WebLayerImpl.h"
#include "WebToCCInputHandlerAdapter.h"
#include "webcore_convert.h"
#include <public/WebGraphicsContext3D.h>
#include <public/WebInputHandler.h>
#include <public/WebLayer.h>
#include <public/WebLayerTreeView.h>
#include <public/WebLayerTreeViewClient.h>
#include <public/WebRenderingStats.h>
#include <public/WebSize.h>

using namespace cc;

namespace WebKit {

WebLayerTreeView* WebLayerTreeView::create(WebLayerTreeViewClient* client, const WebLayer& root, const WebLayerTreeView::Settings& settings)
{
    OwnPtr<WebLayerTreeViewImpl> layerTreeViewImpl = adoptPtr(new WebLayerTreeViewImpl(client));
    if (!layerTreeViewImpl->initialize(settings))
        return 0;
    layerTreeViewImpl->setRootLayer(root);
    return layerTreeViewImpl.leakPtr();
}

WebLayerTreeViewImpl::WebLayerTreeViewImpl(WebLayerTreeViewClient* client)
    : m_client(client)
{
}

WebLayerTreeViewImpl::~WebLayerTreeViewImpl()
{
}

bool WebLayerTreeViewImpl::initialize(const WebLayerTreeView::Settings& webSettings)
{
    CCLayerTreeSettings settings;
    settings.acceleratePainting = webSettings.acceleratePainting;
    settings.showFPSCounter = webSettings.showFPSCounter;
    settings.showPlatformLayerTree = webSettings.showPlatformLayerTree;
    settings.showPaintRects = webSettings.showPaintRects;
    settings.renderVSyncEnabled = webSettings.renderVSyncEnabled;
    settings.refreshRate = webSettings.refreshRate;
    settings.defaultTileSize = convert(webSettings.defaultTileSize);
    settings.maxUntiledLayerSize = convert(webSettings.maxUntiledLayerSize);
    m_layerTreeHost = CCLayerTreeHost::create(this, settings);
    if (!m_layerTreeHost)
        return false;
    return true;
}

void WebLayerTreeViewImpl::setSurfaceReady()
{
    m_layerTreeHost->setSurfaceReady();
}

void WebLayerTreeViewImpl::setRootLayer(const WebLayer& root)
{
    m_layerTreeHost->setRootLayer(static_cast<const WebLayerImpl*>(&root)->layer());
}

void WebLayerTreeViewImpl::clearRootLayer()
{
    m_layerTreeHost->setRootLayer(PassRefPtr<LayerChromium>());
}

void WebLayerTreeViewImpl::setViewportSize(const WebSize& layoutViewportSize, const WebSize& deviceViewportSize)
{
    if (!deviceViewportSize.isEmpty())
        m_layerTreeHost->setViewportSize(convert(layoutViewportSize), convert(deviceViewportSize));
    else
        m_layerTreeHost->setViewportSize(convert(layoutViewportSize), convert(layoutViewportSize));
}

WebSize WebLayerTreeViewImpl::layoutViewportSize() const
{
    return convert(m_layerTreeHost->layoutViewportSize());
}

WebSize WebLayerTreeViewImpl::deviceViewportSize() const
{
    return convert(m_layerTreeHost->deviceViewportSize());
}

void WebLayerTreeViewImpl::setDeviceScaleFactor(const float deviceScaleFactor)
{
    m_layerTreeHost->setDeviceScaleFactor(deviceScaleFactor);
}

float WebLayerTreeViewImpl::deviceScaleFactor() const
{
    return m_layerTreeHost->deviceScaleFactor();
}

void WebLayerTreeViewImpl::setBackgroundColor(WebColor color)
{
    m_layerTreeHost->setBackgroundColor(color);
}

void WebLayerTreeViewImpl::setHasTransparentBackground(bool transparent)
{
    m_layerTreeHost->setHasTransparentBackground(transparent);
}

void WebLayerTreeViewImpl::setVisible(bool visible)
{
    m_layerTreeHost->setVisible(visible);
}

void WebLayerTreeViewImpl::setPageScaleFactorAndLimits(float pageScaleFactor, float minimum, float maximum)
{
    m_layerTreeHost->setPageScaleFactorAndLimits(pageScaleFactor, minimum, maximum);
}

void WebLayerTreeViewImpl::startPageScaleAnimation(const WebPoint& scroll, bool useAnchor, float newPageScale, double durationSec)
{
    m_layerTreeHost->startPageScaleAnimation(IntSize(scroll.x, scroll.y), useAnchor, newPageScale, durationSec);
}

void WebLayerTreeViewImpl::setNeedsAnimate()
{
    m_layerTreeHost->setNeedsAnimate();
}

void WebLayerTreeViewImpl::setNeedsRedraw()
{
    m_layerTreeHost->setNeedsRedraw();
}

bool WebLayerTreeViewImpl::commitRequested() const
{
    return m_layerTreeHost->commitRequested();
}

void WebLayerTreeViewImpl::composite()
{
    if (CCProxy::hasImplThread())
        m_layerTreeHost->setNeedsCommit();
    else
        m_layerTreeHost->composite();
}

void WebLayerTreeViewImpl::updateAnimations(double frameBeginTime)
{
    m_layerTreeHost->updateAnimations(frameBeginTime);
}

bool WebLayerTreeViewImpl::compositeAndReadback(void *pixels, const WebRect& rect)
{
    return m_layerTreeHost->compositeAndReadback(pixels, convert(rect));
}

void WebLayerTreeViewImpl::finishAllRendering()
{
    m_layerTreeHost->finishAllRendering();
}

void WebLayerTreeViewImpl::renderingStats(WebRenderingStats& stats) const
{
    CCRenderingStats ccStats;
    m_layerTreeHost->renderingStats(ccStats);

    stats.numAnimationFrames = ccStats.numAnimationFrames;
    stats.numFramesSentToScreen = ccStats.numFramesSentToScreen;
    stats.droppedFrameCount = ccStats.droppedFrameCount;
    stats.totalPaintTimeInSeconds = ccStats.totalPaintTimeInSeconds;
    stats.totalRasterizeTimeInSeconds = ccStats.totalRasterizeTimeInSeconds;
}

void WebLayerTreeViewImpl::setFontAtlas(SkBitmap bitmap, WebRect asciiToWebRectTable[128], int fontHeight)
{
    IntRect asciiToRectTable[128];
    for (int i = 0; i < 128; ++i)
        asciiToRectTable[i] = convert(asciiToWebRectTable[i]);
    OwnPtr<CCFontAtlas> fontAtlas = CCFontAtlas::create(bitmap, asciiToRectTable, fontHeight);
    m_layerTreeHost->setFontAtlas(fontAtlas.release());
}

void WebLayerTreeViewImpl::loseCompositorContext(int numTimes)
{
    m_layerTreeHost->loseContext(numTimes);
}

void WebLayerTreeViewImpl::willBeginFrame()
{
    m_client->willBeginFrame();
}

void WebLayerTreeViewImpl::didBeginFrame()
{
    m_client->didBeginFrame();
}

void WebLayerTreeViewImpl::animate(double monotonicFrameBeginTime)
{
    m_client->updateAnimations(monotonicFrameBeginTime);
}

void WebLayerTreeViewImpl::layout()
{
    m_client->layout();
}

void WebLayerTreeViewImpl::applyScrollAndScale(const cc::IntSize& scrollDelta, float pageScale)
{
    m_client->applyScrollAndScale(convert(scrollDelta), pageScale);
}

PassOwnPtr<WebCompositorOutputSurface> WebLayerTreeViewImpl::createOutputSurface()
{
    return adoptPtr(m_client->createOutputSurface());
}

void WebLayerTreeViewImpl::didRecreateOutputSurface(bool success)
{
    m_client->didRecreateOutputSurface(success);
}

PassOwnPtr<CCInputHandler> WebLayerTreeViewImpl::createInputHandler()
{
    OwnPtr<WebInputHandler> handler = adoptPtr(m_client->createInputHandler());
    if (handler)
        return WebToCCInputHandlerAdapter::create(handler.release());
    return nullptr;
}

void WebLayerTreeViewImpl::willCommit()
{
    m_client->willCommit();
}

void WebLayerTreeViewImpl::didCommit()
{
    m_client->didCommit();
}

void WebLayerTreeViewImpl::didCommitAndDrawFrame()
{
    m_client->didCommitAndDrawFrame();
}

void WebLayerTreeViewImpl::didCompleteSwapBuffers()
{
    m_client->didCompleteSwapBuffers();
}

void WebLayerTreeViewImpl::scheduleComposite()
{
    m_client->scheduleComposite();
}

} // namespace WebKit
