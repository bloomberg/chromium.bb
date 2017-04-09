// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StubChromeClientForSPv2_h
#define StubChromeClientForSPv2_h

#include "core/loader/EmptyClients.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"

namespace blink {

// A simple ChromeClient implementation which forwards painted artifacts to a
// PaintArtifactCompositor attached to a testing WebLayerTreeView, and permits
// simple analysis of the results.
class StubChromeClientForSPv2 : public EmptyChromeClient {
 public:
  StubChromeClientForSPv2() : layer_tree_view_() {}

  bool HasLayer(const WebLayer& layer) {
    return layer_tree_view_.HasLayer(layer);
  }

  void AttachRootLayer(WebLayer* layer, LocalFrame* local_root) override {
    layer_tree_view_.SetRootLayer(*layer);
  }

 private:
  WebLayerTreeViewImplForTesting layer_tree_view_;
};

}  // namespace blink

#endif  // StubChromeClientForSPv2_h
