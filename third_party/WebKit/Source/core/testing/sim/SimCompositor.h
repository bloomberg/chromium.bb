// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimCompositor_h
#define SimCompositor_h

#include "public/platform/WebLayerTreeView.h"

namespace blink {

class SimDisplayItemList;
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
class SimCompositor final : public WebLayerTreeView {
 public:
  explicit SimCompositor();
  ~SimCompositor();

  void SetWebView(WebViewImpl&);

  // Execute the BeginMainFrame processing steps, an approximation of what
  // cc::ThreadProxy::BeginMainFrame would do.
  // If time is not specified a 60Hz frame rate time progression is used.
  SimDisplayItemList BeginFrame(double time_delta_in_seconds = 0.016);

  bool NeedsBeginFrame() const { return needs_begin_frame_; }
  bool DeferCommits() const { return defer_commits_; }

  bool HasSelection() const { return has_selection_; }

  void SetBackgroundColor(WebColor background_color) override {
    background_color_ = background_color;
  }

  WebColor background_color() { return background_color_; }

 private:
  void SetNeedsBeginFrame() override;
  void SetDeferCommits(bool) override;
  void RegisterSelection(const WebSelection&) override;
  void ClearSelection() override;

  bool needs_begin_frame_;
  bool defer_commits_;
  bool has_selection_;
  WebViewImpl* web_view_;
  double last_frame_time_monotonic_;
  WebColor background_color_;
};

}  // namespace blink

#endif
