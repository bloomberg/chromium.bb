// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/tests/sim/SimWebViewClient.h"

#include "public/platform/WebLayerTreeView.h"

namespace blink {

SimWebViewClient::SimWebViewClient(WebLayerTreeView& layerTreeView)
    : m_hadVisuallyNonEmptyLayout(false)
    , m_hadFinishedParsingLayout(false)
    , m_hadFinishedLoadingLayout(false)
    , m_layerTreeView(&layerTreeView)
{
}

void SimWebViewClient::didMeaningfulLayout(WebMeaningfulLayout meaningfulLayout)
{
    switch (meaningfulLayout) {
    case WebMeaningfulLayout::VisuallyNonEmpty:
        m_hadVisuallyNonEmptyLayout = true;
        break;
    case WebMeaningfulLayout::FinishedParsing:
        m_hadFinishedParsingLayout = true;
        break;
    case WebMeaningfulLayout::FinishedLoading:
        m_hadFinishedLoadingLayout = true;
        break;
    }
}

} // namespace blink
