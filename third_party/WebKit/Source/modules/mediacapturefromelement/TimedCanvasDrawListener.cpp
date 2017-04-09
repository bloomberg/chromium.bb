// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/TimedCanvasDrawListener.h"

#include "third_party/skia/include/core/SkImage.h"
#include <memory>

namespace blink {

TimedCanvasDrawListener::TimedCanvasDrawListener(
    std::unique_ptr<WebCanvasCaptureHandler> handler,
    double frame_rate)
    : CanvasDrawListener(std::move(handler)),
      frame_interval_(1 / frame_rate),
      request_frame_timer_(this,
                           &TimedCanvasDrawListener::RequestFrameTimerFired) {}

TimedCanvasDrawListener::~TimedCanvasDrawListener() {}

// static
TimedCanvasDrawListener* TimedCanvasDrawListener::Create(
    std::unique_ptr<WebCanvasCaptureHandler> handler,
    double frame_rate) {
  TimedCanvasDrawListener* listener =
      new TimedCanvasDrawListener(std::move(handler), frame_rate);
  listener->request_frame_timer_.StartRepeating(listener->frame_interval_,
                                                BLINK_FROM_HERE);
  return listener;
}

void TimedCanvasDrawListener::SendNewFrame(sk_sp<SkImage> image) {
  frame_capture_requested_ = false;
  CanvasDrawListener::SendNewFrame(std::move(image));
}

void TimedCanvasDrawListener::RequestFrameTimerFired(TimerBase*) {
  // TODO(emircan): Measure the jitter and log, see crbug.com/589974.
  frame_capture_requested_ = true;
}

}  // namespace blink
