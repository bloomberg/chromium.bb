// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSurfaceLayerBridge_h
#define WebSurfaceLayerBridge_h

#include "WebCommon.h"
#include "WebLayer.h"
#include "WebLayerTreeView.h"

namespace viz {
class FrameSinkId;
}

namespace blink {

// Listens for updates made on the WebLayer by the WebSurfaceLayerBridge.
class BLINK_PLATFORM_EXPORT WebSurfaceLayerBridgeObserver {
 public:
  virtual void OnWebLayerReplaced() = 0;
};

// Maintains and exposes the SurfaceLayer.
class BLINK_PLATFORM_EXPORT WebSurfaceLayerBridge {
 public:
  static std::unique_ptr<WebSurfaceLayerBridge> Create(
      WebLayerTreeView*,
      WebSurfaceLayerBridgeObserver*);
  virtual ~WebSurfaceLayerBridge();
  virtual WebLayer* GetWebLayer() const = 0;
  virtual const viz::FrameSinkId& GetFrameSinkId() const = 0;
};

}  // namespace blink

#endif  // WebSurfaceLayerBridge_h
