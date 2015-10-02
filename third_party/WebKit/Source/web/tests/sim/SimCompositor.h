// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimCompositor_h
#define SimCompositor_h

namespace blink {

class SimDisplayItemList;
class SimLayerTreeView;
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
class SimCompositor final {
public:
    explicit SimCompositor(SimLayerTreeView&);
    ~SimCompositor();

    void setWebViewImpl(WebViewImpl&);

    // Execute the BeginMainFrame processing steps, an approximation of what
    // cc::ThreadProxy::BeginMainFrame would do.
    SimDisplayItemList beginFrame();

private:
    SimLayerTreeView* m_layerTreeView;
    WebViewImpl* m_webViewImpl;
    double m_lastFrameTimeMonotonic;
};

} // namespace blink

#endif
