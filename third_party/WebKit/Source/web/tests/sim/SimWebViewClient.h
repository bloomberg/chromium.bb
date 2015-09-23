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

private:
    WebLayerTreeView* m_layerTreeView;
};

} // namespace blink

#endif
