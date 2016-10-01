// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/tests/sim/SimWebViewClient.h"

#include "public/platform/WebLayerTreeView.h"

namespace blink {

SimWebViewClient::SimWebViewClient(WebLayerTreeView& layerTreeView)
    : m_visuallyNonEmptyLayoutCount(0),
      m_finishedParsingLayoutCount(0),
      m_finishedLoadingLayoutCount(0),
      m_layerTreeView(&layerTreeView) {}

void SimWebViewClient::didMeaningfulLayout(
    WebMeaningfulLayout meaningfulLayout) {
  switch (meaningfulLayout) {
    case WebMeaningfulLayout::VisuallyNonEmpty:
      m_visuallyNonEmptyLayoutCount++;
      break;
    case WebMeaningfulLayout::FinishedParsing:
      m_finishedParsingLayoutCount++;
      break;
    case WebMeaningfulLayout::FinishedLoading:
      m_finishedLoadingLayoutCount++;
      break;
  }
}

}  // namespace blink
