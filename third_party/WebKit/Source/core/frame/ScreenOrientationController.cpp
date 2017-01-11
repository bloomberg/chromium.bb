// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/ScreenOrientationController.h"

namespace blink {

ScreenOrientationController::ScreenOrientationController(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {}

// static
ScreenOrientationController* ScreenOrientationController::from(
    LocalFrame& frame) {
  return static_cast<ScreenOrientationController*>(
      Supplement<LocalFrame>::from(frame, supplementName()));
}

DEFINE_TRACE(ScreenOrientationController) {
  Supplement<LocalFrame>::trace(visitor);
}

// static
void ScreenOrientationController::provideTo(
    LocalFrame& frame,
    ScreenOrientationController* controller) {
  Supplement<LocalFrame>::provideTo(frame, supplementName(), controller);
}

// static
const char* ScreenOrientationController::supplementName() {
  return "ScreenOrientationController";
}

}  // namespace blink
