// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasDrawListener.h"

#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

CanvasDrawListener::~CanvasDrawListener() {}

void CanvasDrawListener::SendNewFrame(
    sk_sp<SkImage> image,
    WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider) {
  handler_->SendNewFrame(
      image, context_provider ? context_provider->ContextProvider() : nullptr);
}

bool CanvasDrawListener::NeedsNewFrame() const {
  return frame_capture_requested_ && handler_->NeedsNewFrame();
}

void CanvasDrawListener::RequestFrame() {
  frame_capture_requested_ = true;
}

CanvasDrawListener::CanvasDrawListener(
    std::unique_ptr<WebCanvasCaptureHandler> handler)
    : frame_capture_requested_(true), handler_(std::move(handler)) {}

}  // namespace blink
