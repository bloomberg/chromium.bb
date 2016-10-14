// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasFrameDispatcher_h
#define OffscreenCanvasFrameDispatcher_h

#include "platform/PlatformExport.h"
#include "wtf/RefPtr.h"

namespace blink {

class StaticBitmapImage;
class WebLayer;

class PLATFORM_EXPORT OffscreenCanvasFrameDispatcher {
 public:
  virtual ~OffscreenCanvasFrameDispatcher(){};
  virtual void dispatchFrame(RefPtr<StaticBitmapImage>,
                             double commitStartTime,
                             bool isWebGLSoftwareRendering = false) = 0;
};

}  // namespace blink

#endif  // OffscreenCanvasFrameDispatcher_h
