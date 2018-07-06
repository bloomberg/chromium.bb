// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_WEB_VIEW_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_WEB_VIEW_CLIENT_H_

#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"

namespace content {
class LayerTreeView;
}

namespace blink {

class SimWebViewClient final : public FrameTestHelpers::TestWebViewClient {
 public:
  // The LayerTreeView to be returned from InitializeLayerTreeView()
  // must be constructed before this class, and given to it.
  explicit SimWebViewClient(content::LayerTreeView&);

  int VisuallyNonEmptyLayoutCount() const {
    return visually_non_empty_layout_count_;
  }
  int FinishedParsingLayoutCount() const {
    return finished_parsing_layout_count_;
  }
  int FinishedLoadingLayoutCount() const {
    return finished_loading_layout_count_;
  }

  // WebViewClient implementation.
  WebLayerTreeView* InitializeLayerTreeView() override;
  WebView* CreateView(WebLocalFrame* opener,
                      const WebURLRequest&,
                      const WebWindowFeatures&,
                      const WebString& name,
                      WebNavigationPolicy,
                      bool,
                      WebSandboxFlags) override;

 private:
  // WebWidgetClient overrides.
  void DidMeaningfulLayout(WebMeaningfulLayout) override;

  int visually_non_empty_layout_count_;
  int finished_parsing_layout_count_;
  int finished_loading_layout_count_;

  content::LayerTreeView* layer_tree_view_;
  FrameTestHelpers::WebViewHelper web_view_helper_;
};

}  // namespace blink

#endif
