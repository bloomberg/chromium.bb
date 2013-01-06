// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_LAYER_TREE_VIEW_IMPL_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_LAYER_TREE_VIEW_IMPL_H_

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

class WebLayerTreeViewImpl : public WebLayerTreeView,
                             public cc::LayerTreeHostClient {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebLayerTreeViewImpl(
      WebLayerTreeViewClient* client);
  virtual ~WebLayerTreeViewImpl();

  WEBKIT_COMPOSITOR_BINDINGS_EXPORT bool initialize(
      const Settings& settings, scoped_ptr<cc::Thread> impl_thread);

  WEBKIT_COMPOSITOR_BINDINGS_EXPORT cc::LayerTreeHost* layer_tree_host() const;

  // WebLayerTreeView implementation.
  virtual void setSurfaceReady() OVERRIDE;
  virtual void setRootLayer(const WebLayer&) OVERRIDE;
  virtual void clearRootLayer() OVERRIDE;
  virtual void setViewportSize(
      const WebSize& layout_viewport_size,
      const WebSize& deviceViewportSize = WebSize()) OVERRIDE;
  virtual WebSize layoutViewportSize() const OVERRIDE;
  virtual WebSize deviceViewportSize() const OVERRIDE;
  virtual WebFloatPoint adjustEventPointForPinchZoom(
      const WebFloatPoint& point) const OVERRIDE;
  virtual void setDeviceScaleFactor(float device_scale_factor) OVERRIDE;
  virtual float deviceScaleFactor() const OVERRIDE;
  virtual void setBackgroundColor(WebColor) OVERRIDE;
  virtual void setHasTransparentBackground(bool transparent) OVERRIDE;
  virtual void setVisible(bool visible) OVERRIDE;
  virtual void setPageScaleFactorAndLimits(float page_scale_factor,
                                           float minimum,
                                           float maximum) OVERRIDE;
  virtual void startPageScaleAnimation(const WebPoint& destination,
                                       bool use_anchor,
                                       float new_page_scale,
                                       double duration_sec) OVERRIDE;
  virtual void setNeedsAnimate() OVERRIDE;
  virtual void setNeedsRedraw() OVERRIDE;
  virtual bool commitRequested() const OVERRIDE;
  virtual void composite() OVERRIDE;
  virtual void updateAnimations(double frame_begin_time) OVERRIDE;
  virtual void didStopFlinging() OVERRIDE;
  virtual bool compositeAndReadback(void* pixels,
                                    const WebRect& viewport_rect) OVERRIDE;
  virtual void finishAllRendering() OVERRIDE;
  virtual void setDeferCommits(bool defer_commits) OVERRIDE;
  virtual void renderingStats(WebRenderingStats& stats) const OVERRIDE;
  virtual void setShowFPSCounter(bool show);
  virtual void setShowPaintRects(bool show);
  virtual void setContinuousPaintingEnabled(bool enabled);
  virtual void loseCompositorContext(int num_times) OVERRIDE;

  // cc::LayerTreeHostClient implementation.
  virtual void willBeginFrame() OVERRIDE;
  virtual void didBeginFrame() OVERRIDE;
  virtual void animate(double monotonicFrameBeginTime) OVERRIDE;
  virtual void layout() OVERRIDE;
  virtual void applyScrollAndScale(gfx::Vector2d scroll_delta,
                                   float page_scale) OVERRIDE;
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
  WebLayerTreeViewClient* client_;
  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;
};

}  // namespace WebKit

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_LAYER_TREE_VIEW_IMPL_H_
