// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_WEB_VIEW_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_WEB_VIEW_CLIENT_H_

#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"

namespace blink {

class SimWebWidgetClient final
    : public frame_test_helpers::TestWebWidgetClient {
 public:
  // WebWidgetClient overrides.
  void DidMeaningfulLayout(WebMeaningfulLayout) override;

  int VisuallyNonEmptyLayoutCount() const {
    return visually_non_empty_layout_count_;
  }
  int FinishedParsingLayoutCount() const {
    return finished_parsing_layout_count_;
  }
  int FinishedLoadingLayoutCount() const {
    return finished_loading_layout_count_;
  }

 private:
  int visually_non_empty_layout_count_ = 0;
  int finished_parsing_layout_count_ = 0;
  int finished_loading_layout_count_ = 0;
};

class SimWebViewClient final : public frame_test_helpers::TestWebViewClient {
 public:
  explicit SimWebViewClient(SimWebWidgetClient*,
                            content::LayerTreeViewDelegate*);

  // WebViewClient implementation.
  WebView* CreateView(WebLocalFrame* opener,
                      const WebURLRequest&,
                      const WebWindowFeatures&,
                      const WebString& name,
                      WebNavigationPolicy,
                      bool,
                      WebSandboxFlags,
                      const SessionStorageNamespaceId&) override;

  int VisuallyNonEmptyLayoutCount() const {
    return widget_client_->VisuallyNonEmptyLayoutCount();
  }
  int FinishedParsingLayoutCount() const {
    return widget_client_->FinishedParsingLayoutCount();
  }
  int FinishedLoadingLayoutCount() const {
    return widget_client_->FinishedLoadingLayoutCount();
  }

 private:
  SimWebWidgetClient* const widget_client_;

  frame_test_helpers::WebViewHelper web_view_helper_;
};

}  // namespace blink

#endif
