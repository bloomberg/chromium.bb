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

    WebLayerTreeView* layerTreeView() override { return m_layerTreeView; }

    bool hadVisuallyNonEmptyLayout() const { return m_hadVisuallyNonEmptyLayout; }
    bool hadFinishedParsingLayout() const { return m_hadFinishedParsingLayout; }
    bool hadFinishedLoadingLayout() const { return m_hadFinishedLoadingLayout; }

private:
    // WebWidgetClient overrides.
    void didMeaningfulLayout(WebMeaningfulLayout) override;

    bool m_hadVisuallyNonEmptyLayout;
    bool m_hadFinishedParsingLayout;
    bool m_hadFinishedLoadingLayout;

    WebLayerTreeView* m_layerTreeView;
};

} // namespace blink

#endif
