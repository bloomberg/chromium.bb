// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasDrawListener_h
#define CanvasDrawListener_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCanvasCaptureHandler.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include <memory>

class SkImage;

namespace blink {

class CORE_EXPORT CanvasDrawListener : public GarbageCollectedMixin {
 public:
  virtual ~CanvasDrawListener();
  virtual void SendNewFrame(sk_sp<SkImage>);
  bool NeedsNewFrame() const;
  void RequestFrame();

 protected:
  explicit CanvasDrawListener(std::unique_ptr<WebCanvasCaptureHandler>);

  bool frame_capture_requested_;
  std::unique_ptr<WebCanvasCaptureHandler> handler_;
};

}  // namespace blink

#endif
