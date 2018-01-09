// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebSurfaceLayerBridge.h"

#include <memory>
#include "third_party/WebKit/Source/platform/graphics/SurfaceLayerBridge.h"

namespace blink {

std::unique_ptr<WebSurfaceLayerBridge> WebSurfaceLayerBridge::Create(
    WebLayerTreeView* layer_tree_view,
    WebSurfaceLayerBridgeObserver* observer) {
  return std::make_unique<SurfaceLayerBridge>(layer_tree_view, observer);
}

WebSurfaceLayerBridge::~WebSurfaceLayerBridge() = default;

}  // namespace blink
