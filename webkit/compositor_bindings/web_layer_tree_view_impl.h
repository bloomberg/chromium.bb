// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLayerTreeViewImpl_h
#define WebLayerTreeViewImpl_h

#include "base/memory/scoped_ptr.h"
#include "cc/layer_tree_host_client.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeView.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc {
class FontAtlas;
class LayerTreeHost;
class Thread;
}

namespace WebKit {
class WebLayer;
class WebLayerTreeViewClient;
class WebLayerTreeViewClientAdapter;

class WebLayerTreeViewImpl : public WebLayerTreeView, public cc::LayerTreeHostClient {
public:
    WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebLayerTreeViewImpl(
        WebLayerTreeViewClient*);
    virtual ~WebLayerTreeViewImpl();

    WEBKIT_COMPOSITOR_BINDINGS_EXPORT bool initialize(
        const Settings&, scoped_ptr<cc::Thread> implThread);

    WEBKIT_COMPOSITOR_BINDINGS_EXPORT cc::LayerTreeHost* layer_tree_host() const;

    // WebLayerTreeView implementation.
    virtual void setSurfaceReady();
    virtual void setRootLayer(const WebLayer&);
    virtual void clearRootLayer();
    virtual void setViewportSize(const WebSize& layoutViewportSize, const WebSize& deviceViewportSize);
    virtual WebSize layoutViewportSize() const;
    virtual WebSize deviceViewportSize() const;
    virtual WebFloatPoint adjustEventPointForPinchZoom(const WebFloatPoint& point) const; // FIXME: remove this after WebKit roll.
    virtual void setDeviceScaleFactor(float);
    virtual float deviceScaleFactor() const;
    virtual void setBackgroundColor(WebColor);
    virtual void setHasTransparentBackground(bool);
    virtual void setVisible(bool);
    virtual void setPageScaleFactorAndLimits(float pageScaleFactor, float minimum, float maximum);
    virtual void startPageScaleAnimation(const WebPoint& destination, bool useAnchor, float newPageScale, double durationSec);
    virtual void setNeedsAnimate();
    virtual void setNeedsRedraw();
    virtual bool commitRequested() const;
    virtual void composite();
    virtual void updateAnimations(double frameBeginTime);
    virtual void didStopFlinging();
    virtual bool compositeAndReadback(void *pixels, const WebRect&);
    virtual void finishAllRendering();
    virtual void setDeferCommits(bool deferCommits);
    virtual void renderingStats(WebRenderingStats&) const;
    virtual void setShowFPSCounter(bool show);
    virtual void setShowPaintRects(bool show);
    virtual void setShowDebugBorders(bool show);
    virtual void setContinuousPaintingEnabled(bool);

    // cc::LayerTreeHostClient implementation.
    virtual void willBeginFrame() OVERRIDE;
    virtual void didBeginFrame() OVERRIDE;
    virtual void animate(double monotonicFrameBeginTime) OVERRIDE;
    virtual void layout() OVERRIDE;
    virtual void applyScrollAndScale(gfx::Vector2d scrollDelta, float pageScale) OVERRIDE;
    virtual scoped_ptr<cc::OutputSurface> createOutputSurface() OVERRIDE;
    virtual void didRecreateOutputSurface(bool success) OVERRIDE;
    virtual scoped_ptr<cc::InputHandler> createInputHandler() OVERRIDE;
    virtual void willCommit() OVERRIDE;
    virtual void didCommit() OVERRIDE;
    virtual void didCommitAndDrawFrame() OVERRIDE;
    virtual void didCompleteSwapBuffers() OVERRIDE;
    virtual void scheduleComposite() OVERRIDE;
    virtual scoped_ptr<cc::FontAtlas> createFontAtlas();

private:
    WebLayerTreeViewClient* m_client;
    scoped_ptr<cc::LayerTreeHost> m_layerTreeHost;
};

} // namespace WebKit

#endif // WebLayerTreeViewImpl_h
