// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/ScreenOrientationController.h"

namespace blink {

ScreenOrientationController::ScreenOrientationController(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {}

// static
ScreenOrientationController* ScreenOrientationController::From(
    LocalFrame& frame) {
  return static_cast<ScreenOrientationController*>(
      Supplement<LocalFrame>::From(frame, SupplementName()));
}

void ScreenOrientationController::Trace(blink::Visitor* visitor) {
  Supplement<LocalFrame>::Trace(visitor);
}

// static
void ScreenOrientationController::ProvideTo(
    LocalFrame& frame,
    ScreenOrientationController* controller) {
  Supplement<LocalFrame>::ProvideTo(frame, SupplementName(), controller);
}

// static
const char* ScreenOrientationController::SupplementName() {
  return "ScreenOrientationController";
}

}  // namespace blink
