// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_

#include "base/memory/scoped_ptr.h"
#include "cc/trees/layer_tree_host_client.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeView.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"
#include "webkit/support/webkit_support.h"

namespace cc {
class LayerTreeHost;
class Thread;
}

namespace WebKit { class WebLayer; }

namespace webkit {

class WebLayerTreeViewImplForTesting : public WebKit::WebLayerTreeView,
                                       public cc::LayerTreeHostClient {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebLayerTreeViewImplForTesting(
      webkit_support::LayerTreeViewType type,
      webkit_support::DRTLayerTreeViewClient* client);
  virtual ~WebLayerTreeViewImplForTesting();

  WEBKIT_COMPOSITOR_BINDINGS_EXPORT bool initialize(
      scoped_ptr<cc::Thread> compositor_thread);

  // WebKit::WebLayerTreeView implementation.
  virtual void setSurfaceReady();
  virtual void setRootLayer(const WebKit::WebLayer& layer);
  virtual void clearRootLayer();
  virtual void setViewportSize(const WebKit::WebSize& layout_viewport_size,
                               const WebKit::WebSize& device_viewport_size);
  virtual WebKit::WebSize layoutViewportSize() const;
  virtual WebKit::WebSize deviceViewportSize() const;
  virtual void setDeviceScaleFactor(float scale_factor);
  virtual float deviceScaleFactor() const;
  virtual void setBackgroundColor(WebKit::WebColor);
  virtual void setHasTransparentBackground(bool transparent);
  virtual void setVisible(bool visible);
  virtual void setPageScaleFactorAndLimits(float page_scale_factor,
                                           float minimum,
                                           float maximum);
  virtual void startPageScaleAnimation(const WebKit::WebPoint& destination,
                                       bool use_anchor,
                                       float new_page_scale,
                                       double duration_sec);
  virtual void setNeedsAnimate();
  virtual void setNeedsRedraw();
  virtual bool commitRequested() const;
  virtual void composite();
  virtual void updateAnimations(double frame_begin_time);
  virtual void didStopFlinging();
  virtual bool compositeAndReadback(void* pixels, const WebKit::WebRect& rect);
  virtual void finishAllRendering();
  virtual void setDeferCommits(bool defer_commits);
  virtual void renderingStats(
      WebKit::WebRenderingStats& stats) const;  // NOLINT(runtime/references)

  // cc::LayerTreeHostClient implementation.
  virtual void WillBeginFrame() OVERRIDE {}
  virtual void DidBeginFrame() OVERRIDE {}
  virtual void Animate(double frame_begin_time) OVERRIDE {}
  virtual void Layout() OVERRIDE;
  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta, float page_scale)
      OVERRIDE;
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface() OVERRIDE;
  virtual void DidRecreateOutputSurface(bool success) OVERRIDE {}
  virtual scoped_ptr<cc::InputHandler> CreateInputHandler() OVERRIDE;
  virtual void WillCommit() OVERRIDE {}
  virtual void DidCommit() OVERRIDE {}
  virtual void DidCommitAndDrawFrame() OVERRIDE {}
  virtual void DidCompleteSwapBuffers() OVERRIDE {}
  virtual void ScheduleComposite() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread() OVERRIDE;

 private:
  webkit_support::LayerTreeViewType type_;
  webkit_support::DRTLayerTreeViewClient* client_;
  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(WebLayerTreeViewImplForTesting);
};

}  // namespace webkit

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_
