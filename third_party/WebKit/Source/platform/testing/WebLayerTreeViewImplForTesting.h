// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLayerTreeViewImplForTesting_h
#define WebLayerTreeViewImplForTesting_h

#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "public/platform/WebLayerTreeView.h"
#include <memory>

namespace cc {
class AnimationHost;
class LayerTreeHost;
class LayerTreeSettings;
}

namespace blink {

class WebLayer;

// Dummy WeblayerTeeView that does not support any actual compositing.
class WebLayerTreeViewImplForTesting
    : public blink::WebLayerTreeView,
      public cc::LayerTreeHostClient,
      public cc::LayerTreeHostSingleThreadClient {
  WTF_MAKE_NONCOPYABLE(WebLayerTreeViewImplForTesting);

 public:
  WebLayerTreeViewImplForTesting();
  explicit WebLayerTreeViewImplForTesting(const cc::LayerTreeSettings&);
  ~WebLayerTreeViewImplForTesting() override;

  static cc::LayerTreeSettings DefaultLayerTreeSettings();
  cc::LayerTreeHost* GetLayerTreeHost() { return layer_tree_host_.get(); }
  bool HasLayer(const WebLayer&);
  void SetViewportSize(const blink::WebSize&);

  // blink::WebLayerTreeView implementation.
  void SetRootLayer(const blink::WebLayer&) override;
  void ClearRootLayer() override;
  cc::AnimationHost* CompositorAnimationHost() override;
  WebSize GetViewportSize() const override;
  void SetDeviceScaleFactor(float) override;
  void SetBackgroundColor(blink::WebColor) override;
  void SetVisible(bool) override;
  void SetPageScaleFactorAndLimits(float page_scale_factor,
                                   float minimum,
                                   float maximum) override;
  void StartPageScaleAnimation(const blink::WebPoint& destination,
                               bool use_anchor,
                               float new_page_scale,
                               double duration_sec) override;
  void SetNeedsBeginFrame() override;
  void DidStopFlinging() override;
  void SetDeferCommits(bool) override;
  void RegisterViewportLayers(const WebLayerTreeView::ViewportLayers&) override;
  void ClearViewportLayers() override;
  void RegisterSelection(const blink::WebSelection&) override;
  void ClearSelection() override;
  void SetEventListenerProperties(blink::WebEventListenerClass event_class,
                                  blink::WebEventListenerProperties) override;
  blink::WebEventListenerProperties EventListenerProperties(
      blink::WebEventListenerClass event_class) const override;
  void SetHaveScrollEventHandlers(bool) override;
  bool HaveScrollEventHandlers() const override;

  // cc::LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void BeginMainFrame(const viz::BeginFrameArgs& args) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void BeginMainFrameNotExpectedUntil(base::TimeTicks) override {}
  void UpdateLayerTreeHost() override;
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float browser_controls_delta) override;
  void RecordWheelAndTouchScrollingCount(bool has_scrolled_by_wheel,
                                         bool has_scrolled_by_touch) override;
  void RequestNewLayerTreeFrameSink() override;
  void DidInitializeLayerTreeFrameSink() override {}
  void DidFailToInitializeLayerTreeFrameSink() override;
  void WillCommit() override {}
  void DidCommit() override {}
  void DidCommitAndDrawFrame() override {}
  void DidReceiveCompositorFrameAck() override {}
  void DidCompletePageScaleAnimation() override {}

  bool IsForSubframe() override;

  // cc::LayerTreeHostSingleThreadClient implementation.
  void DidSubmitCompositorFrame() override {}
  void DidLoseLayerTreeFrameSink() override {}

 private:
  cc::TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<cc::AnimationHost> animation_host_;
  std::unique_ptr<cc::LayerTreeHost> layer_tree_host_;
};

}  // namespace blink

#endif  // WebLayerTreeViewImplForTesting_h
