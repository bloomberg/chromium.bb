// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_
#define WEBKIT_SUPPORT_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_

#include "base/memory/scoped_ptr.h"
#include "cc/trees/layer_tree_host_client.h"
#include "third_party/WebKit/public/platform/WebLayerTreeView.h"

namespace cc {
class LayerTreeHost;
}

namespace WebKit { class WebLayer; }

namespace webkit {

class WebLayerTreeViewImplForTesting : public WebKit::WebLayerTreeView,
                                       public cc::LayerTreeHostClient {
 public:
  WebLayerTreeViewImplForTesting();
  virtual ~WebLayerTreeViewImplForTesting();

  bool Initialize();

  // WebKit::WebLayerTreeView implementation.
  virtual void setSurfaceReady();
  virtual void setRootLayer(const WebKit::WebLayer& layer);
  virtual void clearRootLayer();
  virtual void setViewportSize(const WebKit::WebSize& unused_deprecated,
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
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE;
  virtual void DidInitializeOutputSurface(bool success) OVERRIDE {}
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
  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(WebLayerTreeViewImplForTesting);
};

}  // namespace webkit

#endif  // WEBKIT_SUPPORT_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_
