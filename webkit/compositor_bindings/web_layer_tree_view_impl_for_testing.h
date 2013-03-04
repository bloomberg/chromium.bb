// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_

#include "base/memory/scoped_ptr.h"
#include "cc/layer_tree_host_client.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeView.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"
#include "webkit/support/webkit_support.h"

namespace cc {
class LayerTreeHost;
class Thread;
}

namespace WebKit {
class WebLayer;

class WebLayerTreeViewImplForTesting : public WebKit::WebLayerTreeView,
                                       public cc::LayerTreeHostClient {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebLayerTreeViewImplForTesting(
      webkit_support::LayerTreeViewType type,
      webkit_support::DRTLayerTreeViewClient* client);
  virtual ~WebLayerTreeViewImplForTesting();

  WEBKIT_COMPOSITOR_BINDINGS_EXPORT bool initialize(
      scoped_ptr<cc::Thread> compositor_thread);

  // WebLayerTreeView implementation.
  virtual void setSurfaceReady();
  virtual void setRootLayer(const WebLayer&);
  virtual void clearRootLayer();
  virtual void setViewportSize(const WebSize& layout_viewport_size,
                               const WebSize& device_viewport_size);
  virtual WebSize layoutViewportSize() const;
  virtual WebSize deviceViewportSize() const;
  virtual void setDeviceScaleFactor(float);
  virtual float deviceScaleFactor() const;
  virtual void setBackgroundColor(WebColor);
  virtual void setHasTransparentBackground(bool);
  virtual void setVisible(bool);
  virtual void setPageScaleFactorAndLimits(float page_scale_factor,
                                           float minimum,
                                           float maximum);
  virtual void startPageScaleAnimation(const WebPoint& destination,
                                       bool use_anchor,
                                       float new_page_scale,
                                       double duration_sec);
  virtual void setNeedsAnimate();
  virtual void setNeedsRedraw();
  virtual bool commitRequested() const;
  virtual void composite();
  virtual void updateAnimations(double frame_begin_time);
  virtual void didStopFlinging();
  virtual bool compositeAndReadback(void* pixels, const WebRect&);
  virtual void finishAllRendering();
  virtual void setDeferCommits(bool defer_commits);
  virtual void renderingStats(WebRenderingStats&) const;

  // cc::LayerTreeHostClient implementation.
  virtual void willBeginFrame() OVERRIDE;
  virtual void didBeginFrame() OVERRIDE;
  virtual void animate(double monotonic_frame_begin_time) OVERRIDE;
  virtual void layout() OVERRIDE;
  virtual void applyScrollAndScale(gfx::Vector2d scroll_delta, float page_scale)
      OVERRIDE;
  virtual scoped_ptr<cc::OutputSurface> createOutputSurface() OVERRIDE;
  virtual void didRecreateOutputSurface(bool success) OVERRIDE;
  virtual scoped_ptr<cc::InputHandler> createInputHandler() OVERRIDE;
  virtual void willCommit() OVERRIDE;
  virtual void didCommit() OVERRIDE;
  virtual void didCommitAndDrawFrame() OVERRIDE;
  virtual void didCompleteSwapBuffers() OVERRIDE;
  virtual void scheduleComposite() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
  OffscreenContextProviderForMainThread() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
  OffscreenContextProviderForCompositorThread() OVERRIDE;

 private:
  webkit_support::LayerTreeViewType type_;
  webkit_support::DRTLayerTreeViewClient* client_;
  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;
};

}  // namespace Web_kit

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_
