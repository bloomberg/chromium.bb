// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimWebViewClient_h
#define SimWebViewClient_h

#include "web/tests/FrameTestHelpers.h"

namespace blink {

class WebLayerTreeView;

class SimWebViewClient final : public FrameTestHelpers::TestWebViewClient {
 public:
  explicit SimWebViewClient(WebLayerTreeView&);

  WebLayerTreeView* initializeLayerTreeView() override {
    return m_layerTreeView;
  }

  int visuallyNonEmptyLayoutCount() const {
    return m_visuallyNonEmptyLayoutCount;
  }
  int finishedParsingLayoutCount() const {
    return m_finishedParsingLayoutCount;
  }
  int finishedLoadingLayoutCount() const {
    return m_finishedLoadingLayoutCount;
  }

 private:
  // WebWidgetClient overrides.
  void didMeaningfulLayout(WebMeaningfulLayout) override;

  int m_visuallyNonEmptyLayoutCount;
  int m_finishedParsingLayoutCount;
  int m_finishedLoadingLayoutCount;

  WebLayerTreeView* m_layerTreeView;
};

}  // namespace blink

#endif
