// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_COMPOSITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_COMPOSITOR_H_

#include "base/time/time.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/testing/sim/sim_canvas.h"
#include "third_party/blink/renderer/platform/graphics/apply_viewport_changes.h"

namespace blink {

class WebViewImpl;

// Simulated very basic compositor that's capable of running the BeginMainFrame
// processing steps on WebView: beginFrame, layout, paint.
//
// The painting capabilities are very limited in that only the main layer of
// every CompositedLayerMapping will be painted, squashed layers
// are not supported and the entirety of every layer is always repainted even if
// only part of the layer was invalid.
//
// Note: This also does not support compositor driven animations.
class SimCompositor final : public frame_test_helpers::TestWebWidgetClient {
 public:
  SimCompositor();
  ~SimCompositor() override;

  // When the compositor asks for a main frame, this WebViewImpl will have its
  // lifecycle updated and be painted.
  // The WebWidget client is overridden (via the WebViewClient) to control
  // BeginMainFrame scheduling since this test suite does not use the
  // compositor's scheduler. The SimCompositor wants to monitor and verify
  // expectations around this scheduling, so receives the WebViewClient. We
  // pass it here explicitly to provide type safety, though it is the client
  // available on the WebViewImpl as well.
  void SetWebView(WebViewImpl&, frame_test_helpers::TestWebViewClient&);

  // Executes the BeginMainFrame processing steps, an approximation of what
  // cc::ThreadProxy::BeginMainFrame would do.
  // If time is not specified a 60Hz frame rate time progression is used.
  // Returns all drawing commands that were issued during painting the frame
  // (including cached ones).
  // TODO(dcheng): This should take a base::TimeDelta.
  SimCanvas::Commands BeginFrame(double time_delta_in_seconds = 0.016);

  // Similar to BeginFrame() but doesn't require NeedsBeginFrame(). This is
  // useful for testing the painting after a frame is throttled (for which
  // we don't schedule a BeginFrame).
  SimCanvas::Commands PaintFrame();

  // Helpers to query the state of the compositor from tests.
  //
  // Returns true if a main frame has been requested from blink, until the
  // BeginFrame() step occurs. The AnimationScheduled() checks if an explicit
  // requet for BeginFrame() was made, vs an implicit one by making changes
  // to the compositor's state.
  bool NeedsBeginFrame() const {
    return AnimationScheduled() ||
           layer_tree_host()->RequestedMainFramePendingForTesting();
  }
  // Returns true if commits are deferred in the compositor. Since these tests
  // use synchronous compositing through BeginFrame(), the deferred state has no
  // real effect.
  bool DeferMainFrameUpdate() const {
    return layer_tree_host()->defer_main_frame_update();
  }
  // Returns true if a selection is set on the compositor.
  bool HasSelection() const {
    return layer_tree_host()->selection() != cc::LayerSelection();
  }
  // Returns the background color set on the compositor.
  SkColor background_color() { return layer_tree_host()->background_color(); }

  base::TimeTicks LastFrameTime() const { return last_frame_time_; }

 private:
  // TestWebWidgetClient overrides:
  void DidBeginMainFrame() override;

  WebViewImpl* web_view_ = nullptr;
  frame_test_helpers::TestWebViewClient* test_web_view_client_ = nullptr;

  base::TimeTicks last_frame_time_;

  SimCanvas::Commands* paint_commands_;

  std::unique_ptr<cc::ScopedDeferMainFrameUpdate>
      scoped_defer_main_frame_update_;
};

}  // namespace blink

#endif
