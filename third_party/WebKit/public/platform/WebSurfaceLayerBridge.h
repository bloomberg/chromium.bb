// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSurfaceLayerBridge_h
#define WebSurfaceLayerBridge_h

#include "WebCommon.h"
#include "WebLayer.h"

namespace blink {

// Maintains and exposes the SurfaceLayer.
class BLINK_PLATFORM_EXPORT WebSurfaceLayerBridge {
 public:
  static WebSurfaceLayerBridge* Create();
  virtual ~WebSurfaceLayerBridge();
  virtual WebLayer* GetWebLayer() const = 0;
};

}  // namespace blink

#endif  // WebSurfaceLayerBridge_h
