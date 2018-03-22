// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/keyboard/NavigatorKeyboard.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/keyboard/Keyboard.h"

namespace blink {

// static
const char NavigatorKeyboard::kSupplementName[] = "NavigatorKeyboard";

NavigatorKeyboard::NavigatorKeyboard(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      keyboard_(
          new Keyboard(GetSupplementable()->GetFrame()
                           ? GetSupplementable()->GetFrame()->GetDocument()
                           : nullptr)) {}

// static
Keyboard* NavigatorKeyboard::keyboard(Navigator& navigator) {
  NavigatorKeyboard* supplement =
      Supplement<Navigator>::From<NavigatorKeyboard>(navigator);
  if (!supplement) {
    supplement = new NavigatorKeyboard(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement->keyboard_;
}

void NavigatorKeyboard::Trace(blink::Visitor* visitor) {
  visitor->Trace(keyboard_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
