// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLayerTreeViewImpl_h
#define WebLayerTreeViewImpl_h

#include "CCLayerTreeHostClient.h"
#include <public/WebLayerTreeView.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {
class CCLayerTreeHost;
}

namespace WebKit {
class WebLayer;
class WebLayerTreeViewClient;
class WebLayerTreeViewClientAdapter;

class WebLayerTreeViewImpl : public WebLayerTreeView, public WebCore::CCLayerTreeHostClient {
public:
    explicit WebLayerTreeViewImpl(WebLayerTreeViewClient*);
    virtual ~WebLayerTreeViewImpl();

    bool initialize(const Settings&);

    // WebLayerTreeView implementation.
    virtual void setSurfaceReady() OVERRIDE;
    virtual void setRootLayer(const WebLayer&) OVERRIDE;
    virtual void clearRootLayer() OVERRIDE;
    virtual void setViewportSize(const WebSize& layoutViewportSize, const WebSize& deviceViewportSize = WebSize()) OVERRIDE;
    virtual WebSize layoutViewportSize() const OVERRIDE;
    virtual WebSize deviceViewportSize() const OVERRIDE;
    virtual void setDeviceScaleFactor(float) OVERRIDE;
    virtual float deviceScaleFactor() const OVERRIDE;
    virtual void setBackgroundColor(WebColor) OVERRIDE;
    virtual void setHasTransparentBackground(bool) OVERRIDE;
    virtual void setVisible(bool) OVERRIDE;
    virtual void setPageScaleFactorAndLimits(float pageScaleFactor, float minimum, float maximum) OVERRIDE;
    virtual void startPageScaleAnimation(const WebPoint& destination, bool useAnchor, float newPageScale, double durationSec) OVERRIDE;
    virtual void setNeedsAnimate() OVERRIDE;
    virtual void setNeedsRedraw() OVERRIDE;
    virtual bool commitRequested() const OVERRIDE;
    virtual void composite() OVERRIDE;
    virtual void updateAnimations(double frameBeginTime) OVERRIDE;
    virtual bool compositeAndReadback(void *pixels, const WebRect&) OVERRIDE;
    virtual void finishAllRendering() OVERRIDE;
    virtual void renderingStats(WebRenderingStats&) const OVERRIDE;
    virtual void setFontAtlas(SkBitmap, WebRect asciiToRectTable[128], int fontHeight) OVERRIDE;
    virtual void loseCompositorContext(int numTimes) OVERRIDE;

    // WebCore::CCLayerTreeHostClient implementation.
    virtual void willBeginFrame() OVERRIDE;
    virtual void didBeginFrame() OVERRIDE;
    virtual void animate(double monotonicFrameBeginTime) OVERRIDE;
    virtual void layout() OVERRIDE;
    virtual void applyScrollAndScale(const WebCore::IntSize& scrollDelta, float pageScale) OVERRIDE;
    virtual PassOwnPtr<WebCompositorOutputSurface> createOutputSurface() OVERRIDE;
    virtual void didRecreateOutputSurface(bool success) OVERRIDE;
    virtual PassOwnPtr<WebCore::CCInputHandler> createInputHandler() OVERRIDE;
    virtual void willCommit() OVERRIDE;
    virtual void didCommit() OVERRIDE;
    virtual void didCommitAndDrawFrame() OVERRIDE;
    virtual void didCompleteSwapBuffers() OVERRIDE;
    virtual void scheduleComposite() OVERRIDE;

private:
    WebLayerTreeViewClient* m_client;
    OwnPtr<WebCore::CCLayerTreeHost> m_layerTreeHost;
};

} // namespace WebKit

#endif // WebLayerTreeViewImpl_h
