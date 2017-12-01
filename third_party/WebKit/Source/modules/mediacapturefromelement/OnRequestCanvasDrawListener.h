// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OnRequestCanvasDrawListener_h
#define OnRequestCanvasDrawListener_h

#include <memory>
#include "base/memory/weak_ptr.h"
#include "core/html/canvas/CanvasDrawListener.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCanvasCaptureHandler.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class OnRequestCanvasDrawListener final
    : public GarbageCollectedFinalized<OnRequestCanvasDrawListener>,
      public CanvasDrawListener {
  USING_GARBAGE_COLLECTED_MIXIN(OnRequestCanvasDrawListener);

 public:
  ~OnRequestCanvasDrawListener() override;
  static OnRequestCanvasDrawListener* Create(
      std::unique_ptr<WebCanvasCaptureHandler>);
  void SendNewFrame(
      sk_sp<SkImage>,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) override;

  void Trace(blink::Visitor* visitor) override {}

 private:
  OnRequestCanvasDrawListener(std::unique_ptr<WebCanvasCaptureHandler>);
};

}  // namespace blink

#endif
