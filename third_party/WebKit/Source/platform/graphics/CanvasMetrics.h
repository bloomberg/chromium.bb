// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasMetrics_h
#define CanvasMetrics_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class PLATFORM_EXPORT CanvasMetrics {
  STATIC_ONLY(CanvasMetrics);

 public:
  enum CanvasContextUsage {
    kCanvasCreated = 0,
    kGPUAccelerated2DCanvasImageBufferCreated = 1,
    kUnaccelerated2DCanvasImageBufferCreated = 3,
    kAccelerated2DCanvasGPUContextLost = 4,
    kUnaccelerated2DCanvasImageBufferCreationFailed = 5,
    kGPUAccelerated2DCanvasImageBufferCreationFailed = 6,
    kGPUAccelerated2DCanvasDeferralDisabled = 8,
    kGPUAccelerated2DCanvasSurfaceCreationFailed = 9,
    kNumberOfUsages
  };

  static void CountCanvasContextUsage(const CanvasContextUsage);
};

}  // namespace blink

#endif  // CanvasMetrics_h
