// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasFrameDispatcher_h
#define OffscreenCanvasFrameDispatcher_h

#include "platform/PlatformExport.h"

namespace blink {

class WebLayer;

class PLATFORM_EXPORT OffscreenCanvasFrameDispatcher {
public:
    virtual ~OffscreenCanvasFrameDispatcher() {};
    virtual void dispatchFrame() = 0;
};

} // namespace blink

#endif // OffscreenCanvasFrameDispatcher_h
