// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/screen_orientation/ScreenScreenOrientation.h"

#include "core/frame/Screen.h"
#include "modules/screen_orientation/ScreenOrientation.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

// static
ScreenScreenOrientation& ScreenScreenOrientation::From(Screen& screen) {
  ScreenScreenOrientation* supplement = static_cast<ScreenScreenOrientation*>(
      Supplement<Screen>::From(screen, SupplementName()));
  if (!supplement) {
    supplement = new ScreenScreenOrientation();
    ProvideTo(screen, SupplementName(), supplement);
  }
  return *supplement;
}

// static
ScreenOrientation* ScreenScreenOrientation::orientation(ScriptState* state,
                                                        Screen& screen) {
  ScreenScreenOrientation& self = ScreenScreenOrientation::From(screen);
  if (!screen.GetFrame())
    return nullptr;

  if (!self.orientation_)
    self.orientation_ = ScreenOrientation::Create(screen.GetFrame());

  return self.orientation_;
}

const char* ScreenScreenOrientation::SupplementName() {
  return "ScreenScreenOrientation";
}

void ScreenScreenOrientation::Trace(blink::Visitor* visitor) {
  visitor->Trace(orientation_);
  Supplement<Screen>::Trace(visitor);
}

}  // namespace blink
